#include "dialogue_art.h"

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
    const int radius_i = (int)SDL_ceilf(bounded_radius);
    const SDL_FRect center = {
        rect->x,
        rect->y + (float)radius_i,
        rect->w,
        rect->h - (float)(radius_i * 2),
    };
    if (center.h > 0.0F) {
        SDL_RenderFillRect(renderer, &center);
    }
    for (int row = 0; row < radius_i; ++row) {
        const float yf = bounded_radius - ((float)row + 0.5F);
        const float span = SDL_sqrtf(SDL_max(0.0F, bounded_radius * bounded_radius - yf * yf));
        const float inset = bounded_radius - span;
        SDL_FRect strips[2] = {
            {
                rect->x + inset,
                rect->y + (float)row,
                rect->w - inset * 2.0F,
                1.0F,
            },
            {
                rect->x + inset,
                rect->y + rect->h - (float)row - 1.0F,
                rect->w - inset * 2.0F,
                1.0F,
            },
        };
        SDL_RenderFillRects(renderer, strips, SDL_arraysize(strips));
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

static DialogueThemeStyle draw_chrome(SDL_Renderer *renderer, EidolonDialogueTheme theme,
                                      const SDL_FRect *bubble, const EidolonDialogue *dialogue,
                                      float opacity, bool points_right) {
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
    SDL_SetRenderDrawColor(renderer, style.divider.r, style.divider.g, style.divider.b,
                           style.divider.a);
    SDL_RenderLine(renderer, bubble->x + 17.0F, bubble->y + 35.0F, bubble->x + bubble->w - 18.0F,
                   bubble->y + 35.0F);

    if (eidolon_dialogue_has_next_page(dialogue)) {
        SDL_Color indicator = style.accent;
        indicator.a = (Uint8)((float)indicator.a *
                              eidolon_dialogue_indicator_alpha(dialogue, SDL_GetTicks()));
        fill_triangle(
            renderer, (SDL_FPoint){bubble->x + bubble->w - 32.0F, bubble->y + bubble->h - 20.0F},
            (SDL_FPoint){bubble->x + bubble->w - 22.0F, bubble->y + bubble->h - 20.0F},
            (SDL_FPoint){bubble->x + bubble->w - 27.0F, bubble->y + bubble->h - 13.0F}, indicator);
    }
    return style;
}

static void draw_renderer_body(SDL_Renderer *renderer, EidolonTextRenderer *text_renderer,
                               const EidolonDialogue *dialogue, const SDL_FRect *bubble,
                               size_t body_slot, SDL_Color color) {
    if (eidolon_text_renderer_draw(text_renderer, body_slot, dialogue->page, dialogue->revealed,
                                   bubble->x + 19.0F, bubble->y + 39.0F, (int)(bubble->w - 38.0F),
                                   color)) {
        return;
    }
    char line[64];
    size_t line_length = 0U;
    float y = bubble->y + 39.0F;
    for (size_t index = 0U; index < dialogue->revealed && dialogue->page[index] != '\0'; ++index) {
        const char character = dialogue->page[index];
        if (character == '\n') {
            line[line_length] = '\0';
            SDL_RenderDebugText(renderer, bubble->x + 19.0F, y, line);
            line_length = 0U;
            y += 13.0F;
        } else if (line_length + 1U < sizeof(line)) {
            line[line_length++] = character;
        }
    }
    line[line_length] = '\0';
    SDL_RenderDebugText(renderer, bubble->x + 19.0F, y, line);
}

void eidolon_dialogue_art_draw_renderer(SDL_Renderer *renderer, EidolonTextRenderer *text_renderer,
                                        EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                        const EidolonDialogue *dialogue, const char *title,
                                        size_t title_slot, size_t body_slot, float opacity,
                                        bool points_right) {
    if (renderer == NULL || bubble == NULL || dialogue == NULL || opacity <= 0.0F) {
        return;
    }
    const DialogueThemeStyle style =
        draw_chrome(renderer, theme, bubble, dialogue, opacity, points_right);
    char heading[40];
    SDL_strlcpy(heading, title != NULL && title[0] != '\0' ? title : "EIDOLON", sizeof(heading));
    if (!eidolon_text_renderer_draw(text_renderer, title_slot, heading, strlen(heading),
                                    bubble->x + 28.0F, bubble->y + 11.0F, 0, style.title)) {
        SDL_SetRenderDrawColor(renderer, style.title.r, style.title.g, style.title.b,
                               style.title.a);
        SDL_RenderDebugText(renderer, bubble->x + 28.0F, bubble->y + 17.0F, heading);
    }
    draw_renderer_body(renderer, text_renderer, dialogue, bubble, body_slot, style.body);
}

bool eidolon_dialogue_art_draw_surface(SDL_Surface *surface, EidolonTextRenderer *text_renderer,
                                       EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                       const EidolonDialogue *dialogue, const char *title,
                                       size_t title_slot, size_t body_slot, bool points_right) {
    if (surface == NULL || text_renderer == NULL || bubble == NULL || dialogue == NULL) {
        return false;
    }
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == NULL) {
        return false;
    }
    const bool cleared =
        SDL_SetRenderDrawColor(renderer, 0U, 0U, 0U, 0U) && SDL_RenderClear(renderer);
    const DialogueThemeStyle style =
        draw_chrome(renderer, theme, bubble, dialogue, 1.0F, points_right);
    const bool flushed = SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);
    if (!cleared || !flushed) {
        return false;
    }

    char heading[40];
    SDL_strlcpy(heading, title != NULL && title[0] != '\0' ? title : "EIDOLON", sizeof(heading));
    return eidolon_text_renderer_draw_surface(text_renderer, title_slot, heading, strlen(heading),
                                              (int)(bubble->x + 28.0F), (int)(bubble->y + 11.0F), 0,
                                              style.title, surface) &&
           eidolon_text_renderer_draw_surface(text_renderer, body_slot, dialogue->page,
                                              dialogue->revealed, (int)(bubble->x + 19.0F),
                                              (int)(bubble->y + 39.0F), (int)(bubble->w - 38.0F),
                                              style.body, surface);
}
