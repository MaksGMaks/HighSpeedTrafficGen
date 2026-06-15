#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_icmp.h>

#include "common_generator.hpp"

class Generator {
public:
    explicit Generator(uint16_t port_id);
    ~Generator();

    generator::statisticQueue* getQueueP();
    generator::messageQueue* getMSGQueueP();

    void doStart(const genParams& params);
    void doStartFromBuffer(const genParams& params,
                           const std::vector<uint8_t>& pcapData);
    void doStop();
    void doPause();
    void doResume();

private:
    void workerRandomLaw();
    void workerPcapPlayer();
    void workerStats();           // ← new: publishes one snapshot/second

    bool setupPort(uint32_t maxPktSize, rte_mempool*& pool_out);
    void teardownPort(rte_mempool* pool);

    void pushError(const std::string& msg);
    void pushWarning(const std::string& msg);

    // Called by worker threads instead of pushing directly to statQueue.
    // Thread-safe; just updates the two atomic accumulators.
    void recordStats(uint64_t packets, uint64_t bytes);

    rte_ring* m_txRing = nullptr;

    std::thread m_builderThread;

    void workerBuilder(rte_mempool* pool, const GenLaw::Law& law, int core);
    void workerTX(rte_mempool* pool);

    generator::statisticQueue* statQueue = nullptr;
    generator::messageQueue* msgQueue = nullptr;
    genParams   m_params{};
    std::vector<uint8_t>   m_pcapData;
    uint16_t    m_portId;

    std::thread             m_workerThread;
    std::thread             m_statsThread;   // ← new

    std::mutex              m_mutex;
    std::condition_variable m_pauseCV;
    std::atomic<bool>       isRunning{false};
    std::atomic<bool>       isPaused{false};

    // Rolling totals written by worker threads, read by the stats thread.
    std::atomic<uint64_t>   m_totalPackets{0};
    std::atomic<uint64_t>   m_totalBytes{0};
};