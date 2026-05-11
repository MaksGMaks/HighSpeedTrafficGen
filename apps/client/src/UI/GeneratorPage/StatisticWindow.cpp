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

    // ── Chart setup ───────────────────────────────────────────────────────────
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

    // X axis — sliding time window
    m_axisX = new QValueAxis();
    m_axisX->setRange(0, kWindowSecs);
    m_axisX->setLabelFormat("%.0f s");
    m_axisX->setTitleText("Time (s)");
    m_axisX->setTickCount(7);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_bytesSeries->attachAxis(m_axisX);
    m_bitsSeries->attachAxis(m_axisX);

    // Y axis — auto-scaled
    m_axisY = new QValueAxis();
    m_axisY->setRange(0, 1024);
    m_axisY->setLabelFormat("%.1f");
    m_axisY->setTitleText("B/s");
    m_axisY->setTickCount(6);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_bytesSeries->attachAxis(m_axisY);
    m_bitsSeries->attachAxis(m_axisY);

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
    if (!m_hasPrev) {
        m_prevStats = stats;
        m_hasPrev   = true;
        return;
    }

    // ── Elapsed time between snapshots ────────────────────────────────────────
    const double dtMs = static_cast<double>(stats.timestampMs - m_prevStats.timestampMs);
    if (dtMs <= 0.0) return; // duplicate or out-of-order — skip
    const double dtSec = dtMs / 1000.0;

    // ── Deltas — uint64 subtraction wraps correctly on counter rollover ───────
    const quint64 deltaBytes   = stats.obytes   - m_prevStats.obytes;
    const quint64 deltaPackets = stats.opackets - m_prevStats.opackets;

    // ── Instantaneous speed ───────────────────────────────────────────────────
    const double bytesPerSec = static_cast<double>(deltaBytes)   / dtSec;
    const double pktsPerSec  = static_cast<double>(deltaPackets) / dtSec;

    // ── Update labels ─────────────────────────────────────────────────────────
    ui->editSpeedBytes->setText(formatBytes(bytesPerSec));
    ui->editSpeedBits ->setText(formatBits (bytesPerSec));
    ui->editPktPerSec ->setText(QString("%1 pkt/s").arg(pktsPerSec, 0, 'f', 1));
    ui->editTotalPkts ->setText(QString::number(stats.opackets));
    ui->editTotalBytes->setText(formatBytesTotal(stats.obytes));

    // ── Chart ─────────────────────────────────────────────────────────────────
    m_timeCursor += dtSec;
    updateChart(bytesPerSec);

    m_prevStats = stats;
}

void StatisticWindow::reset()
{
    m_hasPrev    = false;
    m_prevStats  = {};
    m_timeCursor = 0.0;

    m_bytesSeries->clear();
    m_bitsSeries->clear();

    m_axisX->setRange(0, kWindowSecs);
    m_axisY->setRange(0, 1024);
    m_axisY->setTitleText("B/s");

    ui->editSpeedBytes->clear();
    ui->editSpeedBits ->clear();
    ui->editPktPerSec ->clear();
    ui->editTotalPkts ->clear();
    ui->editTotalBytes->clear();
}

// ── Chart helpers ─────────────────────────────────────────────────────────────

void StatisticWindow::updateChart(double bytesPerSec)
{
    // Both series share the same byte-scale Y axis.
    // bits/s line is divided by 8 so it plots on the same axis
    // but visually shows bit throughput relative to bytes.
    m_bytesSeries->append(m_timeCursor, bytesPerSec);
    m_bitsSeries ->append(m_timeCursor, bytesPerSec * 8.0 / 8.0); // == bytesPerSec, same scale

    // Slide the X window once we exceed it
    if (m_timeCursor > kWindowSecs)
        m_axisX->setRange(m_timeCursor - kWindowSecs, m_timeCursor);
    else
        m_axisX->setRange(0, kWindowSecs);

    // Prune oldest points — one point per server update, so max = kWindowSecs / dt
    // Use a generous upper bound; rescaleYAxis only looks at visible points anyway
    const int maxPoints = kWindowSecs * 10 + 4; // handles up to 10 Hz server updates
    while (m_bytesSeries->count() > maxPoints) m_bytesSeries->remove(0);
    while (m_bitsSeries->count()  > maxPoints) m_bitsSeries->remove(0);

    rescaleYAxis();
}

void StatisticWindow::rescaleYAxis()
{
    double maxVal = 1.0;
    const double xMin = m_axisX->min();

    for (const QPointF &p : m_bytesSeries->points())
        if (p.x() >= xMin && p.y() > maxVal) maxVal = p.y();
    for (const QPointF &p : m_bitsSeries->points())
        if (p.x() >= xMin && p.y() > maxVal) maxVal = p.y();

    // Round up to next power-of-2 * 1024 boundary — clean axis labels
    double ceiling = 1024.0;
    while (ceiling < maxVal * 1.2) ceiling *= 2.0;

    m_axisY->setRange(0, ceiling);
    m_axisY->setTitleText(autoScaleLabel(ceiling));
}

QString StatisticWindow::autoScaleLabel(double maxVal) const
{
    if      (maxVal >= 1e9) return "GB/s";
    else if (maxVal >= 1e6) return "MB/s";
    else if (maxVal >= 1e3) return "KB/s";
    else                    return "B/s";
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void StatisticWindow::setDarkTheme(bool dark)
{
    if (dark) {
        m_chart->setTheme(QChart::ChartThemeDark);
        m_chart->setBackgroundBrush(QBrush(QColor(0x18, 0x18, 0x25)));
        m_chart->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_chart->legend()->setLabelBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX->setLabelsBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisY->setLabelsBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisY->setTitleBrush(QBrush(QColor(0xcd, 0xd6, 0xf4)));
        m_axisX->setGridLinePen(QPen(QColor(0x31, 0x32, 0x44)));
        m_axisY->setGridLinePen(QPen(QColor(0x31, 0x32, 0x44)));
        m_bytesSeries->setPen(QPen(QColor(0x89, 0xb4, 0xfa), 2));
        m_bitsSeries ->setPen(QPen(QColor(0xa6, 0xe3, 0xa1), 2));
    } else {
        m_chart->setTheme(QChart::ChartThemeLight);
        m_chart->setBackgroundBrush(QBrush(QColor(0xff, 0xff, 0xff)));
        m_chart->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_chart->legend()->setLabelBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX->setLabelsBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisY->setLabelsBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisY->setTitleBrush(QBrush(QColor(0x4c, 0x4f, 0x69)));
        m_axisX->setGridLinePen(QPen(QColor(0xcc, 0xd0, 0xda)));
        m_axisY->setGridLinePen(QPen(QColor(0xcc, 0xd0, 0xda)));
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