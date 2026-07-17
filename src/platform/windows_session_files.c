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

static void insert_latest(LatestFile *files, size_t capacity, const wchar_t *path,
                          ULARGE_INTEGER stamp) {
    size_t position = 0U;
    while (position < capacity && files[position].found &&
           files[position].stamp.QuadPart >= stamp.QuadPart) {
        ++position;
    }
    if (position >= capacity) {
        return;
    }
    for (size_t index = capacity - 1U; index > position; --index) {
        files[index] = files[index - 1U];
    }
    files[position].found = true;
    files[position].stamp = stamp;
    wcscpy_s(files[position].path, _countof(files[position].path), path);
}

static bool has_jsonl_suffix(const wchar_t *name) {
    const size_t length = wcslen(name);
    return length >= 6 && _wcsicmp(name + length - 6, L".jsonl") == 0;
}

static void scan_directory(const wchar_t *directory, unsigned depth, LatestFile *latest,
                           size_t capacity) {
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
                scan_directory(child, depth + 1, latest, capacity);
            }
            continue;
        }
        if (!has_jsonl_suffix(data.cFileName)) {
            continue;
        }

        ULARGE_INTEGER stamp;
        stamp.LowPart = data.ftLastWriteTime.dwLowDateTime;
        stamp.HighPart = data.ftLastWriteTime.dwHighDateTime;
        insert_latest(latest, capacity, child, stamp);
    } while (FindNextFileW(search, &data));
    FindClose(search);
}

size_t eidolon_platform_list_transcripts(EidolonTranscriptFile *files, size_t capacity) {
    if (files == NULL || capacity == 0U) {
        return 0U;
    }
    wchar_t profile[WIDE_PATH_CAPACITY];
    const DWORD profile_length =
        GetEnvironmentVariableW(L"USERPROFILE", profile, (DWORD)_countof(profile));
    if (profile_length == 0 || profile_length >= _countof(profile)) {
        return 0U;
    }

    wchar_t root[WIDE_PATH_CAPACITY];
    if (swprintf(root, _countof(root), L"%ls\\.codex\\sessions", profile) < 0) {
        return 0U;
    }

    LatestFile *latest = calloc(capacity, sizeof(*latest));
    if (latest == NULL) {
        return 0U;
    }
    scan_directory(root, 0, latest, capacity);
    size_t count = 0U;
    while (count < capacity && latest[count].found) {
        if (WideCharToMultiByte(CP_UTF8, 0, latest[count].path, -1, files[count].path,
                                (int)sizeof(files[count].path), NULL, NULL) <= 0) {
            break;
        }
        files[count].stamp = latest[count].stamp.QuadPart;
        ++count;
    }
    free(latest);
    return count;
}

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp) {
    EidolonTranscriptFile file;
    if (path == NULL || capacity == 0U || stamp == NULL ||
        eidolon_platform_list_transcripts(&file, 1U) == 0U) {
        return false;
    }
    if (strlen(file.path) + 1U > capacity) {
        return false;
    }
    memcpy(path, file.path, strlen(file.path) + 1U);
    *stamp = file.stamp;
    return true;
}

bool eidolon_platform_transcript_stamp(const char *path, uint64_t *stamp) {
    if (path == NULL || stamp == NULL) {
        return false;
    }
    wchar_t wide_path[WIDE_PATH_CAPACITY];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, (int)_countof(wide_path)) <= 0) {
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(wide_path, GetFileExInfoStandard, &data)) {
        return false;
    }
    ULARGE_INTEGER modified;
    modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
    modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
    *stamp = modified.QuadPart;
    return true;
}

bool eidolon_platform_session_index_path(char *path, size_t capacity) {
    wchar_t profile[WIDE_PATH_CAPACITY];
    wchar_t index[WIDE_PATH_CAPACITY];
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", profile, (DWORD)_countof(profile));
    if (path == NULL || capacity == 0U || length == 0U || length >= _countof(profile) ||
        swprintf(index, _countof(index), L"%ls\\.codex\\session_index.jsonl", profile) < 0) {
        return false;
    }
    return WideCharToMultiByte(CP_UTF8, 0, index, -1, path, (int)capacity, NULL, NULL) > 0;
}
