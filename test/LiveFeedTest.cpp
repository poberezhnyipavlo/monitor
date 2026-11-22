#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "api/LiveFeed.h"

namespace {

using std::chrono::milliseconds;

LivePoint point(double value) {
    return {
        .id = {.device = "20:9b:a9:61:15:6c", .name = "t"},
        .point = {.ts_ms = 1, .value = value}
    };
}

std::vector<double> values(const std::vector<LivePoint>& points) {
    std::vector<double> out;
    for (const LivePoint& p : points) {
        out.push_back(p.point.value);
    }
    return out;
}

}  // namespace

TEST(LiveFeed, NewReaderGetsOnlyLaterPoints) {
    LiveFeed feed;
    feed.publish(point(1));
    uint64_t cursor = feed.cursor();
    feed.publish(point(2));
    feed.publish(point(3));

    EXPECT_EQ(values(feed.waitNewer(cursor, milliseconds(0))), (std::vector<double>{2, 3}));
    EXPECT_TRUE(feed.waitNewer(cursor, milliseconds(0)).empty());  // nothing new since
}

TEST(LiveFeed, TimeoutWithoutPoints) {
    LiveFeed feed;
    uint64_t cursor = feed.cursor();
    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(feed.waitNewer(cursor, milliseconds(50)).empty());
    EXPECT_GE(std::chrono::steady_clock::now() - start, milliseconds(50));
}

TEST(LiveFeed, PublishWakesWaitingReader) {
    LiveFeed feed;
    uint64_t cursor = feed.cursor();
    std::thread writer([&feed] {
        std::this_thread::sleep_for(milliseconds(20));
        feed.publish(point(7));
    });
    const auto start = std::chrono::steady_clock::now();
    const auto got = feed.waitNewer(cursor, milliseconds(5000));
    writer.join();
    EXPECT_EQ(values(got), (std::vector<double>{7}));
    EXPECT_LT(
        std::chrono::steady_clock::now() - start,
        milliseconds(1000)
    );  // woken, not timed out
}

TEST(LiveFeed, CloseWakesWaitingReader) {
    LiveFeed feed;
    uint64_t cursor = feed.cursor();
    std::thread closer([&feed] {
        std::this_thread::sleep_for(milliseconds(20));
        feed.close();
    });
    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(feed.waitNewer(cursor, milliseconds(5000)).empty());
    closer.join();
    EXPECT_TRUE(feed.closed());
    EXPECT_LT(std::chrono::steady_clock::now() - start, milliseconds(1000));
}

TEST(LiveFeed, SlowReaderLosesOnlyTheOldest) {
    LiveFeed feed;
    uint64_t cursor = feed.cursor();
    const auto total = static_cast<int>(LiveFeed::kKeep) + 5;
    for (int i = 1; i <= total; ++i) {
        feed.publish(point(i));
    }
    const auto got = feed.waitNewer(cursor, milliseconds(0));
    ASSERT_EQ(got.size(), LiveFeed::kKeep);
    EXPECT_DOUBLE_EQ(got.front().point.value, 6);  // 1..5 dropped
    EXPECT_DOUBLE_EQ(got.back().point.value, total);
}