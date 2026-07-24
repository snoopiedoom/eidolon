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
    EidolonPresentationEnvironment initial_environment = {};
    if (!eidolon_presentation_get_environment(presentation, &initial_environment) ||
        initial_environment.revision == 0U || initial_environment.topology_revision == 0U ||
        initial_environment.active_output.value == 0U ||
        initial_environment.coordinate_space !=
            EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL) {
        std::fprintf(stderr, "environment bootstrap failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    {
        EidolonPresentationTopologyResult topology =
            eidolon_presentation_copy_outputs(presentation, nullptr, 0U);
        if (topology.status != EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY ||
            topology.revision != initial_environment.topology_revision ||
            topology.required_count == 0U) {
            std::fprintf(stderr, "topology probe failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        std::vector<EidolonPresentationOutputInfo> outputs(topology.required_count);
        topology =
            eidolon_presentation_copy_outputs(presentation, outputs.data(), outputs.size());
        if (topology.status != EIDOLON_PRESENTATION_TOPOLOGY_OK ||
            topology.copied_count != outputs.size()) {
            std::fprintf(stderr, "topology copy failed: %s\n", SDL_GetError());
            goto cleanup;
        }
    }
    {
        EidolonPresentationGeometry geometry = initial_environment.host_geometry;
        ++geometry.x;
        if (!eidolon_presentation_set_geometry(presentation, &geometry)) {
            std::fprintf(stderr, "environment move failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        EidolonPresentationEvent event = {};
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED ||
            event.data.environment.environment.revision <= initial_environment.revision ||
            event.data.environment.environment.host_geometry.x != geometry.x) {
            std::fprintf(stderr, "environment publication failed: %s\n", SDL_GetError());
            goto cleanup;
        }
    }
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
                EIDOLON_SCENE_INTERACTION_MOVE_ANCHOR |
                    EIDOLON_SCENE_INTERACTION_ROUTE_POINTER,
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
    {
        HWND window = FindWindowW(L"EidolonDirectCompositionHost",
                                  L"Eidolon DirectComposition backend smoke");
        if (window == nullptr) {
            std::fprintf(stderr, "native host lookup failed\n");
            goto cleanup;
        }
        SendMessageW(window, WM_MBUTTONDOWN, MK_MBUTTON, MAKELPARAM(10, 10));
        EidolonPresentationEvent event = {};
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_POINTER_DOWN ||
            event.data.pointer.layer.value != layer.value ||
            event.data.pointer.click_count != 1U) {
            std::fprintf(stderr, "native routed pointer down failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        SendMessageW(window, WM_MOUSEMOVE, MK_MBUTTON, MAKELPARAM(15, 12));
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_POINTER_MOTION ||
            event.data.pointer.layer_x_relative != 5.0F ||
            event.data.pointer.layer_y_relative != 2.0F) {
            std::fprintf(stderr, "native routed pointer motion failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        SendMessageW(window, WM_MBUTTONUP, 0U, MAKELPARAM(15, 12));
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_POINTER_UP) {
            std::fprintf(stderr, "native routed pointer up failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_MOVE_STARTED) {
            std::fprintf(stderr, "native move start failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        SendMessageW(window, WM_CAPTURECHANGED, 0U,
                     reinterpret_cast<LPARAM>(GetDesktopWindow()));
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED) {
            std::fprintf(stderr, "native capture cancellation failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        SendMessageW(window, WM_CLOSE, 0U, 0U);
        if (!eidolon_presentation_poll_event(presentation, &event) ||
            event.kind != EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED) {
            std::fprintf(stderr, "native close event failed: %s\n", SDL_GetError());
            goto cleanup;
        }
    }
    result = 0;
    std::puts("DirectComposition presentation backend smoke passed");

cleanup:
    eidolon_presentation_destroy(presentation);
    return result;
}
