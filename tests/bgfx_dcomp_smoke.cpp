#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <sdkddkver.h>

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10
#endif

#include <bgfx/c99/bgfx.h>

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr wchar_t kWindowClass[] = L"EidolonBgfxDcompSmoke";
constexpr UINT kHostWidth = 320;
constexpr UINT kHostHeight = 240;
constexpr UINT kBufferCount = 2;

template <typename T> void release(T *&value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool texture_handle_valid(bgfx_texture_handle_t handle) { return handle.idx != UINT16_MAX; }

bool frame_buffer_handle_valid(bgfx_frame_buffer_handle_t handle) {
    return handle.idx != UINT16_MAX;
}

bool check_hresult(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) {
        return true;
    }
    std::fprintf(stderr, "bgfx dcomp: %s failed: 0x%08lx\n", operation,
                 static_cast<unsigned long>(result));
    return false;
}

LRESULT CALLBACK host_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct CompositionLayer {
    IDXGISwapChain1 *swap_chain = nullptr;
    ID3D11Texture2D *back_buffer = nullptr;
    IDCompositionVisual *visual = nullptr;
    IDCompositionEffectGroup *effect = nullptr;
    bgfx_texture_handle_t texture = BGFX_INVALID_HANDLE;
    bgfx_frame_buffer_handle_t frame_buffer = BGFX_INVALID_HANDLE;
    uint16_t width = 0;
    uint16_t height = 0;
    bgfx_view_id_t view = 0;
};

void destroy_layer_bgfx(CompositionLayer &layer) {
    if (frame_buffer_handle_valid(layer.frame_buffer)) {
        bgfx_destroy_frame_buffer(layer.frame_buffer);
        layer.frame_buffer = BGFX_INVALID_HANDLE;
    }
    if (texture_handle_valid(layer.texture)) {
        bgfx_destroy_texture(layer.texture);
        layer.texture = BGFX_INVALID_HANDLE;
    }
}

void destroy_layer_native(CompositionLayer &layer) {
    release(layer.effect);
    release(layer.visual);
    release(layer.back_buffer);
    release(layer.swap_chain);
}

bool create_layer(IDXGIFactory2 *factory, ID3D11Device *device,
                  IDCompositionDevice *composition_device, uint16_t width, uint16_t height,
                  bgfx_view_id_t view, CompositionLayer &layer) {
    DXGI_SWAP_CHAIN_DESC1 description = {};
    description.Width = width;
    description.Height = height;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    description.BufferCount = kBufferCount;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    if (!check_hresult(factory->CreateSwapChainForComposition(device, &description, nullptr,
                                                              &layer.swap_chain),
                       "CreateSwapChainForComposition")) {
        return false;
    }
    if (!check_hresult(layer.swap_chain->GetBuffer(0, IID_PPV_ARGS(&layer.back_buffer)),
                       "composition swap-chain GetBuffer(0)")) {
        return false;
    }
    if (!check_hresult(composition_device->CreateVisual(&layer.visual),
                       "IDCompositionDevice::CreateVisual")) {
        return false;
    }
    if (!check_hresult(layer.visual->SetContent(layer.swap_chain),
                       "IDCompositionVisual::SetContent")) {
        return false;
    }
    if (!check_hresult(composition_device->CreateEffectGroup(&layer.effect),
                       "IDCompositionDevice::CreateEffectGroup") ||
        !check_hresult(layer.effect->SetOpacity(1.0f), "IDCompositionEffectGroup::SetOpacity") ||
        !check_hresult(layer.visual->SetEffect(layer.effect), "IDCompositionVisual::SetEffect")) {
        return false;
    }

    const uint64_t flags =
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
    layer.texture = bgfx_create_texture_2d(
        width, height, false, 1, BGFX_TEXTURE_FORMAT_BGRA8, flags, nullptr,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(layer.back_buffer)));
    if (!texture_handle_valid(layer.texture)) {
        std::fprintf(stderr, "bgfx dcomp: bgfx rejected composition back buffer 0\n");
        return false;
    }
    (void)bgfx_frame(BGFX_FRAME_NONE);

    layer.frame_buffer = bgfx_create_frame_buffer_from_handles(1, &layer.texture, false);
    if (!frame_buffer_handle_valid(layer.frame_buffer)) {
        std::fprintf(stderr, "bgfx dcomp: framebuffer creation failed\n");
        return false;
    }
    (void)bgfx_frame(BGFX_FRAME_NONE);

    layer.width = width;
    layer.height = height;
    layer.view = view;
    return true;
}

bool verify_layer_pixel(ID3D11Device *device, const CompositionLayer &layer,
                        uint32_t premultiplied_rgba) {
    D3D11_TEXTURE2D_DESC description = {};
    layer.back_buffer->GetDesc(&description);
    if (description.Width != layer.width || description.Height != layer.height ||
        description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        std::fprintf(stderr, "bgfx dcomp: unexpected composition target description\n");
        return false;
    }

    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;

    ID3D11Texture2D *staging = nullptr;
    if (!check_hresult(device->CreateTexture2D(&description, nullptr, &staging),
                       "verification staging texture creation")) {
        return false;
    }

    ID3D11DeviceContext *context = nullptr;
    device->GetImmediateContext(&context);
    context->CopyResource(staging, layer.back_buffer);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT map_result = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (!check_hresult(map_result, "verification staging map")) {
        release(context);
        release(staging);
        return false;
    }

    const uint8_t *row = static_cast<const uint8_t *>(mapped.pData) +
                         static_cast<size_t>(layer.height / 2u) * mapped.RowPitch;
    const uint8_t *pixel = row + static_cast<size_t>(layer.width / 2u) * 4u;
    const uint8_t expected[4] = {
        static_cast<uint8_t>((premultiplied_rgba >> 8u) & 0xffu),
        static_cast<uint8_t>((premultiplied_rgba >> 16u) & 0xffu),
        static_cast<uint8_t>((premultiplied_rgba >> 24u) & 0xffu),
        static_cast<uint8_t>(premultiplied_rgba & 0xffu),
    };
    const bool matches = pixel[0] == expected[0] && pixel[1] == expected[1] &&
                         pixel[2] == expected[2] && pixel[3] == expected[3];
    if (!matches) {
        std::fprintf(stderr,
                     "bgfx dcomp: expected BGRA %02x %02x %02x %02x, got "
                     "%02x %02x %02x %02x\n",
                     expected[0], expected[1], expected[2], expected[3], pixel[0], pixel[1],
                     pixel[2], pixel[3]);
    }

    context->Unmap(staging, 0);
    release(context);
    release(staging);
    return matches;
}

void render_layer(const CompositionLayer &layer, uint32_t premultiplied_rgba) {
    /* Present unbinds D3D11 flip buffer 0; selecting the view binds it again. */
    bgfx_set_view_frame_buffer(layer.view, layer.frame_buffer);
    bgfx_set_view_rect(layer.view, 0, 0, layer.width, layer.height);
    bgfx_set_view_clear(layer.view, BGFX_CLEAR_COLOR, premultiplied_rgba, 1.0f, 0);
    bgfx_touch(layer.view);
}

void pump_messages() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void hold_visual_stage(bool show, ULONGLONG duration_ms) {
    if (!show) {
        return;
    }
    const ULONGLONG deadline = GetTickCount64() + duration_ms;
    while (GetTickCount64() < deadline) {
        pump_messages();
        Sleep(8);
    }
}

} // namespace

int main(int argc, char **argv) {
    const bool show = argc > 1 && std::strcmp(argv[1], "--show") == 0;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    bool class_registered = false;
    bool com_initialized = false;
    bool bgfx_ready = false;
    HWND window = nullptr;
    ID3D11Device *device = nullptr;
    IDXGIDevice *dxgi_device = nullptr;
    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory2 *factory = nullptr;
    IDCompositionDevice *composition_device = nullptr;
    IDCompositionTarget *target = nullptr;
    IDCompositionVisual *root = nullptr;
    CompositionLayer body;
    CompositionLayer bubble;
    uint32_t content_frames = 0;
    uint32_t body_content_revisions = 0;
    uint32_t bubble_content_revisions = 0;
    uint32_t composition_commits = 0;
    UINT body_present_count = 0;
    UINT bubble_present_count = 0;
    int exit_code = 1;
    WNDCLASSW window_class = {};
    const bgfx_internal_data_t *internal = nullptr;
    const float angle = 0.08f;
    const float scale = 1.08f;
    D2D_MATRIX_3X2_F body_transform = {};

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(com_result)) {
        com_initialized = true;
    } else if (com_result != RPC_E_CHANGED_MODE) {
        check_hresult(com_result, "CoInitializeEx");
        goto cleanup;
    }

    window_class.lpfnWndProc = host_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassW(&window_class) == 0) {
        std::fprintf(stderr, "bgfx dcomp: RegisterClassW failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        goto cleanup;
    }
    class_registered = true;

    window = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        kWindowClass, L"Eidolon bgfx DirectComposition smoke", WS_POPUP, show ? 100 : -32000,
        show ? 100 : -32000, kHostWidth, kHostHeight, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "bgfx dcomp: CreateWindowExW failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        goto cleanup;
    }
    ShowWindow(window, SW_SHOWNOACTIVATE);
    UpdateWindow(window);

    (void)bgfx_render_frame(0);
    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_DIRECT3D11;
    init.resolution.width = 0;
    init.resolution.height = 0;
    init.resolution.reset = BGFX_RESET_NONE;
    if (!bgfx_init(&init)) {
        std::fprintf(stderr, "bgfx dcomp: headless bgfx_init failed\n");
        goto cleanup;
    }
    bgfx_ready = true;

    internal = bgfx_get_internal_data();
    if (internal == nullptr || internal->context == nullptr || internal->caps == nullptr ||
        internal->caps->rendererType != BGFX_RENDERER_TYPE_DIRECT3D11 ||
        (internal->caps->supported & BGFX_CAPS_TEXTURE_EXTERNAL) == 0u) {
        std::fprintf(stderr, "bgfx dcomp: D3D11 external-texture interop unavailable\n");
        goto cleanup;
    }
    device = static_cast<ID3D11Device *>(internal->context);
    device->AddRef();

    if (!check_hresult(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)),
                       "ID3D11Device::QueryInterface(IDXGIDevice)") ||
        !check_hresult(dxgi_device->GetAdapter(&adapter), "IDXGIDevice::GetAdapter") ||
        !check_hresult(adapter->GetParent(IID_PPV_ARGS(&factory)),
                       "IDXGIAdapter::GetParent(IDXGIFactory2)") ||
        !check_hresult(DCompositionCreateDevice(dxgi_device, IID_PPV_ARGS(&composition_device)),
                       "DCompositionCreateDevice") ||
        !check_hresult(composition_device->CreateTargetForHwnd(window, TRUE, &target),
                       "IDCompositionDevice::CreateTargetForHwnd") ||
        !check_hresult(composition_device->CreateVisual(&root),
                       "IDCompositionDevice::CreateVisual(root)")) {
        goto cleanup;
    }

    if (!create_layer(factory, device, composition_device, 96, 128, 0, body) ||
        !create_layer(factory, device, composition_device, 160, 64, 1, bubble) ||
        !check_hresult(root->AddVisual(body.visual, FALSE, nullptr),
                       "IDCompositionVisual::AddVisual(body)") ||
        !check_hresult(root->AddVisual(bubble.visual, TRUE, body.visual),
                       "IDCompositionVisual::AddVisual(bubble)") ||
        !check_hresult(target->SetRoot(root), "IDCompositionTarget::SetRoot")) {
        goto cleanup;
    }

    render_layer(body, 0x40201080u);
    render_layer(bubble, 0x20408080u);
    (void)bgfx_frame(BGFX_FRAME_NONE);
    ++content_frames;
    ++body_content_revisions;
    ++bubble_content_revisions;

    if (!verify_layer_pixel(device, body, 0x40201080u) ||
        !verify_layer_pixel(device, bubble, 0x20408080u)) {
        goto cleanup;
    }

    if (!check_hresult(body.swap_chain->Present(0, 0), "body Present") ||
        !check_hresult(bubble.swap_chain->Present(0, 0), "bubble Present") ||
        !check_hresult(body.visual->SetOffsetX(28.0f), "body SetOffsetX") ||
        !check_hresult(body.visual->SetOffsetY(72.0f), "body SetOffsetY") ||
        !check_hresult(bubble.visual->SetOffsetX(132.0f), "bubble SetOffsetX") ||
        !check_hresult(bubble.visual->SetOffsetY(24.0f), "bubble SetOffsetY") ||
        !check_hresult(composition_device->Commit(), "initial DirectComposition Commit") ||
        !check_hresult(composition_device->WaitForCommitCompletion(),
                       "initial DirectComposition completion")) {
        goto cleanup;
    }
    ++composition_commits;

    hold_visual_stage(show, 500);

    /*
     * D3D11 flip-model chains expose buffer 0 as a stable logical interface whose
     * underlying identity rotates after Present. Selecting the framebuffer again
     * rebinds that logical buffer; only the bubble receives new content.
     */
    render_layer(bubble, 0x603018c0u);
    (void)bgfx_frame(BGFX_FRAME_NONE);
    ++content_frames;
    ++bubble_content_revisions;
    if (!verify_layer_pixel(device, bubble, 0x603018c0u)) {
        goto cleanup;
    }
    if (!check_hresult(bubble.swap_chain->Present(0, 0), "bubble second Present") ||
        !check_hresult(composition_device->Commit(), "bubble content Commit") ||
        !check_hresult(composition_device->WaitForCommitCompletion(),
                       "bubble content completion")) {
        goto cleanup;
    }
    ++composition_commits;
    hold_visual_stage(show, 500);

    body_transform._11 = scale * std::cos(angle);
    body_transform._12 = scale * std::sin(angle);
    body_transform._21 = -scale * std::sin(angle);
    body_transform._22 = scale * std::cos(angle);
    if (!check_hresult(body.visual->SetTransform(body_transform), "body SetTransform") ||
        !check_hresult(composition_device->Commit(), "body transform Commit")) {
        goto cleanup;
    }
    ++composition_commits;
    hold_visual_stage(show, 500);

    if (!check_hresult(bubble.visual->SetOffsetX(116.0f), "bubble independent SetOffsetX") ||
        !check_hresult(composition_device->Commit(), "bubble transform Commit")) {
        goto cleanup;
    }
    ++composition_commits;
    hold_visual_stage(show, 500);

    if (!check_hresult(bubble.effect->SetOpacity(0.35f), "bubble SetOpacity") ||
        !check_hresult(composition_device->Commit(), "bubble opacity Commit") ||
        !check_hresult(composition_device->WaitForCommitCompletion(),
                       "final DirectComposition completion") ||
        !check_hresult(body.swap_chain->GetLastPresentCount(&body_present_count),
                       "body GetLastPresentCount") ||
        !check_hresult(bubble.swap_chain->GetLastPresentCount(&bubble_present_count),
                       "bubble GetLastPresentCount")) {
        goto cleanup;
    }
    ++composition_commits;
    hold_visual_stage(show, 1200);

    if (content_frames != 2 || body_content_revisions != 1 || bubble_content_revisions != 2 ||
        composition_commits != 5 || body_present_count < 1 || bubble_present_count < 2) {
        std::fprintf(stderr, "bgfx dcomp: revision accounting failed\n");
        goto cleanup;
    }

    std::printf("bgfx dcomp: layers=2 content_frames=%u body_revisions=%u "
                "bubble_revisions=%u presents=%u/%u compositor_commits=%u "
                "content=verified bridge_copies=0 presentation_cpu_readbacks=0\n",
                content_frames, body_content_revisions, bubble_content_revisions,
                body_present_count, bubble_present_count, composition_commits);
    exit_code = 0;

cleanup:
    if (bgfx_ready) {
        const bgfx_frame_buffer_handle_t invalid = BGFX_INVALID_HANDLE;
        bgfx_set_view_frame_buffer(0, invalid);
        bgfx_set_view_frame_buffer(1, invalid);
        destroy_layer_bgfx(bubble);
        destroy_layer_bgfx(body);
        (void)bgfx_frame(BGFX_FRAME_NONE);
        (void)bgfx_frame(BGFX_FRAME_NONE);
    }

    if (target != nullptr) {
        (void)target->SetRoot(nullptr);
    }
    if (composition_device != nullptr) {
        (void)composition_device->Commit();
        (void)composition_device->WaitForCommitCompletion();
    }
    release(root);
    release(target);
    destroy_layer_native(bubble);
    destroy_layer_native(body);
    release(composition_device);
    release(factory);
    release(adapter);
    release(dxgi_device);
    release(device);

    if (bgfx_ready) {
        bgfx_shutdown();
    }
    if (window != nullptr) {
        DestroyWindow(window);
    }
    if (class_registered) {
        UnregisterClassW(kWindowClass, instance);
    }
    if (com_initialized) {
        CoUninitialize();
    }
    return exit_code;
}
