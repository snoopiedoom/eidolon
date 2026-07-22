#include <SDL3/SDL.h>
#include <bgfx/c99/bgfx.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum {
    SMOKE_WIDTH = 64,
    SMOKE_HEIGHT = 64,
    SMOKE_FRAMES = 8,
};

static int fail(const char *message)
{
    fprintf(stderr, "bgfx smoke: %s\n", message);
    return 1;
}

int main(void)
{
    SDL_Window *window = NULL;
    bool bgfx_ready = false;
    int result = 1;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "bgfx smoke: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Eidolon bgfx smoke", SMOKE_WIDTH, SMOKE_HEIGHT,
                              SDL_WINDOW_HIDDEN);
    if (window == NULL) {
        fprintf(stderr, "bgfx smoke: SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void *window_handle = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (window_handle == NULL) {
        result = fail("SDL did not expose a Win32 HWND");
        goto cleanup;
    }

    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_DIRECT3D11;
    init.platformData.nwh = window_handle;
    init.platformData.type = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.resolution.width = SMOKE_WIDTH;
    init.resolution.height = SMOKE_HEIGHT;
    init.resolution.reset = BGFX_RESET_NONE;

    if (!bgfx_init(&init)) {
        result = fail("bgfx_init rejected the D3D11 configuration");
        goto cleanup;
    }
    bgfx_ready = true;

    const bgfx_renderer_type_t renderer = bgfx_get_renderer_type();
    const bgfx_caps_t *caps = bgfx_get_caps();
    if (renderer != BGFX_RENDERER_TYPE_DIRECT3D11 || caps == NULL ||
        caps->rendererType != BGFX_RENDERER_TYPE_DIRECT3D11) {
        result = fail("bgfx selected a renderer other than D3D11");
        goto cleanup;
    }

    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x203040ff, 1.0f, 0);
    bgfx_set_view_rect(0, 0, 0, SMOKE_WIDTH, SMOKE_HEIGHT);
    for (uint32_t frame = 0; frame < SMOKE_FRAMES; ++frame) {
        bgfx_touch(0);
        (void)bgfx_frame(BGFX_FRAME_NONE);
    }

    printf("bgfx smoke: renderer=%s frames=%u\n", bgfx_get_renderer_name(renderer),
           (unsigned int)SMOKE_FRAMES);
    result = 0;

cleanup:
    if (bgfx_ready) {
        bgfx_shutdown();
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return result;
}
