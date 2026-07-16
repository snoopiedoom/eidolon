#include "motion_config.h"
#include "pose.h"

#include <assert.h>

int main(void) {
    assert(eidolon_semantic_pose_count() >= 4U);
    for (size_t index = 0; index < eidolon_semantic_pose_count(); ++index) {
        const EidolonSemanticPose *pose = eidolon_semantic_pose(index);
        assert(pose != NULL);
        assert(pose->name != NULL && pose->name[0] != '\0');
        assert(pose->intent != NULL && pose->intent[0] != '\0');
        assert(pose->soften_ratio >= 0.0F && pose->soften_ratio < 1.0F);
        for (size_t side = 0; side < EIDOLON_POSE_ARM_COUNT; ++side) {
            assert(pose->arms[side].weight >= 0.0F && pose->arms[side].weight <= 1.0F);
            for (size_t axis = 0; axis < 3; ++axis) {
                assert(pose->arms[side].hand[axis] >= -1.5F &&
                       pose->arms[side].hand[axis] <= 1.5F);
                assert(pose->arms[side].elbow_pole[axis] >= -1.5F &&
                       pose->arms[side].elbow_pole[axis] <= 1.5F);
            }
        }
    }
    assert(eidolon_semantic_pose(eidolon_semantic_pose_count()) == NULL);
    return 0;
}
