#include "draw.h"

#include "animation.h"
#include "debug_ui.h"
#include "log.h"
#include "platform/overlay.h"

static void fill_rounded_rect(SDL_Renderer *renderer, const SDL_FRect *rect, float radius,
                              SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    SDL_FRect middle = {rect->x + radius, rect->y, rect->w - (radius * 2.0F), rect->h};
    SDL_FRect center = {rect->x, rect->y + radius, rect->w, rect->h - (radius * 2.0F)};
    SDL_RenderFillRect(renderer, &middle);
    SDL_RenderFillRect(renderer, &center);

    const int radius_i = (int)radius;
    for (int y = -radius_i; y <= radius_i; ++y) {
        const float yf = (float)y;
        const float span = SDL_sqrtf((radius * radius) - (yf * yf));
        SDL_FRect left = {
            rect->x + radius - span,
            rect->y + radius + yf,
            span,
            1.0F,
        };
        SDL_FRect right = {
            rect->x + rect->w - radius,
            rect->y + radius + yf,
            span,
            1.0F,
        };
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);

        left.y = rect->y + rect->h - radius + yf;
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

static const char *state_label(EidolonState state) {
    switch (state) {
    case EIDOLON_STATE_IDLE:
        return "IDLE";
    case EIDOLON_STATE_RUNNING:
        return "WORKING";
    case EIDOLON_STATE_WAITING:
        return "NEEDS INPUT";
    case EIDOLON_STATE_REVIEW:
        return "READY";
    case EIDOLON_STATE_FAILED:
        return "BLOCKED";
    case EIDOLON_STATE_COUNT:
        break;
    }
    return "UNKNOWN";
}

static SDL_Color accent_for(EidolonState state) {
    switch (state) {
    case EIDOLON_STATE_RUNNING:
        return (SDL_Color){112, 156, 255, 255};
    case EIDOLON_STATE_WAITING:
        return (SDL_Color){255, 188, 79, 255};
    case EIDOLON_STATE_REVIEW:
        return (SDL_Color){103, 211, 142, 255};
    case EIDOLON_STATE_FAILED:
        return (SDL_Color){255, 103, 119, 255};
    case EIDOLON_STATE_IDLE:
    case EIDOLON_STATE_COUNT:
        return (SDL_Color){177, 151, 255, 255};
    }
    return (SDL_Color){255, 255, 255, 255};
}

static void draw_state_bubble(EidolonApp *app) {
    const SDL_FRect shadow = {29.0F, 29.0F, 305.0F, 78.0F};
    const SDL_FRect bubble = {26.0F, 25.0F, 305.0F, 78.0F};
    const SDL_Color accent = accent_for(app->state);

    fill_rounded_rect(app->renderer, &shadow, 19.0F, (SDL_Color){0, 0, 0, 70});
    fill_triangle(app->renderer, (SDL_FPoint){299.0F, 102.0F}, (SDL_FPoint){325.0F, 90.0F},
                  (SDL_FPoint){349.0F, 137.0F}, (SDL_Color){0, 0, 0, 70});
    fill_rounded_rect(app->renderer, &bubble, 19.0F, (SDL_Color){20, 20, 25, 232});
    fill_triangle(app->renderer, (SDL_FPoint){296.0F, 99.0F}, (SDL_FPoint){322.0F, 87.0F},
                  (SDL_FPoint){346.0F, 132.0F}, (SDL_Color){20, 20, 25, 232});

    const SDL_FRect accent_bar = {42.0F, 41.0F, 4.0F, 46.0F};
    fill_rounded_rect(app->renderer, &accent_bar, 2.0F, accent);

    SDL_SetRenderDrawColor(app->renderer, 245, 244, 248, 255);
    SDL_RenderDebugText(app->renderer, 60.0F, 42.0F, state_label(app->state));
    SDL_SetRenderDrawColor(app->renderer, 168, 166, 177, 255);
    SDL_RenderDebugText(app->renderer, 60.0F, 66.0F, "EIDOLON / LOCAL STATE");
}

static void draw_dialogue_text(EidolonApp *app) {
    char line[64];
    size_t line_length = 0;
    float y = 59.0F;

    for (size_t index = 0; index < app->dialogue.revealed && app->dialogue.page[index] != '\0';
         ++index) {
        const char character = app->dialogue.page[index];
        if (character == '\n') {
            line[line_length] = '\0';
            SDL_RenderDebugText(app->renderer, 36.0F, y, line);
            line_length = 0;
            y += 13.0F;
        } else if (line_length + 1 < sizeof(line)) {
            line[line_length++] = character;
        }
    }
    line[line_length] = '\0';
    SDL_RenderDebugText(app->renderer, 36.0F, y, line);
}

static void draw_dialogue_bubble(EidolonApp *app) {
    const SDL_FRect shadow = {20.0F, 20.0F, 365.0F, 132.0F};
    const SDL_FRect bubble = {17.0F, 16.0F, 365.0F, 132.0F};
    const SDL_Color accent = accent_for(EIDOLON_STATE_REVIEW);

    fill_rounded_rect(app->renderer, &shadow, 17.0F, (SDL_Color){0, 0, 0, 76});
    fill_triangle(app->renderer, (SDL_FPoint){326.0F, 147.0F}, (SDL_FPoint){357.0F, 132.0F},
                  (SDL_FPoint){374.0F, 176.0F}, (SDL_Color){0, 0, 0, 76});
    fill_rounded_rect(app->renderer, &bubble, 17.0F, (SDL_Color){17, 17, 23, 242});
    fill_triangle(app->renderer, (SDL_FPoint){322.0F, 145.0F}, (SDL_FPoint){353.0F, 130.0F},
                  (SDL_FPoint){370.0F, 172.0F}, (SDL_Color){17, 17, 23, 242});

    const SDL_FRect accent_bar = {33.0F, 31.0F, 4.0F, 15.0F};
    fill_rounded_rect(app->renderer, &accent_bar, 2.0F, accent);
    SDL_SetRenderDrawColor(app->renderer, 245, 244, 248, 255);
    SDL_RenderDebugText(app->renderer, 45.0F, 33.0F, "EIDOLON");
    SDL_SetRenderDrawColor(app->renderer, 72, 70, 82, 255);
    SDL_RenderLine(app->renderer, 34.0F, 51.0F, 364.0F, 51.0F);

    SDL_SetRenderDrawColor(app->renderer, 226, 224, 232, 255);
    draw_dialogue_text(app);

    if (eidolon_dialogue_has_next_page(&app->dialogue)) {
        fill_triangle(app->renderer, (SDL_FPoint){350.0F, 128.0F}, (SDL_FPoint){360.0F, 128.0F},
                      (SDL_FPoint){355.0F, 135.0F}, accent);
    }
}

static void draw_scene(EidolonApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0);
    SDL_RenderClear(app->renderer);

    if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        draw_dialogue_bubble(app);
    } else if (app->state != EIDOLON_STATE_IDLE) {
        draw_state_bubble(app);
    }

    SDL_Texture *model_texture = eidolon_model_texture(app->model);
    if (model_texture != NULL) {
        const float model_size = EIDOLON_MODEL_DISPLAY_SIZE * app->model_scale;
        const SDL_FRect destination = {
            (float)app->window_width - model_size,
            (float)app->window_height - model_size,
            model_size,
            model_size,
        };
        const bool rendered = SDL_RenderTexture(app->renderer, model_texture, NULL, &destination);
        static bool model_draw_reported = false;
        if (!rendered && !model_draw_reported) {
            eidolon_log_write("renderer", "model texture draw success=%s error=%s",
                              rendered ? "yes" : "no", SDL_GetError());
            model_draw_reported = true;
        }
    } else {
        const SDL_FRect source = eidolon_animation_source_rect(&app->animation);
        const float width = (float)EIDOLON_CELL_WIDTH * app->model_scale;
        const float height = (float)EIDOLON_CELL_HEIGHT * app->model_scale;
        const SDL_FRect destination = {
            (float)app->window_width - 18.0F - width,
            (float)app->window_height - 24.0F - height,
            width,
            height,
        };
        SDL_RenderTexture(app->renderer, app->atlas, &source, &destination);
    }

    eidolon_debug_ui_draw(app);
}

static int hit_test_mode(const EidolonApp *app) {
    if (app->state == EIDOLON_STATE_REVIEW && eidolon_dialogue_is_active(&app->dialogue)) {
        return 2;
    }
    return app->state == EIDOLON_STATE_IDLE ? 0 : 1;
}

static void update_hit_test_if_needed(EidolonApp *app) {
    const int mode = hit_test_mode(app);
    const uint64_t model_transform_revision =
        eidolon_model_presented_transform_revision(app->model);
    const bool model_active = eidolon_model_texture(app->model) != NULL;
    const bool interaction_active =
        app->model_rotation_dragging || app->debug_drag_control != EIDOLON_DEBUG_CONTROL_NONE;
    const bool model_transform_changed =
        app->hit_test_model_transform_revision != model_transform_revision;
    const bool sprite_unchanged =
        app->hit_test_row == app->animation.row && app->hit_test_frame == app->animation.frame;
    if (app->hit_test_initialized && app->hit_test_mode == mode) {
        if (model_active) {
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
