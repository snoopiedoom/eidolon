#ifndef EIDOLON_PLATFORM_OVERLAY_H
#define EIDOLON_PLATFORM_OVERLAY_H

#include <SDL3/SDL.h>

#include <stdbool.h>

bool eidolon_platform_configure_overlay(SDL_Window *window);
bool eidolon_platform_update_hit_test(SDL_Window *window, SDL_Renderer *renderer);
void eidolon_platform_destroy_overlay(SDL_Window *window);

#endif
