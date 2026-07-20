#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include "affect.h"
#include "affect_protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct WorkerProcess {
    HANDLE process;
    HANDLE thread;
    HANDLE input;
    HANDLE output;
} WorkerProcess;

typedef struct Sample {
    double milliseconds;
    size_t top_emotion;
    float top_probability;
    size_t second_emotion;
    float second_probability;
    EidolonExpressionIntent expression;
    float evidence;
} Sample;

static double counter_milliseconds(LARGE_INTEGER start, LARGE_INTEGER end,
                                   LARGE_INTEGER frequency) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)frequency.QuadPart;
}

static double filetime_milliseconds(FILETIME value) {
    ULARGE_INTEGER ticks;
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return (double)ticks.QuadPart / 10000.0;
}

static bool write_exact(HANDLE handle, const void *data, size_t length) {
    const uint8_t *cursor = data;
    while (length > 0U) {
        const DWORD chunk = length > (size_t)UINT32_MAX ? UINT32_MAX : (DWORD)length;
        DWORD written = 0U;
        if (!WriteFile(handle, cursor, chunk, &written, NULL) || written == 0U) {
            return false;
        }
        cursor += written;
        length -= written;
    }
    return true;
}

static bool read_exact(HANDLE handle, void *data, size_t length) {
    uint8_t *cursor = data;
    while (length > 0U) {
        const DWORD chunk = length > (size_t)UINT32_MAX ? UINT32_MAX : (DWORD)length;
        DWORD received = 0U;
        if (!ReadFile(handle, cursor, chunk, &received, NULL) || received == 0U) {
            return false;
        }
        cursor += received;
        length -= received;
    }
    return true;
}

static bool utf8_to_wide(const char *text, wchar_t *wide, size_t capacity) {
    if (capacity > (size_t)INT_MAX) {
        return false;
    }
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, (int)capacity) > 0;
}

static bool worker_start(WorkerProcess *worker, const char *path) {
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE child_input = NULL;
    HANDLE child_output = NULL;
    memset(worker, 0, sizeof(*worker));

    if (!CreatePipe(&child_input, &worker->input, &security, 0U) ||
        !SetHandleInformation(worker->input, HANDLE_FLAG_INHERIT, 0U) ||
        !CreatePipe(&worker->output, &child_output, &security, 0U) ||
        !SetHandleInformation(worker->output, HANDLE_FLAG_INHERIT, 0U)) {
        goto fail;
    }

    wchar_t wide_path[MAX_PATH];
    wchar_t command[MAX_PATH + 3U];
    if (!utf8_to_wide(path, wide_path, _countof(wide_path)) ||
        swprintf(command, _countof(command), L"\"%ls\"", wide_path) < 0) {
        goto fail;
    }

    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessW(wide_path, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
                        &startup, &process)) {
        goto fail;
    }

    CloseHandle(child_input);
    CloseHandle(child_output);
    worker->process = process.hProcess;
    worker->thread = process.hThread;
    return true;

fail:
    if (child_input != NULL) {
        CloseHandle(child_input);
    }
    if (child_output != NULL) {
        CloseHandle(child_output);
    }
    if (worker->input != NULL) {
        CloseHandle(worker->input);
    }
    if (worker->output != NULL) {
        CloseHandle(worker->output);
    }
    memset(worker, 0, sizeof(*worker));
    return false;
}

static void worker_stop(WorkerProcess *worker) {
    if (worker->input != NULL) {
        CloseHandle(worker->input);
        worker->input = NULL;
    }
    if (worker->process != NULL && WaitForSingleObject(worker->process, 5000U) == WAIT_TIMEOUT) {
        TerminateProcess(worker->process, 1U);
        WaitForSingleObject(worker->process, 1000U);
    }
    if (worker->output != NULL) {
        CloseHandle(worker->output);
    }
    if (worker->thread != NULL) {
        CloseHandle(worker->thread);
    }
    if (worker->process != NULL) {
        CloseHandle(worker->process);
    }
    memset(worker, 0, sizeof(*worker));
}

static bool infer(WorkerProcess *worker, uint64_t sequence, const char *text,
                  EidolonAffectResponse *response, double *milliseconds,
                  LARGE_INTEGER frequency) {
    const size_t length = strlen(text);
    if (length > EIDOLON_AFFECT_TEXT_CAPACITY) {
        return false;
    }
    const EidolonAffectRequestHeader request = {
        EIDOLON_AFFECT_PROTOCOL_MAGIC,
        EIDOLON_AFFECT_PROTOCOL_VERSION,
        sequence,
        (uint32_t)length,
    };
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    QueryPerformanceCounter(&start);
    if (!write_exact(worker->input, &request, sizeof(request)) ||
        !write_exact(worker->input, text, length) ||
        !read_exact(worker->output, response, sizeof(*response))) {
        return false;
    }
    QueryPerformanceCounter(&end);
    *milliseconds = counter_milliseconds(start, end, frequency);
    return response->magic == EIDOLON_AFFECT_PROTOCOL_MAGIC &&
           response->version == EIDOLON_AFFECT_PROTOCOL_VERSION &&
           response->sequence == sequence && response->status == 0U;
}

static Sample summarize(const EidolonAffectResponse *response, double milliseconds) {
    Sample sample = {.milliseconds = milliseconds, .top_emotion = 0U, .second_emotion = 1U};
    if (response->probabilities[1] > response->probabilities[0]) {
        sample.top_emotion = 1U;
        sample.second_emotion = 0U;
    }
    for (size_t index = 2U; index < EIDOLON_GOEMOTIONS_COUNT; ++index) {
        if (response->probabilities[index] > response->probabilities[sample.top_emotion]) {
            sample.second_emotion = sample.top_emotion;
            sample.top_emotion = index;
        } else if (response->probabilities[index] >
                   response->probabilities[sample.second_emotion]) {
            sample.second_emotion = index;
        }
    }
    sample.top_probability = response->probabilities[sample.top_emotion];
    sample.second_probability = response->probabilities[sample.second_emotion];
    const EidolonAffect affect = eidolon_affect_from_goemotions(
        response->probabilities, eidolon_affect_for_state(EIDOLON_STATE_RUNNING),
        &sample.evidence);
    sample.expression = eidolon_affect_expression(&affect);
    return sample;
}

static int compare_doubles(const void *left, const void *right) {
    const double a = *(const double *)left;
    const double b = *(const double *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static double percentile(const double *values, size_t count, double fraction) {
    double sorted[16];
    if (count == 0U || count > _countof(sorted)) {
        return 0.0;
    }
    memcpy(sorted, values, count * sizeof(*values));
    qsort(sorted, count, sizeof(*sorted), compare_doubles);
    const size_t index = (size_t)((double)(count - 1U) * fraction + 0.5);
    return sorted[index];
}

int main(int argc, char **argv) {
    static const char *const lines[] = {
        "you changed the default renderer without breaking inheritance?",
        "\xE2\x80\xA6suspicious. that was far too easy.",
        "oh, shit\xE2\x80\x94the user config disappeared. did reset delete the whole\xE2\x80\x94",
        "no. version remains, override removed, system default restored. exactly right.",
        "ugh. fine. you win. this is clean, elegant, and disgustingly satisfying.",
        "now stop looking so smug before i climb into your lap and give you a much better reason. \xE2\x9D\xA4\xEF\xB8\x8F",
    };
    if (argc != 2) {
        fprintf(stderr, "usage: affect_benchmark WORKER_PATH\n");
        return 2;
    }

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    WorkerProcess worker;
    LARGE_INTEGER spawn_start;
    QueryPerformanceCounter(&spawn_start);
    if (!worker_start(&worker, argv[1])) {
        fprintf(stderr, "could not start worker: Windows error %lu\n", GetLastError());
        return 1;
    }

    EidolonAffectResponse response;
    double warmup_roundtrip_ms = 0.0;
    if (!infer(&worker, 1U, "warmup", &response, &warmup_roundtrip_ms, frequency)) {
        fprintf(stderr, "worker warmup failed\n");
        worker_stop(&worker);
        return 1;
    }
    LARGE_INTEGER first_response;
    QueryPerformanceCounter(&first_response);
    const double cold_ms = counter_milliseconds(spawn_start, first_response, frequency);

    FILETIME created;
    FILETIME exited;
    FILETIME kernel_before;
    FILETIME user_before;
    PROCESS_MEMORY_COUNTERS_EX memory_before;
    memory_before.cb = sizeof(memory_before);
    if (!GetProcessTimes(worker.process, &created, &exited, &kernel_before, &user_before) ||
        !GetProcessMemoryInfo(worker.process, (PROCESS_MEMORY_COUNTERS *)&memory_before,
                              sizeof(memory_before))) {
        fprintf(stderr, "could not sample worker resources\n");
        worker_stop(&worker);
        return 1;
    }

    Sample samples[_countof(lines)];
    double latencies[_countof(lines)];
    LARGE_INTEGER batch_start;
    LARGE_INTEGER batch_end;
    QueryPerformanceCounter(&batch_start);
    for (size_t index = 0U; index < _countof(lines); ++index) {
        double elapsed = 0.0;
        if (!infer(&worker, index + 2U, lines[index], &response, &elapsed, frequency)) {
            fprintf(stderr, "line %zu inference failed\n", index + 1U);
            worker_stop(&worker);
            return 1;
        }
        latencies[index] = elapsed;
        samples[index] = summarize(&response, elapsed);
    }
    QueryPerformanceCounter(&batch_end);

    FILETIME kernel_after;
    FILETIME user_after;
    PROCESS_MEMORY_COUNTERS_EX memory_after;
    memory_after.cb = sizeof(memory_after);
    GetProcessTimes(worker.process, &created, &exited, &kernel_after, &user_after);
    GetProcessMemoryInfo(worker.process, (PROCESS_MEMORY_COUNTERS *)&memory_after,
                         sizeof(memory_after));

    const double batch_ms = counter_milliseconds(batch_start, batch_end, frequency);
    const double cpu_ms = filetime_milliseconds(kernel_after) + filetime_milliseconds(user_after) -
                          filetime_milliseconds(kernel_before) - filetime_milliseconds(user_before);
    printf("persistent GoEmotions worker benchmark\n");
    printf("cold start + warmup: %.2f ms (%.2f ms request roundtrip)\n", cold_ms,
           warmup_roundtrip_ms);
    printf("six-line batch:      %.2f ms wall, %.2f ms worker CPU\n", batch_ms, cpu_ms);
    printf("latency:             %.2f ms mean, %.2f ms p50, %.2f ms p95\n\n",
           batch_ms / (double)_countof(lines), percentile(latencies, _countof(lines), 0.50),
           percentile(latencies, _countof(lines), 0.95));

    for (size_t index = 0U; index < _countof(lines); ++index) {
        const Sample *sample = &samples[index];
        printf("%zu  %6.2f ms  %-13s %5.1f%%  %-13s %5.1f%%  => %s\n", index + 1U,
               sample->milliseconds, eidolon_goemotion_name(sample->top_emotion),
               (double)sample->top_probability * 100.0,
               eidolon_goemotion_name(sample->second_emotion),
               (double)sample->second_probability * 100.0,
               eidolon_expression_intent_name(sample->expression));
    }

    printf("\nworker memory after warmup: %.1f MiB working set, %.1f MiB private\n",
           (double)memory_before.WorkingSetSize / (1024.0 * 1024.0),
           (double)memory_before.PrivateUsage / (1024.0 * 1024.0));
    printf("worker memory after batch:  %.1f MiB working set, %.1f MiB private\n",
           (double)memory_after.WorkingSetSize / (1024.0 * 1024.0),
           (double)memory_after.PrivateUsage / (1024.0 * 1024.0));
    printf("worker peak working set:    %.1f MiB\n",
           (double)memory_after.PeakWorkingSetSize / (1024.0 * 1024.0));

    worker_stop(&worker);
    return 0;
}
