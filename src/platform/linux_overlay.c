#include "platform/overlay.h"

bool eidolon_platform_configure_overlay(SDL_Window *window) {
    (void)window;
    /*
     * SDL owns transparency and always-on-top. Click-through and workspace
     * behavior differ between X11 and Wayland compositors, so those belong in
     * explicit capability backends instead of one fake "Linux" promise.
     */
    return true;
}

bool eidolon_platform_begin_window_drag(SDL_Window *window) {
    (void)window;
    return false;
}

void eidolon_platform_suspend_hit_test(SDL_Window *window) { (void)window; }

bool eidolon_platform_update_hit_test(SDL_Window *window, SDL_Renderer *renderer) {
    (void)window;
    (void)renderer;
    return true;
}

SDL_Surface *eidolon_platform_read_pixels(SDL_Renderer *renderer) {
    return SDL_RenderReadPixels(renderer, NULL);
}

void eidolon_platform_destroy_overlay(SDL_Window *window) { (void)window; }
