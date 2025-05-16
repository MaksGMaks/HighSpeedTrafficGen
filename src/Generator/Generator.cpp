#include "Generator.hpp"

Generator::Generator(QObject *parent)
: QObject(parent) {
    isRunning = false;
    isPaused = false;
}

Generator::~Generator() {
    doStop();
}

void Generator::doStart(const genParams& params) {
    isRunning = true;
    switch (params.mode) {
    case 1:
        if(params.fileSend) m_workerThread = std::thread(&Generator::pfringSendFile, this, std::cref(params));
        else m_workerThread = std::thread(&Generator::pfringSend, this, std::cref(params));
        break;
    case 2:
        if(params.fileSend) m_workerThread = std::thread(&Generator::pfringZCSendFile, this, std::cref(params));
        else m_workerThread = std::thread(&Generator::pfringZCSend, this, std::cref(params));
        break;
    case 4:
        if(params.fileSend) m_workerThread = std::thread(&Generator::dpdkSendFile, this, std::cref(params));
        else m_workerThread = std::thread(&Generator::dpdkSend, this, std::cref(params));
        break;
    default:
        break;
    }
}

void Generator::doStop() {
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

void Generator::pfringSend(const genParams& params) {
    // Preset pf_ring
    pfring* ring = pfring_open(params.interfaceName.c_str(), 1500 /* snaplen */, PF_RING_PROMISC);
    if (!ring) {
        std::cerr << "Error opening PF_RING on interface: " << params.interfaceName << std::endl;
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
    std::vector<uint8_t> packet(params.packSize, 0xAA); // Fill with dummy data
    
    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = params.packSize + 20;
    uint64_t internalNS = 1'000'000'000ULL * bytesPerPacket / params.speed;
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
        if((currTime.tv_sec - startTime.tv_sec) == params.time 
            || totalCopies == params.copies || totalSend == params.totalSend )
            break;
        
        if(params.speed != 0) {
            nextTime += internalNS;
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
}

void Generator::pfringSendFile(const genParams& params) {
    // Preset pf_ring
    pfring* ring = pfring_open(params.interfaceName.c_str(), 1500 /* snaplen */, PF_RING_PROMISC);
    if (!ring) {
        std::cerr << "Error opening PF_RING on interface: " << params.interfaceName << std::endl;
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
    std::ifstream file(params.filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << params.filePath << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    if(fileSize % params.packSize != 0) {
        fileSize += params.packSize - (fileSize % params.packSize);
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> packet(fileSize, 0x00); 
    if (!file.read(reinterpret_cast<char*>(packet.data()), fileSize)) {
        std::cerr << "Failed to read file." << std::endl;
        return;
    }

    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0, offset = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = params.packSize + 20;
    uint64_t internalNS = 1'000'000'000ULL * bytesPerPacket / params.speed;
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

        pfringGenerate(ring, packet.data() + offset, params.packSize);
        clock_gettime(CLOCK_MONOTONIC, &currTime);
        totalSend += packet.size();
        if((offset + params.packSize) >= fileSize - 1) {
            offset = 0;
            ++totalCopies;
        } else {
            offset += params.packSize;
        }
        if((currTime.tv_sec - startTime.tv_sec) == params.time 
            || (totalCopies == params.copies && params.copies != 0) || totalSend == params.totalSend )
            break;
        
        if(params.speed != 0) {
            nextTime += internalNS;
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
}

void Generator::pfringZCSend(const genParams& params) {

    // Preset packet
    std::vector<uint8_t> packet(params.packSize, 0xAA); // Fill with dummy data
    
    // Preset stop conditions
    uint totalCopies = 0, totalSend = 0;
    struct timespec startTime{}, currTime{};
    
    // Preset speed variables
    uint64_t bytesPerPacket = params.packSize + 20;
    uint64_t internalNS = 1'000'000'000ULL * bytesPerPacket / params.speed;
    uint64_t nextTime, sleepNS;

    clock_gettime(CLOCK_MONOTONIC, &startTime);
    nextTime = startTime.tv_sec * 1'000'000'000ULL + startTime.tv_nsec;
    while (isRunning)
    {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // process();
    }
}

void Generator::pfringZCSendFile(const genParams& params) {

}

void Generator::dpdkSend(const genParams& params) {
    while (isRunning)
    {
        {
            std::unique_lock lock(m_mutex);
            m_pause.wait(lock, [this](){ return !isPaused || !isRunning; });
        }

        if(!isRunning)
            break;

        // process();
    }
} 

void Generator::dpdkSendFile(const genParams& params) {

}