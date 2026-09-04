#ifndef EIDOLON_VRM_BODY_H
#define EIDOLON_VRM_BODY_H

#include "epr/performance_runtime.h"
#include "humanoid_pose.h"

#include <cgltf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_METADATA_CAPACITY 128U
#define EIDOLON_VRM_LICENSE_CAPACITY 256U
#define EIDOLON_VRM_EXPRESSION_BIND_CAPACITY 16U
#define EIDOLON_VRM_CAPABILITY_DIAGNOSTIC_CAPACITY 128U

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

typedef enum EidolonVrmCapabilityState {
    EIDOLON_VRM_CAPABILITY_ABSENT = 0,
    EIDOLON_VRM_CAPABILITY_DECLARED,
    EIDOLON_VRM_CAPABILITY_PARSED,
    EIDOLON_VRM_CAPABILITY_EXECUTABLE
} EidolonVrmCapabilityState;

typedef enum EidolonVrmLookAtType {
    EIDOLON_VRM_LOOK_AT_UNSPECIFIED = 0,
    EIDOLON_VRM_LOOK_AT_BONE,
    EIDOLON_VRM_LOOK_AT_EXPRESSION
} EidolonVrmLookAtType;

typedef struct EidolonVrmLookAtRangeMap {
    float input_max_degrees;
    float output_scale;
    bool present;
    bool has_input_max;
    bool has_output_scale;
} EidolonVrmLookAtRangeMap;

typedef struct EidolonVrmLookAt {
    EidolonVrmCapabilityState state;
    EidolonVrmLookAtType type;
    float offset_from_head[3];
    EidolonVrmLookAtRangeMap horizontal_inner;
    EidolonVrmLookAtRangeMap horizontal_outer;
    EidolonVrmLookAtRangeMap vertical_down;
    EidolonVrmLookAtRangeMap vertical_up;
    char diagnostic[EIDOLON_VRM_CAPABILITY_DIAGNOSTIC_CAPACITY];
    bool has_offset;
} EidolonVrmLookAt;

typedef struct EidolonVrmExpression {
    EidolonVrmExpressionBind morph_binds[EIDOLON_VRM_EXPRESSION_BIND_CAPACITY];
    size_t morph_bind_count;
    size_t declared_morph_bind_count;
    size_t material_color_bind_count;
    size_t texture_transform_bind_count;
    EidolonVrmCapabilityState state;
    char diagnostic[EIDOLON_VRM_CAPABILITY_DIAGNOSTIC_CAPACITY];
    bool is_binary;
} EidolonVrmExpression;

typedef struct EidolonVrmBody {
    int node_by_bone[EIDOLON_VRM_BONE_COUNT];
    int node_by_role[EIDOLON_HUMANOID_ROLE_COUNT];
    EidolonVrmExpression neutral_expression;
    EidolonVrmExpression relaxed_expression;
    EidolonVrmLookAt look_at;
    char name[EIDOLON_VRM_METADATA_CAPACITY];
    char author[EIDOLON_VRM_METADATA_CAPACITY];
    char license_url[EIDOLON_VRM_LICENSE_CAPACITY];
    char commercial_usage[EIDOLON_VRM_METADATA_CAPACITY];
    char credit_notation[EIDOLON_VRM_METADATA_CAPACITY];
    uint64_t fingerprint;
    EidolonVrmCapabilityState spring_bones_state;
    EidolonVrmCapabilityState node_constraints_state;
    EidolonVrmCapabilityState mtoon_state;
    EidolonVrmCapabilityState *mtoon_material_states;
    size_t material_count;
    size_t mtoon_material_count;
    bool eye_bones_present;
} EidolonVrmBody;

bool eidolon_vrm_body_parse(const cgltf_data *data, EidolonVrmBody *body, char *error,
                            size_t error_capacity);
void eidolon_vrm_body_destroy(EidolonVrmBody *body);
bool eidolon_vrm_body_make_profile(const cgltf_data *data, const EidolonVrmBody *body,
                                   EidolonEprBodyProfile *profile, char *error,
                                   size_t error_capacity);
int eidolon_vrm_body_node(const EidolonVrmBody *body, EidolonVrmHumanBone bone);
const char *eidolon_vrm_capability_state_name(EidolonVrmCapabilityState state);

#endif
