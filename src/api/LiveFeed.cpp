#include "LiveFeed.h"

void LiveFeed::publish(LivePoint point) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recent_.push_back(std::move(point));
        if (recent_.size() > kKeep) {
            recent_.pop_front();
        }

        ++last_number_;
    }

    changed_.notify_all();
}

uint64_t LiveFeed::cursor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_number_;
}

std::vector<LivePoint> LiveFeed::waitNewer(uint64_t& cursor, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait_for(lock, timeout, [&] {
        return closed_ || last_number_ > cursor;
    });

    if (closed_ || last_number_ <= cursor) {
        return {};
    }

    const uint64_t first_kept = last_number_ - recent_.size() + 1;
    const uint64_t from = std::max(cursor + 1, first_kept);
    std::vector<LivePoint> out(recent_.begin() + static_cast<std::ptrdiff_t>(from - first_kept), recent_.end());
    cursor = last_number_;

    return out;
}

void LiveFeed::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    changed_.notify_all();
}

bool LiveFeed::closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}