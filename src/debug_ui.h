#ifndef EIDOLON_DEBUG_UI_H
#define EIDOLON_DEBUG_UI_H

#include "eidolon.h"

bool eidolon_debug_ui_handle_event(EidolonApp *app, const SDL_Event *event);
void eidolon_debug_ui_draw(EidolonApp *app);

#endif
