#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "platform/windows_dcomp.h"
#include "presentation_event_queue.h"
#include "presentation_internal.h"

#include <SDL3/SDL.h>

#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"EidolonDirectCompositionHost";
constexpr size_t kTargetCapacity = EIDOLON_SCENE_LAYER_CAPACITY * 2U;
constexpr float kPi = 3.14159265358979323846F;
constexpr LONG kActivationDragThreshold = 4L;
constexpr uint64_t kCapabilities =
    EIDOLON_PRESENTATION_CAP_PERSISTENT_OVER_OTHER_APPS |
    EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT | EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS |
    EIDOLON_PRESENTATION_CAP_COMPOSITOR_TRANSFORM | EIDOLON_PRESENTATION_CAP_COMPOSITOR_OPACITY |
    EIDOLON_PRESENTATION_CAP_GPU_ZERO_COPY | EIDOLON_PRESENTATION_CAP_BACKGROUND_VISIBILITY |
    EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT | EIDOLON_PRESENTATION_CAP_NATIVE_INTERACTIVE_MOVE;

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
    EidolonSceneInteractionPolicy pending_interaction = EIDOLON_SCENE_INTERACTION_PASS_THROUGH;
    EidolonSceneInteractionPolicy committed_interaction = EIDOLON_SCENE_INTERACTION_PASS_THROUGH;
    uint64_t pending_scene_revision = 0U;
    uint64_t committed_scene_revision = 0U;
    bool occupied = false;
};

struct HitTestResult {
    DcompTarget *target = nullptr;
    float layer_x = 0.0F;
    float layer_y = 0.0F;
};

struct Win32OutputRecord {
    HMONITOR monitor = nullptr;
    EidolonPresentationOutputInfo info = {};
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
    EidolonPresentationEventQueue event_queue = {};
    std::vector<Win32OutputRecord> outputs;
    EidolonPresentationEnvironment environment = {};
    uint64_t topology_revision = 0U;
    uint32_t next_output_id = 1U;
    size_t input_count = 0U;
    POINT drag_cursor_origin = {};
    POINT drag_window_origin = {};
    POINT activation_cursor_origin = {};
    DcompTarget *pointer_target = nullptr;
    float pointer_host_x = 0.0F;
    float pointer_host_y = 0.0F;
    float pointer_layer_x = 0.0F;
    float pointer_layer_y = 0.0F;
    EidolonSceneLayerId interaction_layer = {};
    uint64_t interaction_scene_revision = 0U;
    bool visible = false;
    bool com_initialized = false;
    bool input_suspended = false;
    bool dragging = false;
    bool activation_pending = false;
    bool pointer_routing = false;
    bool environment_valid = false;
    bool environment_dirty = true;
    bool graphics_reset_pending = false;
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

bool map_target(const DcompTarget *target, float client_x, float client_y, float *layer_x,
                float *layer_y) {
    if (target == nullptr || layer_x == nullptr || layer_y == nullptr) {
        return false;
    }
    const D2D_MATRIX_3X2_F &matrix = target->committed_matrix;
    const float determinant = matrix._11 * matrix._22 - matrix._12 * matrix._21;
    if (std::fabs(determinant) < 0.000001F) {
        return false;
    }
    const float translated_x = client_x - target->committed_offset_x - matrix._31;
    const float translated_y = client_y - target->committed_offset_y - matrix._32;
    *layer_x = (translated_x * matrix._22 - translated_y * matrix._21) / determinant;
    *layer_y = (translated_y * matrix._11 - translated_x * matrix._12) / determinant;
    return true;
}

HitTestResult hit_test(Win32DcompPresentation *backend, float client_x, float client_y) {
    HitTestResult result;
    if (backend == nullptr || backend->input_suspended) {
        return result;
    }
    for (size_t index = backend->input_count; index > 0U; --index) {
        DcompTarget *target = backend->input_order[index - 1U];
        if (target == nullptr || !target->occupied || target->alpha_mask == nullptr ||
            target->committed_interaction == EIDOLON_SCENE_INTERACTION_PASS_THROUGH) {
            continue;
        }
        float source_x = 0.0F;
        float source_y = 0.0F;
        if (!map_target(target, client_x, client_y, &source_x, &source_y)) {
            continue;
        }
        if (source_x < 0.0F || source_y < 0.0F || source_x >= static_cast<float>(target->width) ||
            source_y >= static_cast<float>(target->height)) {
            continue;
        }
        const size_t pixel = static_cast<size_t>(source_y) * static_cast<size_t>(target->width) +
                             static_cast<size_t>(source_x);
        if (target->alpha_mask[pixel] > 8U) {
            result.target = target;
            result.layer_x = source_x;
            result.layer_y = source_y;
            return result;
        }
    }
    return result;
}

EidolonPresentationGeometry current_geometry(Win32DcompPresentation *backend) {
    RECT bounds = {};
    if (backend == nullptr || !GetWindowRect(backend->window, &bounds)) {
        return {};
    }
    return {
        bounds.left,
        bounds.top,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
    };
}

bool enqueue_event(Win32DcompPresentation *backend, EidolonPresentationEventKind kind,
                   EidolonSceneLayerId layer, uint64_t scene_revision, float host_x, float host_y,
                   float layer_x, float layer_y) {
    if (backend == nullptr || kind == EIDOLON_PRESENTATION_EVENT_NONE) {
        return false;
    }
    EidolonPresentationEvent event = {};
    event.kind = kind;
    event.monotonic_ns = SDL_GetTicksNS();
    event.host = {1U};
    if (kind == EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED ||
        kind == EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED) {
        event.data.layer = {
            scene_revision, layer, host_x, host_y, layer_x, layer_y,
        };
    } else {
        event.data.move = {
            scene_revision, backend->environment_valid ? backend->environment.revision : 0U,
            layer,          current_geometry(backend),
            host_x,         host_y,
            layer_x,        layer_y,
        };
    }
    return eidolon_presentation_event_queue_push(&backend->event_queue, &event);
}

bool enqueue_structural_event(Win32DcompPresentation *backend,
                              EidolonPresentationEventKind kind,
                              EidolonPresentationGraphicsResetKind reset_kind) {
    if (backend == nullptr || kind == EIDOLON_PRESENTATION_EVENT_NONE) {
        return false;
    }
    EidolonPresentationEvent event = {};
    event.kind = kind;
    event.monotonic_ns = SDL_GetTicksNS();
    event.host = {1U};
    event.data.graphics.reset_kind = reset_kind;
    return eidolon_presentation_event_queue_push(&backend->event_queue, &event);
}

uint64_t pointer_buttons(WPARAM state) {
    uint64_t buttons = 0U;
    if ((state & MK_LBUTTON) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_PRIMARY;
    }
    if ((state & MK_MBUTTON) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE;
    }
    if ((state & MK_RBUTTON) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_SECONDARY;
    }
    return buttons;
}

uint64_t pointer_modifiers() {
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0
               ? EIDOLON_PRESENTATION_POINTER_MODIFIER_SHIFT
               : 0U;
}

bool enqueue_pointer_event(Win32DcompPresentation *backend, EidolonPresentationEventKind kind,
                           float host_x, float host_y, float layer_x, float layer_y,
                           uint64_t buttons, uint32_t click_count) {
    if (backend == nullptr || backend->pointer_target == nullptr) {
        return false;
    }
    const EidolonPresentationGeometry geometry = current_geometry(backend);
    EidolonPresentationEvent event = {};
    event.kind = kind;
    event.monotonic_ns = SDL_GetTicksNS();
    event.host = {1U};
    event.data.pointer = {
        backend->pointer_target->committed_scene_revision,
        1U,
        buttons,
        pointer_modifiers(),
        EIDOLON_PRESENTATION_POINTER_COORDINATE_HOST |
            EIDOLON_PRESENTATION_POINTER_COORDINATE_LAYER |
            EIDOLON_PRESENTATION_POINTER_COORDINATE_GLOBAL,
        backend->pointer_target->layer,
        EIDOLON_PRESENTATION_POINTER_DEVICE_MOUSE,
        click_count,
        host_x,
        host_y,
        layer_x,
        layer_y,
        layer_x - backend->pointer_layer_x,
        layer_y - backend->pointer_layer_y,
        static_cast<float>(geometry.x) + host_x,
        static_cast<float>(geometry.y) + host_y,
    };
    const bool accepted = eidolon_presentation_event_queue_push(&backend->event_queue, &event);
    backend->pointer_host_x = host_x;
    backend->pointer_host_y = host_y;
    backend->pointer_layer_x = layer_x;
    backend->pointer_layer_y = layer_y;
    return accepted;
}

void stop_pointer_routing(Win32DcompPresentation *backend, bool release_capture) {
    if (backend == nullptr) {
        return;
    }
    backend->pointer_routing = false;
    backend->pointer_target = nullptr;
    if (release_capture && GetCapture() == backend->window) {
        ReleaseCapture();
    }
}

bool start_pointer_routing(Win32DcompPresentation *backend, const HitTestResult &hit, float host_x,
                           float host_y, uint64_t buttons, uint32_t click_count) {
    if (backend == nullptr || hit.target == nullptr ||
        (hit.target->committed_interaction & EIDOLON_SCENE_INTERACTION_ROUTE_POINTER) == 0U) {
        return false;
    }
    backend->pointer_target = hit.target;
    backend->pointer_routing = true;
    backend->pointer_host_x = host_x;
    backend->pointer_host_y = host_y;
    backend->pointer_layer_x = hit.layer_x;
    backend->pointer_layer_y = hit.layer_y;
    SetCapture(backend->window);
    if (GetCapture() != backend->window) {
        stop_pointer_routing(backend, false);
        return false;
    }
    if (enqueue_pointer_event(backend, EIDOLON_PRESENTATION_EVENT_POINTER_DOWN, host_x, host_y,
                              hit.layer_x, hit.layer_y, buttons, click_count)) {
        return true;
    }
    stop_pointer_routing(backend, true);
    return false;
}

bool graphics_hresult_ok(Win32DcompPresentation *backend, HRESULT result,
                         const char *operation) {
    if (hresult_ok(result, operation)) {
        return true;
    }
    if (backend != nullptr && !backend->graphics_reset_pending) {
        EidolonPresentationGraphicsResetKind reset_kind =
            EIDOLON_PRESENTATION_GRAPHICS_RESET_BACKEND;
        if (backend->device != nullptr && FAILED(backend->device->GetDeviceRemovedReason())) {
            reset_kind = EIDOLON_PRESENTATION_GRAPHICS_RESET_DEVICE;
        }
        backend->graphics_reset_pending = true;
        (void)enqueue_structural_event(
            backend, EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED, reset_kind);
    }
    return false;
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
    if (GetCapture() == backend->window) {
        return true;
    }
    backend->dragging = false;
    return false;
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
        return hit_test(backend, static_cast<float>(cursor.x), static_cast<float>(cursor.y))
                           .target != nullptr
                   ? HTCLIENT
                   : HTTRANSPARENT;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_CLOSE:
        if (backend != nullptr) {
            (void)enqueue_structural_event(
                backend, EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED,
                EIDOLON_PRESENTATION_GRAPHICS_RESET_NONE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
        const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
        const HitTestResult hit = hit_test(backend, host_x, host_y);
        if (hit.target == nullptr || backend == nullptr) {
            return 0;
        }
        backend->interaction_layer = hit.target->layer;
        backend->interaction_scene_revision = hit.target->committed_scene_revision;
        if ((hit.target->committed_interaction & EIDOLON_SCENE_INTERACTION_MOVE_ANCHOR) != 0U) {
            if (start_drag(backend)) {
                (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_MOVE_STARTED,
                                    backend->interaction_layer, backend->interaction_scene_revision,
                                    host_x, host_y, hit.layer_x, hit.layer_y);
            } else {
                backend->interaction_layer = {};
                backend->interaction_scene_revision = 0U;
            }
        } else if ((hit.target->committed_interaction & EIDOLON_SCENE_INTERACTION_ACTIVATE) != 0U) {
            backend->activation_pending = GetCursorPos(&backend->activation_cursor_origin) != FALSE;
            if (backend->activation_pending) {
                SetCapture(window);
            }
        }
        return 0;
    }
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK: {
        const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
        const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
        const HitTestResult hit = hit_test(backend, host_x, host_y);
        (void)start_pointer_routing(
            backend, hit, host_x, host_y,
            pointer_buttons(wparam) | EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE,
            message == WM_MBUTTONDBLCLK ? 2U : 1U);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (backend != nullptr && backend->pointer_routing &&
            backend->pointer_target != nullptr) {
            const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
            const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
            float layer_x = 0.0F;
            float layer_y = 0.0F;
            if (!map_target(backend->pointer_target, host_x, host_y, &layer_x, &layer_y) ||
                !enqueue_pointer_event(backend, EIDOLON_PRESENTATION_EVENT_POINTER_MOTION, host_x,
                                       host_y, layer_x, layer_y, pointer_buttons(wparam), 0U)) {
                stop_pointer_routing(backend, true);
            }
        } else if (backend != nullptr && backend->dragging) {
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
    case WM_MBUTTONUP:
        if (backend != nullptr && backend->pointer_routing &&
            backend->pointer_target != nullptr) {
            const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
            const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
            float layer_x = 0.0F;
            float layer_y = 0.0F;
            if (map_target(backend->pointer_target, host_x, host_y, &layer_x, &layer_y)) {
                (void)enqueue_pointer_event(
                    backend, EIDOLON_PRESENTATION_EVENT_POINTER_UP, host_x, host_y, layer_x,
                    layer_y,
                    pointer_buttons(wparam) &
                        ~(uint64_t)EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE,
                    0U);
            }
            stop_pointer_routing(backend, true);
        }
        return 0;
    case WM_LBUTTONUP: {
        const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
        const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
        if (backend != nullptr && backend->dragging) {
            const EidolonSceneLayerId layer = backend->interaction_layer;
            const uint64_t scene_revision = backend->interaction_scene_revision;
            stop_drag(backend, true);
            backend->environment_dirty = true;
            (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED, layer,
                                scene_revision, host_x, host_y, 0.0F, 0.0F);
            backend->interaction_layer = {};
            backend->interaction_scene_revision = 0U;
        } else if (backend != nullptr && backend->activation_pending) {
            POINT cursor = {};
            const bool cursor_valid = GetCursorPos(&cursor) != FALSE;
            const LONG moved_x = cursor_valid
                                     ? std::abs(cursor.x - backend->activation_cursor_origin.x)
                                     : kActivationDragThreshold + 1L;
            const LONG moved_y = cursor_valid
                                     ? std::abs(cursor.y - backend->activation_cursor_origin.y)
                                     : kActivationDragThreshold + 1L;
            const HitTestResult hit = hit_test(backend, host_x, host_y);
            const bool activated = hit.target != nullptr &&
                                   hit.target->layer.value == backend->interaction_layer.value &&
                                   moved_x <= kActivationDragThreshold &&
                                   moved_y <= kActivationDragThreshold;
            const EidolonSceneLayerId layer = backend->interaction_layer;
            const uint64_t scene_revision = backend->interaction_scene_revision;
            backend->activation_pending = false;
            backend->interaction_layer = {};
            backend->interaction_scene_revision = 0U;
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            if (activated) {
                (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED, layer,
                                    scene_revision, host_x, host_y, hit.layer_x, hit.layer_y);
            }
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
        return 0;
    case WM_RBUTTONUP: {
        const float host_x = static_cast<float>(GET_X_LPARAM(lparam));
        const float host_y = static_cast<float>(GET_Y_LPARAM(lparam));
        const HitTestResult hit = hit_test(backend, host_x, host_y);
        if (hit.target != nullptr) {
            (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED,
                                hit.target->layer, hit.target->committed_scene_revision, host_x,
                                host_y, hit.layer_x, hit.layer_y);
        }
        return 0;
    }
    case WM_DPICHANGED:
        if (backend != nullptr) {
            const auto *suggested = reinterpret_cast<const RECT *>(lparam);
            if (suggested != nullptr) {
                (void)SetWindowPos(window, HWND_TOPMOST, suggested->left, suggested->top,
                                   suggested->right - suggested->left,
                                   suggested->bottom - suggested->top,
                                   SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
            backend->environment_dirty = true;
        }
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_DEVICECHANGE:
        if (backend != nullptr) {
            backend->environment_dirty = true;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_WINDOWPOSCHANGED:
        if (backend != nullptr && !backend->dragging) {
            backend->environment_dirty = true;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_CAPTURECHANGED:
        if (backend != nullptr && backend->pointer_routing &&
            backend->pointer_target != nullptr) {
            (void)enqueue_pointer_event(
                backend, EIDOLON_PRESENTATION_EVENT_POINTER_CANCELED,
                backend->pointer_host_x, backend->pointer_host_y, backend->pointer_layer_x,
                backend->pointer_layer_y, 0U, 0U);
            stop_pointer_routing(backend, false);
        }
        if (backend != nullptr && backend->dragging) {
            const EidolonSceneLayerId layer = backend->interaction_layer;
            const uint64_t scene_revision = backend->interaction_scene_revision;
            stop_drag(backend, false);
            (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED, layer,
                                scene_revision, 0.0F, 0.0F, 0.0F, 0.0F);
        }
        if (backend != nullptr) {
            backend->activation_pending = false;
            backend->interaction_layer = {};
            backend->interaction_scene_revision = 0U;
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

bool register_window_class(HINSTANCE instance) {
    WNDCLASSW window_class = {};
    window_class.style = CS_DBLCLKS;
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
    backend->environment_dirty = true;
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
    if (backend->pointer_routing && backend->pointer_target != nullptr) {
        (void)enqueue_pointer_event(
            backend, EIDOLON_PRESENTATION_EVENT_POINTER_CANCELED,
            backend->pointer_host_x, backend->pointer_host_y, backend->pointer_layer_x,
            backend->pointer_layer_y, 0U, 0U);
        stop_pointer_routing(backend, true);
    }
    if (backend->dragging) {
        const EidolonSceneLayerId layer = backend->interaction_layer;
        const uint64_t scene_revision = backend->interaction_scene_revision;
        stop_drag(backend, true);
        (void)enqueue_event(backend, EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED, layer,
                            scene_revision, 0.0F, 0.0F, 0.0F, 0.0F);
    }
    backend->activation_pending = false;
    backend->interaction_layer = {};
    backend->interaction_scene_revision = 0U;
    if (GetCapture() == backend->window) {
        ReleaseCapture();
    }
}

bool update_input_region(void *opaque) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    backend->input_suspended = false;
    return true;
}

EidolonPresentationRect portable_rect(const RECT &rect) {
    return {
        static_cast<float>(rect.left),
        static_cast<float>(rect.top),
        static_cast<float>(rect.right - rect.left),
        static_cast<float>(rect.bottom - rect.top),
    };
}

EidolonPresentationOrientation portable_orientation(DWORD orientation) {
    switch (orientation) {
    case DMDO_DEFAULT:
        return EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE;
    case DMDO_90:
        return EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT;
    case DMDO_180:
        return EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE_FLIPPED;
    case DMDO_270:
        return EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT_FLIPPED;
    default:
        return EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN;
    }
}

uint32_t existing_output_id(const Win32DcompPresentation *backend, HMONITOR monitor) {
    for (const Win32OutputRecord &record : backend->outputs) {
        if (record.monitor == monitor) {
            return record.info.output.value;
        }
    }
    return 0U;
}

struct OutputEnumeration {
    Win32DcompPresentation *backend = nullptr;
    std::vector<Win32OutputRecord> records;
    bool failed = false;
};

BOOL CALLBACK enumerate_output(HMONITOR monitor, HDC, LPRECT, LPARAM opaque) {
    auto *enumeration = reinterpret_cast<OutputEnumeration *>(opaque);
    MONITORINFOEXW native_info = {};
    native_info.cbSize = sizeof(native_info);
    if (!GetMonitorInfoW(monitor, &native_info)) {
        enumeration->failed = true;
        return FALSE;
    }

    uint32_t output_id = existing_output_id(enumeration->backend, monitor);
    if (output_id == 0U) {
        output_id = enumeration->backend->next_output_id++;
        if (output_id == 0U) {
            enumeration->failed = true;
            return FALSE;
        }
    }

    Win32OutputRecord record = {};
    record.monitor = monitor;
    record.info.output = {output_id};
    record.info.bounds = portable_rect(native_info.rcMonitor);
    record.info.usable_bounds = portable_rect(native_info.rcWork);
    record.info.coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL;
    record.info.capabilities = kCapabilities;
    record.info.flags = (native_info.dwFlags & MONITORINFOF_PRIMARY) != 0U
                            ? EIDOLON_PRESENTATION_OUTPUT_PRIMARY
                            : 0U;
    record.info.valid_fields = EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                               EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
                               EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE;

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(native_info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        if ((mode.dmFields & DM_DISPLAYFREQUENCY) != 0U && mode.dmDisplayFrequency > 1U) {
            record.info.nominal_refresh_hz = static_cast<float>(mode.dmDisplayFrequency);
            record.info.valid_fields |= EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH;
        }
        if ((mode.dmFields & DM_DISPLAYORIENTATION) != 0U) {
            const EidolonPresentationOrientation orientation =
                portable_orientation(mode.dmDisplayOrientation);
            if (orientation != EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN) {
                record.info.orientation = orientation;
                record.info.valid_fields |= EIDOLON_PRESENTATION_ENV_ORIENTATION;
            }
        }
    }

    try {
        enumeration->records.push_back(record);
    } catch (...) {
        enumeration->failed = true;
        return FALSE;
    }
    return TRUE;
}

bool same_rect(const EidolonPresentationRect &left, const EidolonPresentationRect &right) {
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

bool same_insets(const EidolonPresentationInsets &left, const EidolonPresentationInsets &right) {
    return left.top == right.top && left.right == right.right && left.bottom == right.bottom &&
           left.left == right.left;
}

bool same_output(const Win32OutputRecord &left, const Win32OutputRecord &right) {
    const EidolonPresentationOutputInfo &a = left.info;
    const EidolonPresentationOutputInfo &b = right.info;
    return left.monitor == right.monitor && a.output.value == b.output.value &&
           same_rect(a.bounds, b.bounds) && same_rect(a.usable_bounds, b.usable_bounds) &&
           same_insets(a.safe_area, b.safe_area) && a.content_scale == b.content_scale &&
           a.pixel_scale == b.pixel_scale && a.nominal_refresh_hz == b.nominal_refresh_hz &&
           a.orientation == b.orientation && a.coordinate_space == b.coordinate_space &&
           a.capabilities == b.capabilities && a.flags == b.flags &&
           a.valid_fields == b.valid_fields;
}

bool same_topology(const std::vector<Win32OutputRecord> &left,
                   const std::vector<Win32OutputRecord> &right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t index = 0U; index < left.size(); ++index) {
        if (!same_output(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool field_presence_changed(const EidolonPresentationEnvironment &previous,
                            const EidolonPresentationEnvironment &candidate, uint64_t field) {
    return ((previous.valid_fields ^ candidate.valid_fields) & field) != 0U;
}

uint64_t environment_changes(const EidolonPresentationEnvironment &previous,
                             const EidolonPresentationEnvironment &candidate) {
    uint64_t changed = previous.valid_fields ^ candidate.valid_fields;
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY) != 0U &&
        (previous.host_geometry.x != candidate.host_geometry.x ||
         previous.host_geometry.y != candidate.host_geometry.y ||
         previous.host_geometry.width != candidate.host_geometry.width ||
         previous.host_geometry.height != candidate.host_geometry.height)) {
        changed |= EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT) != 0U &&
        previous.active_output.value != candidate.active_output.value) {
        changed |= EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS) != 0U &&
        !same_rect(previous.output_bounds, candidate.output_bounds)) {
        changed |= EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS) != 0U &&
        !same_rect(previous.usable_bounds, candidate.usable_bounds)) {
        changed |= EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_SAFE_AREA) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_SAFE_AREA) != 0U &&
        !same_insets(previous.safe_area, candidate.safe_area)) {
        changed |= EIDOLON_PRESENTATION_ENV_SAFE_AREA;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_CONTENT_SCALE) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_CONTENT_SCALE) != 0U &&
        previous.content_scale != candidate.content_scale) {
        changed |= EIDOLON_PRESENTATION_ENV_CONTENT_SCALE;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_PIXEL_SCALE) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_PIXEL_SCALE) != 0U &&
        previous.pixel_scale != candidate.pixel_scale) {
        changed |= EIDOLON_PRESENTATION_ENV_PIXEL_SCALE;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) != 0U &&
        previous.nominal_refresh_hz != candidate.nominal_refresh_hz) {
        changed |= EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_ORIENTATION) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_ORIENTATION) != 0U &&
        previous.orientation != candidate.orientation) {
        changed |= EIDOLON_PRESENTATION_ENV_ORIENTATION;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE) != 0U &&
        previous.coordinate_space != candidate.coordinate_space) {
        changed |= EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY) != 0U &&
        previous.topology_revision != candidate.topology_revision) {
        changed |= EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY;
    }
    if (!field_presence_changed(previous, candidate, EIDOLON_PRESENTATION_ENV_CAPABILITIES) &&
        (candidate.valid_fields & EIDOLON_PRESENTATION_ENV_CAPABILITIES) != 0U &&
        previous.capabilities != candidate.capabilities) {
        changed |= EIDOLON_PRESENTATION_ENV_CAPABILITIES;
    }
    return changed;
}

bool reconcile_environment(Win32DcompPresentation *backend, bool publish_event) {
    if (backend == nullptr || (!backend->environment_dirty && backend->environment_valid)) {
        return backend != nullptr;
    }

    OutputEnumeration enumeration;
    enumeration.backend = backend;
    try {
        enumeration.records.reserve(backend->outputs.size() + 2U);
    } catch (...) {
        SDL_SetError("could not reserve DirectComposition output topology");
        return false;
    }
    const BOOL enumerated = EnumDisplayMonitors(nullptr, nullptr, enumerate_output,
                                                reinterpret_cast<LPARAM>(&enumeration));
    if (!enumerated || enumeration.failed || enumeration.records.empty()) {
        SDL_SetError("could not enumerate DirectComposition outputs");
        return false;
    }
    std::sort(enumeration.records.begin(), enumeration.records.end(),
              [](const Win32OutputRecord &left, const Win32OutputRecord &right) {
                  return left.info.output.value < right.info.output.value;
              });

    const bool topology_changed = !same_topology(backend->outputs, enumeration.records);
    if (topology_changed) {
        if (backend->topology_revision == UINT64_MAX) {
            SDL_SetError("DirectComposition topology revision exhausted");
            return false;
        }
        ++backend->topology_revision;
    }
    backend->outputs = std::move(enumeration.records);

    RECT host_rect = {};
    if (!GetWindowRect(backend->window, &host_rect)) {
        SDL_SetError("GetWindowRect failed: %lu", static_cast<unsigned long>(GetLastError()));
        return false;
    }

    EidolonPresentationEnvironment candidate = {};
    candidate.host = {1U};
    candidate.topology_revision = backend->topology_revision;
    candidate.host_geometry = {
        host_rect.left,
        host_rect.top,
        host_rect.right - host_rect.left,
        host_rect.bottom - host_rect.top,
    };
    candidate.coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL;
    candidate.capabilities = kCapabilities;
    candidate.valid_fields =
        EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY | EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE |
        EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY | EIDOLON_PRESENTATION_ENV_CAPABILITIES;

    const HMONITOR active_monitor = MonitorFromWindow(backend->window, MONITOR_DEFAULTTONEAREST);
    for (const Win32OutputRecord &record : backend->outputs) {
        if (record.monitor != active_monitor) {
            continue;
        }
        candidate.active_output = record.info.output;
        candidate.output_bounds = record.info.bounds;
        candidate.usable_bounds = record.info.usable_bounds;
        candidate.nominal_refresh_hz = record.info.nominal_refresh_hz;
        candidate.orientation = record.info.orientation;
        candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT |
                                  EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                                  EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS;
        if ((record.info.valid_fields & EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) != 0U) {
            candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH;
        }
        if ((record.info.valid_fields & EIDOLON_PRESENTATION_ENV_ORIENTATION) != 0U) {
            candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_ORIENTATION;
        }
        break;
    }

    const UINT dpi = GetDpiForWindow(backend->window);
    if (dpi != 0U) {
        candidate.content_scale = static_cast<float>(dpi) / 96.0F;
        candidate.pixel_scale = candidate.content_scale;
        candidate.valid_fields |=
            EIDOLON_PRESENTATION_ENV_CONTENT_SCALE | EIDOLON_PRESENTATION_ENV_PIXEL_SCALE;
    }

    const uint64_t changed = backend->environment_valid
                                 ? environment_changes(backend->environment, candidate)
                                 : candidate.valid_fields;
    if (!backend->environment_valid || changed != 0U) {
        if (backend->environment_valid && backend->environment.revision == UINT64_MAX) {
            SDL_SetError("DirectComposition environment revision exhausted");
            return false;
        }
        candidate.revision = backend->environment_valid ? backend->environment.revision + 1U : 1U;
        candidate.changed_fields = changed;
        backend->environment = candidate;
        backend->environment_valid = true;
        if (publish_event) {
            EidolonPresentationEvent event = {};
            event.kind = EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED;
            event.monotonic_ns = SDL_GetTicksNS();
            event.host = candidate.host;
            event.data.environment.environment = candidate;
            (void)eidolon_presentation_event_queue_push(&backend->event_queue, &event);
        }
    }
    backend->environment_dirty = false;
    return true;
}

bool poll_event(void *opaque, EidolonPresentationEvent *event) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    (void)reconcile_environment(backend, true);
    return eidolon_presentation_event_queue_poll(&backend->event_queue, event);
}

bool get_environment(void *opaque, EidolonPresentationEnvironment *environment) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (!reconcile_environment(backend, backend->environment_valid)) {
        return false;
    }
    *environment = backend->environment;
    return true;
}

EidolonPresentationTopologyResult copy_outputs(void *opaque, EidolonPresentationOutputInfo *outputs,
                                               size_t capacity) {
    auto *backend = static_cast<Win32DcompPresentation *>(opaque);
    if (!reconcile_environment(backend, backend->environment_valid)) {
        return {
            0U,
            0U,
            0U,
            EIDOLON_PRESENTATION_TOPOLOGY_ERROR,
        };
    }
    const size_t required = backend->outputs.size();
    const size_t copied = std::min(capacity, required);
    for (size_t index = 0U; index < copied; ++index) {
        outputs[index] = backend->outputs[index].info;
    }
    return {
        backend->topology_revision,
        required,
        copied,
        copied < required ? EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY
                          : EIDOLON_PRESENTATION_TOPOLOGY_OK,
    };
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
    if (!graphics_hresult_ok(backend, target->swap_chain->Present(0U, 0U),
                             "composition swap-chain Present")) {
        return false;
    }
    release(target->back_buffer);
    return true;
}

bool configure_layer(Win32DcompPresentation *backend,
                     const EidolonPresentationCommittedLayer &committed, uint64_t scene_revision) {
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
    target->pending_interaction = committed.scene.interaction;
    target->pending_scene_revision = scene_revision;
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
        if (!configure_layer(backend, layer, commit->revision)) {
            return false;
        }
        DcompTarget *target = find_target(backend, layer.target, layer.target_generation);
        if (!hresult_ok(backend->root->AddVisual(target->visual, previous != nullptr, previous),
                        "IDCompositionVisual::AddVisual")) {
            return false;
        }
        previous = target->visual;
    }
    if (!graphics_hresult_ok(backend, backend->composition_device->Commit(),
                             "IDCompositionDevice::Commit")) {
        return false;
    }
    backend->input_count = 0U;
    for (size_t index = 0U; index < count; ++index) {
        const EidolonPresentationCommittedLayer &layer = commit->layers[order[index]];
        DcompTarget *target = find_target(backend, layer.target, layer.target_generation);
        target->committed_matrix = target->pending_matrix;
        target->committed_offset_x = target->pending_offset_x;
        target->committed_offset_y = target->pending_offset_y;
        target->committed_interaction = target->pending_interaction;
        target->committed_scene_revision = target->pending_scene_revision;
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
    eidolon_presentation_event_queue_init(&backend->event_queue);
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
    if (!reconcile_environment(backend, false)) {
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
        poll_event,
        create_target,
        destroy_target,
        set_target_alpha_mask,
        submit_target,
        commit_scene,
        present,
        get_environment,
        copy_outputs,
    };
    EidolonPresentation *presentation =
        eidolon_presentation_create_backend("win32_dcomp", kCapabilities, backend, &operations);
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
