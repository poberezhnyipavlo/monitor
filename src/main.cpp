#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>
#include <csignal>
#include <thread>

#include "metrics/metric_factory.h"

std::atomic<bool> g_stopRequested{false};

void signalHandler(int) {
    g_stopRequested.store(true);
}

std::string formatMetric(const double value, const std::string_view unit) {
    if (isnan(value)) {
        return "n/a";
    }

    std::ostringstream oss;

    oss << std::fixed << std::setprecision(2) << value << " " << unit;

    return oss.str();
}

void monitoringLoop(std::stop_token token) {
    const auto metric = MetricFactory::make();

    while (!token.stop_requested()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        const std::tm* now_tm = std::localtime(&now_time);

        std::cout
            << "[" << std::put_time(now_tm, "%H:%M:%S") << "] "
            << "CPU: " << formatMetric(metric->getCpuUsage(), "%")
            << " | "
            << "Memory: " << formatMetric(metric->getMemoryUsage(), "%")
            << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::jthread monitoringThread(monitoringLoop);

    while (!g_stopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    monitoringThread.request_stop();

    return 0;
}