#include <iostream>
#include "metrics/metric_factory.h"

using namespace std;

int main() {
    auto metric = MetricFactory::make();

    cout << metric->getCpuUsage() << endl;
    return 0;
}