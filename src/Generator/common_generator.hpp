#pragma once
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>

// Linux network
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

struct interfaceModes {
    std::string interfaceName{};
    bool dpdk_support{};
    bool pf_ring_zc_support{};
    bool pf_ring_standart_support{};

    interfaceModes(const std::string interfaceName_) {
        interfaceName = interfaceName_;
    }

    uint8_t packValue() {
        return (dpdk_support << 2) | (pf_ring_zc_support << 1) | pf_ring_standart_support;
    }
};

struct genParams {
    std::string interfaceName{};    // Name of interface for generation
    uint8_t mode{};                 // Method, which interface use for generation (PF_RING, PF_RING ZC of DPDK)
    uint time{};                    // Time for sending; if 0 - infinite sending
    uint64_t speed{};               // Speed of sending; if 0 - unlimited speed
    uint packSize{};                // Size of package for sending
    bool fileSend{};                // Is file sending
    std::string filePath{};         // Path for file if sending
    uint copies{};                  // Copies of file or packets for sending; if 0 - infinite sending
    uint totalSend{};               // Total size of generated information; if 0 - infinite sending
};

std::vector<interfaceModes> findAllDevices();
