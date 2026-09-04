#ifndef EIDOLON_VRMA_REVIEW_SELECTION_H
#define EIDOLON_VRMA_REVIEW_SELECTION_H

#include <stdbool.h>
#include <stdint.h>

#define EIDOLON_VRMA_REVIEW_SELECTION_VERSION 1U

typedef struct EidolonVrmaReviewSelection {
    uint32_t version;
    float duration_seconds;
    float start_seconds;
    float end_seconds;
    bool has_start;
    bool has_end;
} EidolonVrmaReviewSelection;

bool eidolon_vrma_review_selection_init(EidolonVrmaReviewSelection *selection,
                                        float duration_seconds);
bool eidolon_vrma_review_selection_mark_start(EidolonVrmaReviewSelection *selection,
                                              float position_seconds);
bool eidolon_vrma_review_selection_mark_end(EidolonVrmaReviewSelection *selection,
                                            float position_seconds);
bool eidolon_vrma_review_selection_range(const EidolonVrmaReviewSelection *selection,
                                         float *start_seconds, float *end_seconds);
float eidolon_vrma_review_selection_seek(const EidolonVrmaReviewSelection *selection,
                                         float position_seconds, float delta_seconds);
uint64_t eidolon_vrma_review_selection_milliseconds(float seconds);

#endif
