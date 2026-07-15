#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/session_files.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define WIDE_PATH_CAPACITY 32768

typedef struct LatestFile {
    wchar_t path[WIDE_PATH_CAPACITY];
    ULARGE_INTEGER stamp;
    bool found;
} LatestFile;

static bool has_jsonl_suffix(const wchar_t *name) {
    const size_t length = wcslen(name);
    return length >= 6 && _wcsicmp(name + length - 6, L".jsonl") == 0;
}

static void scan_directory(const wchar_t *directory, unsigned depth, LatestFile *latest) {
    if (depth > 8) {
        return;
    }

    wchar_t pattern[WIDE_PATH_CAPACITY];
    if (swprintf(pattern, _countof(pattern), L"%ls\\*", directory) < 0) {
        return;
    }

    WIN32_FIND_DATAW data;
    HANDLE search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        wchar_t child[WIDE_PATH_CAPACITY];
        if (swprintf(child, _countof(child), L"%ls\\%ls", directory, data.cFileName) < 0) {
            continue;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                scan_directory(child, depth + 1, latest);
            }
            continue;
        }
        if (!has_jsonl_suffix(data.cFileName)) {
            continue;
        }

        ULARGE_INTEGER stamp;
        stamp.LowPart = data.ftLastWriteTime.dwLowDateTime;
        stamp.HighPart = data.ftLastWriteTime.dwHighDateTime;
        if (!latest->found || stamp.QuadPart > latest->stamp.QuadPart) {
            latest->stamp = stamp;
            latest->found = true;
            wcscpy_s(latest->path, _countof(latest->path), child);
        }
    } while (FindNextFileW(search, &data));
    FindClose(search);
}

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp) {
    if (path == NULL || capacity == 0 || stamp == NULL) {
        return false;
    }
    wchar_t profile[WIDE_PATH_CAPACITY];
    const DWORD profile_length =
        GetEnvironmentVariableW(L"USERPROFILE", profile, (DWORD)_countof(profile));
    if (profile_length == 0 || profile_length >= _countof(profile)) {
        return false;
    }

    wchar_t root[WIDE_PATH_CAPACITY];
    if (swprintf(root, _countof(root), L"%ls\\.codex\\sessions", profile) < 0) {
        return false;
    }

    LatestFile latest = {0};
    SYSTEMTIME today;
    GetLocalTime(&today);
    FILETIME today_file_time;
    if (!SystemTimeToFileTime(&today, &today_file_time)) {
        return false;
    }
    ULARGE_INTEGER day;
    day.LowPart = today_file_time.dwLowDateTime;
    day.HighPart = today_file_time.dwHighDateTime;

    for (unsigned offset = 0; offset < 2; ++offset) {
        ULARGE_INTEGER date_value = day;
        date_value.QuadPart -= (uint64_t)offset * 24U * 60U * 60U * 10000000U;
        FILETIME date_file_time;
        date_file_time.dwLowDateTime = date_value.LowPart;
        date_file_time.dwHighDateTime = date_value.HighPart;
        SYSTEMTIME date;
        if (!FileTimeToSystemTime(&date_file_time, &date)) {
            continue;
        }
        wchar_t directory[WIDE_PATH_CAPACITY];
        if (swprintf(directory, _countof(directory), L"%ls\\%04u\\%02u\\%02u", root,
                     (unsigned)date.wYear, (unsigned)date.wMonth, (unsigned)date.wDay) >= 0) {
            scan_directory(directory, 0, &latest);
        }
    }
    if (!latest.found) {
        return false;
    }

    const int written = WideCharToMultiByte(CP_UTF8, 0, latest.path, -1, path, (int)capacity, NULL,
                                            NULL);
    if (written <= 0) {
        return false;
    }
    *stamp = latest.stamp.QuadPart;
    return true;
}
