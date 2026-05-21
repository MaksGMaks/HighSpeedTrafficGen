#pragma once
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <iostream>

#include <QObject>

#include "../../../../third-party/nlohmann_json/nlohmann/json.hpp"
#include "../../../../third-party/boost/boost/asio.hpp"

#include "generator_values.hpp"

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void connect   (const std::string &ip, uint16_t port);
    void disconnect();

public slots:
    void startGenerator (const genParams &params);
    void pauseGenerator ();
    void resumeGenerator();
    void stopGenerator  ();

signals:
    void connected        ();
    void disconnectNotify ();
    void statsReceived    (const ServerStats &stats);
    void messageReceived  (MessageSeverity severity,
                           const QString &text);        // new
    void commandAck       (jsonHeaders::CommandC cmd, bool accepted);
    void fileSendProgress (int percent);   // 0-100, emitted during chunked send

private:
    void startReadLoop   ();
    void doRead          ();
    void handleMessage   (const nlohmann::json &doc);
    void handleDisconnect(const boost::system::error_code &ec);

    void postSend        (const nlohmann::json &payload);
    void doSendNext      ();

    // Reads file at path, XOR-encrypts, base64-encodes, sends as JSON chunks.
    // Each chunk: [{"FILE_DATA": "<base64>", "chunk": N, "total": T}]
    // Final chunk also carries "eof": true so server knows transfer is complete.
    void sendFileChunked (const std::string &path);

    static std::vector<uint8_t> encryptXor (const std::vector<uint8_t> &data,
                                            uint8_t key = 0xA5);
    static std::string          toBase64   (const std::vector<uint8_t> &data);

    boost::asio::io_context       m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
                                  m_workGuard;
    boost::asio::ip::tcp::socket   m_socket;
    boost::asio::ip::tcp::resolver m_resolver;
    std::thread                    m_ioThread;

    // Read state — io_context thread only
    std::array<char, 4096> m_readBuf{};
    std::string            m_readAccumulator;

    // Write state — io_context thread only
    bool m_writing = false;
    std::vector<std::shared_ptr<std::string>> m_writeQueue;

    std::atomic<bool> m_running{false};

    // Chunk size for file transfer (bytes of raw data per JSON message)
    static constexpr std::size_t FILE_CHUNK_BYTES = 65536; // 64 KB raw → ~88 KB base64
};