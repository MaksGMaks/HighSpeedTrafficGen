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
#include <QDateTime>
#include <QHostAddress>

class PcapReader {
public:
    static QList<QPacket::Packet> readFile(const QString &path, bool *ok = nullptr);
    static QPacket::Packet parseFrame(const QByteArray &frame,
                                  quint32 tsSec, quint32 tsUsec);

private:

    static void          parseEthernet(const QByteArray &frame, QPacket::Packet &pkt);
    static void          parseARP(const QByteArray &data, QPacket::Packet &pkt);
    static void          parseIPv4(const QByteArray &data, QPacket::Packet &pkt);
    static void          parseIPv6(const QByteArray &data, QPacket::Packet &pkt);
    static void          parseTCP(const QByteArray &data, QPacket::Packet &pkt);
    static void          parseUDP(const QByteArray &data, QPacket::Packet &pkt);
    static void          parseICMP(const QByteArray &data, QPacket::Packet &pkt);

    static QString       bytesToMac(const QByteArray &data, int offset);
    static QString       bytesToIPv4(const QByteArray &data, int offset);
    static QString       bytesToIPv6(const QByteArray &data, int offset);
    static QString       bytesToHex(const QByteArray &data);
};