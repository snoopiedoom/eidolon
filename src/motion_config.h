#ifndef EIDOLON_MOTION_CONFIG_H
#define EIDOLON_MOTION_CONFIG_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_MOTION_CONFIG_VERSION 1U
#define EIDOLON_MOTION_CONFIG_ERROR_CAPACITY 256
#define EIDOLON_MOTION_CONFIG_POLL_INTERVAL_MS 250U

#define EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES -45.0F
#define EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES 90.0F
#define EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES -90.0F
#define EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES 90.0F
#define EIDOLON_IDLE_ROTATION_MIN_DEGREES -15.0F
#define EIDOLON_IDLE_ROTATION_MAX_DEGREES 15.0F

typedef struct EidolonMotionConfig {
    uint32_t version;
    uint64_t seed;
    float neutral_arm_lower_degrees;
    float neutral_elbow_add_degrees;
    float breath_period_seconds;
    float breath_chest_degrees;
    float breath_neck_counter_degrees;
    float sway_period_seconds;
    float sway_spine_degrees;
    float sway_chest_counter_degrees;
    float sway_head_degrees;
} EidolonMotionConfig;

typedef struct EidolonMotionConfigWatch {
    uint64_t next_poll_ms;
    uint64_t observed_hash;
    uint64_t active_hash;
    uint64_t revision;
    bool has_observed_hash;
    bool error_is_parse;
    char error[EIDOLON_MOTION_CONFIG_ERROR_CAPACITY];
} EidolonMotionConfigWatch;

typedef enum EidolonMotionConfigPollResult {
    EIDOLON_MOTION_CONFIG_UNCHANGED,
    EIDOLON_MOTION_CONFIG_APPLIED,
    EIDOLON_MOTION_CONFIG_ERROR,
    EIDOLON_MOTION_CONFIG_RECOVERED,
} EidolonMotionConfigPollResult;

void eidolon_motion_config_defaults(EidolonMotionConfig *config);
bool eidolon_motion_config_parse(const char *text, size_t size, EidolonMotionConfig *config,
                                 char *error, size_t error_capacity);
void eidolon_motion_config_watch_init(EidolonMotionConfigWatch *watch);
void eidolon_motion_config_watch_force_reload(EidolonMotionConfigWatch *watch);
EidolonMotionConfigPollResult eidolon_motion_config_watch_poll(EidolonMotionConfigWatch *watch,
                                                               const char *path, uint64_t now_ms,
                                                               EidolonMotionConfig *active_config);

#endif
