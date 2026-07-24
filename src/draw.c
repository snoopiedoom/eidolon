#include "draw.h"

#include "animation.h"
#include "bubble_layout.h"
#include "log.h"
#include "presentation_sdl_legacy.h"
#include "raster_sdl_legacy.h"

#if defined(_WIN32)
#include "raster_d3d11.h"
#endif

#include <string.h>

#define TEXT_SLOT_DIALOGUE_TITLE_BASE 2U
#define TEXT_SLOT_DIALOGUE_BODY_BASE 7U
#define BUBBLE_LAYER_PADDING 32.0F
#define SCENE_BODY_KEY UINT64_C(1)
#define SCENE_DIALOGUE_KEY_BASE (UINT64_C(1) << 63)
#define SCENE_FALLBACK_DIALOGUE_KEY (SCENE_DIALOGUE_KEY_BASE | UINT64_C(1))

static bool compositor_targets_active(const EidolonApp *app) {
    return eidolon_presentation_supports(app->presentation,
                                         EIDOLON_PRESENTATION_CAP_COMPOSITOR_TRANSFORM) &&
           eidolon_presentation_supports(app->presentation, EIDOLON_PRESENTATION_CAP_GPU_ZERO_COPY);
}

static uint64_t scene_hash_bytes(uint64_t hash, const void *data, size_t length) {
    const uint8_t *bytes = data;
    for (size_t index = 0U; index < length; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t scene_hash_u64(uint64_t hash, uint64_t value) {
    return scene_hash_bytes(hash, &value, sizeof(value));
}

static uint64_t dialogue_stable_key(const EidolonSessionEntry *session, int slot) {
    if (session->id[0] == '\0') {
        return SCENE_DIALOGUE_KEY_BASE | (uint64_t)(slot + 2);
    }
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = scene_hash_bytes(hash, session->provider, strlen(session->provider));
    hash = scene_hash_bytes(hash, session->id, strlen(session->id));
    return SCENE_DIALOGUE_KEY_BASE | (hash & ~(UINT64_C(1) << 63));
}

static uint64_t dialogue_content_token(const EidolonApp *app, const EidolonDialogue *dialogue,
                                       const char *title, bool points_right, uint64_t now_ms) {
    uint64_t hash = UINT64_C(1469598103934665603);
    const size_t revealed = SDL_min(dialogue->revealed, strlen(dialogue->page));
    hash = scene_hash_bytes(hash, title, strlen(title));
    hash = scene_hash_bytes(hash, dialogue->page, revealed);
    hash = scene_hash_u64(hash, (uint64_t)revealed);
    hash = scene_hash_u64(hash, (uint64_t)app->dialogue_theme);
    hash = scene_hash_u64(hash, points_right ? 1U : 0U);
    if (eidolon_dialogue_has_next_page(dialogue)) {
        hash = scene_hash_u64(hash, now_ms / 250U);
    }
    return hash;
}

static uint64_t body_content_token(const EidolonApp *app) {
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = scene_hash_u64(hash, (uint64_t)app->render_mode);
    switch (app->render_mode) {
    case EIDOLON_RENDER_MODE_SPRITE:
        hash = scene_hash_u64(hash, (uint64_t)app->animation.row);
        hash = scene_hash_u64(hash, (uint64_t)app->animation.frame);
        break;
    case EIDOLON_RENDER_MODE_PORTRAIT:
        hash = scene_hash_u64(hash, eidolon_portrait_revision(app->portrait));
        break;
    case EIDOLON_RENDER_MODE_MODEL_3D:
        hash = scene_hash_u64(hash, eidolon_model_presented_transform_revision(app->model));
        break;
    case EIDOLON_RENDER_MODE_COUNT:
        break;
    }
    return hash;
}

static EidolonSceneRect global_scene_rect(const EidolonApp *app, SDL_FRect local,
                                          const EidolonPresentationGeometry *host) {
    return (EidolonSceneRect){
        .x = (float)host->x + local.x * app->window_coordinate_scale,
        .y = (float)host->y + local.y * app->window_coordinate_scale,
        .width = local.w * app->window_coordinate_scale,
        .height = local.h * app->window_coordinate_scale,
    };
}

static bool append_dialogue_scene_layer(EidolonApp *app, EidolonSceneLayerInput *layers,
                                        size_t *layer_count, uint64_t stable_key,
                                        const SDL_FRect *bubble, const EidolonDialogue *dialogue,
                                        const char *title, float opacity, int z_order,
                                        uint64_t now_ms, const EidolonPresentationGeometry *host) {
    if (*layer_count >= EIDOLON_SCENE_LAYER_CAPACITY || opacity <= 0.0F) {
        return false;
    }
    const bool points_right =
        bubble->x + bubble->w * 0.5F < app->body_rect.x + app->body_rect.w * 0.5F;
    const SDL_FRect padded = {
        bubble->x - BUBBLE_LAYER_PADDING,
        bubble->y - BUBBLE_LAYER_PADDING,
        bubble->w + BUBBLE_LAYER_PADDING * 2.0F,
        bubble->h + BUBBLE_LAYER_PADDING * 2.0F,
    };
    layers[(*layer_count)++] = (EidolonSceneLayerInput){
        .stable_key = stable_key,
        .kind = EIDOLON_SCENE_LAYER_DIALOGUE,
        .interaction = EIDOLON_SCENE_INTERACTION_ACTIVATE,
        .content_token = dialogue_content_token(app, dialogue, title, points_right, now_ms),
        .content_width = (uint32_t)SDL_max(1, (int)SDL_ceilf(padded.w)),
        .content_height = (uint32_t)SDL_max(1, (int)SDL_ceilf(padded.h)),
        .bounds = global_scene_rect(app, padded, host),
        .pivot_x = 0.5F,
        .pivot_y = 0.5F,
        .opacity = SDL_clamp(opacity, 0.0F, 1.0F),
        .z_order = z_order,
        .visible = true,
    };
    return true;
}

static bool publish_scene_snapshot(EidolonApp *app, uint64_t now_ms,
                                   const EidolonPortraitTransform *portrait_transform) {
    EidolonPresentationGeometry host = {
        .width = app->window_width,
        .height = app->window_height,
    };
    (void)eidolon_presentation_get_geometry(app->presentation, &host);
    EidolonSceneLayerInput layers[EIDOLON_SCENE_LAYER_CAPACITY];
    size_t layer_count = 0U;

    if (app->body_rect.w > 0.0F && app->body_rect.h > 0.0F) {
        uint32_t content_width = (uint32_t)SDL_max(1, (int)SDL_ceilf(app->body_rect.w));
        uint32_t content_height = (uint32_t)SDL_max(1, (int)SDL_ceilf(app->body_rect.h));
        SDL_FRect presented_body = app->body_rect;
        float rotation_degrees = 0.0F;
        float pivot_x = 0.5F;
        float pivot_y = 0.5F;
        if (portrait_transform != NULL) {
            presented_body = (SDL_FRect){
                portrait_transform->x,
                portrait_transform->y,
                portrait_transform->width,
                portrait_transform->height,
            };
            rotation_degrees = portrait_transform->rotation_degrees;
            pivot_x = portrait_transform->pivot_x;
            pivot_y = portrait_transform->pivot_y;
            (void)eidolon_portrait_content_size(app->portrait, &content_width, &content_height);
        }
        layers[layer_count++] = (EidolonSceneLayerInput){
            .stable_key = SCENE_BODY_KEY,
            .kind = EIDOLON_SCENE_LAYER_BODY,
            .interaction =
                EIDOLON_SCENE_INTERACTION_MOVE_ANCHOR |
                (app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D
                     ? EIDOLON_SCENE_INTERACTION_ROUTE_POINTER
                     : EIDOLON_SCENE_INTERACTION_PASS_THROUGH),
            .content_token = body_content_token(app),
            .content_width = content_width,
            .content_height = content_height,
            .bounds = global_scene_rect(app, presented_body, &host),
            .rotation_degrees = rotation_degrees,
            .pivot_x = pivot_x,
            .pivot_y = pivot_y,
            .opacity = 1.0F,
            .z_order = 10,
            .visible = true,
        };
    }

    const size_t visible_sessions = eidolon_session_registry_visible_count(&app->session_registry);
    if (visible_sessions > 0U) {
        for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
            const EidolonSessionEntry *session =
                eidolon_session_registry_at_slot_const(&app->session_registry, slot);
            if (session == NULL || !app->bubble_rect_valid[slot]) {
                continue;
            }
            (void)append_dialogue_scene_layer(
                app, layers, &layer_count, dialogue_stable_key(session, slot),
                &app->bubble_rects[slot], &session->dialogue,
                session->title[0] != '\0' ? session->title : "EIDOLON",
                eidolon_session_entry_opacity(session, now_ms), 20 + slot, now_ms, &host);
        }
    } else if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        (void)append_dialogue_scene_layer(app, layers, &layer_count, SCENE_FALLBACK_DIALOGUE_KEY,
                                          &bubble, &app->dialogue, "EIDOLON", 1.0F, 20, now_ms,
                                          &host);
    }

    if (!eidolon_scene_publish(&app->scene, layers, layer_count, &app->scene_snapshot)) {
        static bool scene_failure_reported = false;
        if (!scene_failure_reported) {
            eidolon_log_write("renderer", "scene publication failed: %s", SDL_GetError());
            scene_failure_reported = true;
        }
        return false;
    }

    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        app->bubble_layers[slot] = (EidolonSceneLayerId){0U};
        const EidolonSessionEntry *session =
            eidolon_session_registry_at_slot_const(&app->session_registry, slot);
        if (session == NULL || !app->bubble_rect_valid[slot]) {
            continue;
        }
        const EidolonSceneLayerSnapshot *bubble =
            eidolon_scene_snapshot_layer(&app->scene_snapshot, dialogue_stable_key(session, slot));
        if (bubble != NULL) {
            app->bubble_layers[slot] = bubble->id;
        }
    }
    app->fallback_dialogue_layer = (EidolonSceneLayerId){0U};
    const EidolonSceneLayerSnapshot *fallback =
        eidolon_scene_snapshot_layer(&app->scene_snapshot, SCENE_FALLBACK_DIALOGUE_KEY);
    if (fallback != NULL) {
        app->fallback_dialogue_layer = fallback->id;
    }
    return true;
}

static bool draw_portrait_target(EidolonApp *app, const EidolonPortraitTransform *transform) {
    const EidolonSceneLayerSnapshot *body =
        eidolon_scene_snapshot_layer(&app->scene_snapshot, SCENE_BODY_KEY);
    if (body == NULL || transform == NULL) {
        return false;
    }
    EidolonPresentationTargetUpdate update;
    const bool native_targets = compositor_targets_active(app);
    if (!eidolon_presentation_begin_target_update(
            app->presentation, body->id, body->content_width, body->content_height,
            native_targets ? EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED
                           : EIDOLON_PRESENTATION_ALPHA_STRAIGHT,
            body->content_revision, &update)) {
        return false;
    }
    if (update.redraw_required) {
#if defined(_WIN32)
        if (native_targets) {
            const bool content_valid =
                eidolon_d3d11_raster_portrait(app->presentation, app->portrait, &update);
            if (!eidolon_presentation_finish_target_update(app->presentation, &update,
                                                           content_valid)) {
                return false;
            }
            if (!content_valid &&
                !eidolon_presentation_target_for_layer(app->presentation, body->id, &update)) {
                return false;
            }
        } else
#endif
        {
            const EidolonSdlLegacyRasterResult raster_result =
                eidolon_sdl_legacy_raster_portrait(app->presentation, app->portrait, &update);
            const bool content_valid = raster_result == EIDOLON_SDL_LEGACY_RASTER_VALID;
            if (!eidolon_presentation_finish_target_update(app->presentation, &update,
                                                           content_valid)) {
                return false;
            }
            if (raster_result == EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED) {
                return false;
            }
            if (content_valid) {
                eidolon_log_write("renderer",
                                  "body target redraw target=%u generation=%llu "
                                  "content_revision=%llu extent=%ux%u",
                                  update.target.value, (unsigned long long)update.generation,
                                  (unsigned long long)update.content_revision, update.width,
                                  update.height);
            } else if (!eidolon_presentation_target_for_layer(app->presentation, body->id,
                                                              &update)) {
                return false;
            }
        }
    }

    if (native_targets) {
        return true;
    }
    return eidolon_sdl_legacy_composite_portrait(app->presentation, update.target, transform);
}

static bool draw_dialogue_bubble(EidolonApp *app, const SDL_FRect *bubble,
                                 const EidolonDialogue *dialogue, const char *title,
                                 size_t title_slot, size_t body_slot, uint64_t stable_key,
                                 float opacity) {
    if (opacity <= 0.0F) {
        return true;
    }
    const bool points_right =
        bubble->x + bubble->w * 0.5F < app->body_rect.x + app->body_rect.w * 0.5F;
    const EidolonSceneLayerSnapshot *layer =
        eidolon_scene_snapshot_layer(&app->scene_snapshot, stable_key);
    EidolonPresentationTargetUpdate update;
    const bool native_targets = compositor_targets_active(app);
    if (layer != NULL &&
        eidolon_presentation_begin_target_update(
            app->presentation, layer->id, layer->content_width, layer->content_height,
            EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED, layer->content_revision, &update)) {
        if (update.redraw_required) {
            const SDL_FRect local_bubble = {
                BUBBLE_LAYER_PADDING,
                BUBBLE_LAYER_PADDING,
                bubble->w,
                bubble->h,
            };
#if defined(_WIN32)
            if (native_targets) {
                const bool content_valid = eidolon_d3d11_raster_dialogue(
                    app->presentation, app->text_renderer, app->dialogue_theme, &update,
                    &local_bubble, dialogue, title, title_slot, body_slot, points_right);
                const bool update_finished = eidolon_presentation_finish_target_update(
                    app->presentation, &update, content_valid);
                if (update_finished && content_valid) {
                    eidolon_log_write(
                        "renderer",
                        "dialogue target redraw target=%u generation=%llu content_revision=%llu "
                        "extent=%ux%u",
                        update.target.value, (unsigned long long)update.generation,
                        (unsigned long long)update.content_revision, update.width, update.height);
                } else if (!eidolon_presentation_target_for_layer(app->presentation, layer->id,
                                                                  &update)) {
                    return false;
                }
            } else
#endif
            {
                const EidolonSdlLegacyRasterResult raster_result =
                    eidolon_sdl_legacy_raster_dialogue(
                        app->presentation, app->text_renderer, app->dialogue_theme, &update,
                        &local_bubble, dialogue, title, title_slot, body_slot, points_right);
                const bool content_valid = raster_result == EIDOLON_SDL_LEGACY_RASTER_VALID;
                const bool update_finished = eidolon_presentation_finish_target_update(
                    app->presentation, &update, content_valid);
                if (raster_result == EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED) {
                    return false;
                }
                if (update_finished && content_valid) {
                    eidolon_log_write(
                        "renderer",
                        "dialogue target redraw target=%u generation=%llu content_revision=%llu "
                        "extent=%ux%u",
                        update.target.value, (unsigned long long)update.generation,
                        (unsigned long long)update.content_revision, update.width, update.height);
                } else if (!eidolon_presentation_target_for_layer(app->presentation, layer->id,
                                                                  &update)) {
                    goto direct_draw;
                }
            }
        }

        if (native_targets) {
            return true;
        }
        const SDL_FRect destination = {
            bubble->x - BUBBLE_LAYER_PADDING,
            bubble->y - BUBBLE_LAYER_PADDING,
            (float)update.width,
            (float)update.height,
        };
        if (eidolon_sdl_legacy_composite_dialogue(app->presentation, update.target, &destination,
                                                  opacity)) {
            return true;
        }
    }

direct_draw:
    if (native_targets) {
        return false;
    }
    return eidolon_sdl_legacy_draw_dialogue(app->presentation, app->text_renderer,
                                            app->dialogue_theme, bubble, dialogue, title,
                                            title_slot, body_slot, opacity, points_right);
}

static bool draw_scene(EidolonApp *app) {
    const bool native_targets = compositor_targets_active(app);
    if (!native_targets && !eidolon_sdl_legacy_clear_host(app->presentation)) {
        return false;
    }

    const uint64_t now_ms = SDL_GetTicks();
    EidolonPortraitTransform portrait_transform;
    const bool portrait_active =
        app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait);
    const bool portrait_transform_ready =
        portrait_active && eidolon_portrait_evaluate_transform(
                               app->portrait, app->body_rect.x, app->body_rect.y, app->body_rect.w,
                               app->body_rect.h, now_ms, &portrait_transform);
    if (!publish_scene_snapshot(app, now_ms,
                                portrait_transform_ready ? &portrait_transform : NULL)) {
        return false;
    }
    bool scene_drawn = true;
    const size_t visible_sessions = eidolon_session_registry_visible_count(&app->session_registry);
    if (visible_sessions > 0U) {
        for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
            const EidolonSessionEntry *session =
                eidolon_session_registry_at_slot_const(&app->session_registry, slot);
            if (session != NULL && app->bubble_rect_valid[slot]) {
                const bool bubble_drawn = draw_dialogue_bubble(
                    app, &app->bubble_rects[slot], &session->dialogue, session->title,
                    TEXT_SLOT_DIALOGUE_TITLE_BASE + (size_t)slot,
                    TEXT_SLOT_DIALOGUE_BODY_BASE + (size_t)slot,
                    dialogue_stable_key(session, slot),
                    eidolon_session_entry_opacity(session, now_ms));
                scene_drawn = bubble_drawn && scene_drawn;
            }
        }
    } else if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        scene_drawn =
            draw_dialogue_bubble(
                app, &bubble, &app->dialogue, "EIDOLON",
                TEXT_SLOT_DIALOGUE_TITLE_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                TEXT_SLOT_DIALOGUE_BODY_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                SCENE_FALLBACK_DIALOGUE_KEY, 1.0F) &&
            scene_drawn;
    }

    if (portrait_active) {
        bool rendered =
            portrait_transform_ready && draw_portrait_target(app, &portrait_transform);
        if (!native_targets && !rendered && portrait_transform_ready) {
            SDL_ClearError();
            rendered = eidolon_sdl_legacy_draw_portrait(app->presentation, app->portrait,
                                                        &portrait_transform);
        }
        if (!portrait_transform_ready || !rendered) {
            static bool portrait_draw_reported = false;
            if (!portrait_draw_reported) {
                eidolon_log_write("renderer", "portrait target fallback: %s", SDL_GetError());
                portrait_draw_reported = true;
            }
        }
        scene_drawn = portrait_transform_ready && rendered && scene_drawn;
    } else if (!native_targets && app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D &&
               eidolon_model_texture(app->model) != NULL) {
        SDL_Texture *model_texture = eidolon_model_texture(app->model);
        const bool rendered =
            eidolon_sdl_legacy_draw_model(app->presentation, model_texture, &app->body_rect);
        static bool model_draw_reported = false;
        if (!rendered && !model_draw_reported) {
            eidolon_log_write("renderer", "model texture draw success=%s error=%s",
                              rendered ? "yes" : "no", SDL_GetError());
            model_draw_reported = true;
        }
        scene_drawn = rendered && scene_drawn;
    } else if (!native_targets && app->render_mode == EIDOLON_RENDER_MODE_SPRITE &&
               app->atlas != NULL) {
        const SDL_FRect source = eidolon_animation_source_rect(&app->animation);
        scene_drawn =
            eidolon_sdl_legacy_draw_sprite(app->presentation, app->atlas, &source,
                                           &app->body_rect) &&
            scene_drawn;
    } else {
        scene_drawn = false;
    }
    return scene_drawn;
}

static int hit_test_mode(const EidolonApp *app) {
    const size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
    if (visible > 0U) {
        return 10 + (int)visible;
    }
    if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        return 2;
    }
    return 0;
}

static void update_hit_test_if_needed(EidolonApp *app) {
    if (!eidolon_presentation_supports(app->presentation,
                                       EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT)) {
        return;
    }
    const int mode = hit_test_mode(app);
    const uint64_t model_transform_revision =
        eidolon_model_presented_transform_revision(app->model);
    const uint64_t portrait_revision = eidolon_portrait_revision(app->portrait);
    const bool portrait_active =
        app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait);
    const bool model_active = app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D &&
                              eidolon_model_texture(app->model) != NULL;
    const bool interaction_active = app->model_rotation_dragging;
    const bool model_transform_changed =
        app->hit_test_model_transform_revision != model_transform_revision;
    const bool sprite_unchanged =
        app->hit_test_row == app->animation.row && app->hit_test_frame == app->animation.frame;
    const bool portrait_unchanged = app->hit_test_portrait_revision == portrait_revision;
    if (app->hit_test_initialized && app->hit_test_mode == mode) {
        if (portrait_active) {
            if (portrait_unchanged) {
                return;
            }
        } else if (model_active) {
            if (interaction_active || !model_transform_changed) {
                return;
            }
        } else if (sprite_unchanged && !model_transform_changed) {
            return;
        }
    }

    if (eidolon_presentation_update_input_region(app->presentation)) {
        app->hit_test_initialized = true;
        app->hit_test_row = app->animation.row;
        app->hit_test_frame = app->animation.frame;
        app->hit_test_mode = mode;
        app->hit_test_model_transform_revision = model_transform_revision;
        app->hit_test_portrait_revision = portrait_revision;
    }
}

bool eidolon_draw_frame(EidolonApp *app) {
    const bool scene_drawn = draw_scene(app);
    update_hit_test_if_needed(app);
    const bool scene_committed =
        eidolon_presentation_commit_scene(app->presentation, &app->scene_snapshot);
    if (!scene_committed) {
        static bool scene_commit_failure_reported = false;
        if (!scene_commit_failure_reported) {
            eidolon_log_write("renderer", "scene commit failed backend=%s error=%s",
                              eidolon_presentation_backend_name(app->presentation), SDL_GetError());
            scene_commit_failure_reported = true;
        }
    }
    const bool presented = eidolon_presentation_present(app->presentation);
    if (!presented) {
        static bool present_failure_reported = false;
        if (!present_failure_reported) {
            eidolon_log_write("renderer", "presentation failed backend=%s error=%s",
                              eidolon_presentation_backend_name(app->presentation), SDL_GetError());
            present_failure_reported = true;
        }
    }
    return scene_drawn && scene_committed && presented;
}

bool eidolon_draw_snapshot(EidolonApp *app, const char *path) {
    /* Hidden QA snapshots deliberately remain an SDL legacy host operation while interactive
       body and dialogue orchestration migrates to backend-owned targets. */
    int width = 0;
    int height = 0;
    if (!SDL_GetCurrentRenderOutputSize(app->renderer, &width, &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not query snapshot size: %s",
                     SDL_GetError());
        return false;
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(app->renderer);
    SDL_Texture *snapshot_target = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ABGR8888,
                                                     SDL_TEXTUREACCESS_TARGET, width, height);
    if (snapshot_target == NULL || !SDL_SetRenderTarget(app->renderer, snapshot_target)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create snapshot target: %s",
                     SDL_GetError());
        SDL_DestroyTexture(snapshot_target);
        return false;
    }
    draw_scene(app);

    SDL_Surface *surface = eidolon_sdl_legacy_read_pixels(app->presentation);
    const bool restored = SDL_SetRenderTarget(app->renderer, previous_target);
    SDL_DestroyTexture(snapshot_target);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not read snapshot pixels: %s",
                     SDL_GetError());
        return false;
    }
    if (!restored) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not restore render target: %s",
                     SDL_GetError());
        SDL_DestroySurface(surface);
        return false;
    }

    const bool saved = SDL_SavePNG(surface, path);
    if (!saved) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not save snapshot %s: %s", path,
                     SDL_GetError());
    }
    SDL_DestroySurface(surface);
    return saved;
}
