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

}  // namespace

NodeIngest::NodeIngest(LmdbStore& store) : store_(store) {}

void NodeIngest::onMetricLine(std::string_view line) {
    const auto message = metric::parse(line);
    if (!message) {
        log("[metrics] rejected: " + std::string(line));
        return;
    }

    try {
        const PutResult result = store_.putMetric(*message, sampleTimeMs(message->age_ms));

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
    const auto record = logproto::parse(line);
    if (!record) {
        log("[metrics] rejected: " + std::string(line));
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
        log("[metrics] not stored: " + std::string(e.what()));
    }
}
