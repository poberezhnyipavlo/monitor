#include "SystemMetricStorage.h"

#include <rrd.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "RrdMutex.h"
#include "StorageExceptions.h"

namespace {

// librrd's rrd_update/rrd_create take `char**` in older versions (1.7.2 on Ubuntu 24.04) and
// `const char**` in newer ones (1.11 in Homebrew). Neither changes the strings, so we keep
// `const char*` and cast to whatever the installed header declares.
template <typename Fn>
using RrdArgv =
    std::conditional_t<std::is_invocable_v<Fn, int, const char**>, const char**, char**>;
}  // namespace

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

    const int result = rrd_update(
        static_cast<int>(argv.size()),
        const_cast<RrdArgv<decltype(&rrd_update)>>(argv.data())
    );

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
    const int result = rrd_create(
        static_cast<int>(argv.size()),
        const_cast<RrdArgv<decltype(&rrd_create)>>(argv.data())
    );

    if (result != 0) {
        throw StorageException("Failed to create RRD file: " + std::string(rrd_get_error()));
    }
}

void SystemMetricStorage::ensureAccess() const {
    std::ofstream test(system_metrics_path_);

    if (!test.is_open()) {
        if (errno == EACCES) {
            throw PermissionDeniedException(system_metrics_path_);
        }
        throw StorageException("Cannot access path: " + system_metrics_path_);
    }

    test.close();
    std::filesystem::remove(system_metrics_path_);
}
