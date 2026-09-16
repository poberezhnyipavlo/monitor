#pragma once

#include <memory>

#include "isystem_metrics.h"

class MetricFactory {
public:
    static std::unique_ptr<ISystemMetrics> make();
};