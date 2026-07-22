#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <sdkddkver.h>

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10
#endif

#ifndef EIDOLON_DCOMP_NATIVE_D3D11
#define EIDOLON_DCOMP_NATIVE_D3D11 0
#endif
#ifndef EIDOLON_DCOMP_SDL_GPU
#define EIDOLON_DCOMP_SDL_GPU 0
#endif
#ifndef EIDOLON_DCOMP_SDL_RENDERER
#define EIDOLON_DCOMP_SDL_RENDERER 0
#endif
#define EIDOLON_DCOMP_BGFX                                                                         \
    (!EIDOLON_DCOMP_NATIVE_D3D11 && !EIDOLON_DCOMP_SDL_GPU && !EIDOLON_DCOMP_SDL_RENDERER)

#if EIDOLON_DCOMP_BGFX
#include <bgfx/c99/bgfx.h>
#endif
#if EIDOLON_DCOMP_SDL_GPU || EIDOLON_DCOMP_SDL_RENDERER
#include <SDL3/SDL.h>
#endif

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

#if EIDOLON_DCOMP_NATIVE_D3D11
constexpr char kBackendName[] = "d3d11";
constexpr wchar_t kWindowClass[] = L"EidolonD3d11DcompSmoke";
constexpr wchar_t kWindowTitle[] = L"Eidolon D3D11 DirectComposition smoke";
#elif EIDOLON_DCOMP_SDL_GPU
constexpr char kBackendName[] = "sdl_gpu";
constexpr char kGpuTimingSource[] = "unavailable-public-api";
constexpr wchar_t kWindowClass[] = L"EidolonSdlGpuDcompSmoke";
constexpr wchar_t kWindowTitle[] = L"Eidolon SDL_GPU DirectComposition smoke";
#elif EIDOLON_DCOMP_SDL_RENDERER
constexpr char kBackendName[] = "sdl_renderer_d3d11";
constexpr char kGpuTimingSource[] = "d3d11-timestamp";
constexpr wchar_t kWindowClass[] = L"EidolonSdlRendererDcompSmoke";
constexpr wchar_t kWindowTitle[] = L"Eidolon SDL_Renderer DirectComposition smoke";
#else
constexpr char kBackendName[] = "bgfx";
constexpr char kGpuTimingSource[] = "d3d11-timestamp";
constexpr wchar_t kWindowClass[] = L"EidolonBgfxDcompSmoke";
constexpr wchar_t kWindowTitle[] = L"Eidolon bgfx DirectComposition smoke";
#endif
#if EIDOLON_DCOMP_NATIVE_D3D11
constexpr char kGpuTimingSource[] = "d3d11-timestamp";
#endif
constexpr UINT kHostWidth = 320;
constexpr UINT kHostHeight = 240;
constexpr UINT kBufferCount = 2;
constexpr uint32_t kMaxMetricSamples = 32;

struct MetricSeries {
    double values[kMaxMetricSamples] = {};
    uint32_t count = 0;
};

struct Instrumentation {
    MetricSeries render_submission_ms;
    MetricSeries gpu_execution_ms;
    MetricSeries frame_latency_ms;
    MetricSeries commit_latency_ms;
    MetricSeries resize_latency_ms;
    uint32_t content_frames = 0;
    uint32_t body_content_revisions = 0;
    uint32_t bubble_content_revisions = 0;
    uint32_t present_calls = 0;
    uint32_t composition_commits = 0;
    uint32_t verification_readbacks = 0;
    uint32_t bridge_copies = 0;
    uint32_t presentation_cpu_readbacks = 0;
    uint32_t resource_rebuilds = 0;
    uint32_t output_observations = 0;
    uint32_t output_transfers = 0;
    uint32_t available_outputs = 0;
    uint32_t device_checks = 0;
    uint32_t device_loss_events = 0;
    uint32_t idle_content_frames = 0;
    uint32_t idle_present_calls = 0;
    uint32_t idle_composition_commits = 0;
    double idle_wall_ms = 0.0;
    double idle_cpu_ms = 0.0;
    double idle_main_thread_cpu_ms = 0.0;
    double idle_background_thread_cpu_ms = 0.0;
    DWORD idle_background_thread_id = 0;
    wchar_t idle_background_thread_name[64] = {};
    size_t working_set_bytes = 0;
    size_t private_bytes = 0;
    size_t peak_working_set_bytes = 0;
    uint64_t executable_bytes = 0;
    HMONITOR current_monitor = nullptr;
    wchar_t current_output[CCHDEVICENAME] = {};
    UINT current_dpi = 0;
    HRESULT last_device_error = S_OK;
};

struct GpuTimerSample {
    ID3D11Query *disjoint = nullptr;
    ID3D11Query *begin = nullptr;
    ID3D11Query *end = nullptr;
};

struct GpuTimerSet {
    GpuTimerSample samples[kMaxMetricSamples] = {};
    uint32_t count = 0;
};

Instrumentation *g_instrumentation = nullptr;
bool g_allow_drag = false;
#if EIDOLON_DCOMP_SDL_GPU
SDL_GPUDevice *g_sdl_gpu_device = nullptr;
#elif EIDOLON_DCOMP_SDL_RENDERER
SDL_Renderer *g_sdl_renderer = nullptr;
#endif

int64_t performance_counter() {
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

double elapsed_ms(int64_t start, int64_t end) {
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(end - start) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

void record_metric(MetricSeries &series, double value) {
    if (series.count < kMaxMetricSamples) {
        series.values[series.count++] = value;
    }
}

double metric_average(const MetricSeries &series) {
    double total = 0.0;
    for (uint32_t i = 0; i < series.count; ++i) {
        total += series.values[i];
    }
    return series.count == 0 ? 0.0 : total / static_cast<double>(series.count);
}

double metric_percentile_95(const MetricSeries &series) {
    if (series.count == 0) {
        return 0.0;
    }
    double sorted[kMaxMetricSamples] = {};
    std::copy_n(series.values, series.count, sorted);
    std::sort(sorted, sorted + series.count);
    const uint32_t rank = (series.count * 95u + 99u) / 100u;
    return sorted[rank == 0 ? 0 : rank - 1u];
}

double metric_maximum(const MetricSeries &series) {
    if (series.count == 0) {
        return 0.0;
    }
    return *std::max_element(series.values, series.values + series.count);
}

uint64_t file_time_ticks(const FILETIME &value) {
    ULARGE_INTEGER ticks = {};
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return ticks.QuadPart;
}

bool process_cpu_ticks(uint64_t &ticks) {
    FILETIME creation = {};
    FILETIME exit = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) == FALSE) {
        std::fprintf(stderr, "%s dcomp: GetProcessTimes failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    ticks = file_time_ticks(kernel) + file_time_ticks(user);
    return true;
}

bool current_thread_cpu_ticks(uint64_t &ticks) {
    FILETIME creation = {};
    FILETIME exit = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) == FALSE) {
        std::fprintf(stderr, "%s dcomp: GetThreadTimes failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    ticks = file_time_ticks(kernel) + file_time_ticks(user);
    return true;
}

struct ThreadCpuSnapshot {
    DWORD ids[64] = {};
    uint64_t ticks[64] = {};
    uint32_t count = 0;
};

bool capture_thread_cpu(ThreadCpuSnapshot &snapshot) {
    HANDLE threads = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threads == INVALID_HANDLE_VALUE) {
        return false;
    }
    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    const DWORD process_id = GetCurrentProcessId();
    BOOL available = Thread32First(threads, &entry);
    while (available != FALSE && snapshot.count < 64) {
        if (entry.th32OwnerProcessID == process_id) {
            HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
            if (thread != nullptr) {
                FILETIME creation = {};
                FILETIME exit = {};
                FILETIME kernel = {};
                FILETIME user = {};
                if (GetThreadTimes(thread, &creation, &exit, &kernel, &user) != FALSE) {
                    snapshot.ids[snapshot.count] = entry.th32ThreadID;
                    snapshot.ticks[snapshot.count] =
                        file_time_ticks(kernel) + file_time_ticks(user);
                    ++snapshot.count;
                }
                CloseHandle(thread);
            }
        }
        available = Thread32Next(threads, &entry);
    }
    CloseHandle(threads);
    return true;
}

void observe_busiest_background_thread(const ThreadCpuSnapshot &started,
                                       const ThreadCpuSnapshot &completed,
                                       Instrumentation &instrumentation) {
    uint64_t busiest_ticks = 0;
    DWORD busiest_id = 0;
    const DWORD main_thread_id = GetCurrentThreadId();
    for (uint32_t end_index = 0; end_index < completed.count; ++end_index) {
        if (completed.ids[end_index] == main_thread_id) {
            continue;
        }
        for (uint32_t start_index = 0; start_index < started.count; ++start_index) {
            if (started.ids[start_index] == completed.ids[end_index] &&
                completed.ticks[end_index] >= started.ticks[start_index]) {
                const uint64_t delta = completed.ticks[end_index] - started.ticks[start_index];
                if (delta > busiest_ticks) {
                    busiest_ticks = delta;
                    busiest_id = completed.ids[end_index];
                }
                break;
            }
        }
    }
    instrumentation.idle_background_thread_id = busiest_id;
    instrumentation.idle_background_thread_cpu_ms = static_cast<double>(busiest_ticks) / 10000.0;
    if (busiest_id != 0) {
        HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, busiest_id);
        PWSTR description = nullptr;
        if (thread != nullptr && SUCCEEDED(GetThreadDescription(thread, &description)) &&
            description != nullptr) {
            const size_t length =
                std::min(std::wcslen(description),
                         _countof(instrumentation.idle_background_thread_name) - 1u);
            std::wmemcpy(instrumentation.idle_background_thread_name, description, length);
            instrumentation.idle_background_thread_name[length] = L'\0';
            LocalFree(description);
        }
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
}

bool observe_process_footprint(Instrumentation &instrumentation) {
    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters)) == FALSE) {
        std::fprintf(stderr, "%s dcomp: GetProcessMemoryInfo failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    instrumentation.working_set_bytes = counters.WorkingSetSize;
    instrumentation.private_bytes = counters.PrivateUsage;
    instrumentation.peak_working_set_bytes = counters.PeakWorkingSetSize;

    wchar_t executable_path[32768] = {};
    const DWORD path_length =
        GetModuleFileNameW(nullptr, executable_path, static_cast<DWORD>(_countof(executable_path)));
    WIN32_FILE_ATTRIBUTE_DATA file_data = {};
    if (path_length == 0 || path_length == _countof(executable_path) ||
        GetFileAttributesExW(executable_path, GetFileExInfoStandard, &file_data) == FALSE) {
        std::fprintf(stderr, "%s dcomp: executable size observation failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    instrumentation.executable_bytes =
        (static_cast<uint64_t>(file_data.nFileSizeHigh) << 32u) | file_data.nFileSizeLow;
    return true;
}

bool is_device_loss(HRESULT result) {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
           result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

template <typename T> void release(T *&value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool check_hresult(HRESULT result, const char *operation);

void destroy_gpu_timers(GpuTimerSet &timers) {
    for (uint32_t index = 0; index < timers.count; ++index) {
        release(timers.samples[index].end);
        release(timers.samples[index].begin);
        release(timers.samples[index].disjoint);
    }
    timers.count = 0;
}

bool create_gpu_timers(ID3D11Device *device, uint32_t count, GpuTimerSet &timers) {
#if EIDOLON_DCOMP_SDL_GPU
    (void)device;
    (void)count;
    (void)timers;
    return true;
#else
    timers.count = count;
    D3D11_QUERY_DESC description = {};
    for (uint32_t index = 0; index < count; ++index) {
        description.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (!check_hresult(device->CreateQuery(&description, &timers.samples[index].disjoint),
                           "GPU disjoint-query creation")) {
            return false;
        }
        description.Query = D3D11_QUERY_TIMESTAMP;
        if (!check_hresult(device->CreateQuery(&description, &timers.samples[index].begin),
                           "GPU begin-query creation") ||
            !check_hresult(device->CreateQuery(&description, &timers.samples[index].end),
                           "GPU end-query creation")) {
            return false;
        }
    }
    return true;
#endif
}

void begin_gpu_timer(ID3D11DeviceContext *context, const GpuTimerSample &sample) {
#if EIDOLON_DCOMP_SDL_GPU
    (void)context;
    (void)sample;
#else
    context->Begin(sample.disjoint);
    context->End(sample.begin);
#endif
}

void end_gpu_timer(ID3D11DeviceContext *context, const GpuTimerSample &sample) {
#if EIDOLON_DCOMP_SDL_GPU
    (void)context;
    (void)sample;
#else
    context->End(sample.end);
    context->End(sample.disjoint);
#endif
}

#if !EIDOLON_DCOMP_SDL_GPU
bool wait_for_query(ID3D11DeviceContext *context, ID3D11Query *query, void *data, UINT data_size) {
    const ULONGLONG deadline = GetTickCount64() + 2000;
    HRESULT result = S_FALSE;
    while (result == S_FALSE && GetTickCount64() < deadline) {
        result = context->GetData(query, data, data_size, 0);
        if (result == S_FALSE) {
            Sleep(0);
        }
    }
    return check_hresult(result == S_FALSE ? DXGI_ERROR_WAIT_TIMEOUT : result,
                         "GPU timestamp query");
}
#endif

bool resolve_gpu_timers(ID3D11Device *device, const GpuTimerSet &timers,
                        Instrumentation &instrumentation) {
#if EIDOLON_DCOMP_SDL_GPU
    (void)device;
    (void)timers;
    (void)instrumentation;
    return true;
#else
    ID3D11DeviceContext *context = nullptr;
    device->GetImmediateContext(&context);
    for (uint32_t index = 0; index < timers.count; ++index) {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
        UINT64 begin = 0;
        UINT64 end = 0;
        if (!wait_for_query(context, timers.samples[index].disjoint, &disjoint, sizeof(disjoint)) ||
            !wait_for_query(context, timers.samples[index].begin, &begin, sizeof(begin)) ||
            !wait_for_query(context, timers.samples[index].end, &end, sizeof(end))) {
            release(context);
            return false;
        }
        if (disjoint.Disjoint == FALSE && disjoint.Frequency != 0 && end >= begin) {
            record_metric(instrumentation.gpu_execution_ms,
                          static_cast<double>(end - begin) * 1000.0 /
                              static_cast<double>(disjoint.Frequency));
        }
    }
    release(context);
    return instrumentation.gpu_execution_ms.count == timers.count;
#endif
}

#if EIDOLON_DCOMP_BGFX
bool texture_handle_valid(bgfx_texture_handle_t handle) { return handle.idx != UINT16_MAX; }

bool frame_buffer_handle_valid(bgfx_frame_buffer_handle_t handle) {
    return handle.idx != UINT16_MAX;
}
#endif

bool check_hresult(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) {
        return true;
    }
    if (g_instrumentation != nullptr && is_device_loss(result)) {
        ++g_instrumentation->device_loss_events;
        g_instrumentation->last_device_error = result;
    }
    std::fprintf(stderr, "%s dcomp: %s failed: 0x%08lx\n", kBackendName, operation,
                 static_cast<unsigned long>(result));
    return false;
}

LRESULT CALLBACK host_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCHITTEST) {
        return g_allow_drag ? HTCAPTION : HTTRANSPARENT;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct CompositionLayer {
    IDXGISwapChain1 *swap_chain = nullptr;
    ID3D11Texture2D *back_buffer = nullptr;
    IDCompositionVisual *visual = nullptr;
    IDCompositionEffectGroup *effect = nullptr;
#if EIDOLON_DCOMP_BGFX
    bgfx_texture_handle_t texture = BGFX_INVALID_HANDLE;
    bgfx_frame_buffer_handle_t frame_buffer = BGFX_INVALID_HANDLE;
#elif EIDOLON_DCOMP_SDL_GPU
    SDL_GPUTexture *gpu_texture = nullptr;
    SDL_GPUTransferBuffer *transfer_buffer = nullptr;
    uint32_t transfer_pixels_per_row = 0;
#elif EIDOLON_DCOMP_SDL_RENDERER
    SDL_Texture *sdl_texture = nullptr;
#endif
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t view = 0;
    uint32_t generation = 0;
};

#if !EIDOLON_DCOMP_NATIVE_D3D11
void destroy_layer_render_resources(CompositionLayer &layer) {
#if EIDOLON_DCOMP_BGFX
    if (frame_buffer_handle_valid(layer.frame_buffer)) {
        bgfx_destroy_frame_buffer(layer.frame_buffer);
        layer.frame_buffer = BGFX_INVALID_HANDLE;
    }
    if (texture_handle_valid(layer.texture)) {
        bgfx_destroy_texture(layer.texture);
        layer.texture = BGFX_INVALID_HANDLE;
    }
#elif EIDOLON_DCOMP_SDL_GPU
    if (layer.transfer_buffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(g_sdl_gpu_device, layer.transfer_buffer);
        layer.transfer_buffer = nullptr;
    }
    if (layer.gpu_texture != nullptr) {
        SDL_ReleaseGPUTexture(g_sdl_gpu_device, layer.gpu_texture);
        layer.gpu_texture = nullptr;
    }
    layer.transfer_pixels_per_row = 0;
#elif EIDOLON_DCOMP_SDL_RENDERER
    if (layer.sdl_texture != nullptr) {
        SDL_DestroyTexture(layer.sdl_texture);
        layer.sdl_texture = nullptr;
    }
#else
    (void)layer;
#endif
}
#endif

void destroy_layer_native(CompositionLayer &layer) {
    release(layer.effect);
    release(layer.visual);
    release(layer.back_buffer);
    release(layer.swap_chain);
}

bool wrap_layer_back_buffer(uint16_t width, uint16_t height, uint16_t view,
                            CompositionLayer &layer) {
    if (!check_hresult(layer.swap_chain->GetBuffer(0, IID_PPV_ARGS(&layer.back_buffer)),
                       "composition swap-chain GetBuffer(0)")) {
        return false;
    }

#if EIDOLON_DCOMP_BGFX
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
#elif EIDOLON_DCOMP_SDL_GPU
    SDL_GPUTextureCreateInfo texture_info = {};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = width;
    texture_info.height = height;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    layer.gpu_texture = SDL_CreateGPUTexture(g_sdl_gpu_device, &texture_info);
    if (layer.gpu_texture == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: texture creation failed: %s\n", SDL_GetError());
        return false;
    }

    const uint32_t row_bytes = static_cast<uint32_t>(width) * 4u;
    const uint32_t aligned_row_bytes = (row_bytes + 255u) & ~255u;
    layer.transfer_pixels_per_row = aligned_row_bytes / 4u;
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = aligned_row_bytes * static_cast<uint32_t>(height);
    layer.transfer_buffer = SDL_CreateGPUTransferBuffer(g_sdl_gpu_device, &transfer_info);
    if (layer.transfer_buffer == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: transfer-buffer creation failed: %s\n",
                     SDL_GetError());
        return false;
    }
#elif EIDOLON_DCOMP_SDL_RENDERER
    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0 ||
        !SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                               SDL_PIXELFORMAT_BGRA32) ||
        !SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                               SDL_TEXTUREACCESS_TARGET) ||
        !SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width) ||
        !SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height) ||
        !SDL_SetPointerProperty(properties, SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER,
                                layer.back_buffer)) {
        std::fprintf(stderr, "sdl_renderer dcomp: texture properties failed: %s\n", SDL_GetError());
        if (properties != 0) {
            SDL_DestroyProperties(properties);
        }
        return false;
    }
    layer.sdl_texture = SDL_CreateTextureWithProperties(g_sdl_renderer, properties);
    SDL_DestroyProperties(properties);
    if (layer.sdl_texture == nullptr) {
        std::fprintf(stderr, "sdl_renderer dcomp: external texture failed: %s\n", SDL_GetError());
        return false;
    }
#endif

    layer.width = width;
    layer.height = height;
    layer.view = view;
    ++layer.generation;
    return true;
}

bool create_layer(IDXGIFactory2 *factory, ID3D11Device *device,
                  IDCompositionDevice *composition_device, uint16_t width, uint16_t height,
                  uint16_t view, CompositionLayer &layer) {
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

    return wrap_layer_back_buffer(width, height, view, layer);
}

bool resize_layer(uint16_t width, uint16_t height, CompositionLayer &layer,
                  Instrumentation &instrumentation) {
    const int64_t started = performance_counter();
#if EIDOLON_DCOMP_BGFX
    const bgfx_frame_buffer_handle_t invalid = BGFX_INVALID_HANDLE;
    bgfx_set_view_frame_buffer(layer.view, invalid);
    destroy_layer_render_resources(layer);
    (void)bgfx_frame(BGFX_FRAME_NONE);
    (void)bgfx_frame(BGFX_FRAME_NONE);
#elif EIDOLON_DCOMP_SDL_GPU || EIDOLON_DCOMP_SDL_RENDERER
    destroy_layer_render_resources(layer);
#endif
    release(layer.back_buffer);

    if (!check_hresult(layer.swap_chain->ResizeBuffers(kBufferCount, width, height,
                                                       DXGI_FORMAT_B8G8R8A8_UNORM, 0),
                       "composition swap-chain ResizeBuffers") ||
        !wrap_layer_back_buffer(width, height, layer.view, layer)) {
        return false;
    }

    ++instrumentation.resource_rebuilds;
    record_metric(instrumentation.resize_latency_ms, elapsed_ms(started, performance_counter()));
    return true;
}

bool verify_layer_pixel(ID3D11Device *device, const CompositionLayer &layer,
                        uint32_t premultiplied_rgba, Instrumentation &instrumentation) {
    ++instrumentation.verification_readbacks;
    D3D11_TEXTURE2D_DESC description = {};
    layer.back_buffer->GetDesc(&description);
    if (description.Width != layer.width || description.Height != layer.height ||
        description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        std::fprintf(stderr, "%s dcomp: unexpected composition target description\n", kBackendName);
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
                     "%s dcomp: expected BGRA %02x %02x %02x %02x, got "
                     "%02x %02x %02x %02x\n",
                     kBackendName, expected[0], expected[1], expected[2], expected[3], pixel[0],
                     pixel[1], pixel[2], pixel[3]);
    }

    context->Unmap(staging, 0);
    release(context);
    release(staging);
    return matches;
}

bool render_layer(ID3D11Device *device, const CompositionLayer &layer,
                  uint32_t premultiplied_rgba) {
#if EIDOLON_DCOMP_NATIVE_D3D11
    ID3D11RenderTargetView *target = nullptr;
    if (!check_hresult(device->CreateRenderTargetView(layer.back_buffer, nullptr, &target),
                       "native render-target view creation")) {
        return false;
    }
    ID3D11DeviceContext *context = nullptr;
    device->GetImmediateContext(&context);
    const float color[4] = {
        static_cast<float>((premultiplied_rgba >> 24u) & 0xffu) / 255.0f,
        static_cast<float>((premultiplied_rgba >> 16u) & 0xffu) / 255.0f,
        static_cast<float>((premultiplied_rgba >> 8u) & 0xffu) / 255.0f,
        static_cast<float>(premultiplied_rgba & 0xffu) / 255.0f,
    };
    context->ClearRenderTargetView(target, color);
    release(context);
    release(target);
#elif EIDOLON_DCOMP_SDL_GPU
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_sdl_gpu_device);
    if (command_buffer == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: command-buffer acquisition failed: %s\n",
                     SDL_GetError());
        return false;
    }

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = layer.gpu_texture;
    target_info.clear_color.r = static_cast<float>((premultiplied_rgba >> 24u) & 0xffu) / 255.0f;
    target_info.clear_color.g = static_cast<float>((premultiplied_rgba >> 16u) & 0xffu) / 255.0f;
    target_info.clear_color.b = static_cast<float>((premultiplied_rgba >> 8u) & 0xffu) / 255.0f;
    target_info.clear_color.a = static_cast<float>(premultiplied_rgba & 0xffu) / 255.0f;
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *render_pass =
        SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
    if (render_pass == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: render-pass creation failed: %s\n", SDL_GetError());
        (void)SDL_CancelGPUCommandBuffer(command_buffer);
        return false;
    }
    SDL_EndGPURenderPass(render_pass);

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (copy_pass == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: copy-pass creation failed: %s\n", SDL_GetError());
        (void)SDL_CancelGPUCommandBuffer(command_buffer);
        return false;
    }
    SDL_GPUTextureRegion source = {};
    source.texture = layer.gpu_texture;
    source.w = layer.width;
    source.h = layer.height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination = {};
    destination.transfer_buffer = layer.transfer_buffer;
    destination.pixels_per_row = layer.transfer_pixels_per_row;
    destination.rows_per_layer = layer.height;
    SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (fence == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: command submission failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GPUFence *fences[] = {fence};
    const bool completed = SDL_WaitForGPUFences(g_sdl_gpu_device, true, fences, 1);
    SDL_ReleaseGPUFence(g_sdl_gpu_device, fence);
    if (!completed) {
        std::fprintf(stderr, "sdl_gpu dcomp: fence wait failed: %s\n", SDL_GetError());
        return false;
    }

    const void *pixels = SDL_MapGPUTransferBuffer(g_sdl_gpu_device, layer.transfer_buffer, false);
    if (pixels == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: transfer-buffer mapping failed: %s\n", SDL_GetError());
        return false;
    }
    ID3D11DeviceContext *context = nullptr;
    device->GetImmediateContext(&context);
    const UINT row_pitch = layer.transfer_pixels_per_row * 4u;
    context->UpdateSubresource(layer.back_buffer, 0, nullptr, pixels, row_pitch,
                               row_pitch * static_cast<UINT>(layer.height));
    release(context);
    SDL_UnmapGPUTransferBuffer(g_sdl_gpu_device, layer.transfer_buffer);
    if (g_instrumentation != nullptr) {
        g_instrumentation->bridge_copies += 2;
        ++g_instrumentation->presentation_cpu_readbacks;
    }
#elif EIDOLON_DCOMP_SDL_RENDERER
    (void)device;
    const Uint8 red = static_cast<Uint8>((premultiplied_rgba >> 24u) & 0xffu);
    const Uint8 green = static_cast<Uint8>((premultiplied_rgba >> 16u) & 0xffu);
    const Uint8 blue = static_cast<Uint8>((premultiplied_rgba >> 8u) & 0xffu);
    const Uint8 alpha = static_cast<Uint8>(premultiplied_rgba & 0xffu);
    if (!SDL_SetRenderTarget(g_sdl_renderer, layer.sdl_texture) ||
        !SDL_SetRenderDrawColor(g_sdl_renderer, red, green, blue, alpha) ||
        !SDL_RenderClear(g_sdl_renderer) || !SDL_FlushRenderer(g_sdl_renderer) ||
        !SDL_SetRenderTarget(g_sdl_renderer, nullptr)) {
        std::fprintf(stderr, "sdl_renderer dcomp: clear failed: %s\n", SDL_GetError());
        return false;
    }
#else
    (void)device;
    /* Present unbinds D3D11 flip buffer 0; selecting the view binds it again. */
    bgfx_set_view_frame_buffer(layer.view, layer.frame_buffer);
    bgfx_set_view_rect(layer.view, 0, 0, layer.width, layer.height);
    bgfx_set_view_clear(layer.view, BGFX_CLEAR_COLOR, premultiplied_rgba, 1.0f, 0);
    bgfx_touch(layer.view);
#endif
    return true;
}

void submit_render_work() {
#if EIDOLON_DCOMP_BGFX
    (void)bgfx_frame(BGFX_FRAME_NONE);
#endif
}

void pump_messages() {
#if EIDOLON_DCOMP_SDL_GPU || EIDOLON_DCOMP_SDL_RENDERER
    SDL_PumpEvents();
#else
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
#endif
}

struct MonitorCollection {
    HMONITOR handles[16] = {};
    RECT work_areas[16] = {};
    uint32_t count = 0;
};

BOOL CALLBACK collect_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
    MonitorCollection &collection = *reinterpret_cast<MonitorCollection *>(context);
    if (collection.count >= 16) {
        return FALSE;
    }
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) != FALSE) {
        collection.handles[collection.count] = monitor;
        collection.work_areas[collection.count] = info.rcWork;
        ++collection.count;
    }
    return TRUE;
}

bool observe_output(HWND window, Instrumentation &instrumentation) {
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || GetMonitorInfoW(monitor, &info) == FALSE) {
        std::fprintf(stderr, "%s dcomp: output observation failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }

    ++instrumentation.output_observations;
    if (instrumentation.current_monitor != nullptr && instrumentation.current_monitor != monitor) {
        ++instrumentation.output_transfers;
    }
    instrumentation.current_monitor = monitor;
    const size_t output_length =
        std::min(std::wcslen(info.szDevice), CCHDEVICENAME - static_cast<size_t>(1));
    std::wmemcpy(instrumentation.current_output, info.szDevice, output_length);
    instrumentation.current_output[output_length] = L'\0';
    instrumentation.current_dpi = GetDpiForWindow(window);
    return true;
}

bool exercise_hidden_output_transfer(HWND window, bool show, Instrumentation &instrumentation) {
    MonitorCollection collection;
    if (EnumDisplayMonitors(nullptr, nullptr, collect_monitor,
                            reinterpret_cast<LPARAM>(&collection)) == FALSE) {
        std::fprintf(stderr, "%s dcomp: display enumeration failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    instrumentation.available_outputs = collection.count;
    if (!observe_output(window, instrumentation) || show || collection.count < 2) {
        return true;
    }

    RECT original = {};
    if (GetWindowRect(window, &original) == FALSE) {
        std::fprintf(stderr, "%s dcomp: GetWindowRect failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }

    uint32_t target_index = 0;
    while (target_index < collection.count &&
           collection.handles[target_index] == instrumentation.current_monitor) {
        ++target_index;
    }
    if (target_index == collection.count) {
        return true;
    }

    const RECT &target = collection.work_areas[target_index];
    if (SetWindowPos(window, nullptr, target.left + 16, target.top + 16, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER) == FALSE) {
        std::fprintf(stderr, "%s dcomp: output transfer SetWindowPos failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    pump_messages();
    if (!observe_output(window, instrumentation)) {
        return false;
    }

    if (SetWindowPos(window, nullptr, original.left, original.top, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER) == FALSE) {
        std::fprintf(stderr, "%s dcomp: output restore SetWindowPos failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    pump_messages();
    return observe_output(window, instrumentation);
}

bool observe_device(ID3D11Device *device, const char *stage, Instrumentation &instrumentation) {
    ++instrumentation.device_checks;
    const HRESULT result = device->GetDeviceRemovedReason();
    if (SUCCEEDED(result)) {
        return true;
    }
    ++instrumentation.device_loss_events;
    instrumentation.last_device_error = result;
    std::fprintf(stderr, "%s dcomp: device lost after %s: 0x%08lx\n", kBackendName, stage,
                 static_cast<unsigned long>(result));
    return false;
}

bool present_layer(CompositionLayer &layer, const char *operation,
                   Instrumentation &instrumentation) {
    if (!check_hresult(layer.swap_chain->Present(0, 0), operation)) {
        return false;
    }
    ++instrumentation.present_calls;
    return true;
}

bool commit_and_wait(IDCompositionDevice *device, const char *commit_operation,
                     const char *completion_operation, Instrumentation &instrumentation,
                     int64_t frame_started = 0) {
    const int64_t commit_started = performance_counter();
    if (!check_hresult(device->Commit(), commit_operation) ||
        !check_hresult(device->WaitForCommitCompletion(), completion_operation)) {
        return false;
    }
    const int64_t completed = performance_counter();
    ++instrumentation.composition_commits;
    record_metric(instrumentation.commit_latency_ms, elapsed_ms(commit_started, completed));
    if (frame_started != 0) {
        record_metric(instrumentation.frame_latency_ms, elapsed_ms(frame_started, completed));
    }
    return true;
}

bool measure_idle(Instrumentation &instrumentation, ULONGLONG duration_ms) {
    uint64_t cpu_started = 0;
    uint64_t cpu_completed = 0;
    uint64_t thread_started = 0;
    uint64_t thread_completed = 0;
    ThreadCpuSnapshot threads_started;
    ThreadCpuSnapshot threads_completed;
    if (!capture_thread_cpu(threads_started) || !process_cpu_ticks(cpu_started) ||
        !current_thread_cpu_ticks(thread_started)) {
        return false;
    }
    const uint32_t content_started = instrumentation.content_frames;
    const uint32_t presents_started = instrumentation.present_calls;
    const uint32_t commits_started = instrumentation.composition_commits;
    const int64_t wall_started = performance_counter();
    const ULONGLONG deadline = GetTickCount64() + duration_ms;
    while (GetTickCount64() < deadline) {
        const ULONGLONG remaining = deadline - GetTickCount64();
        const DWORD wait_ms = static_cast<DWORD>(std::min<ULONGLONG>(remaining, MAXDWORD));
        const DWORD wait_result =
            MsgWaitForMultipleObjectsEx(0, nullptr, wait_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait_result == WAIT_OBJECT_0) {
            pump_messages();
        } else if (wait_result == WAIT_TIMEOUT) {
            break;
        } else {
            std::fprintf(stderr, "%s dcomp: idle message wait failed: %lu\n", kBackendName,
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
    }
    const int64_t wall_completed = performance_counter();
    if (!process_cpu_ticks(cpu_completed) || !current_thread_cpu_ticks(thread_completed) ||
        !capture_thread_cpu(threads_completed)) {
        return false;
    }

    instrumentation.idle_wall_ms = elapsed_ms(wall_started, wall_completed);
    instrumentation.idle_cpu_ms = static_cast<double>(cpu_completed - cpu_started) / 10000.0;
    instrumentation.idle_main_thread_cpu_ms =
        static_cast<double>(thread_completed - thread_started) / 10000.0;
    observe_busiest_background_thread(threads_started, threads_completed, instrumentation);
    instrumentation.idle_content_frames = instrumentation.content_frames - content_started;
    instrumentation.idle_present_calls = instrumentation.present_calls - presents_started;
    instrumentation.idle_composition_commits =
        instrumentation.composition_commits - commits_started;
    return instrumentation.idle_content_frames == 0 && instrumentation.idle_present_calls == 0 &&
           instrumentation.idle_composition_commits == 0;
}

bool hold_visual_stage(bool show, ULONGLONG duration_ms, HWND window,
                       Instrumentation &instrumentation) {
    if (!show) {
        return true;
    }
    const ULONGLONG deadline = GetTickCount64() + duration_ms;
    while (GetTickCount64() < deadline) {
        pump_messages();
        if (MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) !=
                instrumentation.current_monitor &&
            !observe_output(window, instrumentation)) {
            return false;
        }
        Sleep(8);
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    bool show = false;
    bool benchmark = false;
    bool idle_only = false;
    for (int argument = 1; argument < argc; ++argument) {
        show = show || std::strcmp(argv[argument], "--show") == 0;
        benchmark = benchmark || std::strcmp(argv[argument], "--benchmark") == 0;
        idle_only = idle_only || std::strcmp(argv[argument], "--idle-only") == 0;
    }
    const uint16_t body_width = benchmark ? 1024 : 96;
    const uint16_t body_height = benchmark ? 1024 : 128;
    const uint16_t bubble_width = benchmark ? 768 : 160;
    const uint16_t bubble_height = benchmark ? 256 : 64;
    const uint16_t resized_bubble_width = benchmark ? 896 : 192;
    const uint16_t resized_bubble_height = benchmark ? 320 : 72;
    const uint32_t timed_sample_count = benchmark ? 16 : 8;
#if EIDOLON_DCOMP_SDL_GPU
    const uint32_t expected_bridge_copies = (timed_sample_count + 4u) * 2u;
    const uint32_t expected_presentation_readbacks = timed_sample_count + 4u;
#else
    const uint32_t expected_bridge_copies = 0;
    const uint32_t expected_presentation_readbacks = 0;
#endif
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    bool class_registered = false;
    bool com_initialized = false;
#if EIDOLON_DCOMP_BGFX
    bool bgfx_ready = false;
#elif EIDOLON_DCOMP_SDL_GPU || EIDOLON_DCOMP_SDL_RENDERER
    bool sdl_initialized = false;
#endif
#if EIDOLON_DCOMP_SDL_RENDERER
    SDL_Window *sdl_device_window = nullptr;
    SDL_PropertiesID renderer_properties = 0;
#endif
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
    Instrumentation instrumentation;
    GpuTimerSet gpu_timers;
    ID3D11DeviceContext *gpu_timer_context = nullptr;
    UINT body_present_count = 0;
    UINT bubble_present_count = 0;
    int exit_code = 1;
    WNDCLASSW window_class = {};
#if EIDOLON_DCOMP_BGFX
    const bgfx_internal_data_t *internal = nullptr;
#endif
    const float angle = 0.08f;
    const float scale = 1.08f;
    D2D_MATRIX_3X2_F body_transform = {};
    DXGI_ADAPTER_DESC adapter_description = {};
    char output_name[64] = "unknown";
    char idle_thread_name[128] = "unnamed";

    g_instrumentation = &instrumentation;
    g_allow_drag = show;

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
        std::fprintf(stderr, "%s dcomp: RegisterClassW failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        goto cleanup;
    }
    class_registered = true;

    window = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        kWindowClass, kWindowTitle, WS_POPUP, show ? 100 : -32000, show ? 100 : -32000, kHostWidth,
        kHostHeight, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "%s dcomp: CreateWindowExW failed: %lu\n", kBackendName,
                     static_cast<unsigned long>(GetLastError()));
        goto cleanup;
    }
    if (show) {
        ShowWindow(window, SW_SHOWNOACTIVATE);
        UpdateWindow(window);
    }

#if EIDOLON_DCOMP_NATIVE_D3D11 || EIDOLON_DCOMP_SDL_GPU
#if EIDOLON_DCOMP_SDL_GPU
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "sdl_gpu dcomp: SDL initialization failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = true;
    g_sdl_gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, "direct3d12");
    if (g_sdl_gpu_device == nullptr) {
        std::fprintf(stderr, "sdl_gpu dcomp: device creation failed: %s\n", SDL_GetError());
        goto cleanup;
    }
#endif
    {
        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (!check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                             nullptr, 0, D3D11_SDK_VERSION, &device, &feature_level,
                                             nullptr),
                           "D3D11CreateDevice") ||
            feature_level < D3D_FEATURE_LEVEL_11_0) {
            goto cleanup;
        }
    }
#elif EIDOLON_DCOMP_SDL_RENDERER
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "sdl_renderer dcomp: SDL initialization failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = true;
    sdl_device_window = SDL_CreateWindow("Eidolon SDL_Renderer device", 1, 1, SDL_WINDOW_HIDDEN);
    if (sdl_device_window == nullptr) {
        std::fprintf(stderr, "sdl_renderer dcomp: device window failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    renderer_properties = SDL_CreateProperties();
    if (renderer_properties == 0 ||
        !SDL_SetStringProperty(renderer_properties, SDL_PROP_RENDERER_CREATE_NAME_STRING,
                               "direct3d11") ||
        !SDL_SetPointerProperty(renderer_properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER,
                                sdl_device_window) ||
        !SDL_SetNumberProperty(renderer_properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER,
                               0)) {
        std::fprintf(stderr, "sdl_renderer dcomp: renderer properties failed: %s\n",
                     SDL_GetError());
        if (renderer_properties != 0) {
            SDL_DestroyProperties(renderer_properties);
        }
        goto cleanup;
    }
    g_sdl_renderer = SDL_CreateRendererWithProperties(renderer_properties);
    SDL_DestroyProperties(renderer_properties);
    renderer_properties = 0;
    if (g_sdl_renderer == nullptr) {
        std::fprintf(stderr, "sdl_renderer dcomp: renderer creation failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    if (!SDL_SetRenderVSync(g_sdl_renderer, 0)) {
        std::fprintf(stderr, "sdl_renderer dcomp: disabling hidden-window vsync failed: %s\n",
                     SDL_GetError());
        goto cleanup;
    }
    device = static_cast<ID3D11Device *>(
        SDL_GetPointerProperty(SDL_GetRendererProperties(g_sdl_renderer),
                               SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, nullptr));
    if (device == nullptr) {
        std::fprintf(stderr, "sdl_renderer dcomp: D3D11 device unavailable: %s\n", SDL_GetError());
        goto cleanup;
    }
    device->AddRef();
#else
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
#endif

    if (idle_only) {
        if (!measure_idle(instrumentation, 1000)) {
            goto cleanup;
        }
        std::printf("%s dcomp initialization idle: wall_ms=%.1f process_ms=%.3f "
                    "main_ms=%.3f worker=%lu/%.3f\n",
                    kBackendName, instrumentation.idle_wall_ms, instrumentation.idle_cpu_ms,
                    instrumentation.idle_main_thread_cpu_ms,
                    static_cast<unsigned long>(instrumentation.idle_background_thread_id),
                    instrumentation.idle_background_thread_cpu_ms);
        exit_code = 0;
        goto cleanup;
    }

    if (!check_hresult(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)),
                       "ID3D11Device::QueryInterface(IDXGIDevice)") ||
        !check_hresult(dxgi_device->GetAdapter(&adapter), "IDXGIDevice::GetAdapter") ||
        !check_hresult(adapter->GetDesc(&adapter_description), "IDXGIAdapter::GetDesc") ||
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

    if (!create_layer(factory, device, composition_device, body_width, body_height, 0, body) ||
        !create_layer(factory, device, composition_device, bubble_width, bubble_height, 1,
                      bubble) ||
        !check_hresult(root->AddVisual(body.visual, FALSE, nullptr),
                       "IDCompositionVisual::AddVisual(body)") ||
        !check_hresult(root->AddVisual(bubble.visual, TRUE, body.visual),
                       "IDCompositionVisual::AddVisual(bubble)") ||
        !check_hresult(target->SetRoot(root), "IDCompositionTarget::SetRoot")) {
        goto cleanup;
    }

    if (!render_layer(device, body, 0x40201080u) || !render_layer(device, bubble, 0x20408080u)) {
        goto cleanup;
    }
    submit_render_work();
    ++instrumentation.content_frames;
    ++instrumentation.body_content_revisions;
    ++instrumentation.bubble_content_revisions;

    if (!verify_layer_pixel(device, body, 0x40201080u, instrumentation) ||
        !verify_layer_pixel(device, bubble, 0x20408080u, instrumentation)) {
        goto cleanup;
    }

    if (!present_layer(body, "body Present", instrumentation) ||
        !present_layer(bubble, "bubble Present", instrumentation) ||
        !check_hresult(body.visual->SetOffsetX(28.0f), "body SetOffsetX") ||
        !check_hresult(body.visual->SetOffsetY(72.0f), "body SetOffsetY") ||
        !check_hresult(bubble.visual->SetOffsetX(132.0f), "bubble SetOffsetX") ||
        !check_hresult(bubble.visual->SetOffsetY(24.0f), "bubble SetOffsetY") ||
        !commit_and_wait(composition_device, "initial DirectComposition Commit",
                         "initial DirectComposition completion", instrumentation) ||
        !observe_device(device, "initial composition", instrumentation) ||
        !exercise_hidden_output_transfer(window, show, instrumentation)) {
        goto cleanup;
    }

    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    /*
     * D3D11 flip-model chains expose buffer 0 as a stable logical interface whose
     * underlying identity rotates after Present. Selecting the framebuffer again
     * rebinds that logical buffer; only the bubble receives new content.
     */
    if (!render_layer(device, bubble, 0x603018c0u)) {
        goto cleanup;
    }
    submit_render_work();
    ++instrumentation.content_frames;
    ++instrumentation.bubble_content_revisions;
    if (!verify_layer_pixel(device, bubble, 0x603018c0u, instrumentation)) {
        goto cleanup;
    }
    if (!present_layer(bubble, "bubble second Present", instrumentation) ||
        !commit_and_wait(composition_device, "bubble content Commit", "bubble content completion",
                         instrumentation) ||
        !observe_device(device, "verified bubble revision", instrumentation)) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    /* Measure ordinary content revisions without the test-only staging readback. */
    if (!create_gpu_timers(device, timed_sample_count, gpu_timers)) {
        goto cleanup;
    }
    device->GetImmediateContext(&gpu_timer_context);
    for (uint32_t sample = 0; sample < timed_sample_count; ++sample) {
        const int64_t frame_started = performance_counter();
        begin_gpu_timer(gpu_timer_context, gpu_timers.samples[sample]);
        if (!render_layer(device, bubble, (sample & 1u) == 0u ? 0x403010a0u : 0x304020a0u)) {
            goto cleanup;
        }
        submit_render_work();
        end_gpu_timer(gpu_timer_context, gpu_timers.samples[sample]);
        record_metric(instrumentation.render_submission_ms,
                      elapsed_ms(frame_started, performance_counter()));
        ++instrumentation.content_frames;
        ++instrumentation.bubble_content_revisions;
        if (!present_layer(bubble, "bubble timed Present", instrumentation) ||
            !commit_and_wait(composition_device, "bubble timed Commit", "bubble timed completion",
                             instrumentation, frame_started) ||
            !observe_device(device, "timed bubble revision", instrumentation)) {
            goto cleanup;
        }
    }
    release(gpu_timer_context);
    if (!resolve_gpu_timers(device, gpu_timers, instrumentation)) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    if (!resize_layer(resized_bubble_width, resized_bubble_height, bubble, instrumentation)) {
        goto cleanup;
    }
    if (!render_layer(device, bubble, 0x20406080u)) {
        goto cleanup;
    }
    submit_render_work();
    ++instrumentation.content_frames;
    ++instrumentation.bubble_content_revisions;
    if (!verify_layer_pixel(device, bubble, 0x20406080u, instrumentation) ||
        !present_layer(bubble, "resized bubble Present", instrumentation) ||
        !commit_and_wait(composition_device, "resized bubble Commit", "resized bubble completion",
                         instrumentation) ||
        !observe_device(device, "resized bubble revision", instrumentation)) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    body_transform._11 = scale * std::cos(angle);
    body_transform._12 = scale * std::sin(angle);
    body_transform._21 = -scale * std::sin(angle);
    body_transform._22 = scale * std::cos(angle);
    if (!check_hresult(body.visual->SetTransform(body_transform), "body SetTransform") ||
        !commit_and_wait(composition_device, "body transform Commit", "body transform completion",
                         instrumentation)) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    if (!check_hresult(bubble.visual->SetOffsetX(116.0f), "bubble independent SetOffsetX") ||
        !commit_and_wait(composition_device, "bubble transform Commit",
                         "bubble transform completion", instrumentation)) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 500, window, instrumentation)) {
        goto cleanup;
    }

    if (!check_hresult(bubble.effect->SetOpacity(0.35f), "bubble SetOpacity") ||
        !commit_and_wait(composition_device, "bubble opacity Commit",
                         "final DirectComposition completion", instrumentation) ||
        !observe_device(device, "final composition", instrumentation) ||
        !observe_output(window, instrumentation) ||
        !measure_idle(instrumentation, benchmark ? 1000 : 250) ||
        !observe_process_footprint(instrumentation) ||
        !check_hresult(body.swap_chain->GetLastPresentCount(&body_present_count),
                       "body GetLastPresentCount") ||
        !check_hresult(bubble.swap_chain->GetLastPresentCount(&bubble_present_count),
                       "bubble GetLastPresentCount")) {
        goto cleanup;
    }
    if (!hold_visual_stage(show, 6000, window, instrumentation)) {
        goto cleanup;
    }

    if (instrumentation.content_frames != timed_sample_count + 3u ||
        instrumentation.body_content_revisions != 1 ||
        instrumentation.bubble_content_revisions != timed_sample_count + 3u ||
        instrumentation.present_calls != timed_sample_count + 4u ||
        instrumentation.composition_commits != timed_sample_count + 6u ||
        instrumentation.verification_readbacks != 4 || instrumentation.resource_rebuilds != 1 ||
        body.generation != 1 || bubble.generation != 2 ||
        instrumentation.render_submission_ms.count != timed_sample_count ||
#if EIDOLON_DCOMP_SDL_GPU
        instrumentation.gpu_execution_ms.count != 0 ||
#else
        instrumentation.gpu_execution_ms.count != timed_sample_count ||
#endif
        instrumentation.frame_latency_ms.count != timed_sample_count ||
        instrumentation.commit_latency_ms.count != timed_sample_count + 6u ||
        instrumentation.available_outputs == 0 || instrumentation.output_observations < 2 ||
        (!show && instrumentation.available_outputs > 1 && instrumentation.output_transfers < 2) ||
        instrumentation.device_checks < timed_sample_count + 4u ||
        instrumentation.device_loss_events != 0 ||
        instrumentation.bridge_copies != expected_bridge_copies ||
        instrumentation.presentation_cpu_readbacks != expected_presentation_readbacks ||
        instrumentation.idle_content_frames != 0 || instrumentation.idle_present_calls != 0 ||
        instrumentation.idle_composition_commits != 0 || body_present_count < 1 ||
        bubble_present_count < timed_sample_count + 3u) {
        std::fprintf(stderr, "%s dcomp: instrumentation contract failed\n", kBackendName);
        goto cleanup;
    }

    (void)WideCharToMultiByte(CP_UTF8, 0, instrumentation.current_output, -1, output_name,
                              static_cast<int>(sizeof(output_name)), nullptr, nullptr);
    if (instrumentation.idle_background_thread_name[0] != L'\0') {
        (void)WideCharToMultiByte(CP_UTF8, 0, instrumentation.idle_background_thread_name, -1,
                                  idle_thread_name, static_cast<int>(sizeof(idle_thread_name)),
                                  nullptr, nullptr);
    }
    std::printf("%s dcomp: layers=2 content_frames=%u body_revisions=%u "
                "bubble_revisions=%u presents=%u/%u compositor_commits=%u "
                "content=verified bridge_copies=%u presentation_cpu_readbacks=%u "
                "verification_readbacks=%u\n",
                kBackendName, instrumentation.content_frames,
                instrumentation.body_content_revisions, instrumentation.bubble_content_revisions,
                body_present_count, bubble_present_count, instrumentation.composition_commits,
                instrumentation.bridge_copies, instrumentation.presentation_cpu_readbacks,
                instrumentation.verification_readbacks);
    std::printf("%s dcomp metrics: submit_ms=%.3f/%.3f/%.3f "
                "gpu_ms=%.3f/%.3f/%.3f gpu_source=%s "
                "frame_ms=%.3f/%.3f/%.3f "
                "commit_ms=%.3f/%.3f/%.3f resize_ms=%.3f "
                "idle_ms=%.1f idle_cpu_ms=%.3f idle_main_ms=%.3f "
                "idle_worker=%lu/%.3f/%s idle_work=%u/%u/%u\n",
                kBackendName, metric_average(instrumentation.render_submission_ms),
                metric_percentile_95(instrumentation.render_submission_ms),
                metric_maximum(instrumentation.render_submission_ms),
                metric_average(instrumentation.gpu_execution_ms),
                metric_percentile_95(instrumentation.gpu_execution_ms),
                metric_maximum(instrumentation.gpu_execution_ms), kGpuTimingSource,
                metric_average(instrumentation.frame_latency_ms),
                metric_percentile_95(instrumentation.frame_latency_ms),
                metric_maximum(instrumentation.frame_latency_ms),
                metric_average(instrumentation.commit_latency_ms),
                metric_percentile_95(instrumentation.commit_latency_ms),
                metric_maximum(instrumentation.commit_latency_ms),
                metric_average(instrumentation.resize_latency_ms), instrumentation.idle_wall_ms,
                instrumentation.idle_cpu_ms, instrumentation.idle_main_thread_cpu_ms,
                static_cast<unsigned long>(instrumentation.idle_background_thread_id),
                instrumentation.idle_background_thread_cpu_ms, idle_thread_name,
                instrumentation.idle_content_frames, instrumentation.idle_present_calls,
                instrumentation.idle_composition_commits);
    std::printf("%s dcomp platform: generations=%u/%u rebuilds=%u "
                "outputs=%u observations=%u transfers=%u current=%s dpi=%u "
                "adapter_luid=%08lx:%08lx device_checks=%u device_losses=%u\n",
                kBackendName, body.generation, bubble.generation, instrumentation.resource_rebuilds,
                instrumentation.available_outputs, instrumentation.output_observations,
                instrumentation.output_transfers, output_name, instrumentation.current_dpi,
                static_cast<unsigned long>(adapter_description.AdapterLuid.HighPart),
                static_cast<unsigned long>(adapter_description.AdapterLuid.LowPart),
                instrumentation.device_checks, instrumentation.device_loss_events);
    std::printf("%s dcomp footprint: profile=%s surfaces=%ux%u/%ux%u executable_kib=%.1f "
                "working_set_mib=%.2f private_mib=%.2f peak_working_set_mib=%.2f\n",
                kBackendName, benchmark ? "eidolon" : "smoke", body_width, body_height,
                resized_bubble_width, resized_bubble_height,
                static_cast<double>(instrumentation.executable_bytes) / 1024.0,
                static_cast<double>(instrumentation.working_set_bytes) / (1024.0 * 1024.0),
                static_cast<double>(instrumentation.private_bytes) / (1024.0 * 1024.0),
                static_cast<double>(instrumentation.peak_working_set_bytes) / (1024.0 * 1024.0));
    exit_code = 0;

cleanup:
    release(gpu_timer_context);
    destroy_gpu_timers(gpu_timers);
#if EIDOLON_DCOMP_BGFX
    if (bgfx_ready) {
        const bgfx_frame_buffer_handle_t invalid = BGFX_INVALID_HANDLE;
        bgfx_set_view_frame_buffer(0, invalid);
        bgfx_set_view_frame_buffer(1, invalid);
        destroy_layer_render_resources(bubble);
        destroy_layer_render_resources(body);
        (void)bgfx_frame(BGFX_FRAME_NONE);
        (void)bgfx_frame(BGFX_FRAME_NONE);
    }
#elif EIDOLON_DCOMP_SDL_GPU || EIDOLON_DCOMP_SDL_RENDERER
    destroy_layer_render_resources(bubble);
    destroy_layer_render_resources(body);
#endif

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

#if EIDOLON_DCOMP_BGFX
    if (bgfx_ready) {
        bgfx_shutdown();
    }
#elif EIDOLON_DCOMP_SDL_GPU
    if (g_sdl_gpu_device != nullptr) {
        SDL_DestroyGPUDevice(g_sdl_gpu_device);
        g_sdl_gpu_device = nullptr;
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
#elif EIDOLON_DCOMP_SDL_RENDERER
    if (renderer_properties != 0) {
        SDL_DestroyProperties(renderer_properties);
    }
    if (g_sdl_renderer != nullptr) {
        SDL_DestroyRenderer(g_sdl_renderer);
        g_sdl_renderer = nullptr;
    }
    if (sdl_device_window != nullptr) {
        SDL_DestroyWindow(sdl_device_window);
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
#endif
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
