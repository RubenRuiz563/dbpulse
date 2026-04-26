#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <stdint.h>
#include <pthread.h>
#include <sqlite3.h>

#define MAX_METRICS 1024
#define QUERY_LEN 256

typedef struct {
    int query_id;
    char query[QUERY_LEN];
    double duration_ms;
    int rows_scanned;
    long timestamp;
} QueryMetric;

typedef struct {
    QueryMetric entries[MAX_METRICS];   
    int count;
    pthread_mutex_t lock;
} MetricsBuffer;

void metrics_buffer_init(MetricsBuffer* buf);
void metrics_buffer_add(MetricsBuffer* buf, QueryMetric metric);
void run_collector(MetricsBuffer* buf, sqlite3* db, int thread_id);

#endif // COLLECTOR_H