#include "NetworkManager.hpp"

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
            while (!m_readAccumulator.empty())
            {
                nlohmann::json doc = nlohmann::json::parse(
                    m_readAccumulator, nullptr, /*exceptions=*/false);

                if (doc.is_discarded())
                    break;  // incomplete — wait for more data

                // TODO: track actual consumed bytes for true multi-message
                // streams (same caveat as server side).
                m_readAccumulator.clear();
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
    if (obj.contains(jsonHeaders::InterfaceName)) {
        // TODO: parse and expose interface capabilities if the UI needs them.
        emit receiverJSON();
        return;
    }

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

    // ── Stat frame ────────────────────────────────────────────────────────
    if (obj.contains(jsonHeaders::Data::Key)) {
        jsonHeaders::receivedData d{};
        d.BPS         = std::stoull(obj.value(jsonHeaders::Data::BPS,         std::string("0")));
        d.bps         = std::stoull(obj.value(jsonHeaders::Data::Bps,         std::string("0")));
        d.pps         = std::stoull(obj.value(jsonHeaders::Data::Pps,         std::string("0")));
        d.time        = std::stoll (obj.value(jsonHeaders::Data::Time,        std::string("0")));
        d.totalCopies = std::stoull(obj.value(jsonHeaders::Data::TotalCopies, std::string("0")));
        d.totalSend   = std::stoull(obj.value(jsonHeaders::Data::TotalSend,   std::string("0")));
        emit dataResponse(d);
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

// ── File send ─────────────────────────────────────────────────────────────────

void NetworkManager::sendFileAsync(const std::string &path)
{
    // Called from io_context thread (via boost::asio::post in startGenerator).
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[C][NM] Cannot open file: " << path << "\n";
        return;
    }

    std::vector<uint8_t> raw(
        (std::istreambuf_iterator<char>(file)),
         std::istreambuf_iterator<char>());

    std::vector<uint8_t> encrypted = encryptXor(raw);

    nlohmann::json envelope = nlohmann::json::array();
    envelope.push_back({
        { "FILE_DATA", nlohmann::json::binary(encrypted) }
    });

    postSend(envelope);
}

// static
std::vector<uint8_t> NetworkManager::encryptXor(const std::vector<uint8_t> &data,
                                                 uint8_t key)
{
    std::vector<uint8_t> out(data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key;
    return out;
}

// ── Commands ──────────────────────────────────────────────────────────────────

void NetworkManager::startGenerator(const genParams &params)
{
    if (!m_running.load(std::memory_order_relaxed)) return;

    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    static_cast<int>(jsonHeaders::TypeC::Request)         },
        { jsonHeaders::Command, static_cast<int>(jsonHeaders::CommandC::Start)        },
        { "params",             params }   // to_json(genParams) fires automatically
    });
    postSend(arr);

    if (params.mode == GeneratorMode::PcapPlayer &&
        !params.playerSettings.filePath.empty())
    {
        boost::asio::post(m_ioContext,
            [this, path = params.playerSettings.filePath]{ sendFileAsync(path); });
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