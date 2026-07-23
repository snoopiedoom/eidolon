#ifndef EIDOLON_WINDOWS_DCOMP_H
#define EIDOLON_WINDOWS_DCOMP_H

#include <stdbool.h>
#include <stdint.h>

#include "presentation.h"

#if defined(_WIN32)
#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EidolonWin32DcompConfig {
    const char *title;
    int x;
    int y;
    int width;
    int height;
    bool visible;
} EidolonWin32DcompConfig;

EidolonPresentation *eidolon_win32_dcomp_presentation_create(const EidolonWin32DcompConfig *config);
ID3D11Device *eidolon_win32_dcomp_device(EidolonPresentation *presentation);
ID3D11DeviceContext *eidolon_win32_dcomp_device_context(EidolonPresentation *presentation);
ID3D11Texture2D *eidolon_win32_dcomp_target_texture(EidolonPresentation *presentation,
                                                    EidolonPresentationTarget target,
                                                    uint64_t generation);

#ifdef __cplusplus
}
#endif
#endif

#endif
