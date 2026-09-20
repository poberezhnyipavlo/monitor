#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>

inline std::string formatMetric(const double value, const std::string_view unit) {
    if (isnan(value)) {
        return "n/a";
    }

    std::ostringstream oss;

    oss << std::fixed << std::setprecision(2) << value << " " << unit;

    return oss.str();
}