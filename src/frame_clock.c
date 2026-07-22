#include "frame_clock.h"

#include <SDL3/SDL.h>

#include <limits.h>

#define NANOSECONDS_PER_SECOND UINT64_C(1000000000)
#define NANOSECONDS_PER_MILLISECOND UINT64_C(1000000)
#define DEFAULT_REFRESH_RATE 60.0F
#define MAX_REFRESH_RATE 1000.0F
#define MAX_SIMULATION_DELTA_SECONDS 0.1

uint64_t eidolon_frame_interval_ns(int refresh_numerator, int refresh_denominator,
                                   float refresh_rate) {
    if (refresh_numerator > 0 && refresh_denominator > 0) {
        const uint64_t numerator = (uint64_t)refresh_numerator;
        const uint64_t scaled = NANOSECONDS_PER_SECOND * (uint64_t)refresh_denominator;
        return (scaled + numerator / 2U) / numerator;
    }
    const double rate = refresh_rate >= 1.0F && refresh_rate <= MAX_REFRESH_RATE
                            ? (double)refresh_rate
                            : (double)DEFAULT_REFRESH_RATE;
    return (uint64_t)((double)NANOSECONDS_PER_SECOND / rate + 0.5);
}

uint64_t eidolon_frame_policy_interval_ns(uint64_t display_interval_ns, bool vsync_requested,
                                          bool vsync_active, int fps_limit, bool *uncapped,
                                          bool *software_paced) {
    const uint64_t display_interval = display_interval_ns > 0U
                                          ? display_interval_ns
                                          : eidolon_frame_interval_ns(0, 0, DEFAULT_REFRESH_RATE);
    const bool vsync_bound = vsync_requested || vsync_active;
    if (!vsync_bound && fps_limit <= 0) {
        if (uncapped != NULL) {
            *uncapped = true;
        }
        if (software_paced != NULL) {
            *software_paced = false;
        }
        return display_interval;
    }
    if (uncapped != NULL) {
        *uncapped = false;
    }
    if (fps_limit <= 0) {
        if (software_paced != NULL) {
            *software_paced = !vsync_active;
        }
        return display_interval;
    }
    const uint64_t limit_interval = eidolon_frame_interval_ns(fps_limit, 1, 0.0F);
    const uint64_t effective_interval =
        vsync_bound ? SDL_max(display_interval, limit_interval) : limit_interval;
    if (software_paced != NULL) {
        *software_paced = !vsync_active || effective_interval > display_interval;
    }
    return effective_interval;
}

void eidolon_frame_clock_init(EidolonFrameClock *clock, uint64_t now_ns, uint64_t interval_ns) {
    if (clock == NULL) {
        return;
    }
    clock->interval_ns = interval_ns > 0U ? interval_ns : 1U;
    clock->next_frame_ns = now_ns;
    clock->previous_frame_ns = 0U;
}

void eidolon_frame_clock_set_interval(EidolonFrameClock *clock, uint64_t now_ns,
                                      uint64_t interval_ns) {
    if (clock == NULL) {
        return;
    }
    clock->interval_ns = interval_ns > 0U ? interval_ns : 1U;
    clock->next_frame_ns = now_ns;
}

int32_t eidolon_frame_clock_wait_ms(const EidolonFrameClock *clock, uint64_t now_ns) {
    if (clock == NULL || now_ns >= clock->next_frame_ns) {
        return 0;
    }
    const uint64_t remaining_ns = clock->next_frame_ns - now_ns;
    const uint64_t wait_ms =
        (remaining_ns + NANOSECONDS_PER_MILLISECOND - 1U) / NANOSECONDS_PER_MILLISECOND;
    return wait_ms > (uint64_t)INT32_MAX ? INT32_MAX : (int32_t)wait_ms;
}

bool eidolon_frame_clock_due(const EidolonFrameClock *clock, uint64_t now_ns) {
    return clock == NULL || now_ns >= clock->next_frame_ns;
}

float eidolon_frame_clock_begin(EidolonFrameClock *clock, uint64_t now_ns) {
    if (clock == NULL) {
        return 0.0F;
    }
    double delta_seconds = (double)clock->interval_ns / (double)NANOSECONDS_PER_SECOND;
    if (clock->previous_frame_ns > 0U && now_ns >= clock->previous_frame_ns) {
        delta_seconds =
            (double)(now_ns - clock->previous_frame_ns) / (double)NANOSECONDS_PER_SECOND;
    }
    clock->previous_frame_ns = now_ns;
    if (delta_seconds > MAX_SIMULATION_DELTA_SECONDS) {
        delta_seconds = MAX_SIMULATION_DELTA_SECONDS;
    }
    return (float)delta_seconds;
}

void eidolon_frame_clock_finish(EidolonFrameClock *clock, uint64_t completed_ns) {
    if (clock == NULL) {
        return;
    }
    clock->next_frame_ns += clock->interval_ns;
    if (completed_ns >= clock->next_frame_ns + clock->interval_ns) {
        clock->next_frame_ns = completed_ns + clock->interval_ns;
    }
}
