#include "Generator.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

// ── DPDK TX constants (all generation-related, not EAL) ──────────────────────
static constexpr uint16_t TX_QUEUE_ID = 0;
static constexpr uint16_t TX_DESC     = 1024;
static constexpr uint16_t RX_DESC_MIN = 64;   // some PMDs require ≥1 RX queue
static constexpr uint32_t MBUF_COUNT  = 8192;
static constexpr uint32_t MBUF_CACHE  = 256;
static constexpr uint16_t BURST_SIZE  = 32;

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
}

Generator::~Generator()
{
    doStop();
    delete statQueue;
}

generator::statisticQueue* Generator::getQueueP() { return statQueue; }

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
    // Publish one snapshot to statQueue each second while the generator runs.
    // Uses a condition-variable wait with a 1-second timeout so it also wakes
    // up immediately when isRunning is cleared (avoids a 1-second hang on stop).
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
}

void Generator::pushWarning(const std::string& msg)
{
    std::cerr << "[Generator] WARN:  " << msg << "\n";
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

// ─────────────────────────────────────────────────────────────────────────────
// workerRandomLaw
// ─────────────────────────────────────────────────────────────────────────────

static uint16_t ipChecksum(const void* data, size_t len)
{
    const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    for (; len > 1; len -= 2) sum += *p++;
    if (len) sum += *reinterpret_cast<const uint8_t*>(p);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

void Generator::workerRandomLaw()
{
    const GenLaw::Law& law = m_params.law;
    std::mt19937 rng(std::random_device{}());

    auto sample = law.resolve(rng);
    uint32_t maxPkt = (law.packetSize.max > 0)
                      ? law.packetSize.max : sample.packetSize;
    maxPkt = std::max(maxPkt, static_cast<uint32_t>(
                 sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr) +
                 sizeof(rte_tcp_hdr)));

    rte_mempool* pool = nullptr;
    if (!setupPort(maxPkt, pool)) return;

    uint64_t totalPackets = 0;
    uint64_t totalBytes   = 0;
    const uint64_t packetLimit = law.packetCount;

    std::vector<rte_mbuf*>  burst;
    std::vector<uint32_t>   burstSizes;
    burst.reserve(BURST_SIZE);
    burstSizes.reserve(BURST_SIZE);

    while (isRunning.load()) {
        if (!waitIfPaused(isRunning, isPaused, m_mutex, m_pauseCV)) break;

        burst.clear();
        burstSizes.clear();

        GenLaw::Law::Resolved p{};

        for (uint16_t bi = 0; bi < BURST_SIZE && isRunning.load(); ++bi) {
            p = law.resolve(rng);

            const uint32_t minFrame = sizeof(rte_ether_hdr) +
                                      sizeof(rte_ipv4_hdr) +
                                      sizeof(rte_tcp_hdr);
            uint32_t pktSize = std::max(p.packetSize, minFrame);

            rte_mbuf* m = rte_pktmbuf_alloc(pool);
            if (!m) { pushWarning("mbuf alloc failed"); break; }

            uint8_t* raw = reinterpret_cast<uint8_t*>(
                               rte_pktmbuf_append(m, pktSize));
            if (!raw) { rte_pktmbuf_free(m); break; }
            memset(raw, 0, pktSize);

            // Ethernet
            auto* eth = reinterpret_cast<rte_ether_hdr*>(raw);
            memset(&eth->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
            rte_eth_macaddr_get(m_portId, &eth->src_addr);
            eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

            // IP
            const size_t ipOff = sizeof(rte_ether_hdr);
            auto* ip = reinterpret_cast<rte_ipv4_hdr*>(raw + ipOff);
            ip->version_ihl     = 0x45;
            ip->type_of_service = 0;
            ip->total_length    = htons(static_cast<uint16_t>(
                                        pktSize - ipOff));
            ip->packet_id       = 0;
            ip->fragment_offset = 0;
            ip->time_to_live    = p.ttl;
            ip->src_addr        = inet_addr(p.srcIP.c_str());
            ip->dst_addr        = inet_addr(p.dstIP.c_str());

            const size_t l4Off = ipOff + sizeof(rte_ipv4_hdr);

            switch (p.protocol) {
            case GenLaw::Protocol::UDP: {
                ip->next_proto_id = IPPROTO_UDP;
                auto* udp = reinterpret_cast<rte_udp_hdr*>(raw + l4Off);
                udp->src_port    = htons(p.srcPort);
                udp->dst_port    = htons(p.dstPort);
                udp->dgram_len   = htons(static_cast<uint16_t>(
                                         pktSize - l4Off));
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
                tcp->src_port  = htons(p.srcPort);
                tcp->dst_port  = htons(p.dstPort);
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
            burstSizes.push_back(pktSize);
            burst.push_back(m);
        }

        if (burst.empty()) break;

        uint16_t sent = rte_eth_tx_burst(m_portId, TX_QUEUE_ID,
                                         burst.data(),
                                         static_cast<uint16_t>(burst.size()));
        for (uint16_t i = sent; i < static_cast<uint16_t>(burst.size()); ++i)
            rte_pktmbuf_free(burst[i]);

        totalPackets += sent;
        for (uint16_t i = 0; i < sent; ++i)
            totalBytes += burstSizes[i];

        // Hand off totals to the stats thread — no direct queue push here.
        recordStats(totalPackets, totalBytes);

        if (packetLimit > 0 && totalPackets >= packetLimit) break;

        if (p.timeDiff > 0.0) {
            uint64_t sleepNS = static_cast<uint64_t>(p.timeDiff * 1e9);
            struct timespec ts {
                .tv_sec  = static_cast<time_t>(sleepNS / 1'000'000'000ULL),
                .tv_nsec = static_cast<long>  (sleepNS % 1'000'000'000ULL)
            };
            nanosleep(&ts, nullptr);
        }
    }

    teardownPort(pool);
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