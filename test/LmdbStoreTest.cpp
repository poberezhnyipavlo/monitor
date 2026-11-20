#include <gtest/gtest.h>
#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/MetricProtocol.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "storage/LmdbStore.h"

namespace {

constexpr std::string_view kDev = "20:9b:a9:61:15:6c";
constexpr std::string_view kOtherDev = "20:9b:a9:61:15:6d";
constexpr std::size_t kTestMapSize = std::size_t{16} << 20U;  // 16 MiB

metric::Message metricLine(std::string_view name, double value, uint32_t seq, uint32_t boot = 1) {
    return metric::Message{kDev, boot, seq, 0, name, value};
}

// Each test gets its own empty LMDB file, removed afterwards.
class LmdbStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        file_ = std::filesystem::temp_directory_path() /
                ("monitor_test_" + std::string(info->name()) + ".lmdb");
        removeFiles();
    }

    void TearDown() override {
        removeFiles();
    }

    void removeFiles() const {
        std::filesystem::remove(file_);
        std::filesystem::remove(file_.string() + "-lock");
    }

    std::filesystem::path file_;
};

}  // namespace

TEST_F(LmdbStoreTest, StoresAndReadsSeries) {
    LmdbStore store(file_, kTestMapSize);
    EXPECT_EQ(store.putMetric(metricLine("temperature_c", 24.5, 0), 1000), PutResult::kStored);
    EXPECT_EQ(store.putMetric(metricLine("temperature_c", 25.0, 1), 3000), PutResult::kStored);

    const auto points = store.series(kDev, "temperature_c", 0, UINT64_MAX);
    ASSERT_EQ(points.size(), 2U);
    EXPECT_EQ(points[0].ts_ms, 1000U);
    EXPECT_DOUBLE_EQ(points[0].value, 24.5);
    EXPECT_EQ(points[1].ts_ms, 3000U);
    EXPECT_DOUBLE_EQ(points[1].value, 25.0);
}

TEST_F(LmdbStoreTest, SameDevBootSeqIsDuplicate) {
    LmdbStore store(file_, kTestMapSize);
    EXPECT_EQ(store.putMetric(metricLine("temperature_c", 24.5, 7), 1000), PutResult::kStored);
    // Resent after a reconnect: same identity, maybe a different age -> different time.
    EXPECT_EQ(store.putMetric(metricLine("temperature_c", 24.5, 7), 1500), PutResult::kDuplicate);

    EXPECT_EQ(store.series(kDev, "temperature_c", 0, UINT64_MAX).size(), 1U);  // series untouched
}

TEST_F(LmdbStoreTest, NewBootIsNotDuplicate) {
    LmdbStore store(file_, kTestMapSize);
    EXPECT_EQ(store.putMetric(metricLine("dht_ok", 1, 0, 0xaaaa), 1000), PutResult::kStored);
    EXPECT_EQ(store.putMetric(metricLine("dht_ok", 1, 0, 0xbbbb), 2000), PutResult::kStored);
}

TEST_F(LmdbStoreTest, PointsComeInTimeOrder) {
    LmdbStore store(file_, kTestMapSize);
    // Written out of order, across a byte boundary (255 / 256 / 65536).
    store.putMetric(metricLine("t", 3, 0), 65536);
    store.putMetric(metricLine("t", 1, 1), 255);
    store.putMetric(metricLine("t", 2, 2), 256);

    const auto points = store.series(kDev, "t", 0, UINT64_MAX);
    ASSERT_EQ(points.size(), 3U);
    EXPECT_EQ(points[0].ts_ms, 255U);
    EXPECT_EQ(points[1].ts_ms, 256U);
    EXPECT_EQ(points[2].ts_ms, 65536U);
}

TEST_F(LmdbStoreTest, RangeBoundsAreInclusive) {
    LmdbStore store(file_, kTestMapSize);
    for (uint32_t i = 0; i < 5; ++i) {
        store.putMetric(metricLine("t", i, i), 1000U * i);  // 0, 1000, ..., 4000
    }
    const auto points = store.series(kDev, "t", 1000, 3000);
    ASSERT_EQ(points.size(), 3U);
    EXPECT_EQ(points.front().ts_ms, 1000U);
    EXPECT_EQ(points.back().ts_ms, 3000U);
}

TEST_F(LmdbStoreTest, SeriesDoNotMix) {
    LmdbStore store(file_, kTestMapSize);
    store.putMetric(metricLine("temp", 1, 0), 1000);
    store.putMetric(metricLine("temp_alert", 0, 1), 1000);
    store.putMetric(metric::Message{kOtherDev, 1, 0, 0, "temp", 9}, 1000);

    const auto points = store.series(kDev, "temp", 0, UINT64_MAX);
    ASSERT_EQ(points.size(), 1U);
    EXPECT_DOUBLE_EQ(points[0].value, 1);
    EXPECT_TRUE(store.series(kDev, "missing", 0, UINT64_MAX).empty());
}

TEST_F(LmdbStoreTest, DataSurvivesReopen) {
    {
        LmdbStore store(file_, kTestMapSize);
        store.putMetric(metricLine("t", 1, 0), 1000);
    }
    LmdbStore reopened(file_, kTestMapSize);
    EXPECT_EQ(reopened.series(kDev, "t", 0, UINT64_MAX).size(), 1U);
    EXPECT_EQ(reopened.putMetric(metricLine("t", 1, 0), 1000), PutResult::kDuplicate);
}

TEST_F(LmdbStoreTest, LogsHaveTheirOwnSeqSpace) {
    LmdbStore store(file_, kTestMapSize);
    const logproto::Record log{kDev, 1, 0, 0, logproto::Level::kInfo, "boot", "started"};
    EXPECT_EQ(store.putLog(log, 1000), PutResult::kStored);
    EXPECT_EQ(store.putLog(log, 1000), PutResult::kDuplicate);
    // Metric seq 0 of the same boot is a different line.
    EXPECT_EQ(store.putMetric(metricLine("t", 1, 0), 1000), PutResult::kStored);
}

TEST_F(LmdbStoreTest, InvalidDevThrows) {
    LmdbStore store(file_, kTestMapSize);
    EXPECT_THROW(
        store.putMetric(metric::Message{"bad", 1, 0, 0, "t", 1}, 0),
        std::invalid_argument
    );
}