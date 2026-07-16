#include "pose.h"

#include <SDL3/SDL.h>

/*
 * These are intentionally guesses, not canonical animation data. Each reviewed correction becomes
 * evidence for the next pose and, as the rig gains channels, this structure grows with it.
 */
static const EidolonSemanticPose POSES[] = {
    {
        .name = "BIND A / CALIBRATION",
        .intent = "raw symmetric reference",
        .arms = {{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 0.0F},
                 {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 0.0F}},
        .soften_ratio = 0.0F,
    },
    {
        .name = "RELAXED / OPEN",
        .intent = "proper posture with relaxed arms clearing the torso",
        .arms = {{{-0.06F, -0.90F, 0.08F}, {0.15F, -0.50F, -0.28F}, 1.0F},
                 {{-0.06F, -0.90F, 0.08F}, {0.15F, -0.50F, -0.28F}, 1.0F}},
        .soften_ratio = 0.04F,
    },
    {
        .name = "ATTENTIVE",
        .intent = "quiet attention with hands gathered in front",
        .arms = {{{-0.34F, -0.74F, 0.20F}, {0.48F, -0.30F, 0.40F}, 1.0F},
                 {{-0.34F, -0.74F, 0.20F}, {0.48F, -0.30F, 0.40F}, 1.0F}},
        .soften_ratio = 0.06F,
    },
    {
        .name = "RESERVED / GUARDED",
        .intent = "contained silhouette prototype",
        .arms = {{{-0.65F, -0.38F, 0.30F}, {0.52F, -0.12F, 0.48F}, 1.0F},
                 {{-0.65F, -0.38F, 0.30F}, {0.52F, -0.12F, 0.48F}, 1.0F}},
        .soften_ratio = 0.06F,
    },
    {
        .name = "PLAYFUL / OPEN",
        .intent = "light, inviting silhouette prototype",
        .arms = {{{0.75F, -0.50F, 0.18F}, {0.70F, 0.00F, 0.65F}, 1.0F},
                 {{0.75F, -0.50F, 0.18F}, {0.70F, 0.00F, 0.65F}, 1.0F}},
        .soften_ratio = 0.08F,
    },
};

size_t eidolon_semantic_pose_count(void) { return SDL_arraysize(POSES); }

const EidolonSemanticPose *eidolon_semantic_pose(size_t index) {
    return index < SDL_arraysize(POSES) ? &POSES[index] : NULL;
}
