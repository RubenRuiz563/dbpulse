#ifndef ANALYZER_H
#define ANALYZER_H

#include "collector.h"

typedef struct {
    double mean;
    double stddev;
    int anomalyCount;
} AnalysisResult;

AnalysisResult analyze_metrics(MetricsBuffer* buf);
int is_anomaly(double durationMS, AnalysisResult* result);

#endif // ANALYZER_H