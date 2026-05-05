#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sqlite3.h>
#include "collector.h"
#include "analyzer.h"
#include "display.h"

#define NUM_THREADS 4

static MetricsBuffer  g_buffer;
static volatile int   g_threads_done = 0;
static pthread_mutex_t g_done_lock = PTHREAD_MUTEX_INITIALIZER;

static void seed_database(sqlite3 *db) {
    const char *schema =
        "CREATE TABLE IF NOT EXISTS orders ("
        "  id INTEGER PRIMARY KEY,"
        "  customer_id INTEGER,"
        "  amount REAL,"
        "  timestamp TEXT"
        ");";
    char *err = NULL;
    sqlite3_exec(db, schema, NULL, NULL, &err);
    if (err) { fprintf(stderr, "Schema error: %s\n", err); sqlite3_free(err); }

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    for (int i = 0; i < 10000; i++) {
        char insert[256];
        snprintf(insert, sizeof(insert),
            "INSERT OR IGNORE INTO orders (id, customer_id, amount, timestamp) "
            "VALUES (%d, %d, %.2f, '2024-%02d-%02d');",
            i, i % 200, (double)(rand() % 500 + 1),
            (i % 12) + 1, (i % 28) + 1);
        sqlite3_exec(db, insert, NULL, NULL, NULL);
    }
    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
}

typedef struct { int thread_id; } ThreadArgs;

static void *collector_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    sqlite3 *db;
    if (sqlite3_open("data/bench.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Thread %d: cannot open db\n", args->thread_id);
        return NULL;
    }

    for (int round = 0; round < 20; round++) {
        run_collector(&g_buffer, db, args->thread_id);
        usleep(200000);
    }

    sqlite3_close(db);

    pthread_mutex_lock(&g_done_lock);
    g_threads_done++;
    pthread_mutex_unlock(&g_done_lock);

    return NULL;
}

int main() {
    sqlite3 *seed_db;
    if (sqlite3_open("data/bench.db", &seed_db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(seed_db));
        return 1;
    }

    printf("Seeding database...\n");
    seed_database(seed_db);
    sqlite3_close(seed_db);

    metrics_buffer_init(&g_buffer);

    pthread_t  threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, collector_thread, &args[i]);
    }

    // Display loop, runs until all threads are done
    while (1) {
        sleep(1);

        pthread_mutex_lock(&g_done_lock);
        int done = g_threads_done;
        pthread_mutex_unlock(&g_done_lock);

        AnalysisResult result = analyze_metrics(&g_buffer);
        display_dashboard(&g_buffer, &result);

        if (done >= NUM_THREADS) break; 
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    AnalysisResult final  = analyze_metrics(&g_buffer);
    Percentiles    percs  = calc_percentiles(&g_buffer);
    QueryMetric    slowest = find_slowest(&g_buffer);
    QueryMetric    fastest = find_fastest(&g_buffer);

    QueryStat  qstats[10];
    ThreadStat tstats[NUM_THREADS];
    int nq = 0, nt = 0;
    calc_query_stats(&g_buffer, &final, qstats, &nq);
    calc_thread_stats(&g_buffer, &final, tstats, &nt);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                      POST-RUN ANALYSIS                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    // Overview
    printf("  OVERVIEW\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  Total queries    : %d\n",        g_buffer.count);
    printf("  Anomalies        : %d (%.1f%%)\n",
           final.anomalyCount,
           (double)final.anomalyCount / g_buffer.count * 100);
    printf("  Mean latency     : %.3f ms\n",   final.mean);
    printf("  Std deviation    : %.3f ms\n\n", final.stddev);

    // Latency percentiles
    printf("  LATENCY PERCENTILES\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  p50  : %.3f ms\n", percs.p50);
    printf("  p90  : %.3f ms\n", percs.p90);
    printf("  p95  : %.3f ms\n", percs.p95);
    printf("  p99  : %.3f ms\n\n", percs.p99);

    printf("  EXTREMES\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  Slowest  %.3f ms  [ID %d]  %.60s\n",
           slowest.duration_ms, slowest.query_id, slowest.query);
    printf("  Fastest  %.3f ms  [ID %d]  %.60s\n\n",
           fastest.duration_ms, fastest.query_id, fastest.query);

    printf("  PER-QUERY BREAKDOWN\n");
    printf("  %-10s %-10s %-10s %-10s %-8s %s\n",
           "Avg(ms)", "Min(ms)", "Max(ms)", "Count", "Anomaly", "Query");
    printf("  ──────────────────────────────────────────────────────────────\n");
    for (int i = 0; i < nq; i++) {
        printf("  %-10.3f %-10.3f %-10.3f %-10d %-8d %.45s\n",
               qstats[i].avgMS, qstats[i].minMS, qstats[i].maxMS,
               qstats[i].count,  qstats[i].anomalies, qstats[i].query);
    }

    printf("\n  PER-THREAD BREAKDOWN\n");
    printf("  %-10s %-10s %-10s %s\n", "Thread", "Queries", "Anomalies", "Avg(ms)");
    printf("  ──────────────────────────────────────────────────────────────\n");
    for (int i = 0; i < nt; i++) {
        printf("  %-10d %-10d %-10d %.3f ms\n",
               tstats[i].threadID, tstats[i].count,
               tstats[i].anomalies, tstats[i].avgMS);
    }

    printf("\n");
    pthread_mutex_destroy(&g_buffer.lock);
    pthread_mutex_destroy(&g_done_lock);
    return 0;
}