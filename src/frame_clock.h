#ifndef EIDOLON_FRAME_CLOCK_H
#define EIDOLON_FRAME_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct EidolonFrameClock {
    uint64_t interval_ns;
    uint64_t next_frame_ns;
    uint64_t previous_frame_ns;
} EidolonFrameClock;

uint64_t eidolon_frame_interval_ns(int refresh_numerator, int refresh_denominator,
                                   float refresh_rate);
uint64_t eidolon_frame_policy_interval_ns(uint64_t display_interval_ns, bool vsync_requested,
                                          bool vsync_active, int fps_limit, bool *uncapped,
                                          bool *software_paced);
void eidolon_frame_clock_init(EidolonFrameClock *clock, uint64_t now_ns, uint64_t interval_ns);
void eidolon_frame_clock_set_interval(EidolonFrameClock *clock, uint64_t now_ns,
                                      uint64_t interval_ns);
int32_t eidolon_frame_clock_wait_ms(const EidolonFrameClock *clock, uint64_t now_ns);
bool eidolon_frame_clock_due(const EidolonFrameClock *clock, uint64_t now_ns);
float eidolon_frame_clock_begin(EidolonFrameClock *clock, uint64_t now_ns);
void eidolon_frame_clock_finish(EidolonFrameClock *clock, uint64_t completed_ns);

#endif
