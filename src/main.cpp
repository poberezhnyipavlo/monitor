#include <grpc/grpc.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/MetricProtocol.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>

#include "api/MonitorService.h"
#include "core/Logger.h"
#include "ingest/NodeIngest.h"
#include "metrics/FormatMetric.h"
#include "metrics/MetricFactory.h"
#include "network/TcpLineReceiver.h"
#include "storage/LmdbStore.h"
#include "storage/RrdSeriesStore.h"
#include "storage/SystemMetricStorage.h"

namespace {
constexpr uint16_t kMetricsPort = 9002;
constexpr uint16_t kLogsPort = 9003;
constexpr std::chrono::seconds kMetricsIdleTimeout{10};
constexpr const char* kNodeDataFile = "node_data.lmdb";
constexpr const char* kSystemMetricsFile = "system_metrics.rrd";
// Only this computer: the API has no authentication (README).
constexpr const char* kApiAddress = "127.0.0.1:50051";
constexpr const char* kHmacKeyEnv = "MONITOR_HMAC_KEY";
}  // namespace

std::atomic<bool> stop_requested{false};
std::atomic<bool> receiver_failed{false};

void signalHandler(int) {
    stop_requested.store(true);
}

void monitoringLoop(std::stop_token token) {
    try {
        const auto metric = MetricFactory::make();
        SystemMetricStorage storage(kSystemMetricsFile);

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

    std::optional<LmdbStore> store;
    try {
        store.emplace(kNodeDataFile);
    } catch (const std::exception& e) {
        log("FATAL (storage): " + std::string(e.what()));
        return 1;
    }

    // Shared key: 64 hex digits, the same as kHmacKeyHex in the node's secrets.h. From the
    // environment, so it is never in the repository (`openssl rand -hex 32`).
    const char* key_hex = std::getenv(kHmacKeyEnv);
    const auto key = sig::parseKey(key_hex == nullptr ? "" : key_hex);
    if (!key) {
        log(std::string("FATAL (hmac): set ") + kHmacKeyEnv + " to 64 lowercase hex digits");
        return 1;
    }
    const HmacSha256 hmac(*key);

    LiveFeed live_feed;
    NodeIngest ingest(*store, live_feed, hmac);

    RrdSeriesStore host_store(kSystemMetricsFile);
    MonitorService api(*store, host_store, live_feed);
    // Reflection: grpcurl/Postman can list the API without the .proto file.
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    builder.AddListeningPort(kApiAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&api);
    const std::unique_ptr<grpc::Server> api_server = builder.BuildAndStart();
    if (!api_server) {
        log(std::string("FATAL (api): cannot listen on  ") + kApiAddress);
        return 1;
    }
    log(std::string("[api] gRPC on ") + kApiAddress);

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
        LineHandler([&ingest](std::string_view line) { ingest.onMetricLine(line); })
    );
    std::jthread logs_thread(
        receiverLoop,
        std::ref(logs_receiver),
        LineHandler([&ingest](std::string_view line) { ingest.onLogLine(line); })
    );

    while (!stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    log("Stopping");

    // Wake the WatchLatest streams first: a stream waiting for points would hold Shutdown back.
    live_feed.close();
    // Calls in progress get up to 1 s to finish, then are cancelled.
    api_server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));

    monitoring_thread.request_stop();
    metric_thread.request_stop();
    logs_thread.request_stop();

    return receiver_failed.load() ? 1 : 0;
}