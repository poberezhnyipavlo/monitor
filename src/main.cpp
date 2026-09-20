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
            const int fd = socket(AF_INET, SOCK_DGRAM, 0);
            Socket sock(fd);

            sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(9000);

            const int bind_result = bind(
                sock.get(),
                reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)
            );

            if (bind_result < 0) {
                throw std::runtime_error("Failed to bind socket");
            }

            while (!token.stop_requested()) {
                char buffer[1024];

                sockaddr_in sender_addr{};
                socklen_t sender_len = sizeof(sender_addr);

                const ssize_t bytes_received = recvfrom(
                    sock.get(),
                    buffer,
                    sizeof(buffer),
                    0,
                    reinterpret_cast<sockaddr*>(&sender_addr),
                    &sender_len
                );

                if (bytes_received < 0) {
                    throw std::runtime_error("Failed to receive data");
                }

                const std::string message(buffer, bytes_received);
                log("Received: " + message);

            }
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