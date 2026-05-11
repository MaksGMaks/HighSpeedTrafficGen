#pragma once
#include <QCheckBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QWidget>
#include <QTreeWidgetItem>

#include "../../../../../build/apps/client/HSET_GeneratorClient_autogen/include/ui_ConstructorPage.h"

#include <iostream>
#include <time.h>
#include <thread>
#include <atomic>

#include "../../PcapUtils/PacketBuilder.hpp"
#include "../../PcapUtils/PcapReader.hpp"
#include "../commonUI.hpp"
#include "PacketTableModel.hpp"

namespace Ui {
    class ConstructorPage;
}

class ConstructorPage : public QWidget {
    Q_OBJECT

public:
    explicit ConstructorPage(QWidget *parent = nullptr);
    ~ConstructorPage() override;

    PacketTableModel* tableModel() { return m_tableModel; }
    void changeEvent(QEvent *event) override;

private slots:
    // Combo Boxes status changed
    void onNPBox_valueChanged(int index);
    void onTPBox_valueChanged(int index);

    // Checkboxes state changed
    void setCustomTCPFlags(bool checked);

    // Text Helpers
    void onHexChanged();
    void onAsciiChanged();

    // Buttons
    void onOpenFileBtnClicked();
    void onSaveFileBtnClicked();
    void onCancelBtnClicked();
    void onSaveBtnClicked();
    void onEditBtnClicked();
    void onDeleteBtnClicked();
    void onClearBtnClicked();
    void onAddBtnClicked();

    void onPcapReadProgress(int current, int total);
    void onPcapReadFinished(QList<QPacket::Packet> *packets, bool ok, QString errorMsg);

    // Table
    void onPacketSelected(int row);


private:
    Ui::ConstructorPage *ui;

    QList<QPacket::Packet> m_packets;
    PacketTableModel      *m_tableModel = nullptr;

    QProgressDialog  *m_progressDialog = nullptr;

    std::thread  m_readerThread;
    std::atomic<bool> m_readerRunning{false};

    void pcapReaderThread(const QString path);

    // Setup helpers
    void setupConnection();

    // Data editor helpers
    static QString    bytesToHex(const QByteArray &data);
    static QByteArray hexToBytes(const QString &hex, bool *ok = nullptr);
    static QString    bytesToAscii(const QByteArray &data);

    bool m_syncing = false;

    // Packet editing/parsing
    QPacket::Packet     parseFieldsToPacket();
    void                enterEditMode(int packetIndex);
    void                exitEditMode();
    void                populateFieldsFromPacket(const QPacket::Packet &pkt);  // for Edit
    QByteArray          hexEditToBytes(const QString &hexStr);

    // Table
    void populatePacketTree(const QPacket::Packet &pkt);
    QTreeWidgetItem* makeSection(const QString &title);
    QTreeWidgetItem* makeField(const QString &name, const QString &value);

    QString resolveProtocolString(const QPacket::Packet &pkt);
    QString resolveSource(const QPacket::Packet &pkt);
    QString resolveDestination(const QPacket::Packet &pkt);
    int  m_editingIndex = -1;

    // Clean/Reset for elements
    void clearLineEdits(QWidget *parent);
    void resetARPPage();
    void resetIP4Page();
    void resetIP6Page();
    void resetTCPPage();
    void resetUDPPage();
    void resetICMPPage();
    void resetDATAPage();
    void resetTCPFlags();

    void setToCurrentTime(bool state);
};