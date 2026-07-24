#include "event_pump.h"

#include "presentation_sdl_legacy.h"
#include "settings_ui.h"

#include <SDL3/SDL.h>

struct EidolonEventPump {
    EidolonPresentation *presentation;
    SDL_Window *presentation_window;
    SDL_Renderer *presentation_renderer;
    EidolonSettingsUi *settings_ui;
};

static EidolonAppPointerButton pointer_button(Uint8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return EIDOLON_APP_POINTER_BUTTON_PRIMARY;
    case SDL_BUTTON_MIDDLE:
        return EIDOLON_APP_POINTER_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT:
        return EIDOLON_APP_POINTER_BUTTON_SECONDARY;
    default:
        return EIDOLON_APP_POINTER_BUTTON_NONE;
    }
}

static uint64_t current_modifiers(void) {
    return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0 ? EIDOLON_APP_MODIFIER_SHIFT : 0U;
}

static bool belongs_to_presentation(const EidolonEventPump *pump, const SDL_Event *event) {
    SDL_Window *window = SDL_GetWindowFromEvent(event);
    return pump->presentation_window != NULL && window == pump->presentation_window;
}

static void pointer_position(EidolonAppPointerEvent *pointer) {
    (void)SDL_GetGlobalMouseState(&pointer->global_x, &pointer->global_y);
    pointer->global_position_valid = true;
}

static void route_event(EidolonEventPump *pump, const SDL_Event *source, EidolonAppEvent *event) {
    SDL_zero(*event);
    event->monotonic_ns = source->common.timestamp;
    (void)eidolon_sdl_legacy_handle_event(pump->presentation, source);
    if (eidolon_settings_ui_handle_event(pump->settings_ui, source)) {
        return;
    }

    switch (source->type) {
    case SDL_EVENT_QUIT:
        event->kind = EIDOLON_APP_EVENT_QUIT_REQUESTED;
        return;
    case SDL_EVENT_KEY_DOWN:
        if (!belongs_to_presentation(pump, source) || source->key.repeat) {
            return;
        }
        switch (source->key.key) {
        case SDLK_ESCAPE:
            event->kind = EIDOLON_APP_EVENT_QUIT_REQUESTED;
            break;
        case SDLK_F1:
            event->kind = EIDOLON_APP_EVENT_OPEN_SETTINGS;
            break;
        case SDLK_F5:
            event->kind = EIDOLON_APP_EVENT_RELOAD_CONFIGS;
            break;
        default:
            break;
        }
        return;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (belongs_to_presentation(pump, source)) {
            event->kind = EIDOLON_APP_EVENT_QUIT_REQUESTED;
        }
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (belongs_to_presentation(pump, source)) {
            event->kind = EIDOLON_APP_EVENT_FOCUS_LOST;
        }
        return;
    case SDL_EVENT_RENDER_TARGETS_RESET:
        event->kind = EIDOLON_APP_EVENT_GRAPHICS_TARGETS_RESET;
        return;
    case SDL_EVENT_RENDER_DEVICE_RESET:
    case SDL_EVENT_RENDER_DEVICE_LOST:
        event->kind = EIDOLON_APP_EVENT_GRAPHICS_DEVICE_LOST;
        return;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!belongs_to_presentation(pump, source)) {
            return;
        }
        SDL_Event converted = *source;
        if (pump->presentation_renderer != NULL &&
            !SDL_ConvertEventToRenderCoordinates(pump->presentation_renderer, &converted)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not convert input coordinates: %s",
                        SDL_GetError());
        }
        event->kind = source->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EIDOLON_APP_EVENT_POINTER_DOWN
                                                                  : EIDOLON_APP_EVENT_POINTER_UP;
        event->data.pointer.x = converted.button.x;
        event->data.pointer.y = converted.button.y;
        event->data.pointer.button = pointer_button(converted.button.button);
        event->data.pointer.clicks = converted.button.clicks;
        event->data.pointer.modifiers = current_modifiers();
        pointer_position(&event->data.pointer);
        return;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (!belongs_to_presentation(pump, source)) {
            return;
        }
        SDL_Event converted = *source;
        if (pump->presentation_renderer != NULL &&
            !SDL_ConvertEventToRenderCoordinates(pump->presentation_renderer, &converted)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not convert input coordinates: %s",
                        SDL_GetError());
        }
        event->kind = EIDOLON_APP_EVENT_POINTER_MOTION;
        event->data.pointer.x = converted.motion.x;
        event->data.pointer.y = converted.motion.y;
        event->data.pointer.x_relative = converted.motion.xrel;
        event->data.pointer.y_relative = converted.motion.yrel;
        event->data.pointer.modifiers = current_modifiers();
        pointer_position(&event->data.pointer);
        return;
    }
    default:
        return;
    }
}

EidolonEventPump *eidolon_event_pump_create(EidolonPresentation *presentation,
                                            EidolonSettingsUi *settings_ui) {
    if (presentation == NULL) {
        SDL_SetError("event pump requires a presentation");
        return NULL;
    }
    EidolonEventPump *pump = SDL_calloc(1U, sizeof(*pump));
    if (pump == NULL) {
        return NULL;
    }
    pump->presentation = presentation;
    pump->presentation_window = eidolon_sdl_legacy_window(presentation);
    pump->presentation_renderer = eidolon_sdl_legacy_renderer(presentation);
    pump->settings_ui = settings_ui;
    return pump;
}

void eidolon_event_pump_destroy(EidolonEventPump *pump) { SDL_free(pump); }

bool eidolon_event_pump_wait(EidolonEventPump *pump, int timeout_ms, EidolonAppEvent *event) {
    if (pump == NULL || event == NULL) {
        return false;
    }
    SDL_Event source;
    if (!SDL_WaitEventTimeout(&source, timeout_ms)) {
        SDL_zero(*event);
        return false;
    }
    route_event(pump, &source, event);
    return true;
}

bool eidolon_event_pump_poll(EidolonEventPump *pump, EidolonAppEvent *event) {
    if (pump == NULL || event == NULL) {
        return false;
    }
    SDL_Event source;
    if (!SDL_PollEvent(&source)) {
        SDL_zero(*event);
        return false;
    }
    route_event(pump, &source, event);
    return true;
}
