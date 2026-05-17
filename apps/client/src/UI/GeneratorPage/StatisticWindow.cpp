#include "StatisticWindow.hpp"

// ── Formatting helpers ────────────────────────────────────────────────────────

QString StatisticWindow::formatBytes(double Bps)
{
    if      (Bps >= 1e9) return QString("%1 GB/s").arg(Bps / 1e9, 0, 'f', 3);
    else if (Bps >= 1e6) return QString("%1 MB/s").arg(Bps / 1e6, 0, 'f', 3);
    else if (Bps >= 1e3) return QString("%1 KB/s").arg(Bps / 1e3, 0, 'f', 3);
    else                 return QString("%1 B/s" ).arg(Bps,        0, 'f', 1);
}

QString StatisticWindow::formatBits(double Bps)
{
    const double bps = Bps * 8.0;
    if      (bps >= 1e9) return QString("%1 Gbit/s").arg(bps / 1e9, 0, 'f', 3);
    else if (bps >= 1e6) return QString("%1 Mbit/s").arg(bps / 1e6, 0, 'f', 3);
    else if (bps >= 1e3) return QString("%1 Kbit/s").arg(bps / 1e3, 0, 'f', 3);
    else                 return QString("%1 bit/s" ).arg(bps,        0, 'f', 1);
}

QString StatisticWindow::formatBytesTotal(quint64 bytes)
{
    if      (bytes >= (quint64)1e9) return QString("%1 GB").arg(bytes / 1e9, 0, 'f', 2);
    else if (bytes >= (quint64)1e6) return QString("%1 MB").arg(bytes / 1e6, 0, 'f', 2);
    else if (bytes >= (quint64)1e3) return QString("%1 KB").arg(bytes / 1e3, 0, 'f', 2);
    else                            return QString("%1 B" ).arg(bytes);
}

// ── Constructor ───────────────────────────────────────────────────────────────

StatisticWindow::StatisticWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatisticWindow)
{
    ui->setupUi(this);

    m_chart = new QChart();
    m_chart->setTitle("Throughput");
    m_chart->legend()->setVisible(true);
    m_chart->setAnimationOptions(QChart::NoAnimation);

    m_bytesSeries = new QLineSeries();
    m_bytesSeries->setName("B/s");
    m_bytesSeries->setPen(QPen(QColor(0x1e, 0x66, 0xf5), 2));

    m_bitsSeries = new QLineSeries();
    m_bitsSeries->setName("bit/s");
    m_bitsSeries->setPen(QPen(QColor(0x40, 0xa0, 0x2b), 2));

    m_chart->addSeries(m_bytesSeries);
    m_chart->addSeries(m_bitsSeries);

    // X axis
    m_axisX = new QValueAxis();
    m_axisX->setRange(0, kWindowSecs);
    m_axisX->setLabelFormat("%.0f s");
    m_axisX->setTitleText("Time (s)");
    m_axisX->setTickCount(7);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_bytesSeries->attachAxis(m_axisX);
    m_bitsSeries ->attachAxis(m_axisX);

    // Left Y axis — B/s
    m_axisYBytes = new QValueAxis();
    m_axisYBytes->setRange(0, 1.0);
    m_axisYBytes->setLabelFormat("%.2f");
    m_axisYBytes->setTitleText("B/s");
    m_axisYBytes->setTickCount(6);
    m_chart->addAxis(m_axisYBytes, Qt::AlignLeft);
    m_bytesSeries->attachAxis(m_axisYBytes);

    // Right Y axis — bit/s
    m_axisYBits = new QValueAxis();
    m_axisYBits->setRange(0, 8.0);
    m_axisYBits->setLabelFormat("%.2f");
    m_axisYBits->setTitleText("bit/s");
    m_axisYBits->setTickCount(6);
    m_chart->addAxis(m_axisYBits, Qt::AlignRight);
    m_bitsSeries->attachAxis(m_axisYBits);

    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
    ui->verticalLayout->setContentsMargins(12, 12, 12, 12);

    connect(ui->resetBtn, &QPushButton::clicked, this, &StatisticWindow::reset);

    setDarkTheme(false);
}

StatisticWindow::~StatisticWindow()
{
    delete ui;
}

// ── Public slots ──────────────────────────────────────────────────────────────

void StatisticWindow::onStatsReceived(const ServerStats &stats)
{
    // dt from wall clock — independent of the stats struct
    double dtSec = 1.0;

    if (!m_elapsedStarted) {
        m_elapsed.start();
        m_elapsedStarted = true;
    } else {
        const qint64 elapsedMs = m_elapsed.restart();
        if (elapsedMs > 0)
            dtSec = elapsedMs / 1000.0;
    }

    if (!m_hasPrev) {
        m_prevStats = stats;
        m_hasPrev   = true;
        return;
    }

    const quint64 deltaBytes   = stats.obytes   - m_prevStats.obytes;
    const quint64 deltaPackets = stats.opackets - m_prevStats.opackets;

    const double bytesPerSec = static_cast<double>(deltaBytes)   / dtSec;
    const double bitsPerSec  = bytesPerSec * 8.0;
    const double pktsPerSec  = static_cast<double>(deltaPackets) / dtSec;

    ui->editSpeedBytes->setText(formatBytes(bytesPerSec));
    ui->editSpeedBits ->setText(formatBits (bytesPerSec));
    ui->editPktPerSec ->setText(QString("%1 pkt/s").arg(pktsPerSec, 0, 'f', 1));
    ui->editTotalPkts ->setText(QString::number(stats.opackets));
    ui->editTotalBytes->setText(formatBytesTotal(stats.obytes));

    m_timeCursor += dtSec;
    updateChart(bytesPerSec, bitsPerSec);

    m_prevStats = stats;
}

void StatisticWindow::reset()
{
    m_hasPrev        = false;
    m_prevStats      = {};
    m_timeCursor     = 0.0;
    m_elapsedStarted = false;
    m_byteDivisor    = 1.0;
    m_bitDivisor     = 1.0;

    m_bytesSeries->clear();
    m_bitsSeries ->clear();

    m_axisX->setRange(0, kWindowSecs);
    m_axisYBytes->setRange(0, 1.0);
    m_axisYBytes->setTitleText("B/s");
    m_axisYBits ->setRange(0, 8.0);
    m_axisYBits ->setTitleText("bit/s");

    ui->editSpeedBytes->clear();
    ui->editSpeedBits ->clear();
    ui->editPktPerSec ->clear();
    ui->editTotalPkts ->clear();
    ui->editTotalBytes->clear();
}

// ── Chart helpers ─────────────────────────────────────────────────────────────

static void resolveByteUnit(double rawBps, double &div, QString &label)
{
    if      (rawBps >= 1e9) { div = 1e9; label = "GB/s";  }
    else if (rawBps >= 1e6) { div = 1e6; label = "MB/s";  }
    else if (rawBps >= 1e3) { div = 1e3; label = "KB/s";  }
    else                    { div = 1.0; label = "B/s";   }
}

static void resolveBitUnit(double rawBps, double &div, QString &label)
{
    if      (rawBps >= 1e9) { div = 1e9; label = "Gbit/s"; }
    else if (rawBps >= 1e6) { div = 1e6; label = "Mbit/s"; }
    else if (rawBps >= 1e3) { div = 1e3; label = "Kbit/s"; }
    else                    { div = 1.0; label = "bit/s";  }
}

// Same roundUp as the reference: val * 1.5 rounded up to next clean power-of-10 step
static double roundUp(double val)
{
    if (val <= 0.0) return 1.0;
    const double raw  = val * 1.5;
    const double step = std::pow(10.0, std::floor(std::log10(raw)));
    return std::ceil(raw / step) * step;
}

void StatisticWindow::updateChart(double bytesPerSec, double bitsPerSec)
{
    // ── Resolve unit tiers for the incoming raw values ────────────────────────
    double newByteDiv, newBitDiv;
    QString byteLabel, bitLabel;
    resolveByteUnit(bytesPerSec, newByteDiv, byteLabel);
    resolveBitUnit (bitsPerSec,  newBitDiv,  bitLabel);

    // ── On tier change: rescale all stored (already-scaled) points ────────────
    // Mirrors the reference: stored_value × (oldDiv / newDiv)
    if (newByteDiv != m_byteDivisor) {
        const double ratio = m_byteDivisor / newByteDiv;
        for (int i = 0; i < m_bytesSeries->count(); ++i) {
            QPointF pt = m_bytesSeries->at(i);
            pt.setY(pt.y() * ratio);
            m_bytesSeries->replace(i, pt);
        }
        m_byteDivisor = newByteDiv;
        m_axisYBytes->setTitleText(byteLabel);
    }

    if (newBitDiv != m_bitDivisor) {
        const double ratio = m_bitDivisor / newBitDiv;
        for (int i = 0; i < m_bitsSeries->count(); ++i) {
            QPointF pt = m_bitsSeries->at(i);
            pt.setY(pt.y() * ratio);
            m_bitsSeries->replace(i, pt);
        }
        m_bitDivisor = newBitDiv;
        m_axisYBits->setTitleText(bitLabel);
    }

    // ── Append new point divided by the current tier ──────────────────────────
    const double scaledBytes = bytesPerSec / m_byteDivisor;
    const double scaledBits  = bitsPerSec  / m_bitDivisor;
    m_bytesSeries->append(m_timeCursor, scaledBytes);
    m_bitsSeries ->append(m_timeCursor, scaledBits);

    // ── Slide X window ────────────────────────────────────────────────────────
    if (m_timeCursor > kWindowSecs)
        m_axisX->setRange(m_timeCursor - kWindowSecs, m_timeCursor);
    else
        m_axisX->setRange(0, kWindowSecs);

    const int maxPoints = kWindowSecs + 4;
    while (m_bytesSeries->count() > maxPoints) m_bytesSeries->remove(0);
    while (m_bitsSeries ->count() > maxPoints) m_bitsSeries ->remove(0);

    // ── Y axis range based on the latest scaled value ─────────────────────────
    m_axisYBytes->setRange(0, roundUp(scaledBytes));
    m_axisYBits ->setRange(0, roundUp(scaledBits));
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void StatisticWindow::setDarkTheme(bool dark)
{
    if (dark) {
        m_chart->setTheme(QChart::ChartThemeDark);
        m_chart->setBackgroundBrush(QBrush(QColor(0x18, 0x18, 0x25)));
        m_chart->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_chart->legend()->setLabelBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX     ->setLabelsBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisYBytes->setLabelsBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisYBits ->setLabelsBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX     ->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisYBytes->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisYBits ->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX     ->setGridLinePen(QPen(QColor(0x31, 0x32, 0x44)));
        m_axisYBytes->setGridLinePen(QPen(QColor(0x31, 0x32, 0x44)));
        m_axisYBits ->setGridLinePen(QPen(QColor(0x31, 0x32, 0x44)));
        m_bytesSeries->setPen(QPen(QColor(0x89, 0xb4, 0xfa), 2));
        m_bitsSeries ->setPen(QPen(QColor(0xa6, 0xe3, 0xa1), 2));
    } else {
        m_chart->setTheme(QChart::ChartThemeLight);
        m_chart->setBackgroundBrush(QBrush(QColor(0xff, 0xff, 0xff)));
        m_chart->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_chart->legend()->setLabelBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX     ->setLabelsBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisYBytes->setLabelsBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisYBits ->setLabelsBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX     ->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisYBytes->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisYBits ->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX     ->setGridLinePen(QPen(QColor(0xcc, 0xd0, 0xda)));
        m_axisYBytes->setGridLinePen(QPen(QColor(0xcc, 0xd0, 0xda)));
        m_axisYBits ->setGridLinePen(QPen(QColor(0xcc, 0xd0, 0xda)));
        m_bytesSeries->setPen(QPen(QColor(0x1e, 0x66, 0xf5), 2));
        m_bitsSeries ->setPen(QPen(QColor(0x40, 0xa0, 0x2b), 2));
    }
}

void StatisticWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}