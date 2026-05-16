#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../../../../third-party/boost/boost/asio.hpp"
#include "../../../../third-party/nlohmann_json/nlohmann/json.hpp"

#include "generator_values.hpp"
#include "../Generator/Generator.hpp"

class NetworkManager
{
public:
    std::function<void(const std::string &ip)> onClientConnected;
    std::function<void()>                      onClientDisconnected;

    explicit NetworkManager(uint16_t port);
    ~NetworkManager();

    // Blocks until the client disconnects or an error occurs.
    // Single-client by design — call listen() again to accept the next one.
    // Returns 0 on clean disconnect, -1 on error.
    int listen();

    // Thread-safe — call from any thread (e.g. Generator callback).
    void pushStats(const nlohmann::json &payload);
    void pushStats(const ServerStats &stats);

private:
    void handleCommand  (const nlohmann::json &obj);
    genParams parseParams(const nlohmann::json &obj) const;

    void writerLoop();
    void syncWrite(const nlohmann::json &payload);

    // ── Network ───────────────────────────────────────────────────────────────
    boost::asio::io_context        m_ioContext;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::socket   m_socket;

    // ── Generator ─────────────────────────────────────────────────────────────
    Generator m_generator;

    // ── Write queue ───────────────────────────────────────────────────────────
    std::queue<std::string> m_writeQueue;
    std::mutex              m_writeMutex;
    std::condition_variable m_writeCv;
    bool                    m_writerRunning{false};
    std::thread             m_writerThread;
};