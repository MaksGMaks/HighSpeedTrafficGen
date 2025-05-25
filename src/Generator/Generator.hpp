#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <QObject>

#include "common_generator.hpp"
#include "DPDK_Engine.hpp"
#include "PFRING_Engine.hpp"

class Generator : public QObject {
    Q_OBJECT
public:
    Generator(QObject *parent = nullptr);
    ~Generator();

public slots:
    // Control method
    void doPause();                                     // pause thread's work but don't stop it
    void doResume();                                    // resume thread's work after pause
    void doStart(const genParams& params);              // start thread. should be called first
    void doStop();                                      // stop thread by joining it

signals:
    void finished();
    void sendProgress(const uint64_t &totalSend, const uint64_t &totalCopies, const struct timespec &startTime
                    , const uint64_t &packetSize, const uint64_t &burstSize); 

private:
    void pfringSend();
    void pfringZCSend();
    void dpdkSend(); 

    void pfringSendFile();
    void pfringZCSendFile();
    void dpdkSendFile(); 

    genParams m_params;
    // Thread variables
    // Thread
    std::thread m_workerThread;

    // Mutex
    std::mutex m_mutex;

    // Condition variable
    std::condition_variable m_pause;

    // Atomic
    std::atomic<bool> isRunning;
    std::atomic<bool> isPaused;
};


