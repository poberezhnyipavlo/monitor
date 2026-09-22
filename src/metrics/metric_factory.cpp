#include "metric_factory.h"

#ifdef __APPLE__
    #include "macos_metric.h"
#elif __linux__
    #include "linux_metric.h"
#endif

std::unique_ptr<ISystemMetrics> MetricFactory::make() {
    #ifdef __APPLE__
        return std::make_unique<MacOSMetric>();
    #elif __linux__
        return std::make_unique<LinuxMetric>();
    #else
        #error "Unsupported platform: only macOS and Linux are supported"
    #endif
}
