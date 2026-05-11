#include "NetworkManager.hpp"

#include <fstream>

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
        m_workGuard.reset();
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

        m_socket   = boost::asio::ip::tcp::socket(m_ioContext);
        m_resolver = boost::asio::ip::tcp::resolver(m_ioContext);

        boost::system::error_code resolveEc;
        boost::asio::ip::tcp::resolver::results_type endpoints =
            m_resolver.resolve(ip, std::to_string(port), resolveEc);

        if (resolveEc) {
            std::cerr << "[C][NM] Resolve failed: " << resolveEc.message() << "\n";
            return;
        }

        boost::asio::async_connect(m_socket, endpoints,
            [this](boost::system::error_code ec,
                   const boost::asio::ip::tcp::endpoint &ep)
            {
                if (ec) {
                    std::cerr << "[C][NM] Connect failed: " << ec.message() << "\n";
                    return;
                }

                std::cout << "[C][NM] Connected to " << ep << "\n";
                m_running = true;
                m_socket.set_option(boost::asio::ip::tcp::no_delay(true));

                startReadLoop();
                emit connected();
            });
    });
}

void NetworkManager::disconnect()
{
    boost::asio::post(m_ioContext, [this]{
        if (!m_socket.is_open()) return;
        m_running = false;
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
    if (!m_running) return;
    std::cerr << "[C][NM] Lost connection: " << ec.message() << "\n";
    m_running = false;
    boost::system::error_code closeEc;
    m_socket.close(closeEc);
    emit disconnectNotify();
}

// ── Read loop — same accumulator strategy as server ───────────────────────────
// No length framing on the wire: accumulate until nlohmann accepts the JSON.

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

            // Try to parse accumulated bytes
            nlohmann::json doc = nlohmann::json::parse(
                m_readAccumulator, nullptr, /*exceptions*/false);

            if (!doc.is_discarded()) {
                m_readAccumulator.clear();
                handleMessage(doc);
            }
            // else: incomplete — keep reading

            doRead();
        });
}

void NetworkManager::handleMessage(const nlohmann::json &doc)
{
    if (!doc.is_array() || doc.empty()) {
        std::cerr << "[C][NM] Expected JSON array\n";
        return;
    }

    const nlohmann::json &obj = doc.at(0);

    // ── Device info (first message after connect) ──────────────────────────
    if (obj.contains(jsonHeaders::InterfaceName)) {

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

    // ── Stat update ───────────────────────────────────────────────────────
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

// ── Write queue ───────────────────────────────────────────────────────────────

void NetworkManager::postSend(const nlohmann::json &payload)
{
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
        [this, frame](boost::system::error_code ec, std::size_t)
        {
            if (ec) { handleDisconnect(ec); return; }
            m_writeQueue.erase(m_writeQueue.begin());
            doSendNext();
        });
}

// ── File send ─────────────────────────────────────────────────────────────────

void NetworkManager::sendFileAsync(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[C][NM] Cannot open file: " << path << "\n";
        return;
    }

    std::vector<uint8_t> raw(
        (std::istreambuf_iterator<char>(file)),
         std::istreambuf_iterator<char>());

    std::vector<uint8_t> encrypted = encryptXor(raw);

    // Wrap encrypted bytes as base64 string inside a JSON envelope
    // so it stays valid JSON on the wire
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
    if (!m_running) return;

    nlohmann::json obj;
    obj[jsonHeaders::Type]                       = jsonHeaders::REQUEST;
    obj[jsonHeaders::Command]                    = jsonHeaders::START;
    obj[jsonHeaders::Parameters::Mode]           = std::to_string(params.mode);
    obj[jsonHeaders::Parameters::Time]           = std::to_string(params.time);
    obj[jsonHeaders::Parameters::Speed]          = std::to_string(params.speed);
    obj[jsonHeaders::Parameters::BurstSize]      = std::to_string(params.burstSize);
    obj[jsonHeaders::Parameters::PackSize]       = std::to_string(params.packSize);
    obj[jsonHeaders::Parameters::PacketPattern]  = std::to_string(params.packetPattern);
    obj[jsonHeaders::Parameters::FileSend]       = params.fileSend;
    obj[jsonHeaders::Parameters::Copies]         = std::to_string(params.copies);
    obj[jsonHeaders::Parameters::TotalSend]      = std::to_string(params.totalSend);

    nlohmann::json arr = nlohmann::json::array();
    arr.push_back(obj);
    postSend(arr);

    if (params.fileSend && !params.filePath.empty()) {
        boost::asio::post(m_ioContext,
            [this, path = params.filePath]{ sendFileAsync(path); });
    }
}

void NetworkManager::pauseGenerator()
{
    if (!m_running) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::PAUSE   }
    });
    postSend(arr);
}

void NetworkManager::resumeGenerator()
{
    if (!m_running) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::RESUME  }
    });
    postSend(arr);
}

void NetworkManager::stopGenerator()
{
    if (!m_running) return;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back({
        { jsonHeaders::Type,    jsonHeaders::REQUEST },
        { jsonHeaders::Command, jsonHeaders::FINISH  }
    });
    postSend(arr);
}