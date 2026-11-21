#pragma once

#include <string>

#include "ISeriesStore.h"

class RrdSeriesStore : public ISeriesStore {
public:
    static constexpr const char* kDevice = "host";

    explicit RrdSeriesStore(std::string path);

    [[nodiscard]] std::vector<SeriesId> listSeries() const override;

    [[nodiscard]] std::vector<SeriesPoint> series(
        std::string_view device,
        std::string_view name,
        uint64_t from_ms,
        uint64_t to_ms
    ) const override;

private:
    std::string path_;
};
