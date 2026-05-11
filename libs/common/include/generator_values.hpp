#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/ioctl.h>

#include <random>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Linux network
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>

namespace GenLaw {

// ── Enums ─────────────────────────────────────────────────────────────────────

enum class Mode         { Static, Range, Random };
enum class Distribution { Uniform, Normal, Exponential };
enum class Protocol     { TCP, UDP, ICMP, ARP };

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace detail {

inline double applyDist(std::mt19937 &rng, double mn, double mx,
                        Distribution dist)
{
    if (mn > mx) std::swap(mn, mx);

    switch (dist) {
    case Distribution::Normal: {
        const double mid   = (mn + mx) / 2.0;
        const double sigma = (mx - mn) / 6.0;
        std::normal_distribution<double> d(mid, sigma);
        return std::clamp(d(rng), mn, mx);
    }
    case Distribution::Exponential: {
        const double lambda = 2.0 / std::max(mx - mn, 1e-12);
        std::exponential_distribution<double> d(lambda);
        return std::clamp(mn + d(rng), mn, mx);
    }
    default: {
        std::uniform_real_distribution<double> d(mn, mx);
        return d(rng);
    }
    }
}

// Parse "A.B.C.D" → uint32
inline uint32_t ipToU32(const std::string &ip)
{
    uint32_t result = 0;
    int      octet  = 0;
    int      shift  = 24;
    for (char c : ip) {
        if (c == '.') {
            result |= static_cast<uint32_t>(octet) << shift;
            shift  -= 8;
            octet   = 0;
        } else if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');
        }
    }
    result |= static_cast<uint32_t>(octet); // last octet, shift == 0
    return result;
}

inline std::string u32ToIp(uint32_t n)
{
    return std::to_string((n >> 24) & 0xFF) + '.' +
           std::to_string((n >> 16) & 0xFF) + '.' +
           std::to_string((n >>  8) & 0xFF) + '.' +
           std::to_string( n        & 0xFF);
}

} // namespace detail

// ── Generic Param ─────────────────────────────────────────────────────────────

template<typename T>
struct Param {
    Mode         mode  = Mode::Static;
    T            value = {};
    T            min   = {};
    T            max   = {};
    T            step  = {};           // 0 = continuous
    Distribution dist  = Distribution::Uniform;

    T resolve(std::mt19937 &rng) const;
};

// ── Specialization: uint8_t (TTL, etc.) ──────────────────────────────────────

template<>
inline uint8_t Param<uint8_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 1.0   : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 255.0 : static_cast<double>(max);
    return static_cast<uint8_t>(
        std::clamp(std::round(detail::applyDist(rng, mn, mx, dist)), mn, mx));
}

// ── Specialization: uint16_t (ports) ─────────────────────────────────────────

template<>
inline uint16_t Param<uint16_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0     : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 65535.0 : static_cast<double>(max);
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0)
        val = std::round(val / step) * step;
    return static_cast<uint16_t>(
        std::clamp(std::round(val), mn, mx));
}

// ── Specialization: uint32_t (packet size, etc.) ─────────────────────────────

template<>
inline uint32_t Param<uint32_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0           : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 4294967295.0  : static_cast<double>(max);
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0)
        val = std::round(val / step) * step;
    return static_cast<uint32_t>(
        std::clamp(std::round(val), mn, mx));
}

// ── Specialization: double (time diff, etc.) ──────────────────────────────────

template<>
inline double Param<double>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0    : min;
    const double mx = (mode == Mode::Random) ? 3600.0 : max;
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0.0)
        val = std::round(val / step) * step;
    return val;
}

// ── Specialization: std::string (IP addresses) ───────────────────────────────

template<>
inline std::string Param<std::string>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;

    uint32_t lo, hi;
    if (mode == Mode::Random) {
        lo = detail::ipToU32("1.0.0.0");
        hi = detail::ipToU32("254.255.255.255");
    } else {
        lo = detail::ipToU32(min);
        hi = detail::ipToU32(max);
        if (lo > hi) std::swap(lo, hi);
    }

    std::uniform_int_distribution<uint32_t> d(lo, hi);
    return detail::u32ToIp(d(rng));
}

// ── Protocol param — separate since it's not a simple scalar ─────────────────

struct ProtocolParam {
    Mode     mode     = Mode::Static;
    Protocol protocol = Protocol::TCP;

    // Pool for Random mode — at least one must be true
    bool tcp  = true;
    bool udp  = true;
    bool icmp = false;
    bool arp  = false;

    Protocol resolve(std::mt19937 &rng) const
    {
        if (mode == Mode::Static) return protocol;

        // Build pool of enabled protocols
        std::vector<Protocol> pool;
        pool.reserve(4);
        if (tcp)  pool.push_back(Protocol::TCP);
        if (udp)  pool.push_back(Protocol::UDP);
        if (icmp) pool.push_back(Protocol::ICMP);
        if (arp)  pool.push_back(Protocol::ARP);

        if (pool.empty()) return Protocol::TCP; // fallback

        std::uniform_int_distribution<size_t> d(0, pool.size() - 1);
        return pool[d(rng)];
    }
};

// ── Full generator law ────────────────────────────────────────────────────────

struct Law {
    Param<std::string> srcIP;
    Param<std::string> dstIP;
    Param<uint16_t>    srcPort;
    Param<uint16_t>    dstPort;
    Param<uint8_t>     ttl;
    Param<uint32_t>    packetSize;  // bytes, min 14 (ethernet header)
    Param<double>      timeDiff;    // seconds between packets

    ProtocolParam      protocol;

    uint64_t           packetCount = 0; // 0 = infinite

    // ── Convenience: resolve all at once into a flat struct ──────────────────
    struct Resolved {
        std::string srcIP;
        std::string dstIP;
        uint16_t    srcPort;
        uint16_t    dstPort;
        uint8_t     ttl;
        uint32_t    packetSize;
        double      timeDiff;
        Protocol    protocol;
    };

    Resolved resolve(std::mt19937 &rng) const
    {
        return Resolved {
            srcIP.resolve(rng),
            dstIP.resolve(rng),
            srcPort.resolve(rng),
            dstPort.resolve(rng),
            ttl.resolve(rng),
            std::max(packetSize.resolve(rng), 14u), // enforce min frame size
            timeDiff.resolve(rng),
            protocol.resolve(rng)
        };
    }
};

// ── Default laws ──────────────────────────────────────────────────────────────

inline Law makeDefaultLaw()
{
    Law l;

    l.srcIP.mode  = Mode::Static;
    l.srcIP.value = "192.168.0.1";

    l.dstIP.mode  = Mode::Static;
    l.dstIP.value = "192.168.0.2";

    l.srcPort.mode  = Mode::Range;
    l.srcPort.min   = 1024;
    l.srcPort.max   = 65535;

    l.dstPort.mode  = Mode::Static;
    l.dstPort.value = 80;

    l.ttl.mode  = Mode::Static;
    l.ttl.value = 64;

    l.packetSize.mode = Mode::Range;
    l.packetSize.min  = 64;
    l.packetSize.max  = 1500;

    l.timeDiff.mode  = Mode::Static;
    l.timeDiff.value = 0.001;

    l.protocol.mode     = Mode::Static;
    l.protocol.protocol = Protocol::TCP;

    l.packetCount = 0; // infinite

    return l;
}

}

struct genParams {
    std::string interfaceName{};
    int         mode          = 0;
    uint32_t    time          = 0;
    uint64_t    speed         = 0;
    uint32_t    packSize      = 0;
    bool        fileSend      = false;
    std::string filePath{};
    uint64_t    copies        = 0;
    uint64_t    totalSend     = 0;
    int         burstSize     = 0;
    int         packetPattern = 0;
};

struct ServerStats {
    uint64_t opackets    = 0;
    uint64_t obytes      = 0;
    uint64_t ipackets    = 0;
    uint64_t ibytes      = 0;
    uint64_t oerrors     = 0;
    uint64_t ierrors     = 0;
    int64_t  timestampMs = 0;
};


namespace jsonHeaders {

enum TypeC    { ACCEPT, FAILED, REQUEST, RESPONSE, MESSAGE };
enum CommandC { START, PAUSE, RESUME, FINISH };

// Top-level keys
constexpr const char *Type    = "TYPE";
constexpr const char *Command = "COMMAND";

// Device info keys
constexpr const char *InterfaceName     = "interfaceName";
constexpr const char *DpdkSupported     = "dpdkSupported";
constexpr const char *PfRingSupported   = "pf_ringSupported";
constexpr const char *PfRingZcSupported = "pf_ring_zcSupported";

namespace Parameters {
constexpr const char *Mode          = "MODE";
constexpr const char *Time          = "TIME";
constexpr const char *Speed         = "SPEED";
constexpr const char *PackSize      = "PACK_SIZE";
constexpr const char *BurstSize     = "BURST_SIZE";
constexpr const char *PacketPattern = "PACK_PATTERN";
constexpr const char *FileSend      = "FILE_SEND";
constexpr const char *Copies        = "COPIES";
constexpr const char *TotalSend     = "TOTAL_SEND";
}

namespace Data {
constexpr const char *Key         = "DATA";
constexpr const char *Pps         = "pps";
constexpr const char *Bps         = "bps";
constexpr const char *BPS         = "BPS";
constexpr const char *Time        = "time";
constexpr const char *TotalSend   = "totalSend";
constexpr const char *TotalCopies = "totalCopies";
}

struct receivedData {
    uint64_t pps         = 0;
    uint64_t bps         = 0;
    uint64_t BPS         = 0;
    int64_t  time        = 0;
    uint64_t totalCopies = 0;
    uint64_t totalSend   = 0;
};

}

namespace networkAttribute {
  const uint16_t PORT_NUM = 54000;
}
