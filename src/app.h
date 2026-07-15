#ifndef EIDOLON_APP_H
#define EIDOLON_APP_H

#include "eidolon.h"

bool eidolon_app_init(EidolonApp *app);
void eidolon_app_run(EidolonApp *app);
void eidolon_app_set_state(EidolonApp *app, EidolonState state);
void eidolon_app_destroy(EidolonApp *app);

#endif
