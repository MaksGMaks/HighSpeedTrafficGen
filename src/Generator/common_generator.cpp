#include "common_generator.hpp"

std::vector<interfaceModes> findAllDevices() {
    struct ifaddrs *interfaces = nullptr, *ifa = nullptr;
    void *addr_ptr = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs");
        exit(-1);
    }
    
    std::set<std::string> seen;
    std::vector<interfaceModes> ifModes;

    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name && seen.insert(ifa->ifa_name).second) {
            ifModes.push_back(interfaceModes(ifa->ifa_name));
        }
    }
    freeifaddrs(interfaces);
    return ifModes;
}

int get_interface_mtu(const std::string& ifname) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFMTU, &ifr) < 0) {
        close(sock);
        return -1;
    }
    close(sock);
    return ifr.ifr_mtu;
}