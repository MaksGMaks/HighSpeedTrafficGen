#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../../third-party/nlohmann_json/nlohmann/json.hpp"

#if defined(__linux__) || defined(__APPLE__)
#   include <arpa/inet.h>
#   include <ifaddrs.h>
#   include <net/if.h>
#   include <netinet/in.h>
#endif

#include <random>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// GenLaw — packet generation laws
// ─────────────────────────────────────────────────────────────────────────────

namespace GenLaw {

enum class Mode         { Static, Range, Random };
enum class Distribution { Uniform, Normal, Exponential };
enum class Protocol     { TCP, UDP, ICMP, ARP };

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

    inline uint32_t ipToU32(const std::string &ip)
    {
        uint32_t result = 0;
        int octet = 0, shift = 24;
        for (char c : ip) {
            if (c == '.') {
                result |= static_cast<uint32_t>(octet) << shift;
                shift -= 8;
                octet  = 0;
            } else if (c >= '0' && c <= '9') {
                octet = octet * 10 + (c - '0');
            }
        }
        result |= static_cast<uint32_t>(octet);
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
    T            step  = {};
    Distribution dist  = Distribution::Uniform;

    T resolve(std::mt19937 &rng) const;
};

template<>
inline uint8_t Param<uint8_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 1.0   : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 255.0 : static_cast<double>(max);
    return static_cast<uint8_t>(
        std::clamp(std::round(detail::applyDist(rng, mn, mx, dist)), mn, mx));
}

template<>
inline uint16_t Param<uint16_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0     : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 65535.0 : static_cast<double>(max);
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0) val = std::round(val / step) * step;
    return static_cast<uint16_t>(std::clamp(std::round(val), mn, mx));
}

template<>
inline uint32_t Param<uint32_t>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0          : static_cast<double>(min);
    const double mx = (mode == Mode::Random) ? 4294967295.0 : static_cast<double>(max);
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0) val = std::round(val / step) * step;
    return static_cast<uint32_t>(std::clamp(std::round(val), mn, mx));
}

template<>
inline double Param<double>::resolve(std::mt19937 &rng) const
{
    if (mode == Mode::Static) return value;
    const double mn = (mode == Mode::Random) ? 0.0    : min;
    const double mx = (mode == Mode::Random) ? 3600.0 : max;
    double val = detail::applyDist(rng, mn, mx, dist);
    if (step > 0.0) val = std::round(val / step) * step;
    return val;
}

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

// ── Protocol param ────────────────────────────────────────────────────────────

struct ProtocolParam {
    Mode     mode     = Mode::Static;
    Protocol protocol = Protocol::TCP;
    bool tcp  = true;
    bool udp  = true;
    bool icmp = false;
    bool arp  = false;

    Protocol resolve(std::mt19937 &rng) const
    {
        if (mode == Mode::Static) return protocol;
        std::vector<Protocol> pool;
        pool.reserve(4);
        if (tcp)  pool.push_back(Protocol::TCP);
        if (udp)  pool.push_back(Protocol::UDP);
        if (icmp) pool.push_back(Protocol::ICMP);
        if (arp)  pool.push_back(Protocol::ARP);
        if (pool.empty()) return Protocol::TCP;
        std::uniform_int_distribution<std::size_t> d(0, pool.size() - 1);
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
    Param<uint32_t>    packetSize;
    Param<double>      timeDiff;
    ProtocolParam      protocol;
    uint64_t           packetCount = 0;

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
        return Resolved{
            srcIP.resolve(rng),
            dstIP.resolve(rng),
            srcPort.resolve(rng),
            dstPort.resolve(rng),
            ttl.resolve(rng),
            std::max(packetSize.resolve(rng), 14u),
            timeDiff.resolve(rng),
            protocol.resolve(rng)
        };
    }
};

inline Law makeDefaultLaw()
{
    Law l;
    l.srcIP.mode  = Mode::Static;   l.srcIP.value  = "192.168.0.1";
    l.dstIP.mode  = Mode::Static;   l.dstIP.value  = "192.168.0.2";
    l.srcPort.mode = Mode::Range;   l.srcPort.min  = 1024; l.srcPort.max = 65535;
    l.dstPort.mode = Mode::Static;  l.dstPort.value = 80;
    l.ttl.mode     = Mode::Static;  l.ttl.value     = 64;
    l.packetSize.mode = Mode::Range;
    l.packetSize.min  = 64;
    l.packetSize.max  = 1500;
    l.timeDiff.mode  = Mode::Static; l.timeDiff.value = 0.001;
    l.protocol.mode  = Mode::Static; l.protocol.protocol = Protocol::TCP;
    l.packetCount = 0;
    return l;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON serialisation — inside GenLaw namespace so ADL finds them
// ─────────────────────────────────────────────────────────────────────────────

// ── Enums — must come first, Param<T> serializers depend on them ──────────────

inline void to_json(nlohmann::json &j, Mode v)
    { j = static_cast<int>(v); }
inline void from_json(const nlohmann::json &j, Mode &v)
    { v = static_cast<Mode>(j.get<int>()); }

inline void to_json(nlohmann::json &j, Distribution v)
    { j = static_cast<int>(v); }
inline void from_json(const nlohmann::json &j, Distribution &v)
    { v = static_cast<Distribution>(j.get<int>()); }

inline void to_json(nlohmann::json &j, Protocol v)
    { j = static_cast<int>(v); }
inline void from_json(const nlohmann::json &j, Protocol &v)
    { v = static_cast<Protocol>(j.get<int>()); }

// ── Param<T> ──────────────────────────────────────────────────────────────────

template<typename T>
void to_json(nlohmann::json &j, const Param<T> &p)
{
    j = {
        { "mode",  p.mode  },
        { "value", p.value },
        { "min",   p.min   },
        { "max",   p.max   },
        { "step",  p.step  },
        { "dist",  p.dist  }
    };
}

template<typename T>
void from_json(const nlohmann::json &j, Param<T> &p)
{
    j.at("mode") .get_to(p.mode);
    j.at("value").get_to(p.value);
    j.at("min")  .get_to(p.min);
    j.at("max")  .get_to(p.max);
    j.at("step") .get_to(p.step);
    j.at("dist") .get_to(p.dist);
}

// ── ProtocolParam ─────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json &j, const ProtocolParam &p)
{
    j = {
        { "mode",     p.mode     },
        { "protocol", p.protocol },
        { "tcp",      p.tcp      },
        { "udp",      p.udp      },
        { "icmp",     p.icmp     },
        { "arp",      p.arp      }
    };
}

inline void from_json(const nlohmann::json &j, ProtocolParam &p)
{
    j.at("mode")    .get_to(p.mode);
    j.at("protocol").get_to(p.protocol);
    j.at("tcp")     .get_to(p.tcp);
    j.at("udp")     .get_to(p.udp);
    j.at("icmp")    .get_to(p.icmp);
    j.at("arp")     .get_to(p.arp);
}

// ── Law ───────────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json &j, const Law &l)
{
    j = {
        { "srcIP",       l.srcIP       },
        { "dstIP",       l.dstIP       },
        { "srcPort",     l.srcPort     },
        { "dstPort",     l.dstPort     },
        { "ttl",         l.ttl         },
        { "packetSize",  l.packetSize  },
        { "timeDiff",    l.timeDiff    },
        { "protocol",    l.protocol    },
        { "packetCount", l.packetCount }
    };
}

inline void from_json(const nlohmann::json &j, Law &l)
{
    j.at("srcIP")      .get_to(l.srcIP);
    j.at("dstIP")      .get_to(l.dstIP);
    j.at("srcPort")    .get_to(l.srcPort);
    j.at("dstPort")    .get_to(l.dstPort);
    j.at("ttl")        .get_to(l.ttl);
    j.at("packetSize") .get_to(l.packetSize);
    j.at("timeDiff")   .get_to(l.timeDiff);
    j.at("protocol")   .get_to(l.protocol);
    j.at("packetCount").get_to(l.packetCount);
}

} // namespace GenLaw

// ─────────────────────────────────────────────────────────────────────────────
// PcapParams
// ─────────────────────────────────────────────────────────────────────────────

namespace PcapParams {

enum class SpeedMode { Original = 0, Multiplier = 1, Fixed = 2, Max = 3 };

struct PlayerSettings {
    std::string filePath;
    int         startPacket = 1;
    int         endPacket   = 0;
    bool        loop        = false;
    int         loopCount   = 1;
    SpeedMode   speedMode   = SpeedMode::Original;
    double      speedMult   = 1.0;
    int         fixedRate   = 1000;
};

// ── SpeedMode ─────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json &j, SpeedMode v)
    { j = static_cast<int>(v); }
inline void from_json(const nlohmann::json &j, SpeedMode &v)
    { v = static_cast<SpeedMode>(j.get<int>()); }

// ── PlayerSettings ────────────────────────────────────────────────────────────
// filePath is NOT serialised — populated server-side after file transfer.

inline void to_json(nlohmann::json &j, const PlayerSettings &s)
{
    j = {
        { "startPacket", s.startPacket },
        { "endPacket",   s.endPacket   },
        { "loop",        s.loop        },
        { "loopCount",   s.loopCount   },
        { "speedMode",   s.speedMode   },
        { "speedMult",   s.speedMult   },
        { "fixedRate",   s.fixedRate   }
    };
}

inline void from_json(const nlohmann::json &j, PlayerSettings &s)
{
    j.at("startPacket").get_to(s.startPacket);
    j.at("endPacket")  .get_to(s.endPacket);
    j.at("loop")       .get_to(s.loop);
    j.at("loopCount")  .get_to(s.loopCount);
    j.at("speedMode")  .get_to(s.speedMode);
    j.at("speedMult")  .get_to(s.speedMult);
    j.at("fixedRate")  .get_to(s.fixedRate);
}

} // namespace PcapParams

// ─────────────────────────────────────────────────────────────────────────────
// genParams — what the client sends in a START command
// ─────────────────────────────────────────────────────────────────────────────

enum class GeneratorMode : int8_t {
    RandomLaw  = 0,
    PcapPlayer = 1,
};

// GeneratorMode serializers — global namespace matches the enum's namespace
inline void to_json(nlohmann::json &j, GeneratorMode v)
    { j = static_cast<int>(v); }
inline void from_json(const nlohmann::json &j, GeneratorMode &v)
    { v = static_cast<GeneratorMode>(j.get<int>()); }

struct genParams {
    GeneratorMode mode    = GeneratorMode::RandomLaw;
    uint32_t      time    = 0;

    GenLaw::Law                law{};
    PcapParams::PlayerSettings playerSettings{};
};

// ─────────────────────────────────────────────────────────────────────────────
// ServerStats
// ─────────────────────────────────────────────────────────────────────────────

struct ServerStats {
    uint64_t opackets    = 0;
    uint64_t obytes      = 0;
    uint64_t ipackets    = 0;
    uint64_t ibytes      = 0;
    uint64_t oerrors     = 0;
    uint64_t ierrors     = 0;
    int64_t  timestampMs = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// JSON keys
// ─────────────────────────────────────────────────────────────────────────────

namespace jsonHeaders {

enum class TypeC    : int { Accept = 0, Failed = 1, Request = 2, Response = 3, File = 4 };
enum class CommandC : int { Start = 0, Pause = 1, Resume = 2, Finish = 3 };

constexpr const char *Type    = "TYPE";
constexpr const char *Command = "COMMAND";

// Numeric shorthands used in NetworkManager
constexpr int REQUEST = static_cast<int>(TypeC::Request);
constexpr int ACCEPT  = static_cast<int>(TypeC::Accept);
constexpr int FAILED  = static_cast<int>(TypeC::Failed);
constexpr int PAUSE   = static_cast<int>(CommandC::Pause);
constexpr int RESUME  = static_cast<int>(CommandC::Resume);
constexpr int FINISH  = static_cast<int>(CommandC::Finish);

constexpr const char *DpdkSupported     = "dpdkSupported";
constexpr const char *PfRingSupported   = "pf_ringSupported";
constexpr const char *PfRingZcSupported = "pf_ring_zcSupported";

namespace Params {
    constexpr const char *Mode = "mode";
    constexpr const char *Time = "time";
    constexpr const char *Law  = "law";
    constexpr const char *Pcap = "pcap";
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

namespace File {
    constexpr const char *Key = "FILE_DATA";
}

} // namespace jsonHeaders

// ─────────────────────────────────────────────────────────────────────────────
// genParams JSON — global namespace, after both sub-type serializers are visible
// ─────────────────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json &j, const genParams &p)
{
    j[jsonHeaders::Params::Mode] = p.mode;
    j[jsonHeaders::Params::Time] = p.time;

    if (p.mode == GeneratorMode::RandomLaw)
        j[jsonHeaders::Params::Law]  = p.law;
    else
        j[jsonHeaders::Params::Pcap] = p.playerSettings;
}

inline void from_json(const nlohmann::json &j, genParams &p)
{
    j.at(jsonHeaders::Params::Mode).get_to(p.mode);
    j.at(jsonHeaders::Params::Time).get_to(p.time);

    if (p.mode == GeneratorMode::RandomLaw) {
        if (j.contains(jsonHeaders::Params::Law))
            j.at(jsonHeaders::Params::Law).get_to(p.law);
    } else {
        if (j.contains(jsonHeaders::Params::Pcap))
            j.at(jsonHeaders::Params::Pcap).get_to(p.playerSettings);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Network constants
// ─────────────────────────────────────────────────────────────────────────────

namespace networkAttribute {
    constexpr uint16_t PORT_NUM = 54000;
}