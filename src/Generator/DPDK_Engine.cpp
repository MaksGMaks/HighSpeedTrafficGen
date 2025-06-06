#include "DPDK_Engine.hpp"

bool initialize_dpdk() {
    std::vector<char*> argv = {"detect", "--huge-dir=/mnt/huge"};
    //const char* argv[] = {"detect", "-c", "2", "--huge-dir=/mnt/huge"};

    if(!isHugepageMounted("/mnt/huge")) {
        std::cerr << "Hugepage not mounted. Run \nmount -t hugetlbfs nodev /mnt/huge \nmanualy with root previlage (mount hugepages)."
                     "Make sure it reserves 2MB for dpdk. If not - run \necho 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" << std::endl;
    }

    int argc = static_cast<int>(argv.size());

    if (rte_eal_init(argc, const_cast<char**>(argv.data())) < 0) {
        std::cerr << "Failed to initialize DPDK EAL\n";
        return false;
    }
    return true;
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

void checkDPDKSupport(std::vector<interfaceModes>& interfaces) {
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