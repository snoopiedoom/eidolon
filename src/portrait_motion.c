#include "portrait_motion.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <string.h>

#define SPRING_STEP_SECONDS (1.0F / 120.0F)
#define SPRING_MAX_ELAPSED_SECONDS 0.1F

static void step_channel(float *value, float *velocity, float stiffness, float damping,
                         float elapsed) {
    const float acceleration = -*value * stiffness - *velocity * damping;
    *velocity += acceleration * elapsed;
    *value += *velocity * elapsed;
}

static void clamp_channel(float *value, float *velocity, float limit, float velocity_limit) {
    *value = SDL_clamp(*value, -limit, limit);
    *velocity = SDL_clamp(*velocity, -velocity_limit, velocity_limit);
    if (fabsf(*value) < 0.00001F && fabsf(*velocity) < 0.00001F) {
        *value = 0.0F;
        *velocity = 0.0F;
    }
}

void eidolon_portrait_spring_reset(EidolonPortraitSpring *spring, uint64_t now_ms) {
    if (spring == NULL) {
        return;
    }
    memset(spring, 0, sizeof(*spring));
    spring->last_update_ms = now_ms;
}

void eidolon_portrait_spring_update(EidolonPortraitSpring *spring, uint64_t now_ms) {
    if (spring == NULL) {
        return;
    }
    if (spring->last_update_ms == 0U || now_ms < spring->last_update_ms) {
        spring->last_update_ms = now_ms;
        return;
    }
    float remaining =
        SDL_min((float)(now_ms - spring->last_update_ms) / 1000.0F, SPRING_MAX_ELAPSED_SECONDS);
    spring->last_update_ms = now_ms;
    while (remaining > 0.0F) {
        const float elapsed = SDL_min(remaining, SPRING_STEP_SECONDS);
        step_channel(&spring->x, &spring->velocity_x, 42.0F, 9.8F, elapsed);
        step_channel(&spring->y, &spring->velocity_y, 46.0F, 10.4F, elapsed);
        step_channel(&spring->scale, &spring->velocity_scale, 54.0F, 11.8F, elapsed);
        step_channel(&spring->angle, &spring->velocity_angle, 48.0F, 10.8F, elapsed);
        remaining -= elapsed;
    }
    clamp_channel(&spring->x, &spring->velocity_x, 8.0F, 110.0F);
    clamp_channel(&spring->y, &spring->velocity_y, 11.0F, 140.0F);
    clamp_channel(&spring->scale, &spring->velocity_scale, 0.022F, 0.20F);
    clamp_channel(&spring->angle, &spring->velocity_angle, 0.85F, 8.0F);
}

void eidolon_portrait_spring_impulse(EidolonPortraitSpring *spring, float velocity_x,
                                     float velocity_y, float velocity_scale, float velocity_angle) {
    if (spring == NULL) {
        return;
    }
    spring->velocity_x = SDL_clamp(spring->velocity_x + velocity_x, -110.0F, 110.0F);
    spring->velocity_y = SDL_clamp(spring->velocity_y + velocity_y, -140.0F, 140.0F);
    spring->velocity_scale = SDL_clamp(spring->velocity_scale + velocity_scale, -0.20F, 0.20F);
    spring->velocity_angle = SDL_clamp(spring->velocity_angle + velocity_angle, -8.0F, 8.0F);
}
