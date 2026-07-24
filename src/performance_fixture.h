#ifndef EIDOLON_PERFORMANCE_FIXTURE_H
#define EIDOLON_PERFORMANCE_FIXTURE_H

#include "epr/performance_runtime.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct EidolonPerformanceFixture {
    uint64_t start_ms;
    EidolonEprTick next_tick;
    unsigned int stage;
    bool started;
    bool failed;
} EidolonPerformanceFixture;

void eidolon_performance_fixture_init(EidolonPerformanceFixture *fixture);
bool eidolon_performance_fixture_update(EidolonPerformanceFixture *fixture,
                                        EidolonPerformanceRuntime *runtime, uint64_t now_ms);

#endif
