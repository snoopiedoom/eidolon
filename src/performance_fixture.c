#include "performance_fixture.h"

#include <limits.h>
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

static uint32_t resource_bit(EidolonEprBodyResource resource) {
    return UINT32_C(1) << (uint32_t)resource;
}

static void set_arm(EidolonEprPoseAnchor *anchor, float hand_x, float hand_y, float hand_z,
                    float wrist_x, float wrist_y) {
    anchor->right_arm.hand_target[0] = hand_x;
    anchor->right_arm.hand_target[1] = hand_y;
    anchor->right_arm.hand_target[2] = hand_z;
    anchor->right_arm.elbow_pole[0] = 0.50F;
    anchor->right_arm.elbow_pole[1] = 0.05F;
    anchor->right_arm.elbow_pole[2] = -0.45F;
    anchor->right_arm.wrist_euler[0] = wrist_x;
    anchor->right_arm.wrist_euler[1] = wrist_y;
    anchor->right_arm.weight = 1.0F;
}

bool eidolon_performance_fixture_make_realization_profile(
    const EidolonEprBodyProfile *body, EidolonEprRealizationProfile *profile) {
    const uint32_t posture_resources = resource_bit(EIDOLON_EPR_RESOURCE_TORSO) |
                                       resource_bit(EIDOLON_EPR_RESOURCE_HEAD) |
                                       resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint32_t gesture_resources = resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    if (body == NULL || profile == NULL || body->fingerprint == 0U) {
        return false;
    }
    memset(profile, 0, sizeof(*profile));
    profile->version = EIDOLON_EPR_REALIZATION_PROFILE_VERSION;
    profile->body_fingerprint = body->fingerprint;
    profile->anchor_mask =
        (UINT32_C(1) << (uint32_t)EIDOLON_EPR_POSE_ANCHOR_COUNT) - 1U;

    EidolonEprPoseAnchor *neutral = &profile->anchors[EIDOLON_EPR_POSE_NEUTRAL];
    neutral->resource_mask = posture_resources;
    set_arm(neutral, 0.45F, -0.75F, 0.08F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *attentive = &profile->anchors[EIDOLON_EPR_POSE_ATTENTIVE];
    attentive->resource_mask = posture_resources;
    attentive->torso_euler[0] = 0.035F;
    attentive->head_euler[0] = -0.025F;
    set_arm(attentive, 0.40F, -0.70F, 0.08F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *thinking = &profile->anchors[EIDOLON_EPR_POSE_THINKING];
    thinking->resource_mask = posture_resources;
    thinking->torso_euler[0] = 0.075F;
    thinking->head_euler[0] = 0.11F;
    set_arm(thinking, 0.25F, -0.55F, 0.30F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *responding = &profile->anchors[EIDOLON_EPR_POSE_RESPONDING];
    responding->resource_mask = posture_resources;
    responding->torso_euler[0] = -0.025F;
    responding->head_euler[0] = -0.015F;
    set_arm(responding, 0.50F, -0.65F, 0.12F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *preparation =
        &profile->anchors[EIDOLON_EPR_POSE_CONTRAST_PREPARATION];
    preparation->resource_mask = gesture_resources;
    set_arm(preparation, 0.35F, -0.42F, 0.35F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *peak = &profile->anchors[EIDOLON_EPR_POSE_CONTRAST_PEAK];
    peak->resource_mask = gesture_resources;
    set_arm(peak, 0.70F, -0.20F, 0.45F, -0.12F, 0.18F);

    EidolonEprPoseAnchor *recovery =
        &profile->anchors[EIDOLON_EPR_POSE_CONTRAST_RECOVERY];
    recovery->resource_mask = gesture_resources;
    set_arm(recovery, 0.40F, -0.42F, 0.28F, 0.0F, 0.0F);

    EidolonEprPoseAnchor *guarded =
        &profile->anchors[EIDOLON_EPR_POSE_INTERRUPTED_GUARDED];
    guarded->resource_mask = posture_resources;
    guarded->torso_euler[0] = 0.055F;
    guarded->head_euler[0] = 0.015F;
    set_arm(guarded, 0.18F, -0.45F, 0.32F, 0.0F, 0.0F);
    return eidolon_epr_realization_profile_validate(profile, body);
}

static EidolonPerformanceIntent make_intent(const EidolonPerformanceFixture *fixture,
                                            unsigned int stage) {
    const FixtureStage *source = &STAGES[stage];
    EidolonPerformanceIntent intent;
    memset(&intent, 0, sizeof(intent));
    intent.version = EIDOLON_EPR_INTENT_VERSION;
    intent.revision = fixture->revision_base + (uint64_t)stage + 1U;
    intent.predecessor_revision = fixture->revision_base + (uint64_t)stage;
    intent.observed_tick = fixture->tick_offset + source->tick;
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
        intent.beats[0].id = UINT64_C(0xe1d01000c017a57) ^
                             fixture->revision_base * UINT64_C(0x9e3779b97f4a7c15);
        intent.beats[0].kind = EIDOLON_EPR_BEAT_CONTRAST;
        intent.beats[0].stability = EIDOLON_EPR_EVIDENCE_STABLE_PREFIX;
        intent.beats[0].source_start = 18U;
        intent.beats[0].source_end = 36U;
        intent.beats[0].anchor_tick = fixture->tick_offset + 3520;
        intent.beat_count = 1U;
    }
    return intent;
}

void eidolon_performance_fixture_init(EidolonPerformanceFixture *fixture) {
    if (fixture != NULL) {
        memset(fixture, 0, sizeof(*fixture));
    }
}

bool eidolon_performance_fixture_restart(EidolonPerformanceFixture *fixture,
                                         const EidolonPerformanceRuntime *runtime,
                                         uint64_t now_ms) {
    if (fixture == NULL || runtime == NULL || fixture->failed || !fixture->started ||
        fixture->stage != sizeof(STAGES) / sizeof(STAGES[0]) || !runtime->has_intent ||
        !runtime->has_tick || runtime->last_tick > INT64_MAX - FIXTURE_CONTROL_STEP_MS ||
        runtime->intent.revision > UINT64_MAX - sizeof(STAGES) / sizeof(STAGES[0])) {
        return false;
    }
    fixture->start_ms = now_ms;
    fixture->tick_offset = runtime->last_tick + FIXTURE_CONTROL_STEP_MS;
    fixture->next_tick = fixture->tick_offset;
    fixture->revision_base = runtime->intent.revision;
    fixture->stage = 0U;
    return true;
}

bool eidolon_performance_fixture_update(EidolonPerformanceFixture *fixture,
                                        EidolonPerformanceRuntime *runtime, uint64_t now_ms) {
    unsigned int steps = 0U;
    EidolonEprTick available_tick;
    uint64_t elapsed_ms;
    if (fixture == NULL || runtime == NULL || fixture->failed) {
        return false;
    }
    if (!fixture->started) {
        fixture->started = true;
        fixture->start_ms = now_ms;
        fixture->next_tick = 0;
    }
    if (now_ms < fixture->start_ms) {
        fixture->failed = true;
        return false;
    }
    elapsed_ms = now_ms - fixture->start_ms;
    if (elapsed_ms > (uint64_t)INT64_MAX ||
        fixture->tick_offset > INT64_MAX - (EidolonEprTick)elapsed_ms) {
        fixture->failed = true;
        return false;
    }
    available_tick = fixture->tick_offset + (EidolonEprTick)elapsed_ms;
    while (fixture->next_tick <= available_tick && steps < FIXTURE_MAX_STEPS_PER_UPDATE) {
        while (fixture->stage < sizeof(STAGES) / sizeof(STAGES[0]) &&
               fixture->tick_offset + STAGES[fixture->stage].tick <= fixture->next_tick) {
            EidolonPerformanceIntent intent = make_intent(fixture, fixture->stage);
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
