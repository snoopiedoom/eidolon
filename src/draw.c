#include "draw.h"

#include "animation.h"
#include "bubble_layout.h"
#include "log.h"
#include "platform/overlay.h"

#include <string.h>

#define TEXT_SLOT_DIALOGUE_TITLE_BASE 2U
#define TEXT_SLOT_DIALOGUE_BODY_BASE 7U
#define BUBBLE_LAYER_PADDING 32.0F

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

static bool ensure_bubble_layer(EidolonApp *app, size_t layer_slot, int width, int height) {
    if (layer_slot >= SDL_arraysize(app->bubble_layers) || width <= 0 || height <= 0) {
        return false;
    }
    if (app->bubble_layers[layer_slot] != NULL &&
        (app->bubble_layer_widths[layer_slot] != width ||
         app->bubble_layer_heights[layer_slot] != height)) {
        SDL_DestroyTexture(app->bubble_layers[layer_slot]);
        app->bubble_layers[layer_slot] = NULL;
    }
    if (app->bubble_layers[layer_slot] == NULL) {
        const SDL_BlendMode premultiplied_blend = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
        app->bubble_layers[layer_slot] = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ABGR8888,
                                                           SDL_TEXTUREACCESS_TARGET, width, height);
        if (app->bubble_layers[layer_slot] == NULL ||
            !SDL_SetTextureBlendMode(app->bubble_layers[layer_slot], premultiplied_blend)) {
            SDL_DestroyTexture(app->bubble_layers[layer_slot]);
            app->bubble_layers[layer_slot] = NULL;
            return false;
        }
        app->bubble_layer_widths[layer_slot] = width;
        app->bubble_layer_heights[layer_slot] = height;
    }
    return true;
}

static void draw_dialogue_bubble(EidolonApp *app, const SDL_FRect *bubble,
                                 const EidolonDialogue *dialogue, const char *title,
                                 size_t title_slot, size_t body_slot, size_t layer_slot,
                                 float opacity) {
    if (opacity <= 0.0F) {
        return;
    }
    const bool points_right =
        bubble->x + bubble->w * 0.5F < app->body_rect.x + app->body_rect.w * 0.5F;
    const int layer_width = (int)SDL_ceilf(bubble->w + BUBBLE_LAYER_PADDING * 2.0F);
    const int layer_height = (int)SDL_ceilf(bubble->h + BUBBLE_LAYER_PADDING * 2.0F);
    SDL_Texture *previous_target = SDL_GetRenderTarget(app->renderer);
    if (ensure_bubble_layer(app, layer_slot, layer_width, layer_height) &&
        SDL_SetRenderTarget(app->renderer, app->bubble_layers[layer_slot])) {
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0);
        SDL_RenderClear(app->renderer);
        const SDL_FRect local_bubble = {
            BUBBLE_LAYER_PADDING,
            BUBBLE_LAYER_PADDING,
            bubble->w,
            bubble->h,
        };
        draw_dialogue_bubble_content(app, &local_bubble, dialogue, title, title_slot, body_slot,
                                     1.0F, points_right);
        if (SDL_SetRenderTarget(app->renderer, previous_target) &&
            SDL_SetTextureColorModFloat(app->bubble_layers[layer_slot], opacity, opacity,
                                        opacity) &&
            SDL_SetTextureAlphaModFloat(app->bubble_layers[layer_slot], opacity)) {
            const SDL_FRect destination = {
                bubble->x - BUBBLE_LAYER_PADDING,
                bubble->y - BUBBLE_LAYER_PADDING,
                (float)layer_width,
                (float)layer_height,
            };
            SDL_RenderTexture(app->renderer, app->bubble_layers[layer_slot], NULL, &destination);
            return;
        }
    }

    (void)SDL_SetRenderTarget(app->renderer, previous_target);
    draw_dialogue_bubble_content(app, bubble, dialogue, title, title_slot, body_slot, opacity,
                                 points_right);
}

static void draw_scene(EidolonApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0);
    SDL_RenderClear(app->renderer);

    const uint64_t now_ms = SDL_GetTicks();
    const size_t visible_sessions = eidolon_session_registry_visible_count(&app->session_registry);
    if (visible_sessions > 0U) {
        for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
            const EidolonSessionEntry *session =
                eidolon_session_registry_at_slot_const(&app->session_registry, slot);
            if (session != NULL && app->bubble_rect_valid[slot]) {
                draw_dialogue_bubble(app, &app->bubble_rects[slot], &session->dialogue,
                                     session->title, TEXT_SLOT_DIALOGUE_TITLE_BASE + (size_t)slot,
                                     TEXT_SLOT_DIALOGUE_BODY_BASE + (size_t)slot, (size_t)slot,
                                     eidolon_session_entry_opacity(session, now_ms));
            }
        }
    } else if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        draw_dialogue_bubble(app, &bubble, &app->dialogue, "EIDOLON",
                             TEXT_SLOT_DIALOGUE_TITLE_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                             TEXT_SLOT_DIALOGUE_BODY_BASE + EIDOLON_VISIBLE_SESSION_CAPACITY,
                             EIDOLON_VISIBLE_SESSION_CAPACITY, 1.0F);
    }

    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait)) {
        if (!eidolon_portrait_draw(app->portrait, app->renderer, &app->body_rect, SDL_GetTicks())) {
            static bool portrait_draw_reported = false;
            if (!portrait_draw_reported) {
                eidolon_log_write("renderer", "portrait draw failed: %s", SDL_GetError());
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

    if (eidolon_platform_update_hit_test(app->window, app->renderer)) {
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
    SDL_RenderPresent(app->renderer);
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

    SDL_Surface *surface = eidolon_platform_read_pixels(app->renderer);
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
