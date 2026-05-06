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

    pthread_mutex_lock(&buf->lock);

    double threshold = result->mean + 2.0 * result->stddev;

    // Anomalies section 
    printf("  \033[31m⚠  ANOMALIES DETECTED\033[0m\n");
    printf("  %-6s %-10s %-8s %s\n", "ID", "Time(ms)", "Rows", "Query");
    printf("  ──────────────────────────────────────────────────────────────\n");

    int shown = 0;
    for (int i = 0; i < buf->count; i++) {
        QueryMetric *m = &buf->entries[i];
        if (m->duration_ms > threshold) {
            printf("  \033[31m%-6d %-10.3f %-8d %.50s\033[0m\n",
                   m->query_id, m->duration_ms, m->rows_scanned, m->query);
            shown++;
        }
    }
    if (shown == 0) printf("  \033[90m  none yet\033[0m\n");

    printf("\n  RECENT QUERIES\n");
    printf("  %-6s %-10s %-8s %s\n", "ID", "Time(ms)", "Rows", "Query");
    printf("  ──────────────────────────────────────────────────────────────\n");

    int start = buf->count > 10 ? buf->count - 10 : 0;
    for (int i = start; i < buf->count; i++) {
        QueryMetric *m = &buf->entries[i];
        if (m->duration_ms > threshold) continue; 
        printf("  %-6d %-10.3f %-8d %.50s\n",
               m->query_id, m->duration_ms, m->rows_scanned, m->query);
    }

    pthread_mutex_unlock(&buf->lock);

    printf("\n  \033[31m■\033[0m = anomaly (> mean + 2σ)    refreshes every 1s\n");
}


void clear_terminal() {
    printf("\033c");
    fflush(stdout);
}
