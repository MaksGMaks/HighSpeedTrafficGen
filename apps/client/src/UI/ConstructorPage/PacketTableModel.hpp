#pragma once
#include "../../../../../third-party/pcapplusplus/include/pcapplusplus/Packet.h"
#include "../commonUI.hpp"
#include <QAbstractTableModel>
#include <QBrush>
#include <QColor>
#include <QList>

class PacketTableModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  explicit PacketTableModel(QList<QPacket::Packet> &packets,
                            QObject *parent = nullptr)
      : QAbstractTableModel(parent), m_packets(packets) {}

  void setTheme(AppTheme theme) { m_theme = theme; }

  int rowCount(const QModelIndex &parent = {}) const override
  {
    if (parent.isValid()) return 0;
    return m_packets.size();  // size() is always safe
  }

  void beginResetModel() { QAbstractTableModel::beginResetModel(); }
  void endResetModel()   { QAbstractTableModel::endResetModel();   }

  int columnCount(const QModelIndex &parent = {}) const override
  { return parent.isValid() ? 0 : 6; }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  // Call these instead of touching the table directly
  void packetAdded();                        // one appended to m_packets
  void packetUpdated(int index);             // one edited in place
  void packetRemoved(int index);             // one removed from m_packets
  void fullReload();                         // whole list replaced

private:
  QList<QPacket::Packet> &m_packets;         // reference — no copy
  AppTheme m_theme = AppTheme::Light;

  QString resolveSource(const QPacket::Packet &pkt)      const;
  QString resolveDestination(const QPacket::Packet &pkt) const;
  QString resolveProtocol(const QPacket::Packet &pkt)    const;
  QString resolveTime(const QPacket::Packet &pkt)        const;
  int     resolveLength(const QPacket::Packet &pkt)      const;
  QString resolveDataPreview(const QPacket::Packet &pkt) const;
  QColor  resolveRowColor(const QPacket::Packet &pkt)    const;
};