#include "epr/performance_trace.h"

#include <string.h>

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = data;
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void eidolon_epr_trace_init(EidolonEprTrace *trace) {
    if (trace == NULL) {
        return;
    }
    memset(trace, 0, sizeof(*trace));
    trace->next_sequence = 1U;
}

bool eidolon_epr_trace_emit(EidolonEprTrace *trace, EidolonEprTraceRecord record) {
    size_t insert;
    if (trace == NULL) {
        return false;
    }
    if (trace->count < EIDOLON_EPR_TRACE_CAPACITY) {
        insert = (trace->start + trace->count) % EIDOLON_EPR_TRACE_CAPACITY;
        trace->count += 1U;
    } else {
        insert = trace->start;
        trace->start = (trace->start + 1U) % EIDOLON_EPR_TRACE_CAPACITY;
        trace->dropped += 1U;
    }
    record.version = EIDOLON_EPR_TRACE_VERSION;
    record.sequence = trace->next_sequence;
    trace->next_sequence += 1U;
    trace->records[insert] = record;
    return true;
}

const EidolonEprTraceRecord *eidolon_epr_trace_record(const EidolonEprTrace *trace, size_t index) {
    if (trace == NULL || index >= trace->count) {
        return NULL;
    }
    return &trace->records[(trace->start + index) % EIDOLON_EPR_TRACE_CAPACITY];
}

uint64_t eidolon_epr_trace_hash(const EidolonEprTrace *trace) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (trace == NULL) {
        return hash;
    }
    hash = hash_bytes(hash, &trace->count, sizeof(trace->count));
    hash = hash_bytes(hash, &trace->dropped, sizeof(trace->dropped));
    for (size_t index = 0; index < trace->count; ++index) {
        const EidolonEprTraceRecord *record = eidolon_epr_trace_record(trace, index);
        hash = hash_bytes(hash, &record->version, sizeof(record->version));
        hash = hash_bytes(hash, &record->sequence, sizeof(record->sequence));
        hash = hash_bytes(hash, &record->tick, sizeof(record->tick));
        hash = hash_bytes(hash, &record->intent_revision, sizeof(record->intent_revision));
        hash = hash_bytes(hash, &record->plan_generation, sizeof(record->plan_generation));
        hash = hash_bytes(hash, &record->event, sizeof(record->event));
        hash = hash_bytes(hash, &record->reason, sizeof(record->reason));
        hash = hash_bytes(hash, &record->provenance.source, sizeof(record->provenance.source));
        hash = hash_bytes(hash, &record->provenance.session, sizeof(record->provenance.session));
        hash = hash_bytes(hash, &record->provenance.turn, sizeof(record->provenance.turn));
        hash = hash_bytes(hash, &record->provenance.response, sizeof(record->provenance.response));
        hash = hash_bytes(hash, &record->provenance.message, sizeof(record->provenance.message));
        hash = hash_bytes(hash, &record->provenance.interaction,
                          sizeof(record->provenance.interaction));
        hash = hash_bytes(hash, &record->provenance.truth_revision,
                          sizeof(record->provenance.truth_revision));
        hash = hash_bytes(hash, &record->behavior, sizeof(record->behavior));
        hash = hash_bytes(hash, &record->cause, sizeof(record->cause));
        hash = hash_bytes(hash, &record->resource, sizeof(record->resource));
        hash = hash_bytes(hash, &record->value, sizeof(record->value));
        hash = hash_bytes(hash, &record->control_hash, sizeof(record->control_hash));
    }
    return hash;
}

const char *eidolon_epr_trace_event_name(EidolonEprTraceEvent event) {
    static const char *const names[] = {
        "intent.accepted",      "intent.rejected",     "plan.published",      "plan.rejected",
        "behavior.transition",  "anchor.observed",     "resource.granted",    "resource.denied",
        "resource.transferred", "resource.released",   "realizer.selected",   "realizer.failed",
        "solve.committed",      "solve.rejected",      "capability.degraded", "control.published",
        "projection.committed", "projection.rejected",
    };
    if (event < EIDOLON_EPR_TRACE_INTENT_ACCEPTED ||
        event > EIDOLON_EPR_TRACE_PROJECTION_REJECTED) {
        return "unknown";
    }
    return names[(size_t)event];
}
