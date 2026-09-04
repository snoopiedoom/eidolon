#include "vrm_playback.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYBACK_EPSILON 0.00001F
#define PLAYBACK_RATE_MAX 16.0F

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static void clear_error(char *error, size_t capacity) {
    if (error != NULL && capacity > 0U) {
        error[0] = '\0';
    }
}

static bool valid_playback(const EidolonVrmPlayback *playback) {
    return playback != NULL && playback->version == EIDOLON_VRM_PLAYBACK_VERSION &&
           playback->ready && !playback->failed;
}

static void record_failure(EidolonVrmPlayback *playback, char *error, size_t error_capacity,
                           const char *format, ...) {
    char message[EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY];
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (playback != NULL) {
        (void)snprintf(playback->failure, sizeof(playback->failure), "%s", message);
        playback->failed = true;
        playback->paused = true;
    }
    set_error(error, error_capacity, "%s", message);
}

static bool timeline_position(const EidolonVrmPlayback *playback, uint64_t now_ms, float *position,
                              bool *ended) {
    *position = playback->position_seconds;
    *ended = false;
    if (playback->paused || !playback->has_clock) {
        return true;
    }
    if (now_ms < playback->last_update_ms) {
        return false;
    }
    const double elapsed = (double)(now_ms - playback->last_update_ms) / 1000.0;
    const double advanced =
        (double)playback->position_seconds + elapsed * (double)playback->playback_rate;
    const double duration = (double)playback->clip.duration_seconds;
    if (!isfinite(advanced) || !isfinite(duration) || duration < 0.0) {
        return false;
    }
    if (duration <= (double)PLAYBACK_EPSILON) {
        *position = 0.0F;
        *ended = !playback->loop;
        return true;
    }
    if (playback->loop) {
        double wrapped = fmod(advanced, duration);
        if (wrapped < 0.0) {
            wrapped += duration;
        }
        *position = (float)wrapped;
        return isfinite(*position);
    }
    if (advanced >= duration) {
        *position = playback->clip.duration_seconds;
        *ended = true;
    } else {
        *position = (float)fmax(advanced, 0.0);
    }
    return isfinite(*position);
}

static void copy_rig_nodes(EidolonMotionRig *destination, const EidolonMotionRig *source) {
    memcpy(destination->nodes, source->nodes, source->node_count * sizeof(*source->nodes));
}

bool eidolon_vrm_playback_take_clip(EidolonVrmPlayback *playback, EidolonVrmaClip *clip,
                                    const char *identity, const EidolonVrmBody *destination,
                                    const EidolonMotionRig *destination_rig,
                                    EidolonVrmRootMotionPolicy root_motion_policy, char *error,
                                    size_t error_capacity) {
    EidolonVrmPlayback candidate;
    const size_t identity_length = identity != NULL ? strlen(identity) : 0U;
    if (playback == NULL || clip == NULL || destination == NULL || destination_rig == NULL ||
        destination_rig->nodes == NULL || destination_rig->node_count == 0U ||
        (playback->version != 0U && playback->version != EIDOLON_VRM_PLAYBACK_VERSION) ||
        identity_length == 0U || identity_length >= EIDOLON_VRM_PLAYBACK_IDENTITY_CAPACITY ||
        (root_motion_policy != EIDOLON_VRM_ROOT_MOTION_IN_PLACE &&
         root_motion_policy != EIDOLON_VRM_ROOT_MOTION_FULL)) {
        set_error(error, error_capacity, "invalid VRM playback initialization request");
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_VRM_PLAYBACK_VERSION;
    candidate.playback_rate = 1.0F;
    candidate.paused = true;
    candidate.needs_sample = true;
    candidate.root_motion_policy = root_motion_policy;
    memcpy(candidate.clip_identity, identity, identity_length + 1U);
    if (!eidolon_vrm_retargeter_init(&candidate.retargeter, clip, destination, destination_rig,
                                     error, error_capacity)) {
        eidolon_vrm_playback_destroy(&candidate);
        return false;
    }
    candidate.candidate = *destination_rig;
    candidate.candidate.nodes =
        calloc(destination_rig->node_count, sizeof(*candidate.candidate.nodes));
    if (candidate.candidate.nodes == NULL) {
        set_error(error, error_capacity, "out of memory allocating VRM playback candidate rig");
        eidolon_vrm_playback_destroy(&candidate);
        return false;
    }
    copy_rig_nodes(&candidate.candidate, destination_rig);
    candidate.clip = *clip;
    memset(clip, 0, sizeof(*clip));
    candidate.ready = true;
    eidolon_vrm_playback_destroy(playback);
    *playback = candidate;
    clear_error(error, error_capacity);
    return true;
}

bool eidolon_vrm_playback_load(EidolonVrmPlayback *playback, const char *path, const char *identity,
                               const EidolonVrmBody *destination,
                               const EidolonMotionRig *destination_rig,
                               EidolonVrmRootMotionPolicy root_motion_policy, char *error,
                               size_t error_capacity) {
    EidolonVrmaClip clip;
    memset(&clip, 0, sizeof(clip));
    if (path == NULL || path[0] == '\0' ||
        !eidolon_vrma_clip_load(path, &clip, error, error_capacity)) {
        if (path == NULL || path[0] == '\0') {
            set_error(error, error_capacity, "VRMA playback path is empty");
        }
        return false;
    }
    const char *resolved_identity = identity != NULL && identity[0] != '\0' ? identity : path;
    const bool loaded =
        eidolon_vrm_playback_take_clip(playback, &clip, resolved_identity, destination,
                                       destination_rig, root_motion_policy, error, error_capacity);
    eidolon_vrma_clip_destroy(&clip);
    return loaded;
}

bool eidolon_vrm_playback_play(EidolonVrmPlayback *playback, uint64_t now_ms) {
    if (!valid_playback(playback)) {
        return false;
    }
    playback->paused = false;
    playback->last_update_ms = now_ms;
    playback->has_clock = true;
    return true;
}

bool eidolon_vrm_playback_pause(EidolonVrmPlayback *playback, uint64_t now_ms) {
    float position;
    bool ended;
    if (!valid_playback(playback) || !timeline_position(playback, now_ms, &position, &ended)) {
        return false;
    }
    playback->position_seconds = position;
    playback->last_update_ms = now_ms;
    playback->has_clock = true;
    playback->paused = true;
    playback->needs_sample = true;
    return true;
}

bool eidolon_vrm_playback_seek(EidolonVrmPlayback *playback, float seconds, uint64_t now_ms) {
    if (!valid_playback(playback) || !isfinite(seconds) || seconds < 0.0F) {
        return false;
    }
    playback->position_seconds = fminf(seconds, playback->clip.duration_seconds);
    playback->last_update_ms = now_ms;
    playback->has_clock = true;
    playback->needs_sample = true;
    return true;
}

bool eidolon_vrm_playback_set_loop(EidolonVrmPlayback *playback, bool loop) {
    if (!valid_playback(playback)) {
        return false;
    }
    playback->loop = loop;
    return true;
}

bool eidolon_vrm_playback_set_rate(EidolonVrmPlayback *playback, float playback_rate,
                                   uint64_t now_ms) {
    float position;
    bool ended;
    if (!valid_playback(playback) || !isfinite(playback_rate) || playback_rate <= 0.0F ||
        playback_rate > PLAYBACK_RATE_MAX ||
        !timeline_position(playback, now_ms, &position, &ended)) {
        return false;
    }
    playback->position_seconds = position;
    playback->playback_rate = playback_rate;
    playback->last_update_ms = now_ms;
    playback->has_clock = true;
    playback->needs_sample = true;
    return true;
}

bool eidolon_vrm_playback_update(EidolonVrmPlayback *playback, uint64_t now_ms,
                                 const EidolonMotionRig *current_rig,
                                 const EidolonMotionRig **candidate, char *error,
                                 size_t error_capacity) {
    float position;
    bool ended;
    char detail[EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY];
    EidolonHumanoidPose pose;
    if (candidate != NULL) {
        *candidate = NULL;
    }
    if (!valid_playback(playback) || current_rig == NULL || current_rig->nodes == NULL ||
        current_rig->node_count != playback->candidate.node_count || candidate == NULL) {
        set_error(error, error_capacity, "VRM playback is unavailable or has an incompatible rig");
        return false;
    }
    if (!timeline_position(playback, now_ms, &position, &ended)) {
        record_failure(playback, error, error_capacity,
                       "VRM playback clock regressed or overflowed");
        return false;
    }
    const bool clock_advanced =
        !playback->paused && (!playback->has_clock || now_ms != playback->last_update_ms);
    if (!playback->needs_sample && !clock_advanced) {
        clear_error(error, error_capacity);
        return true;
    }
    if (!eidolon_vrma_clip_sample(&playback->clip, position, false, &pose, detail,
                                  sizeof(detail))) {
        record_failure(playback, error, error_capacity, "VRMA sample failed: %s", detail);
        return false;
    }
    copy_rig_nodes(&playback->candidate, current_rig);
    if (!eidolon_vrm_retargeter_apply(&playback->retargeter, &pose, playback->root_motion_policy,
                                      &playback->candidate, detail, sizeof(detail))) {
        record_failure(playback, error, error_capacity, "VRM retarget failed: %s", detail);
        return false;
    }
    if (playback->sample_revision == UINT64_MAX) {
        record_failure(playback, error, error_capacity, "VRM playback sample revision overflowed");
        return false;
    }
    playback->position_seconds = position;
    playback->last_update_ms = now_ms;
    playback->has_clock = true;
    playback->needs_sample = false;
    playback->paused = playback->paused || ended;
    playback->sample_revision += 1U;
    *candidate = &playback->candidate;
    clear_error(error, error_capacity);
    return true;
}

bool eidolon_vrm_playback_note_published(EidolonVrmPlayback *playback, uint64_t sample_revision) {
    if (!valid_playback(playback) || sample_revision != playback->sample_revision ||
        sample_revision <= playback->published_revision) {
        return false;
    }
    playback->published_revision = sample_revision;
    return true;
}

void eidolon_vrm_playback_fail(EidolonVrmPlayback *playback, const char *failure) {
    if (playback == NULL || playback->version != EIDOLON_VRM_PLAYBACK_VERSION) {
        return;
    }
    const char *message =
        failure != NULL && failure[0] != '\0' ? failure : "VRM playback publication failed";
    (void)snprintf(playback->failure, sizeof(playback->failure), "%s", message);
    playback->failed = true;
    playback->paused = true;
}

bool eidolon_vrm_playback_report(const EidolonVrmPlayback *playback,
                                 EidolonVrmPlaybackReport *report) {
    if (playback == NULL || report == NULL || playback->version != EIDOLON_VRM_PLAYBACK_VERSION) {
        return false;
    }
    memset(report, 0, sizeof(*report));
    if (playback->ready && !eidolon_vrma_clip_coverage(&playback->clip, &report->coverage)) {
        return false;
    }
    report->version = EIDOLON_VRM_PLAYBACK_VERSION;
    report->state = playback->failed   ? EIDOLON_VRM_PLAYBACK_FAILED
                    : !playback->ready ? EIDOLON_VRM_PLAYBACK_EMPTY
                    : playback->paused ? EIDOLON_VRM_PLAYBACK_PAUSED
                                       : EIDOLON_VRM_PLAYBACK_PLAYING;
    memcpy(report->clip_identity, playback->clip_identity, sizeof(report->clip_identity));
    memcpy(report->failure, playback->failure, sizeof(report->failure));
    report->duration_seconds = playback->clip.duration_seconds;
    report->position_seconds = playback->position_seconds;
    report->phase = playback->clip.duration_seconds > PLAYBACK_EPSILON
                        ? playback->position_seconds / playback->clip.duration_seconds
                        : 0.0F;
    report->playback_rate = playback->playback_rate;
    report->sample_revision = playback->sample_revision;
    report->published_revision = playback->published_revision;
    report->loop = playback->loop;
    return true;
}

void eidolon_vrm_playback_destroy(EidolonVrmPlayback *playback) {
    if (playback == NULL) {
        return;
    }
    free(playback->candidate.nodes);
    eidolon_vrm_retargeter_destroy(&playback->retargeter);
    eidolon_vrma_clip_destroy(&playback->clip);
    memset(playback, 0, sizeof(*playback));
}
