#ifndef EIDOLON_EPR_TEMPORAL_H
#define EIDOLON_EPR_TEMPORAL_H

#include "epr/performance_intent.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_TEMPORAL_NODE_CAPACITY 48U
#define EIDOLON_EPR_TEMPORAL_CONSTRAINT_CAPACITY 96U

typedef uint64_t EidolonEprAnchorId;

typedef struct EidolonEprTemporalNode {
    EidolonEprAnchorId id;
    EidolonEprTick tick;
    bool tick_known;
    bool observed;
} EidolonEprTemporalNode;

typedef struct EidolonEprTemporalConstraint {
    EidolonEprAnchorId from;
    EidolonEprAnchorId to;
    EidolonEprTick minimum;
    EidolonEprTick maximum;
} EidolonEprTemporalConstraint;

typedef struct EidolonEprTemporalNetwork {
    EidolonEprTemporalNode nodes[EIDOLON_EPR_TEMPORAL_NODE_CAPACITY];
    size_t node_count;
    EidolonEprTemporalConstraint constraints[EIDOLON_EPR_TEMPORAL_CONSTRAINT_CAPACITY];
    size_t constraint_count;
} EidolonEprTemporalNetwork;

void eidolon_epr_temporal_init(EidolonEprTemporalNetwork *network);
bool eidolon_epr_temporal_add_node(EidolonEprTemporalNetwork *network, EidolonEprAnchorId id);
bool eidolon_epr_temporal_set_tick(EidolonEprTemporalNetwork *network, EidolonEprAnchorId id,
                                   EidolonEprTick tick, bool observed);
bool eidolon_epr_temporal_add_constraint(EidolonEprTemporalNetwork *network,
                                         EidolonEprTemporalConstraint constraint);
bool eidolon_epr_temporal_validate(const EidolonEprTemporalNetwork *network);
const EidolonEprTemporalNode *
eidolon_epr_temporal_find(const EidolonEprTemporalNetwork *network, EidolonEprAnchorId id);

#endif
