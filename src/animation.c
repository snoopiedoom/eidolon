#include "animation.h"

typedef struct AnimationDefinition {
    int row;
    int frame_count;
    const uint16_t *durations_ms;
} AnimationDefinition;

static const uint16_t IDLE_DURATIONS[] = {280, 110, 110, 140, 140, 320};
static const uint16_t RUNNING_DURATIONS[] = {120, 120, 120, 120, 120, 220};
static const uint16_t WAITING_DURATIONS[] = {150, 150, 150, 150, 150, 260};
static const uint16_t REVIEW_DURATIONS[] = {150, 150, 150, 150, 150, 280};
static const uint16_t FAILED_DURATIONS[] = {140, 140, 140, 140, 140, 140, 140, 240};

static const AnimationDefinition DEFINITIONS[EIDOLON_STATE_COUNT] = {
    [EIDOLON_STATE_IDLE] = {0, 6, IDLE_DURATIONS},
    [EIDOLON_STATE_RUNNING] = {7, 6, RUNNING_DURATIONS},
    [EIDOLON_STATE_WAITING] = {6, 6, WAITING_DURATIONS},
    [EIDOLON_STATE_REVIEW] = {8, 6, REVIEW_DURATIONS},
    [EIDOLON_STATE_FAILED] = {5, 8, FAILED_DURATIONS},
};

static const AnimationDefinition *definition_for(EidolonState state) {
    if (state < 0 || state >= EIDOLON_STATE_COUNT) {
        return &DEFINITIONS[EIDOLON_STATE_IDLE];
    }
    return &DEFINITIONS[state];
}

void eidolon_animation_set_state(EidolonAnimation *animation, EidolonState state,
                                 uint64_t now_ms) {
    const AnimationDefinition *definition = definition_for(state);
    animation->row = definition->row;
    animation->frame_count = definition->frame_count;
    animation->frame = 0;
    animation->frame_started_ms = now_ms;
}

void eidolon_animation_update(EidolonAnimation *animation, EidolonState state,
                              uint64_t now_ms) {
    const AnimationDefinition *definition = definition_for(state);
    const uint64_t elapsed_ms = now_ms - animation->frame_started_ms;
    const uint16_t duration_ms = definition->durations_ms[animation->frame];

    if (elapsed_ms < duration_ms) {
        return;
    }

    animation->frame = (animation->frame + 1) % definition->frame_count;
    animation->frame_started_ms = now_ms;
}

SDL_FRect eidolon_animation_source_rect(const EidolonAnimation *animation) {
    return (SDL_FRect){
        .x = (float)(animation->frame * EIDOLON_CELL_WIDTH),
        .y = (float)(animation->row * EIDOLON_CELL_HEIGHT),
        .w = EIDOLON_CELL_WIDTH,
        .h = EIDOLON_CELL_HEIGHT,
    };
}

