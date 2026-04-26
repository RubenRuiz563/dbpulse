#include <studio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "collector.h"

static const char* QUERIES[] = {
    "SELECT COUNT(*) FROM orders",
    "SELECT customer_id, SUM(amount) FROM orders GROUP BY customer_id",
    "SELECT * FROM orders WHERE amount > 100 ORDER BY timestamp DESC LIMIT 50",
    "SELECT customer_id, COUNT(*) FROM orders GROUP BY customer_id HAVING COUNT(*) > 5",
    "SELECT AVG(amount) FROM orders WHERE strftime('%Y', timestamp) = '2024'"
};

static const int NUM_QUERIES = 5;

void metrics_buffer_init(MetricsBuffer* buf) {
    buf->count = 0;
    pthread_mutex_init(&buf->lock, NULL);
}

void metrics_buffer_add(MetricsBuffer* buf, QueryMetric metric) {
    pthread_mutex_lock(&buf->lock);
    if (buf->count < MAX_METRICS) {
        buf->entries[buf->count++] = metric;
    }
    pthread_mutex_unlock(&buf->lock);
}

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int count_rows_callback(void* count, int argc, char** argv, char** col) {
    (void)argc; (void)argv; (void)col;
    (*(int *)count)++;
    return 0;
}

void run_collector(MetricsBuffer* buf, sqlite3* db, int thread_id) {
    for (int i = 0; i < NUM_QUERIES; i++) {
        int row_count = 0;
        char* err_msg = NULL;

        double start_time = get_time_ms();
        sqlite3_exec(db, QUERIES[i], count_rows_callback, &row_count, &err_msg);
        double end_time = get_time_ms();

        if (err_msg) {
            sqlite3_free(err_msg);
            continue;
        }

        QueryMetric m;
        m.query_id = thread_id * 100 + i;
        m.duration_ms = end_time - start_time;
        m.rows_scanned = row_count;
        m.timestamp = (long)time(NULL);
        strncpy(m.query, QUERIES[i], QUERY_LEN - 1);

        metrics_buffer_add(buf, m);
    }
}