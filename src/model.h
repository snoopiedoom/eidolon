#ifndef EIDOLON_MODEL_H
#define EIDOLON_MODEL_H

#include <SDL3/SDL.h>

typedef struct EidolonModelRenderer EidolonModelRenderer;

EidolonModelRenderer *eidolon_model_create(SDL_Renderer *renderer, const char *model_path,
                                            const char *shader_directory);
void eidolon_model_destroy(EidolonModelRenderer *model);
SDL_Texture *eidolon_model_texture(const EidolonModelRenderer *model);

#endif
