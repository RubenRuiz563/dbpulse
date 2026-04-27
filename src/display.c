#include <stdio.h>
#include <time.h>
#include "display.h"

void display_dashboard(MetricsBuffer *buf, AnalysisResult *result) {
    clear_terminal();

    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║              dbpulse — Database Performance Monitor              ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    printf("  Total Queries : %d\n", buf->count);
    printf("  Mean Latency  : %.3f ms\n", result->mean);
    printf("  Std Deviation : %.3f ms\n", result->stddev);
    printf("  Anomalies     : %d\n\n", result->anomalyCount);

    printf("  %-6s %-10s %-8s %s\n", "ID", "Time(ms)", "Rows", "Query");
    printf("  ──────────────────────────────────────────────────────────────\n");

    pthread_mutex_lock(&buf->lock);
    int start = buf->count > 15 ? buf->count - 15 : 0;  // show last 15
    for (int i = start; i < buf->count; i++) {
        QueryMetric *m = &buf->entries[i];
        int anomaly = m->duration_ms > result->mean + 2.0 * result->stddev;

        if (anomaly) printf("\033[31m");  // red for anomalies
        printf("  %-6d %-10.3f %-8d %.50s\n",
               m->query_id, m->duration_ms, m->rows_scanned, m->query);
        if (anomaly) printf("\033[0m");   // reset color
    }
    pthread_mutex_unlock(&buf->lock);

    printf("\n  \033[31m■\033[0m = anomaly (> mean + 2σ)    refreshes every 1s\n");
}

void clear_terminal() {
    printf("\033c");
    fflush(stdout);
}
