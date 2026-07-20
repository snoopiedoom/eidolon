#ifndef EIDOLON_SETTINGS_UI_H
#define EIDOLON_SETTINGS_UI_H

#include <SDL3/SDL.h>

#include <stdbool.h>

typedef struct EidolonApp EidolonApp;
typedef struct EidolonSettingsUi EidolonSettingsUi;

EidolonSettingsUi *eidolon_settings_ui_create(const char *font_path);
void eidolon_settings_ui_destroy(EidolonSettingsUi *ui);
void eidolon_settings_ui_open(EidolonSettingsUi *ui);
void eidolon_settings_ui_close(EidolonSettingsUi *ui);
bool eidolon_settings_ui_visible(const EidolonSettingsUi *ui);
bool eidolon_settings_ui_handle_event(EidolonSettingsUi *ui, const SDL_Event *event);
void eidolon_settings_ui_draw(EidolonSettingsUi *ui, EidolonApp *app);
bool eidolon_settings_ui_snapshot(EidolonSettingsUi *ui, EidolonApp *app, const char *path);

#endif
