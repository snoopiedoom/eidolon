#ifndef EIDOLON_PRESENTATION_H
#define EIDOLON_PRESENTATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scene.h"

typedef struct EidolonPresentation EidolonPresentation;

typedef struct EidolonPresentationHost {
    uint32_t value;
} EidolonPresentationHost;

typedef struct EidolonPresentationOutput {
    uint32_t value;
} EidolonPresentationOutput;

typedef struct EidolonPresentationLayer {
    uint32_t value;
} EidolonPresentationLayer;

typedef struct EidolonPresentationTarget {
    uint32_t value;
} EidolonPresentationTarget;

typedef enum EidolonPresentationAlphaMode {
    EIDOLON_PRESENTATION_ALPHA_STRAIGHT,
    EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED,
} EidolonPresentationAlphaMode;

typedef struct EidolonPresentationTargetUpdate {
    EidolonPresentationTarget target;
    uint64_t generation;
    uint64_t content_revision;
    uint32_t width;
    uint32_t height;
    EidolonPresentationAlphaMode alpha_mode;
    bool redraw_required;
} EidolonPresentationTargetUpdate;

typedef struct EidolonPresentationCommittedLayer {
    EidolonSceneLayerSnapshot scene;
    EidolonPresentationTarget target;
    uint64_t target_generation;
    uint64_t target_content_revision;
    uint32_t target_width;
    uint32_t target_height;
    EidolonPresentationAlphaMode alpha_mode;
    bool has_target;
} EidolonPresentationCommittedLayer;

typedef struct EidolonPresentationSceneCommit {
    uint64_t revision;
    size_t layer_count;
    EidolonPresentationCommittedLayer layers[EIDOLON_SCENE_LAYER_CAPACITY];
} EidolonPresentationSceneCommit;

typedef enum EidolonPresentationCapability {
    EIDOLON_PRESENTATION_CAP_PERSISTENT_OVER_OTHER_APPS = UINT64_C(1) << 0,
    EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT = UINT64_C(1) << 1,
    EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS = UINT64_C(1) << 2,
    EIDOLON_PRESENTATION_CAP_COMPOSITOR_TRANSFORM = UINT64_C(1) << 3,
    EIDOLON_PRESENTATION_CAP_COMPOSITOR_OPACITY = UINT64_C(1) << 4,
    EIDOLON_PRESENTATION_CAP_COMPOSITOR_ANIMATION = UINT64_C(1) << 5,
    EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT = UINT64_C(1) << 6,
    EIDOLON_PRESENTATION_CAP_NATIVE_INTERACTIVE_MOVE = UINT64_C(1) << 7,
    EIDOLON_PRESENTATION_CAP_GPU_ZERO_COPY = UINT64_C(1) << 8,
    EIDOLON_PRESENTATION_CAP_FEEDBACK = UINT64_C(1) << 9,
    EIDOLON_PRESENTATION_CAP_BACKGROUND_VISIBILITY = UINT64_C(1) << 10,
} EidolonPresentationCapability;

typedef struct EidolonPresentationGeometry {
    int x;
    int y;
    int width;
    int height;
} EidolonPresentationGeometry;

typedef struct EidolonPresentationRect {
    float x;
    float y;
    float width;
    float height;
} EidolonPresentationRect;

typedef struct EidolonPresentationInsets {
    float top;
    float right;
    float bottom;
    float left;
} EidolonPresentationInsets;

typedef enum EidolonPresentationOrientation {
    EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN,
    EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE,
    EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT,
    EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE_FLIPPED,
    EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT_FLIPPED,
} EidolonPresentationOrientation;

typedef enum EidolonPresentationCoordinateSpace {
    EIDOLON_PRESENTATION_COORDINATE_SPACE_UNKNOWN,
    EIDOLON_PRESENTATION_COORDINATE_SPACE_OUTPUT_LOGICAL,
    EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_LOGICAL,
    EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL,
} EidolonPresentationCoordinateSpace;

typedef enum EidolonPresentationEnvironmentField {
    EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY = UINT64_C(1) << 0,
    EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT = UINT64_C(1) << 1,
    EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS = UINT64_C(1) << 2,
    EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS = UINT64_C(1) << 3,
    EIDOLON_PRESENTATION_ENV_SAFE_AREA = UINT64_C(1) << 4,
    EIDOLON_PRESENTATION_ENV_CONTENT_SCALE = UINT64_C(1) << 5,
    EIDOLON_PRESENTATION_ENV_PIXEL_SCALE = UINT64_C(1) << 6,
    EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH = UINT64_C(1) << 7,
    EIDOLON_PRESENTATION_ENV_ORIENTATION = UINT64_C(1) << 8,
    EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE = UINT64_C(1) << 9,
    EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY = UINT64_C(1) << 10,
    EIDOLON_PRESENTATION_ENV_CAPABILITIES = UINT64_C(1) << 11,
} EidolonPresentationEnvironmentField;

#define EIDOLON_PRESENTATION_ENV_ALL_FIELDS ((UINT64_C(1) << 12) - UINT64_C(1))

typedef struct EidolonPresentationEnvironment {
    uint64_t revision;
    uint64_t topology_revision;
    EidolonPresentationHost host;
    EidolonPresentationOutput active_output;
    EidolonPresentationGeometry host_geometry;
    EidolonPresentationRect output_bounds;
    EidolonPresentationRect usable_bounds;
    EidolonPresentationInsets safe_area;
    float content_scale;
    float pixel_scale;
    float nominal_refresh_hz;
    EidolonPresentationOrientation orientation;
    EidolonPresentationCoordinateSpace coordinate_space;
    uint64_t capabilities;
    uint64_t valid_fields;
    uint64_t changed_fields;
} EidolonPresentationEnvironment;

typedef enum EidolonPresentationOutputFlag {
    EIDOLON_PRESENTATION_OUTPUT_PRIMARY = UINT64_C(1) << 0,
} EidolonPresentationOutputFlag;

typedef struct EidolonPresentationOutputInfo {
    EidolonPresentationOutput output;
    EidolonPresentationRect bounds;
    EidolonPresentationRect usable_bounds;
    EidolonPresentationInsets safe_area;
    float content_scale;
    float pixel_scale;
    float nominal_refresh_hz;
    EidolonPresentationOrientation orientation;
    EidolonPresentationCoordinateSpace coordinate_space;
    uint64_t capabilities;
    uint64_t flags;
    uint64_t valid_fields;
} EidolonPresentationOutputInfo;

typedef enum EidolonPresentationTopologyStatus {
    EIDOLON_PRESENTATION_TOPOLOGY_OK,
    EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY,
    EIDOLON_PRESENTATION_TOPOLOGY_CHANGED,
    EIDOLON_PRESENTATION_TOPOLOGY_UNAVAILABLE,
    EIDOLON_PRESENTATION_TOPOLOGY_ERROR,
} EidolonPresentationTopologyStatus;

typedef struct EidolonPresentationTopologyResult {
    uint64_t revision;
    size_t required_count;
    size_t copied_count;
    EidolonPresentationTopologyStatus status;
} EidolonPresentationTopologyResult;

typedef enum EidolonPresentationEventKind {
    EIDOLON_PRESENTATION_EVENT_NONE,
    EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED,
    EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED,
    EIDOLON_PRESENTATION_EVENT_MOVE_STARTED,
    EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED,
    EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED,
    EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED,
    EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED,
    EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED,
    EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED,
} EidolonPresentationEventKind;

typedef struct EidolonPresentationLayerEvent {
    uint64_t scene_revision;
    EidolonSceneLayerId layer;
    float host_x;
    float host_y;
    float layer_x;
    float layer_y;
} EidolonPresentationLayerEvent;

typedef struct EidolonPresentationMoveEvent {
    uint64_t scene_revision;
    uint64_t environment_revision;
    EidolonSceneLayerId layer;
    EidolonPresentationGeometry geometry;
    float host_x;
    float host_y;
    float layer_x;
    float layer_y;
} EidolonPresentationMoveEvent;

typedef struct EidolonPresentationEnvironmentChanged {
    EidolonPresentationEnvironment environment;
} EidolonPresentationEnvironmentChanged;

typedef enum EidolonPresentationGraphicsResetKind {
    EIDOLON_PRESENTATION_GRAPHICS_RESET_NONE,
    EIDOLON_PRESENTATION_GRAPHICS_RESET_TARGETS,
    EIDOLON_PRESENTATION_GRAPHICS_RESET_DEVICE,
    EIDOLON_PRESENTATION_GRAPHICS_RESET_BACKEND,
} EidolonPresentationGraphicsResetKind;

typedef struct EidolonPresentationGraphicsEvent {
    EidolonPresentationGraphicsResetKind reset_kind;
} EidolonPresentationGraphicsEvent;

typedef union EidolonPresentationEventData {
    EidolonPresentationLayerEvent layer;
    EidolonPresentationMoveEvent move;
    EidolonPresentationEnvironmentChanged environment;
    EidolonPresentationGraphicsEvent graphics;
} EidolonPresentationEventData;

typedef struct EidolonPresentationEvent {
    EidolonPresentationEventKind kind;
    uint64_t sequence;
    uint64_t monotonic_ns;
    EidolonPresentationHost host;
    EidolonPresentationEventData data;
} EidolonPresentationEvent;

typedef struct EidolonPresentationBackendOps {
    void (*destroy)(void *context);
    bool (*configure_host)(void *context);
    bool (*get_geometry)(void *context, EidolonPresentationGeometry *geometry);
    bool (*set_geometry)(void *context, const EidolonPresentationGeometry *geometry);
    bool (*sync_host)(void *context);
    float (*display_scale)(void *context);
    bool (*set_vsync)(void *context, int interval);
    bool (*begin_interactive_move)(void *context);
    void (*suspend_input_region)(void *context);
    bool (*update_input_region)(void *context);
    bool (*poll_event)(void *context, EidolonPresentationEvent *event);
    bool (*create_target)(void *context, EidolonSceneLayerId layer,
                          EidolonPresentationTarget target, uint64_t generation, uint32_t width,
                          uint32_t height, EidolonPresentationAlphaMode alpha_mode);
    void (*destroy_target)(void *context, EidolonPresentationTarget target);
    bool (*set_target_alpha_mask)(void *context, EidolonPresentationTarget target,
                                  uint64_t generation, const uint8_t *pixels, size_t pitch,
                                  uint8_t pixel_stride, uint8_t alpha_offset);
    bool (*submit_target)(void *context, EidolonPresentationTarget target, uint64_t generation);
    bool (*commit_scene)(void *context, const EidolonPresentationSceneCommit *commit);
    bool (*present)(void *context);
    bool (*get_environment)(void *context, EidolonPresentationEnvironment *environment);
    EidolonPresentationTopologyResult (*copy_outputs)(void *context,
                                                      EidolonPresentationOutputInfo *outputs,
                                                      size_t capacity);
} EidolonPresentationBackendOps;

#ifdef __cplusplus
extern "C" {
#endif

EidolonPresentation *
eidolon_presentation_create_backend(const char *backend_name, uint64_t capabilities, void *context,
                                    const EidolonPresentationBackendOps *operations);
void eidolon_presentation_destroy(EidolonPresentation *presentation);

const char *eidolon_presentation_backend_name(const EidolonPresentation *presentation);
uint64_t eidolon_presentation_capabilities(const EidolonPresentation *presentation);
bool eidolon_presentation_supports(const EidolonPresentation *presentation,
                                   EidolonPresentationCapability capability);
EidolonPresentationHost eidolon_presentation_host(const EidolonPresentation *presentation);

bool eidolon_presentation_configure_host(EidolonPresentation *presentation);
bool eidolon_presentation_get_geometry(EidolonPresentation *presentation,
                                       EidolonPresentationGeometry *geometry);
bool eidolon_presentation_set_geometry(EidolonPresentation *presentation,
                                       const EidolonPresentationGeometry *geometry);
bool eidolon_presentation_sync_host(EidolonPresentation *presentation);
float eidolon_presentation_display_scale(EidolonPresentation *presentation);
bool eidolon_presentation_set_vsync(EidolonPresentation *presentation, int interval);
bool eidolon_presentation_begin_interactive_move(EidolonPresentation *presentation);
void eidolon_presentation_suspend_input_region(EidolonPresentation *presentation);
bool eidolon_presentation_update_input_region(EidolonPresentation *presentation);
bool eidolon_presentation_poll_event(EidolonPresentation *presentation,
                                     EidolonPresentationEvent *event);
bool eidolon_presentation_get_environment(EidolonPresentation *presentation,
                                          EidolonPresentationEnvironment *environment);
EidolonPresentationTopologyResult
eidolon_presentation_copy_outputs(EidolonPresentation *presentation,
                                  EidolonPresentationOutputInfo *outputs, size_t capacity);
bool eidolon_presentation_begin_target_update(EidolonPresentation *presentation,
                                              EidolonSceneLayerId layer, uint32_t width,
                                              uint32_t height,
                                              EidolonPresentationAlphaMode alpha_mode,
                                              uint64_t content_revision,
                                              EidolonPresentationTargetUpdate *update);
bool eidolon_presentation_finish_target_update(EidolonPresentation *presentation,
                                               const EidolonPresentationTargetUpdate *update,
                                               bool content_valid);
bool eidolon_presentation_set_target_alpha_mask(EidolonPresentation *presentation,
                                                const EidolonPresentationTargetUpdate *update,
                                                const uint8_t *pixels, size_t pitch,
                                                uint8_t pixel_stride, uint8_t alpha_offset);
bool eidolon_presentation_target_for_layer(EidolonPresentation *presentation,
                                           EidolonSceneLayerId layer,
                                           EidolonPresentationTargetUpdate *target);
void eidolon_presentation_release_target(EidolonPresentation *presentation,
                                         EidolonSceneLayerId layer);
bool eidolon_presentation_commit_scene(EidolonPresentation *presentation,
                                       const EidolonSceneSnapshot *scene);
bool eidolon_presentation_present(EidolonPresentation *presentation);

#ifdef __cplusplus
}
#endif

#endif
