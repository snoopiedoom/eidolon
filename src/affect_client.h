#ifndef EIDOLON_AFFECT_CLIENT_H
#define EIDOLON_AFFECT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "affect.h"

typedef struct EidolonAffectClient EidolonAffectClient;

EidolonAffectClient *eidolon_affect_client_create(const char *worker_path);
void eidolon_affect_client_destroy(EidolonAffectClient *client);
bool eidolon_affect_client_submit(EidolonAffectClient *client, uint64_t sequence, const char *text);
bool eidolon_affect_client_poll(EidolonAffectClient *client, uint64_t *sequence,
                                float probabilities[EIDOLON_GOEMOTIONS_COUNT]);
bool eidolon_affect_client_available(EidolonAffectClient *client);

#endif
