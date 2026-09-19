#include "macos_metric.h"

#include <mach/mach.h>
#include <mach/mach_host.h>

#include <thread>

double MacOSMetric::getCpuUsage() {
    auto prev_ticks = this->getTicks();
    if (!prev_ticks) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto curr_ticks = this->getTicks();
    if (!curr_ticks) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto sum_curr_ticks = curr_ticks->user + curr_ticks->system + curr_ticks->nice;
    auto sum_prev_ticks = prev_ticks->user + prev_ticks->system + prev_ticks->nice;

    int64_t delta_busy = static_cast<int64_t>(sum_curr_ticks) - static_cast<int64_t>(sum_prev_ticks);
    int64_t delta_total = delta_busy + (static_cast<int64_t>(curr_ticks->idle) - static_cast<int64_t>(prev_ticks->idle));

    return (static_cast<double>(delta_busy) / static_cast<double>(delta_total)) * 100.0;
}

double MacOSMetric::getMemoryUsage() {
    return 0.0;
}

std::optional<Tiсk> MacOSMetric::getTicks() const {
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t cpuCount;
    natural_t numCpus;

    kern_return_t err = host_processor_info(
        mach_host_self(),
        PROCESSOR_CPU_LOAD_INFO,
        &numCpus,
        &cpuInfo,
        &cpuCount
    );

    if (err != KERN_SUCCESS) {
        return std::nullopt;
    }

    uint64_t user = 0, system = 0, idle = 0, nice = 0;

    for (natural_t i = 0; i < numCpus; i++) {
        user += cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER];
        system += cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM];
        idle += cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE];
        nice += cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE];
    }

    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuInfo), cpuCount * sizeof(integer_t));

    Tiсk t{};

    t.idle = idle;
    t.user = user;
    t.system = system;
    t.nice = nice;

    return t;
}
