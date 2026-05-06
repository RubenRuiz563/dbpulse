# dbpulse

A lightweight database performance monitor and anomaly detector written in C.

Monitors concurrent query workloads against a SQLite database in real time, flags statistically anomalous queries, and produces a detailed post-run analysis report. Inspired by the observability and telemetry systems built by teams like Oracle's Database Manageability group.

---

## Features

- **Concurrent collection** — 4 POSIX threads each run independent query workloads simultaneously
- **Thread-safe metrics buffer** — mutex-protected shared buffer aggregates results across all threads
- **Real-time latency measurement** — per-query timing using `clock_gettime(CLOCK_MONOTONIC)`
- **Statistical anomaly detection** — flags queries exceeding mean + 2σ as outliers
- **Live terminal dashboard** — refreshes every second with pinned anomalies and recent query feed
- **Post-run analysis report** — percentiles, per-query breakdown, per-thread breakdown, and extremes

---

## Architecture

```
┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│  Thread 0   │   │  Thread 1   │   │  Thread 2   │   │  Thread 3   │
│ (collector) │   │ (collector) │   │ (collector) │   │ (collector) │
└──────┬──────┘   └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
       │                 │                 │                 │
       └────────────────►▼◄────────────────┘◄────────────────┘
                  ┌──────────────┐
                  │MetricsBuffer │  (mutex-protected)
                  └──────┬───────┘
                         │
              ┌──────────┴──────────┐
              │                     │
       ┌──────▼──────┐      ┌───────▼──────┐
       │   Analyzer  │      │    Display   │
       │  mean, σ,   │      │  live dash + │
       │ percentiles │      │  post-report │
       └─────────────┘      └──────────────┘
```

Each thread opens its own SQLite connection, runs 5 benchmark queries per round across 20 rounds, and writes results into the shared buffer. The main thread drives the live dashboard and exits as soon as all collector threads finish.

---

## Build

**Requirements**
- GCC
- Make
- SQLite3 (`libsqlite3-dev` on Linux, `brew install sqlite3` on macOS)
- POSIX threads (pthreads)

```bash
make
```

**Run**
```bash
./dbpulse
```

**Clean**
```bash
make clean
```

---

## Live Dashboard

```
╔══════════════════════════════════════════════════════════════════╗
║              dbpulse — Database Performance Monitor              ║
╚══════════════════════════════════════════════════════════════════╝

  Total Queries : 400
  Mean Latency  : 2.008 ms
  Std Deviation : 1.328 ms
  Anomalies     : 10

  ⚠  ANOMALIES DETECTED
  ID     Time(ms)   Rows     Query
  ──────────────────────────────────────────────────────────────
  1      5.690      200      SELECT customer_id, SUM(amount) ...
  301    5.726      200      SELECT customer_id, SUM(amount) ...

  RECENT QUERIES
  ID     Time(ms)   Rows     Query
  ──────────────────────────────────────────────────────────────
  304    1.975      1        SELECT AVG(amount) FROM orders ...
  204    2.182      1        SELECT AVG(amount) FROM orders ...
```

Anomalous queries are pinned to the top section in red. Normal recent queries scroll below. Anomalies are excluded from the recent section to avoid duplication.

---

## Post-Run Analysis Report

```
╔══════════════════════════════════════════════════════════════════╗
║                      POST-RUN ANALYSIS                           ║
╚══════════════════════════════════════════════════════════════════╝

  OVERVIEW
  ─────────────────────────────────────────
  Total queries    : 400
  Anomalies        : 10 (2.5%)
  Mean latency     : 2.008 ms
  Std deviation    : 1.328 ms

  LATENCY PERCENTILES
  ─────────────────────────────────────────
  p50  : 1.900 ms
  p90  : 3.400 ms
  p95  : 4.100 ms
  p99  : 5.600 ms

  EXTREMES
  ─────────────────────────────────────────
  Slowest  5.726 ms  [ID 301]  SELECT customer_id, SUM(amount) FROM orders ...
  Fastest  0.028 ms  [ID 300]  SELECT COUNT(*) FROM orders

  PER-QUERY BREAKDOWN
  Avg(ms)    Min(ms)    Max(ms)    Count      Anomaly  Query
  ──────────────────────────────────────────────────────────────
  1.200      0.028      5.726      80         4        SELECT COUNT(*) ...
  ...

  PER-THREAD BREAKDOWN
  Thread     Queries    Anomalies  Avg(ms)
  ──────────────────────────────────────────────────────────────
  0          100        2          1.980 ms
  1          100        3          2.100 ms
  ...
```

---

## Benchmark Queries

The tool runs five representative SQL patterns against a 10,000-row `orders` table:

| Pattern | Description |
|---|---|
| `SELECT COUNT(*)` | Full table scan aggregate |
| `SELECT ... GROUP BY customer_id` with `SUM` | Grouped aggregation |
| `SELECT ... WHERE amount > 100 ORDER BY ... LIMIT 50` | Filtered sort with limit |
| `SELECT ... GROUP BY ... HAVING COUNT(*) > 5` | Grouped filter |
| `SELECT AVG(amount) WHERE strftime(...)` | Date-filtered aggregate |

---

## Concepts Demonstrated

| Concept | Implementation |
|---|---|
| Concurrent programming | POSIX threads, mutex synchronization, volatile shared flag |
| Algorithm analysis | O(n) mean/stddev, O(n log n) percentile sort |
| Database internals | SQLite C API, query execution, row scanning, connection-per-thread |
| Systems programming | `clock_gettime`, ANSI terminal control, dynamic memory |
| Statistical analysis | Mean + 2σ anomaly detection, p50/p90/p95/p99 latency percentiles |
