#ifndef XVT_RUNTIME_PRESENTATION_H
#define XVT_RUNTIME_PRESENTATION_H
#include "aeron/aeron.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XVT_CLASSIC_WIDTH 640
#define XVT_CLASSIC_HEIGHT 480

void XvtPresentation_Init(void);
/* Logical size tracks the inset box when the drawable is known, otherwise the
 * fallback window size. Height stays 480. */
void XvtPresentation_SyncToAvailable(int width, int height);
AeronRectI XvtPresentation_Frame(void);
AeronRectI XvtPresentation_ClassicRect(void);
AeronRectI XvtPresentation_FromClassic(AeronRectI rect);
int XvtPresentation_MouseToClassic(const AeronInputSnapshot* input, int* x, int* y);
int XvtPresentation_WarpClassic(int x, int y);
/* Called before standalone UI, surface replacement and task teardown draws. */
void XvtPresentation_RequireClassic(void);
/* The movie owns this host frame even if playback ends during its tick. */
void XvtPresentation_EndFrame(int movie_presented);
void XvtPresentation_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
