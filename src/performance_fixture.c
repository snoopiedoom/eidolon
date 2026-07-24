#include "performance_fixture.h"

#include <string.h>

#define FIXTURE_CONTROL_STEP_MS 20
#define FIXTURE_MAX_STEPS_PER_UPDATE 8U

typedef struct FixtureStage {
    EidolonEprTick tick;
    EidolonEprOperationalMode mode;
    bool contrast;
} FixtureStage;

static const FixtureStage STAGES[] = {
    {0, EIDOLON_EPR_MODE_ABSENT, false},       {400, EIDOLON_EPR_MODE_LISTENING, false},
    {1100, EIDOLON_EPR_MODE_THINKING, false},  {2000, EIDOLON_EPR_MODE_RESPONDING, false},
    {3000, EIDOLON_EPR_MODE_RESPONDING, true}, {3600, EIDOLON_EPR_MODE_INTERRUPTED, true},
};

static EidolonPerformanceIntent make_intent(unsigned int stage) {
    const FixtureStage *source = &STAGES[stage];
    EidolonPerformanceIntent intent;
    memset(&intent, 0, sizeof(intent));
    intent.version = EIDOLON_EPR_INTENT_VERSION;
    intent.revision = (uint64_t)stage + 1U;
    intent.predecessor_revision = (uint64_t)stage;
    intent.observed_tick = source->tick;
    intent.mode = source->mode;
    intent.urgency = source->mode == EIDOLON_EPR_MODE_INTERRUPTED ? 1000U : 400U;
    intent.continuity = 800U;
    intent.provenance.source = UINT64_C(0xe1d0100000000001);
    intent.provenance.session = UINT64_C(0xe1d0100000000002);
    intent.provenance.turn = UINT64_C(0xe1d0100000000003);
    intent.provenance.response = UINT64_C(0xe1d0100000000004);
    intent.provenance.message = UINT64_C(0xe1d0100000000005);
    intent.provenance.truth_revision = intent.revision;
    if (source->mode != EIDOLON_EPR_MODE_ABSENT) {
        intent.has_performance_lease = true;
        intent.lease_source = intent.provenance.source;
        intent.lease_session = intent.provenance.session;
    }
    if (source->contrast) {
        intent.beats[0].id = UINT64_C(0xe1d01000c017a57);
        intent.beats[0].kind = EIDOLON_EPR_BEAT_CONTRAST;
        intent.beats[0].stability = EIDOLON_EPR_EVIDENCE_STABLE_PREFIX;
        intent.beats[0].source_start = 18U;
        intent.beats[0].source_end = 36U;
        intent.beats[0].anchor_tick = 3520;
        intent.beat_count = 1U;
    }
    return intent;
}

void eidolon_performance_fixture_init(EidolonPerformanceFixture *fixture) {
    if (fixture != NULL) {
        memset(fixture, 0, sizeof(*fixture));
    }
}

bool eidolon_performance_fixture_update(EidolonPerformanceFixture *fixture,
                                        EidolonPerformanceRuntime *runtime, uint64_t now_ms) {
    unsigned int steps = 0U;
    EidolonEprTick available_tick;
    if (fixture == NULL || runtime == NULL || fixture->failed) {
        return false;
    }
    if (!fixture->started) {
        fixture->started = true;
        fixture->start_ms = now_ms;
        fixture->next_tick = 0;
    }
    available_tick = (EidolonEprTick)(now_ms - fixture->start_ms);
    while (fixture->next_tick <= available_tick && steps < FIXTURE_MAX_STEPS_PER_UPDATE) {
        while (fixture->stage < sizeof(STAGES) / sizeof(STAGES[0]) &&
               STAGES[fixture->stage].tick <= fixture->next_tick) {
            EidolonPerformanceIntent intent = make_intent(fixture->stage);
            if (!eidolon_epr_runtime_accept(runtime, &intent)) {
                fixture->failed = true;
                return false;
            }
            fixture->stage += 1U;
        }
        if (!eidolon_epr_runtime_step(runtime, fixture->next_tick)) {
            fixture->failed = true;
            return false;
        }
        fixture->next_tick += FIXTURE_CONTROL_STEP_MS;
        steps += 1U;
    }
    return true;
}
