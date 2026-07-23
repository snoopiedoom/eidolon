#define WIN32_LEAN_AND_MEAN

#include "platform/windows_dcomp.h"
#include "presentation_internal.h"

#include <SDL3/SDL.h>

#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <windowsx.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace {

constexpr wchar_t kWindowClass[] = L"EidolonDirectCompositionHost";
constexpr size_t kTargetCapacity = EIDOLON_SCENE_LAYER_CAPACITY * 2U;
constexpr float kPi = 3.14159265358979323846F;

struct DcompTarget {
    EidolonSceneLayerId layer = {};
    EidolonPresentationTarget id = {};
    uint64_t generation = 0U;
    uint32_t width = 0U;
    uint32_t height = 0U;
    IDXGISwapChain1 *swap_chain = nullptr;
    ID3D11Texture2D *back_buffer = nullptr;
    IDCompositionVisual *visual = nullptr;
    IDCompositionEffectGroup *effect = nullptr;
    IDCompositionMatrixTransform *transform = nullptr;
    uint8_t *alpha_mask = nullptr;
    D2D_MATRIX_3X2_F pending_matrix = {1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F};
    D2D_MATRIX_3X2_F committed_matrix = {1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F};
    float pending_offset_x = 0.0F;
    float pending_offset_y = 0.0F;
    float committed_offset_x = 0.0F;
    float committed_offset_y = 0.0F;
    EidolonSceneLayerKind pending_kind = EIDOLON_SCENE_LAYER_TRANSIENT;
    EidolonSceneLayerKind committed_kind = EIDOLON_SCENE_LAYER_TRANSIENT;
    bool occupied = false;
};

struct Win32DcompPresentation {
    HWND window = nullptr;
    HINSTANCE instance = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    IDXGIDevice *dxgi_device = nullptr;
    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory2 *factory = nullptr;
    IDCompositionDevice *composition_device = nullptr;
    IDCompositionTarget *composition_target = nullptr;
    IDCompositionVisual *root = nullptr;
    DcompTarget targets[kTargetCapacity] = {};
    DcompTarget *input_order[kTargetCapacity] = {};
    size_t input_count = 0U;
    POINT drag_cursor_origin = {};
    POINT drag_window_origin = {};
    bool visible = false;
    bool com_initialized = false;
    bool input_suspended = false;
    bool dragging = false;
};

template <typename Interface> void release(Interface *&object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

bool hresult_ok(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) {
        return true;
    }
    SDL_SetError("%s failed: 0x%08lx", operation, static_cast<unsigned long>(result));
    return false;
}

Win32DcompPresentation *window_backend(HWND window) {
    return reinterpret_cast<Win32DcompPresentation *>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

DcompTarget *hit_test(Win32DcompPresentation *backend, float client_x, float client_y) {
    if (backend == nullptr || backend->input_suspended) {
        return nullptr;
    }
    for (size_t index = backend->input_count; index > 0U; --index) {
        DcompTarget *target = backend->input_order[index - 1U];
        if (target == nullptr || !target->occupied || target->alpha_mask == nullptr) {
            continue;
        }
        const D2D_MATRIX_3X2_F &matrix = target->committed_matrix;
        const float determinant = matrix._11 * matrix._22 - matrix._12 * matrix._21;
        if (std::fabs(determinant) < 0.000001F) {
            continue;
        }
        const float translated_x = client_x - target->committed_offset_x - matrix._31;
        const float translated_y = client_y - target->committed_offset_y - matrix._32;
        const float source_x =
            (translated_x * matrix._22 - translated_y * matrix._21) / determinant;
        const float source_y =
            (translated_y * matrix._11 - translated_x * matrix._12) / determinant;
        if (source_x < 0.0F || source_y < 0.0F || source_x >= static_cast<float>(target->width) ||
            source_y >= static_cast<float>(target->height)) {
            continue;
        }
        const size_t pixel = static_cast<size_t>(source_y) * static_cast<size_t>(target->width) +
                             static_cast<size_t>(source_x);
        if (target->alpha_mask[pixel] > 8U) {
            return target;
        }
    }
    return nullptr;
}

bool start_drag(Win32DcompPresentation *backend) {
    if (backend == nullptr || backend->input_suspended) {
        return false;
    }
    if (backend->dragging) {
        return true;
    }
    if (!GetCursorPos(&backend->drag_cursor_origin)) {
        return false;
    }
    RECT bounds = {};
    if (!GetWindowRect(backend->window, &bounds)) {
        return false;
    }
    backend->drag_window_origin = {bounds.left, bounds.top};
    backend->dragging = true;
    SetCapture(backend->window);
    return true;
}

void stop_drag(Win32DcompPresentation *backend, bool release_capture) {
    if (backend == nullptr) {
        return;
    }
    backend->dragging = false;
    if (release_capture && GetCapture() == backend->window) {
        ReleaseCapture();
    }
}

LRESULT CALLBACK host_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto *creation = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }
    Win32DcompPresentation *backend = window_backend(window);
    switch (message) {
    case WM_NCHITTEST: {
        POINT cursor = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window, &cursor);
        return hit_test(backend, static_cast<float>(cursor.x), static_cast<float>(cursor.y)) !=
                       nullptr
                   ? HTCLIENT
                   : HTTRANSPARENT;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN: {
        DcompTarget *target = hit_test(backend, static_cast<float>(GET_X_LPARAM(lparam)),
                                       static_cast<float>(GET_Y_LPARAM(lparam)));
        if (target != nullptr && target->committed_kind == EIDOLON_SCENE_LAYER_BODY) {
            (void)start_drag(backend);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (backend != nullptr && backend->dragging) {
            POINT cursor = {};
            if (GetCursorPos(&cursor)) {
                const int x =
                    backend->drag_window_origin.x + cursor.x - backend->drag_cursor_origin.x;
                const int y =
                    backend->drag_window_origin.y + cursor.y - backend->drag_cursor_origin.y;
                (void)SetWindowPos(window, HWND_TOPMOST, x, y, 0, 0,
                                   SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        stop_drag(backend, true);
        return 0;
    case WM_CAPTURECHANGED:
        stop_drag(backend, false);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

bool register_window_class(HINSTANCE instance) {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = host_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassW(&window_class) != 0) {
        return true;
    }
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        return true;
    }
    SDL_SetError("RegisterClassW failed: %lu", static_cast<unsigned long>(GetLastError()));
    return false;
}

DcompTarget *find_target(Win32DcompPresentation *backend, EidolonPresentationTarget target,
                         uint64_t generation = 0U) {
    for (DcompTarget &candidate : backend->targets) {
        if (candidate.occupied && candidate.id.value == target.value &&
            (generation == 0U || candidate.generation == generation)) {
            return &candidate;
        }
    }
    return nullptr;
}

void destroy_target_resource(DcompTarget &target) {
    delete[] target.alpha_mask;
    release(target.transform);
    release(target.effect);
    release(target.visual);
    release(target.back_buffer);
    release(target.swap_chain);
    target = {};
}

bool acquire_back_buffer(DcompTarget &target) {
    if (target.back_buffer != nullptr) {
        return true;
    }
    return hresult_ok(target.swap_chain->GetBuffer(0U, __uuidof(ID3D11Texture2D),
                                                   reinterpret_cast<void **>(&target.back_buffer)),
                      "composition swap-chain GetBuffer(0)");
}

void destroy_backend(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    stop_drag(backend, true);
    if (backend->root != nullptr) {
        backend->root->RemoveAllVisuals();
    }
    if (backend->composition_target != nullptr) {
        backend->composition_target->SetRoot(nullptr);
    }
    if (backend->composition_device != nullptr) {
        backend->composition_device->Commit();
    }
    for (DcompTarget &target : backend->targets) {
        destroy_target_resource(target);
    }
    release(backend->root);
    release(backend->composition_target);
    release(backend->composition_device);
    release(backend->factory);
    release(backend->adapter);
    release(backend->dxgi_device);
    release(backend->context);
    release(backend->device);
    if (backend->window != nullptr) {
        DestroyWindow(backend->window);
    }
    if (backend->com_initialized) {
        CoUninitialize();
    }
    delete backend;
}

bool configure_host(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (backend->visible) {
        ShowWindow(backend->window, SW_SHOWNOACTIVATE);
        UpdateWindow(backend->window);
    }
    return true;
}

bool get_geometry(void *opaque, EidolonPresentationGeometry *geometry) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    RECT bounds = {};
    if (!GetWindowRect(backend->window, &bounds)) {
        SDL_SetError("GetWindowRect failed: %lu", static_cast<unsigned long>(GetLastError()));
        return false;
    }
    *geometry = {
        bounds.left,
        bounds.top,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
    };
    return true;
}

bool set_geometry(void *opaque, const EidolonPresentationGeometry *geometry) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (!SetWindowPos(backend->window, HWND_TOPMOST, geometry->x, geometry->y, geometry->width,
                      geometry->height, SWP_NOACTIVATE | SWP_NOOWNERZORDER)) {
        SDL_SetError("SetWindowPos failed: %lu", static_cast<unsigned long>(GetLastError()));
        return false;
    }
    return true;
}

bool sync_host(void *opaque) {
    (void)opaque;
    return hresult_ok(DwmFlush(), "DwmFlush");
}

float display_scale(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    return static_cast<float>(GetDpiForWindow(backend->window)) / 96.0F;
}

bool set_vsync(void *opaque, int interval) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (interval < 0 || interval > 1) {
        SDL_SetError("DirectComposition VSync interval must be zero or one");
        return false;
    }
    return true;
}

bool begin_interactive_move(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (!start_drag(backend)) {
        SDL_SetError("could not begin native DirectComposition drag");
        return false;
    }
    return true;
}

void suspend_input_region(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    backend->input_suspended = true;
    stop_drag(backend, true);
}

bool update_input_region(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    backend->input_suspended = false;
    return true;
}

bool create_target(void *opaque, EidolonSceneLayerId layer, EidolonPresentationTarget id,
                   uint64_t generation, uint32_t width, uint32_t height,
                   EidolonPresentationAlphaMode alpha_mode) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (alpha_mode != EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED) {
        SDL_SetError("DirectComposition targets require premultiplied alpha");
        return false;
    }
    if (width > UINT16_MAX || height > UINT16_MAX) {
        SDL_SetError("DirectComposition target dimensions exceed the supported extent");
        return false;
    }
    DcompTarget *target = nullptr;
    for (DcompTarget &candidate : backend->targets) {
        if (!candidate.occupied) {
            target = &candidate;
            break;
        }
    }
    if (target == nullptr) {
        SDL_SetError("DirectComposition target capacity exhausted");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 description = {};
    description.Width = width;
    description.Height = height;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1U;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    description.BufferCount = 2U;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    target->layer = layer;
    target->id = id;
    target->generation = generation;
    target->width = width;
    target->height = height;
    target->occupied = true;
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    target->alpha_mask = new (std::nothrow) uint8_t[pixel_count]();
    if (target->alpha_mask == nullptr) {
        SDL_SetError("could not allocate DirectComposition target alpha mask");
        destroy_target_resource(*target);
        return false;
    }
    if (!hresult_ok(backend->factory->CreateSwapChainForComposition(backend->device, &description,
                                                                    nullptr, &target->swap_chain),
                    "CreateSwapChainForComposition") ||
        !hresult_ok(backend->composition_device->CreateVisual(&target->visual),
                    "IDCompositionDevice::CreateVisual") ||
        !hresult_ok(target->visual->SetContent(target->swap_chain),
                    "IDCompositionVisual::SetContent") ||
        !hresult_ok(backend->composition_device->CreateEffectGroup(&target->effect),
                    "IDCompositionDevice::CreateEffectGroup") ||
        !hresult_ok(target->effect->SetOpacity(1.0F), "IDCompositionEffectGroup::SetOpacity") ||
        !hresult_ok(target->visual->SetEffect(target->effect), "IDCompositionVisual::SetEffect") ||
        !hresult_ok(backend->composition_device->CreateMatrixTransform(&target->transform),
                    "IDCompositionDevice::CreateMatrixTransform") ||
        !hresult_ok(target->visual->SetTransform(target->transform),
                    "IDCompositionVisual::SetTransform") ||
        !acquire_back_buffer(*target)) {
        destroy_target_resource(*target);
        return false;
    }
    return true;
}

void destroy_target(void *opaque, EidolonPresentationTarget id) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    DcompTarget *target = find_target(backend, id);
    if (target != nullptr) {
        destroy_target_resource(*target);
    }
}

bool set_target_alpha_mask(void *opaque, EidolonPresentationTarget id, uint64_t generation,
                           const uint8_t *pixels, size_t pitch, uint8_t pixel_stride,
                           uint8_t alpha_offset) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    DcompTarget *target = find_target(backend, id, generation);
    if (target == nullptr || pixels == nullptr || pixel_stride == 0U ||
        alpha_offset >= pixel_stride || pitch < static_cast<size_t>(target->width) * pixel_stride ||
        target->alpha_mask == nullptr) {
        SDL_SetError("invalid DirectComposition target alpha mask");
        return false;
    }
    for (uint32_t y = 0U; y < target->height; ++y) {
        const uint8_t *source = pixels + static_cast<size_t>(y) * pitch;
        uint8_t *destination = target->alpha_mask + static_cast<size_t>(y) * target->width;
        for (uint32_t x = 0U; x < target->width; ++x) {
            destination[x] = source[static_cast<size_t>(x) * pixel_stride + alpha_offset];
        }
    }
    return true;
}

bool submit_target(void *opaque, EidolonPresentationTarget id, uint64_t generation) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    DcompTarget *target = find_target(backend, id, generation);
    if (target == nullptr) {
        SDL_SetError("DirectComposition target generation is stale");
        return false;
    }
    backend->context->Flush();
    if (!hresult_ok(target->swap_chain->Present(0U, 0U), "composition swap-chain Present")) {
        return false;
    }
    release(target->back_buffer);
    return true;
}

bool configure_layer(Win32DcompPresentation *backend,
                     const EidolonPresentationCommittedLayer &committed) {
    DcompTarget *target = find_target(backend, committed.target, committed.target_generation);
    if (target == nullptr || target->width != committed.target_width ||
        target->height != committed.target_height) {
        SDL_SetError("DirectComposition scene references an invalid target generation");
        return false;
    }

    EidolonPresentationGeometry host = {};
    if (!get_geometry(backend, &host)) {
        return false;
    }
    const float source_pivot_x = static_cast<float>(target->width) * committed.scene.pivot_x;
    const float source_pivot_y = static_cast<float>(target->height) * committed.scene.pivot_y;
    const float destination_pivot_x = committed.scene.bounds.width * committed.scene.pivot_x;
    const float destination_pivot_y = committed.scene.bounds.height * committed.scene.pivot_y;
    const float scale_x = committed.scene.bounds.width / static_cast<float>(target->width);
    const float scale_y = committed.scene.bounds.height / static_cast<float>(target->height);
    const float radians = committed.scene.rotation_degrees * kPi / 180.0F;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const D2D_MATRIX_3X2_F matrix = {
        scale_x * cosine,
        scale_x * sine,
        -scale_y * sine,
        scale_y * cosine,
        destination_pivot_x - (source_pivot_x * scale_x * cosine - source_pivot_y * scale_y * sine),
        destination_pivot_y - (source_pivot_x * scale_x * sine + source_pivot_y * scale_y * cosine),
    };
    target->pending_matrix = matrix;
    target->pending_offset_x = committed.scene.bounds.x - static_cast<float>(host.x);
    target->pending_offset_y = committed.scene.bounds.y - static_cast<float>(host.y);
    target->pending_kind = committed.scene.kind;
    return hresult_ok(target->transform->SetMatrix(target->pending_matrix),
                      "IDCompositionMatrixTransform::SetMatrix") &&
           hresult_ok(target->visual->SetOffsetX(target->pending_offset_x),
                      "IDCompositionVisual::SetOffsetX") &&
           hresult_ok(target->visual->SetOffsetY(target->pending_offset_y),
                      "IDCompositionVisual::SetOffsetY") &&
           hresult_ok(target->effect->SetOpacity(committed.scene.opacity),
                      "IDCompositionEffectGroup::SetOpacity");
}

bool commit_scene(void *opaque, const EidolonPresentationSceneCommit *commit) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    size_t order[EIDOLON_SCENE_LAYER_CAPACITY] = {};
    size_t count = 0U;
    for (size_t index = 0U; index < commit->layer_count; ++index) {
        const EidolonPresentationCommittedLayer &layer = commit->layers[index];
        if (!layer.scene.visible || !layer.has_target) {
            continue;
        }
        order[count++] = index;
    }
    for (size_t index = 1U; index < count; ++index) {
        const size_t candidate = order[index];
        size_t insertion = index;
        while (insertion > 0U && commit->layers[order[insertion - 1U]].scene.z_order >
                                     commit->layers[candidate].scene.z_order) {
            order[insertion] = order[insertion - 1U];
            --insertion;
        }
        order[insertion] = candidate;
    }

    if (!hresult_ok(backend->root->RemoveAllVisuals(), "IDCompositionVisual::RemoveAllVisuals")) {
        return false;
    }
    IDCompositionVisual *previous = nullptr;
    for (size_t index = 0U; index < count; ++index) {
        const EidolonPresentationCommittedLayer &layer = commit->layers[order[index]];
        if (!configure_layer(backend, layer)) {
            return false;
        }
        DcompTarget *target = find_target(backend, layer.target, layer.target_generation);
        if (!hresult_ok(backend->root->AddVisual(target->visual, previous != nullptr, previous),
                        "IDCompositionVisual::AddVisual")) {
            return false;
        }
        previous = target->visual;
    }
    if (!hresult_ok(backend->composition_device->Commit(), "IDCompositionDevice::Commit")) {
        return false;
    }
    backend->input_count = 0U;
    for (size_t index = 0U; index < count; ++index) {
        const EidolonPresentationCommittedLayer &layer = commit->layers[order[index]];
        DcompTarget *target = find_target(backend, layer.target, layer.target_generation);
        target->committed_matrix = target->pending_matrix;
        target->committed_offset_x = target->pending_offset_x;
        target->committed_offset_y = target->pending_offset_y;
        target->committed_kind = target->pending_kind;
        backend->input_order[backend->input_count++] = target;
    }
    return true;
}

bool present(void *opaque) {
    (void)opaque;
    return true;
}

bool initialize_backend(Win32DcompPresentation *backend, const EidolonWin32DcompConfig *config) {
    backend->instance = GetModuleHandleW(nullptr);
    backend->visible = config->visible;
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(com_result)) {
        backend->com_initialized = true;
    } else if (com_result != RPC_E_CHANGED_MODE) {
        return hresult_ok(com_result, "CoInitializeEx");
    }
    if (!register_window_class(backend->instance)) {
        return false;
    }

    wchar_t title[128] = L"Eidolon";
    if (config->title != nullptr && config->title[0] != '\0') {
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, config->title, -1, title,
                                static_cast<int>(sizeof(title) / sizeof(title[0]))) == 0) {
            SDL_SetError("invalid UTF-8 DirectComposition window title");
            return false;
        }
    }
    backend->window = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        kWindowClass, title, WS_POPUP, config->x, config->y, config->width, config->height, nullptr,
        nullptr, backend->instance, backend);
    if (backend->window == nullptr) {
        SDL_SetError("CreateWindowExW failed: %lu", static_cast<unsigned long>(GetLastError()));
        return false;
    }

    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0U,
        D3D11_SDK_VERSION, &backend->device, &feature_level, &backend->context);
    if (FAILED(result)) {
        result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0U, D3D11_SDK_VERSION,
                                   &backend->device, &feature_level, &backend->context);
    }
    if (!hresult_ok(result, "D3D11CreateDevice") ||
        !hresult_ok(backend->device->QueryInterface(
                        __uuidof(IDXGIDevice), reinterpret_cast<void **>(&backend->dxgi_device)),
                    "ID3D11Device::QueryInterface(IDXGIDevice)") ||
        !hresult_ok(backend->dxgi_device->GetAdapter(&backend->adapter),
                    "IDXGIDevice::GetAdapter") ||
        !hresult_ok(backend->adapter->GetParent(__uuidof(IDXGIFactory2),
                                                reinterpret_cast<void **>(&backend->factory)),
                    "IDXGIAdapter::GetParent(IDXGIFactory2)") ||
        !hresult_ok(
            DCompositionCreateDevice(backend->dxgi_device, __uuidof(IDCompositionDevice),
                                     reinterpret_cast<void **>(&backend->composition_device)),
            "DCompositionCreateDevice") ||
        !hresult_ok(backend->composition_device->CreateTargetForHwnd(backend->window, TRUE,
                                                                     &backend->composition_target),
                    "IDCompositionDevice::CreateTargetForHwnd") ||
        !hresult_ok(backend->composition_device->CreateVisual(&backend->root),
                    "IDCompositionDevice::CreateVisual(root)") ||
        !hresult_ok(backend->composition_target->SetRoot(backend->root),
                    "IDCompositionTarget::SetRoot") ||
        !hresult_ok(backend->composition_device->Commit(), "initial IDCompositionDevice::Commit")) {
        return false;
    }
    return true;
}

} // namespace

extern "C" EidolonPresentation *
eidolon_win32_dcomp_presentation_create(const EidolonWin32DcompConfig *config) {
    if (config == nullptr || config->width <= 0 || config->height <= 0) {
        SDL_SetError("invalid DirectComposition presentation configuration");
        return nullptr;
    }
    auto *backend = new Win32DcompPresentation();
    if (!initialize_backend(backend, config)) {
        destroy_backend(backend);
        return nullptr;
    }
    const EidolonPresentationBackendOps operations = {
        destroy_backend,
        configure_host,
        get_geometry,
        set_geometry,
        sync_host,
        display_scale,
        set_vsync,
        begin_interactive_move,
        suspend_input_region,
        update_input_region,
        create_target,
        destroy_target,
        set_target_alpha_mask,
        submit_target,
        commit_scene,
        present,
    };
    const uint64_t capabilities =
        EIDOLON_PRESENTATION_CAP_PERSISTENT_OVER_OTHER_APPS |
        EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT | EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS |
        EIDOLON_PRESENTATION_CAP_COMPOSITOR_TRANSFORM |
        EIDOLON_PRESENTATION_CAP_COMPOSITOR_OPACITY | EIDOLON_PRESENTATION_CAP_GPU_ZERO_COPY |
        EIDOLON_PRESENTATION_CAP_BACKGROUND_VISIBILITY | EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT |
        EIDOLON_PRESENTATION_CAP_NATIVE_INTERACTIVE_MOVE;
    EidolonPresentation *presentation =
        eidolon_presentation_create_backend("win32_dcomp", capabilities, backend, &operations);
    if (presentation == nullptr) {
        destroy_backend(backend);
    }
    return presentation;
}

extern "C" ID3D11Device *eidolon_win32_dcomp_device(EidolonPresentation *presentation) {
    auto *backend = static_cast<Win32DcompPresentation *>(
        eidolon_presentation_backend_context(presentation, "win32_dcomp"));
    return backend != nullptr ? backend->device : nullptr;
}

extern "C" ID3D11DeviceContext *
eidolon_win32_dcomp_device_context(EidolonPresentation *presentation) {
    auto *backend = static_cast<Win32DcompPresentation *>(
        eidolon_presentation_backend_context(presentation, "win32_dcomp"));
    return backend != nullptr ? backend->context : nullptr;
}

extern "C" ID3D11Texture2D *eidolon_win32_dcomp_target_texture(EidolonPresentation *presentation,
                                                               EidolonPresentationTarget target,
                                                               uint64_t generation) {
    auto *backend = static_cast<Win32DcompPresentation *>(
        eidolon_presentation_backend_context(presentation, "win32_dcomp"));
    if (backend == nullptr) {
        return nullptr;
    }
    DcompTarget *resource = find_target(backend, target, generation);
    if (resource == nullptr) {
        SDL_SetError("DirectComposition target generation is unavailable");
        return nullptr;
    }
    return acquire_back_buffer(*resource) ? resource->back_buffer : nullptr;
}
