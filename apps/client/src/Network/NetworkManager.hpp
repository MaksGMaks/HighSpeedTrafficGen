#pragma once
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <QObject>

#include "../../../../third-party/nlohmann_json/nlohmann/json.hpp"
#include "../../../../third-party/boost/boost/asio.hpp"

#include "generator_values.hpp"

class NetworkManager : public QObject
{
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
  void receiverJSON     ();
  void dataResponse     (const jsonHeaders::receivedData &data);
  void commandAck       (jsonHeaders::CommandC cmd, bool accepted);

private:
  void startReadLoop  ();
  void doRead         ();
  void handleMessage  (const nlohmann::json &doc);
  void handleDisconnect(const boost::system::error_code &ec);

  void postSend       (const nlohmann::json &payload);
  void doSendNext     ();
  void sendFileAsync  (const std::string &path);

  static std::vector<uint8_t> encryptXor(const std::vector<uint8_t> &data,
                                         uint8_t key = 0xA5);

  boost::asio::io_context       m_ioContext;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
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
};