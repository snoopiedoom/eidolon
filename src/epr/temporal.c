#include "epr/temporal.h"

#include <limits.h>
#include <string.h>

#define EIDOLON_EPR_TEMPORAL_INFINITY (INT64_MAX / 4)

static size_t node_index(const EidolonEprTemporalNetwork *network, EidolonEprAnchorId id) {
    for (size_t index = 0; index < network->node_count; ++index) {
        if (network->nodes[index].id == id) {
            return index;
        }
    }
    return EIDOLON_EPR_TEMPORAL_NODE_CAPACITY;
}

void eidolon_epr_temporal_init(EidolonEprTemporalNetwork *network) {
    if (network != NULL) {
        memset(network, 0, sizeof(*network));
    }
}

bool eidolon_epr_temporal_add_node(EidolonEprTemporalNetwork *network, EidolonEprAnchorId id) {
    if (network == NULL || id == 0U) {
        return false;
    }
    if (node_index(network, id) < network->node_count) {
        return true;
    }
    if (network->node_count >= EIDOLON_EPR_TEMPORAL_NODE_CAPACITY) {
        return false;
    }
    network->nodes[network->node_count].id = id;
    network->node_count += 1U;
    return true;
}

bool eidolon_epr_temporal_set_tick(EidolonEprTemporalNetwork *network, EidolonEprAnchorId id,
                                   EidolonEprTick tick, bool observed) {
    EidolonEprTemporalNetwork candidate;
    size_t index;
    if (network == NULL || id == 0U) {
        return false;
    }
    candidate = *network;
    index = node_index(&candidate, id);
    if (index >= candidate.node_count) {
        if (!eidolon_epr_temporal_add_node(&candidate, id)) {
            return false;
        }
        index = candidate.node_count - 1U;
    }
    if (candidate.nodes[index].observed && candidate.nodes[index].tick_known &&
        candidate.nodes[index].tick != tick) {
        return false;
    }
    candidate.nodes[index].tick = tick;
    candidate.nodes[index].tick_known = true;
    candidate.nodes[index].observed = candidate.nodes[index].observed || observed;
    if (!eidolon_epr_temporal_validate(&candidate)) {
        return false;
    }
    *network = candidate;
    return true;
}

bool eidolon_epr_temporal_add_constraint(EidolonEprTemporalNetwork *network,
                                         EidolonEprTemporalConstraint constraint) {
    EidolonEprTemporalNetwork candidate;
    if (network == NULL || constraint.from == 0U || constraint.to == 0U ||
        constraint.minimum > constraint.maximum ||
        network->constraint_count >= EIDOLON_EPR_TEMPORAL_CONSTRAINT_CAPACITY) {
        return false;
    }
    candidate = *network;
    if (!eidolon_epr_temporal_add_node(&candidate, constraint.from) ||
        !eidolon_epr_temporal_add_node(&candidate, constraint.to)) {
        return false;
    }
    candidate.constraints[candidate.constraint_count] = constraint;
    candidate.constraint_count += 1U;
    if (!eidolon_epr_temporal_validate(&candidate)) {
        return false;
    }
    *network = candidate;
    return true;
}

bool eidolon_epr_temporal_validate(const EidolonEprTemporalNetwork *network) {
    EidolonEprTick distance[EIDOLON_EPR_TEMPORAL_NODE_CAPACITY]
                           [EIDOLON_EPR_TEMPORAL_NODE_CAPACITY];
    if (network == NULL || network->node_count > EIDOLON_EPR_TEMPORAL_NODE_CAPACITY ||
        network->constraint_count > EIDOLON_EPR_TEMPORAL_CONSTRAINT_CAPACITY) {
        return false;
    }
    for (size_t row = 0; row < network->node_count; ++row) {
        for (size_t column = 0; column < network->node_count; ++column) {
            distance[row][column] = row == column ? 0 : EIDOLON_EPR_TEMPORAL_INFINITY;
        }
    }
    for (size_t index = 0; index < network->constraint_count; ++index) {
        const EidolonEprTemporalConstraint *constraint = &network->constraints[index];
        const size_t from = node_index(network, constraint->from);
        const size_t to = node_index(network, constraint->to);
        if (from >= network->node_count || to >= network->node_count) {
            return false;
        }
        if (constraint->maximum < distance[from][to]) {
            distance[from][to] = constraint->maximum;
        }
        if (-constraint->minimum < distance[to][from]) {
            distance[to][from] = -constraint->minimum;
        }
        if (network->nodes[from].tick_known && network->nodes[to].tick_known) {
            const EidolonEprTick delta =
                network->nodes[to].tick - network->nodes[from].tick;
            if (delta < constraint->minimum || delta > constraint->maximum) {
                return false;
            }
        }
    }
    for (size_t pivot = 0; pivot < network->node_count; ++pivot) {
        for (size_t from = 0; from < network->node_count; ++from) {
            if (distance[from][pivot] == EIDOLON_EPR_TEMPORAL_INFINITY) {
                continue;
            }
            for (size_t to = 0; to < network->node_count; ++to) {
                EidolonEprTick through;
                if (distance[pivot][to] == EIDOLON_EPR_TEMPORAL_INFINITY) {
                    continue;
                }
                through = distance[from][pivot] + distance[pivot][to];
                if (through < distance[from][to]) {
                    distance[from][to] = through;
                }
            }
        }
    }
    for (size_t index = 0; index < network->node_count; ++index) {
        if (distance[index][index] < 0) {
            return false;
        }
    }
    return true;
}

const EidolonEprTemporalNode *
eidolon_epr_temporal_find(const EidolonEprTemporalNetwork *network, EidolonEprAnchorId id) {
    size_t index;
    if (network == NULL) {
        return NULL;
    }
    index = node_index(network, id);
    return index < network->node_count ? &network->nodes[index] : NULL;
}
