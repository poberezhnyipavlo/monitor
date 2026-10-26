#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ISystemMetrics.h"

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

    [[nodiscard]] unsigned long long total() const { return busy() + idle + iowait; }
};

struct MemInfoLinux {
    int64_t mem_total;
    int64_t mem_available;
};

class LinuxMetric : public ISystemMetrics {
public:
    LinuxMetric() : prev_tick_(getTicks()) {}

    double getCpuUsage() override;
    double getMemoryUsage() override;

private:
    std::optional<TickLinux> prev_tick_;

    [[nodiscard]] std::optional<TickLinux> getTicks() const;
    [[nodiscard]] std::optional<MemInfoLinux> getMemInfo() const;
    [[nodiscard]] int64_t parseMemLine(const std::string& line) const;
};
