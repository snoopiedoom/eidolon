#ifndef EIDOLON_MODEL_H
#define EIDOLON_MODEL_H

#include <SDL3/SDL.h>

#include "epr/performance_runtime.h"
#include "motion.h"
#include "pose.h"
#include "presentation.h"
#include "semantic_motion_pack.h"
#include "vrm_calibration.h"
#include "vrm_playback.h"

#define EIDOLON_MODEL_RENDER_RESOLUTION_MIN 512
#define EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT 1024
#define EIDOLON_MODEL_RENDER_RESOLUTION_MAX 2048

typedef struct EidolonModelRenderer EidolonModelRenderer;

typedef struct EidolonVrmRuntimeReport {
    size_t draw_count;
    size_t texture_count;
    size_t joint_count;
    uint64_t projection_revision;
    uint64_t base_revision;
    uint64_t playback_revision;
    uint64_t frame_sequence;
    bool geometry_ready;
    bool textures_ready;
    bool skinning_ready;
    bool shaders_ready;
    bool projection_ready;
    bool animation_ready;
    bool hidden_frame_ready;
} EidolonVrmRuntimeReport;

EidolonModelRenderer *eidolon_model_create(SDL_Renderer *renderer,
                                           EidolonPresentation *presentation,
                                           const char *model_path, const char *shader_directory,
                                           EidolonNeutralPose neutral_pose,
                                           EidolonIdleTuning idle_tuning);
void eidolon_model_update(EidolonModelRenderer *model, uint64_t now_ms);
bool eidolon_model_update_motion(EidolonModelRenderer *model, uint64_t now_ms);
void eidolon_model_request_redraw(EidolonModelRenderer *model);
bool eidolon_model_ready(const EidolonModelRenderer *model);
uint64_t eidolon_model_content_revision(const EidolonModelRenderer *model);
bool eidolon_model_render_presentation_target(EidolonModelRenderer *model,
                                              EidolonPresentation *presentation,
                                              const EidolonPresentationTargetUpdate *update);
bool eidolon_model_target_alpha_mask(const EidolonModelRenderer *model, const uint8_t **pixels,
                                     uint32_t *width, uint32_t *height, size_t *pitch);
void eidolon_model_set_rotation(EidolonModelRenderer *model, float yaw_radians, float pitch_radians,
                                float roll_radians);
void eidolon_model_set_neutral_pose(EidolonModelRenderer *model, float arm_lower_radians,
                                    float elbow_bend_add_radians);
void eidolon_model_set_semantic_pose(EidolonModelRenderer *model, const EidolonSemanticPose *pose);
void eidolon_model_clear_semantic_pose(EidolonModelRenderer *model);
void eidolon_model_set_idle_tuning(EidolonModelRenderer *model, EidolonIdleTuning tuning);
bool eidolon_model_body_profile(const EidolonModelRenderer *model, EidolonEprBodyProfile *profile);
bool eidolon_model_vrm_measurements(const EidolonModelRenderer *model,
                                    EidolonVrmMeasurements *measurements);
bool eidolon_model_vrm_calibration(const EidolonModelRenderer *model,
                                   EidolonVrmCalibration *calibration);
bool eidolon_model_set_vrm_calibration(EidolonModelRenderer *model,
                                       const EidolonVrmCalibration *calibration);
bool eidolon_model_apply_control(EidolonModelRenderer *model,
                                 const EidolonCanonicalControl *control);
bool eidolon_model_apply_control_calibrated(EidolonModelRenderer *model,
                                            const EidolonCanonicalControl *control,
                                            const EidolonVrmCalibration *calibration);
bool eidolon_model_apply_performance(EidolonModelRenderer *model,
                                     const EidolonCanonicalControl *control,
                                     const EidolonEprMotionFrame *motion_frame);
bool eidolon_model_epr_motion_load(EidolonModelRenderer *model,
                                   EidolonEprMotionGeneratorId generator, const char *path,
                                   uint64_t source_identity, bool loop);
bool eidolon_model_epr_motion_pack_load(EidolonModelRenderer *model,
                                        const EidolonSemanticMotionAsset *assets, size_t count);
bool eidolon_model_epr_motion_catalog(const EidolonModelRenderer *model,
                                      EidolonEprMotionCatalog *catalog);
bool eidolon_model_vrm_runtime_report(const EidolonModelRenderer *model,
                                      EidolonVrmRuntimeReport *report);
bool eidolon_model_vrm_animation_load(EidolonModelRenderer *model, const char *path,
                                      const char *identity, bool loop, uint64_t now_ms);
bool eidolon_model_vrm_animation_play(EidolonModelRenderer *model, uint64_t now_ms);
bool eidolon_model_vrm_animation_pause(EidolonModelRenderer *model, uint64_t now_ms);
bool eidolon_model_vrm_animation_seek(EidolonModelRenderer *model, float seconds, uint64_t now_ms);
bool eidolon_model_vrm_animation_set_loop(EidolonModelRenderer *model, bool loop);
bool eidolon_model_vrm_animation_set_rate(EidolonModelRenderer *model, float playback_rate,
                                          uint64_t now_ms);
bool eidolon_model_vrm_animation_report(const EidolonModelRenderer *model,
                                        EidolonVrmPlaybackReport *report);
uint64_t eidolon_model_vrm_projection_revision(const EidolonModelRenderer *model);
const char *eidolon_model_body_name(const EidolonModelRenderer *model);
bool eidolon_model_set_render_resolution(EidolonModelRenderer *model, int side);
int eidolon_model_render_resolution(const EidolonModelRenderer *model);
uint64_t eidolon_model_presented_transform_revision(const EidolonModelRenderer *model);
uint64_t eidolon_model_presented_frame_sequence(const EidolonModelRenderer *model);
void eidolon_model_destroy(EidolonModelRenderer *model);
SDL_Texture *eidolon_model_texture(const EidolonModelRenderer *model);

#endif
