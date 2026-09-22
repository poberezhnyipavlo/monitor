#pragma once

#include <optional>

#include "isystem_metrics.h"
#include <cstdint>
#include <string>

struct TickLinux {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;

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
