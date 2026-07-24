#ifndef EIDOLON_VRM_BODY_H
#define EIDOLON_VRM_BODY_H

#include "epr/performance_runtime.h"

#include <cgltf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_METADATA_CAPACITY 128U
#define EIDOLON_VRM_LICENSE_CAPACITY 256U
#define EIDOLON_VRM_EXPRESSION_BIND_CAPACITY 16U

typedef enum EidolonVrmHumanBone {
    EIDOLON_VRM_BONE_HIPS = 0,
    EIDOLON_VRM_BONE_SPINE,
    EIDOLON_VRM_BONE_CHEST,
    EIDOLON_VRM_BONE_UPPER_CHEST,
    EIDOLON_VRM_BONE_NECK,
    EIDOLON_VRM_BONE_HEAD,
    EIDOLON_VRM_BONE_LEFT_EYE,
    EIDOLON_VRM_BONE_RIGHT_EYE,
    EIDOLON_VRM_BONE_LEFT_UPPER_LEG,
    EIDOLON_VRM_BONE_LEFT_LOWER_LEG,
    EIDOLON_VRM_BONE_LEFT_FOOT,
    EIDOLON_VRM_BONE_RIGHT_UPPER_LEG,
    EIDOLON_VRM_BONE_RIGHT_LOWER_LEG,
    EIDOLON_VRM_BONE_RIGHT_FOOT,
    EIDOLON_VRM_BONE_LEFT_SHOULDER,
    EIDOLON_VRM_BONE_LEFT_UPPER_ARM,
    EIDOLON_VRM_BONE_LEFT_LOWER_ARM,
    EIDOLON_VRM_BONE_LEFT_HAND,
    EIDOLON_VRM_BONE_RIGHT_SHOULDER,
    EIDOLON_VRM_BONE_RIGHT_UPPER_ARM,
    EIDOLON_VRM_BONE_RIGHT_LOWER_ARM,
    EIDOLON_VRM_BONE_RIGHT_HAND,
    EIDOLON_VRM_BONE_COUNT
} EidolonVrmHumanBone;

typedef struct EidolonVrmExpressionBind {
    size_t node;
    size_t target;
    float weight;
} EidolonVrmExpressionBind;

typedef struct EidolonVrmBody {
    int node_by_bone[EIDOLON_VRM_BONE_COUNT];
    EidolonVrmExpressionBind neutral_binds[EIDOLON_VRM_EXPRESSION_BIND_CAPACITY];
    size_t neutral_bind_count;
    EidolonVrmExpressionBind focused_binds[EIDOLON_VRM_EXPRESSION_BIND_CAPACITY];
    size_t focused_bind_count;
    char name[EIDOLON_VRM_METADATA_CAPACITY];
    char author[EIDOLON_VRM_METADATA_CAPACITY];
    char license_url[EIDOLON_VRM_LICENSE_CAPACITY];
    char commercial_usage[EIDOLON_VRM_METADATA_CAPACITY];
    char credit_notation[EIDOLON_VRM_METADATA_CAPACITY];
    uint64_t fingerprint;
    bool has_look_at;
    bool has_expression;
    bool has_spring_bones;
    bool has_mtoon;
} EidolonVrmBody;

bool eidolon_vrm_body_parse(const cgltf_data *data, EidolonVrmBody *body, char *error,
                            size_t error_capacity);
bool eidolon_vrm_body_make_profile(const cgltf_data *data, const EidolonVrmBody *body,
                                   EidolonEprBodyProfile *profile, char *error,
                                   size_t error_capacity);
int eidolon_vrm_body_node(const EidolonVrmBody *body, EidolonVrmHumanBone bone);

#endif
