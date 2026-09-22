#pragma once

#include "isystem_metrics.h"

class LinuxMetric : public ISystemMetrics {
public:
    double getCpuUsage() override;
    double getMemoryUsage() override;
};
