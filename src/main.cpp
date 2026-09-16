#include <iostream>
#include "metrics/metric_factory.h"

using namespace std;

int main() {
    auto metric = MetricFactory::make();

    double cpu_usage = metric->getCpuUsage();
    if (std::isnan(cpu_usage)) {
        std::cout << "CPU: n/a" << std::endl;
    } else {
        std::cout << "CPU: " << cpu_usage << "%" << std::endl;
    }

    return 0;
}