#include "NetworkManager.hpp"
#include <iostream>
#include <stdexcept>

static std::vector<uint8_t> fromBase64(const std::string &b64)
{
    static constexpr uint8_t kDec[256] = {
        // 0-based decode table; 0xFF = invalid
#define X 0xFF
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 0-15
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 16-31
        X,X,X,X,X,X,X,X,X,X,X,62,X,X,X,63, // 32-47  (+, /)
        52,53,54,55,56,57,58,59,60,61,X,X,X,X,X,X, // 48-63  (0-9)
        X, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, // 64-79  (A-O)
        15,16,17,18,19,20,21,22,23,24,25,X,X,X,X,X, // 80-95  (P-Z)
        X,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, // 96-111 (a-o)
        41,42,43,44,45,46,47,48,49,50,51,X,X,X,X,X, // 112-127 (p-z)
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 128-143
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 144-159
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 160-175
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 176-191
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 192-207
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 208-223
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, // 224-239
        X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X  // 240-255
        #undef X
    };

    std::vector<uint8_t> out;
    out.reserve((b64.size() / 4) * 3);

    uint32_t accum = 0;
    int      bits  = 0;

    for (unsigned char c : b64) {
        if (c == '=') break;
        const uint8_t v = kDec[c];
        if (v == 0xFF) continue;   // skip whitespace / invalid
        accum = (accum << 6) | v;
        bits  += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
        }
    }
    return out;
}

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

NetworkManager::NetworkManager(uint16_t port, uint16_t devID)
    : m_ioContext()
    , m_acceptor(m_ioContext)
    , m_socket(m_ioContext)
{
    m_generator = new Generator(devID);

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
}

NetworkManager::~NetworkManager()
{
    m_statRunning = false;
    if (m_statThread.joinable())
        m_statThread.join();

    m_msgRunning = false;
    if (m_msgThread.joinable())
        m_msgThread.join();
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writerRunning = false;
    }
    m_writeCv.notify_all();
    if (m_writerThread.joinable())
        m_writerThread.join();

    boost::system::error_code ec;
    m_acceptor.close(ec);
}

// ── Public: enqueue a stat frame ──────────────────────────────────────────────

void NetworkManager::pushStats(const nlohmann::json &payload)
{
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writeQueue.push(payload.dump());
    }
    m_writeCv.notify_one();
}

void NetworkManager::pushStats(const ServerStats &stats)
{
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Data::Key,         jsonHeaders::Data::Key               },
        { jsonHeaders::Data::BPS,         std::to_string(stats.obytes)         },
        { jsonHeaders::Data::Bps,         std::to_string(stats.obytes * 8)     },
        { jsonHeaders::Data::Pps,         std::to_string(stats.opackets)       },
        { jsonHeaders::Data::Time,        std::to_string(stats.timestampMs)    },
        { jsonHeaders::Data::TotalSend,   std::to_string(stats.obytes)         },
        { jsonHeaders::Data::TotalCopies, std::to_string(stats.opackets)       }
    });
    pushStats(arr);
}

// ── Listen (blocking) ─────────────────────────────────────────────────────────

int NetworkManager::listen()
{
    boost::system::error_code ec;

    std::cout << "[S][NM] Waiting for client on port "
              << m_acceptor.local_endpoint().port() << "\n";

    // Block here until a client arrives
    m_acceptor.accept(m_socket, ec);
    if (ec) {
        std::cerr << "[S][NM] Accept error: " << ec.message() << "\n";
        return -1;
    }

    m_socket.set_option(boost::asio::ip::tcp::no_delay(true));

    const std::string clientIp =
        m_socket.remote_endpoint().address().to_string();
    std::cout << "[S][NM] Client connected: " << clientIp << "\n";

    if (onClientConnected)
        onClientConnected(clientIp);

    // ── Start worker threads AFTER accept ────────────────────────────────────
    m_writerRunning = true;
    m_writerThread  = std::thread(&NetworkManager::writerLoop, this);

    m_statRunning = true;
    m_statThread  = std::thread(&NetworkManager::statLoop, this);

    m_msgRunning = true;
    m_msgThread  = std::thread(&NetworkManager::msgLoop, this);

    // ── Read loop ─────────────────────────────────────────────────────────────
    std::string            accumulator;
    std::array<char, 4096> buf;

    for (;;) {
        std::size_t len = m_socket.read_some(boost::asio::buffer(buf), ec);

        if (ec == boost::asio::error::eof) {
            std::cout << "[S][NM] Client disconnected\n";
            break;
        }
        if (ec) {
            std::cerr << "[S][NM] Read error: " << ec.message() << "\n";
            break;
        }
        if (len == 0) continue;

        accumulator.append(buf.data(), len);

        while (!accumulator.empty()) {
            const std::size_t start = accumulator.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) { accumulator.clear(); break; }
            if (start > 0) accumulator.erase(0, start);

            const char open  = accumulator[0];
            const char close = (open == '[') ? ']' : (open == '{') ? '}' : '\0';
            if (close == '\0') { accumulator.clear(); break; }

            int depth = 0;
            bool inString = false, escape = false;
            std::size_t end = std::string::npos;

            for (std::size_t i = 0; i < accumulator.size(); ++i) {
                const char c = accumulator[i];
                if (escape)              { escape = false; continue; }
                if (c == '\\' && inString) { escape = true; continue; }
                if (c == '"')            { inString = !inString; continue; }
                if (inString)            continue;
                if (c == open)           { ++depth; }
                else if (c == close)     { --depth; if (depth == 0) { end = i; break; } }
            }

            if (end == std::string::npos) break;

            const std::string msgStr = accumulator.substr(0, end + 1);
            accumulator.erase(0, end + 1);

            nlohmann::json doc = nlohmann::json::parse(msgStr, nullptr, false);
            if (doc.is_discarded()) {
                std::cerr << "[S][NM] JSON parse error, discarding\n";
                continue;
            }
            if (!doc.is_array() || doc.empty()) {
                std::cerr << "[S][NM] Expected JSON array\n";
                continue;
            }

            const nlohmann::json& msg = doc.at(0);
            if (msg.contains(jsonHeaders::File::Key))
                handleFileData(msg);
            else
                handleCommand(msg);
        }
    }

    // ── Tear down all threads ─────────────────────────────────────────────────
    m_statRunning = false;
    if (m_statThread.joinable())
        m_statThread.join();

    m_msgRunning = false;
    if (m_msgThread.joinable())
        m_msgThread.join();

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writerRunning = false;
    }
    m_writeCv.notify_all();
    if (m_writerThread.joinable())
        m_writerThread.join();

    if (onClientDisconnected)
        onClientDisconnected();

    boost::system::error_code closeEc;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, closeEc);
    m_socket.close(closeEc);
    return 0;
}

// ── Writer thread ─────────────────────────────────────────────────────────────

void NetworkManager::writerLoop()
{
    while (true) {
        std::string frame;
        {
            std::unique_lock<std::mutex> lock(m_writeMutex);
            m_writeCv.wait(lock, [this]{
                return !m_writeQueue.empty() || !m_writerRunning;
            });

            if (!m_writerRunning && m_writeQueue.empty())
                return;

            frame = std::move(m_writeQueue.front());
            m_writeQueue.pop();
        }

        boost::system::error_code ec;
        boost::asio::write(m_socket, boost::asio::buffer(frame), ec);
        if (ec) {
            std::cerr << "[S][NM] Write error: " << ec.message() << "\n";
            std::lock_guard<std::mutex> lock(m_writeMutex);
            while (!m_writeQueue.empty())
                m_writeQueue.pop();
            m_writerRunning = false;
            return;
        }
    }
}

// ── Command dispatch ──────────────────────────────────────────────────────────

void NetworkManager::handleCommand(const nlohmann::json &obj)
{
    if (!obj.contains(jsonHeaders::Type) ||
        !obj.contains(jsonHeaders::Command)) {
        std::cerr << "[S][NM] Missing TYPE or COMMAND\n";
        return;
    }

    const int type = obj.at(jsonHeaders::Type).get<int>();
    const int cmd  = obj.at(jsonHeaders::Command).get<int>();

    if (type != jsonHeaders::REQUEST) {
        std::cerr << "[S][NM] Unexpected type: " << type << "\n";
        return;
    }

    auto sendAck = [&](jsonHeaders::CommandC c, bool ok) {
        nlohmann::json ack = nlohmann::json::array();
        ack.push_back({
            { jsonHeaders::Type,    ok ? jsonHeaders::ACCEPT : jsonHeaders::FAILED },
            { jsonHeaders::Command, static_cast<int>(c) }
        });
        pushStats(ack);
    };

    // FIX: CommandC enum values are Start/Pause/Resume/Finish (capital first letter)
    switch (static_cast<jsonHeaders::CommandC>(cmd)) {

    case jsonHeaders::CommandC::Start: {
        std::cout << "[S][NM] START\n";
        m_pendingParams = parseParams(obj);

        if (m_pendingParams.mode == GeneratorMode::PcapPlayer) {
            if (!m_pcapBuffer.empty()) {
                // File was sent before START (or reusing previous file)
                m_generator->doStartFromBuffer(m_pendingParams, m_pcapBuffer);
            } else {
                // File hasn't arrived yet — wait for FILE_DATA
                m_waitingForFile = true;
                std::cout << "[S][NM] Waiting for PCAP file data...\n";
            }
        } else {
            m_generator->doStart(m_pendingParams);
        }
        sendAck(jsonHeaders::CommandC::Start, true);
        break;
    }

    case jsonHeaders::CommandC::Pause:
        std::cout << "[S][NM] PAUSE\n";
        m_generator->doPause();
        sendAck(jsonHeaders::CommandC::Pause, true);
        break;

    case jsonHeaders::CommandC::Resume:
        std::cout << "[S][NM] RESUME\n";
        m_generator->doResume();
        sendAck(jsonHeaders::CommandC::Resume, true);
        break;

    case jsonHeaders::CommandC::Finish:
        std::cout << "[S][NM] FINISH\n";
        m_generator->doStop();
        sendAck(jsonHeaders::CommandC::Finish, true);
        break;

    default:
        std::cerr << "[S][NM] Unknown command: " << cmd << "\n";
        sendAck(static_cast<jsonHeaders::CommandC>(cmd), false);
        break;
    }
}

genParams NetworkManager::parseParams(const nlohmann::json &obj) const
{
    return obj.at("params").get<genParams>();
}

// ── Sync write ────────────────────────────────────────────────────────────────

void NetworkManager::syncWrite(const nlohmann::json &payload)
{
    const std::string serialised = payload.dump();
    boost::system::error_code ec;
    boost::asio::write(m_socket, boost::asio::buffer(serialised), ec);
    if (ec)
        std::cerr << "[S][NM] syncWrite error: " << ec.message() << "\n";
}

void NetworkManager::handleFileData(const nlohmann::json &obj)
{
    // Each chunk: {"FILE_DATA": "<base64>", "chunk": N, "total": T [, "eof": true]}
    const std::string b64   = obj.at(jsonHeaders::File::Key).get<std::string>();
    const int chunkIdx      = obj.value("chunk", 0);
    const int totalChunks   = obj.value("total", 1);
    const bool eof          = obj.value("eof",   false);

    // First chunk — (re)initialise assembly buffer
    if (chunkIdx == 0) {
        m_pcapAssembly.clear();
        m_expectedChunks = totalChunks;
        m_receivedChunks = 0;
        std::cout << "[S][NM] Starting PCAP receive: "
                  << totalChunks << " chunk(s) expected\n";
    }

    // Decode and decrypt this chunk
    std::vector<uint8_t> decoded  = fromBase64(b64);
    constexpr uint8_t    XOR_KEY  = 0xA5;
    for (uint8_t &byte : decoded) byte ^= XOR_KEY;

    m_pcapAssembly.insert(m_pcapAssembly.end(),
                          decoded.begin(), decoded.end());
    ++m_receivedChunks;

    std::cout << "[S][NM] PCAP chunk " << (chunkIdx + 1)
              << "/" << totalChunks
              << " (" << decoded.size() << " bytes)\n";

    if (!eof) return;   // wait for more chunks

    // Transfer complete
    m_pcapBuffer = std::move(m_pcapAssembly);
    m_pcapAssembly.clear();
    std::cout << "[S][NM] PCAP transfer complete: "
              << m_pcapBuffer.size() << " bytes total\n";

    if (m_waitingForFile) {
        m_waitingForFile = false;
        m_generator->doStartFromBuffer(m_pendingParams, m_pcapBuffer);
    }
}

void NetworkManager::statLoop()
{
    generator::statisticQueue* q = m_generator->getQueueP();
    if (!q) return;

    while (m_statRunning) {
        // ── ADAPT THIS BLOCK to your actual statisticQueue API ────────────────
        // Option A: if statisticQueue has a blocking pop(statisticData&):
        //   generator::statisticData sd;
        //   if (!q->pop(sd)) continue;   // returns false on shutdown/timeout

        // Option B: if it wraps a raw std::queue (based on q->queue.push usage):
        generator::statisticData sd;
        {
            // spin-wait with sleep — replace with condvar if queue supports it
            bool got = false;
            while (m_statRunning) {
                if (!q->queue.empty()) {
                    sd  = q->queue.front();
                    q->queue.pop();
                    got = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!got) continue;
        }
        // ─────────────────────────────────────────────────────────────────────

        // Convert statisticData → ServerStats
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        ServerStats s;
        s.opackets    = sd.packetsSent;
        s.obytes      = sd.bytesSent;
        s.oerrors     = sd.txErrors;
        s.ipackets    = 0;              // generator doesn't track RX
        s.ibytes      = 0;
        s.ierrors     = 0;
        s.timestampMs = nowMs;

        pushStats(s);   // existing method — serializes and enqueues for TX
    }
}

// ── msgLoop ───────────────────────────────────────────────────────────────────
void NetworkManager::msgLoop()
{
    generator::messageQueue* q = m_generator->getMSGQueueP();
    if (!q) return;

    while (m_msgRunning) {
        generatorMessage gm;
        bool got = false;

        while (m_msgRunning) {
            if (!q->queue.empty()) {
                gm  = q->queue.front();
                q->queue.pop();
                got = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!got) continue;

        nlohmann::json arr = nlohmann::json::array();
        arr.push_back({
            { jsonHeaders::Message::Key,      jsonHeaders::Message::Key          },
            { jsonHeaders::Message::Severity, static_cast<int>(gm.severity)     },
            { jsonHeaders::Message::Text,     gm.text                            }
        });
        pushStats(arr);   // reuses the existing write queue
    }
}