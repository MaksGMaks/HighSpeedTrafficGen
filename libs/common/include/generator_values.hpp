#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/ioctl.h>

// Linux network
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>

struct interfaceModes {
    std::string interfaceName{};
    bool dpdk_support{};
    bool pf_ring_zc_support{};
    bool pf_ring_standart_support{};

    interfaceModes() = default;
    interfaceModes(const std::string interfaceName_) {
        interfaceName = interfaceName_;
    }

    uint8_t packValue() {
        return (dpdk_support << 2) | (pf_ring_zc_support << 1) | pf_ring_standart_support;
    }
};

struct genParams {
    std::string interfaceName{};        // Name of interface for generation
    int mode{};                     // Method, which interface use for generation (PF_RING, PF_RING ZC of DPDK)
    uint time{};                        // Time for sending; if 0 - infinite sending
    uint64_t speed{};                   // Speed of sending; if 0 - unlimited speed
    uint packSize{};                    // Size of package for sending
    bool fileSend{};                    // Is file sending
    std::string filePath{};             // Path for file if sending
    uint64_t copies{};                  // Copies of file or packets for sending; if 0 - infinite sending
    uint64_t totalSend{};               // Total size of generated information; if 0 - infinite sending
    int burstSize{};                // Size of burst for sending
    int packetPattern{};        // Pattern for packet generation
};


namespace jsonHeaders {
    enum TypeC {
        ACCEPT,
        FAILED,
        REQUEST,
        RESPONSE,
        MESSAGE
    };

    enum CommandC {
        START,
        PAUSE,
        RESUME,
        FINISH
    };

    const std::string Command = "COMMAND";
    const std::string Type = "TYPE";

    const std::string Port = "port";
    const std::string InterfaceName = "interfaceName";
    const std::string DpdkSupported = "dpdkSupported";
    const std::string PfRingSupported = "pf_ringSupported";
    const std::string PfRingZcSupported = "pf_ring_zcSupported";

    namespace Parameters {
        const std::string Mode = "MODE";
        const std::string Time = "TIME";
        const std::string Speed = "SPEED";
        const std::string PackSize = "PACK_SIZE";
        const std::string BurstSize = "BURST_SIZE";
        const std::string PacketPattern = "PACK_PATTERN";
        const std::string FileSend = "FILE_SEND";
        const std::string Copies = "COPIES";
        const std::string TotalSend = "TOTAL_SEND";
    }

    namespace Data {
        const std::string Data = "DATA";
        const std::string Pps = "pps";
        const std::string Bps = "bps";
        const std::string BPS = "BPS";
        const std::string Time = "time";
        const std::string TotalSend = "totalSend";
        const std::string TotalCopies = "totalCopies";
    }

    struct receivedData {
        uint64_t pps{};
        uint64_t bps{};
        uint64_t BPS{};
        int64_t  time{};
        uint64_t totalCopies{};
        uint64_t totalSend{};

    };
}

namespace networkAttribute {
  const uint16_t PORT_NUM = 54000;
}
