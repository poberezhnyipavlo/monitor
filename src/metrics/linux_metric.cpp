#include "linux_metric.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>

double LinuxMetric::getCpuUsage() {
    const auto current_ticks = this->getTicks();
    if (!current_ticks || !this->prevTick_) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double busy_delta = static_cast<double>(current_ticks->busy()) - static_cast<double>(this->prevTick_->busy());
    const double total_delta = static_cast<double>(current_ticks->total()) - static_cast<double>(this->prevTick_->total());

    this->prevTick_ = current_ticks;

    return busy_delta / total_delta * 100;
}

double LinuxMetric::getMemoryUsage() {
    return 0.0;
}

std::optional<TickLinux> LinuxMetric::getTicks() const {
    std::ifstream file("/proc/stat");
    std::string line;
    std::getline(file, line);

    std::istringstream iss(line);
    std::string label;

    TickLinux tick{};
    iss
        >> label
        >> tick.user
        >> tick.nice
        >> tick.system
        >> tick.idle
        >> tick.iowait
        >> tick.irq
        >> tick.softirq
        >> tick.steal;

    return tick;
}
