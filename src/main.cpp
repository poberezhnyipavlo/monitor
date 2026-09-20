#include <iomanip>
#include <sstream>
#include <csignal>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>

#include "metrics/metric_factory.h"
#include "network/socket.h"
#include "core/logger.h"
#include "metrics/format_metric.h"

std::atomic<bool> g_stopRequested{false};

void signalHandler(int) {
    g_stopRequested.store(true);
}

void monitoringLoop(std::stop_token token) {
    const auto metric = MetricFactory::make();

    while (!token.stop_requested()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        const std::tm* now_tm = std::localtime(&now_time);

        std::ostringstream output;
        output
            << "[" << std::put_time(now_tm, "%H:%M:%S") << "] "
            << "CPU: " << formatMetric(metric->getCpuUsage(), "%")
            << " | "
            << "Memory: " << formatMetric(metric->getMemoryUsage(), "%");

        log(output.str());

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void networkLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        try {
            const int fd = socket(AF_INET, SOCK_STREAM, 0);
            Socket sock(fd);
        } catch (const std::exception& e) {
            log("Failed to create socket: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::jthread monitoringThread(monitoringLoop);

    std::jthread networkThread(networkLoop);

    while (!g_stopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    monitoringThread.request_stop();

    return 0;
}