#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "platform/overlay.h"

typedef struct OverlayHitTest {
    HWND hwnd;
    WNDPROC previous_proc;
    uint8_t *alpha;
    int width;
    int height;
} OverlayHitTest;

static OverlayHitTest overlay;

static LRESULT CALLBACK overlay_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
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

static bool apply_alpha_region(HWND hwnd, const SDL_Surface *rgba) {
    const size_t maximum_rects =
        ((size_t)rgba->w * (size_t)rgba->h + 1U) / 2U + (size_t)rgba->h;
    const size_t data_size = sizeof(RGNDATAHEADER) + maximum_rects * sizeof(RECT);
    RGNDATA *data = SDL_malloc(data_size);
    if (data == NULL) {
        return false;
    }

    data->rdh.dwSize = sizeof(RGNDATAHEADER);
    data->rdh.iType = RDH_RECTANGLES;
    data->rdh.nCount = 0;
    data->rdh.nRgnSize = 0;
    data->rdh.rcBound = (RECT){0, 0, rgba->w, rgba->h};
    RECT *rects = (RECT *)data->Buffer;

    for (int y = 0; y < rgba->h; ++y) {
        const uint8_t *row = (const uint8_t *)rgba->pixels + (size_t)y * (size_t)rgba->pitch;
        int x = 0;
        while (x < rgba->w) {
            while (x < rgba->w && row[(size_t)x * 4U + 3U] == 0) {
                ++x;
            }
            const int start = x;
            while (x < rgba->w && row[(size_t)x * 4U + 3U] != 0) {
                ++x;
            }
            if (start < x) {
                rects[data->rdh.nCount++] = (RECT){start, y, x, y + 1};
            }
        }
    }
    data->rdh.nRgnSize = data->rdh.nCount * sizeof(RECT);

    const DWORD region_size = (DWORD)(sizeof(RGNDATAHEADER) + data->rdh.nRgnSize);
    HRGN region = ExtCreateRegion(NULL, region_size, data);
    SDL_free(data);
    if (region == NULL) {
        return false;
    }
    if (!SetWindowRgn(hwnd, region, TRUE)) {
        DeleteObject(region);
        return false;
    }
    return true;
}

bool eidolon_platform_configure_overlay(SDL_Window *window) {
    HWND hwnd = window_handle(window);
    if (hwnd == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL did not expose the Win32 window handle");
        return false;
    }

    LONG_PTR extended_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    extended_style |= WS_EX_TOOLWINDOW;
    extended_style &= ~WS_EX_APPWINDOW;

    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extended_style) == 0 &&
        GetLastError() != ERROR_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SetWindowLongPtrW failed: %lu",
                     GetLastError());
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

    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
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
    SDL_zero(overlay);
}
