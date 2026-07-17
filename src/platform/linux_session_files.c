#include "platform/session_files.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define SESSION_PATH_CAPACITY 4096

typedef struct LatestFile {
    char path[SESSION_PATH_CAPACITY];
    uint64_t stamp;
    bool found;
} LatestFile;

static bool has_jsonl_suffix(const char *name) {
    const size_t length = strlen(name);
    return length >= 6 && strcmp(name + length - 6, ".jsonl") == 0;
}

static void insert_latest(LatestFile *files, size_t capacity, const char *path, uint64_t stamp) {
    size_t position = 0U;
    while (position < capacity && files[position].found && files[position].stamp >= stamp) {
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
    snprintf(files[position].path, sizeof(files[position].path), "%s", path);
}

static void scan_directory(const char *directory, unsigned depth, LatestFile *latest,
                           size_t capacity) {
    if (depth > 8) {
        return;
    }
    DIR *entries = opendir(directory);
    if (entries == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(entries)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[SESSION_PATH_CAPACITY];
        const int length = snprintf(child, sizeof(child), "%s/%s", directory, entry->d_name);
        if (length <= 0 || (size_t)length >= sizeof(child)) {
            continue;
        }

        struct stat info;
        if (lstat(child, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            scan_directory(child, depth + 1, latest, capacity);
        } else if (S_ISREG(info.st_mode) && has_jsonl_suffix(entry->d_name)) {
            const uint64_t stamp =
                (uint64_t)info.st_mtim.tv_sec * 1000000000U + (uint64_t)info.st_mtim.tv_nsec;
            insert_latest(latest, capacity, child, stamp);
        }
    }
    closedir(entries);
}

size_t eidolon_platform_list_transcripts(EidolonTranscriptFile *files, size_t capacity) {
    if (files == NULL || capacity == 0U) {
        return 0U;
    }
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return 0U;
    }

    char root[SESSION_PATH_CAPACITY];
    const int length = snprintf(root, sizeof(root), "%s/.codex/sessions", home);
    if (length <= 0 || (size_t)length >= sizeof(root)) {
        return 0U;
    }

    LatestFile *latest = calloc(capacity, sizeof(*latest));
    if (latest == NULL) {
        return 0U;
    }
    scan_directory(root, 0, latest, capacity);
    size_t count = 0U;
    while (count < capacity && latest[count].found) {
        memcpy(files[count].path, latest[count].path, strlen(latest[count].path) + 1U);
        files[count].stamp = latest[count].stamp;
        ++count;
    }
    free(latest);
    return count;
}

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp) {
    EidolonTranscriptFile file;
    if (path == NULL || capacity == 0U || stamp == NULL ||
        eidolon_platform_list_transcripts(&file, 1U) == 0U || strlen(file.path) + 1U > capacity) {
        return false;
    }
    memcpy(path, file.path, strlen(file.path) + 1U);
    *stamp = file.stamp;
    return true;
}

bool eidolon_platform_transcript_stamp(const char *path, uint64_t *stamp) {
    struct stat info;
    if (path == NULL || stamp == NULL || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return false;
    }
    *stamp = (uint64_t)info.st_mtim.tv_sec * UINT64_C(1000000000) + (uint64_t)info.st_mtim.tv_nsec;
    return true;
}

bool eidolon_platform_session_index_path(char *path, size_t capacity) {
    const char *home = getenv("HOME");
    if (path == NULL || capacity == 0U || home == NULL || home[0] == '\0') {
        return false;
    }
    const int length = snprintf(path, capacity, "%s/.codex/session_index.jsonl", home);
    return length > 0 && (size_t)length < capacity;
}
