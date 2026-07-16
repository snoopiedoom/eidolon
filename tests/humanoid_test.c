#include "humanoid.h"

#include <assert.h>
#include <math.h>

static const float IDENTITY[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float ONE[3] = {1.0F, 1.0F, 1.0F};

static void node(EidolonMotionRig *rig, size_t index, const char *name, int parent, float x,
                 float y, float z) {
    const float translation[3] = {x, y, z};
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, IDENTITY, ONE));
}

int main(void) {
    EidolonMotionRig rig;
    assert(eidolon_motion_init(&rig, 21));
    node(&rig, 0, "Bip001 Pelvis", -1, 0.0F, 1.0F, 0.0F);
    node(&rig, 1, "Bip001 Spine", 0, 0.0F, 0.2F, 0.0F);
    node(&rig, 2, "Bip001 Spine1", 1, 0.0F, 0.2F, 0.0F);
    node(&rig, 3, "Bip001 Neck", 2, 0.0F, 0.2F, 0.0F);
    node(&rig, 4, "Bip001 Head", 3, 0.0F, 0.2F, 0.0F);
    node(&rig, 5, "Bip001 L UpperArm", 2, 0.3F, 0.1F, 0.0F);
    node(&rig, 6, "Bip001 L Forearm", 5, 0.4F, 0.0F, 0.0F);
    node(&rig, 7, "Bip001 L Hand", 6, 0.3F, 0.0F, 0.0F);
    node(&rig, 8, "Bip001 R UpperArm", 2, -0.3F, 0.2F, 0.0F);
    node(&rig, 9, "Bip001 R Forearm", 8, -0.4F, 0.0F, 0.0F);
    node(&rig, 10, "Bip001 R Hand", 9, -0.3F, 0.0F, 0.0F);
    node(&rig, 11, "Bip001 L Thigh", 0, -0.15F, -0.1F, 0.0F);
    node(&rig, 12, "Bip001 L Calf", 11, 0.0F, -0.5F, 0.0F);
    node(&rig, 13, "Bip001 L Foot", 12, 0.0F, -0.5F, 0.0F);
    node(&rig, 14, "Bip001 R Thigh", 0, 0.15F, -0.1F, 0.0F);
    node(&rig, 15, "Bip001 R Calf", 14, 0.0F, -0.5F, 0.0F);
    node(&rig, 16, "Bip001 R Foot", 15, 0.0F, -0.5F, 0.0F);
    node(&rig, 17, "Bip001 L Toe0", 13, 0.0F, 0.0F, 0.2F);
    node(&rig, 18, "Bip001 R Toe0", 16, 0.0F, 0.0F, 0.2F);
    node(&rig, 19, "Bip001 Xtra_eyeL", 4, 0.05F, 0.05F, -0.1F);
    node(&rig, 20, "Bip001 Xtra_eyeR", 4, -0.05F, 0.05F, -0.1F);

    EidolonHumanoidProfile profile;
    assert(eidolon_humanoid_profile_init(&profile, &rig));
    assert(eidolon_humanoid_node(&profile, EIDOLON_HUMANOID_LEFT_HAND) == 7);
    assert(fabsf(profile.shoulder_width - sqrtf(0.37F)) < 0.0001F);
    assert(fabsf(profile.arm_length[EIDOLON_HUMANOID_LEFT] - 0.7F) < 0.0001F);
    assert(fabsf(profile.right[1]) < 0.0001F);
    assert(profile.forward[2] > 0.99F);
    assert(profile.nodes[EIDOLON_HUMANOID_LEFT_SHOULDER] == -1);
    assert(profile.nodes[EIDOLON_HUMANOID_LEFT_TOES] == 17);

    rig.nodes[7].name[0] = '\0';
    assert(!eidolon_humanoid_profile_init(&profile, &rig));
    assert(SDL_strstr(SDL_GetError(), "leftHand") != NULL);
    SDL_ClearError();

    SDL_strlcpy(rig.nodes[7].name, "Bip001 L Hand", sizeof(rig.nodes[7].name));
    rig.nodes[0].parent = 0;
    assert(!eidolon_humanoid_profile_init(&profile, &rig));
    assert(SDL_strstr(SDL_GetError(), "bind transform") != NULL);
    SDL_ClearError();
    eidolon_motion_destroy(&rig);
    return 0;
}
