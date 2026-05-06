#ifndef DISPLAY_H
#define DISPLAY_H

#include "analyzer.h"
#include "collector.h"

void display_dashboard(MetricsBuffer* buf, AnalysisResult* result);
void clear_terminal();

#endif // DISPLAY_H