#ifndef EIDOLON_PORTRAIT_H
#define EIDOLON_PORTRAIT_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "affect.h"
#include "dialogue.h"
#include "state.h"

#define EIDOLON_PORTRAIT_MAX_EXPRESSIONS 16U
#define EIDOLON_PORTRAIT_NAME_CAPACITY 64U
#define EIDOLON_PORTRAIT_PATH_CAPACITY 256U
#define EIDOLON_PORTRAIT_LABEL_CAPACITY 32U
#define EIDOLON_PORTRAIT_ERROR_CAPACITY 256U

typedef enum EidolonDialogueTheme {
    EIDOLON_DIALOGUE_THEME_CLASSIC,
    EIDOLON_DIALOGUE_THEME_ACADEMY_HEART,
    EIDOLON_DIALOGUE_THEME_COUNT,
} EidolonDialogueTheme;

typedef struct EidolonPortraitExpressionConfig {
    char file[EIDOLON_PORTRAIT_PATH_CAPACITY];
    char label[EIDOLON_PORTRAIT_LABEL_CAPACITY];
    SDL_FRect portrait_crop;
} EidolonPortraitExpressionConfig;

typedef struct EidolonPortraitConfig {
    unsigned int version;
    char name[EIDOLON_PORTRAIT_NAME_CAPACITY];
    char directory[EIDOLON_PORTRAIT_PATH_CAPACITY];
    size_t expression_count;
    EidolonPortraitExpressionConfig expressions[EIDOLON_PORTRAIT_MAX_EXPRESSIONS];
    int state_expressions[EIDOLON_STATE_COUNT];
    float display_height;
    float portrait_display_height;
    bool default_face_mode;
    float breath_amount;
    float breath_period_seconds;
    float sway_pixels;
    float sway_degrees;
    float accent_strength;
    unsigned int accent_duration_ms;
    float posture_strength;
    float speech_strength;
    float attention_strength;
    EidolonDialogueTheme dialogue_theme;
    EidolonDialogueMovement dialogue_movement;
    unsigned int dialogue_hold_ms;
} EidolonPortraitConfig;

typedef struct EidolonPortraitRenderer EidolonPortraitRenderer;

typedef struct EidolonPortraitTransform {
    float x;
    float y;
    float width;
    float height;
    float rotation_degrees;
    float pivot_x;
    float pivot_y;
} EidolonPortraitTransform;

bool eidolon_portrait_config_parse(const char *text, size_t length, EidolonPortraitConfig *config,
                                   char *error, size_t error_capacity);
EidolonPortraitRenderer *eidolon_portrait_create(SDL_Renderer *renderer, const char *config_path,
                                                 const char *asset_directory);
void eidolon_portrait_destroy(EidolonPortraitRenderer *portrait);
void eidolon_portrait_update(EidolonPortraitRenderer *portrait, uint64_t now_ms);
void eidolon_portrait_set_state(EidolonPortraitRenderer *portrait, EidolonState state,
                                uint64_t now_ms);
void eidolon_portrait_set_expression_intent(EidolonPortraitRenderer *portrait,
                                            EidolonExpressionIntent intent, uint64_t now_ms);
void eidolon_portrait_set_override(EidolonPortraitRenderer *portrait, int expression,
                                   uint64_t now_ms);
void eidolon_portrait_deliver(EidolonPortraitRenderer *portrait, EidolonDeliveryCue cue,
                              float intensity, float direction, uint64_t now_ms);
void eidolon_portrait_perform(EidolonPortraitRenderer *portrait, EidolonPerformanceCue cue,
                              float intensity, uint64_t now_ms);
void eidolon_portrait_set_attention(EidolonPortraitRenderer *portrait, float direction);
void eidolon_portrait_force_reload(EidolonPortraitRenderer *portrait);
void eidolon_portrait_set_face_mode(EidolonPortraitRenderer *portrait, bool enabled);
bool eidolon_portrait_face_mode(const EidolonPortraitRenderer *portrait);
bool eidolon_portrait_evaluate_transform(EidolonPortraitRenderer *portrait, float x, float y,
                                         float width, float height, uint64_t now_ms,
                                         EidolonPortraitTransform *transform);
bool eidolon_portrait_draw_transform(EidolonPortraitRenderer *portrait, SDL_Renderer *renderer,
                                     const EidolonPortraitTransform *transform);
bool eidolon_portrait_draw(EidolonPortraitRenderer *portrait, SDL_Renderer *renderer,
                           const SDL_FRect *destination, uint64_t now_ms);
bool eidolon_portrait_ready(const EidolonPortraitRenderer *portrait);
size_t eidolon_portrait_expression_count(const EidolonPortraitRenderer *portrait);
const char *eidolon_portrait_expression_label(const EidolonPortraitRenderer *portrait,
                                              size_t expression);
int eidolon_portrait_current_expression(const EidolonPortraitRenderer *portrait);
int eidolon_portrait_override_expression(const EidolonPortraitRenderer *portrait);
float eidolon_portrait_display_width(const EidolonPortraitRenderer *portrait);
float eidolon_portrait_display_height(const EidolonPortraitRenderer *portrait);
uint64_t eidolon_portrait_revision(const EidolonPortraitRenderer *portrait);
EidolonDialogueTheme eidolon_portrait_dialogue_theme(const EidolonPortraitRenderer *portrait);
EidolonDialogueMovement eidolon_portrait_dialogue_movement(const EidolonPortraitRenderer *portrait);
unsigned int eidolon_portrait_dialogue_hold_ms(const EidolonPortraitRenderer *portrait);
const char *eidolon_portrait_error(const EidolonPortraitRenderer *portrait);

#endif
