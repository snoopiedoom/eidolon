#ifndef EIDOLON_AFFECT_PROTOCOL_H
#define EIDOLON_AFFECT_PROTOCOL_H

#include <stdint.h>

#define EIDOLON_AFFECT_PROTOCOL_MAGIC UINT32_C(0x45414646)
#define EIDOLON_AFFECT_PROTOCOL_VERSION UINT32_C(1)
#define EIDOLON_AFFECT_TEXT_CAPACITY UINT32_C(4096)

typedef struct EidolonAffectRequestHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    uint32_t text_length;
} EidolonAffectRequestHeader;

typedef struct EidolonAffectResponse {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    uint32_t status;
    float probabilities[28];
} EidolonAffectResponse;

#endif
