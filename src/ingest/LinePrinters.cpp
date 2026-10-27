#include "LinePrinters.h"

#include <metric_protocol/MetricProtocol.h>
#include <metric_protocol/LogProtocol.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include "core/Logger.h"

namespace {
std::string formatBoot(uint32_t boot) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << boot;
    return out.str();
}
}

void printMetricLine(std::string_view line) {
    const auto message = metric::parse(line);
    if (!message) {
        log("[metrics] rejected: " + std::string(line));
        return;
    }

    std::ostringstream out;
    out << "[metrics] dev=" << message->dev << " boot=" << std::hex << message->boot << std::dec
        << " seq=" << message->seq << " " << message->name << "=" << message->value << " (age "
        << message->age_ms << " ms)";

    log(out.str());
}

void printLogLine(std::string_view line) {
    const auto record = logproto::parse(line);
    if (!record) {
        log("[metrics] rejected: " + std::string(line));
        return;
    }

    std::ostringstream out;
    out << "[logs] dev=" << record->dev << " boot=" << formatBoot(record->boot)
        << " seq=" << record->seq << " " << logproto::toString(record->level) << " "
        << record->src << ": " << record->text << " (age " << record->age_ms << " ms)";

    log(out.str());
}
