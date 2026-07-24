#include "ik.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define IK_EPSILON 0.00001F

static float vector_dot(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static void vector_subtract(const float left[3], const float right[3], float result[3]) {
    result[0] = left[0] - right[0];
    result[1] = left[1] - right[1];
    result[2] = left[2] - right[2];
}

static float clampf(float value, float minimum, float maximum) {
    return fminf(fmaxf(value, minimum), maximum);
}

static float vector_length(const float vector[3]) { return sqrtf(vector_dot(vector, vector)); }

static bool vector_is_finite(const float vector[3]) {
    return isfinite(vector[0]) && isfinite(vector[1]) && isfinite(vector[2]);
}

static bool vector_normalize(float vector[3]) {
    const float length = vector_length(vector);
    if (length <= IK_EPSILON) {
        return false;
    }
    const float inverse = 1.0F / length;
    vector[0] *= inverse;
    vector[1] *= inverse;
    vector[2] *= inverse;
    return true;
}

static void fallback_perpendicular(const float direction[3], float perpendicular[3]) {
    const float axis[3] = {fabsf(direction[1]) < 0.9F ? 0.0F : 1.0F,
                           fabsf(direction[1]) < 0.9F ? 1.0F : 0.0F, 0.0F};
    const float projection = vector_dot(axis, direction);
    perpendicular[0] = axis[0] - direction[0] * projection;
    perpendicular[1] = axis[1] - direction[1] * projection;
    perpendicular[2] = axis[2] - direction[2] * projection;
    (void)vector_normalize(perpendicular);
}

static float softened_distance(float distance, float reach, float soften_ratio) {
    const float ratio = clampf(soften_ratio, 0.0F, 0.95F);
    if (ratio <= 0.0F) {
        return fminf(distance, reach);
    }
    const float soft_start = reach * (1.0F - ratio);
    if (distance <= soft_start) {
        return distance;
    }
    const float soft_range = reach - soft_start;
    const float excess = distance - soft_start;
    return soft_start + soft_range * (1.0F - expf(-excess / soft_range));
}

bool eidolon_ik_solve_two_bone(const EidolonIkTwoBoneInput *input,
                               EidolonIkTwoBoneSolution *solution) {
    if (input == NULL || solution == NULL || input->upper_length <= IK_EPSILON ||
        input->lower_length <= IK_EPSILON || !isfinite(input->upper_length) ||
        !isfinite(input->lower_length) || !isfinite(input->soften_ratio) ||
        !vector_is_finite(input->root) || !vector_is_finite(input->target) ||
        !vector_is_finite(input->pole) || !vector_is_finite(input->fallback_direction)) {
        return false;
    }

    float direction[3];
    vector_subtract(input->target, input->root, direction);
    const float target_distance = vector_length(direction);
    if (!vector_normalize(direction)) {
        memcpy(direction, input->fallback_direction, sizeof(direction));
        if (!vector_normalize(direction)) {
            direction[0] = 1.0F;
            direction[1] = 0.0F;
            direction[2] = 0.0F;
        }
    }

    const float maximum = input->upper_length + input->lower_length - IK_EPSILON;
    const float minimum = fabsf(input->upper_length - input->lower_length) + IK_EPSILON;
    const float softened = softened_distance(target_distance, maximum, input->soften_ratio);
    const float solved_distance = clampf(softened, minimum, maximum);

    float pole_direction[3];
    vector_subtract(input->pole, input->root, pole_direction);
    const float along = vector_dot(pole_direction, direction);
    pole_direction[0] -= direction[0] * along;
    pole_direction[1] -= direction[1] * along;
    pole_direction[2] -= direction[2] * along;
    if (!vector_normalize(pole_direction)) {
        fallback_perpendicular(direction, pole_direction);
    }

    const float upper_squared = input->upper_length * input->upper_length;
    const float lower_squared = input->lower_length * input->lower_length;
    const float distance_squared = solved_distance * solved_distance;
    const float along_distance =
        (upper_squared - lower_squared + distance_squared) / (2.0F * solved_distance);
    const float bend_squared = fmaxf(0.0F, upper_squared - along_distance * along_distance);
    const float bend_distance = sqrtf(bend_squared);

    for (size_t axis = 0; axis < 3; ++axis) {
        solution->mid[axis] = input->root[axis] + direction[axis] * along_distance +
                              pole_direction[axis] * bend_distance;
        solution->end[axis] = input->root[axis] + direction[axis] * solved_distance;
    }
    solution->target_distance = target_distance;
    solution->solved_distance = solved_distance;
    const float reach_tolerance = IK_EPSILON * fmaxf(1.0F, target_distance);
    solution->target_reached = fabsf(target_distance - solved_distance) <= reach_tolerance;
    return true;
}
