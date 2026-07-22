#define COBJMACROS

#include <SDL3/SDL.h>
#include <bgfx/c99/bgfx.h>
#include <d3d11.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum {
    HOST_WIDTH = 64,
    HOST_HEIGHT = 64,
};

static bool texture_handle_valid(bgfx_texture_handle_t handle)
{
    return handle.idx != UINT16_MAX;
}

static bool frame_buffer_handle_valid(bgfx_frame_buffer_handle_t handle)
{
    return handle.idx != UINT16_MAX;
}

static bool verify_pixel(ID3D11Device *device, ID3D11Texture2D *texture, uint16_t width,
                         uint16_t height, uint32_t rgba)
{
    D3D11_TEXTURE2D_DESC description;
    ID3D11Texture2D_GetDesc(texture, &description);
    if (description.Width != width || description.Height != height ||
        description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        fprintf(stderr, "bgfx interop: unexpected target description\n");
        return false;
    }

    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;

    ID3D11Texture2D *staging = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(device, &description, NULL, &staging);
    if (FAILED(hr)) {
        fprintf(stderr, "bgfx interop: staging texture creation failed: 0x%08lx\n",
                (unsigned long)hr);
        return false;
    }

    ID3D11DeviceContext *context = NULL;
    ID3D11Device_GetImmediateContext(device, &context);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging,
                                     (ID3D11Resource *)texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0,
                                 &mapped);
    if (FAILED(hr)) {
        fprintf(stderr, "bgfx interop: staging map failed: 0x%08lx\n", (unsigned long)hr);
        ID3D11DeviceContext_Release(context);
        ID3D11Texture2D_Release(staging);
        return false;
    }

    const uint8_t *row = (const uint8_t *)mapped.pData +
                         (size_t)(height / 2u) * (size_t)mapped.RowPitch;
    const uint8_t *pixel = row + (size_t)(width / 2u) * 4u;
    const uint8_t expected[4] = {
        (uint8_t)((rgba >> 8u) & 0xffu),
        (uint8_t)((rgba >> 16u) & 0xffu),
        (uint8_t)((rgba >> 24u) & 0xffu),
        (uint8_t)(rgba & 0xffu),
    };
    const bool matches = pixel[0] == expected[0] && pixel[1] == expected[1] &&
                         pixel[2] == expected[2] && pixel[3] == expected[3];
    if (!matches) {
        fprintf(stderr,
                "bgfx interop: expected BGRA %02x %02x %02x %02x, got %02x %02x %02x %02x\n",
                expected[0], expected[1], expected[2], expected[3], pixel[0], pixel[1],
                pixel[2], pixel[3]);
    }

    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    ID3D11DeviceContext_Release(context);
    ID3D11Texture2D_Release(staging);
    return matches;
}

static bool run_target_probe(ID3D11Device *device, uint16_t width, uint16_t height,
                             uint32_t premultiplied_rgba)
{
    bgfx_texture_handle_t texture_handle = BGFX_INVALID_HANDLE;
    bgfx_frame_buffer_handle_t frame_buffer = BGFX_INVALID_HANDLE;
    ID3D11Texture2D *texture = NULL;
    bool result = false;

    D3D11_TEXTURE2D_DESC description = {0};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = ID3D11Device_CreateTexture2D(device, &description, NULL, &texture);
    if (FAILED(hr)) {
        fprintf(stderr, "bgfx interop: external target creation failed: 0x%08lx\n",
                (unsigned long)hr);
        goto cleanup;
    }

    const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP |
                           BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
    texture_handle = bgfx_create_texture_2d(
        width, height, false, 1, BGFX_TEXTURE_FORMAT_BGRA8, flags, NULL,
        (uint64_t)(uintptr_t)texture);
    if (!texture_handle_valid(texture_handle)) {
        fprintf(stderr, "bgfx interop: bgfx rejected the external texture\n");
        goto cleanup;
    }
    (void)bgfx_frame(BGFX_FRAME_NONE);

    frame_buffer = bgfx_create_frame_buffer_from_handles(1, &texture_handle, false);
    if (!frame_buffer_handle_valid(frame_buffer)) {
        fprintf(stderr, "bgfx interop: framebuffer creation failed\n");
        goto cleanup;
    }
    (void)bgfx_frame(BGFX_FRAME_NONE);

    bgfx_set_view_frame_buffer(0, frame_buffer);
    bgfx_set_view_rect(0, 0, 0, width, height);
    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR, premultiplied_rgba, 1.0f, 0);
    bgfx_touch(0);
    (void)bgfx_frame(BGFX_FRAME_NONE);

    result = verify_pixel(device, texture, width, height, premultiplied_rgba);

cleanup:
    if (frame_buffer_handle_valid(frame_buffer)) {
        bgfx_frame_buffer_handle_t invalid = BGFX_INVALID_HANDLE;
        bgfx_set_view_frame_buffer(0, invalid);
        bgfx_destroy_frame_buffer(frame_buffer);
    }
    if (texture_handle_valid(texture_handle)) {
        bgfx_destroy_texture(texture_handle);
    }
    (void)bgfx_frame(BGFX_FRAME_NONE);
    (void)bgfx_frame(BGFX_FRAME_NONE);
    if (texture != NULL) {
        ID3D11Texture2D_Release(texture);
    }
    return result;
}

int main(void)
{
    SDL_Window *window = NULL;
    ID3D11Device *device = NULL;
    bool bgfx_ready = false;
    int result = 1;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "bgfx interop: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Eidolon bgfx interop", HOST_WIDTH, HOST_HEIGHT,
                              SDL_WINDOW_HIDDEN);
    if (window == NULL) {
        fprintf(stderr, "bgfx interop: SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    void *window_handle = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (window_handle == NULL) {
        fprintf(stderr, "bgfx interop: SDL did not expose a Win32 HWND\n");
        goto cleanup;
    }

    (void)bgfx_render_frame(0);

    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_DIRECT3D11;
    init.platformData.nwh = window_handle;
    init.platformData.type = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.resolution.width = HOST_WIDTH;
    init.resolution.height = HOST_HEIGHT;
    init.resolution.reset = BGFX_RESET_NONE;
    if (!bgfx_init(&init)) {
        fprintf(stderr, "bgfx interop: bgfx_init failed\n");
        goto cleanup;
    }
    bgfx_ready = true;

    const bgfx_internal_data_t *internal = bgfx_get_internal_data();
    if (internal == NULL || internal->context == NULL || internal->caps == NULL ||
        internal->caps->rendererType != BGFX_RENDERER_TYPE_DIRECT3D11) {
        fprintf(stderr, "bgfx interop: D3D11 internal data unavailable\n");
        goto cleanup;
    }
    if ((internal->caps->supported & BGFX_CAPS_TEXTURE_EXTERNAL) == 0u) {
        fprintf(stderr, "bgfx interop: D3D11 external textures are unsupported\n");
        goto cleanup;
    }
    device = (ID3D11Device *)internal->context;
    ID3D11Device_AddRef(device);

    if (!run_target_probe(device, 64, 64, 0x20408080u) ||
        !run_target_probe(device, 96, 48, 0x60301880u)) {
        goto cleanup;
    }

    printf("bgfx interop: external D3D11 targets=2 alpha=verified bridge_copies=0\n");
    result = 0;

cleanup:
    if (device != NULL) {
        ID3D11Device_Release(device);
    }
    if (bgfx_ready) {
        bgfx_shutdown();
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return result;
}
