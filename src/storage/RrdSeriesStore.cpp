#include "RrdSeriesStore.h"

#include <rrd.h>

#include <array>
#include <cmath>
#include <ctime>
#include <mutex>
#include <string_view>
#include <utility>

#include "RrdMutex.h"
#include "StorageExceptions.h"

namespace {

constexpr std::array<std::string_view, 2> kNames = {"cpu", "memory"};  // DS names in the RRD file
constexpr uint64_t kMsPerSecond = 1000;

// What rrd_fetch_r allocates with malloc; freed in the destructor.
struct FetchResult {
    unsigned long ds_count = 0;
    char** ds_names = nullptr;
    rrd_value_t* data = nullptr;

    FetchResult() = default;
    FetchResult(const FetchResult&) = delete;
    FetchResult& operator=(const FetchResult&) = delete;
    FetchResult(FetchResult&&) = delete;
    FetchResult& operator=(FetchResult&&) = delete;

    ~FetchResult() {
        for (unsigned long i = 0; i < ds_count && ds_names != nullptr; ++i) {
            rrd_freemem(ds_names[i]);
        }
        rrd_freemem(ds_names);
        rrd_freemem(data);
    }
};

}  // namespace

RrdSeriesStore::RrdSeriesStore(std::string path) : path_(path) {}

std::vector<SeriesId> RrdSeriesStore::listSeries() const {
    std::vector<SeriesId> ids;
    for (const std::string_view name : kNames) {
        ids.push_back({.device = kDevice, .name = std::string(name)});
    }

    return ids;
}

std::vector<SeriesPoint> RrdSeriesStore::series(
    std::string_view device,
    std::string_view name,
    uint64_t from_ms,
    uint64_t to_ms
) const {
    if (device != kDevice) {
        return {};
    }

    auto start = static_cast<time_t>(from_ms / kMsPerSecond);
    auto end = static_cast<time_t>(to_ms / kMsPerSecond);
    unsigned long step = 1;
    FetchResult result;
    {
        // The monitoring thread writes the same file with rrd_update.
        std::lock_guard<std::mutex> lock(rrdMutex());
        rrd_clear_error();
        const int rc = rrd_fetch_r(
            path_.c_str(),
            "AVERAGE",
            &start,
            &end,
            &step,
            &result.ds_count,
            &result.ds_names,
            &result.data
        );
        if (rc != 0) {
            throw StorageException("rrd_fetch " + path_ + ": " + rrd_get_error());
        }
    }

    // Column of the requested series; RRD stores all series of a row together.
    unsigned long column = result.ds_count;
    for (unsigned long i = 0; i < result.ds_count; ++i) {
        if (name == result.ds_names[i]) {
            column = i;
        }
    }

    if (column == result.ds_count) {
        return {};
    }

    // RRD may widen [start, end] to whole steps; row i covers the step ending at start + (i + 1) *
    // step.
    std::vector<SeriesPoint> points;
    const auto rows = static_cast<unsigned long>(end - start) / step;
    for (unsigned long row = 0; row < rows; ++row) {
        const double value = result.data[(row * result.ds_count) + column];
        const auto ts_ms = (static_cast<uint64_t>(start) + ((row + 1) * step)) * kMsPerSecond;
        if (std::isnan(value) || ts_ms < from_ms || ts_ms > to_ms) {
            continue;  // NaN: no sample in that step
        }
        points.push_back({.ts_ms = ts_ms, .value = value});
    }

    return points;
}