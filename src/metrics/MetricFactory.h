#pragma once

#include <memory>

#include "ISystemMetrics.h"

class MetricFactory {
public:
    static std::unique_ptr<ISystemMetrics> make();
};