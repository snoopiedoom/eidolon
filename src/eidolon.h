#ifndef EIDOLON_H
#define EIDOLON_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/ipc.h"
#include "dialogue.h"
#include "model.h"
#include "session_watch.h"
#include "state.h"

#define EIDOLON_WINDOW_WIDTH 520
#define EIDOLON_WINDOW_HEIGHT 360

#define EIDOLON_ATLAS_COLUMNS 8
#define EIDOLON_ATLAS_ROWS 11
#define EIDOLON_CELL_WIDTH 192
#define EIDOLON_CELL_HEIGHT 208

typedef struct EidolonAnimation {
    int row;
    int frame_count;
    int frame;
    uint64_t frame_started_ms;
} EidolonAnimation;

typedef struct EidolonApp {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *atlas;
    EidolonModelRenderer *model;
    EidolonState state;
    EidolonAnimation animation;
    EidolonDialogue dialogue;
    EidolonIpcServer ipc;
    EidolonSessionWatch session_watch;
    bool running;
    bool dragging;
    bool drag_moved;
    float drag_global_x;
    float drag_global_y;
    int drag_window_x;
    int drag_window_y;
    int hit_test_row;
    int hit_test_frame;
    int hit_test_mode;
    bool hit_test_initialized;
} EidolonApp;

#endif
