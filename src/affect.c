#include "affect.h"

#include <math.h>
#include <string.h>

#define EXPRESSION_DWELL_MS 650U
#define EXPRESSION_CHANGE_MARGIN 0.10F

typedef struct AffectPrototype {
    float valence;
    float arousal;
    float dominance;
    float certainty;
    float warmth;
    float surprise;
} AffectPrototype;

static const char *const emotion_names[EIDOLON_GOEMOTIONS_COUNT] = {
    "admiration", "amusement",      "anger",       "annoyance", "approval",
    "caring",     "confusion",      "curiosity",   "desire",    "disappointment",
    "disapproval", "disgust",       "embarrassment", "excitement", "fear",
    "gratitude",  "grief",          "joy",         "love",      "nervousness",
    "optimism",   "pride",          "realization", "relief",    "remorse",
    "sadness",    "surprise",       "neutral",
};

/* Renderer-neutral semantic anchors, not claims about psychological ground truth. */
static const AffectPrototype emotion_prototypes[EIDOLON_GOEMOTIONS_COUNT] = {
    {0.75F, 0.20F, 0.10F, 0.55F, 0.75F, 0.10F},  /* admiration */
    {0.85F, 0.65F, 0.25F, 0.65F, 0.65F, 0.25F},  /* amusement */
    {-0.90F, 0.85F, 0.80F, 0.75F, -0.70F, 0.05F}, /* anger */
    {-0.60F, 0.55F, 0.55F, 0.65F, -0.45F, 0.00F}, /* annoyance */
    {0.55F, 0.15F, 0.35F, 0.80F, 0.45F, 0.00F},   /* approval */
    {0.65F, -0.10F, 0.05F, 0.55F, 0.95F, 0.00F},  /* caring */
    {-0.20F, 0.35F, -0.45F, -0.85F, 0.00F, 0.35F}, /* confusion */
    {0.35F, 0.45F, 0.05F, -0.20F, 0.35F, 0.35F},  /* curiosity */
    {0.55F, 0.55F, 0.25F, 0.45F, 0.65F, 0.10F},   /* desire */
    {-0.65F, -0.15F, -0.35F, 0.45F, -0.20F, 0.10F}, /* disappointment */
    {-0.65F, 0.35F, 0.55F, 0.75F, -0.55F, 0.00F}, /* disapproval */
    {-0.90F, 0.55F, 0.60F, 0.75F, -0.90F, 0.15F}, /* disgust */
    {-0.35F, 0.55F, -0.65F, 0.35F, 0.15F, 0.25F}, /* embarrassment */
    {0.90F, 0.95F, 0.35F, 0.65F, 0.60F, 0.45F},   /* excitement */
    {-0.85F, 0.90F, -0.90F, 0.20F, -0.35F, 0.40F}, /* fear */
    {0.85F, 0.35F, 0.05F, 0.70F, 0.95F, 0.10F},   /* gratitude */
    {-1.00F, -0.60F, -0.95F, 0.50F, -0.35F, 0.05F}, /* grief */
    {1.00F, 0.70F, 0.30F, 0.75F, 0.85F, 0.25F},   /* joy */
    {0.95F, 0.45F, 0.05F, 0.70F, 1.00F, 0.10F},   /* love */
    {-0.45F, 0.75F, -0.75F, -0.25F, 0.05F, 0.25F}, /* nervousness */
    {0.75F, 0.40F, 0.25F, 0.60F, 0.55F, 0.15F},   /* optimism */
    {0.65F, 0.40F, 0.85F, 0.85F, 0.25F, 0.05F},   /* pride */
    {0.20F, 0.40F, 0.10F, 0.75F, 0.05F, 0.75F},   /* realization */
    {0.70F, -0.45F, 0.05F, 0.70F, 0.55F, 0.05F},  /* relief */
    {-0.65F, -0.10F, -0.75F, 0.70F, 0.35F, 0.00F}, /* remorse */
    {-0.85F, -0.50F, -0.70F, 0.55F, -0.10F, 0.05F}, /* sadness */
    {0.05F, 0.90F, -0.10F, -0.40F, 0.05F, 1.00F}, /* surprise */
    {0.00F, -0.35F, 0.05F, 0.60F, 0.05F, 0.00F},  /* neutral */
};

/* Asuna manifest order: default, cheerful, responding, delighted, embarrassed,
 * serious, worried, annoyed, gentle, ecstatic. */
static const AffectPrototype expression_prototypes[10] = {
    {0.00F, -0.20F, 0.10F, 0.65F, 0.05F, 0.00F},
    {0.65F, 0.35F, 0.20F, 0.65F, 0.65F, 0.10F},
    {0.15F, 0.25F, 0.15F, 0.75F, 0.20F, 0.05F},
    {0.90F, 0.65F, 0.20F, 0.70F, 0.80F, 0.25F},
    {-0.20F, 0.45F, -0.55F, 0.10F, 0.25F, 0.20F},
    {-0.15F, 0.00F, 0.55F, 0.90F, -0.10F, 0.00F},
    {-0.55F, 0.45F, -0.65F, -0.15F, 0.05F, 0.20F},
    {-0.65F, 0.55F, 0.60F, 0.70F, -0.55F, 0.00F},
    {0.45F, -0.25F, 0.00F, 0.60F, 0.75F, 0.00F},
    {0.95F, 0.95F, 0.35F, 0.65F, 0.70F, 0.55F},
};

static const char *const expression_names[EIDOLON_EXPRESSION_COUNT] = {
    "default", "cheerful", "responding", "delighted", "embarrassed",
    "serious", "worried", "annoyed", "gentle", "ecstatic",
};

static float clamp_axis(float value) { return fmaxf(-1.0F, fminf(1.0F, value)); }

static EidolonAffect blend(EidolonAffect a, EidolonAffect b, float t) {
    return (EidolonAffect){
        a.valence + (b.valence - a.valence) * t,
        a.arousal + (b.arousal - a.arousal) * t,
        a.dominance + (b.dominance - a.dominance) * t,
        a.certainty + (b.certainty - a.certainty) * t,
        a.warmth + (b.warmth - a.warmth) * t,
        a.surprise + (b.surprise - a.surprise) * t,
    };
}

EidolonAffect eidolon_affect_for_state(EidolonState state) {
    static const EidolonAffect states[EIDOLON_STATE_COUNT] = {
        {0.35F, -0.35F, 0.05F, 0.65F, 0.60F, 0.00F},
        {0.15F, 0.30F, 0.15F, 0.70F, 0.25F, 0.00F},
        {-0.20F, 0.35F, -0.35F, -0.10F, 0.15F, 0.10F},
        {0.45F, 0.15F, 0.15F, 0.75F, 0.55F, 0.05F},
        {-0.65F, 0.55F, 0.45F, 0.55F, -0.35F, 0.05F},
    };
    if (state < 0 || state >= EIDOLON_STATE_COUNT) {
        state = EIDOLON_STATE_IDLE;
    }
    return states[state];
}

EidolonAffect eidolon_affect_from_goemotions(
    const float probabilities[EIDOLON_GOEMOTIONS_COUNT], EidolonAffect prior, float *evidence) {
    if (probabilities == NULL) {
        if (evidence != NULL) {
            *evidence = 0.0F;
        }
        return prior;
    }
    EidolonAffect sensed = {0};
    float total = 0.0F;
    float peak = 0.0F;
    for (size_t index = 0U; index < EIDOLON_GOEMOTIONS_COUNT; ++index) {
        const float probability = fmaxf(0.0F, fminf(1.0F, probabilities[index]));
        const float weight = probability * probability;
        const AffectPrototype *prototype = &emotion_prototypes[index];
        sensed.valence += prototype->valence * weight;
        sensed.arousal += prototype->arousal * weight;
        sensed.dominance += prototype->dominance * weight;
        sensed.certainty += prototype->certainty * weight;
        sensed.warmth += prototype->warmth * weight;
        sensed.surprise += prototype->surprise * weight;
        total += weight;
        peak = fmaxf(peak, probability);
    }
    if (total <= 0.0001F) {
        if (evidence != NULL) {
            *evidence = 0.0F;
        }
        return prior;
    }
    sensed.valence = clamp_axis(sensed.valence / total);
    sensed.arousal = clamp_axis(sensed.arousal / total);
    sensed.dominance = clamp_axis(sensed.dominance / total);
    sensed.certainty = clamp_axis(sensed.certainty / total);
    sensed.warmth = clamp_axis(sensed.warmth / total);
    sensed.surprise = clamp_axis(sensed.surprise / total);
    const float confidence = fminf(1.0F, peak * 1.35F);
    if (evidence != NULL) {
        *evidence = confidence;
    }
    return blend(prior, sensed, 0.25F + confidence * 0.70F);
}

static float distance_squared(const EidolonAffect *affect, const AffectPrototype *prototype) {
    const float dv = affect->valence - prototype->valence;
    const float da = affect->arousal - prototype->arousal;
    const float dd = affect->dominance - prototype->dominance;
    const float dc = affect->certainty - prototype->certainty;
    const float dw = affect->warmth - prototype->warmth;
    const float ds = affect->surprise - prototype->surprise;
    return dv * dv * 1.4F + da * da + dd * dd * 0.65F + dc * dc * 0.35F +
           dw * dw * 0.85F + ds * ds * 0.75F;
}

EidolonExpressionIntent eidolon_affect_expression(const EidolonAffect *affect) {
    if (affect == NULL) {
        return EIDOLON_EXPRESSION_DEFAULT;
    }
    int best = 0;
    float best_distance = distance_squared(affect, &expression_prototypes[0]);
    for (int index = 1; index < 10; ++index) {
        const float distance = distance_squared(affect, &expression_prototypes[index]);
        if (distance < best_distance) {
            best = index;
            best_distance = distance;
        }
    }
    return (EidolonExpressionIntent)best;
}

float eidolon_affect_expression_distance(const EidolonAffect *affect,
                                         EidolonExpressionIntent intent) {
    if (affect == NULL || intent < 0 || intent >= EIDOLON_EXPRESSION_COUNT) {
        return INFINITY;
    }
    return distance_squared(affect, &expression_prototypes[intent]);
}

void eidolon_affect_controller_init(EidolonAffectController *controller, EidolonState state,
                                    uint64_t now_ms) {
    memset(controller, 0, sizeof(*controller));
    controller->state = state;
    controller->current = eidolon_affect_for_state(state);
    controller->target = controller->current;
    controller->source = EIDOLON_AFFECT_SOURCE_STATE;
    controller->expression_intent = eidolon_affect_expression(&controller->current);
    controller->candidate_expression_intent = controller->expression_intent;
    controller->candidate_since_ms = now_ms;
}

void eidolon_affect_controller_set_state(EidolonAffectController *controller, EidolonState state,
                                         uint64_t now_ms) {
    if (controller == NULL || state < 0 || state >= EIDOLON_STATE_COUNT) {
        return;
    }
    controller->state = state;
    controller->target = eidolon_affect_for_state(state);
    controller->source = EIDOLON_AFFECT_SOURCE_STATE;
    controller->evidence = 0.0F;
    controller->expression_intent = eidolon_affect_expression(&controller->target);
    controller->candidate_expression_intent = controller->expression_intent;
    controller->candidate_since_ms = now_ms;
}

uint64_t eidolon_affect_controller_begin_text(EidolonAffectController *controller) {
    if (controller == NULL) {
        return 0U;
    }
    controller->sequence += 1U;
    controller->target = eidolon_affect_for_state(controller->state);
    controller->source = EIDOLON_AFFECT_SOURCE_STATE;
    controller->evidence = 0.0F;
    return controller->sequence;
}

bool eidolon_affect_controller_apply_goemotions(EidolonAffectController *controller,
                                                uint64_t sequence,
                                                const float probabilities[EIDOLON_GOEMOTIONS_COUNT],
                                                uint64_t now_ms) {
    if (controller == NULL || probabilities == NULL || sequence != controller->sequence ||
        sequence < controller->result_sequence) {
        return false;
    }
    controller->target = eidolon_affect_from_goemotions(
        probabilities, eidolon_affect_for_state(controller->state), &controller->evidence);
    controller->source = EIDOLON_AFFECT_SOURCE_GOEMOTIONS;
    controller->result_sequence = sequence;
    controller->candidate_since_ms = now_ms;
    return true;
}

void eidolon_affect_controller_update(EidolonAffectController *controller, float delta_seconds,
                                      uint64_t now_ms) {
    if (controller == NULL) {
        return;
    }
    const float response = 1.0F - expf(-fmaxf(0.0F, delta_seconds) * 7.0F);
    controller->current = blend(controller->current, controller->target, response);
    const EidolonExpressionIntent candidate = eidolon_affect_expression(&controller->current);
    if (candidate != controller->candidate_expression_intent) {
        controller->candidate_expression_intent = candidate;
        controller->candidate_since_ms = now_ms;
    }
    if (candidate == controller->expression_intent ||
        now_ms - controller->candidate_since_ms < EXPRESSION_DWELL_MS) {
        return;
    }
    const float current_distance =
        distance_squared(&controller->current,
                         &expression_prototypes[controller->expression_intent]);
    const float candidate_distance =
        distance_squared(&controller->current, &expression_prototypes[candidate]);
    if (candidate_distance + EXPRESSION_CHANGE_MARGIN < current_distance) {
        controller->expression_intent = candidate;
    }
}

void eidolon_affect_controller_perform(EidolonAffectController *controller,
                                       const EidolonAffect *affect,
                                       EidolonExpressionIntent expression, float evidence,
                                       uint64_t now_ms) {
    if (controller == NULL || affect == NULL || expression < 0 ||
        expression >= EIDOLON_EXPRESSION_COUNT) {
        return;
    }
    controller->current = *affect;
    controller->target = *affect;
    controller->source = EIDOLON_AFFECT_SOURCE_GOEMOTIONS;
    controller->evidence = fmaxf(0.0F, fminf(1.0F, evidence));
    controller->expression_intent = expression;
    controller->candidate_expression_intent = expression;
    controller->candidate_since_ms = now_ms;
}

const char *eidolon_goemotion_name(size_t index) {
    return index < EIDOLON_GOEMOTIONS_COUNT ? emotion_names[index] : "unknown";
}

const char *eidolon_expression_intent_name(EidolonExpressionIntent intent) {
    return intent >= 0 && intent < EIDOLON_EXPRESSION_COUNT ? expression_names[intent] : "default";
}
