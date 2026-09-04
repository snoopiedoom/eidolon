#ifndef EIDOLON_VRMA_REVIEW_HARNESS_H
#define EIDOLON_VRMA_REVIEW_HARNESS_H

#include "app.h"

#include <stdbool.h>

bool eidolon_vrma_review_harness_run(EidolonApp *app,
                                     const EidolonVrmPlaybackReport *initial_playback,
                                     const char *selection_output_path);

#endif
