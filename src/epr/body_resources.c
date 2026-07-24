#include "epr/body_resources.h"

#include <string.h>

int eidolon_epr_resource_rank_compare(const EidolonEprResourceRank *left,
                                      const EidolonEprResourceRank *right) {
    if (left->urgency != right->urgency) {
        return left->urgency > right->urgency ? 1 : -1;
    }
    if (left->priority != right->priority) {
        return left->priority > right->priority ? 1 : -1;
    }
    if (left->committed_phase != right->committed_phase) {
        return left->committed_phase > right->committed_phase ? 1 : -1;
    }
    if (left->anchor_tick != right->anchor_tick) {
        return left->anchor_tick < right->anchor_tick ? 1 : -1;
    }
    if (left->provenance_class != right->provenance_class) {
        return left->provenance_class > right->provenance_class ? 1 : -1;
    }
    if (left->behavior != right->behavior) {
        return left->behavior < right->behavior ? 1 : -1;
    }
    return 0;
}

static bool claim_active(const EidolonEprResourceClaim *claim, EidolonEprTick tick) {
    return tick >= claim->start_tick && tick <= claim->end_tick;
}

static const EidolonEprResourceClaim *
best_claim(const EidolonEprResourceClaim *claims, size_t claim_count, EidolonEprTick tick,
           EidolonEprBodyResource resource, EidolonEprClaimMode mode) {
    const EidolonEprResourceClaim *best = NULL;
    for (size_t index = 0; index < claim_count; ++index) {
        const EidolonEprResourceClaim *claim = &claims[index];
        if (claim->resource != resource || claim->mode != mode || !claim_active(claim, tick)) {
            continue;
        }
        if (best == NULL || eidolon_epr_resource_rank_compare(&claim->rank, &best->rank) > 0) {
            best = claim;
        }
    }
    return best;
}

static bool append_grant(EidolonEprResourceResolution *resolution,
                         const EidolonEprResourceClaim *claim) {
    EidolonEprResourceGrant *grant;
    if (claim == NULL || resolution->grant_count >= EIDOLON_EPR_RESOURCE_GRANT_CAPACITY) {
        return claim == NULL;
    }
    grant = &resolution->grants[resolution->grant_count];
    grant->behavior = claim->behavior;
    grant->resource = claim->resource;
    grant->mode = claim->mode;
    grant->composition_rule = claim->composition_rule;
    resolution->grant_count += 1U;
    return true;
}

static bool grant_contains(const EidolonEprResourceResolution *resolution,
                           const EidolonEprResourceClaim *claim) {
    for (size_t index = 0; index < resolution->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[index];
        if (grant->behavior == claim->behavior && grant->resource == claim->resource &&
            grant->mode == claim->mode) {
            return true;
        }
    }
    return false;
}

static void sort_denied(EidolonEprResourceResolution *resolution) {
    for (size_t index = 1U; index < resolution->denied_count; ++index) {
        const EidolonEprOpaqueId value = resolution->denied[index];
        size_t insert = index;
        while (insert > 0U && resolution->denied[insert - 1U] > value) {
            resolution->denied[insert] = resolution->denied[insert - 1U];
            insert -= 1U;
        }
        resolution->denied[insert] = value;
    }
}

bool eidolon_epr_resource_resolve(const EidolonEprResourceClaim *claims, size_t claim_count,
                                  EidolonEprTick tick, EidolonEprResourceResolution *resolution) {
    if (resolution == NULL || (claims == NULL && claim_count > 0U) ||
        claim_count > EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY) {
        return false;
    }
    memset(resolution, 0, sizeof(*resolution));
    for (int resource_value = 0; resource_value < (int)EIDOLON_EPR_RESOURCE_COUNT;
         ++resource_value) {
        const EidolonEprBodyResource resource = (EidolonEprBodyResource)resource_value;
        const EidolonEprResourceClaim *override =
            best_claim(claims, claim_count, tick, resource, EIDOLON_EPR_CLAIM_OVERRIDE);
        if (override != NULL) {
            if (!append_grant(resolution, override)) {
                return false;
            }
        } else {
            if (!append_grant(
                    resolution,
                    best_claim(claims, claim_count, tick, resource, EIDOLON_EPR_CLAIM_BASE))) {
                return false;
            }
            if (!append_grant(
                    resolution,
                    best_claim(claims, claim_count, tick, resource,
                               EIDOLON_EPR_CLAIM_COOPERATIVE))) {
                return false;
            }
            if (!append_grant(
                    resolution,
                    best_claim(claims, claim_count, tick, resource,
                               EIDOLON_EPR_CLAIM_ADDITIVE))) {
                return false;
            }
        }
    }
    for (size_t index = 0; index < claim_count; ++index) {
        const EidolonEprResourceClaim *claim = &claims[index];
        if (!claim_active(claim, tick) || grant_contains(resolution, claim)) {
            continue;
        }
        if (resolution->denied_count >= EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY) {
            return false;
        }
        resolution->denied[resolution->denied_count] = claim->behavior;
        resolution->denied_count += 1U;
    }
    sort_denied(resolution);
    return true;
}

bool eidolon_epr_resource_is_granted(const EidolonEprResourceResolution *resolution,
                                     EidolonEprOpaqueId behavior,
                                     EidolonEprBodyResource resource) {
    if (resolution == NULL) {
        return false;
    }
    for (size_t index = 0; index < resolution->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[index];
        if (grant->behavior == behavior && grant->resource == resource) {
            return true;
        }
    }
    return false;
}

EidolonEprOpaqueId
eidolon_epr_resource_override_owner(const EidolonEprResourceResolution *resolution,
                                    EidolonEprBodyResource resource) {
    if (resolution == NULL) {
        return 0U;
    }
    for (size_t index = 0; index < resolution->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[index];
        if (grant->resource == resource &&
            (grant->mode == EIDOLON_EPR_CLAIM_OVERRIDE ||
             grant->mode == EIDOLON_EPR_CLAIM_BASE)) {
            return grant->behavior;
        }
    }
    return 0U;
}
