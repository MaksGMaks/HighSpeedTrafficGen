#include <iostream>

#include "Generator/Generator.hpp"

int main(int argc, char **argv) {
    std::vector<interfaceModes> ifModes = findAllDevices();
    initialize_dpdk(ifModes);
    for(auto dev : ifModes) {
        dev.pf_ring_zc_support = check_pfring_zc(dev.interfaceName);
        dev.pf_ring_standart_support = check_pfring_standard(dev.interfaceName);
    }
    for(auto dev : ifModes) {
        std::cout << "Device: " << dev.interfaceName << "\tSupport: " << ((dev.dpdk_support) ? "DPDK\t" : "NO DPDK\t") 
                                << ((dev.pf_ring_zc_support) ? "PF_ZC\t" : "NO PF_ZC\t") << ((dev.pf_ring_standart_support) ? "PF" : "NO PF") << std::endl;
    }
    
    return 0;
}