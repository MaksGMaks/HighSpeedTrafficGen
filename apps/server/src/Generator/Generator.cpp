#include "Generator.hpp"

#include "../../../../libs/common/include/generator_values.hpp"

Generator::Generator()
: isRunning(false)
, isPaused(false) {
    statQueue = new statisticQueue();
    int nb_ports = rte_eth_dev_count_avail();
    if (nb_ports <= 0) {
        std::cerr << "[Generator] No DPDK ports available\n";
        return;
    }


    connect(this, &Generator::finished, this, &Generator::doStop);
}

Generator::~Generator() {
    std::cout << "[Generator::~Generator] stop thread and destroy object" << std::endl;
    doStop();
}

statisticQueue* Generator::getQueueP() {
    return statQueue;
}

bool Generator::presetDPDK() {
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

void Generator::doStart(const genParams& params) {
    std::cout << "[Generator::doStart] start thread" << std::endl;
    isRunning = true;
    isPaused = false;

}

void Generator::doStop() {
    std::cout << "[Generator::doStop] stop thread" << std::endl;
    {
        std::lock_guard lock(m_mutex);
        isRunning = false;
    }
    doResume();
    if(m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void Generator::doPause() {
    std::cout << "[Generator::doPause] pause thread" << std::endl;
    std::lock_guard lock(m_mutex);
    isPaused = true;
}

void Generator::doResume() {
    {
        std::lock_guard lock(m_mutex);
        isPaused = false;
    }
    m_pause.notify_all();
}

void Generator::getDPDKStat() {
    while (isRunning) {
        rte_eth_stats stats;
        rte_eth_stats_get(params.port_id, &stats);

        statisticData currData(stats.opackets, stats.obytes, stats.oerrors, total_sent);
    }
}

/**
 * @brief dpdkSend
 * @details Send packets using DPDK
 * @note DPDK is not supported on all interfaces. Check interface support before using.
*/
void Generator::dpdkSend() {
    uint32_t pktSize;
    if (genParams)
    // Mempool
    uint32_t data_room = std::max(
        params.pkt_size + RTE_PKTMBUF_HEADROOM,
        (uint32_t)RTE_MBUF_DEFAULT_BUF_SIZE
    );
    rte_mempool* pool = rte_pktmbuf_pool_create(
        "TX_POOL", MBUF_COUNT, MBUF_CACHE, 0, data_room, rte_socket_id());
    if (!pool) {
        std::cerr << "Mempool create failed: " << rte_strerror(rte_errno) << "\n";
        rte_eal_cleanup();
        mainQueue.pushStatus(generator::ERROR);
        return;
    }

    // Port init
    if (!port_init(params.port_id, pool)) {
        rte_mempool_free(pool);
        rte_eal_cleanup();
        mainQueue.pushStatus(generator::ERROR);
        return;
    }

    // Burst preallocation
    std::vector<rte_mbuf*> burst(BURST_SIZE);
    for (auto& m : burst) {
        m = rte_pktmbuf_alloc(pool);
        if (!m) {
            std::cerr << "mbuf alloc failed during pre-alloc\n";
            rte_eal_cleanup();
            mainQueue.pushStatus(generator::ERROR);
            return;
        }
        uint8_t* data = (uint8_t*)rte_pktmbuf_append(m, params.pkt_size);
        if (!data) {
            std::cerr << "pktmbuf_append failed\n";
            rte_eal_cleanup();
            mainQueue.pushStatus(generator::ERROR);
            return;
        }
        fill_packet(data, params.pkt_size);
    }

    // 6. TX loop
    uint64_t total_sent = 0;


    // uint16_t port_id;
    // if (rte_eth_dev_get_port_by_name(m_params.interfaceName.c_str(), &port_id) != 0) {
    //     std::cerr << "DPDK: Failed to get port for device " << m_params.interfaceName << std::endl;
    //     emit sendError("DPDK: Failed to get port for device " + m_params.interfaceName);
    //     emit finished();
    //     return;
    // }
    // const uint16_t queue_id = 0;
    // const uint16_t nb_txd = 1024;
    // const uint32_t mbuf_size = m_params.packSize + RTE_PKTMBUF_HEADROOM;
    // const uint32_t nb_mbufs = 8192;
    //
    // // Create mempool
    // std::string pool_name = "MBUF_POOL_" + std::to_string(m_params.packSize);
    // rte_mempool* mbuf_pool = rte_mempool_lookup(pool_name.c_str());
    //
    // if (!mbuf_pool) {
    //     mbuf_pool = rte_pktmbuf_pool_create(
    //         pool_name.c_str(), nb_mbufs, 256, 0, mbuf_size, rte_socket_id());
    //
    //     if (!mbuf_pool) {
    //         std::cerr << "DPDK: Failed to create mbuf pool: " << rte_strerror(rte_errno) << std::endl;
    //         emit sendError("DPDK: Failed to create mbuf pool: " + std::string(rte_strerror(rte_errno)));
    //         emit finished();
    //         return;
    //     }
    // }
    //
    // // Configure port
    // rte_eth_conf port_conf = {};
    // port_conf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    // if (rte_eth_dev_configure(port_id, 0, 1, &port_conf) < 0) {
    //     std::cerr << "DPDK: Failed to configure port" << std::endl;
    //     emit sendError("DPDK: Failed to configure port " + std::to_string(port_id));
    //     emit finished();
    //     return;
    // }
    // if (rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), nullptr) < 0) {
    //     std::cerr << "DPDK: Failed to setup TX queue" << std::endl;
    //     emit sendError("DPDK: Failed to setup TX queue for port " + std::to_string(port_id));
    //     emit finished();
    //     return;
    // }
    // if (rte_eth_dev_start(port_id) < 0) {
    //     std::cerr << "DPDK: Failed to start port" << std::endl;
    //     emit sendError("DPDK: Failed to start port " + std::to_string(port_id));
    //     emit finished();
    //     return;
    // }
    //
    // std::vector<uint8_t> packet(m_params.packSize, m_params.packetPattern); // Dummy payload
    // std::vector<rte_mbuf*> burst(m_params.burstSize);
    //
    // for (uint16_t i = 0; i < m_params.burstSize; ++i) {
    //     rte_mbuf* mbuf = rte_pktmbuf_alloc(mbuf_pool);
    //     if (!mbuf) {
    //         std::cerr << "DPDK: Failed to allocate mbuf\n";
    //         emit sendWarning("DPDK: Failed to allocate mbuf");
    //         burst.resize(i); // Only send what we have
    //         break;
    //     }
    //     uint8_t* data = rte_pktmbuf_mtod(mbuf, uint8_t*);
    //     memcpy(data, packet.data(), m_params.packSize);
    //     mbuf->data_len = m_params.packSize;
    //     mbuf->pkt_len = m_params.packSize;
    //     burst[i] = mbuf;
    // }
    //
    // // Preset stop conditions
    // uint64_t totalCopies = 0, totalSend = 0;
    // struct timespec startTime{}, currTime{};
    //
    // // Preset speed variables
    // uint64_t bytesPerPacket = m_params.packSize + 20;
    // uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    // uint64_t nextTime, sleepNS;
    //
    // clock_gettime(CLOCK_MONOTONIC, &startTime);
    // emit sendProgress(0, 0, {.tv_sec=0});
    // nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    // while (isRunning) {
    //     {
    //         std::unique_lock lock(m_mutex);
    //         m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
    //     }
    //
    //     if(!isRunning)
    //         break;
    //
    //     // SEND
    //     uint16_t sent = rte_eth_tx_burst(port_id, queue_id, burst.data(), burst.size());
    //     if (sent < burst.size()) {
    //         std::cerr << "DPDK: Failed to send all packets. Sent: " << sent << std::endl;
    //         emit sendWarning("DPDK: Failed to send all packets. Sent: " + std::to_string(sent));
    //     }
    //     // END SEND
    //
    //     clock_gettime(CLOCK_MONOTONIC, &currTime);
    //     totalCopies += sent;
    //     totalSend += packet.size() * sent;
    //     emit sendProgress(totalSend, totalCopies, currTime);
    //     if(((currTime.tv_sec - startTime.tv_sec) >= m_params.time && m_params.time != 0)
    //         || (totalCopies == m_params.copies && m_params.copies != 0)
    //         || (totalSend == m_params.totalSend && m_params.totalSend != 0))
    //         break;
    //
    //     if(m_params.speed != 0) {
    //         nextTime += internalNS;
    //         if(nextTime < (currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec))
    //             sleepNS = 0;
    //         else
    //             sleepNS = nextTime - (currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec);
    //         if(sleepNS > 0) {
    //             struct timespec sleepTime = {
    //                 .tv_sec = sleepNS / 1'000'000'000ULL,
    //                 .tv_nsec = sleepNS % 1'000'000'000ULL
    //             };
    //             nanosleep(&sleepTime, nullptr);
    //         } else {
    //             nextTime = currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec;
    //         }
    //     }
    // }
    // // Free mbufs
    // for (uint16_t i = 0; i < burst.size(); ++i) {
    //     rte_pktmbuf_free(burst[i]);
    // }
    //
    // rte_eth_dev_stop(port_id);
    // emit finished();
} 

/**
 * @brief dpdkSendFile
 * @details Send file using DPDK
 * @note DPDK is not supported on all interfaces. Check interface support before using.
*/
void Generator::dpdkSendFile() {
    uint16_t port_id;
    if (rte_eth_dev_get_port_by_name(m_params.interfaceName.c_str(), &port_id) != 0) {
        std::cerr << "DPDK: Failed to get port for device " << m_params.interfaceName << std::endl;
        emit sendError("DPDK: Failed to get port for device " + m_params.interfaceName);
        emit finished();
        return;
    }
    const uint16_t queue_id = 0;
    const uint16_t nb_txd = 1024;
    const uint32_t mbuf_size = m_params.packSize + RTE_PKTMBUF_HEADROOM;
    const uint32_t nb_mbufs = 8192;

    // Create mempool
    std::string pool_name = "MBUF_POOL_" + std::to_string(m_params.packSize);
    rte_mempool* mbuf_pool = rte_mempool_lookup(pool_name.c_str());

    if (!mbuf_pool) {
        mbuf_pool = rte_pktmbuf_pool_create(
            pool_name.c_str(), nb_mbufs, 256, 0, mbuf_size, rte_socket_id());
        
        if (!mbuf_pool) {
            std::cerr << "DPDK: Failed to create mbuf pool: " << rte_strerror(rte_errno) << std::endl;
            emit sendError("DPDK: Failed to create mbuf pool: " + std::string(rte_strerror(rte_errno)));
            emit finished();
            return;
        }
    }

    // Configure port
    rte_eth_conf port_conf = {};
    port_conf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    if (rte_eth_dev_configure(port_id, 0, 1, &port_conf) < 0) {
        std::cerr << "DPDK: Failed to configure port" << std::endl;
        emit sendError("DPDK: Failed to configure port " + std::to_string(port_id));
        emit finished();
        return;
    }
    if (rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), nullptr) < 0) {
        std::cerr << "DPDK: Failed to setup TX queue" << std::endl;
        emit sendError("DPDK: Failed to setup TX queue for port " + std::to_string(port_id));
        emit finished();
        return;
    }
    if (rte_eth_dev_start(port_id) < 0) {
        std::cerr << "DPDK: Failed to start port" << std::endl;
        emit sendError("DPDK: Failed to start port " + std::to_string(port_id));
        emit finished();
        return;
    }

    // Preset packet
    uint64_t offset = 0;
    std::vector<rte_mbuf*> burst(m_params.burstSize);

    std::ifstream file(m_params.filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << m_params.filePath << std::endl;
        rte_eth_dev_stop(port_id);
        emit sendError("Failed to open file: " + m_params.filePath);
        emit finished();
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> fileData(fileSize);
    if (!file.read((fileData.data()), fileSize)) {
        std::cerr << "Failed to read file." << std::endl;
        rte_eth_dev_stop(port_id);
        emit sendError("Failed to read file: " + m_params.filePath);
        emit finished();
        return;
    }
    std::vector<uint8_t> packet(fileData.begin(), fileData.end());
    if(fileSize % m_params.packSize != 0) {
        fileSize += m_params.packSize - (fileSize % m_params.packSize);
    }
    packet.resize(fileSize, 0x00);

    for (uint16_t i = 0; i < m_params.burstSize; ++i) {
        rte_mbuf* mbuf = rte_pktmbuf_alloc(mbuf_pool);
        if (!mbuf) {
            std::cerr << "DPDK: Failed to allocate mbuf\n";
            emit sendWarning("DPDK: Failed to allocate mbuf");
            burst.resize(i); // Only send what we have
            break;
        }
        uint8_t* data = rte_pktmbuf_mtod(mbuf, uint8_t*);
        memcpy(data, packet.data() + offset, m_params.packSize);
        mbuf->data_len = m_params.packSize;
        mbuf->pkt_len = m_params.packSize;
        burst[i] = mbuf;
        if((offset + m_params.packSize) >= fileSize - 1) {
            offset = 0;
        } else {
            offset += m_params.packSize;
        }
    }
    
    // Preset stop conditions
    uint64_t totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    emit sendProgress(0, 0, {.tv_sec=0});
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // SEND
        uint16_t sent = rte_eth_tx_burst(port_id, queue_id, burst.data(), burst.size());
        if (sent < burst.size()) {
            std::cerr << "DPDK: Failed to send all packets. Sent: " << sent << std::endl;
            emit sendWarning("DPDK: Failed to send all packets. Sent: " + std::to_string(sent));
        }
        // END SEND

        clock_gettime(CLOCK_MONOTONIC, &currTime);
        totalCopies += sent;
        totalSend += packet.size() * sent;
        emit sendProgress(totalSend, totalCopies, currTime);
        if(((currTime.tv_sec - startTime.tv_sec) >= m_params.time && m_params.time != 0)  
            || (totalCopies == m_params.copies && m_params.copies != 0) 
            || (totalSend == m_params.totalSend && m_params.totalSend != 0))
            break;
        
        if(m_params.speed != 0) {
            nextTime += internalNS;
            if(nextTime < (currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec))
                sleepNS = 0;
            else
                sleepNS = nextTime - (currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec);
            if(sleepNS > 0) {
                struct timespec sleepTime = {
                    .tv_sec = sleepNS / 1'000'000'000ULL,
                    .tv_nsec = sleepNS % 1'000'000'000ULL
                };
                nanosleep(&sleepTime, nullptr);
            } else {
                nextTime = currTime.tv_sec * 1'000'000'000ULL + currTime.tv_nsec;
            }
        }
    }
    // Free mbufs
    for (uint16_t i = 0; i < burst.size(); ++i) {
        rte_pktmbuf_free(burst[i]);
    }
    
    rte_eth_dev_stop(port_id);
    emit finished();
}