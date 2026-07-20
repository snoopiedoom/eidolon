#include "portrait.h"

#include <assert.h>
#include <string.h>

static const char *VALID_CONFIG = "version = 1\n"
                                  "name = Test Character\n"
                                  "directory = characters/test\n"
                                  "expression_count = 2\n"
                                  "display_height = 560\n"
                                  "portrait_display_height = 360\n"
                                  "framing = full\n"
                                  "motion.breath_amount = 0.006\n"
                                  "motion.breath_period_s = 4.8\n"
                                  "motion.sway_pixels = 1.8\n"
                                  "motion.sway_degrees = 0.22\n"
                                  "motion.accent_strength = 1.25\n"
                                  "motion.accent_duration_ms = 600\n"
                                  "motion.posture_strength = 0.8\n"
                                  "motion.speech_strength = 1.2\n"
                                  "motion.attention_strength = 0.7\n"
                                  "dialogue.theme = academy_heart\n"
                                  "dialogue.movement = paged\n"
                                  "dialogue.hold_ms = 4250\n"
                                  "state.idle = 0\n"
                                  "state.running = 1\n"
                                  "state.waiting = 1\n"
                                  "state.review = 0\n"
                                  "state.failed = 1\n"
                                  "expression.0.file = default.png\n"
                                  "expression.0.label = default\n"
                                  "expression.0.portrait_crop = 10, 20, 300, 400\n"
                                  "expression.1.file = unhappy.png\n"
                                  "expression.1.label = unhappy\n"
                                  "expression.1.portrait_crop = 11, 21, 301, 401\n";

int main(void) {
    EidolonPortraitConfig config;
    char error[EIDOLON_PORTRAIT_ERROR_CAPACITY];
    assert(eidolon_portrait_config_parse(VALID_CONFIG, strlen(VALID_CONFIG), &config, error,
                                         sizeof(error)));
    assert(config.expression_count == 2U);
    assert(strcmp(config.name, "Test Character") == 0);
    assert(strcmp(config.expressions[1].file, "unhappy.png") == 0);
    assert(config.state_expressions[EIDOLON_STATE_RUNNING] == 1);
    assert(config.expressions[1].portrait_crop.x == 11.0F);
    assert(config.accent_strength == 1.25F);
    assert(config.accent_duration_ms == 600U);
    assert(config.posture_strength == 0.8F);
    assert(config.speech_strength == 1.2F);
    assert(config.attention_strength == 0.7F);
    assert(config.dialogue_theme == EIDOLON_DIALOGUE_THEME_ACADEMY_HEART);
    assert(config.dialogue_movement == EIDOLON_DIALOGUE_MOVEMENT_PAGED);
    assert(config.dialogue_hold_ms == 4250U);
    assert(!config.default_face_mode);

    const char *incomplete = "version = 1\nname = Test\ndirectory = test\nexpression_count = 1\n"
                             "display_height = 560\nportrait_display_height = 360\n"
                             "motion.breath_amount = 0.01\nmotion.breath_period_s = 4\n"
                             "motion.sway_pixels = 1\nmotion.sway_degrees = 0.2\n";
    assert(!eidolon_portrait_config_parse(incomplete, strlen(incomplete), &config, error,
                                          sizeof(error)));
    assert(strstr(error, "expression") != NULL || strstr(error, "state") != NULL);
    return 0;
}
