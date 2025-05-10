#include "Generator.hpp"

bool check_pfring_standard(const std::string& ifname) {
    pfring* ring = pfring_open(ifname.c_str(), 1500, PF_RING_PROMISC);
    if (ring) {
        pfring_close(ring);
        return true;
    }
    return false;
}

bool check_pfring_zc(const std::string& ifname) {
    pfring_zc_cluster *cluster = pfring_zc_create_cluster(
        0,            // cluster_id: 0 = auto-assign
        2048,         // buffer_len (bytes): e.g., 2048 or 4096
        0,            // metadata_len: 0 = default
        4096,         // tot_num_buffers: Number of buffers in cluster
        -1,           // numa_node_id: -1 = any node
        NULL,         // hugepages_mountpoint: use default
        0             // flags: default behavior
    );
    
    if (cluster == NULL) {
        return false;
    }

    pfring_zc_queue* queue = pfring_zc_open_device(cluster, ifname.c_str(), rx_only, 0);
    if (queue) {
        pfring_zc_close_device(queue);
        pfring_zc_destroy_cluster(cluster);
        return true;
    }

    pfring_zc_destroy_cluster(cluster);
    return false;
}

void initialize_dpdk(std::vector<interfaceModes>& interfaces) {
    const char* argv[] = {"detect", "-c", "2", "--huge-dir=/mnt/huge"};
    int argc = 4;

    if(!isHugepageMounted("/mnt/huge")) {
        std::cerr << "Hugepage not mounted. Run \nmount -t hugetlbfs nodev /mnt/huge \nmanualy with root previlage (mount hugepages)."
                     "Make sure it reserves 2MB for dpdk. If not - run \necho 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" << std::endl;
    }
    if (rte_eal_init(argc, const_cast<char**>(argv)) < 0) {
        std::cerr << "Failed to initialize DPDK EAL\n";
        return;
    }

    int nb_ports = rte_eth_dev_count_avail();
    for (int i = 0; i < nb_ports; i++) {
        rte_eth_dev_info dev_info;
        rte_eth_dev_info_get(i, &dev_info);
        if (dev_info.device) {
            std::string ifname = rte_dev_name(dev_info.device);
            std::cout << "DPDK: " << ifname << std::endl;
            auto it = std::find_if(interfaces.begin(), interfaces.end(), [&ifname](auto& i) {
                return i.interfaceName == ifname;
            });

            if (it != interfaces.end()) {
                it->dpdk_support = true;
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
