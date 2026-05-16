#include "NetworkManager.hpp"
#include <iostream>
#include <stdexcept>

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

NetworkManager::NetworkManager(uint16_t port)
    : m_ioContext()
    , m_acceptor(m_ioContext)
    , m_socket(m_ioContext)
{
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();

    m_generator.onProgress = [this](const ServerStats &s){ pushStats(s); };
}

NetworkManager::~NetworkManager()
{
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writerRunning.store(false);
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

    // Send device info — client only checks key presence, not values
    nlohmann::json devInfo = nlohmann::json::array();
    devInfo.push_back(nlohmann::json::object({
        { jsonHeaders::InterfaceName,     "" },
        { jsonHeaders::DpdkSupported,     false },
        { jsonHeaders::PfRingSupported,   false },
        { jsonHeaders::PfRingZcSupported, false }
    }));
    syncWrite(devInfo);

    // ── Start writer thread ───────────────────────────────────────────────────
    m_writerRunning.store(true);
    m_writerThread = std::thread(&NetworkManager::writerLoop, this);

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
        if (len == 0)
            continue;

        accumulator.append(buf.data(), len);

        while (!accumulator.empty()) {
            nlohmann::json doc = nlohmann::json::parse(
                accumulator, nullptr, /*exceptions=*/false);

            if (doc.is_discarded())
                break;

            accumulator.clear();

            if (!doc.is_array() || doc.empty()) {
                std::cerr << "[S][NM] Expected JSON array\n";
                continue;
            }
            handleCommand(doc.at(0));
        }
    }

    // ── Tear down writer thread ───────────────────────────────────────────────
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writerRunning.store(false);
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
                return !m_writeQueue.empty() || !m_writerRunning.load();
            });

            if (!m_writerRunning.load() && m_writeQueue.empty())
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
            m_writerRunning.store(false);
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

    case jsonHeaders::CommandC::Start:
        std::cout << "[S][NM] START\n";
        m_generator.doStart(parseParams(obj));
        sendAck(jsonHeaders::CommandC::Start, true);
        break;

    case jsonHeaders::CommandC::Pause:
        std::cout << "[S][NM] PAUSE\n";
        m_generator.doPause();
        sendAck(jsonHeaders::CommandC::Pause, true);
        break;

    case jsonHeaders::CommandC::Resume:
        std::cout << "[S][NM] RESUME\n";
        m_generator.doResume();
        sendAck(jsonHeaders::CommandC::Resume, true);
        break;

    case jsonHeaders::CommandC::Finish:
        std::cout << "[S][NM] FINISH\n";
        m_generator.doStop();
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