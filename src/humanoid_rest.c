#include "humanoid_rest.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define HUMANOID_REST_EPSILON 0.000001F

static bool quaternion_normalize(float quaternion[4]) {
    const float length_squared = quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                 quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3];
    if (!isfinite(length_squared) || length_squared <= HUMANOID_REST_EPSILON) {
        return false;
    }
    const float scale = 1.0F / sqrtf(length_squared);
    for (size_t component = 0U; component < 4U; ++component) {
        quaternion[component] *= scale;
    }
    return true;
}

static void quaternion_inverse(const float quaternion[4], float result[4]) {
    result[0] = -quaternion[0];
    result[1] = -quaternion[1];
    result[2] = -quaternion[2];
    result[3] = quaternion[3];
}

static void quaternion_multiply(const float left[4], const float right[4], float result[4]) {
    const float product[4] = {
        left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1],
        left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0],
        left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3],
        left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2],
    };
    memcpy(result, product, sizeof(product));
}

static bool normalized_quaternion_copy(const float source[4], float result[4]) {
    if (source == NULL || result == NULL) {
        return false;
    }
    memcpy(result, source, sizeof(float) * 4U);
    return quaternion_normalize(result);
}

bool eidolon_humanoid_rest_rotation_to_normalized(const float rest_local[4],
                                                  const float rest_world[4],
                                                  const float authored_local[4], float result[4]) {
    float local_rest[4];
    float world_rest[4];
    float local[4];
    float inverse_local[4];
    float inverse_world[4];
    float first[4];
    float second[4];
    float candidate[4];
    if (result == NULL || !normalized_quaternion_copy(rest_local, local_rest) ||
        !normalized_quaternion_copy(rest_world, world_rest) ||
        !normalized_quaternion_copy(authored_local, local)) {
        return false;
    }
    quaternion_inverse(local_rest, inverse_local);
    quaternion_inverse(world_rest, inverse_world);
    quaternion_multiply(world_rest, inverse_local, first);
    quaternion_multiply(first, local, second);
    quaternion_multiply(second, inverse_world, candidate);
    if (!quaternion_normalize(candidate)) {
        return false;
    }
    memcpy(result, candidate, sizeof(candidate));
    return true;
}

bool eidolon_humanoid_rest_rotation_from_normalized(const float rest_local[4],
                                                    const float rest_world[4],
                                                    const float normalized[4], float result[4]) {
    float local_rest[4];
    float world_rest[4];
    float normalized_rotation[4];
    float inverse_world[4];
    float first[4];
    float second[4];
    float candidate[4];
    if (result == NULL || !normalized_quaternion_copy(rest_local, local_rest) ||
        !normalized_quaternion_copy(rest_world, world_rest) ||
        !normalized_quaternion_copy(normalized, normalized_rotation)) {
        return false;
    }
    quaternion_inverse(world_rest, inverse_world);
    quaternion_multiply(inverse_world, normalized_rotation, first);
    quaternion_multiply(first, world_rest, second);
    quaternion_multiply(local_rest, second, candidate);
    if (!quaternion_normalize(candidate)) {
        return false;
    }
    memcpy(result, candidate, sizeof(candidate));
    return true;
}

static bool finite3(const float values[3]) {
    return values != NULL && isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]);
}

bool eidolon_humanoid_rest_translation_to_normalized(const float rest_translation[3],
                                                     float rest_hips_height,
                                                     const float authored_translation[3],
                                                     float result[3]) {
    float candidate[3];
    if (result == NULL || !finite3(rest_translation) || !finite3(authored_translation) ||
        !isfinite(rest_hips_height) || rest_hips_height <= HUMANOID_REST_EPSILON) {
        return false;
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        candidate[axis] = (authored_translation[axis] - rest_translation[axis]) / rest_hips_height;
        if (!isfinite(candidate[axis])) {
            return false;
        }
    }
    memcpy(result, candidate, sizeof(candidate));
    return true;
}

bool eidolon_humanoid_rest_translation_from_normalized(const float rest_translation[3],
                                                       float rest_hips_height,
                                                       const float normalized[3], float result[3]) {
    float candidate[3];
    if (result == NULL || !finite3(rest_translation) || !finite3(normalized) ||
        !isfinite(rest_hips_height) || rest_hips_height <= HUMANOID_REST_EPSILON) {
        return false;
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        candidate[axis] = rest_translation[axis] + normalized[axis] * rest_hips_height;
        if (!isfinite(candidate[axis])) {
            return false;
        }
    }
    memcpy(result, candidate, sizeof(candidate));
    return true;
}
