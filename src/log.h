#ifndef EIDOLON_LOG_H
#define EIDOLON_LOG_H

#include <stdbool.h>
#include <stddef.h>

bool eidolon_log_path(char *path, size_t capacity);
void eidolon_log_write(const char *component, const char *format, ...);

#endif
