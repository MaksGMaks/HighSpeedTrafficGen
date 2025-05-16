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
    std::vector<uint8_t> packet(m_params.packSize, 0xAA); // Fill with dummy data
    
    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        pfringGenerate(ring, packet.data(), packet.size());
        clock_gettime(CLOCK_MONOTONIC, &currTime);
        ++totalCopies;
        totalSend += packet.size();
        if(((currTime.tv_sec - startTime.tv_sec) > m_params.time && m_params.time != 0) 
            || totalCopies == m_params.copies || totalSend == m_params.totalSend )
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
    for(auto ch : packet) {
        std::cout << ch;
    }
    std::cout << std::endl;
    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0, offset = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec; 
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        pfringGenerate(ring, packet.data() + offset, m_params.packSize);
        clock_gettime(CLOCK_MONOTONIC, &currTime);
        totalSend += packet.size();
        if((offset + m_params.packSize) >= fileSize - 1) {
            offset = 0;
            ++totalCopies;
        } else {
            offset += m_params.packSize;
        }

        if(((currTime.tv_sec - startTime.tv_sec) > m_params.time && m_params.time != 0) 
            || (totalCopies == m_params.copies && m_params.copies != 0) || totalSend == m_params.totalSend )
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
 * Create a new cluster. 
 * @param cluster_id           The unique cluster identifier.
 * @param buffer_len           The size of each buffer: it must be at least as large as the MTU + L2 header (it will be rounded up to cache line) and not bigger than the page size.
 * @param metadata_len         The size of each buffer metadata.
 * @param tot_num_buffers      The total number of buffers to reserve for queues/devices/extra allocations.
 * @param numa_node_id         The NUMA node id for cpu/memory binding.
 * @param hugepages_mountpoint The HugeTLB mountpoint (NULL for auto-detection) for memory allocation.
 * @param flags                Optional flags:
 *                             @code
 *                             PF_RING_ZC_ENABLE_VM_SUPPORT enable KVM support (memory is rounded up to power of 2 thus it allocates more memory!)
 *                             @endcode
 * @return                     The cluster handle on success, NULL otherwise (errno is set appropriately).
 */
void Generator::pfringZCSend() {
    pfring_zc_cluster* cluster = pfring_zc_create_cluster(0, 2048, 0, 2048, 0, NULL, 0);
    if (!cluster) {
        std::cerr << "Failed to create cluster\n";
        return;
    }

    pfring_zc_queue* tx_queue = pfring_zc_open_device(cluster, m_params.interfaceName.c_str(), tx_only, 0);
    if (!tx_queue) {
        std::cerr << "Failed to open device\n";
        pfring_zc_destroy_cluster(cluster);
        return;
    }

    pfring_zc_pkt_buff* pkt = pfring_zc_get_packet_handle(cluster);
    if (!pkt) {
        std::cerr << "Failed to get packet handle\n";
        pfring_zc_close_device(tx_queue);
        pfring_zc_destroy_cluster(cluster);
        return;
    }

    // Preset packet
    std::vector<uint8_t> packet(m_params.packSize, 0xAA); // Fill with dummy data
    
    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = m_params.packSize + 20;
    uint64_t internalNS = 1e9 * bytesPerPacket / m_params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // process();

        clock_gettime(CLOCK_MONOTONIC, &currTime);
        ++totalCopies;
        totalSend += packet.size();
        if(((currTime.tv_sec - startTime.tv_sec) > m_params.time && m_params.time != 0) 
            || totalCopies == m_params.copies || totalSend == m_params.totalSend )
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
}

void Generator::pfringZCSendFile() {

}

void Generator::dpdkSend() {
    while (isRunning) {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // process();
    }
} 

void Generator::dpdkSendFile() {

}