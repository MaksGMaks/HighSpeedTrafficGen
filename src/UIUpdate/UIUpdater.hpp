#pragma once

#include <QObject>

class UIUpdater : public QObject {
    Q_OBJECT
public:
    explicit UIUpdater(QObject *parent = nullptr);
    ~UIUpdater() = default;

signals:
    void updateGraph(const uint64_t &pps, const uint64_t &bps, const uint64_t &BPS, const uint64_t &packetLoss, const uint64_t &totalTime);
    void updateDynamicVariables(const uint64_t &totalSend, const uint64_t &totalCopies);

public slots:
    void onSendProgress(const uint64_t &totalSend, const uint64_t &totalCopies, const struct timespec &startTime
                        , const uint64_t &packetSize, const uint64_t &burstSize);
private:
    uint64_t m_prevTotalSend = 0, m_prevTotalCopies = 0, m_totalLost = 0, m_totalTime = 0, m_prevTime = 0; 
};