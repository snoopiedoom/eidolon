#include "ik.h"

#include <assert.h>
#include <math.h>

static bool near(float actual, float expected) { return fabsf(actual - expected) < 0.0001F; }

int main(void) {
    EidolonIkTwoBoneInput input = {
        .root = {0.0F, 0.0F, 0.0F},
        .target = {1.0F, 0.0F, 0.0F},
        .pole = {0.0F, 1.0F, 0.0F},
        .fallback_direction = {1.0F, 0.0F, 0.0F},
        .upper_length = 1.0F,
        .lower_length = 1.0F,
    };
    EidolonIkTwoBoneSolution solution;
    assert(eidolon_ik_solve_two_bone(&input, &solution));
    assert(near(solution.mid[0], 0.5F));
    assert(near(solution.mid[1], 0.8660254F));
    assert(near(solution.end[0], 1.0F));
    assert(solution.target_reached);

    input.pole[1] = -1.0F;
    assert(eidolon_ik_solve_two_bone(&input, &solution));
    assert(solution.mid[1] < -0.86F);

    input.target[0] = 1.9F;
    input.soften_ratio = 0.2F;
    assert(eidolon_ik_solve_two_bone(&input, &solution));
    assert(solution.solved_distance < input.target[0]);
    assert(!solution.target_reached);

    input.target[0] = 4.0F;
    input.pole[1] = 1.0F;
    input.soften_ratio = 0.0F;
    assert(eidolon_ik_solve_two_bone(&input, &solution));
    assert(near(solution.solved_distance, 2.0F));
    assert(!solution.target_reached);

    input.target[0] = 0.0F;
    assert(eidolon_ik_solve_two_bone(&input, &solution));
    assert(isfinite(solution.mid[0]) && isfinite(solution.mid[1]));

    input.target[0] = NAN;
    assert(!eidolon_ik_solve_two_bone(&input, &solution));
    input.target[0] = 1.0F;

    input.upper_length = 0.0F;
    assert(!eidolon_ik_solve_two_bone(&input, &solution));
    return 0;
}
