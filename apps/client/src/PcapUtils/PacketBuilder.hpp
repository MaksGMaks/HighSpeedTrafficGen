#pragma once
#include "../../../third-party/pcapplusplus/include/pcapplusplus/Packet.h"
#include "../../../third-party/pcapplusplus/include/pcapplusplus/PcapDevice.h"
#include "../../../third-party/pcapplusplus/include/pcapplusplus/PcapFileDevice.h"
#include "../UI/commonUI.hpp"
#include <QByteArray>
#include <QFile>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QtEndian>
#include <QDataStream>
#include <QHostAddress>

class PacketBuilder
{
public:
  static QByteArray build(const QPacket::Packet &pkt);
  static bool saveToFile(const QString &path, const QList<QPacket::Packet> &packets);

private:
  static QByteArray buildEthernet(const QPacket::Packet &pkt);
  static QByteArray buildARP(const QPacket::ArpFields &arp);
  static QByteArray buildIPv4(const QPacket::Packet &pkt);
  static QByteArray buildIPv6(const QPacket::Packet &pkt);
  static QByteArray buildTCP(const QPacket::TcpFields &tcp, const QByteArray &payload);
  static QByteArray buildUDP(const QPacket::UdpFields &udp, const QByteArray &payload);
  static QByteArray buildICMP(const QPacket::IcmpFields &icmp);

  static QByteArray pcapGlobalHeader();
  static QByteArray pcapPacketRecord(const QByteArray &frame, const QPacket::Packet &pkt);

  // Checksum helpers
  static quint16 internetChecksum(const QByteArray &data);
  static quint16 tcpUdpChecksum(const QByteArray &pseudoHeader,
                                 const QByteArray &segment);

  static QByteArray macToBytes(const QString &mac);
  static QByteArray ipv4ToBytes(const QString &ip);
  static QByteArray ipv6ToBytes(const QString &ip);
};