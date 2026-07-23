#include "raster_sdl_legacy.h"

#include "presentation_sdl_legacy.h"

#include <string.h>

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

static void draw_dialogue_text(SDL_Renderer *renderer, EidolonTextRenderer *text_renderer,
                               const EidolonDialogue *dialogue, float x, float start_y, float width,
                               size_t text_slot, SDL_Color color) {
    if (eidolon_text_renderer_draw(text_renderer, text_slot, dialogue->page, dialogue->revealed, x,
                                   start_y, (int)width, color)) {
        return;
    }
    char line[64];
    size_t line_length = 0U;
    float y = start_y;

    for (size_t index = 0U; index < dialogue->revealed && dialogue->page[index] != '\0'; ++index) {
        const char character = dialogue->page[index];
        if (character == '\n') {
            line[line_length] = '\0';
            SDL_RenderDebugText(renderer, x, y, line);
            line_length = 0U;
            y += 13.0F;
        } else if (line_length + 1U < sizeof(line)) {
            line[line_length++] = character;
        }
    }
    line[line_length] = '\0';
    SDL_RenderDebugText(renderer, x, y, line);
}

static void draw_dialogue_content(SDL_Renderer *renderer, EidolonTextRenderer *text_renderer,
                                  EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                  const EidolonDialogue *dialogue, const char *title,
                                  size_t title_slot, size_t body_slot, float opacity,
                                  bool points_right) {
    if (opacity <= 0.0F) {
        return;
    }
    SDL_FRect shadow = *bubble;
    shadow.x += 3.0F;
    shadow.y += 4.0F;
    DialogueThemeStyle style = dialogue_theme_style(theme);
    style.shadow = color_with_opacity(style.shadow, opacity);
    style.background = color_with_opacity(style.background, opacity);
    style.outline = color_with_opacity(style.outline, opacity);
    style.accent = color_with_opacity(style.accent, opacity);
    style.secondary = color_with_opacity(style.secondary, opacity);
    style.title = color_with_opacity(style.title, opacity);
    style.body = color_with_opacity(style.body, opacity);
    style.divider = color_with_opacity(style.divider, opacity);
    const float tail_y = bubble->y + bubble->h * 0.68F;

    fill_rounded_rect(renderer, &shadow, style.radius, style.shadow);
    if (points_right) {
        fill_triangle(renderer, (SDL_FPoint){shadow.x + shadow.w - 8.0F, tail_y - 8.0F},
                      (SDL_FPoint){shadow.x + shadow.w + 25.0F, tail_y + 12.0F},
                      (SDL_FPoint){shadow.x + shadow.w - 8.0F, tail_y + 18.0F}, style.shadow);
        fill_triangle(renderer, (SDL_FPoint){bubble->x + bubble->w - 8.0F, tail_y - 10.0F},
                      (SDL_FPoint){bubble->x + bubble->w + 23.0F, tail_y + 9.0F},
                      (SDL_FPoint){bubble->x + bubble->w - 8.0F, tail_y + 15.0F}, style.background);
    } else {
        fill_triangle(renderer, (SDL_FPoint){shadow.x + 8.0F, tail_y - 8.0F},
                      (SDL_FPoint){shadow.x - 25.0F, tail_y + 12.0F},
                      (SDL_FPoint){shadow.x + 8.0F, tail_y + 18.0F}, style.shadow);
        fill_triangle(renderer, (SDL_FPoint){bubble->x + 8.0F, tail_y - 10.0F},
                      (SDL_FPoint){bubble->x - 23.0F, tail_y + 9.0F},
                      (SDL_FPoint){bubble->x + 8.0F, tail_y + 15.0F}, style.background);
    }
    fill_rounded_rect(renderer, bubble, style.radius, style.outline);
    if (style.outlined) {
        SDL_FRect inner = {bubble->x + 2.0F, bubble->y + 2.0F, bubble->w - 4.0F, bubble->h - 4.0F};
        fill_rounded_rect(renderer, &inner, style.radius - 2.0F, style.background);
    }

    const SDL_FRect accent_bar = {bubble->x + 16.0F, bubble->y + 15.0F, 4.0F, 15.0F};
    fill_rounded_rect(renderer, &accent_bar, 2.0F, style.accent);
    if (style.outlined) {
        const SDL_FRect cyan_tick = {bubble->x + bubble->w - 50.0F, bubble->y + 17.0F, 21.0F, 2.0F};
        const SDL_FRect pink_tick = {bubble->x + bubble->w - 26.0F, bubble->y + 17.0F, 8.0F, 2.0F};
        fill_rounded_rect(renderer, &cyan_tick, 1.0F, style.secondary);
        fill_rounded_rect(renderer, &pink_tick, 1.0F, style.accent);
    }
    char heading[40];
    SDL_strlcpy(heading, title != NULL && title[0] != '\0' ? title : "EIDOLON", sizeof(heading));
    if (!eidolon_text_renderer_draw(text_renderer, title_slot, heading, strlen(heading),
                                    bubble->x + 28.0F, bubble->y + 11.0F, 0, style.title)) {
        SDL_SetRenderDrawColor(renderer, style.title.r, style.title.g, style.title.b,
                               style.title.a);
        SDL_RenderDebugText(renderer, bubble->x + 28.0F, bubble->y + 17.0F, heading);
    }
    SDL_SetRenderDrawColor(renderer, style.divider.r, style.divider.g, style.divider.b,
                           style.divider.a);
    SDL_RenderLine(renderer, bubble->x + 17.0F, bubble->y + 35.0F, bubble->x + bubble->w - 18.0F,
                   bubble->y + 35.0F);

    draw_dialogue_text(renderer, text_renderer, dialogue, bubble->x + 19.0F, bubble->y + 39.0F,
                       bubble->w - 38.0F, body_slot, style.body);

    if (eidolon_dialogue_has_next_page(dialogue)) {
        SDL_Color indicator = style.accent;
        indicator.a = (Uint8)((float)indicator.a *
                              eidolon_dialogue_indicator_alpha(dialogue, SDL_GetTicks()));
        fill_triangle(
            renderer, (SDL_FPoint){bubble->x + bubble->w - 32.0F, bubble->y + bubble->h - 20.0F},
            (SDL_FPoint){bubble->x + bubble->w - 22.0F, bubble->y + bubble->h - 20.0F},
            (SDL_FPoint){bubble->x + bubble->w - 27.0F, bubble->y + bubble->h - 13.0F}, indicator);
    }
}

static SDL_Renderer *legacy_renderer(EidolonPresentation *presentation) {
    return eidolon_sdl_legacy_renderer(presentation);
}

bool eidolon_sdl_legacy_clear_host(EidolonPresentation *presentation) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) &&
           SDL_RenderClear(renderer);
}

EidolonSdlLegacyRasterResult
eidolon_sdl_legacy_raster_portrait(EidolonPresentation *presentation,
                                   EidolonPortraitRenderer *portrait,
                                   const EidolonPresentationTargetUpdate *update) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *target =
        update != NULL ? eidolon_sdl_legacy_target_texture(presentation, update->target) : NULL;
    if (renderer == NULL || portrait == NULL || update == NULL || target == NULL) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, target)) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    const bool content_valid =
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) && SDL_RenderClear(renderer) &&
        eidolon_portrait_draw_content(portrait, renderer, update->width, update->height);
    if (!SDL_SetRenderTarget(renderer, previous_target)) {
        return EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED;
    }
    return content_valid ? EIDOLON_SDL_LEGACY_RASTER_VALID
                         : EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
}

bool eidolon_sdl_legacy_composite_portrait(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const EidolonPortraitTransform *transform) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *texture = eidolon_sdl_legacy_target_texture(presentation, target);
    if (renderer == NULL || texture == NULL || transform == NULL) {
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
    return SDL_RenderTextureRotated(renderer, texture, NULL, &destination,
                                    (double)transform->rotation_degrees, &pivot, SDL_FLIP_NONE);
}

EidolonSdlLegacyRasterResult eidolon_sdl_legacy_raster_dialogue(
    EidolonPresentation *presentation, EidolonTextRenderer *text_renderer,
    EidolonDialogueTheme theme, const EidolonPresentationTargetUpdate *update,
    const SDL_FRect *local_bubble, const EidolonDialogue *dialogue, const char *title,
    size_t title_slot, size_t body_slot, bool points_right) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *target =
        update != NULL ? eidolon_sdl_legacy_target_texture(presentation, update->target) : NULL;
    if (renderer == NULL || update == NULL || target == NULL || local_bubble == NULL ||
        dialogue == NULL) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, target)) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    const bool content_valid =
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) && SDL_RenderClear(renderer);
    if (content_valid) {
        draw_dialogue_content(renderer, text_renderer, theme, local_bubble, dialogue, title,
                              title_slot, body_slot, 1.0F, points_right);
    }
    if (!SDL_SetRenderTarget(renderer, previous_target)) {
        return EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED;
    }
    return content_valid ? EIDOLON_SDL_LEGACY_RASTER_VALID
                         : EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
}

bool eidolon_sdl_legacy_composite_dialogue(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const SDL_FRect *destination, float opacity) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *texture = eidolon_sdl_legacy_target_texture(presentation, target);
    return renderer != NULL && texture != NULL && destination != NULL &&
           SDL_SetTextureColorModFloat(texture, opacity, opacity, opacity) &&
           SDL_SetTextureAlphaModFloat(texture, opacity) &&
           SDL_RenderTexture(renderer, texture, NULL, destination);
}

bool eidolon_sdl_legacy_draw_dialogue(EidolonPresentation *presentation,
                                      EidolonTextRenderer *text_renderer,
                                      EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                      const EidolonDialogue *dialogue, const char *title,
                                      size_t title_slot, size_t body_slot, float opacity,
                                      bool points_right) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    if (renderer == NULL || bubble == NULL || dialogue == NULL) {
        return false;
    }
    draw_dialogue_content(renderer, text_renderer, theme, bubble, dialogue, title, title_slot,
                          body_slot, opacity, points_right);
    return true;
}

bool eidolon_sdl_legacy_draw_portrait(EidolonPresentation *presentation,
                                      EidolonPortraitRenderer *portrait,
                                      const EidolonPortraitTransform *transform) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && portrait != NULL && transform != NULL &&
           eidolon_portrait_draw_transform(portrait, renderer, transform);
}

bool eidolon_sdl_legacy_draw_model(EidolonPresentation *presentation, SDL_Texture *texture,
                                   const SDL_FRect *destination) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && texture != NULL && destination != NULL &&
           SDL_RenderTexture(renderer, texture, NULL, destination);
}

bool eidolon_sdl_legacy_draw_sprite(EidolonPresentation *presentation, SDL_Texture *atlas,
                                    const SDL_FRect *source, const SDL_FRect *destination) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && atlas != NULL && source != NULL && destination != NULL &&
           SDL_RenderTexture(renderer, atlas, source, destination);
}
