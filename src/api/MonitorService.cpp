#include "MonitorService.h"

#include <storage/RrdSeriesStore.h>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

uint64_t nowMs() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    return static_cast<uint64_t>(now.count());
}

}  // namespace

MonitorService::MonitorService(const ISeriesStore& nodes, const ISeriesStore& host)
    : nodes_(nodes), host_(host) {}

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

const ISeriesStore& MonitorService::storeFor(std::string_view device) const {
    return device == RrdSeriesStore::kDevice ? host_ : nodes_;
}
