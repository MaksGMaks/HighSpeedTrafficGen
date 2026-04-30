#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <QObject>

class UIUpdater : public QObject {
    Q_OBJECT
public:
    explicit UIUpdater(QObject *parent = nullptr);
    ~UIUpdater() = default;

signals:
    void updateGraph(const uint64_t &pps, const uint64_t &bps, const uint64_t &BPS, const int64_t &time);
    void updateDynamicVariables(const uint64_t &totalSend, const uint64_t &totalCopies);

public slots:
    void onSendProgress(const uint64_t &totalSend, const uint64_t &totalCopies, const struct timespec &startTime);
    void onSendHalfProgress(const uint64_t &totalCopies, const struct timespec &startTime, const std::string &interfaceName);
    void onStartGenerator();

private:
    uint64_t m_prevTotalSend = 0, m_prevTotalCopies = 0, m_totalLost = 0, m_totalTime = 0, m_prevTime = 0; 
    uint64_t m_filePackets = 0, m_fileBytes = 0, m_fileDrops = 0;
    void readDynamicVariables(const std::string &interfaceName);
};