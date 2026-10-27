#include "LinePrinters.h"

#include <metric_protocol/MetricProtocol.h>

#include <sstream>
#include <string>

#include "core/Logger.h"

void printMetricLine(std::string_view line) {
    const auto message = metric::parse(line);
    if (!message) {
        log("[metrics] rejected: " + std::string(line));
        return;
    }

    std::ostringstream out;
    out << "[metrics] dev=" << message->dev << " boot=" << std::hex << message->boot << std::dec
        << " seq=" << message->seq << " " << message->name << "=" << message->value << "(age "
        << message->age_ms << " ms)";

    log(out.str());
}

void printLogLine(std::string_view line) { log("[logs] " + std::string(line)); }
