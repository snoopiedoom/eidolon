#ifndef EIDOLON_MODEL_H
#define EIDOLON_MODEL_H

#include <SDL3/SDL.h>

#include "motion.h"
#include "pose.h"

#define EIDOLON_MODEL_RENDER_RESOLUTION_MIN 512
#define EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT 1024
#define EIDOLON_MODEL_RENDER_RESOLUTION_MAX 2048

typedef struct EidolonModelRenderer EidolonModelRenderer;

EidolonModelRenderer *eidolon_model_create(SDL_Renderer *renderer, const char *model_path,
                                           const char *shader_directory,
                                           EidolonNeutralPose neutral_pose,
                                           EidolonIdleTuning idle_tuning);
void eidolon_model_update(EidolonModelRenderer *model, uint64_t now_ms);
void eidolon_model_request_redraw(EidolonModelRenderer *model);
void eidolon_model_set_rotation(EidolonModelRenderer *model, float yaw_radians, float pitch_radians,
                                float roll_radians);
void eidolon_model_set_neutral_pose(EidolonModelRenderer *model, float arm_lower_radians,
                                    float elbow_bend_add_radians);
void eidolon_model_set_semantic_pose(EidolonModelRenderer *model, const EidolonSemanticPose *pose);
void eidolon_model_clear_semantic_pose(EidolonModelRenderer *model);
void eidolon_model_set_idle_tuning(EidolonModelRenderer *model, EidolonIdleTuning tuning);
bool eidolon_model_set_render_resolution(EidolonModelRenderer *model, int side);
int eidolon_model_render_resolution(const EidolonModelRenderer *model);
uint64_t eidolon_model_presented_transform_revision(const EidolonModelRenderer *model);
uint64_t eidolon_model_presented_frame_sequence(const EidolonModelRenderer *model);
void eidolon_model_destroy(EidolonModelRenderer *model);
SDL_Texture *eidolon_model_texture(const EidolonModelRenderer *model);

#endif
