#ifndef EIDOLON_PRESENTATION_INTERNAL_H
#define EIDOLON_PRESENTATION_INTERNAL_H

#include "presentation.h"

#ifdef __cplusplus
extern "C" {
#endif

void *eidolon_presentation_backend_context(EidolonPresentation *presentation,
                                           const char *backend_name);

#ifdef __cplusplus
}
#endif

#endif
