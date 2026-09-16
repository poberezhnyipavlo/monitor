#include "metric_factory.h"
#include "macos_metric.h"

std::unique_ptr<ISystemMetrics> MetricFactory::make() {
    #ifdef __APPLE__
        return std::make_unique<MacOSMetric>();
    #endif
}
