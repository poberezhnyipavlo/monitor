#pragma once

#include <optional>

#include "isystem_metrics.h"
#include <cstdint>
#include <string>

struct TickLinux {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;

    [[nodiscard]] unsigned long long busy() const {
        return user + nice + system + irq + softirq + steal;
    }

    [[nodiscard]] unsigned long long total() const {
        return busy() + idle + iowait;
    }
};

struct MemInfoLinux {
    int64_t memTotal;
    int64_t memAvailable;
};

class LinuxMetric : public ISystemMetrics {
public:
    LinuxMetric() : prevTick_(getTicks()) {}

    double getCpuUsage() override;
    double getMemoryUsage() override;
private:
    std::optional<TickLinux> prevTick_;

    [[nodiscard]]std::optional<TickLinux> getTicks() const;
    [[nodiscard]]std::optional<MemInfoLinux> getMemInfo() const;
    [[nodiscard]]int64_t parseMemLine(const std::string& line) const;
};
