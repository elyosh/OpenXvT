#ifndef XVT_RUNTIME_SNAPSHOT_RENDER_FRONTEND_H
#define XVT_RUNTIME_SNAPSHOT_RENDER_FRONTEND_H
#include "xvt/frontend/front_image.h"
#include "xvt_runtime/snapshot/render_snapshot.h"
#ifdef __cplusplus
extern "C" {
#endif
void XvtRenderFrontend_Init(void);
void XvtRenderFrontend_BeginDraw(void);
void XvtRenderFrontend_TextEntry(int field);
void XvtRenderFrontend_TextFieldPressed(void);
unsigned XvtRenderFrontend_Target(void);
void XvtRenderFrontend_Select(unsigned target);
void XvtRenderFrontend_Suppress(int begin);
void XvtRenderFrontend_Image(const ImageResource* image, int sx, int sy, int x, int y, int width, int height,
							 unsigned kind, unsigned tint);
void XvtRenderFrontend_Glyph(const ImageResource* glyph, int x, int y, unsigned color, int remap);
void XvtRenderFrontend_Paint(unsigned kind, int x0, int y0, int x1, int y1, unsigned color);
void XvtRenderFrontend_Copy(unsigned source, unsigned target);
void XvtRenderFrontend_Clear(unsigned target, unsigned color);
void XvtRenderFrontend_Present(void);
void XvtRenderFrontend_Reset(void);
void XvtRenderFrontend_ReleaseSurfaces(void);
void XvtRenderFrontend_Screen(int slot, int restore);
void XvtRenderFrontend_Cursor(int restore);
void XvtRenderFrontend_EndCursor(void);
void XvtRenderFrontend_FontLoaded(const BitmapFont* font);
void XvtRenderFrontend_Movie(int begin);
void XvtRenderFrontend_Commit(XvtRenderSnapshot* snapshot);
void XvtRenderFrontend_PresentedScene(XvtSceneKind kind);
void XvtRenderFrontend_FlightUiScene(XvtSceneKind kind);
#ifdef __cplusplus
}
#endif
#endif
