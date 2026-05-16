#include "GeneratorPlayer.hpp"

#include "../../../../../../libs/common/include/generator_values.hpp"

GeneratorPlayer::GeneratorPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GeneratorPlayer)
{
    ui->setupUi(this);

    ui->mainLayout->setContentsMargins(12, 12, 12, 12);

    connect(ui->browseBtn,       &QPushButton::clicked,
            this, &GeneratorPlayer::onBrowseClicked);
    connect(ui->clearFileBtn,    &QPushButton::clicked,
            this, &GeneratorPlayer::onClearFileClicked);
    connect(ui->speedModeCombo,  &QComboBox::currentIndexChanged,
            this, &GeneratorPlayer::onSpeedModeChanged);
    connect(ui->loopCheck,       &QCheckBox::toggled,
            this, &GeneratorPlayer::onLoopCheckToggled);
}

GeneratorPlayer::~GeneratorPlayer() { delete ui; }

// ── Settings read ─────────────────────────────────────────────────────────────

PcapParams::PlayerSettings GeneratorPlayer::settings() const
{
    PcapParams::PlayerSettings s;
    s.filePath   = ui->filePathEdit->text().toStdString();
    s.startPacket= ui->startOffsetSpin->value();
    s.endPacket  = ui->endOffsetSpin->value();
    s.loop       = ui->loopCheck->isChecked();
    s.loopCount  = ui->loopCountSpin->value();
    switch (ui->speedModeCombo->currentIndex()) {
    case 0:
        s.speedMode = PcapParams::SpeedMode::Original;
        break;

    case 1:
        s.speedMode = PcapParams::SpeedMode::Multiplier;
        break;

    case 2:
        s.speedMode = PcapParams::SpeedMode::Fixed;
        break;

    case 3:
        s.speedMode = PcapParams::SpeedMode::Max;
        break;

    default:
        break;
    }
    s.speedMult  = ui->speedMultSpin->value();
    s.fixedRate  = ui->fixedRateSpin->value();
    return s;
}

// ── File info update (called after pcap header is read) ───────────────────────

void GeneratorPlayer::setFileInfo(int totalPackets, qint64 totalBytes,
                                   double durationSecs, const QString &linkType)
{
    ui->infoTotalPackets->setText(QString::number(totalPackets));
    ui->infoTotalSize->setText(formatBytes(totalBytes));
    ui->infoDuration->setText(formatDuration(durationSecs));
    ui->infoLinkType->setText(linkType);

    ui->startOffsetSpin->setMaximum(totalPackets);
    ui->endOffsetSpin->setMaximum(totalPackets);
}

// ── Lock/unlock ───────────────────────────────────────────────────────────────

void GeneratorPlayer::setEnabled(bool enabled)
{
    ui->fileGroup->setEnabled(enabled);
    ui->playbackGroup->setEnabled(enabled);
}

// ── Browse / clear ────────────────────────────────────────────────────────────

void GeneratorPlayer::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Open PCAP", QString(), "PCAP Files (*.pcap *.pcapng)",
        nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) return;

    ui->filePathEdit->setText(path);

    // Show file size immediately; full info set via setFileInfo() after parsing
    const QFileInfo fi(path);
    ui->infoTotalSize->setText(formatBytes(fi.size()));
    ui->infoTotalPackets->clear();
    ui->infoDuration->clear();
    ui->infoLinkType->setText("Ethernet");

    emit settingsChanged(settings());
}

void GeneratorPlayer::onClearFileClicked()
{
    ui->filePathEdit->clear();
    ui->infoTotalPackets->clear();
    ui->infoTotalSize->clear();
    ui->infoDuration->clear();
    ui->infoLinkType->clear();
    ui->startOffsetSpin->setValue(1);
    ui->endOffsetSpin->setValue(0);

    emit settingsChanged(settings());
}

// ── Speed mode ────────────────────────────────────────────────────────────────

void GeneratorPlayer::onSpeedModeChanged(int idx)
{
    ui->speedMultSpin->setVisible(idx == 1);
    ui->fixedRateSpin->setVisible(idx == 2);
    emit settingsChanged(settings());
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void GeneratorPlayer::onLoopCheckToggled(bool checked)
{
    ui->loopCountSpin->setEnabled(checked);
    if (!checked) ui->loopCountSpin->setValue(1);
    emit settingsChanged(settings());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString GeneratorPlayer::formatBytes(qint64 bytes)
{
    if      (bytes >= (qint64)1e9) return QString("%1 GB").arg(bytes / 1e9, 0, 'f', 2);
    else if (bytes >= (qint64)1e6) return QString("%1 MB").arg(bytes / 1e6, 0, 'f', 2);
    else if (bytes >= (qint64)1e3) return QString("%1 KB").arg(bytes / 1e3, 0, 'f', 2);
    else                           return QString("%1 B" ).arg(bytes);
}

QString GeneratorPlayer::formatDuration(double seconds)
{
    const auto s = static_cast<qint64>(seconds);
    return QString("%1:%2:%3")
        .arg(s / 3600,       2, 10, QChar('0'))
        .arg((s % 3600) / 60, 2, 10, QChar('0'))
        .arg(s % 60,          2, 10, QChar('0'));
}

void GeneratorPlayer::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}