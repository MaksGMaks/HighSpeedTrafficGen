#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "common_generator.hpp"
#include "DPDK_Engine.hpp"

#include "generator_values.hpp"

class Generator {
public:
    Generator();
    ~Generator();

    generator::statisticQueue* getQueueP();

     // Control method
     void doPause();                                     // pause thread's work but don't stop it
     void doResume();                                    // resume thread's work after pause
     void doStart(const genParams& params);              // start thread. should be called first
     void doStop();                                      // stop thread by joining it
//
// signals:
//     void finished();
//     void sendProgress(const uint64_t &totalSend, const uint64_t &totalCopies, const struct timespec &startTime);
//     void sendHalfProgress(const uint64_t &totalCopies, const struct timespec &startTime, const std::string &interfaceName);
//     void sendWarning(const std::string &message); // send warning message to UI
//     void sendError(const std::string &message);   // send error message to UI

private:
    bool presetDPDK();
    void dpdkSend();
    void dpdkSendFile();

    void getDPDKStat();

    generator::statisticQueue* statQueue = nullptr;

    genParams m_params;
    // Thread variables
    // Thread
    std::thread m_workerThread;
    std::thread m_statThread;

    generator::statusQueue mainQueue;

    // Mutex
    std::mutex m_mutex;

    // Condition variable
    std::condition_variable m_pause;

    // Atomic
    std::atomic<bool> isRunning;
    std::atomic<bool> isPaused;
};


