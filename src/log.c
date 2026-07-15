#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#define LOG_LINE_CAPACITY 2304

#ifdef _WIN32

bool eidolon_log_path(char *path, size_t capacity) {
    char local_app_data[32768];
    const DWORD local_app_data_length =
        GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (local_app_data_length == 0 || local_app_data_length >= sizeof(local_app_data)) {
        return false;
    }

    char directory[1024];
    const int directory_length =
        snprintf(directory, sizeof(directory), "%s\\Eidolon", local_app_data);
    if (directory_length <= 0 || (size_t)directory_length >= sizeof(directory)) {
        return false;
    }
    if (!CreateDirectoryA(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }

    const int path_length = snprintf(path, capacity, "%s\\eidolon.log", directory);
    return path_length > 0 && (size_t)path_length < capacity;
}

static void append_line(const char *path, const char *line, size_t length) {
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    (void)WriteFile(file, line, (DWORD)length, &written, NULL);
    CloseHandle(file);
}

static size_t write_prefix(char *line, size_t capacity, const char *component) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    const int length = snprintf(line, capacity, "%04u-%02u-%02u %02u:%02u:%02u.%03u [%lu] [%s] ",
                                time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                                time.wSecond, time.wMilliseconds, GetCurrentProcessId(), component);
    return length > 0 ? (size_t)length : 0;
}

#else

static bool ensure_directory(const char *path) {
    return mkdir(path, S_IRWXU) == 0 || errno == EEXIST;
}

bool eidolon_log_path(char *path, size_t capacity) {
    const char *state_home = getenv("XDG_STATE_HOME");
    char directory[1024];

    if (state_home != NULL && state_home[0] != '\0') {
        const int length = snprintf(directory, sizeof(directory), "%s/eidolon", state_home);
        if (length <= 0 || (size_t)length >= sizeof(directory) || !ensure_directory(state_home) ||
            !ensure_directory(directory)) {
            return false;
        }
    } else {
        const char *home = getenv("HOME");
        if (home == NULL || home[0] == '\0') {
            return false;
        }
        char local[1024];
        char state[1024];
        if (snprintf(local, sizeof(local), "%s/.local", home) <= 0 ||
            snprintf(state, sizeof(state), "%s/state", local) <= 0 ||
            snprintf(directory, sizeof(directory), "%s/eidolon", state) <= 0 ||
            !ensure_directory(local) || !ensure_directory(state) || !ensure_directory(directory)) {
            return false;
        }
    }

    const int length = snprintf(path, capacity, "%s/eidolon.log", directory);
    return length > 0 && (size_t)length < capacity;
}

static void append_line(const char *path, const char *line, size_t length) {
    const int file = open(path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (file < 0) {
        return;
    }
    (void)write(file, line, length);
    close(file);
}

static size_t write_prefix(char *line, size_t capacity, const char *component) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return 0;
    }
    struct tm local;
    if (localtime_r(&now.tv_sec, &local) == NULL) {
        return 0;
    }
    const int length = snprintf(line, capacity, "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%ld] [%s] ",
                                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                local.tm_hour, local.tm_min, local.tm_sec, now.tv_nsec / 1000000L,
                                (long)getpid(), component);
    return length > 0 ? (size_t)length : 0;
}

#endif

void eidolon_log_write(const char *component, const char *format, ...) {
    char path[1200];
    if (!eidolon_log_path(path, sizeof(path))) {
        return;
    }

    char line[LOG_LINE_CAPACITY];
    size_t length = write_prefix(line, sizeof(line), component);
    if (length == 0 || length >= sizeof(line)) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    const int message_length = vsnprintf(line + length, sizeof(line) - length, format, arguments);
    va_end(arguments);
    if (message_length < 0) {
        return;
    }

    const size_t available = sizeof(line) - length;
    length += (size_t)message_length < available ? (size_t)message_length : available - 1;
    if (length + 2 > sizeof(line)) {
        length = sizeof(line) - 2;
    }
    line[length++] = '\r';
    line[length++] = '\n';
    append_line(path, line, length);
}
