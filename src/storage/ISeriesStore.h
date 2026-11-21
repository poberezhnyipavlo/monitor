#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct SeriesPoint {
    uint64_t ts_ms;
    double value;
};

struct SeriesId {
    std::string device;
    std::string name;

    bool operator==(const SeriesId&) const = default;
};

// Read side of a time-series storage. Two implementations: LmdbStore (node data) and
// RrdSeriesStore (this computer's CPU/RAM). The gRPC service only sees this interface.
class ISeriesStore {
public:
    virtual ~ISeriesStore() = default;

    // Every series with at least one point.
    [[nodiscard]] virtual std::vector<SeriesId> listSeries() const = 0;

    // Points with from_ms <= ts_ms <= to_ms, oldest first. Unknown series: empty.
    [[nodiscard]] virtual std::vector<SeriesPoint> series(
        std::string_view device,
        std::string_view name,
        uint64_t from_ms,
        uint64_t to_ms
    ) const = 0;
};
