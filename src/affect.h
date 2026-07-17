#ifndef EIDOLON_AFFECT_H
#define EIDOLON_AFFECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "state.h"

#define EIDOLON_GOEMOTIONS_COUNT 28U

typedef enum EidolonGoEmotion {
    EIDOLON_EMOTION_ADMIRATION,
    EIDOLON_EMOTION_AMUSEMENT,
    EIDOLON_EMOTION_ANGER,
    EIDOLON_EMOTION_ANNOYANCE,
    EIDOLON_EMOTION_APPROVAL,
    EIDOLON_EMOTION_CARING,
    EIDOLON_EMOTION_CONFUSION,
    EIDOLON_EMOTION_CURIOSITY,
    EIDOLON_EMOTION_DESIRE,
    EIDOLON_EMOTION_DISAPPOINTMENT,
    EIDOLON_EMOTION_DISAPPROVAL,
    EIDOLON_EMOTION_DISGUST,
    EIDOLON_EMOTION_EMBARRASSMENT,
    EIDOLON_EMOTION_EXCITEMENT,
    EIDOLON_EMOTION_FEAR,
    EIDOLON_EMOTION_GRATITUDE,
    EIDOLON_EMOTION_GRIEF,
    EIDOLON_EMOTION_JOY,
    EIDOLON_EMOTION_LOVE,
    EIDOLON_EMOTION_NERVOUSNESS,
    EIDOLON_EMOTION_OPTIMISM,
    EIDOLON_EMOTION_PRIDE,
    EIDOLON_EMOTION_REALIZATION,
    EIDOLON_EMOTION_RELIEF,
    EIDOLON_EMOTION_REMORSE,
    EIDOLON_EMOTION_SADNESS,
    EIDOLON_EMOTION_SURPRISE,
    EIDOLON_EMOTION_NEUTRAL,
} EidolonGoEmotion;

typedef struct EidolonAffect {
    float valence;
    float arousal;
    float dominance;
    float certainty;
    float warmth;
    float surprise;
} EidolonAffect;

typedef enum EidolonAffectSource {
    EIDOLON_AFFECT_SOURCE_STATE,
    EIDOLON_AFFECT_SOURCE_GOEMOTIONS,
} EidolonAffectSource;

typedef enum EidolonExpressionIntent {
    EIDOLON_EXPRESSION_DEFAULT,
    EIDOLON_EXPRESSION_CHEERFUL,
    EIDOLON_EXPRESSION_RESPONDING,
    EIDOLON_EXPRESSION_DELIGHTED,
    EIDOLON_EXPRESSION_EMBARRASSED,
    EIDOLON_EXPRESSION_SERIOUS,
    EIDOLON_EXPRESSION_WORRIED,
    EIDOLON_EXPRESSION_ANNOYED,
    EIDOLON_EXPRESSION_GENTLE,
    EIDOLON_EXPRESSION_ECSTATIC,
    EIDOLON_EXPRESSION_COUNT,
} EidolonExpressionIntent;

typedef struct EidolonAffectController {
    EidolonAffect current;
    EidolonAffect target;
    EidolonAffectSource source;
    EidolonState state;
    uint64_t sequence;
    uint64_t result_sequence;
    float evidence;
    EidolonExpressionIntent expression_intent;
    EidolonExpressionIntent candidate_expression_intent;
    uint64_t candidate_since_ms;
} EidolonAffectController;

void eidolon_affect_controller_init(EidolonAffectController *controller, EidolonState state,
                                    uint64_t now_ms);
void eidolon_affect_controller_set_state(EidolonAffectController *controller, EidolonState state,
                                         uint64_t now_ms);
uint64_t eidolon_affect_controller_begin_text(EidolonAffectController *controller);
bool eidolon_affect_controller_apply_goemotions(EidolonAffectController *controller,
                                                uint64_t sequence,
                                                const float probabilities[EIDOLON_GOEMOTIONS_COUNT],
                                                uint64_t now_ms);
void eidolon_affect_controller_update(EidolonAffectController *controller, float delta_seconds,
                                      uint64_t now_ms);
EidolonAffect eidolon_affect_for_state(EidolonState state);
EidolonAffect eidolon_affect_from_goemotions(
    const float probabilities[EIDOLON_GOEMOTIONS_COUNT], EidolonAffect prior, float *evidence);
EidolonExpressionIntent eidolon_affect_expression(const EidolonAffect *affect);
const char *eidolon_expression_intent_name(EidolonExpressionIntent intent);
const char *eidolon_goemotion_name(size_t index);

#endif
