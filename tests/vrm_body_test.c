#include "vrm_body.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char VALID_VRM[] =
    "{"
    "\"specVersion\":\"1.0\","
    "\"meta\":{\"name\":\"Fixture\",\"authors\":[\"Eidolon\"],"
    "\"licenseUrl\":\"https://example.invalid/license\","
    "\"commercialUsage\":\"personalNonProfit\",\"creditNotation\":\"required\"},"
    "\"humanoid\":{\"humanBones\":{"
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"
    "\"leftEye\":{\"node\":3},\"rightEye\":{\"node\":4},"
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"
    "\"rightLowerArm\":{\"node\":15},\"rightHand\":{\"node\":16}}},"
    "\"lookAt\":{\"type\":\"bone\"},"
    "\"expressions\":{\"preset\":{"
    "\"neutral\":{\"morphTargetBinds\":[{\"node\":17,\"index\":0,\"weight\":1}]},"
    "\"relaxed\":{\"morphTargetBinds\":[{\"node\":17,\"index\":2,\"weight\":1}]}}}"
    "}";

static const char MISSING_RIGHT_HAND[] =
    "{"
    "\"specVersion\":\"1.0\","
    "\"meta\":{\"name\":\"Broken\",\"authors\":[\"Eidolon\"],"
    "\"licenseUrl\":\"https://example.invalid/license\"},"
    "\"humanoid\":{\"humanBones\":{"
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"
    "\"rightLowerArm\":{\"node\":15}}}"
    "}";

static const char INVALID_EXPRESSION_TARGET[] =
    "{"
    "\"specVersion\":\"1.0\","
    "\"meta\":{\"name\":\"Fixture\",\"authors\":[\"Eidolon\"],"
    "\"licenseUrl\":\"https://example.invalid/license\"},"
    "\"humanoid\":{\"humanBones\":{"
    "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"
    "\"leftUpperLeg\":{\"node\":5},\"leftLowerLeg\":{\"node\":6},"
    "\"leftFoot\":{\"node\":7},\"rightUpperLeg\":{\"node\":8},"
    "\"rightLowerLeg\":{\"node\":9},\"rightFoot\":{\"node\":10},"
    "\"leftUpperArm\":{\"node\":11},\"leftLowerArm\":{\"node\":12},"
    "\"leftHand\":{\"node\":13},\"rightUpperArm\":{\"node\":14},"
    "\"rightLowerArm\":{\"node\":15},\"rightHand\":{\"node\":16}}},"
    "\"expressions\":{\"preset\":{"
    "\"neutral\":{\"morphTargetBinds\":[{\"node\":17,\"index\":9,\"weight\":1}]},"
    "\"relaxed\":{\"morphTargetBinds\":[{\"node\":17,\"index\":2,\"weight\":1}]}}}"
    "}";

static cgltf_accessor expression_accessors[3];
static cgltf_attribute expression_attributes[3];
static cgltf_morph_target expression_targets[3];
static cgltf_primitive expression_primitive;
static cgltf_mesh expression_mesh;

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
    data->nodes = nodes;
    data->nodes_count = 18U;
    extensions[0].name = (char *)vrm_name;
    extensions[0].data = (char *)vrm_json;
    extensions[1].name = "VRMC_springBone";
    extensions[1].data = "{}";
    extensions[2].name = "VRMC_materials_mtoon";
    extensions[2].data = "{}";
    data->data_extensions = extensions;
    data->data_extensions_count = 3U;
    for (size_t index = 0; index < 3U; ++index) {
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

    nodes[14].has_translation = 1;
    nodes[14].translation[0] = 0.22F;
    nodes[14].translation[1] = 1.42F;
    nodes[11].has_translation = 1;
    nodes[11].translation[0] = -0.22F;
    nodes[11].translation[1] = 1.42F;
    nodes[0].has_translation = 1;
    nodes[0].translation[1] = 0.90F;
    nodes[2].has_translation = 1;
    nodes[2].translation[1] = 1.70F;
    nodes[15].parent = &nodes[14];
    nodes[15].has_translation = 1;
    nodes[15].translation[0] = 0.30F;
    nodes[16].parent = &nodes[15];
    nodes[16].has_translation = 1;
    nodes[16].translation[0] = 0.28F;
}

static void test_valid_fixture(void) {
    cgltf_data data;
    cgltf_node nodes[18];
    cgltf_extension extensions[3];
    EidolonVrmBody body;
    EidolonEprBodyProfile profile;
    char error[256];
    build_fixture(&data, nodes, extensions, VALID_VRM, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(strcmp(body.name, "Fixture") == 0);
    assert(strcmp(body.author, "Eidolon") == 0);
    assert(strcmp(body.commercial_usage, "personalNonProfit") == 0);
    assert(body.has_look_at);
    assert(body.has_expression);
    assert(body.has_spring_bones);
    assert(body.has_mtoon);
    assert(body.neutral_bind_count == 1U);
    assert(body.focused_bind_count == 1U);
    assert(eidolon_vrm_body_node(&body, EIDOLON_VRM_BONE_RIGHT_HAND) == 16);
    if (!eidolon_vrm_body_make_profile(&data, &body, &profile, error, sizeof(error))) {
        fprintf(stderr, "fixture profile failed: %s\n", error);
        assert(false);
    }
    assert(profile.has_required_humanoid);
    assert(profile.has_eyes);
    assert(profile.has_expression);
    assert(profile.right_upper_arm_length > 0.299F);
    assert(profile.right_upper_arm_length < 0.301F);
    assert(profile.right_lower_arm_length > 0.279F);
    assert(profile.right_lower_arm_length < 0.281F);
}

static void test_legacy_and_required_bone_rejection(void) {
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

    build_fixture(&data, nodes, extensions, INVALID_EXPRESSION_TARGET, "VRMC_vrm");
    assert(eidolon_vrm_body_parse(&data, &body, error, sizeof(error)));
    assert(!body.has_expression);
    assert(body.neutral_bind_count == 0U);
    assert(body.focused_bind_count == 0U);
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
    if (!eidolon_vrm_body_parse(data, &body, error, sizeof(error)) ||
        !eidolon_vrm_body_make_profile(data, &body, &profile, error, sizeof(error))) {
        fprintf(stderr, "%s: %s\n", path, error);
        cgltf_free(data);
        return 1;
    }
    printf("VRM 1.0 body: %s by %s, arm %.3f + %.3f, look-at %s, expression %s, "
           "spring %s, license %s\n",
           body.name, body.author, profile.right_upper_arm_length, profile.right_lower_arm_length,
           body.has_look_at ? "yes" : "no", body.has_expression ? "yes" : "no",
           body.has_spring_bones ? "declared" : "none", body.license_url);
    cgltf_free(data);
    return 0;
}

int main(int argc, char **argv) {
    test_valid_fixture();
    test_legacy_and_required_bone_rejection();
    if (argc == 2) {
        return inspect_file(argv[1]);
    }
    puts("vrm body tests passed");
    return 0;
}
