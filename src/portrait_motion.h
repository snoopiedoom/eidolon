#ifndef EIDOLON_PORTRAIT_MOTION_H
#define EIDOLON_PORTRAIT_MOTION_H

#include <stdint.h>

typedef struct EidolonPortraitSpring {
    float x;
    float y;
    float scale;
    float angle;
    float velocity_x;
    float velocity_y;
    float velocity_scale;
    float velocity_angle;
    uint64_t last_update_ms;
} EidolonPortraitSpring;

void eidolon_portrait_spring_reset(EidolonPortraitSpring *spring, uint64_t now_ms);
void eidolon_portrait_spring_update(EidolonPortraitSpring *spring, uint64_t now_ms);
void eidolon_portrait_spring_impulse(EidolonPortraitSpring *spring, float velocity_x,
                                     float velocity_y, float velocity_scale, float velocity_angle);

#endif
