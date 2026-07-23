#ifndef EIDOLON_PRESENTATION_H
#define EIDOLON_PRESENTATION_H

#include <stdbool.h>
#include <stdint.h>

#include "scene.h"

typedef struct EidolonPresentation EidolonPresentation;

typedef struct EidolonPresentationHost {
    uint32_t value;
} EidolonPresentationHost;

typedef struct EidolonPresentationLayer {
    uint32_t value;
} EidolonPresentationLayer;

typedef struct EidolonPresentationTarget {
    uint32_t value;
} EidolonPresentationTarget;

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
    bool (*commit_scene)(void *context, const EidolonSceneSnapshot *scene);
    bool (*present)(void *context);
} EidolonPresentationBackendOps;

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
bool eidolon_presentation_commit_scene(EidolonPresentation *presentation,
                                       const EidolonSceneSnapshot *scene);
bool eidolon_presentation_present(EidolonPresentation *presentation);

#endif
