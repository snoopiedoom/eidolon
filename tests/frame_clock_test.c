#include "frame_clock.h"

#include <assert.h>
#include <math.h>

int main(void) {
    const uint64_t sixty_hz = eidolon_frame_interval_ns(60, 1, 0.0F);
    const uint64_t ntsc_hz = eidolon_frame_interval_ns(60000, 1001, 0.0F);
    const uint64_t one_forty_four_hz = eidolon_frame_interval_ns(0, 0, 144.0F);
    assert(sixty_hz == UINT64_C(16666667));
    assert(ntsc_hz == UINT64_C(16683333));
    assert(one_forty_four_hz == UINT64_C(6944444));
    assert(eidolon_frame_interval_ns(0, 0, 0.0F) == sixty_hz);

    bool uncapped = false;
    bool software_paced = false;
    assert(eidolon_frame_policy_interval_ns(sixty_hz, true, true, 0, &uncapped, &software_paced) ==
           sixty_hz);
    assert(!uncapped);
    assert(!software_paced);
    assert(eidolon_frame_policy_interval_ns(sixty_hz, true, true, 30, &uncapped, &software_paced) ==
           eidolon_frame_interval_ns(30, 1, 0.0F));
    assert(!uncapped);
    assert(software_paced);
    assert(eidolon_frame_policy_interval_ns(sixty_hz, true, true, 120, &uncapped,
                                            &software_paced) == sixty_hz);
    assert(!uncapped);
    assert(!software_paced);
    assert(
        eidolon_frame_policy_interval_ns(sixty_hz, false, false, 120, &uncapped, &software_paced) ==
        eidolon_frame_interval_ns(120, 1, 0.0F));
    assert(!uncapped);
    assert(software_paced);
    assert(eidolon_frame_policy_interval_ns(sixty_hz, false, false, 0, &uncapped,
                                            &software_paced) == sixty_hz);
    assert(uncapped);
    assert(!software_paced);
    assert(eidolon_frame_policy_interval_ns(sixty_hz, true, false, 0, &uncapped, &software_paced) ==
           sixty_hz);
    assert(!uncapped);
    assert(software_paced);

    EidolonFrameClock clock;
    eidolon_frame_clock_init(&clock, 0U, sixty_hz);
    assert(eidolon_frame_clock_due(&clock, 0U));
    assert(fabsf(eidolon_frame_clock_begin(&clock, 0U) - 1.0F / 60.0F) < 0.00001F);

    /* A VSync present completing just after the next deadline must not add a second wait. */
    eidolon_frame_clock_finish(&clock, UINT64_C(16700000));
    assert(eidolon_frame_clock_due(&clock, UINT64_C(16700000)));

    (void)eidolon_frame_clock_begin(&clock, UINT64_C(16700000));
    eidolon_frame_clock_finish(&clock, UINT64_C(16800000));
    assert(!eidolon_frame_clock_due(&clock, UINT64_C(16800000)));
    assert(eidolon_frame_clock_wait_ms(&clock, UINT64_C(16800000)) == 17);

    /* A long stall skips missed deadlines instead of producing catch-up bursts. */
    eidolon_frame_clock_finish(&clock, UINT64_C(500000000));
    assert(!eidolon_frame_clock_due(&clock, UINT64_C(500000000)));
    assert(clock.next_frame_ns == UINT64_C(500000000) + sixty_hz);
    assert(eidolon_frame_clock_begin(&clock, UINT64_C(500000000)) == 0.1F);

    eidolon_frame_clock_set_interval(&clock, UINT64_C(600000000), one_forty_four_hz);
    assert(clock.interval_ns == one_forty_four_hz);
    assert(eidolon_frame_clock_due(&clock, UINT64_C(600000000)));
    return 0;
}
