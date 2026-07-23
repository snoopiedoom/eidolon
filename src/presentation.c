#include "presentation.h"

#include "presentation_internal.h"

#include <SDL3/SDL.h>

#define EIDOLON_PRESENTATION_BACKEND_NAME_CAPACITY 48U

struct EidolonPresentation {
    char backend_name[EIDOLON_PRESENTATION_BACKEND_NAME_CAPACITY];
    uint64_t capabilities;
    void *context;
    EidolonPresentationBackendOps operations;
    EidolonPresentationHost host;
    uint64_t committed_scene_revision;
};

EidolonPresentation *
eidolon_presentation_create_backend(const char *backend_name, uint64_t capabilities, void *context,
                                    const EidolonPresentationBackendOps *operations) {
    if (backend_name == NULL || backend_name[0] == '\0' || context == NULL || operations == NULL ||
        operations->destroy == NULL || operations->present == NULL) {
        SDL_SetError("invalid presentation backend contract");
        return NULL;
    }
    EidolonPresentation *presentation = SDL_calloc(1U, sizeof(*presentation));
    if (presentation == NULL) {
        return NULL;
    }
    SDL_strlcpy(presentation->backend_name, backend_name, sizeof(presentation->backend_name));
    presentation->capabilities = capabilities;
    presentation->context = context;
    presentation->operations = *operations;
    presentation->host.value = 1U;
    return presentation;
}

void eidolon_presentation_destroy(EidolonPresentation *presentation) {
    if (presentation == NULL) {
        return;
    }
    presentation->operations.destroy(presentation->context);
    SDL_free(presentation);
}

const char *eidolon_presentation_backend_name(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->backend_name : "none";
}

uint64_t eidolon_presentation_capabilities(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->capabilities : 0U;
}

bool eidolon_presentation_supports(const EidolonPresentation *presentation,
                                   EidolonPresentationCapability capability) {
    return presentation != NULL &&
           (presentation->capabilities & (uint64_t)capability) == (uint64_t)capability;
}

EidolonPresentationHost eidolon_presentation_host(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->host : (EidolonPresentationHost){0U};
}

bool eidolon_presentation_configure_host(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.configure_host != NULL &&
           presentation->operations.configure_host(presentation->context);
}

bool eidolon_presentation_get_geometry(EidolonPresentation *presentation,
                                       EidolonPresentationGeometry *geometry) {
    return presentation != NULL && geometry != NULL &&
           presentation->operations.get_geometry != NULL &&
           presentation->operations.get_geometry(presentation->context, geometry);
}

bool eidolon_presentation_set_geometry(EidolonPresentation *presentation,
                                       const EidolonPresentationGeometry *geometry) {
    return presentation != NULL && geometry != NULL && geometry->width > 0 &&
           geometry->height > 0 && presentation->operations.set_geometry != NULL &&
           presentation->operations.set_geometry(presentation->context, geometry);
}

bool eidolon_presentation_sync_host(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.sync_host != NULL &&
           presentation->operations.sync_host(presentation->context);
}

float eidolon_presentation_display_scale(EidolonPresentation *presentation) {
    if (presentation == NULL || presentation->operations.display_scale == NULL) {
        return 1.0F;
    }
    return presentation->operations.display_scale(presentation->context);
}

bool eidolon_presentation_set_vsync(EidolonPresentation *presentation, int interval) {
    return presentation != NULL && presentation->operations.set_vsync != NULL &&
           presentation->operations.set_vsync(presentation->context, interval);
}

bool eidolon_presentation_begin_interactive_move(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.begin_interactive_move != NULL &&
           presentation->operations.begin_interactive_move(presentation->context);
}

void eidolon_presentation_suspend_input_region(EidolonPresentation *presentation) {
    if (presentation != NULL && presentation->operations.suspend_input_region != NULL) {
        presentation->operations.suspend_input_region(presentation->context);
    }
}

bool eidolon_presentation_update_input_region(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.update_input_region != NULL &&
           presentation->operations.update_input_region(presentation->context);
}

bool eidolon_presentation_commit_scene(EidolonPresentation *presentation,
                                       const EidolonSceneSnapshot *scene) {
    if (presentation == NULL || scene == NULL || scene->revision == 0U ||
        scene->revision < presentation->committed_scene_revision) {
        SDL_SetError("stale or invalid presentation scene");
        return false;
    }
    if (scene->revision == presentation->committed_scene_revision) {
        return true;
    }
    if (presentation->operations.commit_scene != NULL &&
        !presentation->operations.commit_scene(presentation->context, scene)) {
        return false;
    }
    presentation->committed_scene_revision = scene->revision;
    return true;
}

bool eidolon_presentation_present(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.present(presentation->context);
}

void *eidolon_presentation_backend_context(EidolonPresentation *presentation,
                                           const char *backend_name) {
    if (presentation == NULL || backend_name == NULL ||
        SDL_strcmp(presentation->backend_name, backend_name) != 0) {
        SDL_SetError("presentation backend mismatch");
        return NULL;
    }
    return presentation->context;
}
