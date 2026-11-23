#pragma once

#include <storage/LmdbStore.h>

#include <string_view>

#include "api/LiveFeed.h"
#include "security/HmacSha256.h"

// Lines from nodes: parse, store, print. One object for both ports; the receivers call it
// from their own threads (LmdbStore is thread-safe, printing goes through log()).
class NodeIngest {
public:
    explicit NodeIngest(LmdbStore& store, LiveFeed& feed, const HmacSha256& hmac);

    void onMetricLine(std::string_view line);
    void onLogLine(std::string_view line);

private:
    // The payload of a correctly signed line; nullopt (and a log line) otherwise.
    [[nodiscard]] std::optional<std::string_view> verified(
        std::string_view line,
        std::string_view channel
    );

    LmdbStore& store_;
    LiveFeed& feed_;
    const HmacSha256& hmac_;
};
