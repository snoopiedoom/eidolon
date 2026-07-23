#ifndef EIDOLON_DIALOGUE_ART_H
#define EIDOLON_DIALOGUE_ART_H

#include <SDL3/SDL.h>

#include "dialogue.h"
#include "portrait.h"
#include "text_renderer.h"

void eidolon_dialogue_art_draw_renderer(SDL_Renderer *renderer, EidolonTextRenderer *text_renderer,
                                        EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                        const EidolonDialogue *dialogue, const char *title,
                                        size_t title_slot, size_t body_slot, float opacity,
                                        bool points_right);
bool eidolon_dialogue_art_draw_surface(SDL_Surface *surface, EidolonTextRenderer *text_renderer,
                                       EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                       const EidolonDialogue *dialogue, const char *title,
                                       size_t title_slot, size_t body_slot, bool points_right);

#endif
