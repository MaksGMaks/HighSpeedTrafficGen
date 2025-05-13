#pragma once
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <iomanip>
#include <ctime>

// Linux network
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

// DPDK
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ethdev.h>

// PF_RING
#include <pfring.h>
#include <pfring_zc.h>

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

bool check_pfring_standard(const std::string& ifname);
bool check_pfring_zc(const std::string& ifname);
void initialize_dpdk(std::vector<interfaceModes>& interfaces);
std::vector<interfaceModes> findAllDevices();

bool isHugepageMounted(const std::string& mountPoint);

/// Testing

void print_mac(const char mac[6]);
void list_detailed_pfring_interfaces();