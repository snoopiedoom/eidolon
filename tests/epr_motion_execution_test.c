#include "epr/motion_execution.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct MotionFixture {
    uint64_t rotation_mask;
    float radians;
    float hips_x;
    float last_seconds;
    size_t sample_count;
    bool has_hips;
    bool fail;
} MotionFixture;

static uint64_t role_bit(EidolonHumanoidRole role) { return UINT64_C(1) << (uint32_t)role; }

static uint32_t resource_bit(EidolonEprBodyResource resource) {
    return UINT32_C(1) << (uint32_t)resource;
}

static void quaternion_axis(float x, float y, float z, float radians, float result[4]) {
    const float sine = sinf(radians * 0.5F);
    result[0] = x * sine;
    result[1] = y * sine;
    result[2] = z * sine;
    result[3] = cosf(radians * 0.5F);
}

static bool quaternion_near(const float left[4], const float right[4]) {
    float direct = 0.0F;
    float negated = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        const float same = left[component] - right[component];
        const float opposite = left[component] + right[component];
        direct += same * same;
        negated += opposite * opposite;
    }
    return fminf(direct, negated) < 0.00001F;
}

static bool near(float left, float right) { return fabsf(left - right) < 0.0001F; }

static bool sample_motion(const void *context, float seconds, bool loop,
                          EidolonHumanoidPose *pose) {
    MotionFixture *fixture = (MotionFixture *)context;
    (void)loop;
    if (fixture == NULL || pose == NULL || fixture->fail) {
        return false;
    }
    fixture->last_seconds = seconds;
    fixture->sample_count += 1U;
    eidolon_humanoid_pose_init(pose);
    pose->rotation_mask = fixture->rotation_mask;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if ((fixture->rotation_mask & role_bit((EidolonHumanoidRole)role)) != 0U) {
            quaternion_axis(1.0F, 0.0F, 0.0F, fixture->radians, pose->rotations[role]);
        }
    }
    pose->has_hips_translation = fixture->has_hips;
    pose->hips_translation[0] = fixture->hips_x;
    return true;
}

static EidolonEprMotionCatalogEntry entry(EidolonEprMotionGeneratorId generator, uint64_t identity,
                                          float duration, bool loop, MotionFixture *fixture) {
    EidolonEprMotionCatalogEntry value;
    memset(&value, 0, sizeof(value));
    value.generator = generator;
    value.source.version = EIDOLON_EPR_MOTION_SOURCE_VERSION;
    value.source.identity = identity;
    value.source.context = fixture;
    value.source.sample = sample_motion;
    value.source.rotation_mask = fixture->rotation_mask;
    value.source.duration_seconds = duration;
    value.source.owns_hips_translation = fixture->has_hips;
    value.source.loop = loop;
    return value;
}

static EidolonRealizationProgram program(EidolonEprBehaviorKind kind, EidolonEprModality modality,
                                         EidolonEprMotionGeneratorId generator,
                                         EidolonEprMotionTakeoverPolicy takeover,
                                         uint32_t resources, uint64_t id, uint64_t behavior) {
    EidolonRealizationProgram value;
    memset(&value, 0, sizeof(value));
    value.version = EIDOLON_EPR_PROGRAM_VERSION;
    value.id = id;
    value.behavior = behavior;
    value.cause = 1U;
    value.plan_generation = 7U;
    value.behavior_kind = kind;
    value.modality = modality;
    value.resource_mask = resources;
    value.motion.version = EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION;
    value.motion.generator = generator;
    value.motion.takeover = takeover;
    value.motion.resource_mask = resources;
    value.motion.blend_weight = 1.0F;
    value.motion.intensity = 1.0F;
    value.motion.playback_rate = 1.0F;
    assert(eidolon_epr_resource_mask_humanoid_channels(
        resources, &value.motion.humanoid_rotation_mask, &value.motion.owns_hips_translation));
    assert(eidolon_epr_motion_generator_reference_validate(&value.motion));
    return value;
}

static EidolonRealizationProgram posture_program(uint32_t resources, uint64_t id,
                                                 uint64_t behavior) {
    EidolonRealizationProgram value =
        program(EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE, EIDOLON_EPR_MODALITY_POSTURE,
                EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, EIDOLON_EPR_MOTION_TAKEOVER_BASE, resources,
                id, behavior);
    value.has_phase[EIDOLON_EPR_PHASE_ONSET] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_ONSET] = 1000;
    value.values[0] = 240.0F;
    return value;
}

static EidolonRealizationProgram idle_program(uint32_t resources, uint64_t id, uint64_t behavior) {
    return program(EIDOLON_EPR_BEHAVIOR_IDLE, EIDOLON_EPR_MODALITY_IDLE,
                   EIDOLON_EPR_MOTION_IDLE_NEUTRAL, EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE, resources,
                   id, behavior);
}

static EidolonRealizationProgram gesture_program(uint64_t id, uint64_t behavior) {
    EidolonRealizationProgram value =
        program(EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT, EIDOLON_EPR_MODALITY_GESTURE,
                EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE,
                resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), id, behavior);
    value.has_phase[EIDOLON_EPR_PHASE_PREPARATION] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_PREPARATION] = 100;
    value.has_phase[EIDOLON_EPR_PHASE_ONSET] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_ONSET] = 310;
    value.has_phase[EIDOLON_EPR_PHASE_PEAK] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_PEAK] = 410;
    value.has_phase[EIDOLON_EPR_PHASE_RECOVERY] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_RECOVERY] = 470;
    value.has_phase[EIDOLON_EPR_PHASE_COMPLETION] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_COMPLETION] = 910;
    return value;
}

static EidolonRealizationProgram settle_program(uint64_t id, uint64_t behavior) {
    EidolonRealizationProgram value;
    memset(&value, 0, sizeof(value));
    value.version = EIDOLON_EPR_PROGRAM_VERSION;
    value.id = id;
    value.behavior = behavior;
    value.cause = 1U;
    value.plan_generation = 7U;
    value.behavior_kind = EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM;
    value.modality = EIDOLON_EPR_MODALITY_SETTLE;
    value.resource_mask = resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    value.motion.version = EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION;
    value.has_phase[EIDOLON_EPR_PHASE_INTERRUPT] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT] = 1000;
    value.has_phase[EIDOLON_EPR_PHASE_SETTLE] = true;
    value.phase_ticks[EIDOLON_EPR_PHASE_SETTLE] = 1320;
    assert(eidolon_epr_motion_generator_reference_validate(&value.motion));
    return value;
}

static EidolonRealizationProgram procedural_gaze_program(uint64_t id, uint64_t behavior) {
    EidolonRealizationProgram value;
    memset(&value, 0, sizeof(value));
    value.version = EIDOLON_EPR_PROGRAM_VERSION;
    value.id = id;
    value.behavior = behavior;
    value.cause = 1U;
    value.plan_generation = 7U;
    value.behavior_kind = EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION;
    value.modality = EIDOLON_EPR_MODALITY_GAZE;
    value.resource_mask =
        resource_bit(EIDOLON_EPR_RESOURCE_EYES) | resource_bit(EIDOLON_EPR_RESOURCE_HEAD);
    value.motion.version = EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION;
    assert(eidolon_epr_motion_generator_reference_validate(&value.motion));
    return value;
}

static void add_entry(EidolonEprMotionCatalog *catalog, EidolonEprMotionCatalogEntry value) {
    assert(eidolon_epr_motion_catalog_add(catalog, &value) == EIDOLON_EPR_MOTION_CATALOG_OK);
}

static void assert_untouched(const EidolonEprMotionExecution *actual,
                             const EidolonEprMotionExecution *expected) {
    assert(memcmp(actual, expected, sizeof(*actual)) == 0);
}

static void test_phase_timing_and_source_time(void) {
    const uint32_t torso = resource_bit(EIDOLON_EPR_RESOURCE_TORSO);
    const uint32_t idle_resources = torso | resource_bit(EIDOLON_EPR_RESOURCE_HEAD);
    const uint32_t posture_resources =
        idle_resources | resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint64_t hips = role_bit(EIDOLON_HUMANOID_ROLE_HIPS);
    const uint64_t right_upper = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM);
    MotionFixture posture_fixture = {.rotation_mask = hips, .radians = 0.3F, .has_hips = true};
    MotionFixture idle_fixture = {.rotation_mask = hips, .radians = 0.1F, .has_hips = true};
    MotionFixture gesture_fixture = {.rotation_mask = right_upper, .radians = 0.5F};
    EidolonEprMotionCatalog catalog;
    EidolonRealizationProgram posture = posture_program(posture_resources, 11U, 101U);
    EidolonRealizationProgram idle = idle_program(idle_resources, 12U, 102U);
    EidolonRealizationProgram gesture = gesture_program(13U, 103U);
    EidolonRealizationProgram settle = settle_program(14U, 104U);
    EidolonRealizationProgram gaze = procedural_gaze_program(15U, 105U);
    EidolonEprMotionExecution execution;
    EidolonEprMotionExecution before;
    EidolonEprMotionExecutionResult status;

    eidolon_epr_motion_catalog_init(&catalog);
    add_entry(&catalog,
              entry(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, 201U, 1.0F, false, &posture_fixture));
    add_entry(&catalog, entry(EIDOLON_EPR_MOTION_IDLE_NEUTRAL, 202U, 1.0F, true, &idle_fixture));
    add_entry(&catalog, entry(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, 203U, 1.0F, false,
                              &gesture_fixture));

    memset(&execution, 0xa5, sizeof(execution));
    before = execution;
    status = eidolon_epr_motion_program_execute(&catalog, &posture, torso, 999, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE);
    assert_untouched(&execution, &before);

    posture.motion.playback_rate = 2.0F;
    status = eidolon_epr_motion_program_execute(&catalog, &posture, torso, 1120, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(execution.timing.stage == EIDOLON_EPR_MOTION_TRANSITION_ENTER);
    assert(near(execution.timing.transition_weight, 0.5F));
    assert(near(execution.layer_weight, 0.5F));
    assert(near(execution.timing.source_seconds, 0.24F));
    assert(near(execution.timing.normalized_source_time, 0.24F));
    assert(near(posture_fixture.last_seconds, 0.24F));

    status = eidolon_epr_motion_program_execute(&catalog, &idle, torso, 1250, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(execution.layer_mode == EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE);
    assert(execution.timing.stage == EIDOLON_EPR_MOTION_TRANSITION_STEADY);
    assert(near(execution.timing.source_seconds, 1.25F));
    assert(near(execution.timing.normalized_source_time, 0.25F));

    status = eidolon_epr_motion_program_execute(
        &catalog, &gesture, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 100, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(execution.timing.stage == EIDOLON_EPR_MOTION_TRANSITION_ENTER);
    assert(near(execution.layer_weight, 0.0F));
    status = eidolon_epr_motion_program_execute(
        &catalog, &gesture, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 310, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(execution.timing.stage == EIDOLON_EPR_MOTION_TRANSITION_HOLD);
    assert(near(execution.layer_weight, 1.0F));
    status = eidolon_epr_motion_program_execute(
        &catalog, &gesture, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 690, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(execution.timing.stage == EIDOLON_EPR_MOTION_TRANSITION_EXIT);
    assert(near(execution.layer_weight, 0.5F));
    status = eidolon_epr_motion_program_execute(
        &catalog, &gesture, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 910, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(near(execution.layer_weight, 0.0F));
    before = execution;
    status = eidolon_epr_motion_program_execute(
        &catalog, &gesture, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 911, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE);
    assert_untouched(&execution, &before);

    before = execution;
    status = eidolon_epr_motion_program_execute(
        &catalog, &settle, resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN), 1160, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED);
    assert_untouched(&execution, &before);
    assert(settle.motion.generator == EIDOLON_EPR_MOTION_GENERATOR_NONE);
    assert(settle.motion.resource_mask == 0U);

    before = execution;
    status = eidolon_epr_motion_program_execute(&catalog, &gaze, 0U, 1200, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED);
    assert_untouched(&execution, &before);
    assert(strcmp(eidolon_epr_motion_execution_status_name(status.status), "not_required") == 0);
}

static EidolonEprResourceGrant grant(uint64_t behavior, EidolonEprBodyResource resource,
                                     EidolonEprClaimMode mode) {
    const EidolonEprResourceGrant value = {
        .behavior = behavior,
        .resource = resource,
        .mode = mode,
    };
    return value;
}

static void test_grant_narrowing_and_deterministic_composition(void) {
    const uint32_t torso = resource_bit(EIDOLON_EPR_RESOURCE_TORSO);
    const uint32_t head = resource_bit(EIDOLON_EPR_RESOURCE_HEAD);
    const uint32_t right_arm = resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint64_t hips = role_bit(EIDOLON_HUMANOID_ROLE_HIPS);
    const uint64_t head_role = role_bit(EIDOLON_HUMANOID_ROLE_HEAD);
    MotionFixture posture_fixture = {
        .rotation_mask = hips | head_role,
        .radians = 1.04719755120F,
        .hips_x = 2.0F,
        .has_hips = true,
    };
    MotionFixture idle_fixture = {
        .rotation_mask = hips,
        .radians = 0.69813170080F,
        .hips_x = 1.0F,
        .has_hips = true,
    };
    EidolonEprMotionCatalog catalog;
    EidolonRealizationProgram posture = posture_program(torso | head | right_arm, 22U, 202U);
    EidolonRealizationProgram idle = idle_program(torso | head, 11U, 201U);
    EidolonRealizationProgramSet first;
    EidolonRealizationProgramSet reordered;
    EidolonEprResourceResolution resolution;
    EidolonHumanoidPose base;
    EidolonEprMotionFrame frame;
    EidolonEprMotionFrame repeated;
    EidolonEprMotionExecution partial;
    EidolonEprMotionExecutionResult status;
    float expected[4];

    eidolon_epr_motion_catalog_init(&catalog);
    add_entry(&catalog,
              entry(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, 301U, 2.0F, false, &posture_fixture));
    add_entry(&catalog, entry(EIDOLON_EPR_MOTION_IDLE_NEUTRAL, 302U, 2.0F, true, &idle_fixture));
    idle.motion.blend_weight = 0.5F;
    idle.motion.intensity = 0.5F;

    memset(&first, 0, sizeof(first));
    first.plan_generation = 7U;
    first.programs[0] = idle;
    first.programs[1] = posture;
    first.count = 2U;
    reordered = first;
    reordered.programs[0] = posture;
    reordered.programs[1] = idle;

    memset(&resolution, 0, sizeof(resolution));
    resolution.grants[0] =
        grant(posture.behavior, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_BASE);
    resolution.grants[1] =
        grant(idle.behavior, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_ADDITIVE);
    resolution.grant_count = 2U;

    eidolon_humanoid_pose_init(&base);
    base.rotation_mask = hips | role_bit(EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.17453292520F, base.rotations[EIDOLON_HUMANOID_ROLE_HIPS]);
    quaternion_axis(0.0F, 1.0F, 0.0F, 0.25F, base.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG]);
    base.has_hips_translation = true;
    base.hips_translation[0] = 10.0F;

    status =
        eidolon_epr_motion_programs_compose(&catalog, &first, &resolution, 1240, &base, &frame);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    status = eidolon_epr_motion_programs_compose(&catalog, &reordered, &resolution, 1240, &base,
                                                 &repeated);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(memcmp(&frame, &repeated, sizeof(frame)) == 0);
    assert(frame.execution_count == 2U);
    assert(frame.executions[0].program == posture.id);
    assert(frame.executions[0].layer_mode == EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE);
    assert(frame.executions[1].program == idle.id);
    assert(frame.executions[1].layer_mode == EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE);
    quaternion_axis(1.0F, 0.0F, 0.0F, 1.22173047640F, expected);
    assert(quaternion_near(frame.pose.rotations[EIDOLON_HUMANOID_ROLE_HIPS], expected));
    assert(near(frame.pose.hips_translation[0], 2.25F));
    assert(memcmp(frame.pose.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG],
                  base.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG], sizeof(expected)) == 0);

    status = eidolon_epr_motion_program_execute(&catalog, &posture, head, 1240, &partial);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(partial.granted_resource_mask == head);
    assert(partial.sample.pose.rotation_mask == head_role);
    assert(!partial.sample.pose.has_hips_translation);
}

static void test_override_skips_ungranted_sources(void) {
    const uint32_t right_arm = resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint32_t posture_resources = resource_bit(EIDOLON_EPR_RESOURCE_TORSO) |
                                       resource_bit(EIDOLON_EPR_RESOURCE_HEAD) | right_arm;
    const uint64_t upper = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM);
    MotionFixture gesture_fixture = {.rotation_mask = upper, .radians = 1.39626340160F};
    EidolonEprMotionCatalog catalog;
    EidolonRealizationProgram posture = posture_program(posture_resources, 31U, 301U);
    EidolonRealizationProgram gesture = gesture_program(32U, 302U);
    EidolonRealizationProgramSet programs;
    EidolonEprResourceResolution resolution;
    EidolonHumanoidPose base;
    EidolonEprMotionFrame frame;
    EidolonEprMotionExecutionResult status;

    eidolon_epr_motion_catalog_init(&catalog);
    add_entry(&catalog, entry(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, 401U, 1.0F, false,
                              &gesture_fixture));
    memset(&programs, 0, sizeof(programs));
    programs.plan_generation = 7U;
    programs.programs[0] = posture;
    programs.programs[1] = gesture;
    programs.count = 2U;
    memset(&resolution, 0, sizeof(resolution));
    resolution.grants[0] =
        grant(gesture.behavior, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN, EIDOLON_EPR_CLAIM_OVERRIDE);
    resolution.grant_count = 1U;
    eidolon_humanoid_pose_init(&base);

    status =
        eidolon_epr_motion_programs_compose(&catalog, &programs, &resolution, 410, &base, &frame);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_OK);
    assert(frame.execution_count == 1U);
    assert(frame.executions[0].program == gesture.id);
    assert(frame.pose.rotation_mask == upper);
}

static void test_failures_are_typed_and_transactional(void) {
    const uint32_t torso = resource_bit(EIDOLON_EPR_RESOURCE_TORSO);
    const uint32_t posture_resources = torso | resource_bit(EIDOLON_EPR_RESOURCE_HEAD) |
                                       resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint64_t hips = role_bit(EIDOLON_HUMANOID_ROLE_HIPS);
    MotionFixture fixture = {.rotation_mask = hips, .radians = 0.2F, .has_hips = true};
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalog empty;
    EidolonRealizationProgram posture = posture_program(posture_resources, 41U, 401U);
    EidolonRealizationProgramSet programs;
    EidolonEprResourceResolution resolution;
    EidolonHumanoidPose base;
    EidolonEprMotionExecution execution;
    EidolonEprMotionExecution execution_before;
    EidolonEprMotionFrame frame;
    EidolonEprMotionFrame frame_before;
    EidolonEprMotionExecutionResult status;

    eidolon_epr_motion_catalog_init(&catalog);
    eidolon_epr_motion_catalog_init(&empty);
    add_entry(&catalog, entry(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, 501U, 1.0F, false, &fixture));
    memset(&execution, 0x6b, sizeof(execution));
    execution_before = execution;

    posture.phase_ticks[EIDOLON_EPR_PHASE_ONSET] = 1300;
    status = eidolon_epr_motion_program_execute(&catalog, &posture, torso, 1240, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE);
    assert_untouched(&execution, &execution_before);
    posture.phase_ticks[EIDOLON_EPR_PHASE_ONSET] = 1000;
    posture.has_phase[EIDOLON_EPR_PHASE_PEAK] = true;
    status = eidolon_epr_motion_program_execute(&catalog, &posture, torso, 1240, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_INVALID_PHASES);
    assert_untouched(&execution, &execution_before);
    posture.has_phase[EIDOLON_EPR_PHASE_PEAK] = false;

    status = eidolon_epr_motion_program_execute(&empty, &posture, torso, 1240, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED);
    assert(status.catalog_status == EIDOLON_EPR_MOTION_CATALOG_MISSING_GENERATOR);
    assert_untouched(&execution, &execution_before);
    fixture.fail = true;
    status = eidolon_epr_motion_program_execute(&catalog, &posture, torso, 1240, &execution);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED);
    assert(status.catalog_status == EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED);
    assert_untouched(&execution, &execution_before);
    fixture.fail = false;

    memset(&programs, 0, sizeof(programs));
    programs.plan_generation = 7U;
    programs.programs[0] = posture;
    programs.count = 1U;
    memset(&resolution, 0, sizeof(resolution));
    resolution.grants[0] = grant(999U, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_BASE);
    resolution.grant_count = 1U;
    eidolon_humanoid_pose_init(&base);
    memset(&frame, 0x3c, sizeof(frame));
    frame_before = frame;
    status =
        eidolon_epr_motion_programs_compose(&catalog, &programs, &resolution, 1240, &base, &frame);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION);
    assert(memcmp(&frame, &frame_before, sizeof(frame)) == 0);

    resolution.grants[0] =
        grant(posture.behavior, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_ADDITIVE);
    status =
        eidolon_epr_motion_programs_compose(&catalog, &programs, &resolution, 1240, &base, &frame);
    assert(status.status == EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION);
    assert(memcmp(&frame, &frame_before, sizeof(frame)) == 0);
    assert(strcmp(eidolon_epr_motion_execution_status_name(
                      EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION),
                  "invalid_resolution") == 0);
}

int main(void) {
    test_phase_timing_and_source_time();
    test_grant_narrowing_and_deterministic_composition();
    test_override_skips_ungranted_sources();
    test_failures_are_typed_and_transactional();
    puts("EPR motion execution tests passed");
    return 0;
}
