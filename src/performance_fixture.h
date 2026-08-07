#ifndef EIDOLON_PERFORMANCE_FIXTURE_H
#define EIDOLON_PERFORMANCE_FIXTURE_H

#include "epr/performance_runtime.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct EidolonPerformanceFixture {
    uint64_t start_ms;
    EidolonEprTick tick_offset;
    EidolonEprTick next_tick;
    uint64_t revision_base;
    unsigned int stage;
    bool started;
    bool failed;
} EidolonPerformanceFixture;

void eidolon_performance_fixture_init(EidolonPerformanceFixture *fixture);
bool eidolon_performance_fixture_make_realization_profile(
    const EidolonEprBodyProfile *body, EidolonEprRealizationProfile *profile);
bool eidolon_performance_fixture_restart(EidolonPerformanceFixture *fixture,
                                         const EidolonPerformanceRuntime *runtime,
                                         uint64_t now_ms);
bool eidolon_performance_fixture_update(EidolonPerformanceFixture *fixture,
                                        EidolonPerformanceRuntime *runtime, uint64_t now_ms);

#endif
