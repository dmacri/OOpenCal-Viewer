#include "PerformanceMetrics.h"

PerformanceMetrics& PerformanceMetrics::instance()
{
    static PerformanceMetrics singleton;
    return singleton;
}
