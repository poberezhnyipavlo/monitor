#pragma once

#include <cstdint>
#include <optional>

#include "isystem_metrics.h"

struct Tiks {
    uint64_t user;
    uint64_t system;
    uint64_t idle;
    uint64_t nice;
};

class MacOSMetric : public ISystemMetrics {
public:
    double getCpuUsage() override;
    double getMemoryUsage() override;
private:
    [[nodiscard]] std::optional<Tiks> getTicks() const;
};
