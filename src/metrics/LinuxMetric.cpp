#include "LinuxMetric.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>

double LinuxMetric::getCpuUsage() {
    const auto current_ticks = this->getTicks();
    if (!current_ticks || !this->prev_tick_) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const int64_t busy_delta = static_cast<int64_t>(current_ticks->busy()) - static_cast<int64_t>(this->prev_tick_->busy());
    const int64_t total_delta = static_cast<int64_t>(current_ticks->total()) - static_cast<int64_t>(this->prev_tick_->total());

    this->prev_tick_ = current_ticks;

    return static_cast<double>(busy_delta) / static_cast<double>(total_delta) * 100;
}

double LinuxMetric::getMemoryUsage() {
    const auto mem_info = this->getMemInfo();

    if (!mem_info) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const int64_t used = mem_info->mem_total - mem_info->mem_available;

    return static_cast<double>(used) / static_cast<double>(mem_info->mem_total) * 100;
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

std::optional<MemInfoLinux> LinuxMetric::getMemInfo() const {
    std::ifstream file("/proc/meminfo");
    std::string line;
    int64_t mem_total = 0;
    int64_t mem_available = 0;

    while (std::getline(file, line)) {
        if (line.starts_with("MemTotal:")) {
            mem_total = this->parseMemLine(line);
        } else if (line.starts_with("MemAvailable:")) {
            mem_available = this->parseMemLine(line);
        }

        if (mem_total != 0 && mem_available != 0) {
            break;
        }
    }

    MemInfoLinux mem_info{.mem_total = mem_total, .mem_available = mem_available};

    return mem_info;
}

int64_t LinuxMetric::parseMemLine(const std::string& line) const {
    std::istringstream iss(line);
    std::string label;
    int64_t value;

    iss >> label >> value;

    return value;
}
