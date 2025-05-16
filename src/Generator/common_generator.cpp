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