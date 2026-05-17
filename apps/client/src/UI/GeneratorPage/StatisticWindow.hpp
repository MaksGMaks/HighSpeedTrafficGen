#pragma once
#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QElapsedTimer>

#include <cmath>

#include "generator_values.hpp"
#include "ui_StatisticWindow.h"

namespace Ui { class StatisticWindow; }

class StatisticWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StatisticWindow(QWidget *parent = nullptr);
    ~StatisticWindow();

    void setDarkTheme(bool dark);
    void changeEvent(QEvent *event) override;

public slots:
    void onStatsReceived(const ServerStats &stats);
    void reset();

private:
    static QString formatBytes     (double Bps);
    static QString formatBits      (double Bps);
    static QString formatBytesTotal(quint64 bytes);
    void           updateChart     (double bytesPerSec, double bitsPerSec);

    Ui::StatisticWindow *ui;

    QChart      *m_chart        = nullptr;
    QLineSeries *m_bytesSeries  = nullptr;
    QLineSeries *m_bitsSeries   = nullptr;
    QValueAxis  *m_axisX        = nullptr;
    QValueAxis  *m_axisYBytes   = nullptr;   // left  — B/s
    QValueAxis  *m_axisYBits    = nullptr;   // right — bit/s

    static constexpr int kWindowSecs = 30;

    ServerStats    m_prevStats;
    bool           m_hasPrev    = false;
    double         m_timeCursor = 0.0;

    // Wall-clock timer — used to compute dt independently of the stats struct
    QElapsedTimer  m_elapsed;
    bool           m_elapsedStarted = false;

    // Current divisor applied to series data (1.0 / 1e3 / 1e6 / 1e9).
    // Tracked so rescaleYAxes can rescale existing points when the unit tier changes.
    double         m_byteDivisor = 1.0;
    double         m_bitDivisor  = 1.0;
};