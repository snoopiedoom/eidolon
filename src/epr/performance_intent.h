#ifndef EIDOLON_EPR_PERFORMANCE_INTENT_H
#define EIDOLON_EPR_PERFORMANCE_INTENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_INTENT_VERSION 1U
#define EIDOLON_EPR_BEAT_CAPACITY 8U

typedef uint64_t EidolonEprOpaqueId;
typedef int64_t EidolonEprTick;

typedef enum EidolonEprOperationalMode {
    EIDOLON_EPR_MODE_ABSENT = 0,
    EIDOLON_EPR_MODE_LISTENING,
    EIDOLON_EPR_MODE_THINKING,
    EIDOLON_EPR_MODE_RESPONDING,
    EIDOLON_EPR_MODE_INTERRUPTED,
    EIDOLON_EPR_MODE_COMPLETED,
    EIDOLON_EPR_MODE_ERRORED
} EidolonEprOperationalMode;

typedef enum EidolonEprBeatKind {
    EIDOLON_EPR_BEAT_NONE = 0,
    EIDOLON_EPR_BEAT_ACCENT,
    EIDOLON_EPR_BEAT_CONTRAST,
    EIDOLON_EPR_BEAT_LANDING,
    EIDOLON_EPR_BEAT_QUESTION
} EidolonEprBeatKind;

typedef enum EidolonEprEvidenceStability {
    EIDOLON_EPR_EVIDENCE_PROVISIONAL = 0,
    EIDOLON_EPR_EVIDENCE_STABLE_PREFIX,
    EIDOLON_EPR_EVIDENCE_FINAL
} EidolonEprEvidenceStability;

typedef struct EidolonEprProvenance {
    EidolonEprOpaqueId source;
    EidolonEprOpaqueId session;
    EidolonEprOpaqueId turn;
    EidolonEprOpaqueId response;
    EidolonEprOpaqueId message;
    EidolonEprOpaqueId interaction;
    uint64_t truth_revision;
} EidolonEprProvenance;

typedef struct EidolonEprSemanticBeat {
    EidolonEprOpaqueId id;
    EidolonEprBeatKind kind;
    EidolonEprEvidenceStability stability;
    size_t source_start;
    size_t source_end;
    EidolonEprTick anchor_tick;
} EidolonEprSemanticBeat;

typedef struct EidolonPerformanceIntent {
    uint32_t version;
    uint64_t revision;
    uint64_t predecessor_revision;
    EidolonEprTick observed_tick;
    EidolonEprOperationalMode mode;
    uint16_t urgency;
    uint16_t continuity;
    int16_t affect_milli[6];
    EidolonEprProvenance provenance;
    bool has_performance_lease;
    EidolonEprOpaqueId lease_source;
    EidolonEprOpaqueId lease_session;
    EidolonEprSemanticBeat beats[EIDOLON_EPR_BEAT_CAPACITY];
    size_t beat_count;
} EidolonPerformanceIntent;

bool eidolon_epr_intent_validate(const EidolonPerformanceIntent *intent);

#endif
