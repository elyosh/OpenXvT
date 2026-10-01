#include "xvt/flight/flight_view.h"

#ifdef XVT_MODERN
#include "xvt_runtime/snapshot/cockpit_capture.h"
#include "xvt_runtime/snapshot/render_capture.h"
#endif
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_display.h"
#include "xvt/flight/flight_hyperspace.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/fview.h"
#include "xvt/flight/hud/flight_map.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/object/damage.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/proving_grounds.h"
#include "xvt/flight/targeting.h"
#include "xvt/flight/transfm2.h"
#include "xvt/math/math.h"
#include "xvt/math/trig2.h"
#include "xvt/net/flight_net.h"
#include "xvt/render/backdrop.h"
#include "xvt/render/flight_light.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/render_list.h"
#include "xvt/render/render_scene.h"
#include "xvt/render/renderer.h"
#include "xvt/render/scene_billboard.h"
#include "xvt/render/std3d.h"
#include "xvt/render/sw3d.h"
#include "xvt/util/time.h"
#include <limits.h>
#include <string.h>

// GLOBAL: XVT 0x9FD398
int g_camRelWorldZ = 0;
// GLOBAL: XVT 0x9FD39C
int g_camRelWorldX = 0;
// GLOBAL: XVT 0x9FD3A0
int g_camRelWorldY = 0;
// GLOBAL: XVT 0x9A8D9C
int g_currentObjectBoundsExtent = 0;
// GLOBAL: XVT 0x9CD264
uint16_t g_flightInitialTextureCacheFlushPending = 0;
// GLOBAL: XVT 0x9A7BA8
uint16_t g_flightRenderDurationTicks = 0;
// GLOBAL: XVT 0x9E9660
uint16_t g_flightRenderScratchWord = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x40B7B0
HRESULT FlightView_CompositeMaskedSoftwareSurface(void) {
	DDSURFACEDESC surfaceDesc;
	HRESULT lockResult;
	uint16_t* backBufferPixels;
	uint16_t* offscreenPixels;
	uint16_t* destination;
	uint16_t* source;
	uint8_t* maskCursor;
	int backBufferPitch;
	int offscreenPitch;
	int8_t runType;
	int runLength;
	int decodedWidth;
	int copyWidth;
	int viewportRow;
	unsigned int row;

	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	surfaceDesc.dwSize = sizeof(surfaceDesc);
	for (;;) {
		lockResult = g_flightBackBuffer->lpVtbl->Lock(g_flightBackBuffer, NULL, &surfaceDesc, 0, NULL);
		if (lockResult == 0)
			break;
		if (lockResult != DX_DDERR_WASSTILLDRAWING) {
			return lockResult;
		}
	}
	destination = surfaceDesc.lpSurface;
	backBufferPixels = destination;
	backBufferPitch = surfaceDesc.lPitch;

	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	surfaceDesc.dwSize = sizeof(surfaceDesc);
	for (;;) {
		lockResult =
			g_flightOffscreenSurface->lpVtbl->Lock(g_flightOffscreenSurface, NULL, &surfaceDesc, 0, NULL);
		if (lockResult == 0)
			break;
		if (lockResult != DX_DDERR_WASSTILLDRAWING) {
			return lockResult;
		}
	}
	source = surfaceDesc.lpSurface;
	offscreenPixels = source;
	offscreenPitch = surfaceDesc.lPitch;

	destination = (uint16_t*)((uint8_t*)destination +
							  g_flight16bppBytesPerPixel * ((unsigned int)(width - g_surfaceWidth) >> 1));
	destination = (uint16_t*)((uint8_t*)destination +
							  backBufferPitch * ((unsigned int)(height - g_surfaceHeight) >> 1));

	for (row = 0; row < (unsigned int)g_flightVpY; ++row) {
		if ((g_surfaceWidth & 1) != 0) {
			*destination = *source;
			if (g_surfaceWidth != 0) {
				memcpy(destination + 1, source + 1, g_flight16bppBytesPerPixel * (g_surfaceWidth - 1));
			}
		} else {
			copyWidth = g_surfaceWidth;
			memcpy(destination, source, copyWidth * g_flight16bppBytesPerPixel);
		}
		source = (uint16_t*)((uint8_t*)source + offscreenPitch);
		destination = (uint16_t*)((uint8_t*)destination + backBufferPitch);
	}

	maskCursor = &g_flightAuxBuffer[g_viewportSpanMaskOffset];
	for (viewportRow = 0; viewportRow < g_flightVpHeight; ++viewportRow) {
		if (g_flightVpX != 0) {
			if ((g_flightVpX & 1) != 0) {
				*destination = *source;
				if (g_flightVpX != 0) {
					memcpy(destination + 1, source + 1, g_flight16bppBytesPerPixel * (g_flightVpX - 1));
				}
			} else {
				memcpy(destination, source, g_flight16bppBytesPerPixel * g_flightVpX);
			}
			source = (uint16_t*)((uint8_t*)source + g_flight16bppBytesPerPixel * g_flightVpX);
			destination = (uint16_t*)((uint8_t*)destination + g_flight16bppBytesPerPixel * g_flightVpX);
		}
		decodedWidth = 0;
		runType = (int8_t)*maskCursor++;
		while (decodedWidth < g_flightVpWidth) {
			runLength = *maskCursor++;
			if (runLength == 0) {
				runLength = *maskCursor++;
				if (runLength == 0) {
					runLength = *maskCursor++ + 256;
				}
				runLength += 255;
			}
			if (runType < 0) {
				if ((runLength & 1) != 0) {
					*destination = *source;
					if (runLength != 0) {
						memcpy(destination + 1, source + 1, g_flight16bppBytesPerPixel * (runLength - 1));
					}
				} else {
					memcpy(destination, source, g_flight16bppBytesPerPixel * runLength);
				}
			}
			source = (uint16_t*)((uint8_t*)source + g_flight16bppBytesPerPixel * runLength);
			destination = (uint16_t*)((uint8_t*)destination + g_flight16bppBytesPerPixel * runLength);
			decodedWidth += runLength;
			runType = -runType;
		}

		decodedWidth += g_flightVpX;
		if ((unsigned int)g_surfaceWidth > (unsigned int)decodedWidth) {
			runLength = g_surfaceWidth - decodedWidth;
			if ((runLength & 1) != 0) {
				*destination = *source;
				if (runLength != 0) {
					memcpy(destination + 1, source + 1, g_flight16bppBytesPerPixel * (runLength - 1));
				}
			} else {
				memcpy(destination, source, g_flight16bppBytesPerPixel * runLength);
			}
			source = (uint16_t*)((uint8_t*)source + g_flight16bppBytesPerPixel * runLength);
			destination = (uint16_t*)((uint8_t*)destination + g_flight16bppBytesPerPixel * runLength);
			decodedWidth = g_surfaceWidth;
		}
		source = (uint16_t*)((uint8_t*)source + offscreenPitch - g_flight16bppBytesPerPixel * decodedWidth);
		destination =
			(uint16_t*)((uint8_t*)destination + backBufferPitch - g_flight16bppBytesPerPixel * decodedWidth);
	}

	for (row = g_flightVpY + g_flightVpHeight; (unsigned int)g_surfaceHeight > row; ++row) {
		if ((g_surfaceWidth & 1) != 0) {
			*destination = *source;
			if (g_surfaceWidth != 0) {
				memcpy(destination + 1, source + 1, g_flight16bppBytesPerPixel * (g_surfaceWidth - 1));
			}
		} else {
			copyWidth = g_surfaceWidth;
			memcpy(destination, source, copyWidth * g_flight16bppBytesPerPixel);
		}
		source = (uint16_t*)((uint8_t*)source + offscreenPitch);
		destination = (uint16_t*)((uint8_t*)destination + backBufferPitch);
	}

	g_flightOffscreenSurface->lpVtbl->Unlock(g_flightOffscreenSurface, offscreenPixels);
#ifdef XVT_MODERN
	lockResult = g_flightBackBuffer->lpVtbl->Unlock(g_flightBackBuffer, backBufferPixels);
	if (lockResult == DX_DD_OK)
		XvtCockpit_LatchComposition();
	return lockResult;
#else
	return g_flightBackBuffer->lpVtbl->Unlock(g_flightBackBuffer, backBufferPixels);
#endif
}

// FUNCTION: XVT 0x438330
int16_t FlightView_RotateViewByInput(int angleQ16, int rollAngleQ16, int playerIdx) {
	int16_t pitch;
	int16_t yaw;
	int16_t pitchCos;
	int16_t pitchSin;
	int16_t yawCos;
	int16_t yawSin;
	int16_t pitchCosYawCos;
	int16_t pitchCosYawSin;
	int16_t pitchSinYawCos;
	int16_t pitchSinYawSin;
	int negativeYawSin;
	int negativePitchSin;
	int16_t rotatedX;
	int16_t rotatedY;
	int16_t rotatedZ;
	int16_t result;

	FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll, g_players[playerIdx].viewState.viewPitch,
							g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL);
	g_curMatR2_X = -g_fviewForwardX_Q15;
	g_curMatR2_Y = -g_fviewForwardY_Q15;
	g_curMatR1_X = g_fviewUpX_Q15;
	g_curMatR1_Y = g_fviewUpY_Q15;
	g_curMatR2_Z = -g_fviewForwardZ_Q15;
	g_curMatR1_Z = g_fviewUpZ_Q15;
	g_curMatR0_X = g_fviewSideX_Q15;
	g_curMatR0_Y = g_fviewSideY_Q15;
	g_curMatR0_Z = g_fviewSideZ_Q15;
	FVIEW_transformaxes(g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, (int16_t)angleQ16);
	if ((g_flightKeyMods & 0xE) != 2)
		FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, (int16_t)rollAngleQ16);

	pitch = trig2_w_arccos((int16_t)-g_curMatR2_Z);
	yaw = (int16_t)-trig2_arctan(g_curMatR2_X, -g_curMatR2_Y);
	g_players[playerIdx].viewState.viewYaw = yaw;
	g_players[playerIdx].viewState.viewPitch = pitch;
	yawCos = trig2_getsignedcos(yaw);
	yawSin = trig2_getsignedsin(yaw);
	pitchCos = trig2_getsignedcos(pitch);
	pitchSin = trig2_getsignedsin(pitch);
	pitchCosYawSin = (int16_t)Math_MulQ15(yawSin, pitchCos);
	pitchSinYawSin = (int16_t)Math_MulQ15(yawSin, pitchSin);
	pitchCosYawCos = (int16_t)Math_MulQ15(yawCos, pitchCos);
	pitchSinYawCos = (int16_t)Math_MulQ15(yawCos, pitchSin);
	negativeYawSin = (int16_t)-yawSin;
	negativePitchSin = (int16_t)-pitchSin;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR0_X = rotatedX;
	g_curMatR0_Y = rotatedY;
	g_curMatR0_Z = rotatedZ;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR1_X = rotatedX;
	g_curMatR1_Y = rotatedY;
	g_curMatR1_Z = rotatedZ;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR2_X = rotatedX;
	g_curMatR2_Y = rotatedY;
	g_curMatR2_Z = rotatedZ;

	result = (int16_t)-trig2_arctan(g_curMatR0_Y, g_curMatR0_X);
	g_players[playerIdx].viewState.viewRoll = result;
	if ((g_flightKeyMods & 0xE) == 2) {
		result = (int16_t)(result + rollAngleQ16);
		g_players[playerIdx].viewState.viewRoll = result;
	}
	return result;
}

// FUNCTION: XVT 0x44EB60
void FlightView_UpdatePlayerCamera(int playerIdx) {
	enum {
		MAX_UNSCALED_EXTENT = INT16_MAX,
		HYPERSPACE_PHASE_TRANSITION = 2,
		HYPERSPACE_EXTERNAL_CAMERA_TICKS = 0x213,
		HYPERSPACE_CAMERA_ROLL_START_TICKS = 0x24E,
		HYPERSPACE_CAMERA_ROLL_RATE = 8,
		HYPERSPACE_CAMERA_ROLL_BIAS = 4720,
		CAMERA_PITCH_DOWN = 0x4000,
	};

	uint16_t cameraFocusObjIdx;

	if (g_players[playerIdx].mapCameraState != 0) {
		FlightMap_UpdateCamera(playerIdx);
	} else {
		cameraFocusObjIdx = g_players[playerIdx].viewState.cameraFocusObjIdx;
		if (cameraFocusObjIdx == UINT16_MAX) {
#ifdef XVT_MODERN
			/* Mission time-limit expiry marks empty slots connected; they have no craft to orbit. */
			if (g_players[playerIdx].objectIndex == -1)
				return;
#endif
			ObjectRecord* playerObject = &g_objectTable[g_players[playerIdx].objectIndex];

			trig2_ctop(playerObject->world_x - g_players[playerIdx].viewState.savedTargetX,
					   playerObject->world_y - g_players[playerIdx].viewState.savedTargetY,
					   playerObject->world_z - g_players[playerIdx].viewState.savedTargetZ);
			g_players[playerIdx].viewState.viewRoll = 0;
			g_players[playerIdx].viewState.viewPitch = pitchQ16;
			g_players[playerIdx].viewState.viewYaw = trig2_xyangle;
			FVIEW_BuildCameraOrient(
				g_players[playerIdx].viewState.viewRoll, g_players[playerIdx].viewState.viewPitch,
				g_players[playerIdx].viewState.viewYaw, 0, g_players[playerIdx].viewState.hudAimX,
				g_players[playerIdx].viewState.hudAimY, NULL);
		} else if ((g_players[playerIdx].viewState.externalCameraActive != 0 &&
					g_players[playerIdx].viewState.transitionTimer == 0) ||
				   (g_players[playerIdx].viewState.transitionTimer != 0 &&
					g_players[playerIdx].viewState.playerInputBlocked != 0)) {
			int extentShift;
			int cameraOffsetX;
			int cameraOffsetY;
			int cameraOffsetZ;

			g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
			g_players[playerIdx].viewState.viewPitch =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
			g_players[playerIdx].viewState.viewYaw =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
			FVIEW_BuildCameraOrient(
				g_players[playerIdx].viewState.viewRoll, g_players[playerIdx].viewState.viewPitch,
				g_players[playerIdx].viewState.viewYaw, 0, g_players[playerIdx].viewState.hudAimX,
				g_players[playerIdx].viewState.hudAimY, NULL);

			Mission_ResolveObjectOrMissionPointWorldLoc(g_players[playerIdx].viewState.cameraFocusObjIdx, 0);
			g_players[playerIdx].viewState.savedTargetX = worldlocx;
			g_players[playerIdx].viewState.savedTargetY = worldlocy;
			g_players[playerIdx].viewState.savedTargetZ = worldlocz;

			g_players[playerIdx].viewState.savedTargetX -=
				Math_MulQ15(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_X);
			g_players[playerIdx].viewState.savedTargetY -=
				Math_MulQ15(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Y);
			g_players[playerIdx].viewState.savedTargetZ -=
				Math_MulQ15(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Z);

			extentShift = 0;
			g_currentObjectBoundsExtent =
				g_modelTypeTable[g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].objectType]
					.maxBoundsExtent;
			while (g_currentObjectBoundsExtent > MAX_UNSCALED_EXTENT) {
				++extentShift;
				g_currentObjectBoundsExtent >>= 1;
			}

			cameraOffsetX = Math_MulQ15(g_currentObjectBoundsExtent, g_camMatR2_X);
			cameraOffsetY = Math_MulQ15(g_currentObjectBoundsExtent, g_camMatR2_Y);
			cameraOffsetZ = Math_MulQ15(g_currentObjectBoundsExtent, g_camMatR2_Z);
			if (extentShift != 0) {
				cameraOffsetX <<= extentShift;
				cameraOffsetY <<= extentShift;
				cameraOffsetZ <<= extentShift;
			}
			g_players[playerIdx].viewState.savedTargetX -= cameraOffsetX;
			g_players[playerIdx].viewState.savedTargetY -= cameraOffsetY;
			g_players[playerIdx].viewState.savedTargetZ -= cameraOffsetZ;
		} else if (g_players[playerIdx].viewState.transitionTimer != 0) {
			Hud_PointCamera(cameraFocusObjIdx, 0, playerIdx);
		} else {
			g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
			g_players[playerIdx].viewState.viewPitch =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
			g_players[playerIdx].viewState.viewYaw =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
			FVIEW_BuildCameraOrient(
				g_players[playerIdx].viewState.viewRoll, g_players[playerIdx].viewState.viewPitch,
				g_players[playerIdx].viewState.viewYaw, g_players[playerIdx].viewState.viewAngleD,
				g_players[playerIdx].viewState.hudAimX, g_players[playerIdx].viewState.hudAimY,
				&g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]);
			g_players[playerIdx].viewState.savedTargetX =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_x;
			g_players[playerIdx].viewState.savedTargetY =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_y;
			g_players[playerIdx].viewState.savedTargetZ =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_z;
			if (g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].playerOwnerIdx == playerIdx) {
				g_players[playerIdx].viewState.savedTargetX += g_players[playerIdx].hardpointWorldX;
				g_players[playerIdx].viewState.savedTargetY += g_players[playerIdx].hardpointWorldY;
				g_players[playerIdx].viewState.savedTargetZ += g_players[playerIdx].hardpointWorldZ;
			}
		}
	}

	if (g_players[playerIdx].hyperspacePhase == HYPERSPACE_PHASE_TRANSITION &&
		g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks >= HYPERSPACE_EXTERNAL_CAMERA_TICKS) {
		if ((unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx != UINT_MAX) {
			g_players[playerIdx].viewState.externalCameraActive = 1;
			Hud_SetHudViewState(HUD_VIEW_FULL_SCREEN, playerIdx);
			g_players[playerIdx].viewState.cameraFocusObjIdx = UINT16_MAX;
		}
		if (g_players[playerIdx].objectIndex != -1) {
			int16_t hyperspaceCameraRoll;
			int cameraDropTicks;
			int cameraY;

			hyperspaceCameraRoll = 0;
			if (g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks >=
				HYPERSPACE_CAMERA_ROLL_START_TICKS) {
				hyperspaceCameraRoll =
					(int16_t)(HYPERSPACE_CAMERA_ROLL_RATE *
								  g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks -
							  HYPERSPACE_CAMERA_ROLL_BIAS);
			}
			FVIEW_BuildCameraOrient(hyperspaceCameraRoll, CAMERA_PITCH_DOWN, 0, 0, 0, 0, NULL);
			cameraY =
				g_objectTable[g_players[playerIdx].objectIndex].world_y -
				g_modelTypeTable[g_objectTable[g_players[playerIdx].objectIndex].objectType].maxBoundsExtent;
			g_players[playerIdx].viewState.savedTargetY = cameraY;
			cameraDropTicks = (int)g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks -
							  HYPERSPACE_EXTERNAL_CAMERA_TICKS;
			if (cameraDropTicks > SIMULATION_TICKS_PER_SECOND)
				cameraDropTicks = SIMULATION_TICKS_PER_SECOND;
			g_players[playerIdx].viewState.savedTargetY = cameraY - cameraDropTicks * cameraDropTicks;
		}
	}
}

// FUNCTION: XVT 0x44F140
void FlightView_Render(void) {
	enum {
		HYPERSPACE_PHASE_STARTING = 1,
		HYPERSPACE_PHASE_TRANSITION = 2,
		HYPERSPACE_TRANSITION_RENDER_TICKS = 0x213,
		BACKDROP_BILLBOARD_TYPE_INDEX = 0x3000,
		NO_BILINEAR_OBJECT_TYPE = 36,
	};

	int16_t mainObjectIndex;
#ifdef XVT_MODERN
	XvtRenderCapture_CaptureView();
#endif

	if (g_players[g_localPlayer].mapCameraState != 0) {
		g_flightLockBackBufferForHudDraw = 0;
		g_flightSurfaceAlreadyLocked = 0;
		FlightMap_RenderView();
		if (g_useHardware3D != 0) {
			FlightView_CompositeMaskedSoftwareSurface();
		}
		Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
		FlightSurface_Lock();
		Hud_BlitSoftwareHudTextPanes();
		Hud_BlitSoftwareMfdPages();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = 1;
		return;
	}

	if (g_players[g_localPlayer].hyperspacePhase == HYPERSPACE_PHASE_TRANSITION) {
		if (g_players[g_localPlayer].hyperspaceRuntime.phaseElapsedTicks <
			HYPERSPACE_TRANSITION_RENDER_TICKS) {
			g_flightLockBackBufferForHudDraw = 0;
			g_flightSurfaceAlreadyLocked = 0;
			RenderScene_Initialize(1);
			FlightHyperspace_RenderTransitionEffect();
			sw3d_DrawVisibleFacesToSurface();
			if (g_useHardware3D != 0) {
				FlightView_CompositeMaskedSoftwareSurface();
			}
			Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
			FlightSurface_Lock();
			Hud_BlitSoftwareHudTextPanes();
			Hud_BlitSoftwareMfdPages();
			FlightSurface_Unlock();
			g_flightLockBackBufferForHudDraw = 1;
			return;
		}
	} else if (g_players[g_localPlayer].hyperspacePhase == HYPERSPACE_PHASE_STARTING) {
		FlightHyperspace_RequestTransitionEffectInitialization();
	}

	g_flightLockBackBufferForHudDraw = 0;
	g_flightSurfaceAlreadyLocked = 0;
	if (g_flightInitialTextureCacheFlushPending != 0) {
		if (g_useHardware3D != 0) {
			std3D_FlushTextureCache();
		}
		g_flightInitialTextureCacheFlushPending = 0;
	}
	RenderScene_Initialize(1);
	g_sceneBillboardQueueCount = 0;
	if (g_useHardware3D != 0) {
		g_billboardObjectOrTypeIndex = BACKDROP_BILLBOARD_TYPE_INDEX;
		Backdrop_RenderCurrentRegion();
		FlightSurface_Lock();
		FlightStarfield_Render();
		FlightSurface_Unlock();
	}

	RenderList_Reset();
	mainObjectIndex = 0;
	if (g_regionMainObjectSlotEnd > 0) {
		do {
			ObjectRecord* object;
			uint16_t objectType;

			if (mainObjectIndex == g_localTransientSlotStart &&
				(g_debrisEnabled == 0 || g_flightMissionState.provingGroundsModeActive != 0 ||
				 g_players[g_localPlayer].hyperspacePhase == HYPERSPACE_PHASE_TRANSITION)) {
				mainObjectIndex = (int16_t)g_localDebrisSlotEnd;
				if (mainObjectIndex == g_regionMainObjectSlotEnd) {
					break;
				}
			}
			if (g_players[g_localPlayer].viewState.cameraFocusObjIdx != mainObjectIndex ||
				g_players[g_localPlayer].viewState.externalCameraActive != 0 || g_replayViewMode != 0) {
				object = &g_objectTable[mainObjectIndex];
				objectType = object->objectType;
				if (objectType != 0) {
					g_currentObjectBoundsExtent = g_modelTypeTable[objectType].maxBoundsExtent;
					switch (object->genusId) {
						case CRAFT_GENUS_STARFIGHTER:
						case CRAFT_GENUS_TRANSPORT:
						case CRAFT_GENUS_UTILITY_VEHICLE:
						case CRAFT_GENUS_FREIGHTER:
						case CRAFT_GENUS_STARSHIP:
						case CRAFT_GENUS_PLATFORM:
						case CRAFT_GENUS_OBSTACLE:
							g_curCraft = object->mobj->pCraft;
							if (FlightView_IsObjectSphereVisible(mainObjectIndex,
																 g_currentObjectBoundsExtent) != 0) {
								RenderList_QueueObject(mainObjectIndex, depthZ);
							}
							break;
						case CRAFT_GENUS_PLAYER_PROJECTILE:
						case CRAFT_GENUS_OTHER_PROJECTILE:
							if (FlightView_IsObjectSphereVisible(mainObjectIndex,
																 g_currentObjectBoundsExtent) != 0) {
								RenderList_QueueObject(mainObjectIndex, depthZ);
							}
							break;
						case CRAFT_GENUS_SMALL_DEBRIS:
							if (FlightView_IsObjectSphereVisible(mainObjectIndex,
																 g_currentObjectBoundsExtent) != 0) {
								RenderList_QueueObject(mainObjectIndex, depthZ);
							}
							break;
						case CRAFT_GENUS_EXPLOSION:
							if (FlightView_IsObjectSphereVisible(mainObjectIndex,
																 g_currentObjectBoundsExtent) != 0) {
								RenderList_QueueObject(mainObjectIndex, depthZ);
							}
							break;
						default:
							break;
					}
				}
			}
			++mainObjectIndex;
		} while (mainObjectIndex < g_regionMainObjectSlotEnd);
	}

	{
		int16_t staticObjectIndex;

		for (staticObjectIndex = (int16_t)g_regionMainObjectSlotEnd;
			 staticObjectIndex < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd;
			 ++staticObjectIndex) {
			ObjectRecord* object = &g_objectTable[staticObjectIndex];
			int genusId;
			uint16_t objectType;

			objectType = object->objectType;
			if (objectType == 0) {
				continue;
			}
			g_currentObjectBoundsExtent = g_modelTypeTable[objectType].maxBoundsExtent;
			genusId = object->genusId;
			if (genusId >= CRAFT_GENUS_MINE && genusId <= CRAFT_GENUS_SMALL_DEBRIS &&
				FlightView_CullWorldSphereToViewport(object->world_x, object->world_y, object->world_z,
													 g_currentObjectBoundsExtent) != 0) {
				RenderList_QueueObject(staticObjectIndex, depthZ);
			}
		}
	}

	RenderList_SortDepthAscending();
	{
		RenderObjectListEntry* renderEntry;
		int16_t renderObjectIndex;
		int objectTableIndex;
		uint16_t genusId;

		for (renderEntry = g_renderListHead; renderEntry != NULL; renderEntry = renderEntry->next) {
			renderObjectIndex = (int16_t)renderEntry->objectIdx;
			objectTableIndex = renderObjectIndex;
			genusId = g_objectTable[objectTableIndex].genusId;
			if (g_regionMainObjectSlotEnd > renderObjectIndex) {
				switch ((int)genusId) {
					case CRAFT_GENUS_STARFIGHTER:
					case CRAFT_GENUS_TRANSPORT:
					case CRAFT_GENUS_UTILITY_VEHICLE:
					case CRAFT_GENUS_FREIGHTER:
					case CRAFT_GENUS_STARSHIP:
					case CRAFT_GENUS_PLATFORM:
					case CRAFT_GENUS_OBSTACLE: {
						int savedBilinearEnabled;

						g_curCraft = g_objectTable[objectTableIndex].mobj->pCraft;
						FlightView_ComputeObjectViewPosition(renderObjectIndex);
						if (genusId == CRAFT_GENUS_OBSTACLE) {
							g_transformLightDirectionToObjectSpace = 0;
						}
						if (g_objectTable[objectTableIndex].objectType == NO_BILINEAR_OBJECT_TYPE) {
							savedBilinearEnabled = g_bilinearEnabled;
							g_bilinearEnabled = 0;
						}
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						if (genusId == CRAFT_GENUS_OBSTACLE) {
							ProvingGrounds_DrawCourseObject(renderObjectIndex);
						} else {
							FlightLight_SetupObjectLighting(&g_objectTable[objectTableIndex]);
							Damage_QueueCraftBillboards(renderObjectIndex);
							RenderScene_DrawObjectModel(&g_objectTable[objectTableIndex]);
							g_objectPointLightCount = 0;
						}
						if (g_objectTable[objectTableIndex].objectType == NO_BILINEAR_OBJECT_TYPE) {
							g_bilinearEnabled = savedBilinearEnabled;
						}
						g_transformLightDirectionToObjectSpace = 1;
						break;
					}
					case CRAFT_GENUS_PLAYER_PROJECTILE:
					case CRAFT_GENUS_OTHER_PROJECTILE:
						FlightView_ComputeObjectViewPosition(renderObjectIndex);
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						RenderBillboard_DrawRollAlignedObjectModel(renderObjectIndex);
						break;
					case CRAFT_GENUS_SMALL_DEBRIS:
						FlightView_ComputeObjectViewPosition(renderObjectIndex);
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						SceneBillboard_QueueObjectTextured(renderObjectIndex);
						break;
					case CRAFT_GENUS_EXPLOSION:
						FlightView_ComputeObjectViewPosition(renderObjectIndex);
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						SceneBillboard_QueueObjectTextured(renderObjectIndex);
						break;
					default:
						break;
				}
			} else {
				int staticGenusId = genusId;
				if (staticGenusId >= CRAFT_GENUS_MINE && staticGenusId <= CRAFT_GENUS_SMALL_DEBRIS) {
					FlightView_ComputeObjectViewPosition(renderObjectIndex);
					FVIEW_SetObjectTransform(g_objectTable[objectTableIndex].roll,
											 g_objectTable[objectTableIndex].pitch,
											 g_objectTable[objectTableIndex].yaw, 0, NULL);
					FlightLight_SetupObjectLighting(&g_objectTable[objectTableIndex]);
					RenderNonCraftSceneObject(renderObjectIndex);
					g_objectPointLightCount = 0;
				}
			}
		}
	}

	if (g_useHardware3D == 0) {
		g_billboardObjectOrTypeIndex = BACKDROP_BILLBOARD_TYPE_INDEX;
		Backdrop_RenderCurrentRegion();
		FlightSurface_Lock();
		FlightStarfield_Render();
		FlightSurface_Unlock();
	}
	g_sceneFlushDrawTargetMarkers = 1;
	sw3d_DrawVisibleFacesToSurface();
	g_sceneFlushDrawTargetMarkers = 0;
	if (g_useHardware3D == 0) {
		SceneBillboard_RenderQueuedTextured(1);
		Targeting_DrawSceneObjectBoxes();
	}
	g_flightRenderDurationTicks = 0;
	g_flightRenderScratchWord = 0;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	g_flightRenderDurationTicks = (uint16_t)g_inputTimestamp;
	RenderScene_UnlockBuffers();
	if (g_useHardware3D != 0) {
		FlightView_CompositeMaskedSoftwareSurface();
	}
	Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
	g_unusedFlightRenderColorByte = 0;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	g_unusedFlightRenderColorByte = g_flightColorEscapeBypassChar;
	g_flightRenderDurationTicks = (uint16_t)(g_inputTimestamp - g_flightRenderDurationTicks);
	FlightSurface_Lock();
	Hud_BlitSoftwareHudTextPanes();
	Hud_BlitSoftwareMfdPages();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = 1;
}

// FUNCTION: XVT 0x44FE40
int FlightView_ComputeObjectViewPosition(uint16_t objectIdx) {
	ObjectRecord* object;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;

	object = &g_objectTable[objectIdx];
	savedTargetX = g_players[g_localPlayer].viewState.savedTargetX;
	savedTargetY = g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldX = object->world_x - savedTargetX;
	savedTargetZ = g_players[g_localPlayer].viewState.savedTargetZ;
	g_camRelWorldY = object->world_y - savedTargetY;
	g_camRelWorldZ = object->world_z - savedTargetZ;
	viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	depthZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	return depthZ;
}

// FUNCTION: XVT 0x44FF10
int FlightView_IsObjectSphereVisible(int objectIdx, unsigned int sphereRadius) {
	int depthWithRadius;
	int transformedX;
	int transformedY;
	int absoluteY;

	g_camRelWorldX = g_objectTable[objectIdx].world_x - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = g_objectTable[objectIdx].world_y - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = g_objectTable[objectIdx].world_z - g_players[g_localPlayer].viewState.savedTargetZ;
	depthZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	depthWithRadius = depthZ + sphereRadius;
	if (depthWithRadius < 0)
		return 0;
	if ((unsigned int)(depthWithRadius >> 8) > sphereRadius)
		return 0;

	transformedX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewX = transformedX;
	if (transformedX < 0) {
#ifdef XVT_MODERN
		transformedX = transformedX == INT32_MIN ? INT32_MAX : -transformedX;
#else
		transformedX = -transformedX;
#endif
	}
	if ((int)((unsigned int)transformedX - sphereRadius) > depthWithRadius)
		return 0;

	transformedY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	absoluteY = transformedY;
	viewY = transformedY;
	if (absoluteY < 0) {
#ifdef XVT_MODERN
		absoluteY = absoluteY == INT32_MIN ? INT32_MAX : -absoluteY;
#else
		absoluteY = -absoluteY;
#endif
	}
	return (int)((unsigned int)absoluteY - sphereRadius) <= depthWithRadius;
}

// FUNCTION: XVT 0x450020
int FlightView_CullWorldSphereToViewport(int worldX, int worldY, int worldZ, int sphereRadius) {
	int savedTargetY;
	int savedTargetZ;
	int depthWithRadius;
	int transformedX;
	int transformedY;
	int absoluteY;

	savedTargetY = g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldX = worldX - g_players[g_localPlayer].viewState.savedTargetX;
	savedTargetZ = g_players[g_localPlayer].viewState.savedTargetZ;
	g_camRelWorldY = worldY - savedTargetY;
	g_camRelWorldZ = worldZ - savedTargetZ;
	depthZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	depthWithRadius = depthZ + sphereRadius;
	if (depthWithRadius < 0) {
		return 0;
	}
	if ((depthWithRadius >> 8) > sphereRadius) {
		return 0;
	}

	transformedX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewX = transformedX;
	if (transformedX < 0) {
#ifdef XVT_MODERN
		transformedX = transformedX == INT32_MIN ? INT32_MAX : -transformedX;
#else
		transformedX = -transformedX;
#endif
	}
	if (transformedX - sphereRadius > depthWithRadius) {
		return 0;
	}

	transformedY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = transformedY;
	absoluteY = transformedY;
	if (absoluteY < 0) {
#ifdef XVT_MODERN
		absoluteY = absoluteY == INT32_MIN ? INT32_MAX : -absoluteY;
#else
		absoluteY = -absoluteY;
#endif
	}
	return absoluteY - sphereRadius <= depthWithRadius;
}

// FUNCTION: XVT 0x450110
void FlightView_RenderStartupFrame(void) {
	int playerIndex;
#ifdef XVT_MODERN
	XvtRenderCapture_BeginClassicFrame();
#endif

	for (playerIndex = 0; playerIndex < (int)(sizeof(g_players) / sizeof(g_players[0])); ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0 && playerIndex != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIndex);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);
	FlightSurface_Lock();
	Hud_RenderHud(g_localPlayer);
	FlightSurface_Unlock();
	FlightDisplay_BlitRenderSurface();
	for (playerIndex = 0; playerIndex < (int)(sizeof(g_players) / sizeof(g_players[0])); ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0 && playerIndex != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIndex);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);
	FlightView_Render();
#ifdef XVT_MODERN
	XvtRenderCapture_SealView();
#endif
	FlightDisplay_Flip();
#ifdef XVT_MODERN
	XvtRenderCapture_EndPresentation();
#endif
	FlightDisplay_BlitRenderSurface();
}

// FUNCTION: XVT 0x4501C0
void FlightView_RenderFrame(void) {
	int playerIndex;
#ifdef XVT_MODERN
	XvtRenderCapture_BeginClassicFrame();
#endif

	for (playerIndex = 0; playerIndex < (int)(sizeof(g_players) / sizeof(g_players[0])); ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0 && playerIndex != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIndex);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);
	FlightView_Render();
	FlightInput_Read(g_localPlayer);
	FlightInput_ScaleAxesForFlight();
	FlightSurface_Lock();
	Hud_RenderHud(g_localPlayer);
	FlightSurface_Unlock();
#ifdef XVT_MODERN
	XvtRenderCapture_SealView();
#endif
	FlightDisplay_Flip();
#ifdef XVT_MODERN
	XvtRenderCapture_EndPresentation();
#endif
	nullsub_11();
	if (g_useHardware3D != 0) {
		RenderScene_ClearFrameBuffers();
	} else {
		FlightDisplay_BlitRenderSurface();
	}
}
