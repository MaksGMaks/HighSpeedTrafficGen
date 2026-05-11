// dpdk_send.cpp
// Build: g++ dpdk_send.cpp -o dpdk_send $(pkg-config --cflags --libs libdpdk)
// Run:   sudo ./dpdk_send -l 0-1 -n 4 -- --port 0 --count 1000 --size 64

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

// ─── Конфігурація ────────────────────────────────────────────────────────────

static constexpr uint16_t TX_QUEUE_ID   = 0;
static constexpr uint16_t TX_DESC       = 1024;
static constexpr uint32_t MBUF_COUNT    = 8192;
static constexpr uint32_t MBUF_CACHE    = 256;
static constexpr uint16_t BURST_SIZE    = 32;

static std::atomic<bool> g_running{true};

// ─── Сигнал ──────────────────────────────────────────────────────────────────

static void signal_handler(int) {
    g_running = false;
}

// ─── Параметри ───────────────────────────────────────────────────────────────

struct AppParams {
    uint16_t port_id  = 0;
    uint32_t pkt_size = 64;     // байт
    uint64_t count    = 0;      // 0 = нескінченно
};

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [EAL options] -- [--port N] [--size N] [--count N]\n"
              << "  --port  N   PCI port index (default 0)\n"
              << "  --size  N   packet size in bytes (default 64, min 60)\n"
              << "  --count N   packets to send, 0 = infinite (default 0)\n";
}

static AppParams parse_app_args(int argc, char** argv) {
    AppParams p;
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--port")  && i+1 < argc) p.port_id  = atoi(argv[++i]);
        if (!strcmp(argv[i], "--size")  && i+1 < argc) p.pkt_size = atoi(argv[++i]);
        if (!strcmp(argv[i], "--count") && i+1 < argc) p.count    = atoll(argv[++i]);
    }
    if (p.pkt_size < 60) {
        std::cerr << "Packet size must be >= 60, setting to 60\n";
        p.pkt_size = 60;
    }
    return p;
}

// ─── Ініціалізація порту ─────────────────────────────────────────────────────

static bool port_init(uint16_t port_id, rte_mempool* pool) {
    if (!rte_eth_dev_is_valid_port(port_id)) {
        std::cerr << "Port " << port_id << " is not valid\n";
        return false;
    }

    rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);
    std::cout << "Port " << port_id
              << " driver: " << dev_info.driver_name << "\n";

    rte_eth_conf port_conf{};
    // r8169 потребує явного MTU
    port_conf.rxmode.mtu = RTE_ETHER_MAX_LEN;

    // 1 RX + 1 TX — r8169 не підтримує 0 RX черг
    if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0) {
        std::cerr << "rte_eth_dev_configure failed\n";
        return false;
    }

    // Коригуємо дескриптори
    uint16_t nb_rxd = dev_info.rx_desc_lim.nb_min;  // мінімум для r8169 = 64
    uint16_t nb_txd = TX_DESC;
    if (rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd) < 0) {
        std::cerr << "adjust_nb_rx_tx_desc failed\n";
        return false;
    }
    std::cout << "RX desc: " << nb_rxd << " TX desc: " << nb_txd << "\n";

    // RX черга — обов'язкова для r8169, але читати з неї не будемо
    rte_eth_rxconf rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    if (rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
            rte_eth_dev_socket_id(port_id), &rx_conf, pool) < 0) {
        std::cerr << "rte_eth_rx_queue_setup failed\n";
        return false;
    }

    // TX черга
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

// ─── Підготовка пакета ───────────────────────────────────────────────────────

static void fill_packet(uint8_t* data, uint32_t size) {
    // Мінімальний Ethernet фрейм: dst mac + src mac + ethertype + payload
    rte_ether_hdr* eth = (rte_ether_hdr*)data;

    // Broadcast destination
    memset(eth->dst_addr.addr_bytes, 0xFF, RTE_ETHER_ADDR_LEN);
    // Source — довільна
    uint8_t src[RTE_ETHER_ADDR_LEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    memcpy(eth->src_addr.addr_bytes, src, RTE_ETHER_ADDR_LEN);
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // Payload заповнюємо патерном
    uint32_t payload_off = sizeof(rte_ether_hdr);
    for (uint32_t i = payload_off; i < size; ++i)
        data[i] = (uint8_t)(i & 0xFF);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    // 1. EAL init — споживає свої аргументи, повертає індекс першого app-аргу
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        std::cerr << "EAL init failed: " << rte_strerror(rte_errno) << "\n";
        return 1;
    }
    // Аргументи після "--"
    int app_argc = argc - ret;
    char** app_argv = argv + ret;

    if (app_argc > 1 && !strcmp(app_argv[1], "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    AppParams params = parse_app_args(app_argc, app_argv);

    // 2. Перевірка портів
    uint16_t nb_ports = rte_eth_dev_count_avail();
    std::cout << "Available DPDK ports: " << nb_ports << "\n";
    if (nb_ports == 0) {
        std::cerr << "No ports found. Bind NIC with dpdk-devbind.py first.\n";
        rte_eal_cleanup();
        return 1;
    }
    if (params.port_id >= nb_ports) {
        std::cerr << "Port " << params.port_id << " not available\n";
        rte_eal_cleanup();
        return 1;
    }

    // 3. Mempool
    uint32_t data_room = std::max(
        params.pkt_size + RTE_PKTMBUF_HEADROOM,
        (uint32_t)RTE_MBUF_DEFAULT_BUF_SIZE
    );
    rte_mempool* pool = rte_pktmbuf_pool_create(
        "TX_POOL", MBUF_COUNT, MBUF_CACHE, 0, data_room, rte_socket_id());
    if (!pool) {
        std::cerr << "Mempool create failed: " << rte_strerror(rte_errno) << "\n";
        rte_eal_cleanup();
        return 1;
    }

    // 4. Порт
    if (!port_init(params.port_id, pool)) {
        rte_mempool_free(pool);
        rte_eal_cleanup();
        return 1;
    }

    // 5. Преалокуємо burst
    std::vector<rte_mbuf*> burst(BURST_SIZE);
    for (auto& m : burst) {
        m = rte_pktmbuf_alloc(pool);
        if (!m) {
            std::cerr << "mbuf alloc failed during pre-alloc\n";
            rte_eal_cleanup();
            return 1;
        }
        uint8_t* data = (uint8_t*)rte_pktmbuf_append(m, params.pkt_size);
        if (!data) {
            std::cerr << "pktmbuf_append failed\n";
            rte_eal_cleanup();
            return 1;
        }
        fill_packet(data, params.pkt_size);
    }

    // 6. TX цикл
    uint64_t total_sent = 0;
    std::cout << "Sending"
              << (params.count ? " " + std::to_string(params.count) : " infinite")
              << " packets of " << params.pkt_size << " bytes"
              << " from port " << params.port_id
              << " (Ctrl+C to stop)...\n";

    while (g_running) {
        // Скільки відправляємо цього burst
        uint16_t to_send = BURST_SIZE;
        if (params.count > 0) {
            uint64_t remaining = params.count - total_sent;
            if (remaining == 0) break;
            if (remaining < BURST_SIZE)
                to_send = (uint16_t)remaining;
        }

        uint16_t sent = rte_eth_tx_burst(
            params.port_id, TX_QUEUE_ID, burst.data(), to_send);

        // Надіслані mbufs DPDK звільнив сам — алокуємо нові на їх місце
        for (uint16_t i = 0; i < sent; ++i) {
            burst[i] = rte_pktmbuf_alloc(pool);
            if (!burst[i]) {
                // Pool вичерпано — чекаємо
                burst[i] = nullptr;
                continue;
            }
            uint8_t* data = (uint8_t*)rte_pktmbuf_append(burst[i], params.pkt_size);
            if (data) fill_packet(data, params.pkt_size);
        }

        total_sent += sent;

        if (sent == 0) {
            // TX ring повний — невелика пауза
            rte_pause();
        }
    }

    // 7. Статистика
    rte_eth_stats stats;
    rte_eth_stats_get(params.port_id, &stats);
    printf("\n--- TX stats ---\n");
    printf("Packets sent:  %lu\n", stats.opackets);
    printf("Bytes sent:    %lu\n", stats.obytes);
    printf("TX errors:     %lu\n", stats.oerrors);
    printf("Total counted: %lu\n", total_sent);

    // 8. Cleanup
    // Звільняємо mbufs що залишились у burst (не надіслані)
    for (auto* m : burst)
        if (m) rte_pktmbuf_free(m);

    rte_eth_dev_stop(params.port_id);
    rte_eth_dev_close(params.port_id);
    rte_mempool_free(pool);
    rte_eal_cleanup();

    return 0;
}