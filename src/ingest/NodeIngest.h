#pragma once

#include <storage/LmdbStore.h>

#include <string_view>

#include "api/LiveFeed.h"

// Lines from nodes: parse, store, print. One object for both ports; the receivers call it
// from their own threads (LmdbStore is thread-safe, printing goes through log()).
class NodeIngest {
public:
    explicit NodeIngest(LmdbStore& store, LiveFeed& feed);

    void onMetricLine(std::string_view line);
    void onLogLine(std::string_view line);

private:
    LmdbStore& store_;
    LiveFeed& feed_;
};
