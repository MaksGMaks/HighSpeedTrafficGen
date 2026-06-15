#include "Generator.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <pthread.h>

// ── DPDK TX constants (all generation-related, not EAL) ──────────────────────
static constexpr uint16_t TX_QUEUE_ID  = 0;
static constexpr uint16_t TX_DESC      = 2048;
static constexpr uint16_t RX_DESC_MIN  = 64;
static constexpr uint32_t MBUF_COUNT   = 16384;
static constexpr uint32_t MBUF_CACHE   = 512;
static constexpr uint16_t BURST_SIZE   = 64;
static constexpr uint32_t RING_SIZE    = 4096;

static constexpr int LCORE_TX      = 1;
static constexpr int LCORE_BUILDER_0 = 2;
static constexpr int LCORE_BUILDER_1 = 3;
static constexpr int LCORE_STATS     = 4;

// ── PCAP structures ───────────────────────────────────────────────────────────
struct PcapGlobalHdr {
    uint32_t magic_number;
    uint16_t version_major, version_minor;
    int32_t  thiszone;
    uint32_t sigfigs, snaplen, network;
};
struct PcapRecHdr {
    uint32_t ts_sec, ts_usec, incl_len, orig_len;
};

// ─────────────────────────────────────────────────────────────────────────────

Generator::Generator(uint16_t port_id)
    : m_portId(port_id)
{
    statQueue = new generator::statisticQueue();
    msgQueue = new generator::messageQueue();
}

Generator::~Generator()
{
    doStop();
    delete statQueue;
    delete msgQueue;
}

generator::statisticQueue* Generator::getQueueP() { return statQueue; }
generator::messageQueue* Generator::getMSGQueueP() { return msgQueue; }

// ── Stats helper ──────────────────────────────────────────────────────────────

void Generator::recordStats(uint64_t packets, uint64_t bytes)
{
    // Workers call this instead of touching statQueue directly.
    // Just update the atomic totals; workerStats() will publish once/second.
    m_totalPackets.store(packets, std::memory_order_relaxed);
    m_totalBytes  .store(bytes,   std::memory_order_relaxed);
}

// ── Stats thread ──────────────────────────────────────────────────────────────

void Generator::workerStats()
{
    // пін stats thread на окреме ядро щоб не заважав TX
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LCORE_STATS, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    std::unique_lock<std::mutex> lk(m_mutex);
    while (isRunning.load(std::memory_order_relaxed)) {
        m_pauseCV.wait_for(lk, std::chrono::seconds(1));

        const uint64_t pkts  = m_totalPackets.load(std::memory_order_relaxed);
        const uint64_t bytes = m_totalBytes  .load(std::memory_order_relaxed);

        if (statQueue) {
            generator::statisticData sd(pkts, bytes, 0, pkts);
            statQueue->queue.push(sd);
        }
    }
}

// ── Control ───────────────────────────────────────────────────────────────────

void Generator::doStart(const genParams& params)
{
    doStop();
    m_params   = params;
    m_pcapData.clear();
    m_totalPackets.store(0);
    m_totalBytes  .store(0);
    isRunning  = true;
    isPaused   = false;

    if (params.mode == GeneratorMode::PcapPlayer)
        m_workerThread = std::thread(&Generator::workerPcapPlayer, this);
    else
        m_workerThread = std::thread(&Generator::workerRandomLaw, this);

    m_statsThread = std::thread(&Generator::workerStats, this);
}

void Generator::doStartFromBuffer(const genParams& params,
                                  const std::vector<uint8_t>& pcapData)
{
    doStop();
    m_params   = params;
    m_pcapData = pcapData;
    m_totalPackets.store(0);
    m_totalBytes  .store(0);
    isRunning  = true;
    isPaused   = false;

    m_workerThread = std::thread(&Generator::workerPcapPlayer, this);
    m_statsThread  = std::thread(&Generator::workerStats, this);
}

void Generator::doStop()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        isRunning = false;
        isPaused  = false;
    }
    m_pauseCV.notify_all();   // wakes both the pause-wait and the stats sleep

    if (m_workerThread.joinable())
        m_workerThread.join();
    if (m_statsThread.joinable())
        m_statsThread.join();
}

void Generator::doPause()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    isPaused = true;
}

void Generator::doResume()
{
    { std::lock_guard<std::mutex> lk(m_mutex); isPaused = false; }
    m_pauseCV.notify_all();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Generator::pushError(const std::string& msg)
{
    std::cerr << "[Generator] ERROR: " << msg << "\n";
    if (msgQueue)
        msgQueue->queue.push({ MessageSeverity::Error, msg });
}

void Generator::pushWarning(const std::string& msg)
{
    std::cerr << "[Generator] WARN: " << msg << "\n";
    if (msgQueue)
        msgQueue->queue.push({ MessageSeverity::Warning, msg });
}

static bool waitIfPaused(std::atomic<bool>& running,
                         std::atomic<bool>& paused,
                         std::mutex& mtx,
                         std::condition_variable& cv)
{
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&]{ return !paused.load() || !running.load(); });
    return running.load();
}

// ── Port setup / teardown (deferred — called from worker threads) ─────────────

bool Generator::setupPort(uint32_t maxPktSize, rte_mempool*& pool_out)
{
    pool_out = nullptr;

    const uint32_t data_room = std::max(maxPktSize + RTE_PKTMBUF_HEADROOM,
                                        (uint32_t)RTE_MBUF_DEFAULT_BUF_SIZE);
    const std::string pool_name = "TX_POOL_" + std::to_string(m_portId);

    pool_out = rte_pktmbuf_pool_create(pool_name.c_str(),
                                       MBUF_COUNT, MBUF_CACHE,
                                       0, data_room,
                                       rte_socket_id());
    if (!pool_out) {
        pushError("rte_pktmbuf_pool_create failed: " +
                  std::string(rte_strerror(rte_errno)));
        return false;
    }

    rte_eth_dev_info dev_info{};
    rte_eth_dev_info_get(m_portId, &dev_info);

    rte_eth_conf port_conf{};
    port_conf.rxmode.mtu = RTE_ETHER_MAX_LEN;

    if (rte_eth_dev_configure(m_portId, 1, 1, &port_conf) < 0) {
        pushError("rte_eth_dev_configure failed on port " +
                  std::to_string(m_portId));
        rte_mempool_free(pool_out); pool_out = nullptr;
        return false;
    }

    uint16_t nb_rxd = dev_info.rx_desc_lim.nb_min > 0
                      ? dev_info.rx_desc_lim.nb_min : RX_DESC_MIN;
    uint16_t nb_txd = TX_DESC;
    rte_eth_dev_adjust_nb_rx_tx_desc(m_portId, &nb_rxd, &nb_txd);

    rte_eth_rxconf rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    if (rte_eth_rx_queue_setup(m_portId, 0, nb_rxd,
                               rte_eth_dev_socket_id(m_portId),
                               &rx_conf, pool_out) < 0) {
        pushError("rte_eth_rx_queue_setup failed");
        rte_mempool_free(pool_out); pool_out = nullptr;
        return false;
    }

    rte_eth_txconf tx_conf = dev_info.default_txconf;
    tx_conf.offloads = port_conf.txmode.offloads;

    // TX threshold tuning для малих пакетів
    tx_conf.tx_thresh.pthresh = 36;
    tx_conf.tx_thresh.hthresh = 0;
    tx_conf.tx_thresh.wthresh = 0;

    tx_conf.tx_free_thresh = 32;   // звільняти дескриптори частіше
    tx_conf.tx_rs_thresh   = 32;   // RS-bit кожні 32 пакети

    if (rte_eth_tx_queue_setup(m_portId, TX_QUEUE_ID, nb_txd,
                               rte_eth_dev_socket_id(m_portId),
                               &tx_conf) < 0) {
        pushError("rte_eth_tx_queue_setup failed");
        rte_mempool_free(pool_out); pool_out = nullptr;
        return false;
    }

    if (rte_eth_dev_start(m_portId) < 0) {
        pushError("rte_eth_dev_start failed on port " +
                  std::to_string(m_portId));
        rte_mempool_free(pool_out); pool_out = nullptr;
        return false;
    }

    rte_eth_promiscuous_enable(m_portId);
    return true;
}

void Generator::teardownPort(rte_mempool* pool)
{
    rte_eth_dev_stop(m_portId);
    if (pool)
        rte_mempool_free(pool);
}

static uint16_t ipChecksum(const void* data, size_t len)
{
    const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    for (; len > 1; len -= 2) sum += *p++;
    if (len) sum += *reinterpret_cast<const uint8_t*>(p);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}


void Generator::workerBuilder(rte_mempool* pool, const GenLaw::Law& law, int core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    std::mt19937 rng(std::random_device{}());

    const uint32_t minFrame = sizeof(rte_ether_hdr) +
                              sizeof(rte_ipv4_hdr)  +
                              sizeof(rte_tcp_hdr);
    const size_t   hdrSize  = minFrame;  // тільки стільки нулюємо

    rte_mbuf* burst[BURST_SIZE];

    rte_ether_addr srcMac{};
    rte_eth_macaddr_get(m_portId, &srcMac);

    uint16_t    cachedSrcPort = 0, cachedDstPort = 0;
    uint16_t    cachedSrcPortN = 0, cachedDstPortN = 0;

    while (isRunning.load(std::memory_order_relaxed)) {
        // mutex тільки якщо реально на паузі
        if (isPaused.load(std::memory_order_relaxed)) {
            if (!waitIfPaused(isRunning, isPaused, m_mutex, m_pauseCV)) break;
        }

        const int allocd = rte_pktmbuf_alloc_bulk(pool, burst, BURST_SIZE);
        if (allocd != 0) { rte_pause(); continue; }

        uint16_t valid = 0;
        for (uint16_t bi = 0; bi < BURST_SIZE; ++bi) {
            const auto p = law.resolve(rng);
            const uint32_t pktSize = std::max(p.packetSize, minFrame);

            rte_mbuf* m = burst[bi];
            rte_pktmbuf_reset(m);

            uint8_t* raw = reinterpret_cast<uint8_t*>(
                               rte_pktmbuf_append(m, pktSize));
            if (!raw) { rte_pktmbuf_free(m); continue; }

            // тільки заголовки, не весь пакет
            memset(raw, 0, pktSize);

            // Ethernet — srcMac з кешу, без syscall
            auto* eth = reinterpret_cast<rte_ether_hdr*>(raw);
            memset(&eth->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
            eth->src_addr   = srcMac;
            eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

            const size_t ipOff = sizeof(rte_ether_hdr);
            auto* ip = reinterpret_cast<rte_ipv4_hdr*>(raw + ipOff);
            ip->version_ihl     = 0x45;
            ip->type_of_service = 0;
            ip->total_length    = htons(static_cast<uint16_t>(pktSize - ipOff));
            ip->packet_id       = 0;
            ip->fragment_offset = 0;
            ip->time_to_live    = p.ttl;
            ip->src_addr = p.srcIP;
            ip->dst_addr = p.dstIP;

            const size_t l4Off = ipOff + sizeof(rte_ipv4_hdr);

            switch (p.protocol) {
            case GenLaw::Protocol::UDP: {
                ip->next_proto_id = IPPROTO_UDP;
                auto* udp = reinterpret_cast<rte_udp_hdr*>(raw + l4Off);
                // htons тільки якщо порт змінився
                if (p.srcPort != cachedSrcPort) {
                    cachedSrcPort  = p.srcPort;
                    cachedSrcPortN = htons(p.srcPort);
                }
                if (p.dstPort != cachedDstPort) {
                    cachedDstPort  = p.dstPort;
                    cachedDstPortN = htons(p.dstPort);
                }
                udp->src_port    = cachedSrcPortN;
                udp->dst_port    = cachedDstPortN;
                udp->dgram_len   = htons(static_cast<uint16_t>(pktSize - l4Off));
                udp->dgram_cksum = 0;
                break;
            }
            case GenLaw::Protocol::ICMP: {
                ip->next_proto_id = IPPROTO_ICMP;
                auto* icmp = reinterpret_cast<rte_icmp_hdr*>(raw + l4Off);
                icmp->icmp_type  = RTE_IP_ICMP_ECHO_REQUEST;
                icmp->icmp_code  = 0;
                icmp->icmp_cksum = 0;
                icmp->icmp_cksum = ipChecksum(icmp, pktSize - l4Off);
                break;
            }
            default: {
                ip->next_proto_id = IPPROTO_TCP;
                auto* tcp = reinterpret_cast<rte_tcp_hdr*>(raw + l4Off);
                if (p.srcPort != cachedSrcPort) {
                    cachedSrcPort  = p.srcPort;
                    cachedSrcPortN = htons(p.srcPort);
                }
                if (p.dstPort != cachedDstPort) {
                    cachedDstPort  = p.dstPort;
                    cachedDstPortN = htons(p.dstPort);
                }
                tcp->src_port  = cachedSrcPortN;
                tcp->dst_port  = cachedDstPortN;
                tcp->data_off  = (sizeof(rte_tcp_hdr) / 4) << 4;
                tcp->tcp_flags = RTE_TCP_SYN_FLAG;
                tcp->rx_win    = htons(65535);
                tcp->cksum     = 0;
                break;
            }
            }

            ip->hdr_checksum = 0;
            ip->hdr_checksum = ipChecksum(ip, sizeof(rte_ipv4_hdr));

            m->data_len = static_cast<uint16_t>(pktSize);
            m->pkt_len  = pktSize;
            burst[valid++] = m;
        }

        if (valid == 0) continue;

        const uint16_t enqueued = rte_ring_enqueue_burst(
            m_txRing,
            reinterpret_cast<void**>(burst),
            valid, nullptr);

        for (uint16_t i = enqueued; i < valid; ++i)
            rte_pktmbuf_free(burst[i]);

        // nanosleep ВИДАЛЕНО
    }
}

// ── workerTX — тільки смокче з ring і шле, нуль overhead ─────────────────────
void Generator::workerTX(rte_mempool* pool)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LCORE_TX, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    rte_mbuf* burst[BURST_SIZE];
    uint64_t  totalPackets = 0;
    uint64_t  totalBytes   = 0;
    uint64_t  ringEmpty    = 0;
    uint64_t  txRetries    = 0;
    const uint64_t packetLimit = m_params.law.packetCount;
    auto tStart = std::chrono::steady_clock::now();  // test

    while (isRunning.load(std::memory_order_relaxed)) {
        const uint16_t dequeued = rte_ring_dequeue_burst(
            m_txRing, reinterpret_cast<void**>(burst), BURST_SIZE, nullptr);

        if (dequeued == 0) { ++ringEmpty; rte_pause(); continue; }

        const uint16_t sent = rte_eth_tx_burst(
            m_portId, TX_QUEUE_ID, burst, dequeued);

        // Звільнити ті, що не пройшли — НЕ ретраїти в петлі
        for (uint16_t i = sent; i < dequeued; ++i)
            rte_pktmbuf_free(burst[i]);

        for (uint16_t i = 0; i < sent; ++i)
            totalBytes += burst[i]->pkt_len;

        totalPackets += sent;
        ++txRetries;  // просто лічильник скидань, не петля

        recordStats(totalPackets, totalBytes);

        if (packetLimit > 0 && totalPackets >= packetLimit) {
            isRunning.store(false, std::memory_order_relaxed);
            break;
        }
    }

    double sec = std::chrono::duration<double>(                     // test
        std::chrono::steady_clock::now() - tStart).count();
    fprintf(stderr,
        "[TX] pkts=%lu bytes=%lu ringEmpty=%lu txRetries=%lu "
        "pps=%.0f mbps=%.1f\n",
        totalPackets, totalBytes, ringEmpty, txRetries,
        totalPackets / sec, totalBytes * 8.0 / 1e6 / sec);              // end test

    while (rte_ring_dequeue(m_txRing, reinterpret_cast<void**>(burst)) == 0)
        rte_pktmbuf_free(burst[0]);

    teardownPort(pool);
}

// ─────────────────────────────────────────────────────────────────────────────
// workerRandomLaw
// ─────────────────────────────────────────────────────────────────────────────

void Generator::workerRandomLaw()
{
    const GenLaw::Law& law = m_params.law;
    std::mt19937 rng(std::random_device{}());

    auto sample = law.resolve(rng);
    uint32_t maxPkt = (law.packetSize.max > 0)
                      ? law.packetSize.max : sample.packetSize;
    const uint32_t minFrame = sizeof(rte_ether_hdr) +
                              sizeof(rte_ipv4_hdr)  +
                              sizeof(rte_tcp_hdr);
    maxPkt = std::max(maxPkt, minFrame);

    rte_mempool* pool = nullptr;
    if (!setupPort(maxPkt, pool)) return;

    // створюємо lock-free ring між builder і TX
    const std::string ring_name = "TX_RING_" + std::to_string(m_portId);
    m_txRing = rte_ring_create(ring_name.c_str(), RING_SIZE,
                               rte_socket_id(),
                               0 | RING_F_SC_DEQ);;
    if (!m_txRing) {
        pushError("rte_ring_create failed: " +
                  std::string(rte_strerror(rte_errno)));
        teardownPort(pool);
        return;
    }

    // запускаємо builder на окремому std::thread
    std::thread builder1(&Generator::workerBuilder, this, pool, std::cref(law), 2);
    std::thread builder2(&Generator::workerBuilder, this, pool, std::cref(law), 3);

    // TX крутиться в поточному thread (він вже workerRandomLaw)
    workerTX(pool);

    builder1.join();
    builder2.join();

    rte_ring_free(m_txRing);
    m_txRing = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// workerPcapPlayer
// ─────────────────────────────────────────────────────────────────────────────

using PcapFrame = std::pair<uint64_t /*ts_µs*/, std::vector<uint8_t>>;

static std::vector<PcapFrame> parsePcap(const std::vector<uint8_t>& buf)
{
    std::vector<PcapFrame> frames;
    if (buf.size() < sizeof(PcapGlobalHdr)) return frames;

    const uint8_t* p   = buf.data();
    const uint8_t* end = buf.data() + buf.size();

    auto r32 = [](const uint8_t* x) {
        uint32_t v; memcpy(&v, x, 4); return v;
    };

    const auto* gh = reinterpret_cast<const PcapGlobalHdr*>(p);
    const bool swapped = (gh->magic_number == 0xD4C3B2A1 ||
                          gh->magic_number == 0x4D3CB2A1);
    const bool nsec    = (gh->magic_number == 0xA1B23C4D ||
                          gh->magic_number == 0x4D3CB2A1);

    auto r32s = [&](const uint8_t* x) {
        uint32_t v = r32(x);
        return swapped ? __builtin_bswap32(v) : v;
    };

    p += sizeof(PcapGlobalHdr);

    while (p + sizeof(PcapRecHdr) <= end) {
        const uint32_t ts_sec  = r32s(p);
        const uint32_t ts_frac = r32s(p + 4);
        const uint32_t incl    = r32s(p + 8);
        p += sizeof(PcapRecHdr);

        if (p + incl > end) break;

        const uint64_t ts_us = static_cast<uint64_t>(ts_sec) * 1'000'000ULL
                               + (nsec ? ts_frac / 1000 : ts_frac);
        frames.push_back({ ts_us, std::vector<uint8_t>(p, p + incl) });
        p += incl;
    }
    return frames;
}

void Generator::workerPcapPlayer()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    const PcapParams::PlayerSettings& ps = m_params.playerSettings;

    std::vector<uint8_t> raw;

    if (!m_pcapData.empty()) {
        raw = m_pcapData;
        std::cout << "[Generator] PcapPlayer: using in-RAM buffer ("
                  << raw.size() << " bytes)\n";
    } else {
        std::ifstream f(ps.filePath, std::ios::binary | std::ios::ate);
        if (!f) { pushError("PcapPlayer: cannot open " + ps.filePath); return; }
        const std::streamsize fsize = f.tellg();
        f.seekg(0);
        raw.resize(static_cast<size_t>(fsize));
        if (!f.read(reinterpret_cast<char*>(raw.data()), fsize)) {
            pushError("PcapPlayer: read error on " + ps.filePath);
            return;
        }
        std::cout << "[Generator] PcapPlayer: loaded file " << ps.filePath
                  << " (" << fsize << " bytes)\n";
    }

    const std::vector<PcapFrame> allFrames = parsePcap(raw);
    if (allFrames.empty()) {
        pushError("PcapPlayer: no frames parsed");
        return;
    }

    const int total  = static_cast<int>(allFrames.size());
    int startIdx = std::max(0, ps.startPacket - 1);
    int endIdx   = (ps.endPacket <= 0 || ps.endPacket > total)
                   ? total - 1 : ps.endPacket - 1;
    if (startIdx > endIdx) startIdx = 0;

    uint32_t maxPkt = 64;
    for (int i = startIdx; i <= endIdx; ++i)
        maxPkt = std::max(maxPkt,
                          static_cast<uint32_t>(allFrames[i].second.size()));

    rte_mempool* pool = nullptr;
    if (!setupPort(maxPkt, pool)) return;

    uint64_t fixedInterNS = 0;
    if (ps.speedMode == PcapParams::SpeedMode::Fixed && ps.fixedRate > 0)
        fixedInterNS = static_cast<uint64_t>(1e9 / ps.fixedRate);

    uint64_t totalPackets = 0;
    uint64_t totalBytes   = 0;

    struct timespec startWall{};
    clock_gettime(CLOCK_MONOTONIC, &startWall);
    const uint64_t startNS = static_cast<uint64_t>(startWall.tv_sec) * 1'000'000'000ULL
                             + startWall.tv_nsec;

    const int loopLimit = (ps.loop && ps.loopCount > 0) ? ps.loopCount : 1;

    for (int loopN = 0; loopN < loopLimit && isRunning.load(); ++loopN) {
        uint64_t prevTs = allFrames[startIdx].first;

        for (int fi = startIdx; fi <= endIdx; ++fi) {
            if (!isRunning.load()) goto done;
            if (!waitIfPaused(isRunning, isPaused, m_mutex, m_pauseCV)) goto done;

            if (m_params.time > 0) {
                struct timespec tNow{};
                clock_gettime(CLOCK_MONOTONIC, &tNow);
                const uint64_t nowNS =
                    static_cast<uint64_t>(tNow.tv_sec) * 1'000'000'000ULL
                    + tNow.tv_nsec;
                if ((nowNS - startNS) >=
                    static_cast<uint64_t>(m_params.time) * 1'000'000'000ULL)
                    goto done;
            }

            const PcapFrame& frame   = allFrames[fi];
            const auto&      payload = frame.second;
            const uint32_t   pktSize = static_cast<uint32_t>(payload.size());

            uint64_t sleepNS = 0;
            switch (ps.speedMode) {
            case PcapParams::SpeedMode::Max:
                sleepNS = 0;
                break;
            case PcapParams::SpeedMode::Fixed:
                sleepNS = fixedInterNS;
                break;
            case PcapParams::SpeedMode::Multiplier: {
                const uint64_t deltaUs = frame.first > prevTs
                                         ? frame.first - prevTs : 0;
                sleepNS = (ps.speedMult > 0.0)
                          ? static_cast<uint64_t>((deltaUs * 1000ULL) / ps.speedMult)
                          : deltaUs * 1000ULL;
                break;
            }
            default:
                sleepNS = frame.first > prevTs
                          ? (frame.first - prevTs) * 1000ULL : 0;
                break;
            }
            prevTs = frame.first;

            if (sleepNS > 0) {
                struct timespec ts {
                    .tv_sec  = static_cast<time_t>(sleepNS / 1'000'000'000ULL),
                    .tv_nsec = static_cast<long>  (sleepNS % 1'000'000'000ULL)
                };
                nanosleep(&ts, nullptr);
            }

            rte_mbuf* m = rte_pktmbuf_alloc(pool);
            if (!m) { pushWarning("PcapPlayer: mbuf alloc failed"); continue; }

            uint8_t* dst = reinterpret_cast<uint8_t*>(
                               rte_pktmbuf_append(m, pktSize));
            if (!dst) { rte_pktmbuf_free(m); continue; }

            memcpy(dst, payload.data(), pktSize);
            m->data_len = static_cast<uint16_t>(pktSize);
            m->pkt_len  = pktSize;

            rte_mbuf* one[1] = { m };
            const uint16_t sent = rte_eth_tx_burst(m_portId, TX_QUEUE_ID, one, 1);
            if (sent < 1) {
                pushWarning("PcapPlayer: TX dropped packet");
                rte_pktmbuf_free(m);
            } else {
                ++totalPackets;
                totalBytes += pktSize;
                // Hand off totals to the stats thread — no direct queue push here.
                recordStats(totalPackets, totalBytes);
            }
        }
    }

done:
    teardownPort(pool);
}