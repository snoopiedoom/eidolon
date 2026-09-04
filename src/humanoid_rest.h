#ifndef EIDOLON_HUMANOID_REST_H
#define EIDOLON_HUMANOID_REST_H

#include <stdbool.h>

/* All functions leave result untouched when an input is rejected. */
bool eidolon_humanoid_rest_rotation_to_normalized(const float rest_local[4],
                                                  const float rest_world[4],
                                                  const float authored_local[4], float result[4]);
bool eidolon_humanoid_rest_rotation_from_normalized(const float rest_local[4],
                                                    const float rest_world[4],
                                                    const float normalized[4], float result[4]);
bool eidolon_humanoid_rest_translation_to_normalized(const float rest_translation[3],
                                                     float rest_hips_height,
                                                     const float authored_translation[3],
                                                     float result[3]);
bool eidolon_humanoid_rest_translation_from_normalized(const float rest_translation[3],
                                                       float rest_hips_height,
                                                       const float normalized[3], float result[3]);

#endif
