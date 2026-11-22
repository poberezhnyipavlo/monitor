#include "MonitorService.h"

#include <storage/RrdSeriesStore.h>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>

#include "core/Logger.h"

namespace {

uint64_t nowMs() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    return static_cast<uint64_t>(now.count());
}

// How often a waiting stream checks whether its client is still there.
constexpr std::chrono::milliseconds kWatchPoll{500};

// Empty device or name in the filter matches any.
bool matches(const monitor::v1::SeriesId& filter, const SeriesId& id) {
    return (filter.device().empty() || filter.device() == id.device) &&
           (filter.name().empty() || filter.name() == id.name);
}

}  // namespace

MonitorService::MonitorService(const ISeriesStore& nodes, const ISeriesStore& host, LiveFeed& feed)
    : nodes_(nodes), host_(host), feed_(feed) {}

grpc::Status MonitorService::ListSeries(
    grpc::ServerContext* /*context*/,
    const monitor::v1::ListSeriesRequest* /*request*/,
    monitor::v1::ListSeriesResponse* response
) {
    try {
        for (const ISeriesStore* store : {&host_, &nodes_}) {
            for (const SeriesId& id : store->listSeries()) {
                monitor::v1::SeriesId* out = response->add_series();
                out->set_device(id.device);
                out->set_name(id.name);
            }
        }
        return grpc::Status::OK;
    } catch (std::exception& e) {
        return {grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status MonitorService::GetSeries(
    grpc::ServerContext* /*context*/,
    const monitor::v1::GetSeriesRequest* request,
    monitor::v1::GetSeriesResponse* response
) {
    const uint64_t to_ms = request->to_ms() == 0 ? nowMs() : request->to_ms();
    const std::string_view device = request->id().device();
    const std::string_view name = request->id().name();
    const uint64_t from_ms = request->from_ms();

    if (device.empty() || name.empty() || from_ms > to_ms) {
        return {grpc::StatusCode::INVALID_ARGUMENT, "need id.device, id.name, from_ms <= to_ms"};
    }

    try {
        for (const SeriesPoint& p : storeFor(device).series(device, name, from_ms, to_ms)) {
            monitor::v1::Point* point = response->add_points();
            point->set_ts_ms(p.ts_ms);
            point->set_value(p.value);
        }
        return grpc::Status::OK;
    } catch (const std::invalid_argument& e) {
        return {grpc::StatusCode::INVALID_ARGUMENT, e.what()};
    } catch (const std::exception& e) {
        return {grpc::StatusCode::INTERNAL, e.what()};
    }
}

grpc::Status MonitorService::WatchLatest(
    grpc::ServerContext* context,
    const monitor::v1::WatchLatestRequest* request,
    grpc::ServerWriter<monitor::v1::LivePoint>* writer
) {
    log("[api] watch started: " + context->peer());
    uint64_t cursor = feed_.cursor();
    monitor::v1::LivePoint out;

    // IsCancelled: the client closed the call or went away. Without this check the loop would keep
    // a server thread forever.
    while (!context->IsCancelled()) {
        const std::vector<LivePoint> points = feed_.waitNewer(cursor, kWatchPoll);
        if (feed_.closed()) {
            log("[api] watch ended: server is stopping");
            return {grpc::StatusCode::UNAVAILABLE, "server is stopping"};
        }
        for (const LivePoint& p : points) {
            if (!matches(request->id(), p.id)) {
                continue;
            }
            out.mutable_id()->set_device(p.id.device);
            out.mutable_id()->set_name(p.id.name);
            out.mutable_point()->set_ts_ms(p.point.ts_ms);
            out.mutable_point()->set_value(p.point.value);
            if (!writer->Write(out)) {  // false: the stream is broken, the client is gone
                log("[api] watch ended: client gone");
                return grpc::Status::OK;
            }
        }
    }
    log("[api] watch ended: canceled by client");
    return grpc::Status::CANCELLED;
}

const ISeriesStore& MonitorService::storeFor(std::string_view device) const {
    return device == RrdSeriesStore::kDevice ? host_ : nodes_;
}
