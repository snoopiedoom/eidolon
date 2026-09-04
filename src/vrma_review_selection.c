#include "vrma_review_selection.h"

#include <math.h>
#include <string.h>

#define SELECTION_EPSILON 1.0e-5F

static bool valid_selection(const EidolonVrmaReviewSelection *selection) {
    return selection != NULL && selection->version == EIDOLON_VRMA_REVIEW_SELECTION_VERSION &&
           isfinite(selection->duration_seconds) && selection->duration_seconds > 0.0F;
}

static float clamp_position(const EidolonVrmaReviewSelection *selection, float position_seconds) {
    return fminf(fmaxf(position_seconds, 0.0F), selection->duration_seconds);
}

bool eidolon_vrma_review_selection_init(EidolonVrmaReviewSelection *selection,
                                        float duration_seconds) {
    if (selection == NULL || !isfinite(duration_seconds) || duration_seconds <= 0.0F) {
        return false;
    }
    memset(selection, 0, sizeof(*selection));
    selection->version = EIDOLON_VRMA_REVIEW_SELECTION_VERSION;
    selection->duration_seconds = duration_seconds;
    return true;
}

bool eidolon_vrma_review_selection_mark_start(EidolonVrmaReviewSelection *selection,
                                              float position_seconds) {
    if (!valid_selection(selection) || !isfinite(position_seconds)) {
        return false;
    }
    selection->start_seconds = clamp_position(selection, position_seconds);
    selection->has_start = true;
    if (selection->has_end &&
        selection->end_seconds <= selection->start_seconds + SELECTION_EPSILON) {
        selection->end_seconds = 0.0F;
        selection->has_end = false;
    }
    return true;
}

bool eidolon_vrma_review_selection_mark_end(EidolonVrmaReviewSelection *selection,
                                            float position_seconds) {
    if (!valid_selection(selection) || !selection->has_start || !isfinite(position_seconds)) {
        return false;
    }
    const float end_seconds = clamp_position(selection, position_seconds);
    if (end_seconds <= selection->start_seconds + SELECTION_EPSILON) {
        return false;
    }
    selection->end_seconds = end_seconds;
    selection->has_end = true;
    return true;
}

bool eidolon_vrma_review_selection_range(const EidolonVrmaReviewSelection *selection,
                                         float *start_seconds, float *end_seconds) {
    if (!valid_selection(selection) || !selection->has_start || !selection->has_end ||
        selection->end_seconds <= selection->start_seconds + SELECTION_EPSILON) {
        return false;
    }
    if (start_seconds != NULL) {
        *start_seconds = selection->start_seconds;
    }
    if (end_seconds != NULL) {
        *end_seconds = selection->end_seconds;
    }
    return true;
}

float eidolon_vrma_review_selection_seek(const EidolonVrmaReviewSelection *selection,
                                         float position_seconds, float delta_seconds) {
    if (!valid_selection(selection) || !isfinite(position_seconds) || !isfinite(delta_seconds)) {
        return 0.0F;
    }
    return clamp_position(selection, position_seconds + delta_seconds);
}

uint64_t eidolon_vrma_review_selection_milliseconds(float seconds) {
    if (!isfinite(seconds) || seconds <= 0.0F) {
        return 0U;
    }
    const double milliseconds = (double)seconds * 1000.0;
    if (milliseconds >= (double)UINT64_MAX) {
        return UINT64_MAX;
    }
    return (uint64_t)llround(milliseconds);
}
