#ifndef EIDOLON_ANIMATION_H
#define EIDOLON_ANIMATION_H

#include "eidolon.h"

void eidolon_animation_set_state(EidolonAnimation *animation, EidolonState state,
                                 uint64_t now_ms);
void eidolon_animation_update(EidolonAnimation *animation, EidolonState state,
                              uint64_t now_ms);
SDL_FRect eidolon_animation_source_rect(const EidolonAnimation *animation);

#endif

