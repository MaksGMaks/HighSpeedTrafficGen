#include "Generator.hpp"

bool check_pfring_standard(const std::string& ifname) {
    pfring* ring = pfring_open(ifname.c_str(), 1500, PF_RING_PROMISC);
    if (ring == nullptr) {
        std::cerr << "pfring_open failed on " << ifname << ": " << strerror(errno) << std::endl;
        return false;
    }

    if (pfring_enable_ring(ring) != 0) {
        std::cerr << "pfring_enable_ring failed on " << ifname << std::endl;
        pfring_close(ring);
        return false;
    }

    pfring_close(ring);
    return true;
}

bool check_pfring_zc(const std::string& ifname) {
    pfring_zc_cluster* cluster = pfring_zc_create_cluster(
        0,            // cluster_id: 0 = auto
        2048,         // buffer_len
        0,            // metadata_len
        4096,         // tot_num_buffers
        -1,           // numa_node_id: any NUMA node
        NULL,         // hugepages_mountpoint: default
        0             // flags
    );

    if (cluster == nullptr) {
        std::cerr << "Failed to create ZC cluster" << std::endl;
        return false;
    }

    pfring_zc_queue* queue = pfring_zc_open_device(cluster, ifname.c_str(), rx_only, 0);
    if (queue == nullptr) {
        std::cerr << "Failed to open ZC device on " << ifname << std::endl;
        pfring_zc_destroy_cluster(cluster);
        return false;
    }

    pfring_zc_close_device(queue);
    pfring_zc_destroy_cluster(cluster);

    return true;
}

void initialize_dpdk(std::vector<interfaceModes>& interfaces) {
    std::vector<char*> argv = {"detect", "-c", "2", "--huge-dir=/mnt/huge"};
    //const char* argv[] = {"detect", "-c", "2", "--huge-dir=/mnt/huge"};

    if(!isHugepageMounted("/mnt/huge")) {
        std::cerr << "Hugepage not mounted. Run \nmount -t hugetlbfs nodev /mnt/huge \nmanualy with root previlage (mount hugepages)."
                     "Make sure it reserves 2MB for dpdk. If not - run \necho 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" << std::endl;
    }

    int argc = 4;

    if (rte_eal_init(argc, const_cast<char**>(argv.data())) < 0) {
        std::cerr << "Failed to initialize DPDK EAL\n";
        return;
    }

    int nb_ports = rte_eth_dev_count_avail();
    for (int i = 0; i < nb_ports; i++) {
        rte_eth_dev_info dev_info;
        rte_eth_dev_info_get(i, &dev_info);
        if (dev_info.device) {
            std::string ifname = rte_dev_name(dev_info.device);
            auto it = std::find_if(interfaces.begin(), interfaces.end(), [&ifname](auto& i) {
                return i.interfaceName == ifname;
            });

            if (it != interfaces.end()) {
                it->dpdk_support = true;
            } else {
                interfaceModes intr(ifname);
                intr.dpdk_support = true;
                interfaces.push_back(intr);
            }
        }
    }
}

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

bool isHugepageMounted(const std::string& mountPoint) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(mountPoint) != std::string::npos && line.find("hugetlbfs") != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// Testing


void print_mac(const char mac[6]) {
    std::ios_base::fmtflags f(std::cout.flags());
    std::cout << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(mac[i]));
        if (i < 5) std::cout << ":";
    }
    std::cout.flags(f); // restore original flags
}

void list_detailed_pfring_interfaces() {
    pfring_if_t* dev_list = pfring_findalldevs();
    if (!dev_list) {
        std::cerr << "Error: Failed to get PF_RING interfaces.\n";
        return;
    }

    std::cout << "PF_RING Interfaces:\n";
    for (pfring_if_t* dev = dev_list; dev; dev = dev->next) {
        std::cout << "Interface: " << (dev->name ? dev->name : "N/A") << "\n";
        std::cout << "  System Name:     " << (dev->system_name ? dev->system_name : "N/A") << "\n";
        std::cout << "  Module:          " << (dev->module ? dev->module : "N/A")
                  << " (version: " << (dev->module_version ? dev->module_version : "N/A") << ")\n";
        std::cout << "  Serial Number:   " << (dev->sn ? dev->sn : "N/A") << "\n";
        std::cout << "  MAC Address:     "; print_mac(dev->mac); std::cout << "\n";
        std::cout << "  Bus ID:          "
                  << std::hex << std::setw(4) << std::setfill('0') << dev->bus_id.slot << ":"
                  << std::setw(2) << dev->bus_id.bus << ":"
                  << std::setw(2) << dev->bus_id.device << "."
                  << dev->bus_id.function << std::dec << "\n";
        std::cout << "  Interface Index: " << dev->ifindex << "\n";

        std::cout << "  Status:          ";
        switch (dev->status) {
            case 1: std::cout << "UP\n"; break;
            case 0: std::cout << "DOWN\n"; break;
            default: std::cout << "UNKNOWN\n"; break;
        }

        std::cout << "  License:         " << (dev->license == 1 ? "Valid" : "Invalid/Not installed") << "\n";
        if (dev->license == 1) {
            std::cout << "  License Expiry:  " << std::ctime(&dev->license_expiration);
        }

        std::cout << "--------------------------------------------------\n";
    }

    pfring_freealldevs(dev_list);
}