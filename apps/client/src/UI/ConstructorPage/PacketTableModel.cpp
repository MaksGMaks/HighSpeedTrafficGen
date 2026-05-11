#include "PacketTableModel.hpp"
#include <QDateTime>

// ── Header ────────────────────────────────────────────────────────────────────

QVariant PacketTableModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    switch (section) {
    case 0: return "Time";
    case 1: return "Source";
    case 2: return "Destination";
    case 3: return "Protocol";
    case 4: return "Length";
    case 5: return "Data";
    }
    return {};
}

// ── Cell helpers ──────────────────────────────────────────────────────────────

QString PacketTableModel::resolveTime(const QPacket::Packet &pkt) const {
    return pkt.ethernet.timestamp.toString("yyyy-MM-dd hh:mm:ss")
           + QString(".%1").arg(pkt.ethernet.timestampUsec, 6, 10, QChar('0'));
}

QString PacketTableModel::resolveProtocol(const QPacket::Packet &pkt) const
{
    using TP = QPacket::TransportProtocol;
    using NP = QPacket::NetworkProtocol;

    switch (pkt.transProto) {
    case TP::TCP:    return "TCP";
    case TP::UDP:    return "UDP";
    case TP::ICMP:   return "ICMP";
    case TP::Custom: return "Custom";
    default: break;
    }
    switch (pkt.netProto) {
    case NP::ARP:    return "ARP";
    case NP::IPv4:   return "IPv4";
    case NP::IPv6:   return "IPv6";
    case NP::Custom: return "Custom";
    default:         return "Ethernet";
    }
}

QString PacketTableModel::resolveSource(const QPacket::Packet &pkt) const
{
    using NP = QPacket::NetworkProtocol;
    switch (pkt.netProto) {
    case NP::IPv4: return pkt.ipv4.srcIP;
    case NP::IPv6: return pkt.ipv6.srcIP;
    case NP::ARP:  return pkt.arp.srcIP.isEmpty() ? pkt.arp.srcMAC : pkt.arp.srcIP;
    default:       return pkt.ethernet.srcMAC;
    }
}

QString PacketTableModel::resolveDestination(const QPacket::Packet &pkt) const
{
    using NP = QPacket::NetworkProtocol;
    switch (pkt.netProto) {
    case NP::IPv4: return pkt.ipv4.dstIP;
    case NP::IPv6: return pkt.ipv6.dstIP;
    case NP::ARP:  return pkt.arp.dstIP.isEmpty() ? pkt.arp.dstMAC : pkt.arp.dstIP;
    default:       return pkt.ethernet.dstMAC;
    }
}

int PacketTableModel::resolveLength(const QPacket::Packet &pkt) const
{
    using NP = QPacket::NetworkProtocol;
    using TP = QPacket::TransportProtocol;

    int length = 14;
    switch (pkt.netProto) {
    case NP::IPv4: length += 20; break;
    case NP::IPv6: length += 40; break;
    case NP::ARP:  length += 28; break;
    default: break;
    }
    switch (pkt.transProto) {
    case TP::TCP:  length += 20; break;
    case TP::UDP:  length +=  8; break;
    case TP::ICMP: length +=  8; break;
    default: break;
    }
    return length + pkt.payload.size();
}

QString PacketTableModel::resolveDataPreview(const QPacket::Packet &pkt) const
{
    if (pkt.payload.isEmpty()) return "(no payload)";

    QString out;
    const int n = qMin(pkt.payload.size(), 8);
    for (int i = 0; i < n; ++i) {
        if (i > 0) out += ' ';
        out += QString("%1").arg((quint8)pkt.payload[i], 2, 16, QChar('0')).toUpper();
    }
    if (pkt.payload.size() > 8) out += " …";
    return out;
}

QColor PacketTableModel::resolveRowColor(const QPacket::Packet &pkt) const
{
    using TP = QPacket::TransportProtocol;
    using NP = QPacket::NetworkProtocol;

    if (m_theme == AppTheme::Dark) {
        switch (static_cast<int>(pkt.transProto)) {
        case static_cast<int>(TP::TCP):  return QColor(0x1a, 0x2a, 0x3a); // dark blue
        case static_cast<int>(TP::UDP):  return QColor(0x1a, 0x2e, 0x1a); // dark green
        case static_cast<int>(TP::ICMP): return QColor(0x2e, 0x24, 0x10); // dark orange
        default:
            if (static_cast<int>(pkt.netProto) ==
                static_cast<int>(NP::ARP))
                return QColor(0x2a, 0x1a, 0x2e);                           // dark purple
            return QColor(0x18, 0x18, 0x25);                               // base dark
        }
    } else {
        switch (static_cast<int>(pkt.transProto)) {
        case static_cast<int>(TP::TCP):  return QColor(0xE7, 0xF3, 0xFF);
        case static_cast<int>(TP::UDP):  return QColor(0xDA, 0xFF, 0xDA);
        case static_cast<int>(TP::ICMP): return QColor(0xFF, 0xF0, 0xCC);
        default:
            if (static_cast<int>(pkt.netProto) ==
                static_cast<int>(NP::ARP))
                return QColor(0xFF, 0xE8, 0xFF);
            return Qt::white;
        }
    }
}

// ── data() ────────────────────────────────────────────────────────────────────

QVariant PacketTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_packets.size())
        return {};

    const QPacket::Packet &pkt = m_packets.at(index.row());

    if (role == Qt::BackgroundRole)
        return QBrush(resolveRowColor(pkt));

    if (role == Qt::UserRole)
        return index.row();

    if (role != Qt::DisplayRole)
        return {};

    qDebug() << "data() col:" << index.column() << "row:" << index.row();

    switch (index.column()) {
    case 0: { auto r = resolveTime(pkt);        qDebug() << "time ok";   return r; }
    case 1: { auto r = resolveSource(pkt);      qDebug() << "src ok";    return r; }
    case 2: { auto r = resolveDestination(pkt); qDebug() << "dst ok";    return r; }
    case 3: { auto r = resolveProtocol(pkt);    qDebug() << "proto ok";  return r; }
    case 4: { auto r = resolveLength(pkt);      qDebug() << "len ok";    return r; }
    case 5: { auto r = resolveDataPreview(pkt); qDebug() << "data ok";   return r; }
    }
    return {};
}

// ── Mutation helpers ──────────────────────────────────────────────────────────

void PacketTableModel::packetAdded()
{
    const int row = m_packets.size() - 1;
    beginInsertRows({}, row, row);
    endInsertRows();
}

void PacketTableModel::packetUpdated(int index)
{
    emit dataChanged(createIndex(index, 0),
                     createIndex(index, columnCount() - 1));
}

void PacketTableModel::packetRemoved(int index)
{
    beginRemoveRows({}, index, index);
    m_packets.removeAt(index);
    endRemoveRows();
}

void PacketTableModel::fullReload()
{
    beginResetModel();
    endResetModel();
}