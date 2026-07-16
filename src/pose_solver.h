#ifndef EIDOLON_POSE_SOLVER_H
#define EIDOLON_POSE_SOLVER_H

#include "humanoid.h"
#include "pose.h"

bool eidolon_pose_solve(EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                        const EidolonSemanticPose *pose);

#endif
