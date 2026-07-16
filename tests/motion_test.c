#include "motion.h"

#include <assert.h>
#include <math.h>

static const float IDENTITY_ROTATION[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float UNIT_SCALE[3] = {1.0F, 1.0F, 1.0F};

static void add_node(EidolonMotionRig *rig, size_t index, const char *name, int parent, float x,
                     float y, float z) {
    const float translation[3] = {x, y, z};
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, IDENTITY_ROTATION,
                                   UNIT_SCALE));
}

int main(void) {
    EidolonMotionRig rig;
    assert(eidolon_motion_init(&rig, 11));
    add_node(&rig, 0, "Bip001 Pelvis", -1, 0.0F, 0.0F, 0.0F);
    add_node(&rig, 1, "Bip001 Spine", 0, 0.0F, 0.2F, 0.0F);
    add_node(&rig, 2, "Bip001 Spine1", 1, 0.0F, 0.2F, 0.0F);
    add_node(&rig, 3, "Bip001 Neck", 2, 0.0F, 0.2F, 0.0F);
    add_node(&rig, 4, "Bip001 Head", 3, 0.0F, 0.1F, 0.0F);
    add_node(&rig, 5, "Bip001 L UpperArm", 2, 0.2F, 0.1F, 0.0F);
    add_node(&rig, 6, "Bip001 L Forearm", 5, 1.0F, 0.0F, 0.0F);
    add_node(&rig, 7, "Left Hand", 6, 1.0F, 0.0F, 0.0F);
    add_node(&rig, 8, "Bip001 R UpperArm", 2, -0.2F, 0.1F, 0.0F);
    add_node(&rig, 9, "Bip001 R Forearm", 8, -1.0F, 0.0F, 0.0F);
    add_node(&rig, 10, "Right Hand", 9, -1.0F, 0.0F, 0.0F);
    assert(eidolon_motion_finalize(&rig));

    eidolon_motion_set_neutral_pose(&rig, (EidolonNeutralPose){0.0F, 0.0F});
    eidolon_motion_update_idle(&rig, 0);
    assert(fabsf(rig.nodes[5].rotation[1]) < 0.000001F);
    assert(fabsf(rig.nodes[6].rotation[2]) < 0.000001F);
    assert(fabsf(rig.nodes[8].rotation[1]) < 0.000001F);
    assert(fabsf(rig.nodes[9].rotation[2]) < 0.000001F);

    const EidolonNeutralPose adjusted = {
        .shoulder_lower_radians = 0.12F,
        .elbow_bend_add_radians = 0.07F,
    };
    eidolon_motion_set_neutral_pose(&rig, adjusted);
    eidolon_motion_update_idle(&rig, 0);
    assert(rig.nodes[5].rotation[1] > 0.0F);
    assert(rig.nodes[8].rotation[1] < 0.0F);
    assert(rig.nodes[6].rotation[2] < 0.0F);
    assert(rig.nodes[9].rotation[2] < 0.0F);
    assert(fabsf(rig.nodes[5].rotation[2]) < 0.000001F);
    assert(fabsf(rig.nodes[8].rotation[2]) < 0.000001F);

    eidolon_motion_set_neutral_pose(&rig, (EidolonNeutralPose){0.0F, 0.0F});
    eidolon_motion_update_idle(&rig, 0);
    const float first_head_x = eidolon_motion_world(&rig, 4)[12];
    const float first_head_y = eidolon_motion_world(&rig, 4)[13];
    eidolon_motion_update_idle(&rig, 1000);
    const float second_head_x = eidolon_motion_world(&rig, 4)[12];
    const float second_head_y = eidolon_motion_world(&rig, 4)[13];
    assert(fabsf(first_head_x - second_head_x) > 0.00001F ||
           fabsf(first_head_y - second_head_y) > 0.00001F);

    eidolon_motion_update_idle(&rig, 1000);
    assert(fabsf(second_head_x - eidolon_motion_world(&rig, 4)[12]) < 0.000001F);
    assert(fabsf(second_head_y - eidolon_motion_world(&rig, 4)[13]) < 0.000001F);

    EidolonIdleTuning still = eidolon_motion_idle_tuning(&rig);
    still.breath_chest_radians = 0.0F;
    still.breath_neck_counter_radians = 0.0F;
    still.sway_spine_radians = 0.0F;
    still.sway_chest_counter_radians = 0.0F;
    still.sway_head_radians = 0.0F;
    eidolon_motion_set_idle_tuning(&rig, still);
    eidolon_motion_update_idle(&rig, 0);
    const float still_head_x = eidolon_motion_world(&rig, 4)[12];
    const float still_head_y = eidolon_motion_world(&rig, 4)[13];
    eidolon_motion_update_idle(&rig, 1000);
    assert(fabsf(still_head_x - eidolon_motion_world(&rig, 4)[12]) < 0.000001F);
    assert(fabsf(still_head_y - eidolon_motion_world(&rig, 4)[13]) < 0.000001F);

    eidolon_motion_destroy(&rig);
    return 0;
}
