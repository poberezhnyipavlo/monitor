#include "macos_metric.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_types.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <thread>

double MacOSMetric::getCpuUsage() {
    const auto current_ticks = this->getTicks();
    if (!current_ticks || !this->prevTick_) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto sum_curr_ticks = current_ticks->user + current_ticks->system + current_ticks->nice;
    const auto sum_prev_ticks = this->prevTick_->user + this->prevTick_->system + this->prevTick_->nice;

    const int64_t delta_busy = static_cast<int64_t>(sum_curr_ticks) - static_cast<int64_t>(sum_prev_ticks);
    const int64_t delta_total = delta_busy + (static_cast<int64_t>(current_ticks->idle) - static_cast<int64_t>(this->prevTick_->idle));

    this->prevTick_ = current_ticks;

    return (static_cast<double>(delta_busy) / static_cast<double>(delta_total)) * 100.0;
}

double MacOSMetric::getMemoryUsage() {
    const mach_port_t host_port = mach_host_self();
    mach_msg_type_number_t host_size = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stats;

    const kern_return_t status = host_statistics64(
        host_port,
        HOST_VM_INFO64,
        reinterpret_cast<host_info64_t>(&vm_stats),
        &host_size
    );

    if (status != KERN_SUCCESS) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    vm_size_t page_size;
    host_page_size(host_port, &page_size);

    const double total_used_bytes =
        (static_cast<double>(vm_stats.active_count)
        + vm_stats.inactive_count
        + vm_stats.wire_count
        + vm_stats.compressor_page_count) * page_size;

    uint64_t total_mem;
    size_t len = sizeof(total_mem);
    sysctlbyname("hw.memsize", &total_mem, &len, nullptr, 0);

    return total_used_bytes / static_cast<double>(total_mem) * 100;
}

std::optional<TickMacOS> MacOSMetric::getTicks() const {
    const mach_port_t host_port = mach_host_self();
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t cpuCount;
    natural_t numCpus;

    const kern_return_t err = host_processor_info(
        host_port,
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

    TickMacOS t{};

    t.idle = idle;
    t.user = user;
    t.system = system;
    t.nice = nice;

    return t;
}
