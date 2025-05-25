#include "Generator.hpp"

Generator::Generator(QObject *parent)
: QObject(parent) {
    isRunning = false;
    isPaused = false;
    connect(this, &Generator::finished, this, &Generator::doStop);
}

Generator::~Generator() {
    std::cout << "[Generator::~Generator] stop thread and destroy object" << std::endl;
    doStop();
}

void Generator::doStart(const genParams& params) {
    std::cout << "[Generator::doStart] start thread" << std::endl;
    isRunning = true;
    isPaused = false;
    m_params = params;
    switch (params.mode) {
    case 1:
        if(params.fileSend) m_workerThread = std::thread(&Generator::pfringSendFile, this);
        else m_workerThread = std::thread(&Generator::pfringSend, this);
        break;
    case 2:
        if(params.fileSend) m_workerThread = std::thread(&Generator::pfringZCSendFile, this);
        else m_workerThread = std::thread(&Generator::pfringZCSend, this);
        break;
    case 4:
        if(params.fileSend) m_workerThread = std::thread(&Generator::dpdkSendFile, this);
        else m_workerThread = std::thread(&Generator::dpdkSend, this);
        break;
    default:
        break;
    }
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

/**
 * @brief pfringSend
 * @details Send packets using PF_RING
 * @note PF_RING is not supported on all interfaces. Check interface support before using.
*/
void Generator::pfringSend() {
    // Preset pf_ring
    pfring* ring = pfring_open(m_params.interfaceName.c_str(), 1500 /* snaplen */, PF_RING_PROMISC);
    if (!ring) {
        std::cerr << "Error opening PF_RING on interface: " << m_params.interfaceName << std::endl;
        return;
    }

    pfring_set_direction(ring, tx_only_direction); // enable TX
    pfring_set_application_name(ring, "TrafficGenerator");

    if (pfring_enable_ring(ring) != 0) {
        std::cerr << "Failed to enable PF_RING." << std::endl;
        pfring_close(ring);
        return;
    }
    // Preset packet
    std::vector<uint8_t> packet(m_params.packSize, m_params.packetPattern); // Fill with dummy data
    
    // Preset stop conditions
    uint64_t totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    emit sendHalfProgress(0, {.tv_sec=0}, m_params.interfaceName);;
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // SEND
        int rc = pfring_send(ring, (char*)packet.data(), packet.size(), 0 /* flush = false */);
        if (rc < 0) {
            std::cerr << "PF_RING send error: " << rc << " (errno=" << errno << ": " << strerror(errno) << ")\n";
        } else {
            totalSend += rc;
        }
        // END SEND

        clock_gettime(CLOCK_MONOTONIC, &currTime);;
        if(rc = packet.size()) {
            totalCopies++;
        }
        emit sendHalfProgress(totalCopies, currTime, m_params.interfaceName);
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
    pfring_close(ring);
    emit finished();
}

/**
 * @brief pfringSendFile
 * @details Send file using PF_RING
 * @note PF_RING is not supported on all interfaces. Check interface support before using.
*/
void Generator::pfringSendFile() {
    // Preset pf_ring
    pfring* ring = pfring_open(m_params.interfaceName.c_str(), 1500 /* snaplen */, PF_RING_PROMISC);
    if (!ring) {
        std::cerr << "Error opening PF_RING on interface: " << m_params.interfaceName << std::endl;
        return;
    }

    pfring_set_direction(ring, tx_only_direction); // enable TX
    pfring_set_application_name(ring, "TrafficGenerator");

    if (pfring_enable_ring(ring) != 0) {
        std::cerr << "Failed to enable PF_RING." << std::endl;
        pfring_close(ring);
        return;
    }
    // Preset packet
    std::ifstream file(m_params.filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << m_params.filePath << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> fileData(fileSize);
    if (!file.read((fileData.data()), fileSize)) {
        std::cerr << "Failed to read file." << std::endl;
        return;
    }
    std::vector<uint8_t> packet(fileData.begin(), fileData.end());
    if(fileSize % m_params.packSize != 0) {
        fileSize += m_params.packSize - (fileSize % m_params.packSize);
    }
    packet.resize(fileSize, 0x00);
    
    // Preset stop conditions
    uint64_t totalCopies = 0, totalSend = 0, offset = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    emit sendHalfProgress(0, {.tv_sec=0}, m_params.interfaceName);;
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec; 
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // SEND
        int rc = pfring_send(ring, (char*)(packet.data() + offset), m_params.packSize, 0 /* flush = false */);
        if (rc < 0) {
            std::cerr << "PF_RING send error: " << rc << " (errno=" << errno << ": " << strerror(errno) << ")\n";
        } else {
            totalSend += rc;
        }
        // END SEND

        clock_gettime(CLOCK_MONOTONIC, &currTime);
        if((offset + m_params.packSize) >= fileSize - 1) {
            offset = 0;
        } else {
            offset += m_params.packSize;
        }
        totalCopies += rc;
        totalSend += packet.size() * rc;

        emit sendHalfProgress(totalCopies, currTime, m_params.interfaceName);
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
    pfring_close(ring);
    emit finished();
}

/**
 * @brief pfringZCSend
 * @details Send packets using PF_RING ZC
 * @note PF_RING ZC is not supported on all interfaces. Check interface support before using.
*/
void Generator::pfringZCSend() {
    int mtu = get_interface_mtu(m_params.interfaceName);
    if(m_params.packSize > mtu) {
        std::cerr << "Packet size is larger than MTU" << std::endl;
        emit finished();
        return;
    }
    std::cout << "PF_ZC send" << std::endl;
    // Create cluster
    pfring_zc_cluster* cluster = pfring_zc_create_cluster(
        0, mtu + 64, 0, 16384, -1, nullptr, 0);
    if (!cluster) {
        std::cerr << "Failed to create ZC cluster\n";
        emit finished();
        return;
    }

    // Open TX queue
    pfring_zc_queue* tx_queue = pfring_zc_open_device(
        cluster, m_params.interfaceName.c_str(), tx_only, 0);
    if (!tx_queue) {
        std::cerr << "Failed to open TX device\n";
        pfring_zc_destroy_cluster(cluster);
        emit finished();
        return;
    }

    // Allocate burst of packet buffers
    std::vector<pfring_zc_pkt_buff*> burst(m_params.burstSize);
    std::vector<uint8_t> packet(m_params.packSize, m_params.packetPattern);  // Dummy payload

    for (size_t i = 0; i < m_params.burstSize; ++i) {
        burst[i] = pfring_zc_get_packet_handle(cluster);
        if (!burst[i]) {
            std::cerr << "Failed to allocate packet handle\n";
            emit finished();
            return;
        }
        memcpy(pfring_zc_pkt_buff_data(burst[i], tx_queue), packet.data(), m_params.packSize);
        burst[i]->len = m_params.packSize;
    }

    // Preset stop conditions
    uint64_t totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    emit sendHalfProgress(0, {.tv_sec=0}, m_params.interfaceName);;
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // SEND
        int sent = pfring_zc_send_pkt_burst(tx_queue, burst.data(), burst.size(), 0); // 0 = do NOT auto free
        if (sent <= 0) {
            std::cerr << "PF_RING ZC send error: " << sent
                    << " (errno=" << errno << ": " << strerror(errno) << ")\n";
        }
        // END SEND

        clock_gettime(CLOCK_MONOTONIC, &currTime);
        totalCopies += sent;
        totalSend += packet.size() * sent;
        emit sendHalfProgress(totalCopies, currTime, m_params.interfaceName);
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
    // Clean up
    for (auto pkt : burst)
        pfring_zc_release_packet_handle(cluster, pkt);

    pfring_zc_close_device(tx_queue);
    pfring_zc_destroy_cluster(cluster);
    emit finished();
}

/**
 * @brief pfringZCSendFile
 * @details Send file using PF_RING ZC
 * @note PF_RING ZC is not supported on all interfaces. Check interface support before using.
*/
void Generator::pfringZCSendFile() {
    int mtu = get_interface_mtu(m_params.interfaceName);
    if(m_params.packSize > mtu) {
        std::cerr << "Packet size is larger than MTU" << std::endl;
        emit finished();
        return;
    }
    std::cout << "PF_ZC send" << std::endl;
    // Create cluster
    pfring_zc_cluster* cluster = pfring_zc_create_cluster(
        0, mtu + 64, 0, 16384, -1, nullptr, 0);
    if (!cluster) {
        std::cerr << "Failed to create ZC cluster\n";
        emit finished();
        return;
    }

    // Open TX queue
    pfring_zc_queue* tx_queue = pfring_zc_open_device(
        cluster, m_params.interfaceName.c_str(), tx_only, 0);
    if (!tx_queue) {
        std::cerr << "Failed to open TX device\n";
        pfring_zc_destroy_cluster(cluster);
        emit finished();
        return;
    }

    // Allocate burst of packet buffers
    uint64_t offset = 0;
    std::vector<pfring_zc_pkt_buff*> burst(m_params.burstSize);

    std::ifstream file(m_params.filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << m_params.filePath << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> fileData(fileSize);
    if (!file.read((fileData.data()), fileSize)) {
        std::cerr << "Failed to read file." << std::endl;
        return;
    }
    std::vector<uint8_t> packet(fileData.begin(), fileData.end());
    if(fileSize % m_params.packSize != 0) {
        fileSize += m_params.packSize - (fileSize % m_params.packSize);
    }
    packet.resize(fileSize, 0x00);
    

    for (size_t i = 0; i < m_params.burstSize; ++i) {
        burst[i] = pfring_zc_get_packet_handle(cluster);
        if (!burst[i]) {
            std::cerr << "Failed to allocate packet handle\n";
            emit finished();
            return;
        }
        memcpy(pfring_zc_pkt_buff_data(burst[i], tx_queue), packet.data() + offset, m_params.packSize);
        burst[i]->len = m_params.packSize;
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
    emit sendHalfProgress(0, {.tv_sec=0}, m_params.interfaceName);
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // SEND
        int sent = pfring_zc_send_pkt_burst(tx_queue, burst.data(), burst.size(), 0); // 0 = do NOT auto free
        if (sent <= 0) {
            std::cerr << "PF_RING ZC send error: " << sent
                    << " (errno=" << errno << ": " << strerror(errno) << ")\n";
        }
        // END SEND

        clock_gettime(CLOCK_MONOTONIC, &currTime);
        totalCopies += sent;
        totalSend += packet.size() * sent;
        emit sendHalfProgress(totalCopies, currTime, m_params.interfaceName);
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
    // Clean up
    for (auto pkt : burst)
        pfring_zc_release_packet_handle(cluster, pkt);

    pfring_zc_close_device(tx_queue);
    pfring_zc_destroy_cluster(cluster);
    emit finished();
}

/**
 * @brief dpdkSend
 * @details Send packets using DPDK
 * @note DPDK is not supported on all interfaces. Check interface support before using.
*/
void Generator::dpdkSend() {
    uint16_t port_id;
    if (rte_eth_dev_get_port_by_name(m_params.interfaceName.c_str(), &port_id) != 0) {
        std::cerr << "DPDK: Failed to get port for device " << m_params.interfaceName << std::endl;
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
            emit finished();
            return;
        }
    }

    // Configure port
    rte_eth_conf port_conf = {};
    port_conf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    if (rte_eth_dev_configure(port_id, 0, 1, &port_conf) < 0) {
        std::cerr << "DPDK: Failed to configure port" << std::endl;
        emit finished();
        return;
    }
    if (rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), nullptr) < 0) {
        std::cerr << "DPDK: Failed to setup TX queue" << std::endl;
        emit finished();
        return;
    }
    if (rte_eth_dev_start(port_id) < 0) {
        std::cerr << "DPDK: Failed to start port" << std::endl;
        emit finished();
        return;
    }

    std::vector<uint8_t> packet(m_params.packSize, m_params.packetPattern); // Dummy payload
    std::vector<rte_mbuf*> burst(m_params.burstSize);

    for (uint16_t i = 0; i < m_params.burstSize; ++i) {
        rte_mbuf* mbuf = rte_pktmbuf_alloc(mbuf_pool);
        if (!mbuf) {
            std::cerr << "DPDK: Failed to allocate mbuf\n";
            burst.resize(i); // Only send what we have
            break;
        }
        uint8_t* data = rte_pktmbuf_mtod(mbuf, uint8_t*);
        memcpy(data, packet.data(), m_params.packSize);
        mbuf->data_len = m_params.packSize;
        mbuf->pkt_len = m_params.packSize;
        burst[i] = mbuf;
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

/**
 * @brief dpdkSendFile
 * @details Send file using DPDK
 * @note DPDK is not supported on all interfaces. Check interface support before using.
*/
void Generator::dpdkSendFile() {
    uint16_t port_id;
    if (rte_eth_dev_get_port_by_name(m_params.interfaceName.c_str(), &port_id) != 0) {
        std::cerr << "DPDK: Failed to get port for device " << m_params.interfaceName << std::endl;
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
            emit finished();
            return;
        }
    }

    // Configure port
    rte_eth_conf port_conf = {};
    port_conf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    if (rte_eth_dev_configure(port_id, 0, 1, &port_conf) < 0) {
        std::cerr << "DPDK: Failed to configure port" << std::endl;
        emit finished();
        return;
    }
    if (rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), nullptr) < 0) {
        std::cerr << "DPDK: Failed to setup TX queue" << std::endl;
        emit finished();
        return;
    }
    if (rte_eth_dev_start(port_id) < 0) {
        std::cerr << "DPDK: Failed to start port" << std::endl;
        emit finished();
        return;
    }

    // Preset packet
    uint64_t offset = 0;
    std::vector<rte_mbuf*> burst(m_params.burstSize);

    std::ifstream file(m_params.filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << m_params.filePath << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> fileData(fileSize);
    if (!file.read((fileData.data()), fileSize)) {
        std::cerr << "Failed to read file." << std::endl;
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