#include "platform/windows_dcomp.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

int main() {
    const EidolonWin32DcompConfig config = {
        "Eidolon DirectComposition backend smoke", -32000, -32000, 320, 240, false,
    };
    EidolonPresentation *presentation = eidolon_win32_dcomp_presentation_create(&config);
    if (presentation == nullptr) {
        std::fprintf(stderr, "create failed: %s\n", SDL_GetError());
        return 1;
    }

    int result = 1;
    const EidolonSceneLayerId layer = {1U};
    EidolonPresentationTargetUpdate update = {};
    if (!eidolon_presentation_configure_host(presentation) ||
        !eidolon_presentation_begin_target_update(presentation, layer, 128U, 192U,
                                                  EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED, 1U,
                                                  &update)) {
        std::fprintf(stderr, "target setup failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    {
        ID3D11Device *device = eidolon_win32_dcomp_device(presentation);
        ID3D11DeviceContext *context = eidolon_win32_dcomp_device_context(presentation);
        ID3D11Texture2D *texture =
            eidolon_win32_dcomp_target_texture(presentation, update.target, update.generation);
        ID3D11RenderTargetView *view = nullptr;
        if (device == nullptr || context == nullptr || texture == nullptr ||
            FAILED(device->CreateRenderTargetView(texture, nullptr, &view))) {
            std::fprintf(stderr, "render-target access failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        const float color[4] = {0.25F, 0.08F, 0.04F, 0.50F};
        context->ClearRenderTargetView(view, color);
        view->Release();
    }

    {
        const std::vector<uint8_t> alpha_mask(128U * 192U, 255U);
        if (!eidolon_presentation_set_target_alpha_mask(presentation, &update, alpha_mask.data(),
                                                        128U, 1U, 0U)) {
            std::fprintf(stderr, "alpha-mask upload failed: %s\n", SDL_GetError());
            goto cleanup;
        }
    }

    if (!eidolon_presentation_finish_target_update(presentation, &update, true)) {
        std::fprintf(stderr, "target submit failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    {
        const EidolonSceneSnapshot scene = {
            1U,
            1U,
            {{
                layer,
                1U,
                EIDOLON_SCENE_LAYER_BODY,
                EIDOLON_SCENE_INTERACTION_MOVE_ANCHOR,
                1U,
                1U,
                128U,
                192U,
                {-32000.0F, -32000.0F, 128.0F, 192.0F},
                0.0F,
                0.5F,
                0.5F,
                1.0F,
                0,
                true,
            }},
        };
        if (!eidolon_presentation_commit_scene(presentation, &scene) ||
            !eidolon_presentation_sync_host(presentation)) {
            std::fprintf(stderr, "scene commit failed: %s\n", SDL_GetError());
            goto cleanup;
        }
    }
    result = 0;
    std::puts("DirectComposition presentation backend smoke passed");

cleanup:
    eidolon_presentation_destroy(presentation);
    return result;
}
