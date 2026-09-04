#include "vrm_body.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FIXTURE_META                                                                               \
    "\"meta\":{\"name\":\"Fixture\",\"authors\":[\"Eidolon\"],"                                    \
    "\"licenseUrl\":\"https://example.invalid/license\","                                          \
    "\"commercialUsage\":\"personalNonProfit\",\"creditNotation\":\"required\"}"

#define FIXTURE_HUMANOID_WITH_EYES                                                                 \
    "\"humanoid\":{\"humanBones\":{"                                                               \
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"                          \
    "\"leftEye\":{\"node\":3},\"rightEye\":{\"node\":4},"                                          \
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"                                 \
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"                                    \
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"                                  \
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"                               \
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"                                  \
    "\"rightLowerArm\":{\"node\":15},\"rightHand\":{\"node\":16}}}"

#define FIXTURE_HUMANOID_NO_EYES                                                                   \
    "\"humanoid\":{\"humanBones\":{"                                                               \
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"                          \
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"                                 \
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"                                    \
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"                                  \
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"                               \
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"                                  \
    "\"rightLowerArm\":{\"node\":15},\"rightHand\":{\"node\":16}}}"

#define FIXTURE_LOOK_AT                                                                            \
    "\"lookAt\":{\"type\":\"bone\",\"offsetFromHeadBone\":[0,0.06,0],"                             \
    "\"rangeMapHorizontalInner\":{\"inputMaxValue\":90,\"outputScale\":10},"                       \
    "\"rangeMapHorizontalOuter\":{\"inputMaxValue\":80,\"outputScale\":12},"                       \
    "\"rangeMapVerticalDown\":{\"inputMaxValue\":70,\"outputScale\":8},"                           \
    "\"rangeMapVerticalUp\":{\"inputMaxValue\":60,\"outputScale\":7}}"

static const char VALID_VRM[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_WITH_EYES "," FIXTURE_LOOK_AT
    ",\"expressions\":{\"preset\":{"
    "\"neutral\":{\"morphTargetBinds\":[{\"node\":17,\"index\":0,\"weight\":1}]},"
    "\"relaxed\":{\"morphTargetBinds\":[{\"node\":17,\"index\":2,\"weight\":1}]}}}}";

static const char RELAXED_ONLY[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_WITH_EYES ","
    "\"expressions\":{\"preset\":{\"relaxed\":{\"morphTargetBinds\":["
    "{\"node\":17,\"index\":2,\"weight\":1}]}}}}";

static const char INVALID_NEUTRAL_TARGET[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_WITH_EYES ","
    "\"expressions\":{\"preset\":{"
    "\"neutral\":{\"morphTargetBinds\":[{\"node\":17,\"index\":9,\"weight\":1}]},"
    "\"relaxed\":{\"morphTargetBinds\":[{\"node\":17,\"index\":2,\"weight\":1}]}}}}";

static const char MATERIAL_ONLY_RELAXED[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_WITH_EYES ","
    "\"expressions\":{\"preset\":{\"relaxed\":{\"materialColorBinds\":["
    "{\"material\":0,\"type\":\"color\",\"targetValue\":[1,1,1,1]}]}}}}";

static const char OVER_CAPACITY_RELAXED[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_WITH_EYES ","
    "\"expressions\":{\"preset\":{\"relaxed\":{\"morphTargetBinds\":["
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1},{\"node\":17,\"index\":0,\"weight\":1},"
    "{\"node\":17,\"index\":0,\"weight\":1}]}}}}";

static const char EXPRESSION_LOOK_AT_NO_EYES[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META "," FIXTURE_HUMANOID_NO_EYES ","
    "\"lookAt\":{\"type\":\"expression\","
    "\"rangeMapHorizontalOuter\":{\"inputMaxValue\":90,\"outputScale\":1},"
    "\"rangeMapVerticalDown\":{\"inputMaxValue\":90,\"outputScale\":1},"
    "\"rangeMapVerticalUp\":{\"inputMaxValue\":90,\"outputScale\":1}}}";

static const char MISSING_RIGHT_HAND[] =
    "{\"specVersion\":\"1.0\"," FIXTURE_META ","
    "\"humanoid\":{\"humanBones\":{"
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"
    "\"rightLowerArm\":{\"node\":15}}}}";

static const char UNICODE_NAME[] =
    "{\"specVersion\":\"1.0\","
    "\"meta\":{\"name\":\"Fixture \\uD83D\\uDE00\",\"authors\":[\"Eidolon\"],"
    "\"licenseUrl\":\"https://example.invalid/license\"}," FIXTURE_HUMANOID_WITH_EYES "}";

static cgltf_accessor expression_accessors[3];
static cgltf_attribute expression_attributes[3];
static cgltf_morph_target expression_targets[3];
static cgltf_primitive expression_primitive;
static cgltf_mesh expression_mesh;
static cgltf_material fixture_materials[2];
static cgltf_extension fixture_mtoon_extension;
static cgltf_extension fixture_constraint_extension;

static void build_fixture(cgltf_data *data, cgltf_node nodes[18], cgltf_extension extensions[3],
                          const char *vrm_json, const char *vrm_name) {
    memset(data, 0, sizeof(*data));
    memset(nodes, 0, sizeof(cgltf_node) * 18U);
    memset(extensions, 0, sizeof(cgltf_extension) * 3U);
    memset(expression_accessors, 0, sizeof(expression_accessors));
    memset(expression_attributes, 0, sizeof(expression_attributes));
    memset(expression_targets, 0, sizeof(expression_targets));
    memset(&expression_primitive, 0, sizeof(expression_primitive));
    memset(&expression_mesh, 0, sizeof(expression_mesh));
    memset(fixture_materials, 0, sizeof(fixture_materials));
    memset(&fixture_mtoon_extension, 0, sizeof(fixture_mtoon_extension));
    memset(&fixture_constraint_extension, 0, sizeof(fixture_constraint_extension));

    data->nodes = nodes;
    data->nodes_count = 18U;
    extensions[0].name = (char *)vrm_name;
    extensions[0].data = (char *)vrm_json;
    extensions[1].name = "VRMC_springBone";
    extensions[1].data = "{}";
    extensions[2].name = "VRMC_materials_mtoon";
    extensions[2].data = "{\"specVersion\":\"1.0\"}";
    data->data_extensions = extensions;
    data->data_extensions_count = 3U;

    fixture_mtoon_extension.name = "VRMC_materials_mtoon";
    fixture_mtoon_extension.data = "{\"specVersion\":\"1.0\"}";
    fixture_materials[0].extensions = &fixture_mtoon_extension;
    fixture_materials[0].extensions_count = 1U;
    data->materials = fixture_materials;
    data->materials_count = 2U;

    fixture_constraint_extension.name = "VRMC_node_constraint";
    fixture_constraint_extension.data = "{}";
    nodes[17].extensions = &fixture_constraint_extension;
    nodes[17].extensions_count = 1U;

    for (size_t index = 0; index < 3U; ++index) {
        expression_accessors[index].count = 1U;
        expression_attributes[index].type = cgltf_attribute_type_position;
        expression_attributes[index].data = &expression_accessors[index];
        expression_targets[index].attributes = &expression_attributes[index];
        expression_targets[index].attributes_count = 1U;
    }
    expression_primitive.targets = expression_targets;
    expression_primitive.targets_count = 3U;
    expression_mesh.primitives = &expression_primitive;
    expression_mesh.primitives_count = 1U;
    nodes[17].mesh = &expression_mesh;

    for (size_t index = 0; index < 18U; ++index) {
        nodes[index].rotation[3] = 1.0F;
        nodes[index].scale[0] = 1.0F;
        nodes[index].scale[1] = 1.0F;
        nodes[index].scale[2] = 1.0F;
    }
    nodes[1].parent = &nodes[0];
    nodes[2].parent = &nodes[1];
    nodes[3].parent = &nodes[2];
    nodes[4].parent = &nodes[2];
    nodes[5].parent = &nodes[0];
    nodes[6].parent = &nodes[5];
    nodes[7].parent = &nodes[6];
    nodes[8].parent = &nodes[0];
    nodes[9].parent = &nodes[8];
    nodes[10].parent = &nodes[9];
    nodes[11].parent = &nodes[1];
    nodes[12].parent = &nodes[11];
    nodes[13].parent = &nodes[12];
    nodes[14].parent = &nodes[1];
    nodes[17].parent = &nodes[14];
    nodes[15].parent = &nodes[17];
    nodes[16].parent = &nodes[15];

    nodes[0].has_translation = 1;
    nodes[0].translation[1] = 0.90F;
    nodes[2].has_translation = 1;
    nodes[2].translation[1] = 0.80F;
    nodes[11].has_translation = 1;
    nodes[11].translation[0] = -0.22F;
    nodes[11].translation[1] = 0.52F;
    nodes[14].has_translation = 1;
    nodes[14].translation[0] = 0.22F;
    nodes[14].translation[1] = 0.52F;
    nodes[15].has_translation = 1;
    nodes[15].translation[0] = 0.30F;
    nodes[16].has_translation = 1;
    nodes[16].translation[0] = 0.28F;
}

static void assert_profile(const cgltf_data *data, const EidolonVrmBody *body,
                           bool has_expression) {
    EidolonEprBodyProfile profile;
    char error[256];
    if (!eidolon_vrm_body_make_profile(data, body, &profile, error, sizeof(error))) {
        fprintf(stderr, "fixture profile failed: %s\n", error);
        assert(false);
    }
    assert(profile.has_required_humanoid);
    assert(!profile.has_eyes);
    assert(profile.has_expression == has_expression);
    assert(profile.right_upper_arm_length > 0.299F);
    assert(profile.right_upper_arm_length < 0.301F);
    assert(profile.right_lower_arm_length > 0.279F);
    assert(profile.right_lower_arm_length < 0.281F);
}

static void test_valid_fixture(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    char error[256];
    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strcmp(body.name, "Fixture") == 0);
    assert(strcmp(body.author, "Eidolon") == 0);
    assert(strcmp(body.commercial_usage, "personalNonProfit") == 0);
    assert(body.eye_bones_present);
    assert(body.look_at.state == EIDOLON_VRM_CAPABILITY_PARSED);
    assert(body.look_at.type == EIDOLON_VRM_LOOK_AT_BONE);
    assert(body.look_at.horizontal_inner.present);
    assert(body.look_at.horizontal_outer.output_scale == 12.0F);
    assert(body.neutral_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE);
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE);
    assert(body.spring_bones_state == EIDOLON_VRM_CAPABILITY_DECLARED);
    assert(body.node_constraints_state == EIDOLON_VRM_CAPABILITY_DECLARED);
    assert(body.mtoon_state == EIDOLON_VRM_CAPABILITY_PARSED);
    assert(body.material_count == 2U);
    assert(body.mtoon_material_count == 1U);
    assert(body.mtoon_material_states[0] == EIDOLON_VRM_CAPABILITY_PARSED);
    assert(body.mtoon_material_states[1] == EIDOLON_VRM_CAPABILITY_ABSENT);
    assert(eidolon_vrm_body_node(&body, EIDOLON_VRM_BONE_RIGHT_HAND) == 16);
    assert(body.node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_HAND] == 16);
    assert(body.node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_EYE] == 3);
    assert(body.node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_TOES] == -1);
    assert_profile(&data, &body, true);
    eidolon_vrm_body_destroy(&body);
}

static void test_expression_independence(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    char error[256];

    build_fixture(&data, nodes, extensions, RELAXED_ONLY, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.neutral_expression.state == EIDOLON_VRM_CAPABILITY_ABSENT);
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE);
    assert_profile(&data, &body, true);
    eidolon_vrm_body_destroy(&body);

    build_fixture(&data, nodes, extensions, INVALID_NEUTRAL_TARGET, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.neutral_expression.state == EIDOLON_VRM_CAPABILITY_PARSED);
    assert(strstr(body.neutral_expression.diagnostic, "no executable") != NULL);
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE);
    assert_profile(&data, &body, true);
    eidolon_vrm_body_destroy(&body);

    build_fixture(&data, nodes, extensions, MATERIAL_ONLY_RELAXED, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_DECLARED);
    assert(body.relaxed_expression.material_color_bind_count == 1U);
    assert_profile(&data, &body, false);
    eidolon_vrm_body_destroy(&body);

    build_fixture(&data, nodes, extensions, OVER_CAPACITY_RELAXED, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_PARSED);
    assert(body.relaxed_expression.declared_morph_bind_count == 17U);
    assert(strstr(body.relaxed_expression.diagnostic, "exceeds") != NULL);
    assert_profile(&data, &body, false);
    eidolon_vrm_body_destroy(&body);
}

static void test_look_at_and_mtoon_truth(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    char error[256];

    build_fixture(&data, nodes, extensions, EXPRESSION_LOOK_AT_NO_EYES, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(!body.eye_bones_present);
    assert(body.look_at.type == EIDOLON_VRM_LOOK_AT_EXPRESSION);
    assert(body.look_at.state == EIDOLON_VRM_CAPABILITY_PARSED);
    assert_profile(&data, &body, false);
    eidolon_vrm_body_destroy(&body);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    data.materials = NULL;
    data.materials_count = 0U;
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.mtoon_state == EIDOLON_VRM_CAPABILITY_ABSENT);
    assert(body.mtoon_material_count == 0U);
    eidolon_vrm_body_destroy(&body);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    fixture_mtoon_extension.data = "{\"specVersion\":\"0.0\"}";
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(body.mtoon_state == EIDOLON_VRM_CAPABILITY_DECLARED);
    assert(body.mtoon_material_states[0] == EIDOLON_VRM_CAPABILITY_DECLARED);
    eidolon_vrm_body_destroy(&body);
}

static void test_structural_rejections(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    char error[256];

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRM");
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "not VRM 1.0") != NULL);

    build_fixture(&data, nodes, extensions, MISSING_RIGHT_HAND, "VRMC_vrm");
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "rightHand") != NULL);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    nodes[15].parent = &nodes[0];
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "rightLowerArm") != NULL);
    assert(strstr(error, "rightUpperArm") != NULL);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    nodes[14].scale[1] = 0.0F;
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "non-positive scale") != NULL);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    nodes[14].scale[1] = -1.0F;
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "non-positive scale") != NULL);

    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    nodes[2].has_matrix = 1;
    assert(!eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strstr(error, "matrix node") != NULL);
}

static void test_unicode_surrogate_pair(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    char error[256];
    build_fixture(&data, nodes, extensions, UNICODE_NAME, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strcmp(body.name, "Fixture \xF0\x9F\x98\x80") == 0);
    assert(body.neutral_expression.state == EIDOLON_VRM_CAPABILITY_ABSENT);
    assert(body.relaxed_expression.state == EIDOLON_VRM_CAPABILITY_ABSENT);
    eidolon_vrm_body_destroy(&body);
}

static int inspect_file(const char *path) {
    cgltf_options options;
    cgltf_data *data = NULL;
    EidolonVrmBody body;
    EidolonEprBodyProfile profile;
    char error[256];
    memset(&options, 0, sizeof(options));
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success || data == NULL) {
        fprintf(stderr, "could not parse %s\n", path);
        return 1;
    }
    if (cgltf_validate(data) != cgltf_result_success) {
        fprintf(stderr, "cgltf validation failed for %s\n", path);
        cgltf_free(data);
        return 1;
    }
    if (!eidolon_vrm_body_parse(data, &body, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", path, error);
        cgltf_free(data);
        return 1;
    }
    if (!eidolon_vrm_body_make_profile(data, &body, &profile, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", path, error);
        eidolon_vrm_body_destroy(&body);
        cgltf_free(data);
        return 1;
    }
    printf("Experimental reference VRM preflight: %s by %s, arm %.3f + %.3f, eyes %s, "
           "look-at %s, relaxed %s, neutral %s, MToon %s (%llu material(s)), spring %s, "
           "constraints %s, license %s\n",
           body.name, body.author, profile.right_upper_arm_length, profile.right_lower_arm_length,
           body.eye_bones_present ? "present" : "absent",
           eidolon_vrm_capability_state_name(body.look_at.state),
           eidolon_vrm_capability_state_name(body.relaxed_expression.state),
           eidolon_vrm_capability_state_name(body.neutral_expression.state),
           eidolon_vrm_capability_state_name(body.mtoon_state),
           (unsigned long long)body.mtoon_material_count,
           eidolon_vrm_capability_state_name(body.spring_bones_state),
           eidolon_vrm_capability_state_name(body.node_constraints_state), body.license_url);
    if (body.relaxed_expression.state != EIDOLON_VRM_CAPABILITY_EXECUTABLE) {
        printf("  relaxed: %s\n", body.relaxed_expression.diagnostic);
    }
    if (body.look_at.state != EIDOLON_VRM_CAPABILITY_ABSENT) {
        printf("  look-at: %s\n", body.look_at.diagnostic);
    }
    eidolon_vrm_body_destroy(&body);
    cgltf_free(data);
    return 0;
}

int main(int argc, char **argv) {
    test_valid_fixture();
    test_expression_independence();
    test_look_at_and_mtoon_truth();
    test_structural_rejections();
    test_unicode_surrogate_pair();
    if (argc == 2) {
        return inspect_file(argv[1]);
    }
    puts("vrm body tests passed");
    return 0;
}
