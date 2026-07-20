#include <SDL3/SDL.h>

#include <stdio.h>

#include "dcimgui.h"
#include "dcimgui_impl_sdl3.h"
#include "dcimgui_impl_sdlrenderer3.h"

int main(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    ImGuiContext *context = NULL;
    int result = 1;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    window = SDL_CreateWindow("Eidolon ImGui smoke test", 640, 360,
                              SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    context = ImGui_CreateContext(NULL);
    if (context != NULL) {
        ImGui_GetIO()->IniFilename = NULL;
    }
    if (context == NULL || !cImGui_ImplSDL3_InitForSDLRenderer(window, renderer) ||
        !cImGui_ImplSDLRenderer3_Init(renderer)) {
        fprintf(stderr, "Dear ImGui initialization failed\n");
        goto cleanup;
    }

    cImGui_ImplSDLRenderer3_NewFrame();
    cImGui_ImplSDL3_NewFrame();
    ImGui_NewFrame();
    if (ImGui_Begin("Eidolon settings", NULL, 0)) {
        ImGui_Text("Dear ImGui %s is callable from C17.", ImGui_GetVersion());
    }
    ImGui_End();
    ImGui_Render();

    SDL_SetRenderDrawColor(renderer, 18U, 20U, 28U, 255U);
    SDL_RenderClear(renderer);
    cImGui_ImplSDLRenderer3_RenderDrawData(ImGui_GetDrawData(), renderer);
    SDL_RenderPresent(renderer);

    printf("Dear ImGui %s C binding smoke test passed\n", ImGui_GetVersion());
    result = 0;

cleanup:
    if (context != NULL) {
        cImGui_ImplSDLRenderer3_Shutdown();
        cImGui_ImplSDL3_Shutdown();
        ImGui_DestroyContext(context);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
