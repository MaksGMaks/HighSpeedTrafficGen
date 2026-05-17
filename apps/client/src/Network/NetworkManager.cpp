#include "NetworkManager.hpp"

std::string NetworkManager::toBase64(const std::vector<uint8_t> &data)
{
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    for (std::size_t i = 0; i < data.size(); i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        const uint32_t b2 = (i + 2 < data.size()) ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out += kAlphabet[(triple >> 18) & 0x3F];
        out += kAlphabet[(triple >> 12) & 0x3F];
        out += (i + 1 < data.size()) ? kAlphabet[(triple >> 6) & 0x3F] : '=';
        out += (i + 2 < data.size()) ? kAlphabet[(triple >> 0) & 0x3F] : '=';
    }
    return out;
}

// ── XOR encrypt ───────────────────────────────────────────────────────────────

// static
std::vector<uint8_t> NetworkManager::encryptXor(const std::vector<uint8_t> &data,
                                                 uint8_t key)
{
    std::vector<uint8_t> out(data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key;
    return out;
}

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_ioContext()
    , m_workGuard(boost::asio::make_work_guard(m_ioContext))
    , m_socket(m_ioContext)
    , m_resolver(m_ioContext)
{
    m_ioThread = std::thread([this]{ m_ioContext.run(); });
}

NetworkManager::~NetworkManager()
{
    boost::asio::post(m_ioContext, [this]{
        boost::system::error_code ec;
        m_socket.cancel(ec);
        m_socket.close(ec);
        m_workGuard.reset();    // lets io_context::run() return
    });
    if (m_ioThread.joinable())
        m_ioThread.join();
}

// ── Connect / Disconnect ──────────────────────────────────────────────────────

void NetworkManager::connect(const std::string &ip, uint16_t port)
{
    boost::asio::post(m_ioContext, [this, ip, port]{
        if (m_socket.is_open()) {
            boost::system::error_code ec;
            m_socket.close(ec);
        }

        m_socket   = boost::asio::ip::tcp::socket  (m_ioContext);
        m_resolver = boost::asio::ip::tcp::resolver(m_ioContext);

        // BUG FIX: synchronous resolve blocks the io_context thread.
        // Use async_resolve so the thread stays free while DNS resolves.
        m_resolver.async_resolve(ip, std::to_string(port),
            [this](boost::system::error_code ec,
                   boost::asio::ip::tcp::resolver::results_type endpoints)
            {
                if (ec) {
                    std::cerr << "[C][NM] Resolve failed: " << ec.message() << "\n";
                    emit disconnectNotify();
                    return;
                }

                boost::asio::async_connect(m_socket, endpoints,
                    [this](boost::system::error_code ec,
                           const boost::asio::ip::tcp::endpoint &ep)
                    {
                        if (ec) {
                            std::cerr << "[C][NM] Connect failed: " << ec.message() << "\n";
                            emit disconnectNotify();
                            return;
                        }

                        std::cout << "[C][NM] Connected to " << ep << "\n";
                        m_running.store(true, std::memory_order_relaxed);

                        boost::system::error_code optEc;
                        m_socket.set_option(boost::asio::ip::tcp::no_delay(true), optEc);

                        startReadLoop();
                        emit connected();
                    });
            });
    });
}

void NetworkManager::disconnect()
{
    boost::asio::post(m_ioContext, [this]{
        if (!m_socket.is_open()) return;

        m_running.store(false, std::memory_order_relaxed);

        boost::system::error_code ec;
        m_socket.cancel(ec);
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        m_socket.close(ec);

        std::cout << "[C][NM] Disconnected\n";
        emit disconnectNotify();
    });
}

void NetworkManager::handleDisconnect(const boost::system::error_code &ec)
{
    // Guard against re-entry: disconnect() or a prior read/write error
    // may have already cleaned up.
    if (!m_running.load(std::memory_order_relaxed)) return;

    std::cerr << "[C][NM] Lost connection: " << ec.message() << "\n";
    m_running.store(false, std::memory_order_relaxed);

    boost::system::error_code closeEc;
    m_socket.close(closeEc);

    emit disconnectNotify();
}

// ── Read loop ─────────────────────────────────────────────────────────────────

void NetworkManager::startReadLoop()
{
    m_readAccumulator.clear();
    doRead();
}

void NetworkManager::doRead()
{
    m_socket.async_read_some(
        boost::asio::buffer(m_readBuf),
        [this](boost::system::error_code ec, std::size_t len)
        {
            if (ec) { handleDisconnect(ec); return; }
            if (len == 0) { doRead(); return; }

            m_readAccumulator.append(m_readBuf.data(), len);

            // BUG FIX: original called doRead() unconditionally after parse,
            // causing a second async_read_some to be posted even when one was
            // already in flight from the re-entry inside the while loop.
            // Correct pattern: consume all complete messages, then post
            // exactly one new doRead() at the end.
            while (!m_readAccumulator.empty()) {
                const std::size_t start = m_readAccumulator.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) { m_readAccumulator.clear(); break; }
                if (start > 0) m_readAccumulator.erase(0, start);

                const char open  = m_readAccumulator[0];
                const char close = (open == '[') ? ']' : (open == '{') ? '}' : '\0';
                if (close == '\0') { m_readAccumulator.clear(); break; }

                int depth = 0;
                bool inString = false, escape = false;
                std::size_t end = std::string::npos;

                for (std::size_t i = 0; i < m_readAccumulator.size(); ++i) {
                    const char c = m_readAccumulator[i];
                    if (escape)               { escape = false; continue; }
                    if (c == '\\' && inString){ escape = true;  continue; }
                    if (c == '"')             { inString = !inString; continue; }
                    if (inString)             continue;
                    if (c == open)            { ++depth; }
                    else if (c == close)      { --depth; if (depth == 0) { end = i; break; } }
                }

                if (end == std::string::npos) break;  // wait for more data

                const std::string msgStr = m_readAccumulator.substr(0, end + 1);
                m_readAccumulator.erase(0, end + 1);

                nlohmann::json doc = nlohmann::json::parse(msgStr, nullptr, false);
                if (doc.is_discarded()) {
                    std::cerr << "[C][NM] JSON parse error\n";
                    continue;
                }
                handleMessage(doc);
            }

            doRead();  // exactly one re-arm per completion
        });
}

void NetworkManager::handleMessage(const nlohmann::json &doc)
{
    if (!doc.is_array() || doc.empty()) {
        std::cerr << "[C][NM] Expected non-empty JSON array\n";
        return;
    }

    const nlohmann::json &obj = doc.at(0);

    // ── Device info (first message after connect) ─────────────────────────
    // if (obj.contains(jsonHeaders::InterfaceName)) {
    //     // TODO: parse and expose interface capabilities if the UI needs them.
    //     emit receiverJSON();
    //     return;
    // }

    // ── Command ACK ───────────────────────────────────────────────────────
    if (obj.contains(jsonHeaders::Type) && obj.contains(jsonHeaders::Command)) {
        const int type = obj.at(jsonHeaders::Type).get<int>();
        const int cmd  = obj.at(jsonHeaders::Command).get<int>();

        if (type == jsonHeaders::ACCEPT || type == jsonHeaders::FAILED) {
            emit commandAck(static_cast<jsonHeaders::CommandC>(cmd),
                            type == jsonHeaders::ACCEPT);
            return;
        }
    }

    // ── Stat frame ────────────────────────────────────────────────────────────────
    if (obj.contains(jsonHeaders::Data::Key)) {
        ServerStats s{};
        s.obytes      = std::stoull(obj.value(jsonHeaders::Data::BPS,         std::string("0")));
        s.opackets    = std::stoull(obj.value(jsonHeaders::Data::Pps,         std::string("0")));
        s.timestampMs = std::stoll (obj.value(jsonHeaders::Data::Time,        std::string("0")));
        s.oerrors     = 0;
        s.ipackets    = 0;
        s.ibytes      = 0;
        s.ierrors     = 0;
        emit statsReceived(s);
        return;
    }

    std::cerr << "[C][NM] Unrecognised message: " << doc.dump() << "\n";
}

// ── Write queue (io_context thread only) ──────────────────────────────────────

void NetworkManager::postSend(const nlohmann::json &payload)
{
    // Serialise on the caller's thread, then hand ownership to the io thread.
    auto frame = std::make_shared<std::string>(payload.dump());

    boost::asio::post(m_ioContext, [this, frame]{
        m_writeQueue.push_back(frame);
        if (!m_writing)
            doSendNext();
    });
}

void NetworkManager::doSendNext()
{
    if (m_writeQueue.empty()) { m_writing = false; return; }
    m_writing = true;

    std::shared_ptr<std::string> frame = m_writeQueue.front();

    boost::asio::async_write(
        m_socket,
        boost::asio::buffer(*frame),
        [this, frame](boost::system::error_code ec, std::size_t /*sent*/)
        {
            if (ec) { handleDisconnect(ec); return; }
            m_writeQueue.erase(m_writeQueue.begin());
            doSendNext();
        });
}

// ── Commands ──────────────────────────────────────────────────────────────────

void NetworkManager::startGenerator(const genParams &params)
{
    if (!m_running.load(std::memory_order_relaxed)) return;

    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    static_cast<int>(jsonHeaders::TypeC::Request)  },
        { jsonHeaders::Command, static_cast<int>(jsonHeaders::CommandC::Start) },
        { "params",             params }
    });
    postSend(arr);

    if (params.mode == GeneratorMode::PcapPlayer &&
        !params.playerSettings.filePath.empty())
    {
        // Post onto io_context thread so it runs after the START message is queued
        boost::asio::post(m_ioContext,
            [this, path = params.playerSettings.filePath]{
                sendFileChunked(path);
            });
    }
}

void NetworkManager::pauseGenerator()
{
    if (!m_running.load(std::memory_order_relaxed)) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::PAUSE   }
    });
    postSend(arr);
}

void NetworkManager::resumeGenerator()
{
    if (!m_running.load(std::memory_order_relaxed)) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::RESUME  }
    });
    postSend(arr);
}

void NetworkManager::stopGenerator()
{
    if (!m_running.load(std::memory_order_relaxed)) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::FINISH  }
    });
    postSend(arr);
}

// ── Chunked file send ─────────────────────────────────────────────────────────

void NetworkManager::sendFileChunked(const std::string &path)
{
    // Runs on the io_context thread — must not block for long.
    // Reading the whole file first is fine for typical PCAP sizes (< few GB).

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[C][NM] sendFileChunked: cannot open " << path << "\n";
        return;
    }
    const std::streamsize fsize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> raw(static_cast<std::size_t>(fsize));
    if (!file.read(reinterpret_cast<char*>(raw.data()), fsize)) {
        std::cerr << "[C][NM] sendFileChunked: read error on " << path << "\n";
        return;
    }

    // Encrypt whole buffer, then slice into chunks
    std::vector<uint8_t> encrypted = encryptXor(raw);

    const std::size_t total = (encrypted.size() + FILE_CHUNK_BYTES - 1)
                              / FILE_CHUNK_BYTES;
    if (total == 0) {
        std::cerr << "[C][NM] sendFileChunked: empty file\n";
        return;
    }

    std::cout << "[C][NM] Sending PCAP: " << encrypted.size()
              << " bytes in " << total << " chunk(s)\n";

    for (std::size_t chunkIdx = 0; chunkIdx < total; ++chunkIdx) {
        const std::size_t offset = chunkIdx * FILE_CHUNK_BYTES;
        const std::size_t len    = std::min(FILE_CHUNK_BYTES,
                                            encrypted.size() - offset);

        const std::vector<uint8_t> slice(encrypted.begin() + offset,
                                         encrypted.begin() + offset + len);

        nlohmann::json envelope = nlohmann::json::array();
        nlohmann::json msg = {
            { jsonHeaders::File::Key, toBase64(slice)         },
            { "chunk",                static_cast<int>(chunkIdx) },
            { "total",                static_cast<int>(total)    }
        };
        if (chunkIdx + 1 == total)
            msg["eof"] = true;   // server uses this to know transfer is complete

        envelope.push_back(std::move(msg));
        postSend(envelope);

        // Emit progress on Qt side (postSend is thread-safe via io_context)
        const int pct = static_cast<int>(((chunkIdx + 1) * 100) / total);
        emit fileSendProgress(pct);
    }
}
