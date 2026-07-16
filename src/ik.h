#ifndef EIDOLON_IK_H
#define EIDOLON_IK_H

#include <stdbool.h>

typedef struct EidolonIkTwoBoneInput {
    float root[3];
    float target[3];
    float pole[3];
    float fallback_direction[3];
    float upper_length;
    float lower_length;
    float soften_ratio;
} EidolonIkTwoBoneInput;

typedef struct EidolonIkTwoBoneSolution {
    float mid[3];
    float end[3];
    float target_distance;
    float solved_distance;
    bool target_reached;
} EidolonIkTwoBoneSolution;

bool eidolon_ik_solve_two_bone(const EidolonIkTwoBoneInput *input,
                               EidolonIkTwoBoneSolution *solution);

#endif
