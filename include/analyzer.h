#ifndef ANALYZER_H
#define ANALYZER_H

#include "collector.h"

typedef struct {
    double mean;
    double stddev;
    int anomalyCount;
} AnalysisResult;

typedef struct {
    double p50;
    double p90;
    double p95;
    double p99;
} Percentiles;

typedef struct {
    char   query[QUERY_LEN];
    double avgMS;
    double minMS;
    double maxMS;
    int    count;
    int    anomalies;
} QueryStat;

typedef struct {
    int    threadID;
    int    count;
    int    anomalies;
    double avgMS;
} ThreadStat;

AnalysisResult analyze_metrics(MetricsBuffer* buf);
int is_anomaly(double durationMS, AnalysisResult* result);

Percentiles calc_percentiles(MetricsBuffer *buf);
void calc_query_stats(MetricsBuffer *buf, AnalysisResult *result, QueryStat *stats, int *num_stats);
void calc_thread_stats(MetricsBuffer *buf, AnalysisResult *result, ThreadStat *stats, int *num_stats);
QueryMetric find_slowest(MetricsBuffer *buf);
QueryMetric find_fastest(MetricsBuffer *buf);

#endif // ANALYZER_H