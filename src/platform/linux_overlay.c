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

bool eidolon_platform_update_hit_test(SDL_Window *window, SDL_Renderer *renderer) {
    (void)window;
    (void)renderer;
    return true;
}

void eidolon_platform_destroy_overlay(SDL_Window *window) {
    (void)window;
}
