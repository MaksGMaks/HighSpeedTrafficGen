#include <iostream>

// DPDK
extern "C" {
#include <rte_eal.h>
#include <rte_ethdev.h>
}

// PF_RING
#include "pfring.h"

int main() {
    std::cout << "Hello World!" << std::endl;
    return 0;
}