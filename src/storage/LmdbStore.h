#pragma once

#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/MetricProtocol.h>

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include "lmdb/Lmdb.h"

enum class PutResult {
    kStored,
    kDuplicate, // a line with the same dev|boot|seq is already stored: nothing written
};

struct SeriesPoint {
    uint64_t ts_ms;
    double value;
};

// Data from nodes in one LMDB file, three tables:
//   samples: dev|boot|seq          -> ts_ms        identity of a metric line, rejects duplicates
//   series:  dev|name|0x00|ts_ms   -> value        one metric of one device ordered by time
//   logs:    dev|boot|seq          -> ts_ms|level|src|0x00|text
// samples and series are written in one transaction: both or neither.
// Safe to call from several threads: LMDB runs one write transaction at a time.
class LmdbStore {
public:
    // map_size: the most the file may grow to (address space is reserved, disk is used as data
    // grows).
    explicit LmdbStore(
        const std::filesystem::path& file,
        std::size_t map_size_bytes = kDefaultMapSize
    );

    // ts_ms: when the sample was taken, ms since the Unix epoch (receive time - age_ms).
    PutResult putMetric(const metric::Message& message, uint64_t ts_ms);
    PutResult putLog(const logproto::Record& record, uint64_t ts_ms);

    // Points of one series with from_ms <= ts_ms <= to_ms, oldest first.
    [[nodiscard]] std::vector<SeriesPoint> series(
        std::string_view dev,
        std::string_view name,
        uint64_t from_ms,
        uint64_t to_ms
    ) const;

    static constexpr std::size_t kDefaultMapSize = std::size_t{1} << 30U;  // 1 GiB
private:
    LmdbEnv env_;
    MDB_dbi samples_ = 0;
    MDB_dbi series_ = 0;
    MDB_dbi logs_ = 0;
};
