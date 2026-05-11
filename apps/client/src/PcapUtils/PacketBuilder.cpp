#include "PacketBuilder.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────

QByteArray PacketBuilder::macToBytes(const QString &mac)
{
    // Accepts "AA:BB:CC:DD:EE:FF" or "AA-BB-CC-DD-EE-FF"
    QByteArray out;
    const QStringList parts = mac.split(QRegularExpression("[:\\-]"));
    for (const QString &p : parts)
        out.append(static_cast<char>(p.toUInt(nullptr, 16)));
    // Pad to 6 bytes if malformed input
    while (out.size() < 6) out.append('\0');
    return out.left(6);
}

QByteArray PacketBuilder::ipv4ToBytes(const QString &ip)
{
    QByteArray out;
    const QStringList parts = ip.split('.');
    for (const QString &p : parts)
        out.append(static_cast<char>(p.toUInt()));
    while (out.size() < 4) out.append('\0');
    return out.left(4);
}

QByteArray PacketBuilder::ipv6ToBytes(const QString &ip)
{
    // Use Qt's QHostAddress to parse IPv6 properly
    QHostAddress addr(ip);
    Q_IPV6ADDR raw = addr.toIPv6Address();
    return QByteArray(reinterpret_cast<const char*>(raw.c), 16);
}

quint16 PacketBuilder::internetChecksum(const QByteArray &data)
{
    quint32 sum = 0;
    const quint16 *ptr = reinterpret_cast<const quint16*>(data.constData());
    int len = data.size();

    while (len > 1) {
        sum += qFromBigEndian(*ptr++);
        len -= 2;
    }
    if (len == 1)
        sum += static_cast<quint8>(*reinterpret_cast<const quint8*>(ptr)) << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~static_cast<quint16>(sum);
}

quint16 PacketBuilder::tcpUdpChecksum(const QByteArray &pseudoHeader,
                                       const QByteArray &segment)
{
    return internetChecksum(pseudoHeader + segment);
}

// ── Layer builders ────────────────────────────────────────────────────────────

QByteArray PacketBuilder::buildICMP(const QPacket::IcmpFields &icmp)
{
    QByteArray seg;
    QDataStream ds(&seg, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    ds << (quint8) icmp.type.toUInt()
       << (quint8) icmp.code.toUInt()
       << (quint16)0                    // checksum placeholder
       << (quint16)icmp.identifier.toUInt()
       << (quint16)icmp.sequence.toUInt();

    if (!icmp.payloadAuto)
        seg.append(icmp.payload.toLatin1());

    // Checksum
    const quint16 cksum = icmp.checksumAuto
                              ? internetChecksum(seg)
                              : icmp.checksum.toUInt(nullptr, 16);
    seg[2] = (cksum >> 8) & 0xFF;
    seg[3] =  cksum        & 0xFF;

    return seg;
}

QByteArray PacketBuilder::buildUDP(const QPacket::UdpFields &udp, const QByteArray &payload)
{
    QByteArray seg;
    QDataStream ds(&seg, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    const quint16 totalLen = udp.totalLengthAuto
                                 ? 8 + payload.size()
                                 : udp.totalLength.toUInt();

    ds << (quint16)udp.srcPort.toUInt()
       << (quint16)udp.dstPort.toUInt()
       << (quint16)totalLen
       << (quint16)0;                   // checksum placeholder

    seg.append(payload);

    // Checksum — left as 0 (optional in UDP over IPv4)
    if (!udp.checksumAuto) {
        const quint16 cksum = udp.checksum.toUInt(nullptr, 16);
        seg[6] = (cksum >> 8) & 0xFF;
        seg[7] =  cksum        & 0xFF;
    }

    return seg;
}

QByteArray PacketBuilder::buildTCP(const QPacket::TcpFields &tcp, const QByteArray &payload)
{
    QByteArray seg;
    QDataStream ds(&seg, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    // Flags byte
    quint8 flags = 0;
    if (tcp.customFlags) {
        flags = static_cast<quint8>(tcp.customFlagsValue.toUInt(nullptr, 16));
    } else {
        if (tcp.flagACE) flags |= 0x01; // NS/ACE
        if (tcp.flagECE) flags |= 0x40;
        if (tcp.flagURG) flags |= 0x20;
        if (tcp.flagACK) flags |= 0x10;
        if (tcp.flagPSH) flags |= 0x08;
        if (tcp.flagRST) flags |= 0x04;
        if (tcp.flagSYN) flags |= 0x02;
        if (tcp.flagFIN) flags |= 0x01;
    }

    const quint8 dataOffset = tcp.dataOffsetAuto
                                  ? 0x50          // 5 * 4 = 20 bytes, no options
                                  : static_cast<quint8>(tcp.dataOffset.toUInt()) << 4;

    ds << (quint16)tcp.srcPort.toUInt()
       << (quint16)tcp.dstPort.toUInt()
       << (quint32)tcp.seqNum.toUInt()
       << (quint32)tcp.ackNum.toUInt()
       << (quint8) dataOffset
       << (quint8) flags
       << (quint16)tcp.windowSize.toUInt()
       << (quint16)0                              // checksum placeholder
       << (quint16)tcp.urgentPointer.toUInt();

    seg.append(payload);

    // Checksum — computed later in buildIPv4 where we have the pseudo-header
    if (!tcp.checksumAuto) {
        const quint16 cksum = tcp.checksum.toUInt(nullptr, 16);
        seg[16] = (cksum >> 8) & 0xFF;
        seg[17] =  cksum        & 0xFF;
    }

    return seg;
}

QByteArray PacketBuilder::buildARP(const QPacket::ArpFields &arp)
{
    QByteArray seg;
    QDataStream ds(&seg, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    const quint16 hwType  = arp.hwTypeAuto ? 0x0001 : arp.hwType.toUInt(nullptr, 16);
    const quint16 pType   = arp.pTypeAuto  ? 0x0800 : arp.pType.toUInt(nullptr, 16);
    const quint8  hwSize  = arp.hwSizeAuto ? 6      : arp.hwSize.toUInt();
    const quint8  pSize   = arp.pSizeAuto  ? 4      : arp.pSize.toUInt();

    ds << hwType << pType << hwSize << pSize
       << (quint16)arp.operation.toUInt();

    seg.append(macToBytes(arp.srcMAC));
    seg.append(ipv4ToBytes(arp.srcIP));
    seg.append(macToBytes(arp.dstMAC));
    seg.append(ipv4ToBytes(arp.dstIP));

    return seg;
}

QByteArray PacketBuilder::buildIPv4(const QPacket::Packet &pkt)
{
    // Build transport segment first (need its size for IP total length)
    QByteArray transport;
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:  transport = buildTCP(pkt.tcp, pkt.payload);  break;
    case QPacket::TransportProtocol::UDP:  transport = buildUDP(pkt.udp, pkt.payload);  break;
    case QPacket::TransportProtocol::ICMP: transport = buildICMP(pkt.icmp);             break;
    case QPacket::TransportProtocol::Custom:
        transport = QByteArray::fromHex(QString(pkt.customTransProto).remove(' ').toLatin1());
        break;
    default:
        transport = pkt.payload;
        break;
    }

    // Protocol number
    quint8 proto = 0;
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:  proto = 6;   break;
    case QPacket::TransportProtocol::UDP:  proto = 17;  break;
    case QPacket::TransportProtocol::ICMP: proto = 1;   break;
    default:                      proto = 0xFF; break;
    }

    QByteArray hdr;
    QDataStream ds(&hdr, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    const quint8  version = pkt.ipv4.versionAuto  ? 0x45 : // ver=4, IHL=5
                            (pkt.ipv4.version.toUInt() << 4) | 5;
    const quint16 totalLen = pkt.ipv4.totalLenAuto ? 20 + transport.size() :
                             pkt.ipv4.totalLength.toUInt();

    ds << version
       << (quint8) pkt.ipv4.dscpEcn.toUInt(nullptr, 16)
       << totalLen
       << (quint16)0                                       // identification (0 for simplicity)
       << (quint16)(pkt.ipv4.flags.toUInt(nullptr,16) << 13
                    | pkt.ipv4.fragOffset.toUInt())        // flags + frag offset
       << (quint8) pkt.ipv4.ttl.toUInt()
       << proto
       << (quint16)0;                                      // checksum placeholder

    hdr.append(ipv4ToBytes(pkt.ipv4.srcIP));
    hdr.append(ipv4ToBytes(pkt.ipv4.dstIP));

    // IP checksum
    const quint16 ipCksum = pkt.ipv4.checksumAuto
                                ? internetChecksum(hdr)
                                : pkt.ipv4.checksum.toUInt(nullptr, 16);
    hdr[10] = (ipCksum >> 8) & 0xFF;
    hdr[11] =  ipCksum        & 0xFF;

    // TCP checksum needs pseudo-header now that we have src/dst IP
    if (pkt.transProto == QPacket::TransportProtocol::TCP && pkt.tcp.checksumAuto) {
        QByteArray pseudo;
        QDataStream ps(&pseudo, QIODevice::WriteOnly);
        ps.setByteOrder(QDataStream::BigEndian);
        pseudo.append(ipv4ToBytes(pkt.ipv4.srcIP));
        pseudo.append(ipv4ToBytes(pkt.ipv4.dstIP));
        ps << (quint8)0 << proto << (quint16)transport.size();
        const quint16 tcksum = tcpUdpChecksum(pseudo, transport);
        transport[16] = (tcksum >> 8) & 0xFF;
        transport[17] =  tcksum        & 0xFF;
    }

    return hdr + transport;
}

QByteArray PacketBuilder::buildIPv6(const QPacket::Packet &pkt)
{
    QByteArray transport;
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:  transport = buildTCP(pkt.tcp, pkt.payload);  break;
    case QPacket::TransportProtocol::UDP:  transport = buildUDP(pkt.udp, pkt.payload);  break;
    case QPacket::TransportProtocol::ICMP: transport = buildICMP(pkt.icmp);             break;
    default:                      transport = pkt.payload;                      break;
    }

    quint8 nextHeader = 59; // No next header
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:  nextHeader = 6;  break;
    case QPacket::TransportProtocol::UDP:  nextHeader = 17; break;
    case QPacket::TransportProtocol::ICMP: nextHeader = 58; break; // ICMPv6
    default: break;
    }

    QByteArray hdr;
    QDataStream ds(&hdr, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    const quint8  tc       = pkt.ipv6.trafficClass.toUInt(nullptr, 16);
    const quint32 flowLabel= pkt.ipv6.flowLabel.toUInt(nullptr, 16) & 0xFFFFF;
    const quint32 vtcfl    = (6u << 28) | (tc << 20) | flowLabel;
    const quint16 payLen   = pkt.ipv6.payloadLenAuto
                                 ? transport.size()
                                 : pkt.ipv6.payloadLength.toUInt();
    const quint8  hopLimit = pkt.ipv6.hopLimit.toUInt();

    ds << vtcfl << payLen << nextHeader << hopLimit;
    hdr.append(ipv6ToBytes(pkt.ipv6.srcIP));
    hdr.append(ipv6ToBytes(pkt.ipv6.dstIP));

    return hdr + transport;
}

QByteArray PacketBuilder::buildEthernet(const QPacket::Packet &pkt)
{
    QByteArray frame;

    frame.append(macToBytes(pkt.ethernet.dstMAC));
    frame.append(macToBytes(pkt.ethernet.srcMAC));

    // EtherType
    quint16 etherType = 0x0000;
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::IPv4:   etherType = 0x0800; break;
    case QPacket::NetworkProtocol::IPv6:   etherType = 0x86DD; break;
    case QPacket::NetworkProtocol::ARP:    etherType = 0x0806; break;
    case QPacket::NetworkProtocol::Custom: etherType = 0xFFFF; break;
    default:                      etherType = 0x0000; break;
    }

    frame.append(static_cast<char>((etherType >> 8) & 0xFF));
    frame.append(static_cast<char>( etherType        & 0xFF));

    return frame;
}

// ── Entry point ───────────────────────────────────────────────────────────────

QByteArray PacketBuilder::build(const QPacket::Packet &pkt)
{
    QByteArray frame = buildEthernet(pkt);

    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::ARP:
        frame.append(buildARP(pkt.arp));
        break;
    case QPacket::NetworkProtocol::IPv4:
        frame.append(buildIPv4(pkt));
        break;
    case QPacket::NetworkProtocol::IPv6:
        frame.append(buildIPv6(pkt));
        break;
    case QPacket::NetworkProtocol::Custom:
        frame.append(QByteArray::fromHex(QString(pkt.customNetProto).remove(' ').toLatin1()));
        break;
    default:
        frame.append(pkt.payload);
        break;
    }

    return frame;
}

QByteArray PacketBuilder::pcapGlobalHeader()
{
    QByteArray hdr;
    QDataStream ds(&hdr, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << (quint32)0xA1B2C3D4  // magic
       << (quint16)2            // major version
       << (quint16)4            // minor version
       << (qint32) 0            // GMT offset
       << (quint32)0            // timestamp accuracy
       << (quint32)65535        // snaplen
       << (quint32)1;           // LINKTYPE_ETHERNET

    return hdr;
}

QByteArray PacketBuilder::pcapPacketRecord(const QByteArray &frame, const QPacket::Packet &pkt)
{
    quint32 tsSec, tsUsec;
    tsSec  = pkt.ethernet.timestamp.toSecsSinceEpoch();
    tsUsec = pkt.ethernet.timestampUsec;

    QByteArray rec;
    QDataStream ds(&rec, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << tsSec
       << tsUsec
       << (quint32)frame.size()
       << (quint32)frame.size();
    rec.append(frame);

    return rec;
}

bool PacketBuilder::saveToFile(const QString &path, const QList<QPacket::Packet> &packets)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;

    f.write(pcapGlobalHeader());
    for (const QPacket::Packet &pkt : packets)
        f.write(pcapPacketRecord(build(pkt), pkt));

    return true;
}