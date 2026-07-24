#ifndef EIDOLON_APP_EVENT_H
#define EIDOLON_APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum EidolonAppEventKind {
    EIDOLON_APP_EVENT_NONE,
    EIDOLON_APP_EVENT_QUIT_REQUESTED,
    EIDOLON_APP_EVENT_OPEN_SETTINGS,
    EIDOLON_APP_EVENT_RELOAD_CONFIGS,
    EIDOLON_APP_EVENT_FOCUS_LOST,
    EIDOLON_APP_EVENT_POINTER_DOWN,
    EIDOLON_APP_EVENT_POINTER_UP,
    EIDOLON_APP_EVENT_POINTER_MOTION,
    EIDOLON_APP_EVENT_GRAPHICS_TARGETS_RESET,
    EIDOLON_APP_EVENT_GRAPHICS_DEVICE_LOST,
} EidolonAppEventKind;

typedef enum EidolonAppPointerButton {
    EIDOLON_APP_POINTER_BUTTON_NONE,
    EIDOLON_APP_POINTER_BUTTON_PRIMARY,
    EIDOLON_APP_POINTER_BUTTON_MIDDLE,
    EIDOLON_APP_POINTER_BUTTON_SECONDARY,
} EidolonAppPointerButton;

typedef enum EidolonAppModifier {
    EIDOLON_APP_MODIFIER_SHIFT = UINT64_C(1) << 0,
} EidolonAppModifier;

typedef struct EidolonAppPointerEvent {
    float x;
    float y;
    float x_relative;
    float y_relative;
    float global_x;
    float global_y;
    uint64_t modifiers;
    EidolonAppPointerButton button;
    unsigned int clicks;
    bool global_position_valid;
} EidolonAppPointerEvent;

typedef union EidolonAppEventData {
    EidolonAppPointerEvent pointer;
} EidolonAppEventData;

typedef struct EidolonAppEvent {
    EidolonAppEventKind kind;
    uint64_t monotonic_ns;
    EidolonAppEventData data;
} EidolonAppEvent;

#endif
