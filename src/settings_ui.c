#include "settings_ui.h"

#include "app.h"
#include "log.h"

#include "dcimgui.h"
#include "dcimgui_impl_sdl3.h"
#include "dcimgui_impl_sdlrenderer3.h"

#include <stdlib.h>

#define SETTINGS_WINDOW_WIDTH 720
#define SETTINGS_WINDOW_HEIGHT 620
#define SETTINGS_FONT_SIZE 17.0F

struct EidolonSettingsUi {
    SDL_Window *window;
    SDL_Renderer *renderer;
    ImGuiContext *context;
    bool platform_ready;
    bool renderer_ready;
    bool visible;
};

static const char *setting_source(const EidolonApp *app, EidolonUserSettingField field) {
    return eidolon_user_settings_is_overridden(&app->user_settings, field) ? "user override"
                                                                           : "system default";
}

static void reset_setting_button(EidolonApp *app, EidolonUserSettingField field, const char *id) {
    const bool overridden = eidolon_user_settings_is_overridden(&app->user_settings, field);
    ImGui_SameLine();
    ImGui_BeginDisabled(!overridden);
    if (ImGui_Button(id)) {
        (void)eidolon_app_reset_user_setting(app, field);
    }
    ImGui_EndDisabled();
}

static void select_render_mode(EidolonApp *app) {
    const char *preview = eidolon_render_mode_name(app->render_mode);
    if (!ImGui_BeginCombo("preferred renderer", preview, 0)) {
        reset_setting_button(app, EIDOLON_USER_SETTING_RENDER_MODE, "reset##render_mode");
        ImGui_Text("default: %s  |  source: %s",
                   eidolon_render_mode_name((EidolonRenderMode)app->system_settings.render_mode),
                   setting_source(app, EIDOLON_USER_SETTING_RENDER_MODE));
        return;
    }
    for (int value = 0; value < (int)EIDOLON_RENDER_MODE_COUNT; ++value) {
        const EidolonRenderMode mode = (EidolonRenderMode)value;
        const bool selected = mode == app->render_mode;
        if (ImGui_SelectableEx(eidolon_render_mode_name(mode), selected, 0, (ImVec2){0.0F, 0.0F})) {
            (void)eidolon_app_set_render_mode(app, mode);
        }
        if (selected) {
            ImGui_SetItemDefaultFocus();
        }
    }
    ImGui_EndCombo();
    reset_setting_button(app, EIDOLON_USER_SETTING_RENDER_MODE, "reset##render_mode");
    ImGui_Text("default: %s  |  source: %s",
               eidolon_render_mode_name((EidolonRenderMode)app->system_settings.render_mode),
               setting_source(app, EIDOLON_USER_SETTING_RENDER_MODE));
}

static void select_state(EidolonApp *app) {
    if (!ImGui_BeginCombo("runtime state", eidolon_state_name(app->state), 0)) {
        return;
    }
    for (int value = 0; value < (int)EIDOLON_STATE_COUNT; ++value) {
        const EidolonState state = (EidolonState)value;
        const bool selected = state == app->state;
        if (ImGui_SelectableEx(eidolon_state_name(state), selected, 0, (ImVec2){0.0F, 0.0F})) {
            eidolon_app_set_state(app, state);
        }
        if (selected) {
            ImGui_SetItemDefaultFocus();
        }
    }
    ImGui_EndCombo();
}

static void draw_portrait_settings(EidolonApp *app) {
    bool face_mode = eidolon_portrait_face_mode(app->portrait);
    if (ImGui_Checkbox("face framing", &face_mode)) {
        eidolon_app_set_portrait_framing(app, face_mode);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE, "reset##portrait_face_mode");
    ImGui_Text("default: %s  |  source: %s",
               app->system_settings.portrait_face_mode ? "face" : "full body",
               setting_source(app, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE));

    const int override = eidolon_portrait_override_expression(app->portrait);
    const char *preview = "automatic";
    if (override >= 0) {
        preview = eidolon_portrait_expression_label(app->portrait, (size_t) override);
    }
    if (ImGui_BeginCombo("expression", preview, 0)) {
        if (ImGui_SelectableEx("automatic", override < 0, 0, (ImVec2){0.0F, 0.0F})) {
            eidolon_app_select_portrait(app, -1);
        }
        const size_t count = eidolon_portrait_expression_count(app->portrait);
        for (size_t index = 0U; index < count; ++index) {
            const bool selected = override == (int)index;
            const char *label = eidolon_portrait_expression_label(app->portrait, index);
            if (ImGui_SelectableEx(label, selected, 0, (ImVec2){0.0F, 0.0F})) {
                eidolon_app_select_portrait(app, (int)index);
            }
            if (selected) {
                ImGui_SetItemDefaultFocus();
            }
        }
        ImGui_EndCombo();
    }
    ImGui_Text("active expression: %s",
               eidolon_portrait_expression_label(
                   app->portrait, (size_t)eidolon_portrait_current_expression(app->portrait)));
}

static void select_model_resolution(EidolonApp *app) {
    static const int resolutions[] = {512, 1024, 1536, 2048};
    const int current = eidolon_model_render_resolution(app->model);
    char preview[32];
    SDL_snprintf(preview, sizeof(preview), "%d x %d", current, current);
    if (!ImGui_BeginCombo("render resolution", preview, 0)) {
        reset_setting_button(app, EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION,
                             "reset##model_resolution");
        ImGui_Text("default: %d x %d  |  source: %s", app->system_settings.model_render_resolution,
                   app->system_settings.model_render_resolution,
                   setting_source(app, EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION));
        return;
    }
    for (size_t index = 0U; index < SDL_arraysize(resolutions); ++index) {
        char label[32];
        SDL_snprintf(label, sizeof(label), "%d x %d", resolutions[index], resolutions[index]);
        const bool selected = current == resolutions[index];
        if (ImGui_SelectableEx(label, selected, 0, (ImVec2){0.0F, 0.0F})) {
            (void)eidolon_app_set_model_render_resolution(app, resolutions[index]);
        }
        if (selected) {
            ImGui_SetItemDefaultFocus();
        }
    }
    ImGui_EndCombo();
    reset_setting_button(app, EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION,
                         "reset##model_resolution");
    ImGui_Text("default: %d x %d  |  source: %s", app->system_settings.model_render_resolution,
               app->system_settings.model_render_resolution,
               setting_source(app, EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION));
}

static void select_semantic_pose(EidolonApp *app) {
    const char *preview = "neutral";
    if (app->semantic_pose_index >= 0) {
        const EidolonSemanticPose *pose = eidolon_semantic_pose((size_t)app->semantic_pose_index);
        if (pose != NULL) {
            preview = pose->name;
        }
    }
    if (!ImGui_BeginCombo("semantic pose", preview, 0)) {
        return;
    }
    if (ImGui_SelectableEx("neutral", app->semantic_pose_index < 0, 0, (ImVec2){0.0F, 0.0F})) {
        eidolon_app_select_semantic_pose(app, -1);
    }
    const size_t count = eidolon_semantic_pose_count();
    for (size_t index = 0U; index < count; ++index) {
        const EidolonSemanticPose *pose = eidolon_semantic_pose(index);
        const bool selected = app->semantic_pose_index == (int)index;
        if (pose != NULL && ImGui_SelectableEx(pose->name, selected, 0, (ImVec2){0.0F, 0.0F})) {
            eidolon_app_select_semantic_pose(app, (int)index);
        }
        if (selected) {
            ImGui_SetItemDefaultFocus();
        }
    }
    ImGui_EndCombo();
}

static void draw_model_settings(EidolonApp *app) {
    select_model_resolution(app);

    float yaw = app->model_yaw_degrees;
    float pitch = app->model_pitch_degrees;
    float roll = app->model_roll_degrees;
    if (ImGui_SliderFloatEx("yaw", &yaw, -180.0F, 180.0F, "%.1f deg", 0)) {
        eidolon_app_set_model_rotation(app, yaw, pitch, roll);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_MODEL_YAW, "reset##model_yaw");
    ImGui_Text("default: %.1f deg  |  source: %s", app->system_settings.model_yaw_degrees,
               setting_source(app, EIDOLON_USER_SETTING_MODEL_YAW));
    if (ImGui_SliderFloatEx("pitch", &pitch, -180.0F, 180.0F, "%.1f deg", 0)) {
        eidolon_app_set_model_rotation(app, yaw, pitch, roll);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_MODEL_PITCH, "reset##model_pitch");
    ImGui_Text("default: %.1f deg  |  source: %s", app->system_settings.model_pitch_degrees,
               setting_source(app, EIDOLON_USER_SETTING_MODEL_PITCH));
    if (ImGui_SliderFloatEx("roll", &roll, -180.0F, 180.0F, "%.1f deg", 0)) {
        eidolon_app_set_model_rotation(app, yaw, pitch, roll);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_MODEL_ROLL, "reset##model_roll");
    ImGui_Text("default: %.1f deg  |  source: %s", app->system_settings.model_roll_degrees,
               setting_source(app, EIDOLON_USER_SETTING_MODEL_ROLL));

    ImGui_SeparatorText("pose");
    select_semantic_pose(app);
    if (app->semantic_pose_index < 0) {
        float arm = app->motion_config.neutral_arm_lower_degrees;
        float elbow = app->motion_config.neutral_elbow_add_degrees;
        bool pose_changed = false;
        pose_changed |=
            ImGui_SliderFloatEx("arm lower", &arm, EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES,
                                EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES, "%.1f deg", 0);
        pose_changed |=
            ImGui_SliderFloatEx("elbow add", &elbow, EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES,
                                EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES, "%.1f deg", 0);
        if (pose_changed) {
            eidolon_app_set_neutral_pose(app, arm, elbow);
        }
    } else {
        static const char *component_names[3] = {"out", "up", "forward"};
        for (size_t component = 0U; component < 3U; ++component) {
            char hand_label[32];
            char pole_label[32];
            SDL_snprintf(hand_label, sizeof(hand_label), "hand %s", component_names[component]);
            SDL_snprintf(pole_label, sizeof(pole_label), "pole %s", component_names[component]);
            float hand = app->semantic_pose.arms[0].hand[component];
            float pole = app->semantic_pose.arms[0].elbow_pole[component];
            if (ImGui_SliderFloat(hand_label, &hand, EIDOLON_POSE_TARGET_MIN,
                                  EIDOLON_POSE_TARGET_MAX)) {
                eidolon_app_set_semantic_arm_component(app, false, component, hand);
            }
            if (ImGui_SliderFloat(pole_label, &pole, EIDOLON_POSE_TARGET_MIN,
                                  EIDOLON_POSE_TARGET_MAX)) {
                eidolon_app_set_semantic_arm_component(app, true, component, pole);
            }
        }
        if (ImGui_Button("copy pose initializer")) {
            (void)eidolon_app_copy_semantic_pose(app);
        }
    }
}

static void draw_character_tab(EidolonApp *app) {
    ImGui_SeparatorText("renderer");
    select_render_mode(app);

    ImGui_BeginDisabled(true);
    if (ImGui_BeginCombo("model", eidolon_app_model_name(app), 0)) {
        ImGui_SelectableEx(eidolon_app_model_name(app), true, 0, (ImVec2){0.0F, 0.0F});
        ImGui_EndCombo();
    }
    ImGui_EndDisabled();
    ImGui_TextWrapped("one installed model is currently registered for this renderer.");

    float scale = app->model_scale;
    if (ImGui_SliderFloatEx("display scale", &scale, EIDOLON_MODEL_SCALE_MIN,
                            EIDOLON_MODEL_SCALE_MAX, "%.2f x", 0)) {
        eidolon_app_set_model_scale(app, scale);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_DISPLAY_SCALE, "reset##display_scale");
    ImGui_Text("default: %.2f x  |  source: %s", app->system_settings.display_scale,
               setting_source(app, EIDOLON_USER_SETTING_DISPLAY_SCALE));
    select_state(app);

    ImGui_SeparatorText(eidolon_render_mode_name(app->render_mode));
    switch (app->render_mode) {
    case EIDOLON_RENDER_MODE_PORTRAIT:
        draw_portrait_settings(app);
        break;
    case EIDOLON_RENDER_MODE_MODEL_3D:
        draw_model_settings(app);
        break;
    case EIDOLON_RENDER_MODE_SPRITE:
        ImGui_TextWrapped("sprite playback follows the runtime state. per-animation controls come "
                          "with the model catalog.");
        break;
    case EIDOLON_RENDER_MODE_COUNT:
        break;
    }

    ImGui_SeparatorText("configuration");
    if (ImGui_Button("reload configs  F5")) {
        eidolon_app_reload_configs(app);
    }
}

static const char *theme_name(EidolonDialogueTheme theme) {
    return theme == EIDOLON_DIALOGUE_THEME_ACADEMY_HEART ? "academy heart" : "classic";
}

static void draw_bubble_bounds_settings(EidolonApp *app) {
    ImGui_SeparatorText("placement bounds");
    if (ImGui_BeginCombo("monitor policy", eidolon_bubble_bounds_mode_name(app->bubble_bounds_mode),
                         0)) {
        for (int value = 0; value < (int)EIDOLON_BUBBLE_BOUNDS_COUNT; ++value) {
            const EidolonBubbleBoundsMode mode = (EidolonBubbleBoundsMode)value;
            if (ImGui_SelectableEx(eidolon_bubble_bounds_mode_name(mode),
                                   mode == app->bubble_bounds_mode, 0, (ImVec2){0.0F, 0.0F})) {
                eidolon_app_set_bubble_bounds_mode(app, mode);
            }
        }
        ImGui_EndCombo();
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_BUBBLE_BOUNDS, "reset##bubble_bounds");
    ImGui_Text("default: %s  |  source: %s",
               eidolon_bubble_bounds_mode_name(
                   (EidolonBubbleBoundsMode)app->system_settings.bubble_bounds_mode),
               setting_source(app, EIDOLON_USER_SETTING_BUBBLE_BOUNDS));

    if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_CUSTOM) {
        SDL_Rect bounds = app->bubble_custom_bounds;
        bool changed = ImGui_InputInt("custom x", &bounds.x);
        changed = ImGui_InputInt("custom y", &bounds.y) || changed;
        if (ImGui_InputInt("custom width", &bounds.w)) {
            bounds.w = SDL_max(1, bounds.w);
            changed = true;
        }
        if (ImGui_InputInt("custom height", &bounds.h)) {
            bounds.h = SDL_max(1, bounds.h);
            changed = true;
        }
        if (changed) {
            eidolon_app_set_bubble_custom_bounds(app, bounds);
        }
    }
    ImGui_Text("resolved: %d, %d  %d x %d", app->bubble_resolved_bounds.x,
               app->bubble_resolved_bounds.y, app->bubble_resolved_bounds.w,
               app->bubble_resolved_bounds.h);
    ImGui_TextWrapped("avatar follows the character's monitor. primary pins bubbles to the primary "
                      "work area. virtual permits the combined desktop. custom uses the rectangle "
                      "above.");
}

static void draw_dialogue_tab(EidolonApp *app) {
    if (ImGui_BeginCombo("theme", theme_name(app->dialogue_theme), 0)) {
        for (int value = 0; value < (int)EIDOLON_DIALOGUE_THEME_COUNT; ++value) {
            const EidolonDialogueTheme theme = (EidolonDialogueTheme)value;
            if (ImGui_SelectableEx(theme_name(theme), theme == app->dialogue_theme, 0,
                                   (ImVec2){0.0F, 0.0F})) {
                eidolon_app_set_dialogue_theme(app, theme);
            }
        }
        ImGui_EndCombo();
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_DIALOGUE_THEME, "reset##dialogue_theme");
    ImGui_Text("default: %s  |  source: %s",
               theme_name((EidolonDialogueTheme)app->system_settings.dialogue_theme),
               setting_source(app, EIDOLON_USER_SETTING_DIALOGUE_THEME));

    draw_bubble_bounds_settings(app);

    ImGui_SeparatorText("text delivery");

    if (ImGui_BeginCombo("text movement", eidolon_dialogue_movement_name(app->dialogue_movement),
                         0)) {
        for (int value = 0; value < (int)EIDOLON_DIALOGUE_MOVEMENT_COUNT; ++value) {
            const EidolonDialogueMovement movement = (EidolonDialogueMovement)value;
            if (ImGui_SelectableEx(eidolon_dialogue_movement_name(movement),
                                   movement == app->dialogue_movement, 0, (ImVec2){0.0F, 0.0F})) {
                eidolon_app_set_dialogue_movement(app, movement);
            }
        }
        ImGui_EndCombo();
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT, "reset##dialogue_movement");
    ImGui_Text("default: %s  |  source: %s",
               eidolon_dialogue_movement_name(
                   (EidolonDialogueMovement)app->system_settings.dialogue_movement),
               setting_source(app, EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT));

    int hold_ms = (int)app->dialogue_hold_ms;
    if (ImGui_SliderIntEx("page hold", &hold_ms, 250, 10000, "%d ms", 0)) {
        eidolon_app_set_dialogue_hold_ms(app, (unsigned int)hold_ms);
    }
    reset_setting_button(app, EIDOLON_USER_SETTING_DIALOGUE_HOLD, "reset##dialogue_hold");
    ImGui_Text("default: %u ms  |  source: %s", app->system_settings.dialogue_hold_ms,
               setting_source(app, EIDOLON_USER_SETTING_DIALOGUE_HOLD));
    ImGui_TextWrapped("manual advances on click. paged reveals a whole page after the hold. follow "
                      "scrolls one line as the cursor reaches the bottom.");
}

static void draw_input_tab(void) {
    ImGui_SeparatorText("pet window");
    ImGui_Text("left drag character     move pet");
    ImGui_Text("left click bubble       advance manual text");
    ImGui_Text("right click character   open settings");
    ImGui_Text("middle drag 3d model    yaw and pitch");
    ImGui_Text("shift + middle drag     roll");
    ImGui_Text("double middle click     reset rotation");
    ImGui_SeparatorText("keyboard");
    ImGui_Text("F1                       open settings");
    ImGui_Text("F5                       reload configs");
    ImGui_Text("Escape                   close settings / quit pet");
}

static const char *affect_source_name(EidolonAffectSource source) {
    return source == EIDOLON_AFFECT_SOURCE_GOEMOTIONS ? "goemotions" : "runtime state";
}

static void draw_diagnostics_tab(const EidolonApp *app) {
    ImGui_Text("renderer: %s", eidolon_render_mode_name(app->render_mode));
    ImGui_Text("model: %s", eidolon_app_model_name(app));
    ImGui_Text("state: %s", eidolon_state_name(app->state));
    ImGui_Text("affect: %s", affect_source_name(app->affect.source));
    ImGui_Text("expression intent: %s",
               eidolon_expression_intent_name(app->affect.expression_intent));
    ImGui_Text("VAD: %.2f  %.2f  %.2f", app->affect.current.valence, app->affect.current.arousal,
               app->affect.current.dominance);
    ImGui_Text("evidence: %.3f", app->affect.evidence);
    ImGui_Text("motion revision: %llu", (unsigned long long)app->motion_config_watch.revision);
    ImGui_Text("user override fields: 0x%x", app->user_settings.overrides);
    ImGui_Text("bubble bounds: %s  %d,%d %dx%d",
               eidolon_bubble_bounds_mode_name(app->bubble_bounds_mode),
               app->bubble_resolved_bounds.x, app->bubble_resolved_bounds.y,
               app->bubble_resolved_bounds.w, app->bubble_resolved_bounds.h);
    ImGui_TextWrapped("user settings: %s",
                      app->user_settings_path[0] != '\0' ? app->user_settings_path : "disabled");
    if (app->motion_config_watch.error[0] != '\0') {
        ImGui_TextWrapped("motion config: %s", app->motion_config_watch.error);
    }
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT) {
        const char *error = eidolon_portrait_error(app->portrait);
        ImGui_TextWrapped("portrait: %s", error != NULL && error[0] != '\0' ? error : "ready");
    }
}

static void draw_settings(EidolonApp *app) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(app->settings_ui->window, &width, &height);
    ImGui_SetNextWindowPos((ImVec2){0.0F, 0.0F}, ImGuiCond_Always);
    ImGui_SetNextWindowSize((ImVec2){(float)width, (float)height}, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (ImGui_Begin("Eidolon settings##root", NULL, flags) &&
        ImGui_BeginTabBar("settings tabs", 0)) {
        if (ImGui_BeginTabItem("Character", NULL, 0)) {
            draw_character_tab(app);
            ImGui_EndTabItem();
        }
        if (ImGui_BeginTabItem("Dialogue", NULL, 0)) {
            draw_dialogue_tab(app);
            ImGui_EndTabItem();
        }
        if (ImGui_BeginTabItem("Input", NULL, 0)) {
            draw_input_tab();
            ImGui_EndTabItem();
        }
        if (ImGui_BeginTabItem("Diagnostics", NULL, 0)) {
            draw_diagnostics_tab(app);
            ImGui_EndTabItem();
        }
        ImGui_EndTabBar();
    }
    ImGui_End();
}

EidolonSettingsUi *eidolon_settings_ui_create(const char *font_path) {
    EidolonSettingsUi *ui = calloc(1U, sizeof(*ui));
    if (ui == NULL) {
        return NULL;
    }
    ui->window =
        SDL_CreateWindow("Eidolon settings", SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT,
                         SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (ui->window == NULL) {
        eidolon_log_write("settings", "could not create window: %s", SDL_GetError());
        eidolon_settings_ui_destroy(ui);
        return NULL;
    }
    ui->renderer = SDL_CreateRenderer(ui->window, NULL);
    if (ui->renderer == NULL) {
        eidolon_log_write("settings", "could not create renderer: %s", SDL_GetError());
        eidolon_settings_ui_destroy(ui);
        return NULL;
    }
    if (!SDL_SetRenderVSync(ui->renderer, 1)) {
        eidolon_log_write("settings", "could not enable vsync: %s", SDL_GetError());
    }

    ui->context = ImGui_CreateContext(NULL);
    if (ui->context == NULL) {
        eidolon_log_write("settings", "could not create Dear ImGui context");
        eidolon_settings_ui_destroy(ui);
        return NULL;
    }
    ImGui_SetCurrentContext(ui->context);
    ImGuiIO *io = ImGui_GetIO();
    io->IniFilename = NULL;
    ImGui_StyleColorsDark(NULL);
    if (font_path != NULL && ImFontAtlas_AddFontFromFileTTF(
                                 io->Fonts, font_path, SETTINGS_FONT_SIZE, NULL, NULL) == NULL) {
        eidolon_log_write("settings", "could not load font: %s", font_path);
    }
    ui->platform_ready = cImGui_ImplSDL3_InitForSDLRenderer(ui->window, ui->renderer);
    ui->renderer_ready = ui->platform_ready && cImGui_ImplSDLRenderer3_Init(ui->renderer);
    if (!ui->renderer_ready) {
        eidolon_log_write("settings", "Dear ImGui backend initialization failed");
        eidolon_settings_ui_destroy(ui);
        return NULL;
    }
    eidolon_log_write("settings", "window ready Dear ImGui %s", ImGui_GetVersion());
    return ui;
}

void eidolon_settings_ui_destroy(EidolonSettingsUi *ui) {
    if (ui == NULL) {
        return;
    }
    if (ui->context != NULL) {
        ImGui_SetCurrentContext(ui->context);
    }
    if (ui->renderer_ready) {
        cImGui_ImplSDLRenderer3_Shutdown();
    }
    if (ui->platform_ready) {
        cImGui_ImplSDL3_Shutdown();
    }
    if (ui->context != NULL) {
        ImGui_DestroyContext(ui->context);
    }
    SDL_DestroyRenderer(ui->renderer);
    SDL_DestroyWindow(ui->window);
    free(ui);
}

void eidolon_settings_ui_open(EidolonSettingsUi *ui) {
    if (ui == NULL || ui->visible) {
        return;
    }
    ui->visible = true;
    SDL_ShowWindow(ui->window);
    SDL_RaiseWindow(ui->window);
}

void eidolon_settings_ui_close(EidolonSettingsUi *ui) {
    if (ui == NULL || !ui->visible) {
        return;
    }
    ui->visible = false;
    SDL_HideWindow(ui->window);
}

bool eidolon_settings_ui_visible(const EidolonSettingsUi *ui) { return ui != NULL && ui->visible; }

bool eidolon_settings_ui_handle_event(EidolonSettingsUi *ui, const SDL_Event *event) {
    if (ui == NULL || event == NULL || SDL_GetWindowFromEvent(event) != ui->window) {
        return false;
    }
    ImGui_SetCurrentContext(ui->context);
    (void)cImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
        (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
         (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_F1))) {
        eidolon_settings_ui_close(ui);
    }
    return true;
}

static void render_settings_frame(EidolonSettingsUi *ui, EidolonApp *app, bool present) {
    if (ui == NULL || app == NULL || !ui->visible) {
        return;
    }
    ImGui_SetCurrentContext(ui->context);
    cImGui_ImplSDLRenderer3_NewFrame();
    cImGui_ImplSDL3_NewFrame();
    ImGui_NewFrame();
    draw_settings(app);
    ImGui_Render();

    SDL_SetRenderDrawColor(ui->renderer, 14U, 16U, 24U, 255U);
    SDL_RenderClear(ui->renderer);
    cImGui_ImplSDLRenderer3_RenderDrawData(ImGui_GetDrawData(), ui->renderer);
    if (present) {
        SDL_RenderPresent(ui->renderer);
    }
}

void eidolon_settings_ui_draw(EidolonSettingsUi *ui, EidolonApp *app) {
    render_settings_frame(ui, app, true);
}

bool eidolon_settings_ui_snapshot(EidolonSettingsUi *ui, EidolonApp *app, const char *path) {
    if (ui == NULL || app == NULL || path == NULL) {
        return false;
    }
    const bool was_visible = ui->visible;
    ui->visible = true;
    render_settings_frame(ui, app, false);
    render_settings_frame(ui, app, false);
    ui->visible = was_visible;

    SDL_Surface *surface = SDL_RenderReadPixels(ui->renderer, NULL);
    if (surface == NULL) {
        eidolon_log_write("settings", "could not read snapshot pixels: %s", SDL_GetError());
        return false;
    }
    const bool saved = SDL_SavePNG(surface, path);
    if (!saved) {
        eidolon_log_write("settings", "could not save snapshot %s: %s", path, SDL_GetError());
    }
    SDL_DestroySurface(surface);
    return saved;
}
