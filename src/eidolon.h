#ifndef EIDOLON_H
#define EIDOLON_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "affect.h"
#include "affect_client.h"
#include "bubble_layout.h"
#include "conversation_sources.h"
#include "dialogue.h"
#include "model.h"
#include "motion_config.h"
#include "platform/ipc.h"
#include "portrait.h"
#include "pose.h"
#include "session_registry.h"
#include "state.h"
#include "text_renderer.h"
#include "user_settings.h"

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

typedef enum EidolonRenderMode {
    EIDOLON_RENDER_MODE_SPRITE,
    EIDOLON_RENDER_MODE_PORTRAIT,
    EIDOLON_RENDER_MODE_MODEL_3D,
    EIDOLON_RENDER_MODE_COUNT,
} EidolonRenderMode;

typedef enum EidolonPrimaryInteraction {
    EIDOLON_PRIMARY_INTERACTION_NONE,
    EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG,
    EIDOLON_PRIMARY_INTERACTION_SESSION_BUBBLE,
    EIDOLON_PRIMARY_INTERACTION_DIALOGUE_BUBBLE,
} EidolonPrimaryInteraction;

typedef struct EidolonSettingsUi EidolonSettingsUi;

typedef struct EidolonApp {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *atlas;
    EidolonTextRenderer *text_renderer;
    EidolonModelRenderer *model;
    EidolonPortraitRenderer *portrait;
    EidolonSettingsUi *settings_ui;
    EidolonRenderMode render_mode;
    EidolonDialogueTheme dialogue_theme;
    EidolonDialogueMovement dialogue_movement;
    unsigned int dialogue_hold_ms;
    EidolonState state;
    EidolonAnimation animation;
    EidolonDialogue dialogue;
    EidolonAffectController affect;
    EidolonAffectClient *affect_client;
    uint64_t next_affect_sequence;
    uint64_t next_performance_track_id;
    EidolonIpcServer ipc;
    EidolonConversationSources *conversation_sources;
    EidolonSessionRegistry session_registry;
    EidolonMotionConfig motion_config;
    EidolonMotionConfigWatch motion_config_watch;
    bool running;
    bool snapshot_mode;
    EidolonPrimaryInteraction primary_interaction;
    bool primary_moved;
    float primary_local_x;
    float primary_local_y;
    float drag_global_x;
    float drag_global_y;
    int drag_window_x;
    int drag_window_y;
    int primary_session_slot;
    int hit_test_row;
    int hit_test_frame;
    int hit_test_mode;
    uint64_t hit_test_model_transform_revision;
    uint64_t hit_test_portrait_revision;
    bool hit_test_initialized;
    bool model_rotation_dragging;
    bool model_rotation_roll_dragging;
    bool model_rotation_hit_test_suspended;
    float model_scale;
    float model_yaw_degrees;
    float model_pitch_degrees;
    float model_roll_degrees;
    int model_render_resolution;
    int semantic_pose_index;
    EidolonSemanticPose semantic_pose;
    bool semantic_pose_dirty;
    uint64_t semantic_pose_copied_until_ms;
    bool motion_config_dirty;
    float display_scale;
    float window_coordinate_scale;
    int window_width;
    int window_height;
    SDL_FRect body_rect;
    bool body_rect_initialized;
    SDL_FRect bubble_rects[EIDOLON_VISIBLE_SESSION_CAPACITY];
    EidolonBubbleSide bubble_sides[EIDOLON_VISIBLE_SESSION_CAPACITY];
    bool bubble_rect_valid[EIDOLON_VISIBLE_SESSION_CAPACITY];
    EidolonBubbleBoundsMode bubble_bounds_mode;
    SDL_Rect bubble_custom_bounds;
    SDL_Rect bubble_resolved_bounds;
    SDL_DisplayID bubble_display_id;
    char user_settings_path[EIDOLON_USER_SETTINGS_PATH_CAPACITY];
    EidolonUserSettings system_settings;
    EidolonUserSettings user_settings;
    bool user_settings_ready;
    bool user_settings_applying;
    bool user_settings_dirty;
    uint64_t user_settings_save_at_ms;
} EidolonApp;

#endif
