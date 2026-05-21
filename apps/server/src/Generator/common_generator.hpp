#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>

#include <sys/socket.h>   // AF_INET, SOCK_DGRAM, socket()
#include <sys/ioctl.h>    // ioctl(), SIOCGIFMTU
#include <net/if.h>       // ifreq, IFNAMSIZ
#include <cstring>        // std::memset, std::strncpy
#include <unistd.h>       // close()

#include "generator_values.hpp"

namespace generator {
    struct statisticData {
        uint64_t packetsSent{};
        uint64_t bytesSent{};
        uint64_t txErrors{};
        uint64_t totalBytesSent{};

        statisticData() {}
        statisticData(uint64_t packets, uint64_t bytes, uint64_t errors, uint64_t total)
            : packetsSent(packets),
              bytesSent(bytes),
              txErrors(errors),
              totalBytesSent(total)
        {}
    };

    struct statisticQueue {
        std::queue<statisticData> queue{};
        std::mutex mutex{};
        std::condition_variable dataAval{};
    };

    struct messageQueue {
        std::queue<generatorMessage> queue{};
        std::mutex mutex{};
        std::condition_variable dataAval{};
    };
    enum Status {
        SUCCESS,
        WARNING,
        ERROR
    };

    struct statusQueue {
        std::queue<Status> queue{};
        std::mutex mutex{};
        std::condition_variable requestReceived{};

        bool empty() {
            std::unique_lock lock(mutex);
            return queue.empty();
        }

        void pushStatus(const Status status) {
            std::unique_lock lock(mutex);
            queue.push(status);
        }

        Status popStatus() {
            std::unique_lock lock(mutex);
            const Status status = queue.front();
            queue.pop();
            return status;
        }
    };
}

