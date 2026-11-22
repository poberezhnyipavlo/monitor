#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "storage/ISeriesStore.h"

struct LivePoint {
    SeriesId id;
    SeriesPoint point;
};

// Hands new points from the receiver thread (publish) to the gRPC stream threads (waitNewer).
// Every point gets a number; a reader keeps the number of the last point it has seen (its cursor)
// and asks for everything after it. The feed keeps only the last kKeep points: a reader that
// falls further behind loses the oldest ones instead of making the feed grow without limit.
class LiveFeed {
public:
    static constexpr std::size_t kKeep = 1000;

    void publish(LivePoint point);

    // Cursor for a new reader: it will get only points published after this call.
    [[nodiscard]] uint64_t cursor() const;

    // Points after `cursor`, oldest first; moves `cursor` past them. Waits up to `timeout` if there
    // are none yet. Empty result: timeout or the feed was closed.
    [[nodiscard]] std::vector<LivePoint> waitNewer(
        uint64_t& cursor,
        std::chrono::milliseconds timeout
    );

    // Wakes every waiting reader; waitNewer returns at once from now on. Call before stopping the
    // server.
    void close();

    [[nodiscard]] bool closed() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<LivePoint> recent_;  // points last_number_ - size() + 1 ... last_number_
    uint64_t last_number_ = 0;
    bool closed_ = false;
};
