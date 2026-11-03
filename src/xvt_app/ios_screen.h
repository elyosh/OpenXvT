#ifndef XVT_APP_IOS_SCREEN_H
#define XVT_APP_IOS_SCREEN_H

#include <SDL3/SDL_platform_defines.h>

#if defined(SDL_PLATFORM_IOS)
void XvtIosScreen_Fill(void);
#else
static inline void XvtIosScreen_Fill(void) {}
#endif

#endif
