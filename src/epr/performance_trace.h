#ifndef EIDOLON_EPR_PERFORMANCE_TRACE_H
#define EIDOLON_EPR_PERFORMANCE_TRACE_H

#include "epr/performance_intent.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_TRACE_VERSION 1U
#define EIDOLON_EPR_TRACE_CAPACITY 512U

typedef enum EidolonEprTraceEvent {
    EIDOLON_EPR_TRACE_INTENT_ACCEPTED = 0,
    EIDOLON_EPR_TRACE_INTENT_REJECTED,
    EIDOLON_EPR_TRACE_PLAN_PUBLISHED,
    EIDOLON_EPR_TRACE_PLAN_REJECTED,
    EIDOLON_EPR_TRACE_BEHAVIOR_TRANSITION,
    EIDOLON_EPR_TRACE_ANCHOR_OBSERVED,
    EIDOLON_EPR_TRACE_RESOURCE_GRANTED,
    EIDOLON_EPR_TRACE_RESOURCE_DENIED,
    EIDOLON_EPR_TRACE_RESOURCE_TRANSFERRED,
    EIDOLON_EPR_TRACE_RESOURCE_RELEASED,
    EIDOLON_EPR_TRACE_REALIZER_SELECTED,
    EIDOLON_EPR_TRACE_REALIZER_FAILED,
    EIDOLON_EPR_TRACE_SOLVE_COMMITTED,
    EIDOLON_EPR_TRACE_SOLVE_REJECTED,
    EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED,
    EIDOLON_EPR_TRACE_CONTROL_PUBLISHED,
    EIDOLON_EPR_TRACE_PROJECTION_COMMITTED,
    EIDOLON_EPR_TRACE_PROJECTION_REJECTED
} EidolonEprTraceEvent;

typedef enum EidolonEprTraceReason {
    EIDOLON_EPR_REASON_NONE = 0,
    EIDOLON_EPR_REASON_STALE_REVISION,
    EIDOLON_EPR_REASON_INVALID_INTENT,
    EIDOLON_EPR_REASON_TEMPORAL_CONFLICT,
    EIDOLON_EPR_REASON_CAPACITY,
    EIDOLON_EPR_REASON_SELECTED,
    EIDOLON_EPR_REASON_PREEMPTED,
    EIDOLON_EPR_REASON_INTERRUPTED,
    EIDOLON_EPR_REASON_REVISED,
    EIDOLON_EPR_REASON_COMPLETED,
    EIDOLON_EPR_REASON_SETTLED,
    EIDOLON_EPR_REASON_OPTIONAL_MISSING,
    EIDOLON_EPR_REASON_INVALID_CANDIDATE,
    EIDOLON_EPR_REASON_INJECTED_FAILURE
} EidolonEprTraceReason;

typedef struct EidolonEprTraceRecord {
    uint32_t version;
    uint64_t sequence;
    EidolonEprTick tick;
    uint64_t intent_revision;
    uint64_t plan_generation;
    EidolonEprTraceEvent event;
    EidolonEprTraceReason reason;
    EidolonEprProvenance provenance;
    EidolonEprOpaqueId behavior;
    EidolonEprOpaqueId cause;
    uint32_t resource;
    uint64_t value;
    uint64_t control_hash;
} EidolonEprTraceRecord;

typedef struct EidolonEprTrace {
    EidolonEprTraceRecord records[EIDOLON_EPR_TRACE_CAPACITY];
    size_t start;
    size_t count;
    uint64_t next_sequence;
    uint64_t dropped;
} EidolonEprTrace;

void eidolon_epr_trace_init(EidolonEprTrace *trace);
bool eidolon_epr_trace_emit(EidolonEprTrace *trace, EidolonEprTraceRecord record);
const EidolonEprTraceRecord *eidolon_epr_trace_record(const EidolonEprTrace *trace, size_t index);
uint64_t eidolon_epr_trace_hash(const EidolonEprTrace *trace);
const char *eidolon_epr_trace_event_name(EidolonEprTraceEvent event);

#endif
