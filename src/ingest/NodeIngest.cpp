#include "NodeIngest.h"

#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/MetricProtocol.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

#include "core/Logger.h"
#include "security/HmacSha256.h"

namespace {

// boot as on the wire: always 8 hex digits (0x1a -> "0000001a").
std::string formatBoot(uint32_t boot) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << boot;
    return out.str();
}

// When the sample was taken: now on this computer minus how long it waited on the node.
uint64_t sampleTimeMs(uint32_t age_ms) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    return static_cast<uint64_t>(now.count()) - age_ms;
}

const char* suffix(PutResult result) {
    return result == PutResult::kDuplicate ? " duplicate, skipped" : "";
}

std::string printable(std::string_view line) {
    std::string out;
    out.reserve(line.size());
    for (const auto& c : line) {
        const auto byte = static_cast<unsigned char>(c);
        out += (byte >= 0x20 && byte < 0x7f) ? c : '?';
    }

    return out;
}

}  // namespace

NodeIngest::NodeIngest(LmdbStore& store, LiveFeed& feed, const HmacSha256& hmac)
    : store_(store), feed_(feed), hmac_(hmac) {}

void NodeIngest::onMetricLine(std::string_view line) {
    const auto payload = verified(line, "metrics");
    if (!payload) {
        return;
    }
    const auto message = metric::parse(*payload);
    if (!message) {
        log("[metrics] rejected: " + std::string(*payload));
        return;
    }

    try {
        const uint64_t ts_ms = sampleTimeMs(message->age_ms);
        const PutResult result = store_.putMetric(*message, ts_ms);
        if (result == PutResult::kStored) {
            feed_.publish({
                .id = {.device = std::string(message->dev), .name = std::string(message->name)},
                .point = {.ts_ms = ts_ms, .value = message->value},
            });
        }
        std::ostringstream out;
        out << "[metrics] dev=" << message->dev << " boot=" << formatBoot(message->boot)
            << " seq=" << message->seq << " " << message->name << "=" << message->value << " (age "
            << message->age_ms << " ms)" << suffix(result);
        log(out.str());
    } catch (const std::exception& e) {
        log("[metrics] not stored: " + std::string(e.what()));
    }
}

void NodeIngest::onLogLine(std::string_view line) {
    const auto payload = verified(line, "logs");
    if (!payload) {
        return;
    }
    const auto record = logproto::parse(*payload);
    if (!record) {
        log("[logs] rejected: " + std::string(*payload));
        return;
    }

    try {
        const PutResult result = store_.putLog(*record, sampleTimeMs(record->age_ms));

        std::ostringstream out;
        out << "[logs] dev=" << record->dev << " boot=" << formatBoot(record->boot)
            << " seq=" << record->seq << " " << logproto::toString(record->level) << " "
            << record->src << ": " << record->text << " (age " << record->age_ms << " ms)"
            << suffix(result);
        log(out.str());
    } catch (const std::exception& e) {
        log("[logs] not stored: " + std::string(e.what()));
    }
}

std::optional<std::string_view> NodeIngest::verified(
    std::string_view line,
    std::string_view channel
) {
    const CheckedLine checked = checkLine(line, hmac_);
    switch (checked.result) {
        case LineCheck::kOk:
            return checked.payload;
        case LineCheck::kUnsigned:
            log("[" + std::string(channel) + "] rejected, unsigned: " + printable(line));
            return std::nullopt;
        case LineCheck::kBadSignature:
            log("[" + std::string(channel) + "] rejected, bad signature: " + printable(line));
            return std::nullopt;
    }

    return std::nullopt;
}