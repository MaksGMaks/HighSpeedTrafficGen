#include "PFRING_Engine.hpp"

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

void pfringGenerate(pfring* ring, const uint8_t* packetData, uint32_t packetLen) {
    int rc = pfring_send(ring, (char*)packetData, packetLen, 1 /* flush = true */);
    if (rc < 0) {
        std::cerr << "PF_RING send error: " << rc << std::endl;
    }
}