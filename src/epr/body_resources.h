#ifndef EIDOLON_EPR_BODY_RESOURCES_H
#define EIDOLON_EPR_BODY_RESOURCES_H

#include "epr/performance_intent.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY 64U
#define EIDOLON_EPR_RESOURCE_GRANT_CAPACITY 32U

typedef enum EidolonEprBodyResource {
    EIDOLON_EPR_RESOURCE_TORSO = 0,
    EIDOLON_EPR_RESOURCE_HEAD,
    EIDOLON_EPR_RESOURCE_EYES,
    EIDOLON_EPR_RESOURCE_FACE_EXPRESSION,
    EIDOLON_EPR_RESOURCE_LEFT_ARM_CHAIN,
    EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN,
    EIDOLON_EPR_RESOURCE_COUNT
} EidolonEprBodyResource;

typedef enum EidolonEprClaimMode {
    EIDOLON_EPR_CLAIM_BASE = 0,
    EIDOLON_EPR_CLAIM_ADDITIVE,
    EIDOLON_EPR_CLAIM_COOPERATIVE,
    EIDOLON_EPR_CLAIM_OVERRIDE
} EidolonEprClaimMode;

typedef struct EidolonEprResourceRank {
    uint16_t urgency;
    uint16_t priority;
    uint16_t committed_phase;
    EidolonEprTick anchor_tick;
    uint16_t provenance_class;
    EidolonEprOpaqueId behavior;
} EidolonEprResourceRank;

typedef struct EidolonEprResourceClaim {
    EidolonEprOpaqueId behavior;
    uint64_t plan_generation;
    EidolonEprBodyResource resource;
    EidolonEprClaimMode mode;
    EidolonEprTick start_tick;
    EidolonEprTick end_tick;
    EidolonEprResourceRank rank;
    uint16_t composition_rule;
    bool preemptible;
} EidolonEprResourceClaim;

typedef struct EidolonEprResourceGrant {
    EidolonEprOpaqueId behavior;
    EidolonEprBodyResource resource;
    EidolonEprClaimMode mode;
    uint16_t composition_rule;
} EidolonEprResourceGrant;

typedef struct EidolonEprResourceDenial {
    EidolonEprOpaqueId behavior;
    EidolonEprBodyResource resource;
    EidolonEprClaimMode mode;
} EidolonEprResourceDenial;

typedef struct EidolonEprResourceResolution {
    EidolonEprResourceGrant grants[EIDOLON_EPR_RESOURCE_GRANT_CAPACITY];
    size_t grant_count;
    EidolonEprResourceDenial denied[EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY];
    size_t denied_count;
} EidolonEprResourceResolution;

int eidolon_epr_resource_rank_compare(const EidolonEprResourceRank *left,
                                      const EidolonEprResourceRank *right);
bool eidolon_epr_resource_resolve(const EidolonEprResourceClaim *claims, size_t claim_count,
                                  EidolonEprTick tick, EidolonEprResourceResolution *resolution);
bool eidolon_epr_resource_is_granted(const EidolonEprResourceResolution *resolution,
                                     EidolonEprOpaqueId behavior, EidolonEprBodyResource resource);
EidolonEprOpaqueId
eidolon_epr_resource_override_owner(const EidolonEprResourceResolution *resolution,
                                    EidolonEprBodyResource resource);

#endif
