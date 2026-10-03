#ifndef XVT_RUNTIME_SNAPSHOT_RENDER_CAPTURE_H
#define XVT_RUNTIME_SNAPSHOT_RENDER_CAPTURE_H
#include "xvt_runtime/snapshot/render_snapshot.h"
#ifdef __cplusplus
extern "C" {
#endif
void XvtRenderCapture_BeginOverlay(void);
void XvtRenderCapture_EndOverlay(void);
void XvtRenderCapture_Init(void);
void XvtRenderCapture_BeginTick(void);
void XvtRenderCapture_BeginMission(void);
void XvtRenderCapture_EndMission(void);
void XvtRenderCapture_WorldChanged(void);
int XvtRenderCapture_LastViewTick(void);
void XvtRenderCapture_CheckNetworkCorrection(void);
void XvtRenderCapture_CompleteNetworkWorld(void);
/* Cumulative count of prediction corrections that reset world history. */
unsigned XvtRenderCapture_NetworkCorrections(void);
void XvtRenderCapture_CaptureView(void);
void XvtRenderCapture_BeginClassicFrame(void);
void XvtRenderCapture_FrontendPreview(uint16_t handle, const float position[3], const float orientation[9],
									  float scale, uint16_t node_switch, int x, int y, int width, int height);
void XvtRenderCapture_Crt(int x, int y, int width, int height, int masked);
void XvtRenderCapture_CrtMarker(int x, int y, int z);
void XvtRenderCapture_Hyperspace(unsigned count, const int* x, const int* y, const int* z, const int* width,
								 const int* roll);
uint32_t XvtRenderSnapshot_NextOrder(void);
void XvtRenderCapture_SealView(void);
void XvtRenderCapture_Presented(int succeeded);
void XvtRenderCapture_EndPresentation(void);
void XvtRenderCapture_Commit(XvtRenderSnapshot* out, const XvtRenderSnapshot* previous);
/* Capture-side access only; invalid outside an open host tick. */
XvtRenderSnapshot* XvtRenderSnapshot_Writer(void);
#ifdef __cplusplus
}
#endif
#endif
