#include <sys/socket.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <thread>

#include "core/Logger.h"
#include "metrics/FormatMetric.h"
#include "metrics/MetricFactory.h"
#include "storage/SystemMetricStorage.h"

std::atomic<bool> stop_requested{false};

void signalHandler(int) { stop_requested.store(true); }

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

int main() {
    std::signal(SIGINT, signalHandler);

    std::jthread monitoring_thread(monitoringLoop);

    while (!stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    monitoring_thread.request_stop();

    return 0;
}