#include "ConstructorPage.hpp"

ConstructorPage::ConstructorPage(QWidget *parent) : QWidget(parent), ui(new Ui::ConstructorPage) {
    ui->setupUi(this);
    ui->npStackedWidget->setVisible(false);
    ui->tpStackedWidget->setVisible(false);
    ui->tpComboBox->setDisabled(true);
    setupConnection();

    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    ui->dataHexEdit->setFont(mono);
    ui->dataASCIIEdit->setFont(mono);
    ui->dataHexEdit->setPlaceholderText("e.g. 48 65 6C 6C 6F");
    ui->ethNSLineEdit->setValidator(new QIntValidator(0, 999999, ui->ethNSLineEdit));
    ui->saveBtn->setVisible(false);
    ui->cancelBtn->setVisible(false);

    m_tableModel = new PacketTableModel(m_packets, this);
    ui->tableView->setModel(m_tableModel);

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->setShowGrid(false);

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
        this, [this](const QModelIndex &current, const QModelIndex &) {
            if (!current.isValid()) return;
            onPacketSelected(current.row());
        });
}

ConstructorPage::~ConstructorPage() {
    if (m_readerThread.joinable()) {
        m_readerRunning = false;  // signal to stop if you add a cancellation check
        m_readerThread.join();    // wait for completion
    }
    delete ui;
}

void ConstructorPage::setupConnection() {
    // Combo Boxes to Stacked Widgets
    connect(ui->npComboBox, &QComboBox::currentIndexChanged, this, &ConstructorPage::onNPBox_valueChanged);
    connect(ui->tpComboBox, &QComboBox::currentIndexChanged, this, &ConstructorPage::onTPBox_valueChanged);

    // Check Boxes to Line Edits
    // ETH
    connect(ui->ethTimeCurrentCB, &QCheckBox::toggled, this, &ConstructorPage::setToCurrentTime);
    // ARP
    connect(ui->arpHwTypeACB, &QCheckBox::toggled, ui->arpHwTypeLineEdit, &QLineEdit::setDisabled);
    connect(ui->arpHwSizeACB, &QCheckBox::toggled, ui->arpHwSizeLineEdit, &QLineEdit::setDisabled);
    connect(ui->arpPTypeACB, &QCheckBox::toggled, ui->arpPTypeLineEdit, &QLineEdit::setDisabled);
    connect(ui->arpPSizeACB, &QCheckBox::toggled, ui->arpPSizeLineEdit, &QLineEdit::setDisabled);
    // IPv4
    connect(ui->ip4VersionACB, &QCheckBox::toggled, ui->ip4VersionLineEdit, &QLineEdit::setDisabled);
    connect(ui->ip4IHLACB, &QCheckBox::toggled, ui->ip4IHLLineEdit, &QLineEdit::setDisabled);
    connect(ui->ip4ChecksumACB, &QCheckBox::toggled, ui->ip4ChecksumLineEdit, &QLineEdit::setDisabled);
    connect(ui->ip4TotalLenthACB, &QCheckBox::toggled, ui->ip4TotalLenthLineEdit, &QLineEdit::setDisabled);
    // IPv6
    connect(ui->ip6VersionACB, &QCheckBox::toggled, ui->ip6VersionLineEdit, &QLineEdit::setDisabled);
    connect(ui->ip6PLLengthACB, &QCheckBox::toggled, ui->ip6PLLenthLineEdit, &QLineEdit::setDisabled);
    // TCP
    connect(ui->tcpChecksumACB, &QCheckBox::toggled, ui->tcpChecksumLineEdit, &QLineEdit::setDisabled);
    connect(ui->tcpDataOffACB, &QCheckBox::toggled, ui->tcpDataOffLineEdit, &QLineEdit::setDisabled);
    connect(ui->tcpCusFlCheck, &QCheckBox::toggled, this, &ConstructorPage::setCustomTCPFlags);

    // UDP
    connect(ui->udpChecksumACB, &QCheckBox::toggled, ui->udpChecksumLineEdit, &QLineEdit::setDisabled);
    connect(ui->udpTotalLenthACB, &QCheckBox::toggled, ui->udpTotalLenthLineEdit, &QLineEdit::setDisabled);

    // ICMP
    connect(ui->icmpChecksumACB, &QCheckBox::toggled, ui->icmpChecksumLineEdit, &QLineEdit::setDisabled);
    connect(ui->icmpPLACB, &QCheckBox::toggled, ui->icmpPLLineEdit, &QLineEdit::setDisabled);

    // Data page
    connect(ui->dataHexEdit,   &QPlainTextEdit::textChanged, this, &ConstructorPage::onHexChanged);
    connect(ui->dataASCIIEdit, &QPlainTextEdit::textChanged, this, &ConstructorPage::onAsciiChanged);

    // Buttons
    connect(ui->addBtn, &QPushButton::clicked, this, &ConstructorPage::onAddBtnClicked);
    connect(ui->delBtn, &QPushButton::clicked, this, &ConstructorPage::onDeleteBtnClicked);
    connect(ui->clearBtn, &QPushButton::clicked, this, &ConstructorPage::onClearBtnClicked);
    connect(ui->editBtn, &QPushButton::clicked, this, &ConstructorPage::onEditBtnClicked);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &ConstructorPage::onCancelBtnClicked);
    connect(ui->saveBtn, &QPushButton::clicked, this, &ConstructorPage::onSaveBtnClicked);
    connect(ui->saveFileBtn, &QPushButton::clicked, this, &ConstructorPage::onSaveFileBtnClicked);
    connect(ui->openFileBtn, &QPushButton::clicked, this, &ConstructorPage::onOpenFileBtnClicked);
}

void ConstructorPage::onNPBox_valueChanged(int index) {
    switch(index) {
        case 0:
            ui->npStackedWidget->setVisible(false);
            ui->tpComboBox->setCurrentIndex(0);
            ui->tpComboBox->setDisabled(true);
            break;

        case 1:
            resetARPPage();
            ui->npStackedWidget->setCurrentIndex(0);
            ui->npStackedWidget->setVisible(true);
            ui->tpComboBox->setCurrentIndex(0);
            ui->tpComboBox->setDisabled(true);
            break;

        case 2:
            resetIP4Page();
            ui->npStackedWidget->setCurrentIndex(1);
            ui->npStackedWidget->setVisible(true);
            ui->tpComboBox->setDisabled(false);
            break;
        case 3:
            resetIP6Page();
            ui->npStackedWidget->setCurrentIndex(2);
            ui->npStackedWidget->setVisible(true);
            ui->tpComboBox->setDisabled(false);
            break;

        case 4:
            ui->npCustomTextEdit->clear();
            ui->npStackedWidget->setCurrentIndex(3);
            ui->npStackedWidget->setVisible(true);
            ui->tpComboBox->setDisabled(false);
            break;

        default:
            break;
    }
}

void ConstructorPage::onTPBox_valueChanged(int index) {
    switch(index) {
    case 0:
        ui->tpStackedWidget->setVisible(false);
        break;

    case 1:
        resetTCPPage();
        ui->tpStackedWidget->setCurrentIndex(0);
        ui->tpStackedWidget->setVisible(true);
        break;

    case 2:
        resetUDPPage();
        ui->tpStackedWidget->setCurrentIndex(1);
        ui->tpStackedWidget->setVisible(true);
        break;

    case 3:
        resetICMPPage();
        ui->tpStackedWidget->setCurrentIndex(2);
        ui->tpStackedWidget->setVisible(true);
        break;

    case 4:
        ui->tpCustomTextEdit->clear();
        ui->tpStackedWidget->setCurrentIndex(3);
        ui->tpStackedWidget->setVisible(true);
        break;

    default:
        break;

    }
}

void ConstructorPage::clearLineEdits(QWidget *parent) {
    const auto lineEdits = parent->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : lineEdits)
        lineEdit->clear();
}

void ConstructorPage::resetARPPage() {
    clearLineEdits(ui->arpPage);
    ui->arpHwSizeACB->setChecked(true);
    ui->arpHwTypeACB->setChecked(true);
    ui->arpPSizeACB->setChecked(true);
    ui->arpPTypeACB->setChecked(true);
}

void ConstructorPage::resetIP4Page() {
    clearLineEdits(ui->ipv4Page);
    ui->ip4ChecksumACB->setChecked(true);
    ui->ip4VersionACB->setChecked(true);
    ui->ip4TotalLenthACB->setChecked(true);
}

void ConstructorPage::resetIP6Page() {
    clearLineEdits(ui->ipv6Page);
    ui->ip6VersionACB->setChecked(true);
    ui->ip6PLLengthACB->setChecked(true);
}

void ConstructorPage::resetTCPPage() {
    clearLineEdits(ui->tcpPage);
    ui->tcpChecksumACB->setChecked(true);
    ui->tcpDataOffACB->setChecked(true);
    ui->tcpCusFlCheck->setChecked(false);
    resetTCPFlags();
}

void ConstructorPage::resetUDPPage() {
    clearLineEdits(ui->udpPage);
    ui->udpChecksumACB->setChecked(true);
    ui->udpTotalLenthACB->setChecked(true);
}

void ConstructorPage::resetICMPPage() {
    clearLineEdits(ui->icmpPage);
    ui->icmpChecksumACB->setChecked(true);
}

void ConstructorPage::resetDATAPage() {
    ui->dataHexEdit->clear();
    ui->dataASCIIEdit->clear();
}

void ConstructorPage::resetTCPFlags() {
    ui->tcpFlACKCheck->setChecked(false);
    ui->tcpFlSYNCheck->setChecked(false);
    ui->tcpFlENCCheck->setChecked(false);
    ui->tcpFlENCECheck->setChecked(false);
    ui->tcpFlFINCheck->setChecked(false);
    ui->tcpFlPSHCheck->setChecked(false);
    ui->tcpFlRESCheck->setChecked(false);
    ui->tcpFlURGCheck->setChecked(false);
}

void ConstructorPage::setCustomTCPFlags(bool checked) {
    if (checked) {
        resetTCPFlags();

        ui->tcpFlACKCheck->setDisabled(true);
        ui->tcpFlSYNCheck->setDisabled(true);
        ui->tcpFlENCCheck->setDisabled(true);
        ui->tcpFlFINCheck->setDisabled(true);
        ui->tcpFlPSHCheck->setDisabled(true);
        ui->tcpFlRESCheck->setDisabled(true);
        ui->tcpFlURGCheck->setDisabled(true);
        ui->tcpFlENCECheck->setDisabled(true);

        ui->tcpCustFlLineEdit->setDisabled(false);
    } else {
        ui->tcpCustFlLineEdit->clear();
        ui->tcpCustFlLineEdit->setDisabled(true);

        ui->tcpFlACKCheck->setDisabled(false);
        ui->tcpFlSYNCheck->setDisabled(false);
        ui->tcpFlENCCheck->setDisabled(false);
        ui->tcpFlFINCheck->setDisabled(false);
        ui->tcpFlPSHCheck->setDisabled(false);
        ui->tcpFlRESCheck->setDisabled(false);
        ui->tcpFlURGCheck->setDisabled(false);
        ui->tcpFlENCECheck->setDisabled(false);
    }
}

void ConstructorPage::setToCurrentTime(bool state) {
    if (state) {
        ui->ethTimeEdit->setDisabled(true);
        ui->ethNSLineEdit->clear();
        ui->ethNSLineEdit->setDisabled(true);
    } else {
        ui->ethTimeEdit->setDisabled(false);
        ui->ethTimeEdit->setDateTime(QDateTime::currentDateTime());
        ui->ethNSLineEdit->setDisabled(false);
    }
}

void ConstructorPage::onHexChanged()
{
    if (m_syncing) return;
    m_syncing = true;

    bool ok = false;
    const QByteArray bytes = hexToBytes(ui->dataHexEdit->toPlainText(), &ok);

    if (ok) {
        // Restore cursor position in ASCII editor
        const int pos = ui->dataASCIIEdit->textCursor().position();
        ui->dataASCIIEdit->setPlainText(bytesToAscii(bytes));
        QTextCursor cur = ui->dataASCIIEdit->textCursor();
        cur.setPosition(qMin(pos, ui->dataASCIIEdit->document()->characterCount() - 1));
        ui->dataASCIIEdit->setTextCursor(cur);

        // Clear any error styling
        ui->dataHexEdit->setStyleSheet("");
    } else {
        // Visual feedback: invalid hex input
        ui->dataHexEdit->setStyleSheet("QPlainTextEdit { background-color: #3a1a1a; }");
    }

    m_syncing = false;
}

void ConstructorPage::onAsciiChanged()
{
    if (m_syncing) return;
    m_syncing = true;

    const QByteArray bytes = ui->dataASCIIEdit->toPlainText().toUtf8();

    const int pos = ui->dataHexEdit->textCursor().position();
    ui->dataHexEdit->setPlainText(bytesToHex(bytes));
    QTextCursor cur = ui->dataHexEdit->textCursor();
    cur.setPosition(qMin(pos, ui->dataHexEdit->document()->characterCount() - 1));
    ui->dataHexEdit->setTextCursor(cur);

    ui->dataHexEdit->setStyleSheet("");   // ASCII → Hex is always valid

    m_syncing = false;
}

QString ConstructorPage::bytesToHex(const QByteArray &data)
{
    QString out;
    out.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) out += ' ';
        out += QString("%1").arg((quint8)data[i], 2, 16, QChar('0')).toUpper();
    }
    return out;
}

QByteArray ConstructorPage::hexToBytes(const QString &hex, bool *ok)
{
    // Accept tokens separated by spaces/newlines, each must be exactly 2 hex digits
    QByteArray result;
    const QStringList tokens = hex.split(QRegularExpression(R"(\s+)"),
                                         Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        if (tok.size() != 2) {
            if (ok) *ok = false;
            return {};
        }
        bool byteOk = false;
        const quint8 byte = tok.toUInt(&byteOk, 16);
        if (!byteOk) {
            if (ok) *ok = false;
            return {};
        }
        result.append(static_cast<char>(byte));
    }
    if (ok) *ok = true;
    return result;
}

QString ConstructorPage::bytesToAscii(const QByteArray &data)
{
    QString out;
    out.reserve(data.size());
    for (const char c : data)
        // Replace non-printable chars with a dot (classic hex-editor style)
            out += (c >= 0x20 && c < 0x7F) ? QChar(c) : QChar('.');
    return out;
}

void ConstructorPage::onSaveFileBtnClicked() {
    if (m_packets.isEmpty()) {
        QMessageBox::warning(this, "Empty", "No packets to save.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, "Save PCAP", QString(), "PCAP Files (*.pcap)", nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) return;

    if (PacketBuilder::saveToFile(path, m_packets))
        QMessageBox::information(this, "Saved", "PCAP saved successfully.");
    else
        QMessageBox::critical(this, "Error", "Failed to write file.");
}

void ConstructorPage::onCancelBtnClicked() {
    if (m_editingIndex >= 0)
        populateFieldsFromPacket(m_packets[m_editingIndex]);
    exitEditMode();
}

void ConstructorPage::onOpenFileBtnClicked() {
    if (m_readerRunning) return;  // guard against double-click

    const QString path = QFileDialog::getOpenFileName(
        this, "Open PCAP", QString(), "PCAP Files (*.pcap)", nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) return;

    // Join previous thread if any
    if (m_readerThread.joinable())
        m_readerThread.join();

    ui->openFileBtn->setEnabled(false);
    ui->addBtn->setEnabled(false);

    m_progressDialog = new QProgressDialog("Reading PCAP...", QString(), 0, 100, this);
    m_progressDialog->setWindowTitle("Opening");
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->setValue(0);

    m_readerRunning = true;
    m_readerThread = std::thread(&ConstructorPage::pcapReaderThread, this, path);
}

void ConstructorPage::onSaveBtnClicked() {
    if (m_editingIndex < 0) return;
    m_packets[m_editingIndex] = parseFieldsToPacket();
    m_tableModel->packetUpdated(m_editingIndex);
    populatePacketTree(m_packets[m_editingIndex]);
    exitEditMode();
}

void ConstructorPage::onEditBtnClicked() {
    const QModelIndex idx = ui->tableView->currentIndex();
    if (!idx.isValid()) return;
    enterEditMode(idx.row());
}

void ConstructorPage::onDeleteBtnClicked() {
    QMessageBox::StandardButton reply;

    reply = QMessageBox::question(
                this,
                "Delete Item?",
                "This action cannot be undone.",
                QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        const QModelIndex idx = ui->tableView->currentIndex();
        if (!idx.isValid()) return;
        m_tableModel->packetRemoved(idx.row());
        ui->packetDetailTree->clear();
    }
}

void ConstructorPage::onClearBtnClicked() {
    clearLineEdits(ui->ethernetTab);

    ui->tcpCusFlCheck->setChecked(false);
    ui->ethTimeCurrentCB->setChecked(true);
    ui->npComboBox->setCurrentIndex(0);
    resetTCPFlags();
    ui->dataHexEdit->clear();
    ui->dataASCIIEdit->clear();
}

void ConstructorPage::onAddBtnClicked() {
    m_packets.append(parseFieldsToPacket());
    m_tableModel->packetAdded();
    ui->tableView->scrollToBottom();
}

// ── Progress slot ─────────────────────────────────────────────────────────────
void ConstructorPage::onPcapReadProgress(int current, int total)
{
    if (!m_progressDialog) return;
    m_progressDialog->setMaximum(total);
    m_progressDialog->setValue(current);
    m_progressDialog->setLabelText(
        QString("Reading packets... %1 / %2").arg(current).arg(total));
}

// ── Finished slot ─────────────────────────────────────────────────────────────
void ConstructorPage::onPcapReadFinished(QList<QPacket::Packet> *packets, bool ok, QString errorMsg)
{
    // Join the thread before touching any shared state
    if (m_readerThread.joinable())
        m_readerThread.join();

    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }

    ui->openFileBtn->setEnabled(true);
    ui->addBtn->setEnabled(true);

    if (!ok || !packets) {
        delete packets;
        QMessageBox::critical(this, "Error", errorMsg.isEmpty() ? "Unknown error." : errorMsg);
        return;
    }

    m_tableModel->beginResetModel();
    m_packets.clear();
    ui->packetDetailTree->clear();
    m_packets = std::move(*packets);
    delete packets;
    m_tableModel->endResetModel();

    ui->tableView->setColumnWidth(0, 180);
    ui->tableView->setColumnWidth(1, 150);
    ui->tableView->setColumnWidth(2, 150);
    ui->tableView->setColumnWidth(3,  80);
    ui->tableView->setColumnWidth(4,  60);

    QMessageBox::information(this, "Done",
        QString("Loaded %1 packet(s).").arg(m_packets.size()));
}

void ConstructorPage::pcapReaderThread(const QString path)
{
    auto *packets = new QList<QPacket::Packet>();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMetaObject::invokeMethod(this, [this]() {
            onPcapReadFinished(nullptr, false, "Cannot open file.");
        }, Qt::QueuedConnection);
        m_readerRunning = false;
        return;
    }

    // ── Global header ─────────────────────────────────────────────────────────
    quint32 magic = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);

    bool swapped = false;
    if (magic == 0xD4C3B2A1)      swapped = true;
    else if (magic != 0xA1B2C3D4) {
        QMetaObject::invokeMethod(this, [this]() {
            onPcapReadFinished(nullptr, false, "Not a valid PCAP file.");
        }, Qt::QueuedConnection);
        m_readerRunning = false;
        return;
    }

    auto read16 = [&](quint16 &v) {
        f.read(reinterpret_cast<char*>(&v), 2);
        if (swapped) v = qbswap(v);
    };
    auto read32 = [&](quint32 &v) {
        f.read(reinterpret_cast<char*>(&v), 4);
        if (swapped) v = qbswap(v);
    };
    auto read32s = [&](qint32 &v) {
        f.read(reinterpret_cast<char*>(&v), 4);
        if (swapped) v = qbswap(v);
    };

    quint16 verMajor, verMinor;
    qint32  thisZone;
    quint32 sigFigs, snapLen, network;
    read16(verMajor); read16(verMinor);
    read32s(thisZone);
    read32(sigFigs); read32(snapLen); read32(network);

    if (f.error() != QFileDevice::NoError) {
        QMetaObject::invokeMethod(this, [this]() {
            onPcapReadFinished(nullptr, false, "Failed to read PCAP global header.");
        }, Qt::QueuedConnection);
        m_readerRunning = false;
        return;
    }

    if (network != 1) {
        QMetaObject::invokeMethod(this, [this]() {
            onPcapReadFinished(nullptr, false,
                "Only Ethernet (linktype 1) is supported.");
        }, Qt::QueuedConnection);
        m_readerRunning = false;
        return;
    }

    // snapLen of 0 is technically valid but means "no limit" — cap it
    const quint32 maxFrameSize = (snapLen == 0 || snapLen > 65535) ? 65535 : snapLen;

    // ── Pre-scan for total count ──────────────────────────────────────────────
    const qint64 dataStart = f.pos();
    int total = 0;
    while (!f.atEnd() && f.error() == QFileDevice::NoError) {
        quint32 a, b, inclLen, c;
        read32(a); read32(b); read32(inclLen); read32(c);
        if (f.error() != QFileDevice::NoError) break;
        if (inclLen > maxFrameSize) break;          // sanity: corrupt/truncated
        const qint64 skipped = f.skip(inclLen);
        if (skipped != static_cast<qint64>(inclLen)) break;  // short skip = truncated
        ++total;
    }
    f.seek(dataStart);

    // ── Parse ─────────────────────────────────────────────────────────────────
    int current = 0;
    while (!f.atEnd() && f.error() == QFileDevice::NoError) {
        quint32 tsSec, tsUsec, inclLen, origLen;
        read32(tsSec); read32(tsUsec);
        read32(inclLen); read32(origLen);
        if (f.error() != QFileDevice::NoError) break;
        if (inclLen > maxFrameSize) break;          // sanity: corrupt/truncated

        const QByteArray frame = f.read(inclLen);
        if (static_cast<quint32>(frame.size()) != inclLen) break;

        packets->append(PcapReader::parseFrame(frame, tsSec, tsUsec));
        ++current;

        if (current % 100 == 0 || current == total) {
            const int cur = current;
            const int tot = total;
            QMetaObject::invokeMethod(this, [this, cur, tot]() {
                onPcapReadProgress(cur, tot);
            }, Qt::QueuedConnection);
        }
    }

    // ── Done — post result to main thread ─────────────────────────────────────
    QMetaObject::invokeMethod(this, [this, packets]() {
        onPcapReadFinished(packets, true, {});
    }, Qt::QueuedConnection);
    m_readerRunning = false;
}

void ConstructorPage::enterEditMode(int packetIndex) {
    m_editingIndex = packetIndex;

    ui->saveBtn->setVisible(true);
    ui->cancelBtn->setVisible(true);
    ui->addBtn->setEnabled(false);
    ui->editBtn->setEnabled(false);
    ui->delBtn->setEnabled(false);
    ui->clearBtn->setEnabled(false);
    ui->tableView->setEnabled(false);

    populateFieldsFromPacket(m_packets[packetIndex]);
}

void ConstructorPage::exitEditMode() {
    m_editingIndex = -1;

    ui->saveBtn->setVisible(false);
    ui->cancelBtn->setVisible(false);
    ui->addBtn->setEnabled(true);
    ui->editBtn->setEnabled(true);
    ui->delBtn->setEnabled(true);
    ui->clearBtn->setEnabled(true);
    ui->tableView->setEnabled(true);
}

QByteArray ConstructorPage::hexEditToBytes(const QString &hexStr) {
    QByteArray out;
    const QStringList tokens = hexStr.split(QRegularExpression(R"(\s+)"),
                                            Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        bool ok = false;
        out.append(static_cast<char>(tok.toUInt(&ok, 16)));
        if (!ok) return {}; // caller handles invalid hex
    }
    return out;
}

QPacket::Packet ConstructorPage::parseFieldsToPacket()
{
    QPacket::Packet pkt;

    // ── Ethernet ─────────────────────────────────────────────────────────────
    pkt.ethernet.srcMAC = ui->ethSrcLineEdit->text().trimmed();
    pkt.ethernet.dstMAC = ui->ethDstLineEdit->text().trimmed();
    pkt.ethernet.fcs    = ui->ethFCSLineEdit->text().trimmed();
    pkt.ethernet.srcMAC           = ui->ethSrcLineEdit->text().trimmed();
    pkt.ethernet.dstMAC           = ui->ethDstLineEdit->text().trimmed();
    pkt.ethernet.fcs              = ui->ethFCSLineEdit->text().trimmed();
    if (ui->ethTimeCurrentCB->isChecked()) {
        timespec ts{};
        clock_gettime(CLOCK_REALTIME, &ts);
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(ts.tv_sec);
        int nanoseconds = ts.tv_nsec / 1000;

        pkt.ethernet.timestamp = dateTime;
        pkt.ethernet.timestampUsec = nanoseconds;
    } else {
        pkt.ethernet.timestamp = ui->ethTimeEdit->dateTime();
        if (ui->ethNSLineEdit->text().isEmpty()) {
            pkt.ethernet.timestampUsec = 0;
        } else {
            pkt.ethernet.timestampUsec    = ui->ethNSLineEdit->text().toInt();
        }
    }

    // ── Network protocol ─────────────────────────────────────────────────────
    switch (ui->npComboBox->currentIndex()) {
    case 0: pkt.netProto = QPacket::NetworkProtocol::None;   break;
    case 1: pkt.netProto = QPacket::NetworkProtocol::ARP;    break;
    case 2: pkt.netProto = QPacket::NetworkProtocol::IPv4;   break;
    case 3: pkt.netProto = QPacket::NetworkProtocol::IPv6;   break;
    case 4: pkt.netProto = QPacket::NetworkProtocol::Custom; break;
    }

    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::ARP:
        pkt.arp.operation  = ui->arpOperationLineEdit->text().trimmed();
        pkt.arp.dstIP      = ui->arpDstIPLineEdit->text().trimmed();
        pkt.arp.dstMAC     = ui->arpDstMacLineEdit->text().trimmed();
        pkt.arp.srcIP      = ui->arpSrcIPLineEdit->text().trimmed();
        pkt.arp.srcMAC     = ui->arpSrcMACLineEdit->text().trimmed();
        pkt.arp.hwTypeAuto = ui->arpHwTypeACB->isChecked();
        pkt.arp.hwSizeAuto = ui->arpHwSizeACB->isChecked();
        pkt.arp.pTypeAuto  = ui->arpPTypeACB->isChecked();
        pkt.arp.pSizeAuto  = ui->arpPSizeACB->isChecked();
        if (!pkt.arp.hwTypeAuto) pkt.arp.hwType = ui->arpHwTypeLineEdit->text().trimmed();
        if (!pkt.arp.hwSizeAuto) pkt.arp.hwSize = ui->arpHwSizeLineEdit->text().trimmed();
        if (!pkt.arp.pTypeAuto)  pkt.arp.pType  = ui->arpPTypeLineEdit->text().trimmed();
        if (!pkt.arp.pSizeAuto)  pkt.arp.pSize  = ui->arpPSizeLineEdit->text().trimmed();
        break;

    case QPacket::NetworkProtocol::IPv4:
        pkt.ipv4.dstIP       = ui->ip4DstIPLineEdit->text().trimmed();
        pkt.ipv4.srcIP       = ui->ip4SrcIPLineEdit->text().trimmed();
        pkt.ipv4.ttl         = ui->ip4TTLLineEdit->text().trimmed();
        pkt.ipv4.flags       = ui->ip4FlagLineEdit->text().trimmed();
        pkt.ipv4.fragOffset  = ui->ip4FragOffLineEdit->text().trimmed();
        pkt.ipv4.dscpEcn     = ui->ip4DSCPEcnLineEdit->text().trimmed();
        pkt.ipv4.versionAuto  = ui->ip4VersionACB->isChecked();
        pkt.ipv4.ihlAuto      = ui->ip4IHLACB->isChecked();
        pkt.ipv4.checksumAuto = ui->ip4ChecksumACB->isChecked();
        pkt.ipv4.totalLenAuto = ui->ip4TotalLenthACB->isChecked();
        if (!pkt.ipv4.versionAuto)  pkt.ipv4.version     = ui->ip4VersionLineEdit->text().trimmed();
        if (!pkt.ipv4.ihlAuto)      pkt.ipv4.ihl         = ui->ip4IHLLineEdit->text().trimmed();
        if (!pkt.ipv4.checksumAuto) pkt.ipv4.checksum    = ui->ip4ChecksumLineEdit->text().trimmed();
        if (!pkt.ipv4.totalLenAuto) pkt.ipv4.totalLength = ui->ip4TotalLenthLineEdit->text().trimmed();
        break;

    case QPacket::NetworkProtocol::IPv6:
        pkt.ipv6.dstIP         = ui->ip6DstIPLineEdit->text().trimmed();
        pkt.ipv6.srcIP         = ui->ip6SrcIPLineEdit->text().trimmed();
        pkt.ipv6.hopLimit      = ui->ip6HopLimLineEdit->text().trimmed();
        pkt.ipv6.nextHeader    = ui->ip6NextHeadLineEdit->text().trimmed();
        pkt.ipv6.flowLabel     = ui->ip6FlowLLineEdit->text().trimmed();
        pkt.ipv6.trafficClass  = ui->ip6TCLineEdit->text().trimmed();
        pkt.ipv6.versionAuto    = ui->ip6VersionACB->isChecked();
        pkt.ipv6.payloadLenAuto = ui->ip6PLLengthACB->isChecked();
        if (!pkt.ipv6.versionAuto)    pkt.ipv6.version       = ui->ip6VersionLineEdit->text().trimmed();
        if (!pkt.ipv6.payloadLenAuto) pkt.ipv6.payloadLength = ui->ip6PLLenthLineEdit->text().trimmed();
        break;

    case QPacket::NetworkProtocol::Custom:
        pkt.customNetProto = ui->npCustomTextEdit->toPlainText().trimmed();
        break;

    default: break;
    }

    // ── Transport protocol ────────────────────────────────────────────────────
    switch (ui->tpComboBox->currentIndex()) {
    case 0: pkt.transProto = QPacket::TransportProtocol::None;   break;
    case 1: pkt.transProto = QPacket::TransportProtocol::TCP;    break;
    case 2: pkt.transProto = QPacket::TransportProtocol::UDP;    break;
    case 3: pkt.transProto = QPacket::TransportProtocol::ICMP;   break;
    case 4: pkt.transProto = QPacket::TransportProtocol::Custom; break;
    }

    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:
        pkt.tcp.dstPort       = ui->tcpDstPortLineEdit->text().trimmed();
        pkt.tcp.srcPort       = ui->tcpSrcPortLineEdit->text().trimmed();
        pkt.tcp.seqNum        = ui->tcpSeqNumLineEdit->text().trimmed();
        pkt.tcp.ackNum        = ui->tcpAckNumLineEdit->text().trimmed();
        pkt.tcp.windowSize    = ui->tcpWindSizeLineEdit->text().trimmed();
        pkt.tcp.urgentPointer = ui->tcpUrgPLineEdit->text().trimmed();
        pkt.tcp.optionsList   = ui->tcpOptListLineEdit->text().trimmed();
        pkt.tcp.flagACE       = ui->tcpFlENCCheck->isChecked();
        pkt.tcp.flagECE       = ui->tcpFlENCECheck->isChecked();
        pkt.tcp.flagURG       = ui->tcpFlURGCheck->isChecked();
        pkt.tcp.flagACK       = ui->tcpFlACKCheck->isChecked();
        pkt.tcp.flagPSH       = ui->tcpFlPSHCheck->isChecked();
        pkt.tcp.flagRST       = ui->tcpFlRESCheck->isChecked();
        pkt.tcp.flagSYN       = ui->tcpFlSYNCheck->isChecked();
        pkt.tcp.flagFIN       = ui->tcpFlFINCheck->isChecked();
        pkt.tcp.customFlags   = ui->tcpCusFlCheck->isChecked();
        if (pkt.tcp.customFlags)
            pkt.tcp.customFlagsValue = ui->tcpCustFlLineEdit->text().trimmed();
        pkt.tcp.dataOffsetAuto = ui->tcpDataOffACB->isChecked();
        pkt.tcp.checksumAuto   = ui->tcpChecksumACB->isChecked();
        if (!pkt.tcp.dataOffsetAuto) pkt.tcp.dataOffset = ui->tcpDataOffLineEdit->text().trimmed();
        if (!pkt.tcp.checksumAuto)   pkt.tcp.checksum   = ui->tcpChecksumLineEdit->text().trimmed();
        break;

    case QPacket::TransportProtocol::UDP:
        pkt.udp.dstPort         = ui->udpDstPortLineEdit->text().trimmed();
        pkt.udp.srcPort         = ui->udpSrcPortLineEdit->text().trimmed();
        pkt.udp.checksumAuto    = ui->udpChecksumACB->isChecked();
        pkt.udp.totalLengthAuto = ui->udpTotalLenthACB->isChecked();
        if (!pkt.udp.checksumAuto)    pkt.udp.checksum    = ui->udpChecksumLineEdit->text().trimmed();
        if (!pkt.udp.totalLengthAuto) pkt.udp.totalLength = ui->udpTotalLenthLineEdit->text().trimmed();
        break;

    case QPacket::TransportProtocol::ICMP:
        pkt.icmp.type        = ui->icmpTypeLineEdit->text().trimmed();
        pkt.icmp.code        = ui->icmpCodeLineEdit->text().trimmed();
        pkt.icmp.identifier  = ui->icmpIdnLineEdit->text().trimmed();
        pkt.icmp.sequence    = ui->icmpSeqLineEdit->text().trimmed();
        pkt.icmp.payloadAuto = ui->icmpPLACB->isChecked();
        pkt.icmp.checksumAuto= ui->icmpChecksumACB->isChecked();
        if (!pkt.icmp.payloadAuto)  pkt.icmp.payload  = ui->icmpPLLineEdit->text().trimmed();
        if (!pkt.icmp.checksumAuto) pkt.icmp.checksum = ui->icmpChecksumLineEdit->text().trimmed();
        break;

    case QPacket::TransportProtocol::Custom:
        pkt.customTransProto = ui->tpCustomTextEdit->toPlainText().trimmed();
        break;

    default: break;
    }

    // ── Payload (hex → bytes) ─────────────────────────────────────────────────
    pkt.payload = hexEditToBytes(ui->dataHexEdit->toPlainText());

    return pkt;
}

QTreeWidgetItem* ConstructorPage::makeSection(const QString &title)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, title);
    // Bold section headers, like Wireshark
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    return item;
}

QTreeWidgetItem* ConstructorPage::makeField(const QString &name, const QString &value)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setText(1, value.isEmpty() ? "(auto)" : value);
    return item;
}

void ConstructorPage::populatePacketTree(const QPacket::Packet &pkt)
{
    ui->packetDetailTree->clear();

    // ── Frame info ────────────────────────────────────────────────────────────
    auto *frameSection = makeSection("Frame");
    QString timeStr = pkt.ethernet.timestamp.toString("yyyy-MM-dd hh:mm:ss")
          + QString(".%1").arg(pkt.ethernet.timestampUsec, 6, 10, QChar('0'));
    frameSection->addChild(makeField("Arrival Time", timeStr));
    frameSection->addChild(makeField("Protocols in frame",
        resolveProtocolString(pkt)));  // reuse your existing helper
    ui->packetDetailTree->addTopLevelItem(frameSection);

    // ── Ethernet ──────────────────────────────────────────────────────────────
    auto *ethSection = makeSection(
        QString("Ethernet II, Src: %1, Dst: %2")
            .arg(pkt.ethernet.srcMAC, pkt.ethernet.dstMAC));
    ethSection->addChild(makeField("Destination", pkt.ethernet.dstMAC));
    ethSection->addChild(makeField("Source",      pkt.ethernet.srcMAC));
    if (!pkt.ethernet.fcs.isEmpty())
        ethSection->addChild(makeField("FCS", pkt.ethernet.fcs));
    ui->packetDetailTree->addTopLevelItem(ethSection);

    // ── Network layer ─────────────────────────────────────────────────────────
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::ARP: {
        auto *s = makeSection("Address Resolution Protocol");
        s->addChild(makeField("Operation",        pkt.arp.operation));
        s->addChild(makeField("Sender MAC",       pkt.arp.srcMAC));
        s->addChild(makeField("Sender IP",        pkt.arp.srcIP));
        s->addChild(makeField("Target MAC",       pkt.arp.dstMAC));
        s->addChild(makeField("Target IP",        pkt.arp.dstIP));
        s->addChild(makeField("Hardware Type",    pkt.arp.hwTypeAuto ? "(auto)" : pkt.arp.hwType));
        s->addChild(makeField("Hardware Size",    pkt.arp.hwSizeAuto ? "(auto)" : pkt.arp.hwSize));
        s->addChild(makeField("Protocol Type",    pkt.arp.pTypeAuto  ? "(auto)" : pkt.arp.pType));
        s->addChild(makeField("Protocol Size",    pkt.arp.pSizeAuto  ? "(auto)" : pkt.arp.pSize));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::NetworkProtocol::IPv4: {
        auto *s = makeSection(
            QString("Internet Protocol Version 4, Src: %1, Dst: %2")
                .arg(pkt.ipv4.srcIP, pkt.ipv4.dstIP));
        s->addChild(makeField("Version",         pkt.ipv4.versionAuto  ? "(auto)" : pkt.ipv4.version));
        s->addChild(makeField("Header Length",   pkt.ipv4.ihlAuto      ? "(auto)" : pkt.ipv4.ihl));
        s->addChild(makeField("DSCP/ECN",        pkt.ipv4.dscpEcn));
        s->addChild(makeField("Total Length",    pkt.ipv4.totalLenAuto ? "(auto)" : pkt.ipv4.totalLength));
        s->addChild(makeField("Flags",           pkt.ipv4.flags));
        s->addChild(makeField("Fragment Offset", pkt.ipv4.fragOffset));
        s->addChild(makeField("TTL",             pkt.ipv4.ttl));
        s->addChild(makeField("Checksum",        pkt.ipv4.checksumAuto ? "(auto)" : pkt.ipv4.checksum));
        s->addChild(makeField("Source",          pkt.ipv4.srcIP));
        s->addChild(makeField("Destination",     pkt.ipv4.dstIP));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::NetworkProtocol::IPv6: {
        auto *s = makeSection(
            QString("Internet Protocol Version 6, Src: %1, Dst: %2")
                .arg(pkt.ipv6.srcIP, pkt.ipv6.dstIP));
        s->addChild(makeField("Version",        pkt.ipv6.versionAuto    ? "(auto)" : pkt.ipv6.version));
        s->addChild(makeField("Traffic Class",  pkt.ipv6.trafficClass));
        s->addChild(makeField("Flow Label",     pkt.ipv6.flowLabel));
        s->addChild(makeField("Payload Length", pkt.ipv6.payloadLenAuto ? "(auto)" : pkt.ipv6.payloadLength));
        s->addChild(makeField("Next Header",    pkt.ipv6.nextHeader));
        s->addChild(makeField("Hop Limit",      pkt.ipv6.hopLimit));
        s->addChild(makeField("Source",         pkt.ipv6.srcIP));
        s->addChild(makeField("Destination",    pkt.ipv6.dstIP));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::NetworkProtocol::Custom: {
        auto *s = makeSection("Custom Network Protocol");
        s->addChild(makeField("Data", pkt.customNetProto));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    default: break;
    }

    // ── Transport layer ───────────────────────────────────────────────────────
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP: {
        auto *s = makeSection(
            QString("Transmission Control Protocol, Src Port: %1, Dst Port: %2")
                .arg(pkt.tcp.srcPort, pkt.tcp.dstPort));
        s->addChild(makeField("Source Port",      pkt.tcp.srcPort));
        s->addChild(makeField("Destination Port", pkt.tcp.dstPort));
        s->addChild(makeField("Sequence Number",  pkt.tcp.seqNum));
        s->addChild(makeField("Ack Number",       pkt.tcp.ackNum));
        s->addChild(makeField("Data Offset",      pkt.tcp.dataOffsetAuto ? "(auto)" : pkt.tcp.dataOffset));
        // Flags as sub-tree
        auto *flags = makeField("Flags", "");
        flags->addChild(makeField("ACE",   pkt.tcp.flagACE ? "Set" : "Not set"));
        flags->addChild(makeField("ECE",   pkt.tcp.flagECE ? "Set" : "Not set"));
        flags->addChild(makeField("URG",   pkt.tcp.flagURG ? "Set" : "Not set"));
        flags->addChild(makeField("ACK",   pkt.tcp.flagACK ? "Set" : "Not set"));
        flags->addChild(makeField("PSH",   pkt.tcp.flagPSH ? "Set" : "Not set"));
        flags->addChild(makeField("RST",   pkt.tcp.flagRST ? "Set" : "Not set"));
        flags->addChild(makeField("SYN",   pkt.tcp.flagSYN ? "Set" : "Not set"));
        flags->addChild(makeField("FIN",   pkt.tcp.flagFIN ? "Set" : "Not set"));
        if (pkt.tcp.customFlags)
            flags->addChild(makeField("Custom", pkt.tcp.customFlagsValue));
        s->addChild(flags);
        s->addChild(makeField("Window Size",     pkt.tcp.windowSize));
        s->addChild(makeField("Checksum",        pkt.tcp.checksumAuto ? "(auto)" : pkt.tcp.checksum));
        s->addChild(makeField("Urgent Pointer",  pkt.tcp.urgentPointer));
        s->addChild(makeField("Options",         pkt.tcp.optionsList));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::TransportProtocol::UDP: {
        auto *s = makeSection(
            QString("User Datagram Protocol, Src Port: %1, Dst Port: %2")
                .arg(pkt.udp.srcPort, pkt.udp.dstPort));
        s->addChild(makeField("Source Port",      pkt.udp.srcPort));
        s->addChild(makeField("Destination Port", pkt.udp.dstPort));
        s->addChild(makeField("Length",   pkt.udp.totalLengthAuto ? "(auto)" : pkt.udp.totalLength));
        s->addChild(makeField("Checksum", pkt.udp.checksumAuto    ? "(auto)" : pkt.udp.checksum));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::TransportProtocol::ICMP: {
        auto *s = makeSection("Internet Control Message Protocol");
        s->addChild(makeField("Type",       pkt.icmp.type));
        s->addChild(makeField("Code",       pkt.icmp.code));
        s->addChild(makeField("Checksum",   pkt.icmp.checksumAuto ? "(auto)" : pkt.icmp.checksum));
        s->addChild(makeField("Identifier", pkt.icmp.identifier));
        s->addChild(makeField("Sequence",   pkt.icmp.sequence));
        s->addChild(makeField("Payload",    pkt.icmp.payloadAuto  ? "(auto)" : pkt.icmp.payload));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    case QPacket::TransportProtocol::Custom: {
        auto *s = makeSection("Custom Transport Protocol");
        s->addChild(makeField("Data", pkt.customTransProto));
        ui->packetDetailTree->addTopLevelItem(s);
        break;
    }
    default: break;
    }

    // ── Payload ───────────────────────────────────────────────────────────────
    if (!pkt.payload.isEmpty()) {
        auto *s = makeSection(QString("Data (%1 bytes)").arg(pkt.payload.size()));
        // Show hex dump in groups of 8 bytes per row, like Wireshark
        for (int i = 0; i < pkt.payload.size(); i += 8) {
            QString hex, ascii;
            for (int j = i; j < qMin(i + 8, pkt.payload.size()); ++j) {
                const quint8 b = pkt.payload[j];
                hex   += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
            }
            s->addChild(makeField(hex.trimmed(), ascii));
        }
        ui->packetDetailTree->addTopLevelItem(s);
    }

    // Expand all top-level sections by default, like Wireshark
    ui->packetDetailTree->expandAll();
    ui->packetDetailTree->resizeColumnToContents(0);
}

void ConstructorPage::onPacketSelected(int row) {
    if (row >= 0 && row < m_packets.size())
        populatePacketTree(m_packets[row]);
}

QString ConstructorPage::resolveProtocolString(const QPacket::Packet &pkt)
{
    // Show the most specific (innermost) protocol, like Wireshark does
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:    return "TCP";
    case QPacket::TransportProtocol::UDP:    return "UDP";
    case QPacket::TransportProtocol::ICMP:   return "ICMP";
    case QPacket::TransportProtocol::Custom: return "Custom";
    default: break;
    }
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::ARP:    return "ARP";
    case QPacket::NetworkProtocol::IPv4:   return "IPv4";
    case QPacket::NetworkProtocol::IPv6:   return "IPv6";
    case QPacket::NetworkProtocol::Custom: return "Custom";
    default:                      return "Ethernet";
    }
}

QString ConstructorPage::resolveSource(const QPacket::Packet &pkt)
{
    // Prefer IP src if available, fall back to MAC
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::IPv4: return pkt.ipv4.srcIP;
    case QPacket::NetworkProtocol::IPv6: return pkt.ipv6.srcIP;
    case QPacket::NetworkProtocol::ARP:  return pkt.arp.srcIP.isEmpty()
                                       ? pkt.arp.srcMAC : pkt.arp.srcIP;
    default:                    return pkt.ethernet.srcMAC;
    }
}

QString ConstructorPage::resolveDestination(const QPacket::Packet &pkt)
{
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::IPv4: return pkt.ipv4.dstIP;
    case QPacket::NetworkProtocol::IPv6: return pkt.ipv6.dstIP;
    case QPacket::NetworkProtocol::ARP:  return pkt.arp.dstIP.isEmpty()
                                       ? pkt.arp.dstMAC : pkt.arp.dstIP;
    default:                    return pkt.ethernet.dstMAC;
    }
}

void ConstructorPage::populateFieldsFromPacket(const QPacket::Packet &pkt)
{
    // ── Ethernet ──────────────────────────────────────────────────────────────
    ui->ethSrcLineEdit->setText(pkt.ethernet.srcMAC);
    ui->ethDstLineEdit->setText(pkt.ethernet.dstMAC);
    ui->ethFCSLineEdit->setText(pkt.ethernet.fcs);
    ui->ethTimeCurrentCB->setChecked(false);
    ui->ethTimeEdit->setDateTime(pkt.ethernet.timestamp);
    std::cout << "NS: " << std::to_string(pkt.ethernet.timestampUsec) << std::endl;
    ui->ethNSLineEdit->setText(QString::number(pkt.ethernet.timestampUsec));

    // ── Network protocol ──────────────────────────────────────────────────────
    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::None:   ui->npComboBox->setCurrentIndex(0); break;
    case QPacket::NetworkProtocol::ARP:    ui->npComboBox->setCurrentIndex(1); break;
    case QPacket::NetworkProtocol::IPv4:   ui->npComboBox->setCurrentIndex(2); break;
    case QPacket::NetworkProtocol::IPv6:   ui->npComboBox->setCurrentIndex(3); break;
    case QPacket::NetworkProtocol::Custom: ui->npComboBox->setCurrentIndex(4); break;
    }

    switch (pkt.netProto) {
    case QPacket::NetworkProtocol::ARP:
        ui->arpOperationLineEdit->setText(pkt.arp.operation);
        ui->arpDstIPLineEdit->setText(pkt.arp.dstIP);
        ui->arpDstMacLineEdit->setText(pkt.arp.dstMAC);
        ui->arpSrcIPLineEdit->setText(pkt.arp.srcIP);
        ui->arpSrcMACLineEdit->setText(pkt.arp.srcMAC);
        ui->arpHwTypeACB->setChecked(pkt.arp.hwTypeAuto);
        ui->arpHwSizeACB->setChecked(pkt.arp.hwSizeAuto);
        ui->arpPTypeACB->setChecked(pkt.arp.pTypeAuto);
        ui->arpPSizeACB->setChecked(pkt.arp.pSizeAuto);
        if (!pkt.arp.hwTypeAuto) ui->arpHwTypeLineEdit->setText(pkt.arp.hwType);
        if (!pkt.arp.hwSizeAuto) ui->arpHwSizeLineEdit->setText(pkt.arp.hwSize);
        if (!pkt.arp.pTypeAuto)  ui->arpPTypeLineEdit->setText(pkt.arp.pType);
        if (!pkt.arp.pSizeAuto)  ui->arpPSizeLineEdit->setText(pkt.arp.pSize);
        break;

    case QPacket::NetworkProtocol::IPv4:
        ui->ip4DstIPLineEdit->setText(pkt.ipv4.dstIP);
        ui->ip4SrcIPLineEdit->setText(pkt.ipv4.srcIP);
        ui->ip4TTLLineEdit->setText(pkt.ipv4.ttl);
        ui->ip4FlagLineEdit->setText(pkt.ipv4.flags);
        ui->ip4FragOffLineEdit->setText(pkt.ipv4.fragOffset);
        ui->ip4DSCPEcnLineEdit->setText(pkt.ipv4.dscpEcn);
        ui->ip4VersionACB->setChecked(pkt.ipv4.versionAuto);
        ui->ip4IHLACB->setChecked(pkt.ipv4.ihlAuto);
        ui->ip4ChecksumACB->setChecked(pkt.ipv4.checksumAuto);
        ui->ip4TotalLenthACB->setChecked(pkt.ipv4.totalLenAuto);
        if (!pkt.ipv4.versionAuto)  ui->ip4VersionLineEdit->setText(pkt.ipv4.version);
        if (!pkt.ipv4.ihlAuto)      ui->ip4IHLLineEdit->setText(pkt.ipv4.ihl);
        if (!pkt.ipv4.checksumAuto) ui->ip4ChecksumLineEdit->setText(pkt.ipv4.checksum);
        if (!pkt.ipv4.totalLenAuto) ui->ip4TotalLenthLineEdit->setText(pkt.ipv4.totalLength);
        break;

    case QPacket::NetworkProtocol::IPv6:
        ui->ip6DstIPLineEdit->setText(pkt.ipv6.dstIP);
        ui->ip6SrcIPLineEdit->setText(pkt.ipv6.srcIP);
        ui->ip6HopLimLineEdit->setText(pkt.ipv6.hopLimit);
        ui->ip6NextHeadLineEdit->setText(pkt.ipv6.nextHeader);
        ui->ip6FlowLLineEdit->setText(pkt.ipv6.flowLabel);
        ui->ip6TCLineEdit->setText(pkt.ipv6.trafficClass);
        ui->ip6VersionACB->setChecked(pkt.ipv6.versionAuto);
        ui->ip6PLLengthACB->setChecked(pkt.ipv6.payloadLenAuto);
        if (!pkt.ipv6.versionAuto)    ui->ip6VersionLineEdit->setText(pkt.ipv6.version);
        if (!pkt.ipv6.payloadLenAuto) ui->ip6PLLenthLineEdit->setText(pkt.ipv6.payloadLength);
        break;

    case QPacket::NetworkProtocol::Custom:
        ui->npCustomTextEdit->setPlainText(pkt.customNetProto);
        break;

    default: break;
    }

    // ── Transport protocol ────────────────────────────────────────────────────
    switch (pkt.transProto) {
    case QPacket::TransportProtocol::None:   ui->tpComboBox->setCurrentIndex(0); break;
    case QPacket::TransportProtocol::TCP:    ui->tpComboBox->setCurrentIndex(1); break;
    case QPacket::TransportProtocol::UDP:    ui->tpComboBox->setCurrentIndex(2); break;
    case QPacket::TransportProtocol::ICMP:   ui->tpComboBox->setCurrentIndex(3); break;
    case QPacket::TransportProtocol::Custom: ui->tpComboBox->setCurrentIndex(4); break;
    }

    switch (pkt.transProto) {
    case QPacket::TransportProtocol::TCP:
        ui->tcpDstPortLineEdit->setText(pkt.tcp.dstPort);
        ui->tcpSrcPortLineEdit->setText(pkt.tcp.srcPort);
        ui->tcpSeqNumLineEdit->setText(pkt.tcp.seqNum);
        ui->tcpAckNumLineEdit->setText(pkt.tcp.ackNum);
        ui->tcpWindSizeLineEdit->setText(pkt.tcp.windowSize);
        ui->tcpUrgPLineEdit->setText(pkt.tcp.urgentPointer);
        ui->tcpOptListLineEdit->setText(pkt.tcp.optionsList);
        ui->tcpFlENCCheck->setChecked(pkt.tcp.flagACE);
        ui->tcpFlENCECheck->setChecked(pkt.tcp.flagECE);
        ui->tcpFlURGCheck->setChecked(pkt.tcp.flagURG);
        ui->tcpFlACKCheck->setChecked(pkt.tcp.flagACK);
        ui->tcpFlPSHCheck->setChecked(pkt.tcp.flagPSH);
        ui->tcpFlRESCheck->setChecked(pkt.tcp.flagRST);
        ui->tcpFlSYNCheck->setChecked(pkt.tcp.flagSYN);
        ui->tcpFlFINCheck->setChecked(pkt.tcp.flagFIN);
        ui->tcpCusFlCheck->setChecked(pkt.tcp.customFlags);
        ui->tcpCustFlLineEdit->setText(pkt.tcp.customFlagsValue);
        ui->tcpDataOffACB->setChecked(pkt.tcp.dataOffsetAuto);
        ui->tcpChecksumACB->setChecked(pkt.tcp.checksumAuto);
        if (!pkt.tcp.dataOffsetAuto) ui->tcpDataOffLineEdit->setText(pkt.tcp.dataOffset);
        if (!pkt.tcp.checksumAuto)   ui->tcpChecksumLineEdit->setText(pkt.tcp.checksum);
        break;

    case QPacket::TransportProtocol::UDP:
        ui->udpDstPortLineEdit->setText(pkt.udp.dstPort);
        ui->udpSrcPortLineEdit->setText(pkt.udp.srcPort);
        ui->udpChecksumACB->setChecked(pkt.udp.checksumAuto);
        ui->udpTotalLenthACB->setChecked(pkt.udp.totalLengthAuto);
        if (!pkt.udp.checksumAuto)    ui->udpChecksumLineEdit->setText(pkt.udp.checksum);
        if (!pkt.udp.totalLengthAuto) ui->udpTotalLenthLineEdit->setText(pkt.udp.totalLength);
        break;

    case QPacket::TransportProtocol::ICMP:
        ui->icmpTypeLineEdit->setText(pkt.icmp.type);
        ui->icmpCodeLineEdit->setText(pkt.icmp.code);
        ui->icmpIdnLineEdit->setText(pkt.icmp.identifier);
        ui->icmpSeqLineEdit->setText(pkt.icmp.sequence);
        ui->icmpPLACB->setChecked(pkt.icmp.payloadAuto);
        ui->icmpChecksumACB->setChecked(pkt.icmp.checksumAuto);
        if (!pkt.icmp.payloadAuto)  ui->icmpPLLineEdit->setText(pkt.icmp.payload);
        if (!pkt.icmp.checksumAuto) ui->icmpChecksumLineEdit->setText(pkt.icmp.checksum);
        break;

    case QPacket::TransportProtocol::Custom:
        ui->tpCustomTextEdit->setPlainText(pkt.customTransProto);
        break;

    default: break;
    }

    // ── Payload → hex editor ──────────────────────────────────────────────────
    QString hex;
    for (int i = 0; i < pkt.payload.size(); ++i) {
        if (i > 0) hex += ' ';
        hex += QString("%1").arg((quint8)pkt.payload[i], 2, 16, QChar('0')).toUpper();
    }
    ui->dataHexEdit->setPlainText(hex);
    // dataASCIIEdit updates automatically via your textChanged sync
}

void ConstructorPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}