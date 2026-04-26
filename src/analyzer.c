#include <math.h>
#include <stdlib.h>
#include "analyzer.h"

AnalysisResult analyze_metrics(MetricsBuffer* buf) {
    AnalysisResult result = {0.0, 0.0, 0};

    pthread_mutex_lock(&buf->lock);
    int n = buf->count;

    if (n == 0) {
        pthread_mutex_unlock(&buf->lock);
        return result;
    }

    // Calculate mean
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += buf->entries[i].duration_ms;
    }
    result.mean = sum / n;

    // Calculate standard deviation
    double variance_sum = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = buf->entries[i].duration_ms - result.mean;
        variance_sum += diff * diff;
    }
    result.stddev = sqrt(variance_sum / n);

    // Count anomalies
    for (int i = 0; i < n; i++) {
        if (buf->entries[i].duration_ms > result.mean + 2.0 * result.stddev) {
            result.anomalyCount++;
        }
    }

    pthread_mutex_unlock(&buf->lock);
    return result;
}

int is_anomaly(double durationMS, AnalysisResult* result) {
    return durationMS > result->mean + 2.0 * result->stddev;
}