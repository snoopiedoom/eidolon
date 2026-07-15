#include "app.h"

#include "animation.h"
#include "draw.h"
#include "log.h"
#include "platform/overlay.h"

static bool load_atlas(EidolonApp *app) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/mutsuki-dress.png", EIDOLON_ASSET_DIR);

    SDL_Surface *surface = SDL_LoadPNG(path);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load %s: %s", path, SDL_GetError());
        return false;
    }

    const bool dimensions_are_valid =
        surface->w == EIDOLON_ATLAS_COLUMNS * EIDOLON_CELL_WIDTH &&
        surface->h == EIDOLON_ATLAS_ROWS * EIDOLON_CELL_HEIGHT;
    if (!dimensions_are_valid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid v2 atlas size %dx%d", surface->w,
                     surface->h);
        SDL_DestroySurface(surface);
        return false;
    }

    app->atlas = SDL_CreateTextureFromSurface(app->renderer, surface);
    SDL_DestroySurface(surface);
    if (app->atlas == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create atlas texture: %s",
                     SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(app->atlas, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(app->atlas, SDL_BLENDMODE_BLEND);
    return true;
}

static void set_initial_position(SDL_Window *window) {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    SDL_Rect bounds;
    if (!SDL_GetDisplayUsableBounds(display, &bounds)) {
        return;
    }

    const int margin = 24;
    SDL_SetWindowPosition(window, bounds.x + bounds.w - EIDOLON_WINDOW_WIDTH - margin,
                          bounds.y + bounds.h - EIDOLON_WINDOW_HEIGHT - margin);
}

bool eidolon_app_init(EidolonApp *app) {
    SDL_zero(*app);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const SDL_WindowFlags flags = SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
                                  SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    app->window = SDL_CreateWindow("Eidolon", EIDOLON_WINDOW_WIDTH, EIDOLON_WINDOW_HEIGHT, flags);
    if (app->window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    app->renderer = SDL_CreateRenderer(app->window, NULL);
    if (app->renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(app->renderer, 1);
    if (!eidolon_platform_configure_overlay(app->window)) {
        return false;
    }
    set_initial_position(app->window);

    if (!load_atlas(app)) {
        return false;
    }

    app->model = eidolon_model_create(app->renderer, EIDOLON_MODEL_PATH, EIDOLON_SHADER_DIR);
    if (app->model == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Rio 3D renderer unavailable; using sprite fallback: %s", SDL_GetError());
        eidolon_log_write("model", "initialization failed; sprite fallback active: %s",
                          SDL_GetError());
    }

    if (!eidolon_ipc_server_init(&app->ipc)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Could not open the local state channel; is Eidolon already running?");
        return false;
    }
    eidolon_log_write("renderer", "ipc server ready");

    app->running = true;
    app->state = EIDOLON_STATE_IDLE;
    eidolon_animation_set_state(&app->animation, app->state, SDL_GetTicks());
    return true;
}

void eidolon_app_set_state(EidolonApp *app, EidolonState state) {
    if (state == app->state) {
        return;
    }
    app->state = state;
    eidolon_animation_set_state(&app->animation, app->state, SDL_GetTicks());
}

static void handle_key(EidolonApp *app, SDL_Keycode key) {
    switch (key) {
    case SDLK_ESCAPE:
        app->running = false;
        break;
    case SDLK_1:
        eidolon_app_set_state(app, EIDOLON_STATE_IDLE);
        break;
    case SDLK_2:
        eidolon_app_set_state(app, EIDOLON_STATE_RUNNING);
        break;
    case SDLK_3:
        eidolon_app_set_state(app, EIDOLON_STATE_WAITING);
        break;
    case SDLK_4:
        eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
        break;
    case SDLK_5:
        eidolon_app_set_state(app, EIDOLON_STATE_FAILED);
        break;
    case SDLK_SPACE:
        eidolon_app_set_state(app,
                              (EidolonState)((app->state + 1) % EIDOLON_STATE_COUNT));
        break;
    default:
        break;
    }
}

static void begin_drag(EidolonApp *app) {
    app->dragging = true;
    app->drag_moved = false;
    SDL_GetGlobalMouseState(&app->drag_global_x, &app->drag_global_y);
    SDL_GetWindowPosition(app->window, &app->drag_window_x, &app->drag_window_y);
}

static void update_drag(EidolonApp *app) {
    float global_x = 0.0F;
    float global_y = 0.0F;
    SDL_GetGlobalMouseState(&global_x, &global_y);
    const int x = app->drag_window_x + (int)(global_x - app->drag_global_x);
    const int y = app->drag_window_y + (int)(global_y - app->drag_global_y);
    if (SDL_abs((int)(global_x - app->drag_global_x)) > 3 ||
        SDL_abs((int)(global_y - app->drag_global_y)) > 3) {
        app->drag_moved = true;
    }
    SDL_SetWindowPosition(app->window, x, y);
}

static void handle_event(EidolonApp *app, const SDL_Event *event) {
    switch (event->type) {
    case SDL_EVENT_QUIT:
        app->running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (!event->key.repeat) {
            handle_key(app, event->key.key);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            begin_drag(app);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (app->dragging && !app->drag_moved && app->state == EIDOLON_STATE_REVIEW) {
                eidolon_dialogue_advance(&app->dialogue, SDL_GetTicks());
            }
            app->dragging = false;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (app->dragging) {
            update_drag(app);
        }
        break;
    default:
        break;
    }
}

void eidolon_app_run(EidolonApp *app) {
    while (app->running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(app, &event);
        }

        EidolonState received_state;
        char received_text[EIDOLON_IPC_TEXT_CAPACITY + 1];
        while (eidolon_ipc_server_poll(&app->ipc, &received_state, received_text,
                                       sizeof(received_text))) {
            eidolon_log_write("renderer", "ipc receive state=%s bytes=%zu",
                              eidolon_state_name(received_state), strlen(received_text));
            eidolon_app_set_state(app, received_state);
            if (received_text[0] != '\0') {
                eidolon_dialogue_set(&app->dialogue, received_text, SDL_GetTicks());
            }
        }

        const uint64_t now_ms = SDL_GetTicks();
        char session_output[EIDOLON_IPC_TEXT_CAPACITY + 1];
        if (eidolon_session_watch_poll(&app->session_watch, now_ms, session_output,
                                       sizeof(session_output))) {
            eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
            eidolon_dialogue_set(&app->dialogue, session_output, now_ms);
        }
        eidolon_animation_update(&app->animation, app->state, now_ms);
        eidolon_dialogue_update(&app->dialogue, now_ms);
        eidolon_draw_frame(app);
        SDL_Delay(1);
    }
}

void eidolon_app_destroy(EidolonApp *app) {
    eidolon_ipc_server_destroy(&app->ipc);
    eidolon_platform_destroy_overlay(app->window);
    eidolon_model_destroy(app->model);
    SDL_DestroyTexture(app->atlas);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    SDL_zero(*app);
}
