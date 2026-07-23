#include "presentation.h"

#include <assert.h>
#include <stdio.h>

typedef struct FakePresentation {
    EidolonPresentationGeometry geometry;
    unsigned int configure_count;
    unsigned int sync_count;
    unsigned int present_count;
    unsigned int commit_count;
    unsigned int target_create_count;
    unsigned int target_destroy_count;
    unsigned int destroy_count;
    int vsync_interval;
    bool input_suspended;
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

static bool fake_create_target(void *context, EidolonPresentationTarget target, uint32_t width,
                               uint32_t height) {
    FakePresentation *fake = context;
    assert(target.value != 0U && width > 0U && height > 0U);
    ++fake->target_create_count;
    return true;
}

static void fake_destroy_target(void *context, EidolonPresentationTarget target) {
    FakePresentation *fake = context;
    assert(target.value != 0U);
    ++fake->target_destroy_count;
}

static bool fake_present(void *context) {
    FakePresentation *fake = context;
    ++fake->present_count;
    return true;
}

static bool fake_commit_scene(void *context, const EidolonSceneSnapshot *scene) {
    FakePresentation *fake = context;
    assert(scene->revision > 0U);
    ++fake->commit_count;
    return true;
}

int main(void) {
    FakePresentation fake = {
        .geometry = {10, 20, 520, 360},
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
        .create_target = fake_create_target,
        .destroy_target = fake_destroy_target,
        .commit_scene = fake_commit_scene,
        .present = fake_present,
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
    const EidolonSceneSnapshot scene = {.revision = 2U};
    assert(eidolon_presentation_commit_scene(presentation, &scene));
    assert(fake.commit_count == 1U);
    assert(eidolon_presentation_commit_scene(presentation, &scene));
    assert(fake.commit_count == 1U);
    const EidolonSceneSnapshot stale_scene = {.revision = 1U};
    assert(!eidolon_presentation_commit_scene(presentation, &stale_scene));

    const EidolonSceneLayerId body_layer = {1U};
    EidolonPresentationTargetUpdate target;
    assert(eidolon_presentation_begin_target_update(presentation, body_layer, 256U, 512U, 1U,
                                                    &target));
    assert(target.redraw_required && fake.target_create_count == 1U);
    const EidolonPresentationTarget first_target = target.target;
    assert(eidolon_presentation_finish_target_update(presentation, &target, true));
    assert(eidolon_presentation_begin_target_update(presentation, body_layer, 256U, 512U, 1U,
                                                    &target));
    assert(!target.redraw_required && target.target.value == first_target.value);
    assert(eidolon_presentation_begin_target_update(presentation, body_layer, 256U, 512U, 2U,
                                                    &target));
    assert(target.redraw_required && fake.target_create_count == 2U);
    assert(eidolon_presentation_finish_target_update(presentation, &target, false));
    assert(eidolon_presentation_target_for_layer(presentation, body_layer, &target));
    assert(target.target.value == first_target.value && target.content_revision == 1U);
    eidolon_presentation_release_target(presentation, body_layer);
    assert(fake.target_destroy_count == 2U);

    assert(eidolon_presentation_present(presentation));
    assert(fake.present_count == 1U);

    eidolon_presentation_destroy(presentation);
    assert(fake.destroy_count == 1U);
    assert(eidolon_presentation_create_backend("", capabilities, &fake, &operations) == NULL);

    puts("presentation tests passed");
    return 0;
}
