#include <iostream>
#include <cstring>
#include <csignal>
#include <atomic>

#include <rte_eal.h>
#include <rte_ethdev.h>

#include "Network/NetworkManager.hpp"

// Default EAL args: single core, explicit hugepage mount.
// Override by passing your own EAL args before "--" on the command line.
static char arg_l[]        = "-l";
static char arg_core[]     = "0";
static char arg_huge[]     = "--huge-dir=/mnt/huge";
static char* DEFAULT_EAL[] = { arg_l, arg_core, arg_huge };
static constexpr int DEFAULT_EAL_ARGC = 3;

// ── Port selection helpers ────────────────────────────────────────────────────

static void listPorts()
{
    const uint16_t nb = rte_eth_dev_count_avail();
    std::cout << "Available DPDK ports: " << nb << "\n";
    uint16_t id;
    RTE_ETH_FOREACH_DEV(id) {
        rte_eth_dev_info info{};
        rte_eth_dev_info_get(id, &info);
        std::cout << "  port " << id
                  << "  driver=" << (info.driver_name ? info.driver_name : "?")
                  << "  if="    << (info.if_index ? std::to_string(info.if_index) : "-")
                  << "\n";
    }
}

// Returns the port whose name matches `name`, or RTE_MAX_ETHPORTS if not found.
static uint16_t portByName(const char* name)
{
    uint16_t id;
    if (rte_eth_dev_get_port_by_name(name, &id) == 0)
        return id;
    return RTE_MAX_ETHPORTS;
}

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    // ── Parse our own args before "--" ────────────────────────────────────────
    // Usage: server [--port <id|name>] [--list] [-- <eal args>]
    int    appPort     = -1;       // -1 = use first available
    bool   listOnly    = false;
    char*  portName    = nullptr;

    int ealArgc = DEFAULT_EAL_ARGC;
    char** ealArgv = DEFAULT_EAL;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            listOnly = true;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            portName = argv[++i];
        } else if (std::strcmp(argv[i], "--") == 0) {
            // Everything after "--" is passed straight to rte_eal_init
            ealArgc = argc - (i + 1);
            ealArgv = argv + (i + 1);
            break;
        }
    }

    // ── EAL init (no mempool, no queue — just EAL itself) ────────────────────
    const int ret = rte_eal_init(ealArgc, ealArgv);
    if (ret < 0) {
        std::cerr << "rte_eal_init failed: " << rte_strerror(rte_errno) << "\n";
        return 1;
    }

    // ── Port discovery ────────────────────────────────────────────────────────
    if (rte_eth_dev_count_avail() == 0) {
        std::cerr << "No DPDK ports found. "
                     "Bind a NIC with dpdk-devbind.py first.\n";
        rte_eal_cleanup();
        return 1;
    }

    if (listOnly) { listPorts(); rte_eal_cleanup(); return 0; }

    uint16_t port_id = RTE_MAX_ETHPORTS;
    if (portName) {
        // Try numeric ID first, then symbolic name
        char* end;
        long n = std::strtol(portName, &end, 10);
        if (*end == '\0' && n >= 0 && n < RTE_MAX_ETHPORTS) {
            port_id = static_cast<uint16_t>(n);
        } else {
            port_id = portByName(portName);
        }
        if (port_id == RTE_MAX_ETHPORTS || !rte_eth_dev_is_valid_port(port_id)) {
            std::cerr << "Port '" << portName << "' not found or invalid.\n";
            listPorts();
            rte_eal_cleanup();
            return 1;
        }
    } else {
        // Pick first available port
        RTE_ETH_FOREACH_DEV(port_id) { break; }
    }

    std::cout << "Using DPDK port " << port_id << "\n";

    // ── Start application ─────────────────────────────────────────────────────
    // Generator is constructed with the validated port_id.
    // All mempool / queue setup happens inside doStart(), not here.
    NetworkManager manager(54444, port_id);   // pass port_id through
    while (1) {
        manager.listen();
    }

    rte_eal_cleanup();
    return 0;
}