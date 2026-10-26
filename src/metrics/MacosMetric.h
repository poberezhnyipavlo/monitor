#pragma once

#include <optional>

#include "ISystemMetrics.h"

struct TickMacOS {
    uint64_t user;
    uint64_t system;
    uint64_t idle;
    uint64_t nice;
};

class MacOSMetric : public ISystemMetrics {
public:
    MacOSMetric() : prev_tick_(getTicks()) {
        // Перший вимір у конструкторі свідомо "прогріває" стан, щоб getCpuUsage()
        // одразу мав з чим порівнювати. Точність цього першого виміру не критична —
        // короткий інтервал між конструктором і першим getCpuUsage() дає шумний
        // результат через обмежену роздільність системних лічильників тіків.
    }

    double getCpuUsage() override;
    double getMemoryUsage() override;
private:
    [[nodiscard]]std::optional<TickMacOS> getTicks() const;
    std::optional<TickMacOS> prev_tick_;
};
