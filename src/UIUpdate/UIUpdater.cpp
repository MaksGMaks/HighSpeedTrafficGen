#include "UIUpdater.hpp"

UIUpdater::UIUpdater(QObject *parent)
: QObject(parent) {
    // Initialization code can go here if needed
}

void UIUpdater::onSendProgress(const uint64_t &totalSend, const uint64_t &totalCopies
, const struct timespec &startTime, const uint64_t &packetSize, const uint64_t &burstSize) {
    int64_t elapsedTime = (startTime.tv_sec - m_prevTime);
    m_totalTime += elapsedTime;
    m_totalLost += (totalCopies - m_prevTotalCopies);
    if(elapsedTime >= 1) {
        uint64_t speed = ((totalSend - m_prevTotalSend) / elapsedTime);
        m_prevTime = startTime.tv_sec;
        m_prevTotalSend = totalSend;
        uint64_t pps = (totalCopies - m_prevTotalCopies) / elapsedTime;
        emit updateGraph(pps, (speed * 8), speed, m_totalLost, m_totalTime);
        m_prevTotalCopies = totalCopies;
        m_totalLost = 0;
        emit updateDynamicVariables(totalSend, totalCopies);
    }
    
}