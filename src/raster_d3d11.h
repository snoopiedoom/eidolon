#ifndef EIDOLON_RASTER_D3D11_H
#define EIDOLON_RASTER_D3D11_H

#include <SDL3/SDL.h>

#include "dialogue.h"
#include "portrait.h"
#include "presentation.h"
#include "text_renderer.h"

#if defined(_WIN32)

bool eidolon_d3d11_upload_straight_alpha(EidolonPresentation *presentation,
                                         const EidolonPresentationTargetUpdate *update,
                                         SDL_Surface *surface);
bool eidolon_d3d11_raster_portrait(EidolonPresentation *presentation,
                                   EidolonPortraitRenderer *portrait,
                                   const EidolonPresentationTargetUpdate *update);
bool eidolon_d3d11_raster_dialogue(EidolonPresentation *presentation,
                                   EidolonTextRenderer *text_renderer, EidolonDialogueTheme theme,
                                   const EidolonPresentationTargetUpdate *update,
                                   const SDL_FRect *local_bubble, const EidolonDialogue *dialogue,
                                   const char *title, size_t title_slot, size_t body_slot,
                                   bool points_right);

#endif

#endif
