#ifndef EIDOLON_PLATFORM_IPC_PROTOCOL_H
#define EIDOLON_PLATFORM_IPC_PROTOCOL_H

#include "state.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define EIDOLON_IPC_HEADER_SIZE 8
#define EIDOLON_IPC_MAX_MESSAGE_SIZE (EIDOLON_IPC_HEADER_SIZE + EIDOLON_IPC_TEXT_CAPACITY)

static inline size_t eidolon_ipc_encode(uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE],
                                        EidolonState state, const char *text) {
    size_t text_length = text == NULL ? 0 : strlen(text);
    if (text_length > EIDOLON_IPC_TEXT_CAPACITY) {
        text_length = EIDOLON_IPC_TEXT_CAPACITY;
    }
    message[0] = 'E';
    message[1] = 'I';
    message[2] = 'D';
    message[3] = 2;
    message[4] = (uint8_t)state;
    message[5] = 0;
    message[6] = (uint8_t)(text_length & 0xFFU);
    message[7] = (uint8_t)((text_length >> 8U) & 0xFFU);
    if (text_length > 0) {
        memcpy(message + EIDOLON_IPC_HEADER_SIZE, text, text_length);
    }
    return EIDOLON_IPC_HEADER_SIZE + text_length;
}

static inline bool eidolon_ipc_decode(const uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE],
                                      size_t size, EidolonState *state, char *text,
                                      size_t text_capacity) {
    static const uint8_t PREFIX[] = {'E', 'I', 'D', 2};
    if (size < EIDOLON_IPC_HEADER_SIZE || memcmp(message, PREFIX, sizeof(PREFIX)) != 0 ||
        message[4] >= EIDOLON_STATE_COUNT || text == NULL || text_capacity == 0) {
        return false;
    }

    const size_t text_length = (size_t)message[6] | ((size_t)message[7] << 8U);
    if (text_length > EIDOLON_IPC_TEXT_CAPACITY ||
        size != EIDOLON_IPC_HEADER_SIZE + text_length) {
        return false;
    }

    *state = (EidolonState)message[4];
    const size_t copy_length = text_length < text_capacity - 1 ? text_length : text_capacity - 1;
    if (copy_length > 0) {
        memcpy(text, message + EIDOLON_IPC_HEADER_SIZE, copy_length);
    }
    text[copy_length] = '\0';
    return true;
}

#endif
