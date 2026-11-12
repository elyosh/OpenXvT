#ifndef XVT_REMASTER_LIGHTING_H
#define XVT_REMASTER_LIGHTING_H
#include "xvt_remaster/ship.h"
#ifdef __cplusplus
extern "C" {
#endif
int XvtLighting_Begin(AeronScene3D* scene, const XvtRenderSnapshot* snapshot);
/* Adds a classic point light: `color` times `intensity` lights a surface by
 * 0.5 * c / d under the classic law. */
void XvtLighting_AddPoint(AeronScene3D* scene, const float position[3], const float color[3],
						  float intensity);
#ifdef __cplusplus
}
#endif
#endif
