#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>

#include "metrics/metric_factory.h"

std::string formatMetric(const double value, const std::string_view unit) {
    if (isnan(value)) {
        return "n/a";
    }

    std::ostringstream oss;

    oss << std::fixed << std::setprecision(2) << value << " " << unit;

    return oss.str();
}

int main() {
    const auto metric = MetricFactory::make();

    std::cout << "CPU: " << formatMetric(metric->getCpuUsage(), "%") << std::endl;
    std::cout << "Memory: " << formatMetric(metric->getMemoryUsage(), "%") << std::endl;

    return 0;
}