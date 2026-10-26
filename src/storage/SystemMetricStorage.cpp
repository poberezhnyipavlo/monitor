#include "SystemMetricStorage.h"

#include <rrd.h>
#include <sstream>
#include <vector>

#include "RrdMutex.h"
#include "StorageExceptions.h"

std::string SystemMetricStorage::getPath() const {
    return this->system_metrics_path_;
}

void SystemMetricStorage::save(double cpu, double memory) {
    std::lock_guard<std::mutex> lock(rrdMutex());

    std::ostringstream update_value;
    update_value << "N:" << cpu << ":" << memory;

    const std::vector<std::string> args = {
        "update",
        this->system_metrics_path_,
        update_value.str()
    };

    std::vector<const char*> argv;
    argv.reserve(args.size());

    for (const auto& arg : args) {
        argv.push_back(arg.data());
    }

    optind = 0;
    rrd_clear_error();

    const int result = rrd_update(static_cast<int>(argv.size()), argv.data());

    if (result != 0) {
        throw StorageException("Failed to update system metrics");
    }
}


void SystemMetricStorage::createFile() const {
    this->ensureAccess();

    std::lock_guard<std::mutex> lock(rrdMutex());

    std::vector<std::string> args = {
        "create",
        this->system_metrics_path_,
        "--step",
        "1",
        "DS:cpu:GAUGE:5:0:100",
        "DS:memory:GAUGE:5:0:100",
        "RRA:AVERAGE:0.5:1:100",
    };

    std::vector<const char*> argv;
    argv.reserve(args.size());

    for (const auto& arg : args) {
        argv.push_back(arg.data());
    }

    optind = 0;
    rrd_clear_error();
    const int result = rrd_create(static_cast<int>(argv.size()), argv.data());

    if (result != 0) {
        throw StorageException("Failed to create RRD file: " + std::string(rrd_get_error()));
    }
}
