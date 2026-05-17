#include "GeneratorParams.hpp"

GeneratorParams::GeneratorParams(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GeneratorParams)
{
    ui->setupUi(this);
    setupRows();
    setupSectionLabels();

    connect(ui->protoModeCombo, &QComboBox::currentIndexChanged,
            this, &GeneratorParams::onProtoModeChanged);
    // connect(ui->applyBtn, &QPushButton::clicked, this, &GeneratorParams::onApply);
    // connect(ui->resetBtn, &QPushButton::clicked, this, &GeneratorParams::onReset);

    // ui->btnLayout->setContentsMargins(10, 10, 10, 10);
    ui->protoRandomLayout->setContentsMargins(0, 0, 0, 0);

    onProtoModeChanged(0);
    resetToDefaults();
}

GeneratorParams::~GeneratorParams() { delete ui; }

void GeneratorParams::setupRows()
{
    ui->srcIPRow->setLabel("Source IP");
    ui->srcIPRow->setType(ParamRow::Type::IP);

    ui->dstIPRow->setLabel("Destination IP");
    ui->dstIPRow->setType(ParamRow::Type::IP);

    ui->srcPortRow->setLabel("Source Port");
    ui->srcPortRow->setType(ParamRow::Type::Port);

    ui->dstPortRow->setLabel("Destination Port");
    ui->dstPortRow->setType(ParamRow::Type::Port);

    ui->ttlRow->setLabel("TTL");
    ui->ttlRow->setType(ParamRow::Type::TTL);

    ui->sizeRow->setLabel("Packet Size (B)");
    ui->sizeRow->setType(ParamRow::Type::Size);

    ui->timeDiffRow->setLabel("Time Between (s)");
    ui->timeDiffRow->setType(ParamRow::Type::TimeDiff);
}

void GeneratorParams::setupSectionLabels()
{
    auto bold = [](QLabel *l) {
        QFont f = l->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        l->setFont(f);
    };
    bold(ui->netSectionLabel);
    bold(ui->protoSectionLabel);
    bold(ui->pktSectionLabel);
    bold(ui->genSectionLabel);
}

void GeneratorParams::onProtoModeChanged(int idx)
{
    ui->protoStaticCombo ->setVisible(idx == 0);
    ui->protoRandomWidget->setVisible(idx == 1);
}

// ── Read law from UI ──────────────────────────────────────────────────────────

GenLaw::Law GeneratorParams::law() const
{
    GenLaw::Law l;

    // IP params use std::string
    auto readIP = [](ParamRow *row, GenLaw::Param<std::string> &p) {
        p.mode  = row->mode();
        p.value = row->staticValue().toStdString();
        p.min   = row->rangeMin().toStdString();
        p.max   = row->rangeMax().toStdString();
        p.dist  = row->distribution();
    };

    // Integer params — templated on the actual type
    auto readInt = [](ParamRow *row, auto &p) {
        using T = decltype(p.value);
        p.mode  = row->mode();
        p.value = static_cast<T>(row->staticValue().toUInt());
        p.min   = static_cast<T>(row->rangeMin().toUInt());
        p.max   = static_cast<T>(row->rangeMax().toUInt());
        p.step  = static_cast<T>(row->rangeStep().toUInt());
        p.dist  = row->distribution();
    };

    auto readDouble = [](ParamRow *row, GenLaw::Param<double> &p) {
        p.mode  = row->mode();
        p.value = row->staticValue().toDouble();
        p.min   = row->rangeMin().toDouble();
        p.max   = row->rangeMax().toDouble();
        p.step  = row->rangeStep().toDouble();
        p.dist  = row->distribution();
    };

    readIP    (ui->srcIPRow,    l.srcIP);
    readIP    (ui->dstIPRow,    l.dstIP);
    readInt   (ui->srcPortRow,  l.srcPort);
    readInt   (ui->dstPortRow,  l.dstPort);
    readInt   (ui->ttlRow,      l.ttl);
    readInt   (ui->sizeRow,     l.packetSize);
    readDouble(ui->timeDiffRow, l.timeDiff);

    // Protocol mode
    l.protocol.mode = (ui->protoModeCombo->currentIndex() == 0)
                          ? GenLaw::Mode::Static
                          : GenLaw::Mode::Random;

    // Static: single protocol selection
    switch (ui->protoStaticCombo->currentIndex()) {
    case 0: l.protocol.protocol = GenLaw::Protocol::TCP;  break;
    case 1: l.protocol.protocol = GenLaw::Protocol::UDP;  break;
    case 2: l.protocol.protocol = GenLaw::Protocol::ICMP; break;
    case 3: l.protocol.protocol = GenLaw::Protocol::ARP;  break;
    default: break;
    }

    // Random: checkbox pool
    l.protocol.tcp  = ui->protoTCPCheck->isChecked();
    l.protocol.udp  = ui->protoUDPCheck->isChecked();
    l.protocol.icmp = ui->protoICMPCheck->isChecked();
    l.protocol.arp  = ui->protoARPCheck->isChecked();

    l.packetCount = static_cast<uint64_t>(ui->packetCountSpin->value());

    return l;
}

// ── Restore law to UI ─────────────────────────────────────────────────────────

void GeneratorParams::setLaw(const GenLaw::Law &l)
{
    auto restoreIP = [](ParamRow *row, const GenLaw::Param<std::string> &p) {
        row->setMode(p.mode);
        row->setStaticValue(QString::fromStdString(p.value));
        row->setRange(QString::fromStdString(p.min),
                      QString::fromStdString(p.max));
        row->setDistribution(p.dist);
    };

    auto restoreInt = [](ParamRow *row, const auto &p) {
        row->setMode(p.mode);
        row->setStaticValue(QString::number(p.value));
        row->setRange(QString::number(p.min),
                      QString::number(p.max),
                      QString::number(p.step));
        row->setDistribution(p.dist);
    };

    auto restoreDouble = [](ParamRow *row, const GenLaw::Param<double> &p) {
        row->setMode(p.mode);
        row->setStaticValue(QString::number(p.value));
        row->setRange(QString::number(p.min),
                      QString::number(p.max),
                      QString::number(p.step));
        row->setDistribution(p.dist);
    };

    restoreIP    (ui->srcIPRow,    l.srcIP);
    restoreIP    (ui->dstIPRow,    l.dstIP);
    restoreInt   (ui->srcPortRow,  l.srcPort);
    restoreInt   (ui->dstPortRow,  l.dstPort);
    restoreInt   (ui->ttlRow,      l.ttl);
    restoreInt   (ui->sizeRow,     l.packetSize);
    restoreDouble(ui->timeDiffRow, l.timeDiff);

    // Protocol mode combo
    ui->protoModeCombo->setCurrentIndex(
        l.protocol.mode == GenLaw::Mode::Static ? 0 : 1);

    // Static combo — map Protocol enum to index
    switch (l.protocol.protocol) {
    case GenLaw::Protocol::TCP:  ui->protoStaticCombo->setCurrentIndex(0); break;
    case GenLaw::Protocol::UDP:  ui->protoStaticCombo->setCurrentIndex(1); break;
    case GenLaw::Protocol::ICMP: ui->protoStaticCombo->setCurrentIndex(2); break;
    case GenLaw::Protocol::ARP:  ui->protoStaticCombo->setCurrentIndex(3); break;
    default: break;
    }

    // Random checkboxes
    ui->protoTCPCheck ->setChecked(l.protocol.tcp);
    ui->protoUDPCheck ->setChecked(l.protocol.udp);
    ui->protoICMPCheck->setChecked(l.protocol.icmp);
    ui->protoARPCheck ->setChecked(l.protocol.arp);

    ui->packetCountSpin->setValue(static_cast<int>(l.packetCount));
}

void GeneratorParams::resetToDefaults()
{
    setLaw(GenLaw::makeDefaultLaw());
}

// void GeneratorParams::onApply() { emit lawApplied(law()); }
// void GeneratorParams::onReset() { resetToDefaults(); }

void GeneratorParams::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}