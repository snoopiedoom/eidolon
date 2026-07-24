#ifndef EIDOLON_EVENT_PUMP_H
#define EIDOLON_EVENT_PUMP_H

#include "app_event.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct EidolonEventPump EidolonEventPump;
typedef struct EidolonPresentation EidolonPresentation;
typedef struct EidolonSettingsUi EidolonSettingsUi;

EidolonEventPump *eidolon_event_pump_create(EidolonPresentation *presentation,
                                            EidolonSettingsUi *settings_ui);
void eidolon_event_pump_destroy(EidolonEventPump *pump);

bool eidolon_event_pump_wait(EidolonEventPump *pump, int timeout_ms, EidolonAppEvent *event);
bool eidolon_event_pump_poll(EidolonEventPump *pump, EidolonAppEvent *event);

#endif
