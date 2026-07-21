#include "portrait_motion.h"

#include <assert.h>
#include <math.h>

int main(void) {
    EidolonPortraitSpring spring;
    eidolon_portrait_spring_reset(&spring, 1000U);
    eidolon_portrait_spring_impulse(&spring, 70.0F, -110.0F, 0.14F, 6.0F);
    eidolon_portrait_spring_update(&spring, 1016U);
    assert(spring.x > 0.0F);
    assert(spring.y < 0.0F);
    assert(spring.scale > 0.0F);
    assert(spring.angle > 0.0F);

    eidolon_portrait_spring_impulse(&spring, 1000.0F, -1000.0F, 10.0F, 100.0F);
    eidolon_portrait_spring_update(&spring, 1116U);
    assert(fabsf(spring.x) <= 8.0F);
    assert(fabsf(spring.y) <= 11.0F);
    assert(fabsf(spring.scale) <= 0.022F);
    assert(fabsf(spring.angle) <= 0.85F);

    for (uint64_t now_ms = 1132U; now_ms <= 11132U; now_ms += 16U) {
        eidolon_portrait_spring_update(&spring, now_ms);
    }
    assert(fabsf(spring.x) < 0.001F);
    assert(fabsf(spring.y) < 0.001F);
    assert(fabsf(spring.scale) < 0.0001F);
    assert(fabsf(spring.angle) < 0.001F);
    return 0;
}
