#include "vrma_review_selection.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool near(float left, float right) { return fabsf(left - right) < 1.0e-6F; }

int main(void) {
    EidolonVrmaReviewSelection selection;
    float start = 0.0F;
    float end = 0.0F;

    assert(!eidolon_vrma_review_selection_init(NULL, 10.0F));
    assert(!eidolon_vrma_review_selection_init(&selection, 0.0F));
    assert(eidolon_vrma_review_selection_init(&selection, 10.0F));
    assert(!eidolon_vrma_review_selection_range(&selection, &start, &end));
    assert(!eidolon_vrma_review_selection_mark_end(&selection, 4.0F));

    assert(eidolon_vrma_review_selection_mark_start(&selection, -2.0F));
    assert(near(selection.start_seconds, 0.0F));
    assert(!eidolon_vrma_review_selection_mark_end(&selection, 0.0F));
    assert(eidolon_vrma_review_selection_mark_end(&selection, 4.25F));
    assert(eidolon_vrma_review_selection_range(&selection, &start, &end));
    assert(near(start, 0.0F));
    assert(near(end, 4.25F));

    assert(eidolon_vrma_review_selection_mark_start(&selection, 6.0F));
    assert(!selection.has_end);
    assert(eidolon_vrma_review_selection_mark_end(&selection, 20.0F));
    assert(eidolon_vrma_review_selection_range(&selection, &start, &end));
    assert(near(start, 6.0F));
    assert(near(end, 10.0F));

    assert(near(eidolon_vrma_review_selection_seek(&selection, 2.0F, -3.0F), 0.0F));
    assert(near(eidolon_vrma_review_selection_seek(&selection, 9.0F, 3.0F), 10.0F));
    assert(near(eidolon_vrma_review_selection_seek(&selection, 4.0F, 0.25F), 4.25F));
    assert(eidolon_vrma_review_selection_milliseconds(4.2504F) == 4250U);
    assert(eidolon_vrma_review_selection_milliseconds(4.2506F) == 4251U);

    puts("vrma review selection test passed");
    return 0;
}
