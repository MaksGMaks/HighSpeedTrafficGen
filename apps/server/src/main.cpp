#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <atomic>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>

#define DEFAULT_DPDK_ARGC 3
static char* DEFAULT_DPDK_ARGV[] = {(char*)"-l", (char*)"0", (char*)"--huge-dir=/mnt/huge"};

int find_app_arg(char** arr, int count) {
    for (int i = 0; i < count; ++i) {
        if (std::strcmp(arr[i], "--") == 0) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char** argv) {
    // Separate EAL args from APP args
    int app_arg_p = find_app_arg(argv, argc);
    int ret;

    // Initializing EAL depending on input (using minimum cores and own huge mount point)
    if (app_arg_p != -1) {
        std::cout << "EAL args not provided. Using default: -l 0 --huge-dir=/mnt/huge. \nMake sure /mnt/huge exists, huge pages are mounted and reserve 2M \n";
        ret = rte_eal_init(DEFAULT_DPDK_ARGC, DEFAULT_DPDK_ARGV);
    } else {
        ret = rte_eal_init(argc, argv);
    }
    if (ret < 0) {
        std::cerr << "EAL init failed: " << rte_strerror(rte_errno) << "\n";
        return 1;
    }
    // Checking ports
    uint16_t nb_ports = rte_eth_dev_count_avail();
    std::cout << "Available DPDK ports: " << nb_ports << "\n";
    if (nb_ports == 0) {
        std::cerr << "No ports found. Bind NIC with dpdk-devbind.py first.\n";
        rte_eal_cleanup();
        return 1;
    }

    int portID = -1;
    // if (!strcmp(argv[i], "--port")  && i+1 < argc) {
    //     try {
    //         portID = std::stoi(argv[++i]);
    //     } catch (std::invalid_argument e) {
    //         std::cerr << "Parsing error: " << e.what() << "\n";
    //     }
    // } else {
    //     std::cerr << "Unknown argument. Terminating...\n";
    //     return 1;
    // }
    return 0;
}