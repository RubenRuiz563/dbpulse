#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

// Comparison function for qsort
static int cmp_double(const void *a, const void *b) {
    double da = *(double *)a;
    double db = *(double *)b;
    return (da > db) - (da < db);
}

Percentiles calc_percentiles(MetricsBuffer *buf) {
    Percentiles p = {0};
    pthread_mutex_lock(&buf->lock);
    int n = buf->count;
    if (n == 0) { pthread_mutex_unlock(&buf->lock); return p; }

    double *sorted = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) sorted[i] = buf->entries[i].duration_ms;
    pthread_mutex_unlock(&buf->lock);

    qsort(sorted, n, sizeof(double), cmp_double);

    p.p50 = sorted[(int)(n * 0.50)];
    p.p90 = sorted[(int)(n * 0.90)];
    p.p95 = sorted[(int)(n * 0.95)];
    p.p99 = sorted[(int)(n * 0.99)];

    free(sorted);
    return p;
}

void calc_query_stats(MetricsBuffer *buf, AnalysisResult *result, QueryStat *stats, int *num_stats) {
    *num_stats = 0;
    pthread_mutex_lock(&buf->lock);

    for (int i = 0; i < buf->count; i++) {
        QueryMetric *m = &buf->entries[i];

        // Find or create a slot for this query text
        int found = -1;
        for (int j = 0; j < *num_stats; j++) {
            if (strncmp(stats[j].query, m->query, 40) == 0) {
                found = j; break;
            }
        }
        if (found == -1) {
            found = *num_stats;
            strncpy(stats[found].query, m->query, QUERY_LEN - 1);
            stats[found].avgMS   = 0;
            stats[found].maxMS   = 0;
            stats[found].minMS   = 1e9;
            stats[found].count    = 0;
            stats[found].anomalies = 0;
            (*num_stats)++;
        }

        QueryStat *s = &stats[found];
        s->avgMS += m->duration_ms;
        s->count++;
        if (m->duration_ms > s->maxMS) s->maxMS = m->duration_ms;
        if (m->duration_ms < s->minMS) s->minMS = m->duration_ms;
        if (m->duration_ms > result->mean + 2.0 * result->stddev) s->anomalies++;
    }

    pthread_mutex_unlock(&buf->lock);

    for (int i = 0; i < *num_stats; i++)
        if (stats[i].count > 0)
            stats[i].avgMS /= stats[i].count;
}

void calc_thread_stats(MetricsBuffer *buf, AnalysisResult *result, ThreadStat *stats, int *num_stats) {
    *num_stats = 0;
    pthread_mutex_lock(&buf->lock);

    for (int i = 0; i < buf->count; i++) {
        QueryMetric *m  = &buf->entries[i];
        int          tid = m->query_id / 100;

        int found = -1;
        for (int j = 0; j < *num_stats; j++) {
            if (stats[j].threadID == tid) { found = j; break; }
        }
        if (found == -1) {
            found = *num_stats;
            stats[found].threadID = tid;
            stats[found].count     = 0;
            stats[found].anomalies = 0;
            stats[found].avgMS    = 0;
            (*num_stats)++;
        }

        stats[found].avgMS += m->duration_ms;
        stats[found].count++;
        if (m->duration_ms > result->mean + 2.0 * result->stddev)
            stats[found].anomalies++;
    }

    pthread_mutex_unlock(&buf->lock);

    for (int i = 0; i < *num_stats; i++)
        if (stats[i].count > 0)
            stats[i].avgMS /= stats[i].count;
}

QueryMetric find_slowest(MetricsBuffer *buf) {
    QueryMetric worst = {0};
    pthread_mutex_lock(&buf->lock);
    for (int i = 0; i < buf->count; i++)
        if (buf->entries[i].duration_ms > worst.duration_ms)
            worst = buf->entries[i];
    pthread_mutex_unlock(&buf->lock);
    return worst;
}

QueryMetric find_fastest(MetricsBuffer *buf) {
    QueryMetric best = buf->entries[0];
    pthread_mutex_lock(&buf->lock);
    for (int i = 1; i < buf->count; i++)
        if (buf->entries[i].duration_ms < best.duration_ms)
            best = buf->entries[i];
    pthread_mutex_unlock(&buf->lock);
    return best;
}