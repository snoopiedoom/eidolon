#include "text_renderer.h"

#include "log.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <string.h>

#define TEXT_CACHE_CAPACITY 4096U
#define FALLBACK_FONT_CAPACITY 3U

typedef struct TextSlot {
    TTF_Text *text;
    char content[TEXT_CACHE_CAPACITY];
    size_t length;
    int wrap_width;
    SDL_Color color;
    bool initialized;
} TextSlot;

struct EidolonTextRenderer {
    SDL_Renderer *renderer;
    TTF_TextEngine *renderer_engine;
    TTF_TextEngine *surface_engine;
    TTF_Font *font;
    TTF_Font *fallbacks[FALLBACK_FONT_CAPACITY];
    TextSlot renderer_slots[EIDOLON_TEXT_SLOT_COUNT];
    TextSlot surface_slots[EIDOLON_TEXT_SLOT_COUNT];
};

static void add_fallback(EidolonTextRenderer *text_renderer, size_t slot, const char *path,
                         float point_size) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE) {
        return;
    }
    TTF_Font *fallback = TTF_OpenFont(path, point_size);
    if (fallback == NULL || !TTF_AddFallbackFont(text_renderer->font, fallback)) {
        eidolon_log_write("text", "could not add fallback %s: %s", path, SDL_GetError());
        TTF_CloseFont(fallback);
        return;
    }
    text_renderer->fallbacks[slot] = fallback;
    eidolon_log_write("text", "fallback loaded: %s", path);
}

EidolonTextRenderer *eidolon_text_renderer_create(SDL_Renderer *renderer, const char *font_path,
                                                  float point_size) {
    if (font_path == NULL || !TTF_Init()) {
        return NULL;
    }
    EidolonTextRenderer *text_renderer = SDL_calloc(1U, sizeof(*text_renderer));
    if (text_renderer == NULL) {
        TTF_Quit();
        return NULL;
    }
    if (renderer != NULL) {
        text_renderer->renderer_engine = TTF_CreateRendererTextEngine(renderer);
    }
    text_renderer->renderer = renderer;
    text_renderer->surface_engine = TTF_CreateSurfaceTextEngine();
    text_renderer->font = TTF_OpenFont(font_path, point_size);
    if ((renderer != NULL && text_renderer->renderer_engine == NULL) ||
        text_renderer->surface_engine == NULL || text_renderer->font == NULL) {
        eidolon_text_renderer_destroy(text_renderer);
        return NULL;
    }
#if defined(_WIN32)
    add_fallback(text_renderer, 0U, "C:/Windows/Fonts/msyh.ttc", point_size);
    add_fallback(text_renderer, 1U, "C:/Windows/Fonts/malgun.ttf", point_size);
    add_fallback(text_renderer, 2U, "C:/Windows/Fonts/seguiemj.ttf", point_size);
#endif
    eidolon_log_write("text", "primary font loaded: %s %.1fpt", font_path, point_size);
    return text_renderer;
}

void eidolon_text_renderer_destroy(EidolonTextRenderer *text_renderer) {
    if (text_renderer == NULL) {
        return;
    }
    for (size_t index = 0U; index < EIDOLON_TEXT_SLOT_COUNT; ++index) {
        TTF_DestroyText(text_renderer->renderer_slots[index].text);
        TTF_DestroyText(text_renderer->surface_slots[index].text);
    }
    if (text_renderer->font != NULL) {
        TTF_ClearFallbackFonts(text_renderer->font);
    }
    for (size_t index = 0U; index < FALLBACK_FONT_CAPACITY; ++index) {
        TTF_CloseFont(text_renderer->fallbacks[index]);
    }
    TTF_CloseFont(text_renderer->font);
    if (text_renderer->renderer_engine != NULL) {
        TTF_DestroyRendererTextEngine(text_renderer->renderer_engine);
    }
    if (text_renderer->surface_engine != NULL) {
        TTF_DestroySurfaceTextEngine(text_renderer->surface_engine);
    }
    SDL_free(text_renderer);
    TTF_Quit();
}

bool eidolon_text_renderer_set_renderer(EidolonTextRenderer *text_renderer,
                                        SDL_Renderer *renderer) {
    if (text_renderer == NULL) {
        SDL_SetError("missing text renderer");
        return false;
    }
    if (text_renderer->renderer == renderer) {
        return true;
    }

    TTF_TextEngine *engine = NULL;
    if (renderer != NULL) {
        engine = TTF_CreateRendererTextEngine(renderer);
        if (engine == NULL) {
            return false;
        }
    }
    for (size_t index = 0U; index < EIDOLON_TEXT_SLOT_COUNT; ++index) {
        TTF_DestroyText(text_renderer->renderer_slots[index].text);
        SDL_zero(text_renderer->renderer_slots[index]);
    }
    if (text_renderer->renderer_engine != NULL) {
        TTF_DestroyRendererTextEngine(text_renderer->renderer_engine);
    }
    text_renderer->renderer = renderer;
    text_renderer->renderer_engine = engine;
    return true;
}

static TextSlot *prepare_text(EidolonTextRenderer *text_renderer, TTF_TextEngine *engine,
                              TextSlot *slots, size_t slot_index, const char *text, size_t length,
                              int wrap_width, SDL_Color color) {
    if (text_renderer == NULL || engine == NULL || slots == NULL ||
        slot_index >= EIDOLON_TEXT_SLOT_COUNT || text == NULL || length >= TEXT_CACHE_CAPACITY) {
        return NULL;
    }
    TextSlot *slot = &slots[slot_index];
    if (slot->text == NULL) {
        slot->text = TTF_CreateText(engine, text_renderer->font, "", 0U);
        if (slot->text == NULL) {
            return NULL;
        }
    }
    if (!slot->initialized || slot->length != length || memcmp(slot->content, text, length) != 0) {
        if (!TTF_SetTextString(slot->text, text, length)) {
            return NULL;
        }
        SDL_memcpy(slot->content, text, length);
        slot->content[length] = '\0';
        slot->length = length;
    }
    if (!slot->initialized || slot->wrap_width != wrap_width) {
        if (!TTF_SetTextWrapWidth(slot->text, wrap_width)) {
            return NULL;
        }
        slot->wrap_width = wrap_width;
    }
    if (!slot->initialized || slot->color.r != color.r || slot->color.g != color.g ||
        slot->color.b != color.b || slot->color.a != color.a) {
        if (!TTF_SetTextColor(slot->text, color.r, color.g, color.b, color.a)) {
            return NULL;
        }
        slot->color = color;
    }
    slot->initialized = true;
    return slot;
}

bool eidolon_text_renderer_draw(EidolonTextRenderer *text_renderer, size_t slot_index,
                                const char *text, size_t length, float x, float y, int wrap_width,
                                SDL_Color color) {
    if (text_renderer == NULL) {
        return false;
    }
    TextSlot *slot =
        prepare_text(text_renderer, text_renderer->renderer_engine, text_renderer->renderer_slots,
                     slot_index, text, length, wrap_width, color);
    if (slot == NULL) {
        return false;
    }
    return TTF_DrawRendererText(slot->text, x, y);
}

bool eidolon_text_renderer_draw_surface(EidolonTextRenderer *text_renderer, size_t slot_index,
                                        const char *text, size_t length, int x, int y,
                                        int wrap_width, SDL_Color color, SDL_Surface *surface) {
    if (text_renderer == NULL || surface == NULL) {
        return false;
    }
    TextSlot *slot =
        prepare_text(text_renderer, text_renderer->surface_engine, text_renderer->surface_slots,
                     slot_index, text, length, wrap_width, color);
    return slot != NULL && TTF_DrawSurfaceText(slot->text, x, y, surface);
}
