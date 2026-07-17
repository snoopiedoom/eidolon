#include "affect.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static void goemotions_projection_uses_distribution(void) {
    float probabilities[EIDOLON_GOEMOTIONS_COUNT] = {0};
    probabilities[EIDOLON_EMOTION_JOY] = 0.86F;
    probabilities[EIDOLON_EMOTION_GRATITUDE] = 0.62F;
    float evidence = 0.0F;
    const EidolonAffect affect = eidolon_affect_from_goemotions(
        probabilities, eidolon_affect_for_state(EIDOLON_STATE_REVIEW), &evidence);
    assert(affect.valence > 0.65F);
    assert(affect.warmth > 0.60F);
    assert(evidence > 0.80F);
    assert(eidolon_affect_expression(&affect) == 1 ||
           eidolon_affect_expression(&affect) == 3);
}

static void stale_results_are_ignored(void) {
    EidolonAffectController controller;
    eidolon_affect_controller_init(&controller, EIDOLON_STATE_REVIEW, 1000U);
    const uint64_t first = eidolon_affect_controller_begin_text(&controller);
    const uint64_t second = eidolon_affect_controller_begin_text(&controller);
    float probabilities[EIDOLON_GOEMOTIONS_COUNT] = {0};
    probabilities[EIDOLON_EMOTION_ANGER] = 0.9F;
    assert(!eidolon_affect_controller_apply_goemotions(&controller, first, probabilities, 1100U));
    assert(eidolon_affect_controller_apply_goemotions(&controller, second, probabilities, 1100U));
    assert(controller.source == EIDOLON_AFFECT_SOURCE_GOEMOTIONS);
}

static void state_is_always_a_working_fallback(void) {
    EidolonAffectController controller;
    eidolon_affect_controller_init(&controller, EIDOLON_STATE_IDLE, 0U);
    eidolon_affect_controller_set_state(&controller, EIDOLON_STATE_FAILED, 10U);
    for (uint64_t now = 20U; now <= 2000U; now += 20U) {
        eidolon_affect_controller_update(&controller, 0.02F, now);
    }
    assert(controller.source == EIDOLON_AFFECT_SOURCE_STATE);
    assert(controller.current.valence < -0.5F);
    assert(controller.expression_intent == EIDOLON_EXPRESSION_ANNOYED);
}

int main(void) {
    assert(strcmp(eidolon_goemotion_name(EIDOLON_EMOTION_SURPRISE), "surprise") == 0);
    goemotions_projection_uses_distribution();
    stale_results_are_ignored();
    state_is_always_a_working_fallback();
    return 0;
}
