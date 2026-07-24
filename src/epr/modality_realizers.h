#ifndef EIDOLON_EPR_MODALITY_REALIZERS_H
#define EIDOLON_EPR_MODALITY_REALIZERS_H

#include "epr/canonical_control.h"
#include "epr/realization_program.h"

#include <stdbool.h>
#include <stdint.h>

void eidolon_epr_realize_posture(const EidolonEprBodyProfile *body,
                                 const EidolonRealizationProgram *program, EidolonEprTick tick,
                                 EidolonCanonicalControl *candidate, float right_arm_target[3]);
void eidolon_epr_realize_idle(uint64_t seed, const EidolonRealizationProgram *program,
                              EidolonEprTick tick, EidolonCanonicalControl *candidate);
bool eidolon_epr_realize_gaze(const EidolonEprBodyProfile *body,
                              const EidolonRealizationProgram *program, EidolonEprTick tick,
                              EidolonCanonicalControl *candidate);
void eidolon_epr_realize_right_arm(const EidolonEprBodyProfile *body,
                                   const EidolonRealizationProgram *program, EidolonEprTick tick,
                                   const EidolonCanonicalControl *settle_start,
                                   const float posture_target[3],
                                   EidolonCanonicalControl *candidate);
bool eidolon_epr_realize_expression(const EidolonEprBodyProfile *body,
                                    const EidolonRealizationProgram *program,
                                    EidolonCanonicalControl *candidate);

#endif
