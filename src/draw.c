#include "draw.h"

#include "animation.h"
#include "bubble_layout.h"
#include "log.h"
#include "presentation_sdl_legacy.h"

#include <string.h>

#define TEXT_SLOT_DIALOGUE_TITLE_BASE 2U
#define TEXT_SLOT_DIALOGUE_BODY_BASE 7U
#define BUBBLE_LAYER_PADDING 32.0F
#define SCENE_BODY_KEY UINT64_C(1)
#define SCENE_DIALOGUE_KEY_BASE (UINT64_C(1) << 63)
#define SCENE_FALLBACK_DIALOGUE_KEY (SCENE_DIALOGUE_KEY_BASE | UINT64_C(1))

typedef struct DialogueThemeStyle {
    SDL_Color shadow;
    SDL_Color background;
    SDL_Color outline;
    SDL_Color accent;
    SDL_Color secondary;
    SDL_Color title;
    SDL_Color body;
    SDL_Color divider;
    float radius;
    bool outlined;
} DialogueThemeStyle;

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

static void publish_scene_snapshot(EidolonApp *app, uint64_t now_ms,
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
    }
}

static bool draw_portrait_target(EidolonApp *app, const EidolonPortraitTransform *transform) {
    const EidolonSceneLayerSnapshot *body =
        eidolon_scene_snapshot_layer(&app->scene_snapshot, SCENE_BODY_KEY);
    if (body == NULL || transform == NULL) {
        return false;
    }
    EidolonPresentationTargetUpdate update;
    if (!eidolon_presentation_begin_target_update(
            app->presentation, body->id, body->content_width, body->content_height,
            EIDOLON_PRESENTATION_ALPHA_STRAIGHT, body->content_revision, &update)) {
        return false;
    }
    if (update.redraw_required) {
        SDL_Texture *target = eidolon_sdl_legacy_target_texture(app->presentation, update.target);
        SDL_Texture *previous_target = SDL_GetRenderTarget(app->renderer);
        bool content_valid = target != NULL && SDL_SetRenderTarget(app->renderer, target);
        if (content_valid) {
            content_valid = SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0) &&
                            SDL_RenderClear(app->renderer) &&
                            eidolon_portrait_draw_content(app->portrait, app->renderer,
                                                          update.width, update.height);
        }
        content_valid = SDL_SetRenderTarget(app->renderer, previous_target) && content_valid;
        if (!eidolon_presentation_finish_target_update(app->presentation, &update, content_valid)) {
            return false;
        }
        if (content_valid) {
            eidolon_log_write(
                "renderer",
                "body target redraw target=%u generation=%llu content_revision=%llu extent=%ux%u",
                update.target.value, (unsigned long long)update.generation,
                (unsigned long long)update.content_revision, update.width, update.height);
        } else if (!eidolon_presentation_target_for_layer(app->presentation, body->id, &update)) {
            return false;
        }
    }

    SDL_Texture *target = eidolon_sdl_legacy_target_texture(app->presentation, update.target);
    if (target == NULL) {
        return false;
    }
    const SDL_FRect destination = {
        transform->x,
        transform->y,
        transform->width,
        transform->height,
    };
    const SDL_FPoint pivot = {
        transform->width * transform->pivot_x,
        transform->height * transform->pivot_y,
    };
    return SDL_RenderTextureRotated(app->renderer, target, NULL, &destination,
                                    (double)transform->rotation_degrees, &pivot, SDL_FLIP_NONE);
}

static DialogueThemeStyle dialogue_theme_style(EidolonDialogueTheme theme) {
    if (theme == EIDOLON_DIALOGUE_THEME_ACADEMY_HEART) {
        return (DialogueThemeStyle){
            .shadow = {38, 25, 55, 74},
            .background = {250, 246, 252, 246},
            .outline = {117, 210, 234, 238},
            .accent = {255, 105, 164, 255},
            .secondary = {92, 205, 233, 255},
            .title = {76, 48, 79, 255},
            .body = {62, 52, 72, 255},
            .divider = {224, 190, 217, 255},
            .radius = 19.0F,
            .outlined = true,
        };
    }
    return (DialogueThemeStyle){
        .shadow = {0, 0, 0, 76},
        .background = {17, 17, 23, 242},
        .outline = {17, 17, 23, 242},
        .accent = {103, 211, 142, 255},
        .secondary = {103, 211, 142, 255},
        .title = {245, 244, 248, 255},
        .body = {226, 224, 232, 255},
        .divider = {72, 70, 82, 255},
        .radius = 17.0F,
        .outlined = false,
    };
}

static void fill_rounded_rect(SDL_Renderer *renderer, const SDL_FRect *rect, float radius,
                              SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const float bounded_radius = SDL_min(radius, SDL_min(rect->w, rect->h) * 0.5F);
    const SDL_FRect bands[] = {
        {rect->x + bounded_radius, rect->y, rect->w - bounded_radius * 2.0F, bounded_radius},
        {rect->x, rect->y + bounded_radius, rect->w, rect->h - bounded_radius * 2.0F},
        {rect->x + bounded_radius, rect->y + rect->h - bounded_radius,
         rect->w - bounded_radius * 2.0F, bounded_radius},
    };
    SDL_RenderFillRects(renderer, bands, SDL_arraysize(bands));

    const int radius_i = (int)SDL_ceilf(bounded_radius);
    for (int row = 0; row < radius_i; ++row) {
        const float yf = bounded_radius - ((float)row + 0.5F);
        const float span = SDL_sqrtf(SDL_max(0.0F, bounded_radius * bounded_radius - yf * yf));
        SDL_FRect left = {
            rect->x + bounded_radius - span,
            rect->y + (float)row,
            span,
            1.0F,
        };
        SDL_FRect right = {
            rect->x + rect->w - bounded_radius,
            rect->y + (float)row,
            span,
            1.0F,
        };
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);

        left.y = rect->y + rect->h - (float)row - 1.0F;
        right.y = left.y;
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }
}

static void fill_triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
                          SDL_Color color) {
    const SDL_FColor vertex_color = {
        (float)color.r / 255.0F,
        (float)color.g / 255.0F,
        (float)color.b / 255.0F,
        (float)color.a / 255.0F,
    };
    const SDL_Vertex vertices[] = {
        {.position = a, .color = vertex_color, .tex_coord = {0.0F, 0.0F}},
        {.position = b, .color = vertex_color, .tex_coord = {0.0F, 0.0F}},
        {.position = c, .color = vertex_color, .tex_coord = {0.0F, 0.0F}},
    };
    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}

static SDL_Color color_with_opacity(SDL_Color color, float opacity) {
    const float clamped = SDL_clamp(opacity, 0.0F, 1.0F);
    color.a = (Uint8)SDL_roundf((float)color.a * clamped);
    return color;
}

static void draw_dialogue_text(EidolonApp *app, const EidolonDialogue *dialogue, float x,
                               float start_y, float width, size_t text_slot, SDL_Color color) {
    if (eidolon_text_renderer_draw(app->text_renderer, text_slot, dialogue->page,
                                   dialogue->revealed, x, start_y, (int)width, color)) {
        return;
    }
    char line[64];
    size_t line_length = 0;
    float y = start_y;

    for (size_t index = 0; index < dialogue->revealed && dialogue->page[index] != '\0'; ++index) {
        const char character = dialogue->page[index];
        if (character == '\n') {
            line[line_length] = '\0';
            SDL_RenderDebugText(app->renderer, x, y, line);
            line_length = 0;
            y += 13.0F;
        } else if (line_length + 1 < sizeof(line)) {
            line[line_length++] = character;
        }
    }
    line[line_length] = '\0';
    SDL_RenderDebugText(app->renderer, x, y, line);
}

static void draw_dialogue_bubble_content(EidolonApp *app, const SDL_FRect *bubble,
                                         const EidolonDialogue *dialogue, const char *title,
                                         size_t title_slot, size_t body_slot, float opacity,
                                         bool points_right) {
    if (opacity <= 0.0F) {
        return;
    }
    SDL_FRect shadow = *bubble;
    shadow.x += 3.0F;
    shadow.y += 4.0F;
    DialogueThemeStyle style = dialogue_theme_style(app->dialogue_theme);
    style.shadow = color_with_opacity(style.shadow, opacity);
    style.background = color_with_opacity(style.background, opacity);
    style.outline = color_with_opacity(style.outline, opacity);
    style.accent = color_with_opacity(style.accent, opacity);
    style.secondary = color_with_opacity(style.secondary, opacity);
    style.title = color_with_opacity(style.title, opacity);
    style.body = color_with_opacity(style.body, opacity);
    style.divider = color_with_opacity(style.divider, opacity);
    const float tail_y = bubble->y + bubble->h * 0.68F;

    fill_rounded_rect(app->renderer, &shadow, style.radius, style.shadow);
    if (points_right) {
        fill_triangle(app->renderer, (SDL_FPoint){shadow.x + shadow.w - 8.0F, tail_y - 8.0F},
                      (SDL_FPoint){shadow.x + shadow.w + 25.0F, tail_y + 12.0F},
                      (SDL_FPoint){shadow.x + shadow.w - 8.0F, tail_y + 18.0F}, style.shadow);
        fill_triangle(app->renderer, (SDL_FPoint){bubble->x + bubble->w - 8.0F, tail_y - 10.0F},
                      (SDL_FPoint){bubble->x + bubble->w + 23.0F, tail_y + 9.0F},
                      (SDL_FPoint){bubble->x + bubble->w - 8.0F, tail_y + 15.0F}, style.background);
    } else {
        fill_triangle(app->renderer, (SDL_FPoint){shadow.x + 8.0F, tail_y - 8.0F},
                      (SDL_FPoint){shadow.x - 25.0F, tail_y + 12.0F},
                      (SDL_FPoint){shadow.x + 8.0F, tail_y + 18.0F}, style.shadow);
        fill_triangle(app->renderer, (SDL_FPoint){bubble->x + 8.0F, tail_y - 10.0F},
                      (SDL_FPoint){bubble->x - 23.0F, tail_y + 9.0F},
                      (SDL_FPoint){bubble->x + 8.0F, tail_y + 15.0F}, style.background);
    }
    fill_rounded_rect(app->renderer, bubble, style.radius, style.outline);
    if (style.outlined) {
        SDL_FRect inner = {bubble->x + 2.0F, bubble->y + 2.0F, bubble->w - 4.0F, bubble->h - 4.0F};
        fill_rounded_rect(app->renderer, &inner, style.radius - 2.0F, style.background);
    }

    const SDL_FRect accent_bar = {bubble->x + 16.0F, bubble->y + 15.0F, 4.0F, 15.0F};
    fill_rounded_rect(app->renderer, &accent_bar, 2.0F, style.accent);
    if (style.outlined) {
        const SDL_FRect cyan_tick = {bubble->x + bubble->w - 50.0F, bubble->y + 17.0F, 21.0F, 2.0F};
        const SDL_FRect pink_tick = {bubble->x + bubble->w - 26.0F, bubble->y + 17.0F, 8.0F, 2.0F};
        fill_rounded_rect(app->renderer, &cyan_tick, 1.0F, style.secondary);
        fill_rounded_rect(app->renderer, &pink_tick, 1.0F, style.accent);
    }
    char heading[40];
    SDL_strlcpy(heading, title != NULL && title[0] != '\0' ? title : "EIDOLON", sizeof(heading));
    if (!eidolon_text_renderer_draw(app->text_renderer, title_slot, heading, strlen(heading),
                                    bubble->x + 28.0F, bubble->y + 11.0F, 0, style.title)) {
        SDL_SetRenderDrawColor(app->renderer, style.title.r, style.title.g, style.title.b,
                               style.title.a);
        SDL_RenderDebugText(app->renderer, bubble->x + 28.0F, bubble->y + 17.0F, heading);
    }
    SDL_SetRenderDrawColor(app->renderer, style.divider.r, style.divider.g, style.divider.b,
                           style.divider.a);
    SDL_RenderLine(app->renderer, bubble->x + 17.0F, bubble->y + 35.0F,
                   bubble->x + bubble->w - 18.0F, bubble->y + 35.0F);

    draw_dialogue_text(app, dialogue, bubble->x + 19.0F, bubble->y + 39.0F, bubble->w - 38.0F,
                       body_slot, style.body);

    if (eidolon_dialogue_has_next_page(dialogue)) {
        SDL_Color indicator = style.accent;
        indicator.a = (Uint8)((float)indicator.a *
                              eidolon_dialogue_indicator_alpha(dialogue, SDL_GetTicks()));
        fill_triangle(app->renderer,
                      (SDL_FPoint){bubble->x + bubble->w - 32.0F, bubble->y + bubble->h - 20.0F},
                      (SDL_FPoint){bubble->x + bubble->w - 22.0F, bubble->y + bubble->h - 20.0F},
                      (SDL_FPoint){bubble->x + bubble->w - 27.0F, bubble->y + bubble->h - 13.0F},
                      indicator);
    }
}

static void draw_dialogue_bubble(EidolonApp *app, const SDL_FRect *bubble,
                                 const EidolonDialogue *dialogue, const char *title,
                                 size_t title_slot, size_t body_slot, uint64_t stable_key,
                                 float opacity) {
    if (opacity <= 0.0F) {
        return;
    }
    const bool points_right =
        bubble->x + bubble->w * 0.5F < app->body_rect.x + app->body_rect.w * 0.5F;
    const EidolonSceneLayerSnapshot *layer =
        eidolon_scene_snapshot_layer(&app->scene_snapshot, stable_key);
    EidolonPresentationTargetUpdate update;
    if (layer != NULL &&
        eidolon_presentation_begin_target_update(
            app->presentation, layer->id, layer->content_width, layer->content_height,
            EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED, layer->content_revision, &update)) {
        if (update.redraw_required) {
            SDL_Texture *target =
                eidolon_sdl_legacy_target_texture(app->presentation, update.target);
            SDL_Texture *previous_target = SDL_GetRenderTarget(app->renderer);
            bool content_valid = target != NULL && SDL_SetRenderTarget(app->renderer, target);
            if (content_valid) {
                content_valid = SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0) &&
                                SDL_RenderClear(app->renderer);
                if (content_valid) {
                    const SDL_FRect local_bubble = {
                        BUBBLE_LAYER_PADDING,
                        BUBBLE_LAYER_PADDING,
                        bubble->w,
                        bubble->h,
                    };
                    draw_dialogue_bubble_content(app, &local_bubble, dialogue, title, title_slot,
                                                 body_slot, 1.0F, points_right);
                }
            }
            const bool target_restored = SDL_SetRenderTarget(app->renderer, previous_target);
            content_valid = target_restored && content_valid;
            const bool update_finished = eidolon_presentation_finish_target_update(
                app->presentation, &update, content_valid);
            if (!target_restored) {
                return;
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

        SDL_Texture *target = eidolon_sdl_legacy_target_texture(app->presentation, update.target);
        if (target != NULL && SDL_SetTextureColorModFloat(target, opacity, opacity, opacity) &&
            SDL_SetTextureAlphaModFloat(target, opacity)) {
            const SDL_FRect destination = {
                bubble->x - BUBBLE_LAYER_PADDING,
                bubble->y - BUBBLE_LAYER_PADDING,
                (float)update.width,
                (float)update.height,
            };
            if (SDL_RenderTexture(app->renderer, target, NULL, &destination)) {
                return;
            }
        }
    }

direct_draw:
    draw_dialogue_bubble_content(app, bubble, dialogue, title, title_slot, body_slot, opacity,
                                 points_right);
}

static void draw_scene(EidolonApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0);
    SDL_RenderClear(app->renderer);

    const uint64_t now_ms = SDL_GetTicks();
    EidolonPortraitTransform portrait_transform;
    const bool portrait_active =
        app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait);
    const bool portrait_transform_ready =
        portrait_active && eidolon_portrait_evaluate_transform(
                               app->portrait, app->body_rect.x, app->body_rect.y, app->body_rect.w,
                               app->body_rect.h, now_ms, &portrait_transform);
    publish_scene_snapshot(app, now_ms, portrait_transform_ready ? &portrait_transform : NULL);
    const size_t visible_sessions = eidolon_session_registry_visible_count(&app->session_registry);
    if (visible_sessions > 0U) {
        for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
            const EidolonSessionEntry *session =
                eidolon_session_registry_at_slot_const(&app->session_registry, slot);
            if (session != NULL && app->bubble_rect_valid[slot]) {
                draw_dialogue_bubble(app, &app->bubble_rects[slot], &session->dialogue,
                                     session->title, TEXT_SLOT_DIALOGUE_TITLE_BASE + (size_t)slot,
                                     TEXT_SLOT_DIALOGUE_BODY_BASE + (size_t)slot,
                                     dialogue_stable_key(session, slot),
                                     eidolon_session_entry_opacity(session, now_ms));
            }
        }
    } else if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        draw_dialogue_bubble(app, &bubble, &app->dialogue, "EIDOLON",
                             TEXT_SLOT_DIALOGUE_TITLE_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                             TEXT_SLOT_DIALOGUE_BODY_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                             SCENE_FALLBACK_DIALOGUE_KEY, 1.0F);
    }

    if (portrait_active) {
        const bool rendered =
            portrait_transform_ready && draw_portrait_target(app, &portrait_transform);
        if (!rendered && portrait_transform_ready) {
            SDL_ClearError();
            (void)eidolon_portrait_draw_transform(app->portrait, app->renderer,
                                                  &portrait_transform);
        }
        if (!portrait_transform_ready || !rendered) {
            static bool portrait_draw_reported = false;
            if (!portrait_draw_reported) {
                eidolon_log_write("renderer", "portrait target fallback: %s", SDL_GetError());
                portrait_draw_reported = true;
            }
        }
    } else if (app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D &&
               eidolon_model_texture(app->model) != NULL) {
        SDL_Texture *model_texture = eidolon_model_texture(app->model);
        const bool rendered =
            SDL_RenderTexture(app->renderer, model_texture, NULL, &app->body_rect);
        static bool model_draw_reported = false;
        if (!rendered && !model_draw_reported) {
            eidolon_log_write("renderer", "model texture draw success=%s error=%s",
                              rendered ? "yes" : "no", SDL_GetError());
            model_draw_reported = true;
        }
    } else if (app->render_mode == EIDOLON_RENDER_MODE_SPRITE && app->atlas != NULL) {
        const SDL_FRect source = eidolon_animation_source_rect(&app->animation);
        SDL_RenderTexture(app->renderer, app->atlas, &source, &app->body_rect);
    }
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

void eidolon_draw_frame(EidolonApp *app) {
    draw_scene(app);
    update_hit_test_if_needed(app);
    if (!eidolon_presentation_commit_scene(app->presentation, &app->scene_snapshot)) {
        static bool scene_commit_failure_reported = false;
        if (!scene_commit_failure_reported) {
            eidolon_log_write("renderer", "scene commit failed backend=%s error=%s",
                              eidolon_presentation_backend_name(app->presentation), SDL_GetError());
            scene_commit_failure_reported = true;
        }
    }
    if (!eidolon_presentation_present(app->presentation)) {
        static bool present_failure_reported = false;
        if (!present_failure_reported) {
            eidolon_log_write("renderer", "presentation failed backend=%s error=%s",
                              eidolon_presentation_backend_name(app->presentation), SDL_GetError());
            present_failure_reported = true;
        }
    }
}

bool eidolon_draw_snapshot(EidolonApp *app, const char *path) {
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
