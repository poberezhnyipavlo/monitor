#pragma once

#include <grpcpp/grpcpp.h>

#include "LiveFeed.h"
#include "monitor/v1/monitor.grpc.pb.h"
#include "storage/ISeriesStore.h"

// gRPC handlers. Each call runs on a thread of the gRPC server, so the stores must be safe to read
// from several threads (LMDB read transactions, RRD under rrdMutex).
// Series of device "host" come from `host`, every other device from `nodes`.
class MonitorService final : public monitor::v1::MonitorService::Service {
public:
    MonitorService(const ISeriesStore& nodes, const ISeriesStore& host, LiveFeed& feed);

    grpc::Status ListSeries(
        grpc::ServerContext* context,
        const monitor::v1::ListSeriesRequest* request,
        monitor::v1::ListSeriesResponse* response
    ) override;

    grpc::Status GetSeries(
        grpc::ServerContext* context,
        const monitor::v1::GetSeriesRequest* request,
        monitor::v1::GetSeriesResponse* response
    ) override;

    grpc::Status WatchLatest(
        grpc::ServerContext* context,
        const monitor::v1::WatchLatestRequest* request,
        grpc::ServerWriter<monitor::v1::LivePoint>* writer
    ) override;

private:
    [[nodiscard]] const ISeriesStore& storeFor(std::string_view device) const;

    const ISeriesStore& nodes_;
    const ISeriesStore& host_;
    LiveFeed& feed_;
};
