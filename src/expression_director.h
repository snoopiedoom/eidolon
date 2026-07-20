#ifndef EIDOLON_EXPRESSION_DIRECTOR_H
#define EIDOLON_EXPRESSION_DIRECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "affect.h"

#define EIDOLON_EXPRESSION_BEAT_CAPACITY 32U
#define EIDOLON_EXPRESSION_BEAT_TEXT_CAPACITY 512U

typedef enum EidolonPerformanceCue {
    EIDOLON_PERFORMANCE_CUE_NONE,
    EIDOLON_PERFORMANCE_CUE_ACCENT,
    EIDOLON_PERFORMANCE_CUE_LIFT,
    EIDOLON_PERFORMANCE_CUE_RECOIL,
    EIDOLON_PERFORMANCE_CUE_LEAN,
    EIDOLON_PERFORMANCE_CUE_SURPRISE,
} EidolonPerformanceCue;

typedef enum EidolonBeatBoundaryReason {
    EIDOLON_BEAT_BOUNDARY_END,
    EIDOLON_BEAT_BOUNDARY_LINE,
    EIDOLON_BEAT_BOUNDARY_SENTENCE,
    EIDOLON_BEAT_BOUNDARY_CONTRAST,
    EIDOLON_BEAT_BOUNDARY_LENGTH,
} EidolonBeatBoundaryReason;

typedef enum EidolonCueReason {
    EIDOLON_CUE_REASON_NONE,
    EIDOLON_CUE_REASON_INTERJECTION,
    EIDOLON_CUE_REASON_EXCLAMATION,
    EIDOLON_CUE_REASON_SURPRISE_AFFECT,
    EIDOLON_CUE_REASON_VALENCE_DROP,
    EIDOLON_CUE_REASON_VALENCE_LIFT,
    EIDOLON_CUE_REASON_WARMTH_LIFT,
    EIDOLON_CUE_REASON_DEFAULT_ACCENT,
    EIDOLON_CUE_REASON_SIMILAR_SUPPRESSED,
    EIDOLON_CUE_REASON_TRACK_START,
} EidolonCueReason;

typedef struct EidolonExpressionBeat {
    size_t text_start;
    size_t text_end;
    uint64_t sequence;
    uint64_t submitted_ms;
    uint64_t classified_ms;
    EidolonAffect affect;
    EidolonExpressionIntent expression;
    EidolonExpressionIntent raw_expression;
    EidolonPerformanceCue cue;
    EidolonBeatBoundaryReason boundary_reason;
    EidolonCueReason cue_reason;
    size_t top_emotions[3];
    float top_probabilities[3];
    EidolonExpressionIntent runner_up_expression;
    float expression_margin;
    float previous_expression_advantage;
    float evidence;
    float intensity;
    bool ready;
    bool expression_held;
} EidolonExpressionBeat;

typedef struct EidolonExpressionTrack {
    EidolonExpressionBeat beats[EIDOLON_EXPRESSION_BEAT_CAPACITY];
    size_t count;
    size_t ready_count;
    size_t active_index;
    uint64_t deadline_ms;
    uint64_t track_id;
    uint64_t prepared_ms;
    uint64_t ready_ms;
    uint64_t last_activation_ms;
    char owner[64];
    EidolonState state;
    bool waiting;
    bool complete;
} EidolonExpressionTrack;

typedef struct EidolonPerformanceEvent {
    EidolonAffect affect;
    EidolonExpressionIntent expression;
    EidolonPerformanceCue cue;
    float evidence;
    float intensity;
    size_t beat_index;
} EidolonPerformanceEvent;

void eidolon_expression_track_compile(EidolonExpressionTrack *track, const char *text,
                                      EidolonState state);
bool eidolon_expression_track_set_sequence(EidolonExpressionTrack *track, size_t beat_index,
                                           uint64_t sequence);
bool eidolon_expression_track_copy_text(const EidolonExpressionTrack *track, size_t beat_index,
                                        const char *text, char *output, size_t capacity);
bool eidolon_expression_track_apply(EidolonExpressionTrack *track, uint64_t sequence,
                                    const float probabilities[EIDOLON_GOEMOTIONS_COUNT],
                                    uint64_t now_ms);
void eidolon_expression_track_fallback(EidolonExpressionTrack *track, const char *text);
bool eidolon_expression_track_ready(const EidolonExpressionTrack *track);
bool eidolon_expression_track_event(EidolonExpressionTrack *track, size_t text_offset,
                                    EidolonPerformanceEvent *event);
const char *eidolon_performance_cue_name(EidolonPerformanceCue cue);
const char *eidolon_beat_boundary_reason_name(EidolonBeatBoundaryReason reason);
const char *eidolon_cue_reason_name(EidolonCueReason reason);

#endif
