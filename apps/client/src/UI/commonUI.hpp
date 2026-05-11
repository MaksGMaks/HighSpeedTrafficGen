#pragma once

#include <QString>
#include <QByteArray>
#include <QDateTime>

namespace QPacket {
    enum class NetworkProtocol  { None, ARP, IPv4, IPv6, Custom };
    enum class TransportProtocol{ None, TCP, UDP, ICMP, Custom  };

    // ── Sub-structs ───────────────────────────────────────────────────────────────

    struct EthernetFields {
        QString srcMAC;
        QString dstMAC;
        QString fcs;
        QDateTime timestamp;
        quint32   timestampUsec;
    };

    struct ArpFields {
        QString operation;
        QString dstIP, dstMAC;
        QString srcIP, srcMAC;
        // Auto fields (only filled if !auto)
        QString hwType, hwSize, pType, pSize;
        bool hwTypeAuto = true, hwSizeAuto = true,
             pTypeAuto  = true, pSizeAuto  = true;
    };

    struct IPv4Fields {
        QString dstIP, srcIP;
        QString ttl, flags, fragOffset, dscpEcn;
        // Auto fields
        QString version, ihl, checksum, totalLength;
        bool versionAuto = true, ihlAuto    = true,
             checksumAuto= true, totalLenAuto = true;
    };

    struct IPv6Fields {
        QString dstIP, srcIP;
        QString hopLimit, nextHeader, flowLabel, trafficClass;
        // Auto fields
        QString version, payloadLength;
        bool versionAuto = true, payloadLenAuto = true;
    };

    struct TcpFields {
        QString dstPort, srcPort;
        QString seqNum, ackNum;
        QString windowSize, urgentPointer, optionsList;
        // Flags
        bool flagACE=false, flagECE=false, flagURG=false,
             flagACK=false, flagPSH=false, flagRST=false,
             flagSYN=false, flagFIN=false;
        bool customFlags = false;
        QString customFlagsValue;
        // Auto fields
        QString dataOffset, checksum;
        bool dataOffsetAuto = true, checksumAuto = true;
    };

    struct UdpFields {
        QString dstPort, srcPort;
        QString checksum, totalLength;
        bool checksumAuto = true, totalLengthAuto = true;
    };

    struct IcmpFields {
        QString type, code, identifier, sequence;
        QString payload,  checksum;
        bool payloadAuto = true, checksumAuto = true;
    };

    // ── Main packet struct ────────────────────────────────────────────────────────

    struct Packet {
        EthernetFields  ethernet;

        NetworkProtocol  netProto  = NetworkProtocol::None;
        ArpFields        arp;
        IPv4Fields       ipv4;
        IPv6Fields       ipv6;
        QString          customNetProto; // raw hex string when netProto == Custom

        TransportProtocol transProto = TransportProtocol::None;
        TcpFields         tcp;
        UdpFields         udp;
        IcmpFields        icmp;
        QString           customTransProto; // raw hex when transProto == Custom

        QByteArray payload; // from dataHexEdit (already parsed to bytes)
    };
}

enum class AppTheme { Light, Dark };
// Light theme stylesheet (as a raw string)
const QString darkStyleSheet = R"(
    QMainWindow, QWidget {
        background-color: #1e1e2e;
        color: #cdd6f4;
    }
    QMenuBar {
        background-color: #181825;
        color: #cdd6f4;
        border-bottom: 1px solid #313244;
    }
    QMenuBar::item:selected {
        background-color: #313244;
    }
    QMenu {
        background-color: #181825;
        color: #cdd6f4;
        border: 1px solid #313244;
    }
    QMenu::item:selected {
        background-color: #45475a;
    }
    QMenu::separator {
        background-color: #313244;
        height: 1px;
    }
    QTabWidget::pane {
        border: 1px solid #313244;
        background-color: #1e1e2e;
    }
    QTabBar::tab {
        background-color: #181825;
        color: #cdd6f4;
        padding: 6px 14px;
        border: 1px solid #313244;
    }
    QTabBar::tab:selected {
        background-color: #313244;
        border-bottom: 2px solid #89b4fa;
    }
    QTableView {
        background-color: #181825;
        color: #cdd6f4;
        gridline-color: #313244;
        border: 1px solid #313244;
        selection-background-color: #45475a;
    }
    QHeaderView::section {
        background-color: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        padding: 4px;
    }
    QTreeWidget {
        background-color: #181825;
        color: #cdd6f4;
        border: 1px solid #313244;
    }
    QTreeWidget::item:selected {
        background-color: #45475a;
    }
    QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDateTimeEdit {
        background-color: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 2px 4px;
    }
    QLineEdit:disabled, QPlainTextEdit:disabled {
        background-color: #24273a;
        color: #6c7086;
    }
    QComboBox {
        background-color: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 2px 4px;
    }
    QComboBox QAbstractItemView {
        background-color: #181825;
        color: #cdd6f4;
        selection-background-color: #45475a;
    }
    QPushButton {
        background-color: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 4px 12px;
        min-height: 24px;
    }
    QPushButton:hover {
        background-color: #45475a;
    }
    QPushButton:pressed {
        background-color: #585b70;
    }
    QPushButton:disabled {
        background-color: #24273a;
        color: #6c7086;
    }
    QCheckBox {
        color: #cdd6f4;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 14px;
        height: 14px;
        border: 1px solid #45475a;
        border-radius: 3px;
        background-color: #313244;
    }
    QCheckBox::indicator:checked {
        background-color: #89b4fa;
        border-color: #89b4fa;
    }
    QScrollArea {
        background-color: #1e1e2e;
        border: 1px solid #313244;
    }
    QScrollBar:vertical {
        background-color: #181825;
        width: 10px;
    }
    QScrollBar::handle:vertical {
        background-color: #45475a;
        border-radius: 5px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover {
        background-color: #585b70;
    }
    QScrollBar:horizontal {
        background-color: #181825;
        height: 10px;
    }
    QScrollBar::handle:horizontal {
        background-color: #45475a;
        border-radius: 5px;
        min-width: 20px;
    }
    QProgressDialog {
        background-color: #1e1e2e;
        color: #cdd6f4;
    }
    QStatusBar {
        background-color: #181825;
        color: #cdd6f4;
        border-top: 1px solid #313244;
    }
    QLabel {
        color: #cdd6f4;
        background-color: transparent;
    }
    QStackedWidget {
        background-color: #1e1e2e;
    }
    QSplitter::handle {
        background-color: #313244;
    }
    QScrollArea > QWidget > QWidget {
        background-color: #1e1e2e;
    }
    QTabWidget > QWidget {
        background-color: #1e1e2e;
    }
    QStackedWidget > QWidget {
        background-color: #1e1e2e;
    }
    QFrame {
        background-color: #1e1e2e;
        color: #cdd6f4;
    }
    QFrame[frameShape="4"],   /* HLine */
    QFrame[frameShape="5"] {  /* VLine */
        background-color: #313244;
    }
    /* Tree widget branch indicators */
    QTreeWidget::branch {
        background-color: #181825;
    }
    QTreeWidget::branch:selected {
        background-color: #45475a;
    }
    /* TableView selected row text color */
    QTableView::item:selected {
        color: #cdd6f4;
        background-color: #45475a;
    }
    /* Alternate row color */
    QTableView {
        alternate-background-color: #24273a;
    }
)";

// Dark theme stylesheet
const QString lightStyleSheet = R"(
    QMainWindow, QWidget {
        background-color: #eff1f5;
        color: #4c4f69;
    }
    QMenuBar {
        background-color: #e6e9ef;
        color: #4c4f69;
        border-bottom: 1px solid #ccd0da;
    }
    QMenuBar::item:selected {
        background-color: #ccd0da;
    }
    QMenu {
        background-color: #e6e9ef;
        color: #4c4f69;
        border: 1px solid #ccd0da;
    }
    QMenu::item:selected {
        background-color: #ccd0da;
    }
    QMenu::separator {
        background-color: #ccd0da;
        height: 1px;
    }
    QTabWidget::pane {
        border: 1px solid #ccd0da;
        background-color: #eff1f5;
    }
    QTabBar::tab {
        background-color: #e6e9ef;
        color: #4c4f69;
        padding: 6px 14px;
        border: 1px solid #ccd0da;
    }
    QTabBar::tab:selected {
        background-color: #eff1f5;
        border-bottom: 2px solid #1e66f5;
    }
    QTableView {
        background-color: #ffffff;
        color: #4c4f69;
        gridline-color: #ccd0da;
        border: 1px solid #ccd0da;
        selection-background-color: #acb0be;
    }
    QHeaderView::section {
        background-color: #e6e9ef;
        color: #4c4f69;
        border: 1px solid #ccd0da;
        padding: 4px;
    }
    QTreeWidget {
        background-color: #ffffff;
        color: #4c4f69;
        border: 1px solid #ccd0da;
    }
    QTreeWidget::item:selected {
        background-color: #acb0be;
    }
    QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDateTimeEdit {
        background-color: #ffffff;
        color: #4c4f69;
        border: 1px solid #ccd0da;
        border-radius: 4px;
        padding: 2px 4px;
    }
    QLineEdit:disabled, QPlainTextEdit:disabled {
        background-color: #e6e9ef;
        color: #acb0be;
    }
    QComboBox {
        background-color: #ffffff;
        color: #4c4f69;
        border: 1px solid #ccd0da;
        border-radius: 4px;
        padding: 2px 4px;
    }
    QComboBox QAbstractItemView {
        background-color: #ffffff;
        color: #4c4f69;
        selection-background-color: #acb0be;
    }
    QPushButton {
        background-color: #e6e9ef;
        color: #4c4f69;
        border: 1px solid #ccd0da;
        border-radius: 4px;
        padding: 4px 12px;
        min-height: 24px;
    }
    QPushButton:hover {
        background-color: #ccd0da;
    }
    QPushButton:pressed {
        background-color: #acb0be;
    }
    QPushButton:disabled {
        background-color: #eff1f5;
        color: #acb0be;
    }
    QCheckBox {
        color: #4c4f69;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 14px;
        height: 14px;
        border: 1px solid #ccd0da;
        border-radius: 3px;
        background-color: #ffffff;
    }
    QCheckBox::indicator:checked {
        background-color: #1e66f5;
        border-color: #1e66f5;
    }
    QScrollBar:vertical {
        background-color: #e6e9ef;
        width: 10px;
    }
    QScrollBar::handle:vertical {
        background-color: #acb0be;
        border-radius: 5px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover {
        background-color: #9ca0b0;
    }
    QScrollBar:horizontal {
        background-color: #e6e9ef;
        height: 10px;
    }
    QScrollBar::handle:horizontal {
        background-color: #acb0be;
        border-radius: 5px;
        min-width: 20px;
    }
    QStatusBar {
        background-color: #e6e9ef;
        color: #4c4f69;
        border-top: 1px solid #ccd0da;
    }
    QLabel {
        color: #4c4f69;
        background-color: transparent;
    }
    QStackedWidget {
        background-color: #eff1f5;
    }
    QSplitter::handle {
        background-color: #ccd0da;
    }
    QScrollArea > QWidget > QWidget {
        background-color: #eff1f5;
    }
    QTabWidget > QWidget {
        background-color: #eff1f5;
    }
    QStackedWidget > QWidget {
        background-color: #eff1f5;
    }
    QFrame {
        background-color: #eff1f5;
        color: #4c4f69;
    }
    QTreeWidget::branch {
        background-color: #ffffff;
    }
    QTreeWidget::branch:selected {
        background-color: #acb0be;
    }
    QTableView::item:selected {
        color: #4c4f69;
        background-color: #acb0be;
    }
    QTableView {
        alternate-background-color: #f5f5ff;
    }
)";