#include "MetricFactory.h"

#ifdef __APPLE__
    #include "MacosMetric.h"
#elif __linux__
    #include "LinuxMetric.h"
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
