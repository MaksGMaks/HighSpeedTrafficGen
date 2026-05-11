#include "PcapReader.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────

QString PcapReader::bytesToMac(const QByteArray &data, int offset)
{
    if (offset + 6 > data.size()) return {};
    return QString("%1:%2:%3:%4:%5:%6")
        .arg((quint8)data[offset+0], 2, 16, QChar('0'))
        .arg((quint8)data[offset+1], 2, 16, QChar('0'))
        .arg((quint8)data[offset+2], 2, 16, QChar('0'))
        .arg((quint8)data[offset+3], 2, 16, QChar('0'))
        .arg((quint8)data[offset+4], 2, 16, QChar('0'))
        .arg((quint8)data[offset+5], 2, 16, QChar('0'))
        .toUpper();
}

QString PcapReader::bytesToIPv4(const QByteArray &data, int offset)
{
    if (offset + 4 > data.size()) return {};
    return QString("%1.%2.%3.%4")
        .arg((quint8)data[offset+0])
        .arg((quint8)data[offset+1])
        .arg((quint8)data[offset+2])
        .arg((quint8)data[offset+3]);
}

QString PcapReader::bytesToIPv6(const QByteArray &data, int offset)
{
    if (offset + 16 > data.size()) return {};
    Q_IPV6ADDR raw;
    memcpy(raw.c, data.constData() + offset, 16);
    return QHostAddress(raw).toString();
}

QString PcapReader::bytesToHex(const QByteArray &data)
{
    QString out;
    out.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) out += ' ';
        out += QString("%1").arg((quint8)data[i], 2, 16, QChar('0')).toUpper();
    }
    return out;
}

// ── Transport parsers ─────────────────────────────────────────────────────────

void PcapReader::parseTCP(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 20) {
        // Too short — treat as custom
        pkt.transProto       = QPacket::TransportProtocol::Custom;
        pkt.customTransProto = bytesToHex(data);
        return;
    }

    pkt.transProto = QPacket::TransportProtocol::TCP;

    pkt.tcp.srcPort = QString::number(
        ((quint8)data[0] << 8) | (quint8)data[1]);
    pkt.tcp.dstPort = QString::number(
        ((quint8)data[2] << 8) | (quint8)data[3]);
    pkt.tcp.seqNum  = QString::number(
        ((quint32)(quint8)data[4] << 24) | ((quint32)(quint8)data[5] << 16) |
        ((quint32)(quint8)data[6] <<  8) |  (quint32)(quint8)data[7]);
    pkt.tcp.ackNum  = QString::number(
        ((quint32)(quint8)data[8]  << 24) | ((quint32)(quint8)data[9]  << 16) |
        ((quint32)(quint8)data[10] <<  8) |  (quint32)(quint8)data[11]);

    const quint8 dataOffset = ((quint8)data[12] >> 4) * 4; // in bytes
    pkt.tcp.dataOffsetAuto  = false;
    pkt.tcp.dataOffset      = QString::number((quint8)data[12] >> 4);

    const quint8 flags = (quint8)data[13];
    pkt.tcp.flagACE = flags & 0x01; // recheck bit mapping to your struct
    pkt.tcp.flagECE = flags & 0x40;
    pkt.tcp.flagURG = flags & 0x20;
    pkt.tcp.flagACK = flags & 0x10;
    pkt.tcp.flagPSH = flags & 0x08;
    pkt.tcp.flagRST = flags & 0x04;
    pkt.tcp.flagSYN = flags & 0x02;
    pkt.tcp.flagFIN = flags & 0x01;

    pkt.tcp.windowSize    = QString::number(
        ((quint8)data[14] << 8) | (quint8)data[15]);
    pkt.tcp.checksumAuto  = false;
    pkt.tcp.checksum      = QString("0x%1")
        .arg(((quint8)data[16] << 8) | (quint8)data[17], 4, 16, QChar('0')).toUpper();
    pkt.tcp.urgentPointer = QString::number(
        ((quint8)data[18] << 8) | (quint8)data[19]);

    // Options (if any)
    if (dataOffset > 20 && dataOffset <= data.size()) {
        pkt.tcp.optionsList = bytesToHex(data.mid(20, dataOffset - 20));
    }

    // Payload
    if (dataOffset < data.size())
        pkt.payload = data.mid(dataOffset);
}

void PcapReader::parseUDP(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 8) {
        pkt.transProto       = QPacket::TransportProtocol::Custom;
        pkt.customTransProto = bytesToHex(data);
        return;
    }

    pkt.transProto = QPacket::TransportProtocol::UDP;

    pkt.udp.srcPort         = QString::number(
        ((quint8)data[0] << 8) | (quint8)data[1]);
    pkt.udp.dstPort         = QString::number(
        ((quint8)data[2] << 8) | (quint8)data[3]);
    pkt.udp.totalLengthAuto = false;
    pkt.udp.totalLength     = QString::number(
        ((quint8)data[4] << 8) | (quint8)data[5]);
    pkt.udp.checksumAuto    = false;
    pkt.udp.checksum        = QString("0x%1")
        .arg(((quint8)data[6] << 8) | (quint8)data[7], 4, 16, QChar('0')).toUpper();

    if (data.size() > 8)
        pkt.payload = data.mid(8);
}

void PcapReader::parseICMP(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 8) {
        pkt.transProto       = QPacket::TransportProtocol::Custom;
        pkt.customTransProto = bytesToHex(data);
        return;
    }

    pkt.transProto = QPacket::TransportProtocol::ICMP;

    pkt.icmp.type         = QString::number((quint8)data[0]);
    pkt.icmp.code         = QString::number((quint8)data[1]);
    pkt.icmp.checksumAuto = false;
    pkt.icmp.checksum     = QString("0x%1")
        .arg(((quint8)data[2] << 8) | (quint8)data[3], 4, 16, QChar('0')).toUpper();
    pkt.icmp.identifier   = QString::number(
        ((quint8)data[4] << 8) | (quint8)data[5]);
    pkt.icmp.sequence     = QString::number(
        ((quint8)data[6] << 8) | (quint8)data[7]);

    if (data.size() > 8) {
        pkt.icmp.payloadAuto = false;
        pkt.icmp.payload     = bytesToHex(data.mid(8));
    }
}

// ── Network parsers ───────────────────────────────────────────────────────────

void PcapReader::parseARP(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 28) {
        pkt.netProto       = QPacket::NetworkProtocol::Custom;
        pkt.customNetProto = bytesToHex(data);
        return;
    }

    pkt.netProto = QPacket::NetworkProtocol::ARP;

    pkt.arp.hwTypeAuto = false;
    pkt.arp.hwType     = QString("0x%1")
        .arg(((quint8)data[0] << 8) | (quint8)data[1], 4, 16, QChar('0')).toUpper();
    pkt.arp.pTypeAuto  = false;
    pkt.arp.pType      = QString("0x%1")
        .arg(((quint8)data[2] << 8) | (quint8)data[3], 4, 16, QChar('0')).toUpper();
    pkt.arp.hwSizeAuto = false;
    pkt.arp.hwSize     = QString::number((quint8)data[4]);
    pkt.arp.pSizeAuto  = false;
    pkt.arp.pSize      = QString::number((quint8)data[5]);
    pkt.arp.operation  = QString::number(
        ((quint8)data[6] << 8) | (quint8)data[7]);

    pkt.arp.srcMAC = bytesToMac(data,  8);
    pkt.arp.srcIP  = bytesToIPv4(data, 14);
    pkt.arp.dstMAC = bytesToMac(data,  18);
    pkt.arp.dstIP  = bytesToIPv4(data, 24);
}

void PcapReader::parseIPv4(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 20) {
        pkt.netProto       = QPacket::NetworkProtocol::Custom;
        pkt.customNetProto = bytesToHex(data);
        return;
    }

    pkt.netProto = QPacket::NetworkProtocol::IPv4;

    const quint8  versionIHL = (quint8)data[0];
    const quint8  ihl        = (versionIHL & 0x0F) * 4; // header length in bytes
    pkt.ipv4.versionAuto  = false;
    pkt.ipv4.version      = QString::number(versionIHL >> 4);
    pkt.ipv4.ihlAuto      = false;
    pkt.ipv4.ihl          = QString::number(versionIHL & 0x0F);

    pkt.ipv4.dscpEcn      = QString("0x%1").arg((quint8)data[1], 2, 16, QChar('0')).toUpper();

    pkt.ipv4.totalLenAuto = false;
    pkt.ipv4.totalLength  = QString::number(
        ((quint8)data[2] << 8) | (quint8)data[3]);

    const quint16 flagsFrag = ((quint8)data[6] << 8) | (quint8)data[7];
    pkt.ipv4.flags      = QString::number((flagsFrag >> 13) & 0x7);
    pkt.ipv4.fragOffset = QString::number(flagsFrag & 0x1FFF);

    pkt.ipv4.ttl          = QString::number((quint8)data[8]);

    const quint8 proto    = (quint8)data[9];

    pkt.ipv4.checksumAuto = false;
    pkt.ipv4.checksum     = QString("0x%1")
        .arg(((quint8)data[10] << 8) | (quint8)data[11], 4, 16, QChar('0')).toUpper();

    pkt.ipv4.srcIP = bytesToIPv4(data, 12);
    pkt.ipv4.dstIP = bytesToIPv4(data, 16);

    // Transport
    if (ihl < data.size()) {
        const QByteArray transport = data.mid(ihl);
        switch (proto) {
        case 6:   parseTCP(transport,  pkt); break;
        case 17:  parseUDP(transport,  pkt); break;
        case 1:   parseICMP(transport, pkt); break;
        default:
            pkt.transProto       = QPacket::TransportProtocol::Custom;
            pkt.customTransProto = bytesToHex(transport);
            break;
        }
    }
}

void PcapReader::parseIPv6(const QByteArray &data, QPacket::Packet &pkt)
{
    if (data.size() < 40) {
        pkt.netProto       = QPacket::NetworkProtocol::Custom;
        pkt.customNetProto = bytesToHex(data);
        return;
    }

    pkt.netProto = QPacket::NetworkProtocol::IPv6;

    const quint32 vtcfl = ((quint8)data[0] << 24) | ((quint8)data[1] << 16) |
                          ((quint8)data[2] <<  8) |  (quint8)data[3];

    pkt.ipv6.versionAuto    = false;
    pkt.ipv6.version        = QString::number((vtcfl >> 28) & 0xF);
    pkt.ipv6.trafficClass   = QString("0x%1").arg((vtcfl >> 20) & 0xFF, 2, 16, QChar('0')).toUpper();
    pkt.ipv6.flowLabel      = QString("0x%1").arg(vtcfl & 0xFFFFF, 5, 16, QChar('0')).toUpper();

    pkt.ipv6.payloadLenAuto = false;
    pkt.ipv6.payloadLength  = QString::number(
        ((quint8)data[4] << 8) | (quint8)data[5]);

    const quint8 nextHeader = (quint8)data[6];
    pkt.ipv6.nextHeader     = QString::number(nextHeader);
    pkt.ipv6.hopLimit       = QString::number((quint8)data[7]);

    pkt.ipv6.srcIP = bytesToIPv6(data,  8);
    pkt.ipv6.dstIP = bytesToIPv6(data, 24);

    const QByteArray transport = data.mid(40);
    switch (nextHeader) {
    case 6:   parseTCP(transport,  pkt); break;
    case 17:  parseUDP(transport,  pkt); break;
    case 58:  parseICMP(transport, pkt); break; // ICMPv6
    default:
        pkt.transProto       = QPacket::TransportProtocol::Custom;
        pkt.customTransProto = bytesToHex(transport);
        break;
    }
}

// ── Ethernet ──────────────────────────────────────────────────────────────────

void PcapReader::parseEthernet(const QByteArray &frame, QPacket::Packet &pkt)
{
    if (frame.size() < 14) {
        // Malformed — dump everything as custom
        pkt.netProto       = QPacket::NetworkProtocol::Custom;
        pkt.customNetProto = bytesToHex(frame);
        return;
    }

    pkt.ethernet.dstMAC = bytesToMac(frame, 0);
    pkt.ethernet.srcMAC = bytesToMac(frame, 6);

    const quint16 etherType = ((quint8)frame[12] << 8) | (quint8)frame[13];
    const QByteArray payload = frame.mid(14);

    switch (etherType) {
    case 0x0800: parseIPv4(payload, pkt); break;
    case 0x86DD: parseIPv6(payload, pkt); break;
    case 0x0806: parseARP(payload,  pkt); break;
    default:
        pkt.netProto       = QPacket::NetworkProtocol::Custom;
        pkt.customNetProto = bytesToHex(payload);
        break;
    }
}

// ── Frame entry ───────────────────────────────────────────────────────────────

QPacket::Packet PcapReader::parseFrame(const QByteArray &frame,
                               quint32 tsSec, quint32 tsUsec)
{
    QPacket::Packet pkt;
    pkt.ethernet.timestamp        = QDateTime::fromSecsSinceEpoch(tsSec);
    pkt.ethernet.timestampUsec    = tsUsec;

    parseEthernet(frame, pkt);
    return pkt;
}

// ── Public entry ──────────────────────────────────────────────────────────────

QList<QPacket::Packet> PcapReader::readFile(const QString &path, bool *ok)
{
    QList<QPacket::Packet> packets;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return {};
    }

    QDataStream ds(&f);

    // ── Global header ─────────────────────────────────────────────────────────
    quint32 magic;
    ds.setByteOrder(QDataStream::LittleEndian);
    ds >> magic;

    if (magic == 0xD4C3B2A1) {
        // Byte-swapped file — switch to big endian
        ds.setByteOrder(QDataStream::BigEndian);
    } else if (magic != 0xA1B2C3D4) {
        if (ok) *ok = false;
        return {}; // not a pcap file
    }

    quint16 verMajor, verMinor;
    qint32  thisZone;
    quint32 sigFigs, snapLen, network;
    ds >> verMajor >> verMinor >> thisZone >> sigFigs >> snapLen >> network;

    if (network != 1) {
        // Not LINKTYPE_ETHERNET — could extend here for other link types
        if (ok) *ok = false;
        return {};
    }

    // ── Packet records ────────────────────────────────────────────────────────
    while (!f.atEnd()) {
        quint32 tsSec, tsUsec, inclLen, origLen;
        ds >> tsSec >> tsUsec >> inclLen >> origLen;

        if (ds.status() != QDataStream::Ok) break;

        const QByteArray frame = f.read(inclLen);
        if ((quint32)frame.size() != inclLen) break; // truncated file

        packets.append(parseFrame(frame, tsSec, tsUsec));
    }

    if (ok) *ok = true;
    return packets;
}