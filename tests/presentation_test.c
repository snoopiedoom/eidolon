#include "presentation.h"

#include <assert.h>
#include <stdio.h>

typedef struct FakePresentation {
    EidolonPresentationGeometry geometry;
    EidolonPresentationEnvironment environment;
    EidolonPresentationOutputInfo outputs[2];
    uint64_t topology_revision;
    size_t output_count;
    unsigned int configure_count;
    unsigned int sync_count;
    unsigned int present_count;
    unsigned int commit_count;
    unsigned int target_create_count;
    unsigned int target_destroy_count;
    unsigned int target_alpha_mask_count;
    unsigned int target_submit_count;
    unsigned int destroy_count;
    int vsync_interval;
    bool input_suspended;
    bool event_pending;
    bool topology_changed;
    EidolonPresentationEvent event;
} FakePresentation;

static void fake_destroy(void *context) {
    FakePresentation *fake = context;
    ++fake->destroy_count;
}

static bool fake_configure(void *context) {
    FakePresentation *fake = context;
    ++fake->configure_count;
    return true;
}

static bool fake_get_geometry(void *context, EidolonPresentationGeometry *geometry) {
    const FakePresentation *fake = context;
    *geometry = fake->geometry;
    return true;
}

static bool fake_set_geometry(void *context, const EidolonPresentationGeometry *geometry) {
    FakePresentation *fake = context;
    fake->geometry = *geometry;
    return true;
}

static bool fake_sync(void *context) {
    FakePresentation *fake = context;
    ++fake->sync_count;
    return true;
}

static float fake_scale(void *context) {
    (void)context;
    return 1.5F;
}

static bool fake_set_vsync(void *context, int interval) {
    FakePresentation *fake = context;
    fake->vsync_interval = interval;
    return true;
}

static bool fake_begin_move(void *context) {
    (void)context;
    return true;
}

static void fake_suspend_input(void *context) {
    FakePresentation *fake = context;
    fake->input_suspended = true;
}

static bool fake_update_input(void *context) {
    FakePresentation *fake = context;
    fake->input_suspended = false;
    return true;
}

static bool fake_poll_event(void *context, EidolonPresentationEvent *event) {
    FakePresentation *fake = context;
    if (!fake->event_pending) {
        return false;
    }
    *event = fake->event;
    fake->event_pending = false;
    return true;
}

static bool fake_get_environment(void *context, EidolonPresentationEnvironment *environment) {
    const FakePresentation *fake = context;
    *environment = fake->environment;
    return true;
}

static EidolonPresentationTopologyResult
fake_copy_outputs(void *context, EidolonPresentationOutputInfo *outputs, size_t capacity) {
    const FakePresentation *fake = context;
    if (fake->topology_changed) {
        return (EidolonPresentationTopologyResult){
            .revision = fake->topology_revision,
            .required_count = fake->output_count,
            .status = EIDOLON_PRESENTATION_TOPOLOGY_CHANGED,
        };
    }
    const size_t copied = capacity < fake->output_count ? capacity : fake->output_count;
    for (size_t index = 0U; index < copied; ++index) {
        outputs[index] = fake->outputs[index];
    }
    return (EidolonPresentationTopologyResult){
        .revision = fake->topology_revision,
        .required_count = fake->output_count,
        .copied_count = copied,
        .status = copied < fake->output_count ? EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY
                                              : EIDOLON_PRESENTATION_TOPOLOGY_OK,
    };
}

static bool fake_create_target(void *context, EidolonSceneLayerId layer,
                               EidolonPresentationTarget target, uint64_t generation,
                               uint32_t width, uint32_t height,
                               EidolonPresentationAlphaMode alpha_mode) {
    FakePresentation *fake = context;
    assert(layer.value != 0U && target.value != 0U && generation != 0U && width > 0U &&
           height > 0U);
    assert(alpha_mode == EIDOLON_PRESENTATION_ALPHA_STRAIGHT ||
           alpha_mode == EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED);
    ++fake->target_create_count;
    return true;
}

static void fake_destroy_target(void *context, EidolonPresentationTarget target) {
    FakePresentation *fake = context;
    assert(target.value != 0U);
    ++fake->target_destroy_count;
}

static bool fake_set_target_alpha_mask(void *context, EidolonPresentationTarget target,
                                       uint64_t generation, const uint8_t *pixels, size_t pitch,
                                       uint8_t pixel_stride, uint8_t alpha_offset) {
    FakePresentation *fake = context;
    assert(target.value != 0U && generation != 0U && pixels != NULL);
    assert(pitch >= 256U && pixel_stride == 1U && alpha_offset == 0U);
    ++fake->target_alpha_mask_count;
    return true;
}

static bool fake_submit_target(void *context, EidolonPresentationTarget target,
                               uint64_t generation) {
    FakePresentation *fake = context;
    assert(target.value != 0U && generation != 0U);
    ++fake->target_submit_count;
    return true;
}

static bool fake_present(void *context) {
    FakePresentation *fake = context;
    ++fake->present_count;
    return true;
}

static bool fake_commit_scene(void *context, const EidolonPresentationSceneCommit *commit) {
    FakePresentation *fake = context;
    assert(commit->revision > 0U);
    ++fake->commit_count;
    return true;
}

int main(void) {
    FakePresentation fake = {
        .geometry = {10, 20, 520, 360},
        .environment =
            {
                .revision = 4U,
                .topology_revision = 7U,
                .host = {1U},
                .active_output = {9U},
                .host_geometry = {10, 20, 520, 360},
                .output_bounds = {-1920.0F, 0.0F, 1920.0F, 1080.0F},
                .usable_bounds = {-1920.0F, 0.0F, 1920.0F, 1040.0F},
                .safe_area = {0.0F, 0.0F, 40.0F, 0.0F},
                .content_scale = 1.5F,
                .pixel_scale = 1.5F,
                .nominal_refresh_hz = 75.0F,
                .orientation = EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE,
                .coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL,
                .capabilities = EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT |
                                EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS,
                .valid_fields = EIDOLON_PRESENTATION_ENV_ALL_FIELDS,
                .changed_fields = EIDOLON_PRESENTATION_ENV_ALL_FIELDS,
            },
        .outputs =
            {
                {
                    .output = {9U},
                    .bounds = {-1920.0F, 0.0F, 1920.0F, 1080.0F},
                    .usable_bounds = {-1920.0F, 0.0F, 1920.0F, 1040.0F},
                    .safe_area = {0.0F, 0.0F, 40.0F, 0.0F},
                    .content_scale = 1.5F,
                    .pixel_scale = 1.5F,
                    .nominal_refresh_hz = 75.0F,
                    .orientation = EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE,
                    .coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL,
                    .flags = EIDOLON_PRESENTATION_OUTPUT_PRIMARY,
                    .valid_fields = EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                                    EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
                                    EIDOLON_PRESENTATION_ENV_SAFE_AREA |
                                    EIDOLON_PRESENTATION_ENV_CONTENT_SCALE |
                                    EIDOLON_PRESENTATION_ENV_PIXEL_SCALE |
                                    EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH |
                                    EIDOLON_PRESENTATION_ENV_ORIENTATION |
                                    EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE,
                },
                {
                    .output = {10U},
                    .bounds = {0.0F, 0.0F, 2560.0F, 1440.0F},
                    .usable_bounds = {0.0F, 0.0F, 2560.0F, 1400.0F},
                    .content_scale = 1.0F,
                    .pixel_scale = 1.0F,
                    .nominal_refresh_hz = 144.0F,
                    .orientation = EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE,
                    .coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL,
                    .valid_fields = EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                                    EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
                                    EIDOLON_PRESENTATION_ENV_CONTENT_SCALE |
                                    EIDOLON_PRESENTATION_ENV_PIXEL_SCALE |
                                    EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH |
                                    EIDOLON_PRESENTATION_ENV_ORIENTATION |
                                    EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE,
                },
            },
        .topology_revision = 7U,
        .output_count = 2U,
    };
    const EidolonPresentationBackendOps operations = {
        .destroy = fake_destroy,
        .configure_host = fake_configure,
        .get_geometry = fake_get_geometry,
        .set_geometry = fake_set_geometry,
        .sync_host = fake_sync,
        .display_scale = fake_scale,
        .set_vsync = fake_set_vsync,
        .begin_interactive_move = fake_begin_move,
        .suspend_input_region = fake_suspend_input,
        .update_input_region = fake_update_input,
        .poll_event = fake_poll_event,
        .create_target = fake_create_target,
        .destroy_target = fake_destroy_target,
        .set_target_alpha_mask = fake_set_target_alpha_mask,
        .submit_target = fake_submit_target,
        .commit_scene = fake_commit_scene,
        .present = fake_present,
        .get_environment = fake_get_environment,
        .copy_outputs = fake_copy_outputs,
    };
    const uint64_t capabilities =
        EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT | EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS;
    EidolonPresentation *presentation =
        eidolon_presentation_create_backend("fake", capabilities, &fake, &operations);
    assert(presentation != NULL);
    assert(eidolon_presentation_host(presentation).value == 1U);
    assert(eidolon_presentation_supports(presentation, EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT));
    assert(!eidolon_presentation_supports(presentation,
                                          EIDOLON_PRESENTATION_CAP_COMPOSITOR_TRANSFORM));
    assert(eidolon_presentation_configure_host(presentation));
    assert(fake.configure_count == 1U);

    EidolonPresentationGeometry geometry = {0};
    assert(eidolon_presentation_get_geometry(presentation, &geometry));
    assert(geometry.x == 10 && geometry.y == 20 && geometry.width == 520 && geometry.height == 360);
    const EidolonPresentationGeometry moved = {-1920, 40, 640, 480};
    assert(eidolon_presentation_set_geometry(presentation, &moved));
    assert(fake.geometry.x == -1920 && fake.geometry.width == 640);
    const EidolonPresentationGeometry invalid = {0, 0, 0, 480};
    assert(!eidolon_presentation_set_geometry(presentation, &invalid));

    assert(eidolon_presentation_sync_host(presentation));
    assert(fake.sync_count == 1U);
    assert(eidolon_presentation_display_scale(presentation) == 1.5F);
    assert(eidolon_presentation_set_vsync(presentation, 0));
    assert(fake.vsync_interval == 0);
    assert(eidolon_presentation_begin_interactive_move(presentation));
    eidolon_presentation_suspend_input_region(presentation);
    assert(fake.input_suspended);
    assert(eidolon_presentation_update_input_region(presentation));
    assert(!fake.input_suspended);
    EidolonPresentationEnvironment environment;
    assert(eidolon_presentation_get_environment(presentation, &environment));
    assert(environment.revision == 4U && environment.active_output.value == 9U);
    assert(environment.topology_revision == 7U);
    assert(environment.content_scale == 1.5F && environment.nominal_refresh_hz == 75.0F);
    fake.environment.revision = 0U;
    assert(!eidolon_presentation_get_environment(presentation, &environment));
    fake.environment.revision = 4U;

    EidolonPresentationTopologyResult topology =
        eidolon_presentation_copy_outputs(presentation, NULL, 0U);
    assert(topology.status == EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY);
    assert(topology.revision == 7U && topology.required_count == 2U && topology.copied_count == 0U);
    EidolonPresentationOutputInfo outputs[2];
    topology = eidolon_presentation_copy_outputs(presentation, outputs, 1U);
    assert(topology.status == EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY);
    assert(topology.copied_count == 1U && outputs[0].output.value == 9U);
    topology = eidolon_presentation_copy_outputs(presentation, outputs, 2U);
    assert(topology.status == EIDOLON_PRESENTATION_TOPOLOGY_OK);
    assert(topology.copied_count == 2U && outputs[1].output.value == 10U);
    fake.topology_changed = true;
    topology = eidolon_presentation_copy_outputs(presentation, outputs, 2U);
    assert(topology.status == EIDOLON_PRESENTATION_TOPOLOGY_CHANGED && topology.copied_count == 0U);
    fake.topology_changed = false;
    topology = eidolon_presentation_copy_outputs(presentation, NULL, 1U);
    assert(topology.status == EIDOLON_PRESENTATION_TOPOLOGY_ERROR);

    EidolonPresentationEvent presentation_event;
    assert(!eidolon_presentation_poll_event(presentation, &presentation_event));
    fake.event = (EidolonPresentationEvent){
        .kind = EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED,
        .sequence = 1U,
        .monotonic_ns = 100U,
        .host = {1U},
        .data.layer =
            {
                .scene_revision = 1U,
                .layer = {7U},
                .host_x = 12.0F,
                .host_y = 18.0F,
                .layer_x = 4.0F,
                .layer_y = 6.0F,
            },
    };
    fake.event_pending = true;
    assert(eidolon_presentation_poll_event(presentation, &presentation_event));
    assert(presentation_event.kind == EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED);
    assert(presentation_event.data.layer.layer.value == 7U && presentation_event.sequence == 1U);
    assert(!eidolon_presentation_poll_event(presentation, &presentation_event));
    fake.event.kind = EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED;
    fake.event.sequence = 2U;
    fake.event_pending = true;
    assert(eidolon_presentation_poll_event(presentation, &presentation_event));
    assert(presentation_event.kind == EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED);
    assert(presentation_event.data.layer.layer.value == 7U);
    const EidolonSceneSnapshot scene = {
        .revision = 2U,
        .layer_count = 1U,
        .layers = {{.id = {1U}}},
    };
    assert(eidolon_presentation_commit_scene(presentation, &scene));
    assert(fake.commit_count == 1U);
    assert(eidolon_presentation_commit_scene(presentation, &scene));
    assert(fake.commit_count == 1U);
    const EidolonSceneSnapshot stale_scene = {.revision = 1U};
    assert(!eidolon_presentation_commit_scene(presentation, &stale_scene));

    const EidolonSceneLayerId body_layer = {1U};
    EidolonPresentationTargetUpdate target;
    assert(eidolon_presentation_begin_target_update(
        presentation, body_layer, 256U, 512U, EIDOLON_PRESENTATION_ALPHA_STRAIGHT, 1U, &target));
    assert(target.redraw_required && fake.target_create_count == 1U);
    static const uint8_t alpha_mask[256U * 512U] = {0U};
    assert(eidolon_presentation_set_target_alpha_mask(presentation, &target, alpha_mask, 256U, 1U,
                                                      0U));
    assert(fake.target_alpha_mask_count == 1U);
    const EidolonPresentationTarget first_target = target.target;
    assert(eidolon_presentation_finish_target_update(presentation, &target, true));
    assert(fake.target_submit_count == 1U);
    assert(!eidolon_presentation_set_target_alpha_mask(presentation, &target, alpha_mask, 256U, 1U,
                                                       0U));
    assert(eidolon_presentation_begin_target_update(
        presentation, body_layer, 256U, 512U, EIDOLON_PRESENTATION_ALPHA_STRAIGHT, 1U, &target));
    assert(!target.redraw_required && target.target.value == first_target.value);
    assert(eidolon_presentation_begin_target_update(
        presentation, body_layer, 256U, 512U, EIDOLON_PRESENTATION_ALPHA_STRAIGHT, 2U, &target));
    assert(target.redraw_required && fake.target_create_count == 2U);
    assert(eidolon_presentation_finish_target_update(presentation, &target, false));
    assert(fake.target_submit_count == 1U);
    assert(eidolon_presentation_target_for_layer(presentation, body_layer, &target));
    assert(target.target.value == first_target.value && target.content_revision == 1U);
    const EidolonSceneSnapshot retired_scene = {.revision = 3U};
    assert(eidolon_presentation_commit_scene(presentation, &retired_scene));
    assert(fake.commit_count == 2U);
    assert(fake.target_destroy_count == 2U);
    assert(!eidolon_presentation_target_for_layer(presentation, body_layer, &target));

    assert(eidolon_presentation_present(presentation));
    assert(fake.present_count == 1U);

    eidolon_presentation_destroy(presentation);
    assert(fake.destroy_count == 1U);
    assert(eidolon_presentation_create_backend("", capabilities, &fake, &operations) == NULL);

    puts("presentation tests passed");
    return 0;
}
