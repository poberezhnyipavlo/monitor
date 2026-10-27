#include <metric_protocol/MetricProtocol.h>
#include <metric_protocol/LogProtocol.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <thread>

#include "core/Logger.h"
#include "ingest/LinePrinters.h"
#include "metrics/FormatMetric.h"
#include "metrics/MetricFactory.h"
#include "network/TcpLineReceiver.h"
#include "storage/SystemMetricStorage.h"

namespace {
constexpr uint16_t kMetricsPort = 9002;
constexpr uint16_t kLogsPort = 9003;
constexpr std::chrono::seconds kMetricsIdleTimeout{10};
}  // namespace

std::atomic<bool> stop_requested{false};
std::atomic<bool> receiver_failed{false};

void signalHandler(int) {
    stop_requested.store(true);
}

void monitoringLoop(std::stop_token token) {
    try {
        const auto metric = MetricFactory::make();
        SystemMetricStorage storage("system_metrics.rrd");

        while (!token.stop_requested()) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            const std::tm* now_tm = std::localtime(&now_time);

            const auto cpu = metric->getCpuUsage();
            const auto memory = metric->getMemoryUsage();
            std::ostringstream output;
            output << "[" << std::put_time(now_tm, "%H:%M:%S") << "] "
                   << "CPU: " << formatMetric(cpu, "%") << " | "
                   << "Memory: " << formatMetric(memory, "%");

            log(output.str());

            storage.save(cpu, memory);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (PermissionDeniedException& e) {
        log("FATAL (permissions): " + std::string(e.what()));
    } catch (const std::exception& e) {
        log("Monitoring loop failed: " + std::string(e.what()));
    }
}

void receiverLoop(std::stop_token stop, ILineReceiver& receiver, const LineHandler& on_line) {
    try {
        receiver.run(stop, on_line);
    } catch (const std::exception& e) {
        log("FATAL (receiver): " + std::string(e.what()));
        receiver_failed.store(true);
        stop_requested.store(true);
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    TcpLineReceiver metrics_receiver(
        "metrics",
        {
            .port = kMetricsPort,
            .max_line_len = metric::kMaxMessageLen,
            .idle_timeout = kMetricsIdleTimeout,
        }
    );

    TcpLineReceiver logs_receiver(
        "logs",
        {
            .port = kLogsPort,
            .max_line_len = logproto::kMaxLineLen,
            .idle_timeout = std::nullopt,
        }
    );

    std::jthread monitoring_thread(monitoringLoop);
    std::jthread metric_thread(
        receiverLoop,
        std::ref(metrics_receiver),
        LineHandler(printMetricLine)
    );
    std::jthread logs_thread(receiverLoop, std::ref(logs_receiver), LineHandler(printLogLine));

    while (!stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    log("Stopping");

    monitoring_thread.request_stop();
    metric_thread.request_stop();
    logs_thread.request_stop();

    return receiver_failed.load() ? 1 : 0;
}