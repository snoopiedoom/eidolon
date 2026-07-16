#include "motion_config.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static const char *VALID_CONFIG = "version = 1\n"
                                  "seed = 42\n"
                                  "neutral.arm_lower_deg = 7.0\n"
                                  "neutral.elbow_add_deg = 4.0\n"
                                  "idle.breath.period_s = 4.2\n"
                                  "idle.breath.chest_deg = 0.8\n"
                                  "idle.breath.neck_counter_deg = 0.3\n"
                                  "idle.sway.period_s = 12.0\n"
                                  "idle.sway.spine_deg = 0.5\n"
                                  "idle.sway.chest_counter_deg = 0.25\n"
                                  "idle.sway.head_deg = 0.6\n";

static void expect_invalid(const char *text, const char *message_fragment) {
    EidolonMotionConfig config;
    char error[EIDOLON_MOTION_CONFIG_ERROR_CAPACITY];
    assert(!eidolon_motion_config_parse(text, strlen(text), &config, error, sizeof(error)));
    assert(strstr(error, message_fragment) != NULL);
}

static void test_file_watch(void) {
    const char *path = "motion_config_watch.tmp";
    EidolonMotionConfig active;
    EidolonMotionConfigWatch watch;
    eidolon_motion_config_defaults(&active);
    eidolon_motion_config_watch_init(&watch);

    assert(SDL_SaveFile(path, VALID_CONFIG, strlen(VALID_CONFIG)));
    assert(eidolon_motion_config_watch_poll(&watch, path, 0, &active) ==
           EIDOLON_MOTION_CONFIG_APPLIED);
    assert(watch.revision == 1U);
    assert(fabsf(active.neutral_arm_lower_degrees - 7.0F) < 0.0001F);

    const char *truncated = "version = 1\nseed = 42\n";
    assert(SDL_SaveFile(path, truncated, strlen(truncated)));
    assert(eidolon_motion_config_watch_poll(&watch, path, 251, &active) ==
           EIDOLON_MOTION_CONFIG_ERROR);
    assert(fabsf(active.neutral_arm_lower_degrees - 7.0F) < 0.0001F);
    assert(watch.revision == 1U);

    char changed[2048];
    SDL_strlcpy(changed, VALID_CONFIG, sizeof(changed));
    char *arm = strstr(changed, "neutral.arm_lower_deg = 7.0");
    assert(arm != NULL);
    SDL_memcpy(arm, "neutral.arm_lower_deg = 8.0", strlen("neutral.arm_lower_deg = 8.0"));
    assert(SDL_SaveFile(path, changed, strlen(changed)));
    assert(eidolon_motion_config_watch_poll(&watch, path, 502, &active) ==
           EIDOLON_MOTION_CONFIG_APPLIED);
    assert(fabsf(active.neutral_arm_lower_degrees - 8.0F) < 0.0001F);
    assert(watch.revision == 2U);

    assert(SDL_RemovePath(path));
    assert(eidolon_motion_config_watch_poll(&watch, path, 753, &active) ==
           EIDOLON_MOTION_CONFIG_ERROR);
    assert(SDL_SaveFile(path, changed, strlen(changed)));
    assert(eidolon_motion_config_watch_poll(&watch, path, 1004, &active) ==
           EIDOLON_MOTION_CONFIG_RECOVERED);
    assert(watch.revision == 2U);

    eidolon_motion_config_watch_force_reload(&watch);
    assert(eidolon_motion_config_watch_poll(&watch, path, 1005, &active) ==
           EIDOLON_MOTION_CONFIG_APPLIED);
    assert(watch.revision == 3U);
    assert(SDL_RemovePath(path));
}

int main(void) {
    EidolonMotionConfig config;
    char error[EIDOLON_MOTION_CONFIG_ERROR_CAPACITY];
    assert(eidolon_motion_config_parse(VALID_CONFIG, strlen(VALID_CONFIG), &config, error,
                                       sizeof(error)));
    assert(config.version == 1U);
    assert(config.seed == 42U);
    assert(fabsf(config.neutral_arm_lower_degrees - 7.0F) < 0.0001F);
    assert(fabsf(config.breath_period_seconds - 4.2F) < 0.0001F);

    const char *unknown = "version = 1\nseed = 42\nwat = 3\n";
    expect_invalid(unknown, "unknown key");

    const char *truncated = "version = 1\nseed = 42\nneutral.arm_lower_deg = 7.0\n";
    expect_invalid(truncated, "missing required key");

    char invalid_range[2048];
    SDL_strlcpy(invalid_range, VALID_CONFIG, sizeof(invalid_range));
    char *period = strstr(invalid_range, "idle.breath.period_s = 4.2");
    assert(period != NULL);
    SDL_memcpy(period, "idle.breath.period_s = 0.2", strlen("idle.breath.period_s = 0.2"));
    expect_invalid(invalid_range, "invalid value");

    char duplicate[2048];
    SDL_snprintf(duplicate, sizeof(duplicate), "%sneutral.arm_lower_deg = 2.0\n", VALID_CONFIG);
    expect_invalid(duplicate, "duplicate key");
    test_file_watch();
    return 0;
}
