#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <d3d11.h>
#include <windows.h>
#include <windowsx.h>

#include "platform/overlay.h"

#define HIT_REGION_TILE_SIZE 16
#define HIT_REGION_PADDING 16

typedef struct OverlayHitTest {
    HWND hwnd;
    WNDPROC previous_proc;
    uint8_t *alpha;
    int width;
    int height;
    ID3D11Device *readback_device;
    ID3D11Texture2D *readback_staging;
    ID3D11Texture2D *readback_resolve;
    UINT readback_width;
    UINT readback_height;
    UINT readback_mip_levels;
    UINT readback_array_size;
    DXGI_FORMAT readback_format;
    DXGI_SAMPLE_DESC readback_samples;
} OverlayHitTest;

static OverlayHitTest overlay;

static LRESULT CALLBACK overlay_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCACTIVATE) {
        return TRUE;
    }
    if (message == WM_NCPAINT) {
        return 0;
    }
    if (message == WM_NCHITTEST && overlay.alpha != NULL) {
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (ScreenToClient(hwnd, &point)) {
            RECT client;
            if (GetClientRect(hwnd, &client)) {
                const int client_width = client.right - client.left;
                const int client_height = client.bottom - client.top;
                if (client_width > 0 && client_height > 0 && point.x >= 0 && point.y >= 0 &&
                    point.x < client_width && point.y < client_height) {
                    const int x = point.x * overlay.width / client_width;
                    const int y = point.y * overlay.height / client_height;
                    if (x >= 0 && y >= 0 && x < overlay.width && y < overlay.height &&
                        overlay.alpha[(size_t)y * (size_t)overlay.width + (size_t)x] == 0) {
                        return HTTRANSPARENT;
                    }
                }
            }
        }
    }
    return CallWindowProcW(overlay.previous_proc, hwnd, message, wparam, lparam);
}

static HWND window_handle(SDL_Window *window) {
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (HWND)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

static SDL_PixelFormat d3d11_pixel_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return SDL_PIXELFORMAT_ABGR8888;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return SDL_PIXELFORMAT_ARGB8888;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static void destroy_readback_cache(void) {
    if (overlay.readback_resolve != NULL) {
        ID3D11Texture2D_Release(overlay.readback_resolve);
        overlay.readback_resolve = NULL;
    }
    if (overlay.readback_staging != NULL) {
        ID3D11Texture2D_Release(overlay.readback_staging);
        overlay.readback_staging = NULL;
    }
    overlay.readback_device = NULL;
    overlay.readback_width = 0;
    overlay.readback_height = 0;
    overlay.readback_mip_levels = 0;
    overlay.readback_array_size = 0;
    overlay.readback_format = DXGI_FORMAT_UNKNOWN;
    overlay.readback_samples = (DXGI_SAMPLE_DESC){0, 0};
}

static bool readback_cache_matches(ID3D11Device *device, const D3D11_TEXTURE2D_DESC *description) {
    return overlay.readback_staging != NULL && overlay.readback_device == device &&
           overlay.readback_width == description->Width &&
           overlay.readback_height == description->Height &&
           overlay.readback_mip_levels == description->MipLevels &&
           overlay.readback_array_size == description->ArraySize &&
           overlay.readback_format == description->Format &&
           overlay.readback_samples.Count == description->SampleDesc.Count &&
           overlay.readback_samples.Quality == description->SampleDesc.Quality;
}

static bool ensure_readback_cache(ID3D11Device *device,
                                  const D3D11_TEXTURE2D_DESC *source_description) {
    if (readback_cache_matches(device, source_description)) {
        return true;
    }

    destroy_readback_cache();

    D3D11_TEXTURE2D_DESC staging_description = *source_description;
    staging_description.SampleDesc.Count = 1;
    staging_description.SampleDesc.Quality = 0;
    staging_description.Usage = D3D11_USAGE_STAGING;
    staging_description.BindFlags = 0;
    staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_description.MiscFlags = 0;

    HRESULT result =
        ID3D11Device_CreateTexture2D(device, &staging_description, NULL, &overlay.readback_staging);
    if (FAILED(result)) {
        SDL_SetError("could not create D3D11 overlay staging texture (HRESULT 0x%08lx)",
                     (unsigned long)result);
        destroy_readback_cache();
        return false;
    }

    if (source_description->SampleDesc.Count > 1) {
        D3D11_TEXTURE2D_DESC resolve_description = staging_description;
        resolve_description.Usage = D3D11_USAGE_DEFAULT;
        resolve_description.CPUAccessFlags = 0;
        result = ID3D11Device_CreateTexture2D(device, &resolve_description, NULL,
                                              &overlay.readback_resolve);
        if (FAILED(result)) {
            SDL_SetError("could not create D3D11 overlay resolve texture (HRESULT 0x%08lx)",
                         (unsigned long)result);
            destroy_readback_cache();
            return false;
        }
    }

    overlay.readback_device = device;
    overlay.readback_width = source_description->Width;
    overlay.readback_height = source_description->Height;
    overlay.readback_mip_levels = source_description->MipLevels;
    overlay.readback_array_size = source_description->ArraySize;
    overlay.readback_format = source_description->Format;
    overlay.readback_samples = source_description->SampleDesc;
    return true;
}

SDL_Surface *eidolon_platform_read_pixels(SDL_Renderer *renderer) {
    ID3D11Device *device = SDL_GetPointerProperty(SDL_GetRendererProperties(renderer),
                                                  SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, NULL);
    if (device == NULL) {
        return SDL_RenderReadPixels(renderer, NULL);
    }
    if (!SDL_FlushRenderer(renderer)) {
        return NULL;
    }

    ID3D11DeviceContext *context = NULL;
    ID3D11RenderTargetView *render_target = NULL;
    ID3D11Texture2D *source = NULL;
    SDL_Surface *surface = NULL;
    ID3D11Device_GetImmediateContext(device, &context);
    if (context == NULL) {
        SDL_SetError("could not acquire D3D11 context for overlay readback");
        return NULL;
    }

    ID3D11DeviceContext_OMGetRenderTargets(context, 1, &render_target, NULL);
    if (render_target == NULL) {
        SDL_SetError("D3D11 renderer has no bound render target");
        goto done;
    }
    ID3D11View_GetResource(render_target, (ID3D11Resource **)&source);
    if (source == NULL) {
        SDL_SetError("could not get D3D11 overlay render target resource");
        goto done;
    }

    D3D11_TEXTURE2D_DESC description;
    ID3D11Texture2D_GetDesc(source, &description);
    const SDL_PixelFormat format = d3d11_pixel_format(description.Format);
    if (format == SDL_PIXELFORMAT_UNKNOWN || description.Width > INT_MAX ||
        description.Height > INT_MAX) {
        SDL_SetError("unsupported D3D11 overlay target format or dimensions");
        goto done;
    }
    if (!ensure_readback_cache(device, &description)) {
        goto done;
    }

    ID3D11Resource *copy_source = (ID3D11Resource *)source;
    if (overlay.readback_resolve != NULL) {
        ID3D11DeviceContext_ResolveSubresource(context, (ID3D11Resource *)overlay.readback_resolve,
                                               0, (ID3D11Resource *)source, 0, description.Format);
        copy_source = (ID3D11Resource *)overlay.readback_resolve;
    }
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)overlay.readback_staging,
                                     copy_source);

    D3D11_MAPPED_SUBRESOURCE mapped;
    const HRESULT result = ID3D11DeviceContext_Map(
        context, (ID3D11Resource *)overlay.readback_staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(result)) {
        SDL_SetError("could not map D3D11 overlay staging texture (HRESULT 0x%08lx)",
                     (unsigned long)result);
        goto done;
    }
    surface = SDL_CreateSurface((int)description.Width, (int)description.Height, format);
    if (surface != NULL) {
        const size_t row_bytes = (size_t)description.Width * 4U;
        for (UINT y = 0; y < description.Height; ++y) {
            SDL_memcpy((Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch,
                       (const Uint8 *)mapped.pData + (size_t)y * mapped.RowPitch, row_bytes);
        }
    }
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)overlay.readback_staging, 0);

done:
    if (source != NULL) {
        ID3D11Texture2D_Release(source);
    }
    if (render_target != NULL) {
        ID3D11RenderTargetView_Release(render_target);
    }
    ID3D11DeviceContext_Release(context);
    return surface;
}

static bool grow_region_data(RGNDATA **data, size_t *capacity, size_t required) {
    const size_t maximum_region_size = (size_t)MAXDWORD;
    const size_t maximum_rects = (maximum_region_size - sizeof(RGNDATAHEADER)) / sizeof(RECT);
    if (required > maximum_rects) {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return false;
    }

    size_t new_capacity = *capacity != 0 ? *capacity : 64U;
    while (new_capacity < required) {
        if (new_capacity > maximum_rects / 2U) {
            new_capacity = maximum_rects;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity > maximum_rects) {
        new_capacity = maximum_rects;
    }

    const size_t data_size = sizeof(RGNDATAHEADER) + new_capacity * sizeof(RECT);
    RGNDATA *grown = SDL_realloc(*data, data_size);
    if (grown == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
    *data = grown;
    *capacity = new_capacity;
    return true;
}

static bool region_tile_occupied(const SDL_Surface *rgba, int left, int top, int right,
                                 int bottom) {
    const int sample_left = SDL_max(0, left - HIT_REGION_PADDING);
    const int sample_top = SDL_max(0, top - HIT_REGION_PADDING);
    const int sample_right = SDL_min(rgba->w, right + HIT_REGION_PADDING);
    const int sample_bottom = SDL_min(rgba->h, bottom + HIT_REGION_PADDING);
    for (int y = sample_top; y < sample_bottom; ++y) {
        const uint8_t *row = (const uint8_t *)rgba->pixels + (size_t)y * (size_t)rgba->pitch;
        for (int x = sample_left; x < sample_right; ++x) {
            if (row[(size_t)x * 4U + 3U] != 0) {
                return true;
            }
        }
    }
    return false;
}

static bool apply_alpha_region(HWND hwnd, const SDL_Surface *rgba) {
    RGNDATA *data = SDL_malloc(sizeof(RGNDATAHEADER));
    if (data == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }

    data->rdh.dwSize = sizeof(RGNDATAHEADER);
    data->rdh.iType = RDH_RECTANGLES;
    data->rdh.nCount = 0;
    data->rdh.nRgnSize = 0;
    data->rdh.rcBound = (RECT){0, 0, rgba->w, rgba->h};
    size_t rect_count = 0;
    size_t rect_capacity = 0;

    for (int tile_y = 0; tile_y < rgba->h; tile_y += HIT_REGION_TILE_SIZE) {
        const int tile_bottom = SDL_min(tile_y + HIT_REGION_TILE_SIZE, rgba->h);
        int x = 0;
        while (x < rgba->w) {
            const int tile_right = SDL_min(x + HIT_REGION_TILE_SIZE, rgba->w);
            bool occupied = region_tile_occupied(rgba, x, tile_y, tile_right, tile_bottom);
            if (!occupied) {
                x = tile_right;
                continue;
            }

            const int start = x;
            x = tile_right;
            while (x < rgba->w) {
                const int next_right = SDL_min(x + HIT_REGION_TILE_SIZE, rgba->w);
                occupied = region_tile_occupied(rgba, x, tile_y, next_right, tile_bottom);
                if (!occupied) {
                    break;
                }
                x = next_right;
            }
            if (!grow_region_data(&data, &rect_capacity, rect_count + 1U)) {
                SDL_free(data);
                return false;
            }
            RECT *rects = (RECT *)data->Buffer;
            rects[rect_count++] = (RECT){start, tile_y, x, tile_bottom};
        }
    }

    const size_t rect_bytes = rect_count * sizeof(RECT);
    const size_t region_bytes = sizeof(RGNDATAHEADER) + rect_bytes;
    if (rect_count > (size_t)MAXDWORD || rect_bytes > (size_t)MAXDWORD ||
        region_bytes > (size_t)MAXDWORD) {
        SDL_free(data);
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return false;
    }
    data->rdh.nCount = (DWORD)rect_count;
    data->rdh.nRgnSize = (DWORD)rect_bytes;

    HRGN region = ExtCreateRegion(NULL, (DWORD)region_bytes, data);
    SDL_free(data);
    if (region == NULL) {
        return false;
    }
    if (!SetWindowRgn(hwnd, region, FALSE)) {
        DeleteObject(region);
        return false;
    }
    return true;
}

void eidolon_platform_suspend_hit_test(SDL_Window *window) {
    HWND hwnd = window_handle(window);
    if (hwnd != NULL && hwnd == overlay.hwnd) {
        SetWindowRgn(hwnd, NULL, FALSE);
    }
}

bool eidolon_platform_configure_overlay(SDL_Window *window) {
    HWND hwnd = window_handle(window);
    if (hwnd == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL did not expose the Win32 window handle");
        return false;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
               WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    style |= WS_POPUP;

    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWL_STYLE, style) == 0 && GetLastError() != ERROR_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not strip overlay window chrome: %lu",
                     GetLastError());
        return false;
    }

    LONG_PTR extended_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    extended_style |= WS_EX_TOOLWINDOW;
    extended_style &= ~WS_EX_APPWINDOW;

    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extended_style) == 0 &&
        GetLastError() != ERROR_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SetWindowLongPtrW failed: %lu", GetLastError());
        return false;
    }

    if (!SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SetWindowPos failed: %lu", GetLastError());
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous =
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)overlay_window_proc);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not install pixel hit test: %lu",
                     GetLastError());
        return false;
    }
    overlay.hwnd = hwnd;
    overlay.previous_proc = (WNDPROC)previous;

    return true;
}

bool eidolon_platform_update_hit_test(SDL_Window *window, SDL_Renderer *renderer) {
    if (window_handle(window) != overlay.hwnd || renderer == NULL) {
        return false;
    }

    SDL_Surface *surface = eidolon_platform_read_pixels(renderer);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not read overlay alpha: %s",
                     SDL_GetError());
        return false;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (rgba == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not convert overlay alpha: %s",
                     SDL_GetError());
        return false;
    }

    const size_t pixel_count = (size_t)rgba->w * (size_t)rgba->h;
    uint8_t *alpha = SDL_realloc(overlay.alpha, pixel_count);
    if (alpha == NULL) {
        SDL_DestroySurface(rgba);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not allocate overlay hit mask");
        return false;
    }
    overlay.alpha = alpha;
    overlay.width = rgba->w;
    overlay.height = rgba->h;

    for (int y = 0; y < rgba->h; ++y) {
        const uint8_t *row = (const uint8_t *)rgba->pixels + (size_t)y * (size_t)rgba->pitch;
        for (int x = 0; x < rgba->w; ++x) {
            overlay.alpha[(size_t)y * (size_t)rgba->w + (size_t)x] = row[(size_t)x * 4U + 3U];
        }
    }
    if (!apply_alpha_region(overlay.hwnd, rgba)) {
        SDL_DestroySurface(rgba);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not apply overlay hit region: %lu",
                     GetLastError());
        return false;
    }
    SDL_DestroySurface(rgba);
    return true;
}

void eidolon_platform_destroy_overlay(SDL_Window *window) {
    HWND hwnd = window_handle(window);
    if (hwnd != NULL && hwnd == overlay.hwnd && overlay.previous_proc != NULL) {
        SetWindowRgn(hwnd, NULL, FALSE);
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)overlay.previous_proc);
    }
    SDL_free(overlay.alpha);
    destroy_readback_cache();
    SDL_zero(overlay);
}
