#ifndef EIDOLON_VRM_PLAYBACK_H
#define EIDOLON_VRM_PLAYBACK_H

#include "motion.h"
#include "vrm_retarget.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_PLAYBACK_VERSION 1U
#define EIDOLON_VRM_PLAYBACK_IDENTITY_CAPACITY 384U
#define EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY 256U

typedef enum EidolonVrmPlaybackState {
    EIDOLON_VRM_PLAYBACK_EMPTY = 0,
    EIDOLON_VRM_PLAYBACK_PAUSED,
    EIDOLON_VRM_PLAYBACK_PLAYING,
    EIDOLON_VRM_PLAYBACK_FAILED
} EidolonVrmPlaybackState;

typedef struct EidolonVrmPlaybackReport {
    uint32_t version;
    EidolonVrmPlaybackState state;
    EidolonVrmaCoverage coverage;
    char clip_identity[EIDOLON_VRM_PLAYBACK_IDENTITY_CAPACITY];
    char failure[EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY];
    float duration_seconds;
    float position_seconds;
    float phase;
    float playback_rate;
    uint64_t sample_revision;
    uint64_t published_revision;
    bool loop;
} EidolonVrmPlaybackReport;

typedef struct EidolonVrmPlayback {
    uint32_t version;
    EidolonVrmaClip clip;
    EidolonVrmRetargeter retargeter;
    EidolonMotionRig candidate;
    char clip_identity[EIDOLON_VRM_PLAYBACK_IDENTITY_CAPACITY];
    char failure[EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY];
    float position_seconds;
    float playback_rate;
    uint64_t last_update_ms;
    uint64_t sample_revision;
    uint64_t published_revision;
    EidolonVrmRootMotionPolicy root_motion_policy;
    bool loop;
    bool paused;
    bool has_clock;
    bool needs_sample;
    bool ready;
    bool failed;
} EidolonVrmPlayback;

/* On success, ownership of clip and all of its tracks moves into playback and clip is cleared. */
bool eidolon_vrm_playback_take_clip(EidolonVrmPlayback *playback, EidolonVrmaClip *clip,
                                    const char *identity, const EidolonVrmBody *destination,
                                    const EidolonMotionRig *destination_rig,
                                    EidolonVrmRootMotionPolicy root_motion_policy, char *error,
                                    size_t error_capacity);
bool eidolon_vrm_playback_load(EidolonVrmPlayback *playback, const char *path, const char *identity,
                               const EidolonVrmBody *destination,
                               const EidolonMotionRig *destination_rig,
                               EidolonVrmRootMotionPolicy root_motion_policy, char *error,
                               size_t error_capacity);
bool eidolon_vrm_playback_play(EidolonVrmPlayback *playback, uint64_t now_ms);
bool eidolon_vrm_playback_pause(EidolonVrmPlayback *playback, uint64_t now_ms);
bool eidolon_vrm_playback_seek(EidolonVrmPlayback *playback, float seconds, uint64_t now_ms);
bool eidolon_vrm_playback_set_loop(EidolonVrmPlayback *playback, bool loop);
bool eidolon_vrm_playback_set_rate(EidolonVrmPlayback *playback, float playback_rate,
                                   uint64_t now_ms);
/* A non-NULL candidate is an owned scratch pose awaiting atomic publication by the caller. */
bool eidolon_vrm_playback_update(EidolonVrmPlayback *playback, uint64_t now_ms,
                                 const EidolonMotionRig *current_rig,
                                 const EidolonMotionRig **candidate, char *error,
                                 size_t error_capacity);
bool eidolon_vrm_playback_note_published(EidolonVrmPlayback *playback, uint64_t sample_revision);
void eidolon_vrm_playback_fail(EidolonVrmPlayback *playback, const char *failure);
bool eidolon_vrm_playback_report(const EidolonVrmPlayback *playback,
                                 EidolonVrmPlaybackReport *report);
void eidolon_vrm_playback_destroy(EidolonVrmPlayback *playback);

#endif
