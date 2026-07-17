#ifndef EIDOLON_H
#define EIDOLON_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "affect.h"
#include "affect_client.h"
#include "dialogue.h"
#include "model.h"
#include "motion_config.h"
#include "platform/ipc.h"
#include "portrait.h"
#include "pose.h"
#include "session_registry.h"
#include "state.h"
#include "text_renderer.h"

#define EIDOLON_WINDOW_WIDTH 520
#define EIDOLON_WINDOW_HEIGHT 360
#define EIDOLON_MODEL_DISPLAY_SIZE 256.0F
#define EIDOLON_MODEL_SCALE_MIN 0.75F
#define EIDOLON_MODEL_SCALE_MAX 4.0F
#define EIDOLON_MODEL_ROTATION_MIN_DEGREES -180.0F
#define EIDOLON_MODEL_ROTATION_MAX_DEGREES 180.0F
#define EIDOLON_ATLAS_COLUMNS 8
#define EIDOLON_ATLAS_ROWS 11
#define EIDOLON_CELL_WIDTH 192
#define EIDOLON_CELL_HEIGHT 208

typedef struct EidolonAnimation {
    int row;
    int frame_count;
    int frame;
    uint64_t frame_started_ms;
} EidolonAnimation;

typedef enum EidolonDebugControl {
    EIDOLON_DEBUG_CONTROL_NONE,
    EIDOLON_DEBUG_CONTROL_MODEL_SCALE,
    EIDOLON_DEBUG_CONTROL_MODEL_YAW,
    EIDOLON_DEBUG_CONTROL_MODEL_PITCH,
    EIDOLON_DEBUG_CONTROL_MODEL_ROLL,
    EIDOLON_DEBUG_CONTROL_NEUTRAL_ARM_LOWER,
    EIDOLON_DEBUG_CONTROL_NEUTRAL_ELBOW_ADD,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_OUT,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_UP,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_FORWARD,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_OUT,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_UP,
    EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_FORWARD,
} EidolonDebugControl;

typedef struct EidolonApp {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *atlas;
    EidolonTextRenderer *text_renderer;
    EidolonModelRenderer *model;
    EidolonPortraitRenderer *portrait;
    EidolonDialogueTheme dialogue_theme;
    EidolonDialogueMovement dialogue_movement;
    unsigned int dialogue_hold_ms;
    EidolonState state;
    EidolonAnimation animation;
    EidolonDialogue dialogue;
    EidolonAffectController affect;
    EidolonAffectClient *affect_client;
    EidolonIpcServer ipc;
    EidolonSessionRegistry session_registry;
    EidolonMotionConfig motion_config;
    EidolonMotionConfigWatch motion_config_watch;
    bool running;
    bool snapshot_mode;
    bool dragging;
    bool drag_moved;
    float drag_global_x;
    float drag_global_y;
    int drag_window_x;
    int drag_window_y;
    int drag_session_slot;
    int hit_test_row;
    int hit_test_frame;
    int hit_test_mode;
    uint64_t hit_test_model_transform_revision;
    uint64_t hit_test_portrait_revision;
    bool hit_test_initialized;
    bool debug_visible;
    bool debug_pose_dropdown_open;
    bool debug_resolution_dropdown_open;
    bool debug_portrait_dropdown_open;
    bool model_rotation_dragging;
    bool model_rotation_roll_dragging;
    EidolonDebugControl debug_drag_control;
    float model_scale;
    float model_yaw_degrees;
    float model_pitch_degrees;
    float model_roll_degrees;
    int semantic_pose_index;
    EidolonSemanticPose semantic_pose;
    bool semantic_pose_dirty;
    uint64_t semantic_pose_copied_until_ms;
    bool motion_config_dirty;
    float display_scale;
    float window_coordinate_scale;
    int window_width;
    int window_height;
} EidolonApp;

#endif
