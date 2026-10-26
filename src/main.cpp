#include <netinet/in.h>
#include <sys/socket.h>

#include <csignal>
#include <iomanip>
#include <sstream>
#include <thread>

#include "core/Logger.h"
#include "metrics/FormatMetric.h"
#include "metrics/MetricFactory.h"
#include "network/Socket.h"
#include "storage/SystemMetricStorage.h"

std::atomic<bool> stop_requested{false};
std::atomic<bool> monitoring_failed{false};
std::atomic<bool> network_failed{false};

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
        monitoring_failed.store(true);
    } catch (const std::exception& e) {
        log("Monitoring loop failed: " + std::string(e.what()));
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

            const int bind_result =
                bind(sock.get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

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

    std::jthread monitoring_thread(monitoringLoop);

    std::jthread network_thread(networkLoop);

    while (!stop_requested.load()) {
        if (monitoring_failed.load() && network_failed.load()) {
            log("Both threads failed, shutting down");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    monitoring_thread.request_stop();

    return 0;
}