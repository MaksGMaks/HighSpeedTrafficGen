#include "UIUpdater.hpp"

UIUpdater::UIUpdater(QObject *parent)
: QObject(parent) {
    // Initialization code can go here if needed
}

void UIUpdater::onSendProgress(const uint64_t &totalSend, const uint64_t &totalCopies
, const struct timespec &startTime) {
    int64_t elapsedTime;
    if(startTime.tv_sec == 0) {
        elapsedTime = 0;
    } else {
        elapsedTime = (startTime.tv_sec - m_prevTime);
    }
    if(elapsedTime >= 1) {
        uint64_t speed = ((totalSend - m_prevTotalSend) / elapsedTime);
        m_prevTime = startTime.tv_sec;
        m_prevTotalSend = totalSend;
        uint64_t pps = (totalCopies - m_prevTotalCopies) / elapsedTime;
        emit updateGraph(pps, (speed * 8), speed, elapsedTime);
        m_prevTotalCopies = totalCopies;
        m_totalLost = 0;
        emit updateDynamicVariables(totalSend, totalCopies);
    }
    
}

void UIUpdater::onSendHalfProgress(const uint64_t &totalCopies, const struct timespec &startTime
, const std::string &interfaceName) {
    int64_t elapsedTime;
    if(startTime.tv_sec == 0) {
        elapsedTime = 0;
    } else {
        elapsedTime = (startTime.tv_sec - m_prevTime);
    }
    if(elapsedTime >= 1) {
        readDynamicVariables(interfaceName);
        uint64_t speed = ((m_fileBytes - m_prevTotalSend) / elapsedTime);
        m_prevTime = startTime.tv_sec;
        m_prevTotalSend = m_fileBytes;
        uint64_t pps = (totalCopies - m_prevTotalCopies) / elapsedTime;
        emit updateGraph(pps, (speed * 8), speed, elapsedTime);
        m_prevTotalCopies = totalCopies;
        m_totalLost = 0;
        emit updateDynamicVariables(m_fileBytes, totalCopies);
    }
}

void UIUpdater::readDynamicVariables(const std::string &interfaceName) {
    std::ifstream file("/proc/net/dev");
    std::string line;

    // Skip headers
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line)) {
        // Trim leading whitespace
        line.erase(0, line.find_first_not_of(" \t"));

        // Check for matching interface
        if (line.rfind(interfaceName + ":", 0) == 0) {
            std::size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) return;

            std::string data = line.substr(colonPos + 1);
            std::istringstream iss(data);

            unsigned long val;
            for (int i = 0; i <= 8; ++i) {
                if (!(iss >> val)) return; // if we hit invalid data or EOF early
            }

            m_fileBytes = val; // 9th value (index 8) is TX bytes
            return;
        }
    }
}

void UIUpdater::onStartGenerator() {
    m_prevTime = 0;
    m_prevTotalSend = 0;
    m_prevTotalCopies = 0;
    m_totalLost = 0;
    m_filePackets = 0;
    m_fileBytes = 0;
    m_fileDrops = 0;
}