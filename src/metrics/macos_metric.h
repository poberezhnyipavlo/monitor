#pragma once

#include "isystem_metrics.h"

class MacOSMetric : public ISystemMetrics {
public:
    double getCpuUsage() override;
    double getMemoryUsage() override;
};
