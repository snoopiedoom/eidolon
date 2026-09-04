#include "epr/body_resources.h"

#include <string.h>

static uint64_t humanoid_role_range(EidolonHumanoidRole first, EidolonHumanoidRole last) {
    uint64_t mask = 0U;
    for (int role = (int)first; role <= (int)last; ++role) {
        mask |= UINT64_C(1) << (uint32_t)role;
    }
    return mask;
}

static uint64_t humanoid_resource_rotations(EidolonEprBodyResource resource) {
    switch (resource) {
    case EIDOLON_EPR_RESOURCE_TORSO:
        return humanoid_role_range(EIDOLON_HUMANOID_ROLE_HIPS, EIDOLON_HUMANOID_ROLE_UPPER_CHEST);
    case EIDOLON_EPR_RESOURCE_HEAD:
        return humanoid_role_range(EIDOLON_HUMANOID_ROLE_NECK, EIDOLON_HUMANOID_ROLE_HEAD);
    case EIDOLON_EPR_RESOURCE_EYES:
        return humanoid_role_range(EIDOLON_HUMANOID_ROLE_LEFT_EYE, EIDOLON_HUMANOID_ROLE_RIGHT_EYE);
    case EIDOLON_EPR_RESOURCE_FACE_EXPRESSION:
        return 0U;
    case EIDOLON_EPR_RESOURCE_LEFT_ARM_CHAIN:
        return humanoid_role_range(EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER,
                                   EIDOLON_HUMANOID_ROLE_LEFT_HAND) |
               humanoid_role_range(EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL,
                                   EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_DISTAL);
    case EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN:
        return humanoid_role_range(EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER,
                                   EIDOLON_HUMANOID_ROLE_RIGHT_HAND) |
               humanoid_role_range(EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL,
                                   EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL);
    case EIDOLON_EPR_RESOURCE_COUNT:
        return 0U;
    }
    return 0U;
}

bool eidolon_epr_resource_mask_humanoid_channels(uint32_t resource_mask, uint64_t *rotation_mask,
                                                 bool *owns_hips_translation) {
    const uint32_t valid_resources = (UINT32_C(1) << (uint32_t)EIDOLON_EPR_RESOURCE_COUNT) - 1U;
    uint64_t rotations = 0U;
    bool owns_hips = false;
    if (rotation_mask == NULL || owns_hips_translation == NULL ||
        (resource_mask & ~valid_resources) != 0U) {
        return false;
    }
    for (int resource = 0; resource < (int)EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
        if ((resource_mask & (UINT32_C(1) << (uint32_t)resource)) != 0U) {
            rotations |= humanoid_resource_rotations((EidolonEprBodyResource)resource);
            owns_hips = owns_hips || resource == (int)EIDOLON_EPR_RESOURCE_TORSO;
        }
    }
    *rotation_mask = rotations;
    *owns_hips_translation = owns_hips;
    return true;
}

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

static const EidolonEprResourceClaim *best_claim(const EidolonEprResourceClaim *claims,
                                                 size_t claim_count, EidolonEprTick tick,
                                                 EidolonEprBodyResource resource,
                                                 EidolonEprClaimMode mode) {
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
        const EidolonEprResourceDenial value = resolution->denied[index];
        size_t insert = index;
        while (insert > 0U && (resolution->denied[insert - 1U].resource > value.resource ||
                               (resolution->denied[insert - 1U].resource == value.resource &&
                                (resolution->denied[insert - 1U].mode > value.mode ||
                                 (resolution->denied[insert - 1U].mode == value.mode &&
                                  resolution->denied[insert - 1U].behavior > value.behavior))))) {
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
            if (!append_grant(resolution, best_claim(claims, claim_count, tick, resource,
                                                     EIDOLON_EPR_CLAIM_BASE))) {
                return false;
            }
            if (!append_grant(resolution, best_claim(claims, claim_count, tick, resource,
                                                     EIDOLON_EPR_CLAIM_COOPERATIVE))) {
                return false;
            }
            if (!append_grant(resolution, best_claim(claims, claim_count, tick, resource,
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
        resolution->denied[resolution->denied_count].behavior = claim->behavior;
        resolution->denied[resolution->denied_count].resource = claim->resource;
        resolution->denied[resolution->denied_count].mode = claim->mode;
        resolution->denied_count += 1U;
    }
    sort_denied(resolution);
    return true;
}

bool eidolon_epr_resource_is_granted(const EidolonEprResourceResolution *resolution,
                                     EidolonEprOpaqueId behavior, EidolonEprBodyResource resource) {
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
            (grant->mode == EIDOLON_EPR_CLAIM_OVERRIDE || grant->mode == EIDOLON_EPR_CLAIM_BASE)) {
            return grant->behavior;
        }
    }
    return 0U;
}
