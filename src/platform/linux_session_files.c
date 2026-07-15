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

static void scan_directory(const char *directory, unsigned depth, LatestFile *latest) {
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
            scan_directory(child, depth + 1, latest);
        } else if (S_ISREG(info.st_mode) && has_jsonl_suffix(entry->d_name)) {
            const uint64_t stamp = (uint64_t)info.st_mtim.tv_sec * 1000000000U +
                                   (uint64_t)info.st_mtim.tv_nsec;
            if (!latest->found || stamp > latest->stamp) {
                latest->found = true;
                latest->stamp = stamp;
                snprintf(latest->path, sizeof(latest->path), "%s", child);
            }
        }
    }
    closedir(entries);
}

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp) {
    if (path == NULL || capacity == 0 || stamp == NULL) {
        return false;
    }
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return false;
    }

    char root[SESSION_PATH_CAPACITY];
    const int length = snprintf(root, sizeof(root), "%s/.codex/sessions", home);
    if (length <= 0 || (size_t)length >= sizeof(root)) {
        return false;
    }

    LatestFile latest = {0};
    const time_t now = time(NULL);
    for (unsigned offset = 0; offset < 2; ++offset) {
        const time_t day = now - (time_t)offset * 24 * 60 * 60;
        struct tm date;
        if (localtime_r(&day, &date) == NULL) {
            continue;
        }
        char directory[SESSION_PATH_CAPACITY];
        const int directory_length =
            snprintf(directory, sizeof(directory), "%s/%04d/%02d/%02d", root,
                     date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
        if (directory_length > 0 && (size_t)directory_length < sizeof(directory)) {
            scan_directory(directory, 0, &latest);
        }
    }
    if (!latest.found || strlen(latest.path) + 1 > capacity) {
        return false;
    }
    memcpy(path, latest.path, strlen(latest.path) + 1);
    *stamp = latest.stamp;
    return true;
}
