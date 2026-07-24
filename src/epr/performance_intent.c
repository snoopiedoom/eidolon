#include "epr/performance_intent.h"

static bool affect_valid(const EidolonPerformanceIntent *intent) {
    for (size_t index = 0; index < 6U; ++index) {
        if (intent->affect_milli[index] < -1000 || intent->affect_milli[index] > 1000) {
            return false;
        }
    }
    return true;
}

bool eidolon_epr_intent_validate(const EidolonPerformanceIntent *intent) {
    if (intent == NULL || intent->version != EIDOLON_EPR_INTENT_VERSION ||
        intent->revision == 0U || intent->beat_count > EIDOLON_EPR_BEAT_CAPACITY ||
        intent->mode < EIDOLON_EPR_MODE_ABSENT || intent->mode > EIDOLON_EPR_MODE_ERRORED ||
        intent->urgency > 1000U || intent->continuity > 1000U || !affect_valid(intent)) {
        return false;
    }
    if (intent->mode != EIDOLON_EPR_MODE_ABSENT) {
        if (!intent->has_performance_lease || intent->lease_source == 0U ||
            intent->lease_session == 0U || intent->lease_source != intent->provenance.source ||
            intent->lease_session != intent->provenance.session) {
            return false;
        }
    }
    for (size_t index = 0; index < intent->beat_count; ++index) {
        const EidolonEprSemanticBeat *beat = &intent->beats[index];
        if (beat->id == 0U || beat->kind <= EIDOLON_EPR_BEAT_NONE ||
            beat->kind > EIDOLON_EPR_BEAT_QUESTION ||
            beat->stability < EIDOLON_EPR_EVIDENCE_PROVISIONAL ||
            beat->stability > EIDOLON_EPR_EVIDENCE_FINAL ||
            beat->source_end < beat->source_start) {
            return false;
        }
        for (size_t prior = 0; prior < index; ++prior) {
            const EidolonEprSemanticBeat *other = &intent->beats[prior];
            if (other->id == beat->id &&
                (other->kind != beat->kind || other->source_start != beat->source_start ||
                 other->source_end != beat->source_end ||
                 other->anchor_tick != beat->anchor_tick)) {
                return false;
            }
        }
    }
    return true;
}
