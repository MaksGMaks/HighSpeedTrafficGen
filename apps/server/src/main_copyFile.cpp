// dpdk_pcap_player.cpp
// Build: g++ dpdk_pcap_player.cpp -o dpdk_pcap_player \
//        $(pkg-config --cflags --libs libdpdk) \
//        -lPcap++ -lPacket++ -lCommon++ -lpcap -std=c++17
// Run:   sudo ./dpdk_pcap_player -l 0-1 -n 4 --huge-dir=/mnt/huge \
//               -- --port 0 --file traffic.pcap [--loop] [--dst-mac AA:BB:CC:DD:EE:FF]

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <atomic>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_cycles.h>

// PcapPlusPlus
#include "../../third-party/pcapplusplus/include/pcapplusplus/PcapFileDevice.h"
#include "../../third-party/pcapplusplus/include/pcapplusplus/RawPacket.h"

// ─── Globals ─────────────────────────────────────────────────────────────────

static constexpr uint16_t TX_QUEUE_ID = 0;
static constexpr uint16_t TX_DESC     = 1024;
static constexpr uint32_t MBUF_COUNT  = 8192;
static constexpr uint32_t MBUF_CACHE  = 256;

static std::atomic<bool> g_running{true};

static void signal_handler(int) { g_running = false; }

// ─── Структури ───────────────────────────────────────────────────────────────

struct TimedPacket {
    std::vector<uint8_t> data;
    uint64_t offset_ns;  // зміщення від першого пакету в наносекундах
};

struct AppParams {
    uint16_t    port_id  = 0;
    std::string pcap_file{};
    std::string dst_mac{};   // опціонально — замінити dst MAC
    bool        loop     = false;
};

// ─── Аргументи ───────────────────────────────────────────────────────────────

static void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [EAL opts] -- [options]\n"
        << "  --port    N              DPDK port index (default 0)\n"
        << "  --file    path.pcap      pcap file to replay\n"
        << "  --loop                   repeat after end of file\n"
        << "  --dst-mac AA:BB:CC:DD:EE:FF  override destination MAC\n";
}

static AppParams parse_app_args(int argc, char** argv) {
    AppParams p;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--port")    && i+1 < argc) p.port_id   = atoi(argv[++i]);
        if (!strcmp(argv[i], "--file")    && i+1 < argc) p.pcap_file = argv[++i];
        if (!strcmp(argv[i], "--dst-mac") && i+1 < argc) p.dst_mac   = argv[++i];
        if (!strcmp(argv[i], "--loop"))                  p.loop      = true;
        if (!strcmp(argv[i], "--help"))  { print_usage(argv[0]); exit(0); }
    }
    return p;
}

// ─── Завантаження pcap ───────────────────────────────────────────────────────

static std::vector<uint8_t> convert_to_eth(
    const uint8_t* data, int len,
    LinkType link_type,
    const rte_ether_addr& src_mac,
    const rte_ether_addr& dst_mac)
{
    uint16_t proto   = 0;
    int      payload_offset = 0;

    switch (link_type) {
        case LinkType::ETHERNET:
            // Вже Ethernet — лише замінюємо dst/src MAC
            if (len < ETH_HEADER_LEN) return {};
            {
                std::vector<uint8_t> out(data, data + len);
                rte_ether_hdr* eth = (rte_ether_hdr*)out.data();
                memcpy(eth->dst_addr.addr_bytes,
                       dst_mac.addr_bytes, RTE_ETHER_ADDR_LEN);
                memcpy(eth->src_addr.addr_bytes,
                       src_mac.addr_bytes, RTE_ETHER_ADDR_LEN);
                return out;
            }

        case LinkType::SLL:
            if (len < SLL_HEADER_LEN) return {};
            proto          = ((const SLLHeader*)data)->proto_type;
            payload_offset = SLL_HEADER_LEN;
            break;

        case LinkType::SLL2:
            if (len < SLL2_HEADER_LEN) return {};
            proto          = ((const SLL2Header*)data)->proto_type;
            payload_offset = SLL2_HEADER_LEN;
            break;

        case LinkType::RAW_IP:
            // Немає жодного заголовка канального рівня — додаємо ETH
            proto          = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
            payload_offset = 0;
            break;

        default:
            std::cerr << "Unsupported link type, skipping packet\n";
            return {};
    }

    // Будуємо Ethernet фрейм
    int payload_len = len - payload_offset;
    if (payload_len <= 0) return {};

    std::vector<uint8_t> out(ETH_HEADER_LEN + payload_len);
    rte_ether_hdr* eth = (rte_ether_hdr*)out.data();
    memcpy(eth->dst_addr.addr_bytes, dst_mac.addr_bytes, RTE_ETHER_ADDR_LEN);
    memcpy(eth->src_addr.addr_bytes, src_mac.addr_bytes, RTE_ETHER_ADDR_LEN);
    eth->ether_type = proto;
    memcpy(out.data() + ETH_HEADER_LEN, data + payload_offset, payload_len);

    return out;
}

static std::vector<TimedPacket> load_pcap(const std::string& path) {
    pcpp::PcapFileReaderDevice reader(path);
    if (!reader.open()) {
        std::cerr << "Cannot open pcap file: " << path << "\n";
        return {};
    }

    std::vector<TimedPacket> packets;
    pcpp::RawPacket rawPkt;
    uint64_t firstNs = 0;
    bool first = true;

    while (reader.getNextPacket(rawPkt)) {
        timespec ts = rawPkt.getPacketTimeStamp();
        uint64_t ns = (uint64_t)ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;

        if (first) { firstNs = ns; first = false; }

        TimedPacket pkt;
        pkt.offset_ns = ns - firstNs;
        pkt.data.assign(
            rawPkt.getRawData(),
            rawPkt.getRawData() + rawPkt.getRawDataLen()
        );
        packets.push_back(std::move(pkt));
    }

    reader.close();
    return packets;
}

// ─── Опціональна заміна dst MAC ──────────────────────────────────────────────

static bool parse_mac(const std::string& str, rte_ether_addr& out) {
    return sscanf(str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &out.addr_bytes[0], &out.addr_bytes[1], &out.addr_bytes[2],
        &out.addr_bytes[3], &out.addr_bytes[4], &out.addr_bytes[5]) == 6;
}

static void patch_dst_mac(uint8_t* data, size_t len,
                           const rte_ether_addr& mac) {
    if (len < RTE_ETHER_ADDR_LEN) return;
    memcpy(data, mac.addr_bytes, RTE_ETHER_ADDR_LEN);
}

// ─── Ініціалізація порту ─────────────────────────────────────────────────────

static bool port_init(uint16_t port_id, rte_mempool* pool) {
    if (!rte_eth_dev_is_valid_port(port_id)) {
        std::cerr << "Port " << port_id << " is invalid\n";
        return false;
    }

    rte_eth_dev_info dev_info;
    if (rte_eth_dev_info_get(port_id, &dev_info) < 0) {
        std::cerr << "rte_eth_dev_info_get failed\n";
        return false;
    }
    printf("Port %u driver: %s\n", port_id, dev_info.driver_name);

    rte_eth_conf port_conf{};
    port_conf.rxmode.mtu = RTE_ETHER_MAX_LEN;

    if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0) {
        std::cerr << "rte_eth_dev_configure failed\n";
        return false;
    }

    uint16_t nb_rxd = dev_info.rx_desc_lim.nb_min;
    uint16_t nb_txd = TX_DESC;
    rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);

    rte_eth_rxconf rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    if (rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
            rte_eth_dev_socket_id(port_id), &rx_conf, pool) < 0) {
        std::cerr << "rte_eth_rx_queue_setup failed\n";
        return false;
    }

    rte_eth_txconf tx_conf = dev_info.default_txconf;
    tx_conf.offloads = port_conf.txmode.offloads;
    if (rte_eth_tx_queue_setup(port_id, TX_QUEUE_ID, nb_txd,
            rte_eth_dev_socket_id(port_id), &tx_conf) < 0) {
        std::cerr << "rte_eth_tx_queue_setup failed\n";
        return false;
    }

    if (rte_eth_dev_start(port_id) < 0) {
        std::cerr << "rte_eth_dev_start failed\n";
        return false;
    }

    rte_ether_addr mac;
    rte_eth_macaddr_get(port_id, &mac);
    printf("Port %u MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", port_id,
        mac.addr_bytes[0], mac.addr_bytes[1], mac.addr_bytes[2],
        mac.addr_bytes[3], mac.addr_bytes[4], mac.addr_bytes[5]);

    rte_eth_promiscuous_enable(port_id);
    return true;
}

// ─── Один прохід відтворення ─────────────────────────────────────────────────

static uint64_t replay_once(uint16_t port_id, rte_mempool* pool,
                             const std::vector<TimedPacket>& packets,
                             const rte_ether_addr* dst_mac_override,
                             uint32_t pass) {
    uint64_t tsc_hz  = rte_get_tsc_hz();
    uint64_t startTs = rte_rdtsc();
    uint64_t sent_total = 0;

    printf("\n[Pass %u] Replaying %zu packets...\n", pass, packets.size());

    for (size_t i = 0; i < packets.size() && g_running; ++i) {
        // Цільовий TSC для цього пакету
        uint64_t target_tsc = startTs
            + packets[i].offset_ns * tsc_hz / 1'000'000'000ULL;

        // Чекаємо до потрібного часу
        uint64_t now = rte_rdtsc();
        if (target_tsc > now) {
            uint64_t diff_ns = (target_tsc - now) * 1'000'000'000ULL / tsc_hz;
            if (diff_ns > 200'000) {
                // Великі інтервали — nanosleep (звільняємо CPU)
                timespec ts{
                    (time_t)(diff_ns / 1'000'000'000ULL),
                    (long)  (diff_ns % 1'000'000'000ULL)
                };
                nanosleep(&ts, nullptr);
            }
            // Фінальне доточнення — busy wait
            while (rte_rdtsc() < target_tsc && g_running)
                rte_pause();
        }

        // Алокація mbuf
        rte_mbuf* mbuf = rte_pktmbuf_alloc(pool);
        if (!mbuf) {
            std::cerr << "mbuf alloc failed at packet " << i << "\n";
            continue;
        }

        uint8_t* data = (uint8_t*)rte_pktmbuf_append(mbuf, packets[i].data.size());
        if (!data) {
            rte_pktmbuf_free(mbuf);
            continue;
        }

        memcpy(data, packets[i].data.data(), packets[i].data.size());
        if (dst_mac_override)
            patch_dst_mac(data, packets[i].data.size(), *dst_mac_override);

        // TX з retry якщо ring повний
        uint16_t sent = 0;
        while (sent == 0 && g_running) {
            sent = rte_eth_tx_burst(port_id, TX_QUEUE_ID, &mbuf, 1);
            if (sent == 0) rte_pause();
        }
        sent_total += sent;

        // Прогрес кожні 1000 пакетів
        if ((i + 1) % 1000 == 0) {
            printf("  [Pass %u] %zu / %zu packets sent\r",
                pass, i + 1, packets.size());
            fflush(stdout);
        }
    }

    return sent_total;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    // 1. EAL
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        std::cerr << "EAL init failed: " << rte_strerror(rte_errno) << "\n";
        return 1;
    }
    int    app_argc = argc - ret;
    char** app_argv = argv + ret;

    AppParams params = parse_app_args(app_argc, app_argv);

    if (params.pcap_file.empty()) {
        std::cerr << "Error: --file is required\n";
        print_usage(argv[0]);
        return 1;
    }

    // 2. Завантаження pcap
    printf("Loading pcap: %s\n", params.pcap_file.c_str());
    auto packets = load_pcap(params.pcap_file);
    if (packets.empty()) {
        std::cerr << "No packets loaded\n";
        return 1;
    }

    uint64_t duration_ns = packets.back().offset_ns;
    printf("Loaded %zu packets, duration: %.3f s\n",
        packets.size(), duration_ns / 1e9);

    // 3. Опціональний dst MAC override
    rte_ether_addr dst_mac{};
    rte_ether_addr* dst_mac_ptr = nullptr;
    if (!params.dst_mac.empty()) {
        if (!parse_mac(params.dst_mac, dst_mac)) {
            std::cerr << "Invalid MAC: " << params.dst_mac << "\n";
            return 1;
        }
        dst_mac_ptr = &dst_mac;
        printf("Overriding dst MAC: %s\n", params.dst_mac.c_str());
    }

    // 4. Перевірка портів
    uint16_t nb_ports = rte_eth_dev_count_avail();
    printf("Available DPDK ports: %u\n", nb_ports);
    if (nb_ports == 0 || params.port_id >= nb_ports) {
        std::cerr << "Port " << params.port_id << " not available\n";
        return 1;
    }

    // 5. Mempool — розмір під найбільший пакет у файлі
    uint32_t max_pkt = 0;
    for (auto& p : packets)
        max_pkt = std::max(max_pkt, (uint32_t)p.data.size());

    uint32_t data_room = std::max(
        max_pkt + RTE_PKTMBUF_HEADROOM,
        (uint32_t)RTE_MBUF_DEFAULT_BUF_SIZE
    );
    rte_mempool* pool = rte_pktmbuf_pool_create(
        "PCAP_POOL", MBUF_COUNT, MBUF_CACHE, 0, data_room, rte_socket_id());
    if (!pool) {
        std::cerr << "Mempool create failed: " << rte_strerror(rte_errno) << "\n";
        return 1;
    }

    // 6. Порт
    if (!port_init(params.port_id, pool)) {
        rte_mempool_free(pool);
        rte_eal_cleanup();
        return 1;
    }

    // 7. Відтворення
    printf("\nMode: %s\n", params.loop ? "LOOP (Ctrl+C to stop)" : "SINGLE PASS");
    printf("Starting replay...\n");

    uint32_t pass       = 1;
    uint64_t total_sent = 0;

    do {
        uint64_t sent = replay_once(
            params.port_id, pool, packets, dst_mac_ptr, pass);
        total_sent += sent;

        printf("\n[Pass %u] Done. Sent: %lu / %zu packets\n",
            pass, sent, packets.size());

        if (params.loop && g_running) {
            printf("[Pass %u] Restarting...\n", pass + 1);
        }
        ++pass;

    } while (params.loop && g_running);

    // 8. Фінальна статистика
    rte_eth_stats stats;
    rte_eth_stats_get(params.port_id, &stats);

    printf("\n════════════════════════════════\n");
    printf("  Total passes:      %u\n",    pass - 1);
    printf("  Total pkt counted: %lu\n",   total_sent);
    printf("  HW opackets:       %lu\n",   stats.opackets);
    printf("  HW obytes:         %lu\n",   stats.obytes);
    printf("  HW TX errors:      %lu\n",   stats.oerrors);
    printf("════════════════════════════════\n");

    // 9. Cleanup
    rte_eth_dev_stop(params.port_id);
    rte_eth_dev_close(params.port_id);
    rte_mempool_free(pool);
    rte_eal_cleanup();

    return 0;
}