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
    void           updateChart     (double bytesPerSec);
    void           rescaleYAxis    ();
    QString        autoScaleLabel  (double maxVal) const;

    Ui::StatisticWindow *ui;

    QChart      *m_chart       = nullptr;
    QLineSeries *m_bytesSeries = nullptr;
    QLineSeries *m_bitsSeries  = nullptr;
    QValueAxis  *m_axisX       = nullptr;
    QValueAxis  *m_axisY       = nullptr;

    static constexpr int kWindowSecs = 30;

    // Previous snapshot for delta computation
    ServerStats m_prevStats;
    bool        m_hasPrev    = false;
    double      m_timeCursor = 0.0;
};