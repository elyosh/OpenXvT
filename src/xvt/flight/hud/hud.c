#include "xvt/flight/hud/hud.h"
#ifdef XVT_MODERN
#include "xvt_runtime/snapshot/cockpit_capture.h"
#include "xvt_runtime/snapshot/cockpit_instruments.h"
#include "xvt_runtime/snapshot/cockpit_messages.h"
#include "xvt_runtime/snapshot/cockpit_pages.h"
#include "xvt_runtime/snapshot/cockpit_readouts.h"
#include "xvt_runtime/snapshot/cockpit_text.h"
#include "xvt_runtime/snapshot/render_hud.h"
#endif

#ifdef XVT_MODERN
#include "xvt_runtime/snapshot/render_assets.h"
#include "xvt_runtime/snapshot/render_capture.h"
#endif
#include "xvt/assets/file.h"
#include "xvt/assets/model_mesh.h"
#include "xvt/assets/opt_model.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/ai/pai.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_display.h"
#include "xvt/flight/flight_render.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/flight_view.h"
#include "xvt/flight/fview.h"
#include "xvt/flight/hud/flight_map.h"
#include "xvt/flight/hud/flight_text.h"
#include "xvt/flight/hud/mfd.h"
#include "xvt/flight/hud/msg.h"
#include "xvt/flight/mission/goals.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/flight/object/collide.h"
#include "xvt/flight/object/damage.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/proving_grounds.h"
#include "xvt/flight/targeting.h"
#include "xvt/flight/transfm2.h"
#include "xvt/frontend/config.h"
#include "xvt/math/math.h"
#include "xvt/math/math2.h"
#include "xvt/math/trig2.h"
#include "xvt/net/net_session.h"
#include "xvt/render/flight_light.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/render_scene.h"
#include "xvt/render/renderer.h"
#include "xvt/render/scene_billboard.h"
#include "xvt/render/std3d.h"
#include "xvt/render/sw3d.h"
#include "xvt/util/memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifndef XVT_MODERN
int(_fileno)(XvtFile* stream);
long _filelength(int fileDescriptor);
#endif

#ifndef XVT_MODERN
typedef struct Msvc42IconFilePrefix {
	uint8_t reserved[12];
	int flags;
} Msvc42IconFilePrefix;
#endif

typedef struct LfdEntryHeader {
	uint8_t resourceType[4];
	char resourceName[8];
	uint32_t dataSize;
} LfdEntryHeader;

enum { PANEL_BOX_SPAN_SCRATCH_SIZE = 2048 };

// GLOBAL: XVT 0x6122D8
uint8_t g_panelBoxSpanScratch[PANEL_BOX_SPAN_SCRATCH_SIZE] = { 0 };
// GLOBAL: XVT 0x521550
uint16_t g_hudTargetInsetMaskRefreshPending = 0;

// GLOBAL: XVT 0x9D8A50
uint16_t g_hudPanelSpriteDataHandle = 0;
// GLOBAL: XVT 0x9EC5FC
uint16_t g_messageLogHandle = 0;
// GLOBAL: XVT 0xA07CCC
uint16_t g_flightIconFramesHandle = 0;
// GLOBAL: XVT 0xA08A10
HudCockpitResource g_hudCockpitResources[28] = { { 0 } };
// GLOBAL: XVT 0xA08610
HudCockpitResourceDescriptor g_hudCockpitResourceDescriptors[28] = { { 0 } };
// GLOBAL: XVT 0xA08BD0
char g_hudCockpitResourcePath[32] = { 0 };
// GLOBAL: XVT 0xA08310
char g_hudCockpitBasePath[32] = { 0 };
// GLOBAL: XVT 0xA08BC0
HudPanelSpriteFileInfo g_hudPanelSpriteFileInfo = { { 0 }, 0, 0 };
// GLOBAL: XVT 0xA08C74
int16_t g_hudCachedTargetObjectIdx = 0;
// GLOBAL: XVT 0xA0813E
uint8_t g_hudPanelSetId = 0;
// GLOBAL: XVT 0xA08370
uint8_t* g_hudCockpitResourceWriteCursor = NULL;
// GLOBAL: XVT 0x9D8C10
int g_hudCockpitResourcesLoaded = 0;
// GLOBAL: XVT 0x9D113E
uint8_t g_hudLoadedPanelSetId = 0;
// GLOBAL: XVT 0xA00730
uint8_t g_flightDisplayRebuildPending = 0;
// GLOBAL: XVT 0xA08380
const char* g_strWaypointNames[14] = { 0 };
// GLOBAL: XVT 0xA08BF0
const char* g_strMeshComponentNames[33] = { 0 };
// GLOBAL: XVT 0x9ED240
uint8_t* g_hudPanelSpriteDataByIndex[265] = { 0 };
// GLOBAL: XVT 0xA082A0
uint8_t* g_hudPanelSpriteDataWriteCursor = NULL;
// GLOBAL: XVT 0xA08CA0
HudElementLayout g_hudElementLayouts[HUD_INSTRUMENT_COUNT] = { { 0 } };
// GLOBAL: XVT 0xA0A1E0
int16_t g_hudElementStateCache[HUD_INSTRUMENT_COUNT] = { 0 };
// GLOBAL: XVT 0xA08374
uint16_t g_hudInstrumentSetBaseIndex = HUD_COCKPIT_INSTRUMENT_BASE_INDEX;
// GLOBAL: XVT 0xA08368
uint16_t g_radarBlipColor = 0;
// GLOBAL: XVT 0xA08A04
HudRadarBlipPoint* g_radarForeDrawBlips = NULL;
// GLOBAL: XVT 0xA08A02
uint16_t g_radarForeBlipCount = 0;
// GLOBAL: XVT 0xA08340
HudRadarBlipPoint* g_radarAftDrawBlips = NULL;
// GLOBAL: XVT 0xA08C7E
uint16_t g_radarAftBlipCount = 0;
// GLOBAL: XVT 0xA08360
HudRadarBlipPoint* g_radarForeEraseBlips = NULL;
// GLOBAL: XVT 0xA08BB0
HudRadarBlipPoint* g_radarAftEraseBlips = NULL;
// GLOBAL: XVT 0xA083BA
uint16_t g_radarForePrevBlipCount = 0;
// GLOBAL: XVT 0xA08344
uint16_t g_radarAftPrevBlipCount = 0;
// GLOBAL: XVT 0xA08364
uint8_t g_radarBlipBufferParity = 0;
// GLOBAL: XVT 0xA0837A
uint8_t g_radarTargetMarkerBackgroundSaved = 0;
// GLOBAL: XVT 0xA083C0
HudRadarBlipPoint g_radarForeBlipBufferA[48] = { { 0 } };
// GLOBAL: XVT 0xA084E0
HudRadarBlipPoint g_radarForeBlipBufferB[48] = { { 0 } };
// GLOBAL: XVT 0xA0A540
HudRadarBlipPoint g_radarAftBlipBufferA[48] = { { 0 } };
// GLOBAL: XVT 0xA0A660
HudRadarBlipPoint g_radarAftBlipBufferB[48] = { { 0 } };
// GLOBAL: XVT 0xA08366
uint16_t g_mfdMissionScoreboardBlitWidth = 0;
// GLOBAL: XVT 0xA0836A
uint16_t g_mfdMissionScoreboardBlitSourceY = 0;
// GLOBAL: XVT 0xA0836C
uint16_t g_mfdMissionScoreboardBlitHeight = 0;
// GLOBAL: XVT 0xA0836E
uint16_t g_mfdMissionScoreboardBlitSourceX = 0;
// GLOBAL: XVT 0xA08376
uint16_t g_mfdMapBlitSourceY = 0;
// GLOBAL: XVT 0xA08378
uint16_t g_mfdMapBlitWidth = 0;
// GLOBAL: XVT 0xA083B8
uint16_t g_mfdMapBlitHeight = 0;
// GLOBAL: XVT 0xA083BC
uint16_t g_mfdMapBlitSourceX = 0;
// GLOBAL: XVT 0xA08604
uint16_t g_mfdCraftListBlitHeight = 0;
// GLOBAL: XVT 0xA08606
uint16_t g_mfdCraftListBlitSourceX = 0;
// GLOBAL: XVT 0xA08608
uint16_t g_mfdCraftListBlitSourceY = 0;
// GLOBAL: XVT 0xA08A00
uint16_t g_mfdCraftListBlitWidth = 0;
// GLOBAL: XVT 0xA08C80
uint16_t g_mfdGoalsBlitHeight = 0;
// GLOBAL: XVT 0xA08C82
uint16_t g_mfdDamageBlitSourceX = 0;
// GLOBAL: XVT 0xA08C84
uint16_t g_mfdGoalsBlitSourceX = 0;
// GLOBAL: XVT 0xA08C8A
uint16_t g_mfdDamageBlitHeight = 0;
// GLOBAL: XVT 0xA08C8C
uint16_t g_mfdGoalsBlitWidth = 0;
// GLOBAL: XVT 0xA08C8E
uint16_t g_mfdDamageBlitWidth = 0;
// GLOBAL: XVT 0xA08C90
uint16_t g_mfdDamageBlitSourceY = 0;
// GLOBAL: XVT 0xA08C92
uint16_t g_mfdGoalsBlitSourceY = 0;
// GLOBAL: XVT 0x521588
uint8_t g_hudBeamSegmentFadeByChargeStep[4] = { 0x30, 0x2D, 0x31, 0x32 };
// GLOBAL: XVT 0x521590
const HudBeamSegmentOffset g_hudBeamSegmentOffsets480x360[9] = {
	{ 14, 14 }, { 12, 12 }, { 10, 10 }, { 9, 9 }, { 7, 7 }, { 5, 5 }, { 4, 4 }, { 2, 2 }, { 0, 0 },
};
/* Sprite levels 0..10 include hit flash; text uses offset 10 plus levels 0..9. */
// GLOBAL: XVT 0x521570
const uint8_t g_hudShieldColors[22] = {
	0x2c, 0x34, 0x35, 0x36, 0x38, 0x39, 0x3a, 0x3c, 0x3d, 0x3e, 0x2e,
	0x2e, 0x37, 0x37, 0x37, 0x3b, 0x3b, 0x3b, 0x3f, 0x3f, 0x3f, 0x2e,
};
// GLOBAL: XVT 0x9FE7D0
uint8_t g_lastShieldDamageSide = 0;
// GLOBAL: XVT 0x5215B8
const HudBeamSegmentOffset g_hudBeamSegmentOffsets320x240[9] = {
	{ 11, 11 }, { 10, 10 }, { 8, 8 }, { 7, 7 }, { 6, 6 }, { 4, 4 }, { 3, 3 }, { 2, 2 }, { 0, 0 },
};
// GLOBAL: XVT 0xA0A1D0
uint8_t g_targetLockActive = 0;
// GLOBAL: XVT 0x9D8B68
uint8_t g_hudFullRedrawInProgress = 0;
// GLOBAL: XVT 0x556720
uint8_t g_hudViewportSpanMask2[480] = { 0 };
// GLOBAL: XVT 0x556540
uint8_t g_hudViewportSpanMask0[480] = { 0 };
// GLOBAL: XVT 0x556360
uint8_t g_hudViewportSpanMask1[480] = { 0 };
// GLOBAL: XVT 0xA0A130
const char* g_strCockpitOverlayText[40] = { 0 };
// GLOBAL: XVT 0xA0A0E0
const char* g_strCmdThreatDisplayText[18] = { 0 };
// GLOBAL: XVT 0xA08350
const char* g_strThreatDisplayText[4] = { 0 };
// GLOBAL: XVT 0x5240A0
const uint8_t g_messageTextPrefixColorCodes[16] = {
	0x42, 0x4A, 0x46, 0x4E, 0x52, 0x45, 0x42, 0x52, 0x4A, 0x52, 0x46, 0x4E, 0x4A, 0x4E, 0, 0,
};
// GLOBAL: XVT 0x5240B0
const uint8_t g_messageSenderIffColorCodes[8] = { 0x52, 0x4A, 0x46, 0x4E, 0x4A, 0x4E, 0, 0 };
// GLOBAL: XVT 0x5235E0
uint8_t g_flightConfTickCounter = 0;
// GLOBAL: XVT 0x9A8D90
int g_flightTickOverlayLastLoopTicks = 0;
// GLOBAL: XVT 0x9A7BA0
int g_flightTickOverlayWindowTicks = 0;
// GLOBAL: XVT 0xA0829C
int g_flightTickOverlaySampleCount = 0;
// GLOBAL: XVT 0x9A8BFC
int g_pingIndicator = 0;
// GLOBAL: XVT 0x9EC45C
int g_lagIndicator = 0;

// GLOBAL: XVT 0x5569F8
HudInFlightMessageRecord g_systemMessagePane;
// GLOBAL: XVT 0x556A50
HudInFlightMessageRecord g_flightGroupMessagePane;
// GLOBAL: XVT 0x9A6FF0
HudInFlightMessageRecord g_readyMessagePaneQueue[11];
// GLOBAL: XVT 0x9D77F8
uint8_t g_readyMessageQueueCount;
// GLOBAL: XVT 0x556AA4
int g_flightMessagePanesForceExpire = 0;
// GLOBAL: XVT 0x9D7686
static uint16_t g_unusedReadyMessagePaneInitialState = 0;
// GLOBAL: XVT 0x9D7694
int g_targetDescriptionMessageId = 0;
// GLOBAL: XVT 0x9ECC3C
int g_radioMessageBackupEnabled = 0;
// GLOBAL: XVT 0xA00860
uint16_t g_replayViewMode = 0;
// GLOBAL: XVT 0x9A7B54
int g_systemMessageDisplayEnabled = 0;
// GLOBAL: XVT 0x5235DC
int g_readyMessagePaneLeft = -1;
// GLOBAL: XVT 0x9D1148
int g_readyMessagePaneTop = 0;
// GLOBAL: XVT 0x9A7EC0
int g_readyMessagePaneRight = 0;
// GLOBAL: XVT 0x9A7B44
int g_readyMessagePaneBottom = 0;
// GLOBAL: XVT 0x9A7390
int g_systemMessagePaneLeft = 0;
// GLOBAL: XVT 0x9D12F8
int g_systemMessagePaneTop = 0;
// GLOBAL: XVT 0x9A8D98
int g_systemMessagePaneRight = 0;
// GLOBAL: XVT 0x9A8D94
int g_systemMessagePaneBottom = 0;
// GLOBAL: XVT 0x9EC470
int g_flightGroupMessagePaneLeft = 0;
// GLOBAL: XVT 0x9ECC38
int g_flightGroupMessagePaneTop = 0;
// GLOBAL: XVT 0x9D12F4
int g_flightGroupMessagePaneRight = 0;
// GLOBAL: XVT 0x9D6934
int g_flightGroupMessagePaneBottom = 0;
// GLOBAL: XVT 0x5215E0
const char g_countermeasureAmmoWidthText[4] = "000";
// GLOBAL: XVT 0x52168C
const char g_missionClockMinutesWidthText[4] = "00:";
// GLOBAL: XVT 0x521564
const uint8_t g_lfdPaletteResourceTypeTag[4] = { 'P', 'L', 'T', 'T' };

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x40C430
void Hud_DrawBoxOverlayHW(int x, int y, int width, int height, int colorIdx, int depth) {

	enum {
		MARKER_SIZE = 4,
		MIN_CORNER_LENGTH = 3,
		QUAD_VERTEX_COUNT = 4,
		QUAD_TRIANGLE_COUNT = 2,
		MAX_BOX_VERTEX_COUNT = 32,
		MAX_BOX_TRIANGLE_COUNT = 16,
	};

	const Std3DRenderStateFlags renderFlags =
		STD3D_RS_Z_COMPARE_ENABLE | STD3D_RS_Z_WRITE_ENABLE | STD3D_RS_MONO_DISABLE;
	uint8_t savedTextBackgroundColor;
	uint16_t markerX;
	uint16_t markerY;
	uint32_t color;
	float depthValue;
	int boxWidth;
	int boxHeight;
	int adjustedDepth;
	int boxTop;
	int right;
	int bottom;
	int cornerWidth;
	int cornerHeight;
	int start;
	int end;
	int vertexIndex;

	boxWidth = width;
	adjustedDepth = depth;
	if (adjustedDepth == 1 && boxWidth == MARKER_SIZE && height == MARKER_SIZE) {
		FlightSurface_Lock();
		savedTextBackgroundColor = g_flightTextBgColor;
		g_flightTextBgColor = (uint8_t)colorIdx;
		markerX = (uint16_t)(g_flightVpX + x);
		markerY = (uint16_t)(g_flightVpY + y);
		FlightText_SetClipRect(g_flightVpX, g_flightVpY, g_flightVpX + g_flightVpWidth,
							   g_flightVpY + g_flightVpHeight);
		g_flightFillRectClippedFn(markerX, markerY, markerX + MARKER_SIZE, markerY + MARKER_SIZE, 1);
		g_flightTextBgColor = savedTextBackgroundColor;
		FlightSurface_Unlock();
		return;
	}
	boxHeight = height;

	if (g_d3dVertexCount + MAX_BOX_VERTEX_COUNT > g_maxBatchVerts ||
		g_d3dIndexCount + MAX_BOX_TRIANGLE_COUNT > g_maxBatchTris) {
		Math_SetFpuExtendedPrecisionMode();
		std3D_StartScene();
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		std3D_EndScene();
		Math_SetFpuSinglePrecisionMode();
		g_d3dIndexCount = 0;
		g_d3dVertexCount = 0;
	}

	color = 4 * (g_swPalette[colorIdx].b +
				 ((g_swPalette[colorIdx].g + ((g_swPalette[colorIdx].r - 64) << 8)) << 8));
	boxTop = y;
	right = x + boxWidth;
	bottom = boxTop + boxHeight;
	cornerWidth = boxWidth >> 3;
	cornerHeight = boxHeight >> 3;
	if (cornerWidth < MIN_CORNER_LENGTH) {
		cornerWidth = MIN_CORNER_LENGTH;
	}
	if (cornerHeight < MIN_CORNER_LENGTH) {
		cornerHeight = MIN_CORNER_LENGTH;
	}
	if (cornerWidth > boxWidth) {
		cornerWidth = boxWidth;
	}
	if (cornerHeight > boxHeight) {
		cornerHeight = boxHeight;
	}
	if (adjustedDepth < 1) {
		adjustedDepth = 1;
	}
	depthValue = g_renderUnitFloat / ((float)adjustedDepth * g_invDepthProjScale + g_renderUnitFloat);
	if (g_std3DZBufferBitDepth == 2) {
		depthValue = g_renderUnitFloat - depthValue;
	}

	if (boxTop >= 0 && boxTop < g_flightVpHeight) {
		start = x;
		end = x + cornerWidth;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpWidth) {
			end = g_flightVpWidth - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)boxTop;
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)boxTop;
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(boxTop + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(boxTop + 1);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
		start = right - cornerWidth;
		end = right;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpWidth) {
			end = g_flightVpWidth - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)boxTop;
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)boxTop;
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(boxTop + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(boxTop + 1);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
	}

	if (bottom >= 0 && bottom < g_flightVpHeight) {
		start = x;
		end = x + cornerWidth;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpWidth) {
			end = g_flightVpWidth - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(bottom);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(bottom);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(bottom + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(bottom + 1);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
		start = right - cornerWidth;
		end = right + 1;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpWidth) {
			end = g_flightVpWidth - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(bottom);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(bottom);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(bottom + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(bottom + 1);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
	}

	if (x >= 0 && x < g_flightVpWidth) {
		start = boxTop;
		end = boxTop + cornerHeight;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpHeight) {
			end = g_flightVpHeight - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(x);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(x);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(x + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(x + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(start);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
		start = bottom - cornerHeight;
		end = bottom;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpHeight) {
			end = g_flightVpHeight - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(x);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(x);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(x + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(x + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(start);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
	}

	if (right >= 0 && right < g_flightVpWidth) {
		start = boxTop;
		end = boxTop + cornerHeight;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpHeight) {
			end = g_flightVpHeight - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(right);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(right);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(right + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(right + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(start);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
		start = bottom - cornerHeight;
		end = bottom;
		if (start < 0) {
			start = 0;
		}
		if (end >= g_flightVpHeight) {
			end = g_flightVpHeight - 1;
		}
		if (end > start) {
			g_flightVertexBuffer[g_d3dVertexCount].sx = g_flightVpOriginX + (float)(right);
			g_flightVertexBuffer[g_d3dVertexCount].sy = g_flightVpOriginY + (float)(start);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sx = g_flightVpOriginX + (float)(right);
			g_flightVertexBuffer[g_d3dVertexCount + 1].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sx = g_flightVpOriginX + (float)(right + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 2].sy = g_flightVpOriginY + (float)(end);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sx = g_flightVpOriginX + (float)(right + 1);
			g_flightVertexBuffer[g_d3dVertexCount + 3].sy = g_flightVpOriginY + (float)(start);
			for (vertexIndex = 0; vertexIndex < QUAD_VERTEX_COUNT; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].sz = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].rhw = depthValue;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tu = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].tv = 0.0f;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = color;
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].specular = 0;
			}
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 1;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_triBuffer[g_d3dIndexCount].v0 = g_d3dVertexCount;
			g_triBuffer[g_d3dIndexCount].v1 = g_d3dVertexCount + 2;
			g_triBuffer[g_d3dIndexCount].v2 = g_d3dVertexCount + 3;
			g_triBuffer[g_d3dIndexCount].texture = NULL;
			g_triBuffer[g_d3dIndexCount++].flags = renderFlags;
			g_d3dVertexCount += QUAD_VERTEX_COUNT;
		}
	}
}

// FUNCTION: XVT 0x427720
int Hud_SetHudViewState(int hudViewState, int playerIdx) {
	int localPlayer;
	int savedLockCount;
	int remainingLocks;

	localPlayer = g_localPlayer;
	if (playerIdx == localPlayer && g_hudCockpitResourceDescriptors[hudViewState].enabled == 0)
		return 0;
	if (g_players[playerIdx].viewState.hudStateLive == hudViewState)
		return 1;

	g_players[playerIdx].viewState.hudStateLive = (uint8_t)hudViewState;
	if (playerIdx == localPlayer) {
		FlightRender_InvokeTransitionHook(1);
		FlightSurface_Lock();
		Hud_RebuildDisplayForViewState(hudViewState, playerIdx);
		FlightSurface_Unlock();

		savedLockCount = FlightSurface_GetLockCount();
		if (savedLockCount > 0) {
			for (remainingLocks = savedLockCount; remainingLocks != 0; --remainingLocks) {
				FlightSurface_Unlock();
			}
		}
		FlightDisplay_BlitRenderSurface();
		FlightDisplay_Flip();
		FlightDisplay_BlitRenderSurface();
		if (savedLockCount > 0) {
			do {
				FlightSurface_Lock();
				--savedLockCount;
			} while (savedLockCount != 0);
		}
		FlightRender_ResetPalette(1);
	}
	g_players[playerIdx].viewState.hudStateMirror = (uint8_t)hudViewState;
	return 1;
}

// FUNCTION: XVT 0x438A30
void Hud_InitHUD(int playerIdx) {
	enum {
		HUD_LAYOUT_ACTIVE_FLAG_INDEX = 127,
		HUD_ELEMENT_STATE_DIRTY = -2,
		HUD_ELEMENT_STATE_INACTIVE = -3,
	};

	uint16_t instrumentSet;
	uint16_t instrumentIndex;

	g_hudFullRedrawInProgress = 1;
	if (g_localPlayer == playerIdx) {
		switch (g_flightResolutionMode) {
			case FLIGHT_RESOLUTION_320X240:
				g_mfdGoalsBlitSourceX = 6;
				g_mfdGoalsBlitWidth = 141;
				g_mfdDamageBlitSourceY = 103;
				g_mfdDamageBlitWidth = 100;
				g_mfdDamageBlitHeight = 80;
				g_mfdGoalsBlitSourceY = 53;
				g_mfdCraftListBlitHeight = 47;
				g_mfdMissionScoreboardBlitSourceX = 2;
				g_mfdMissionScoreboardBlitSourceY = 109;
				g_mfdGoalsBlitHeight = 53;
				g_mfdMissionScoreboardBlitHeight = 70;
				g_mfdMapBlitSourceX = 121;
				g_mfdMapBlitSourceY = 186;
				g_mfdMapBlitWidth = 111;
				g_mfdMapBlitHeight = 52;
				g_mfdDamageBlitSourceX = 153;
				g_mfdCraftListBlitSourceX = 153;
				g_mfdCraftListBlitSourceY = 53;
				g_mfdCraftListBlitWidth = 112;
				g_mfdMissionScoreboardBlitWidth = 112;
				break;
			case FLIGHT_RESOLUTION_640X480:
				g_mfdGoalsBlitSourceX = 12;
				g_mfdGoalsBlitWidth = 282;
				g_mfdDamageBlitSourceY = 206;
				g_mfdDamageBlitWidth = 200;
				g_mfdDamageBlitHeight = 160;
				g_mfdGoalsBlitSourceY = 107;
				g_mfdCraftListBlitWidth = 225;
				g_mfdCraftListBlitHeight = 94;
				g_mfdMissionScoreboardBlitSourceX = 4;
				g_mfdMissionScoreboardBlitSourceY = 219;
				g_mfdMissionScoreboardBlitWidth = 224;
				g_mfdMissionScoreboardBlitHeight = 140;
				g_mfdMapBlitSourceX = 242;
				g_mfdMapBlitSourceY = 373;
				g_mfdMapBlitWidth = 223;
				g_mfdMapBlitHeight = 104;
				g_mfdGoalsBlitHeight = 107;
				g_mfdDamageBlitSourceX = 306;
				g_mfdCraftListBlitSourceX = 306;
				g_mfdCraftListBlitSourceY = 107;
				break;
			case FLIGHT_RESOLUTION_480X360:
				g_mfdGoalsBlitSourceX = 9;
				g_mfdGoalsBlitWidth = 211;
				g_mfdDamageBlitSourceY = 154;
				g_mfdDamageBlitWidth = 150;
				g_mfdDamageBlitHeight = 120;
				g_mfdGoalsBlitSourceY = 80;
				g_mfdCraftListBlitHeight = 70;
				g_mfdMissionScoreboardBlitSourceX = 3;
				g_mfdMissionScoreboardBlitSourceY = 164;
				g_mfdGoalsBlitHeight = 80;
				g_mfdMissionScoreboardBlitHeight = 105;
				g_mfdMapBlitSourceX = 181;
				g_mfdMapBlitSourceY = 279;
				g_mfdMapBlitWidth = 167;
				g_mfdMapBlitHeight = 78;
				g_mfdDamageBlitSourceX = 229;
				g_mfdCraftListBlitSourceX = 229;
				g_mfdCraftListBlitSourceY = 80;
				g_mfdCraftListBlitWidth = 168;
				g_mfdMissionScoreboardBlitWidth = 168;
				break;
			default:
				break;
		}

		if (g_players[g_localPlayer].mapCameraState != 0) {
			g_mfdCraftListBlitWidth =
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT].clipWidth;
			g_mfdCraftListBlitHeight =
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT]
					.clipHeightOrForegroundColor;
		}

		for (instrumentSet = 0; instrumentSet < HUD_INSTRUMENT_SET_COUNT; ++instrumentSet) {
			for (instrumentIndex = 0; instrumentIndex < HUD_INSTRUMENTS_PER_SET; ++instrumentIndex) {
				if (instrumentIndex >= HUD_MFD_CRAFT_LIST_ELEMENT &&
					instrumentIndex <= HUD_MFD_SCOREBOARD_ELEMENT) {
					g_hudElementStateCache[HUD_INSTRUMENTS_PER_SET * instrumentSet + instrumentIndex] =
						HUD_ELEMENT_STATE_INACTIVE;
				} else {
					g_hudElementStateCache[HUD_INSTRUMENTS_PER_SET * instrumentSet + instrumentIndex] =
						HUD_ELEMENT_STATE_DIRTY;
				}
				if (instrumentIndex == HUD_MFD_MESSAGE_LOG_ELEMENT) {
					g_hudElementStateCache[HUD_INSTRUMENTS_PER_SET * instrumentSet + instrumentIndex] =
						HUD_ELEMENT_STATE_INACTIVE;
				}
			}
		}

		g_hudElementLayouts[HUD_LAYOUT_ACTIVE_FLAG_INDEX].clipHeightOrForegroundColor = 1;
		g_hudCachedTargetObjectIdx = -1;
		g_radarForeBlipCount = 0;
		g_radarTargetMarkerBackgroundSaved = 0;
		g_radarAftBlipCount = 0;
		g_radarBlipBufferParity = 0;
		Hud_ClearUnavailableCraftSystemIndicators();
		if (g_flightSimSideEffectsSuppressed == 0) {
			Hud_UpdateCraftSystemStatusIndicators();
		}
		Hud_RenderHud(g_localPlayer);
		Hud_DrawStaticCockpitText(g_localPlayer);
		nullsub_6(g_localPlayer);
	}
	g_hudFullRedrawInProgress = 0;
}

// FUNCTION: XVT 0x438DF0
void Hud_RenderHud(int playerIdx) {
	enum {
		HUD_STATE_MAIN = 0,
		HUD_STATE_FORWARD_PANEL = 19,
		HUD_STATE_COMMAND_DISPLAY = 20,
	};

	uint8_t hudState;

#ifdef XVT_MODERN
	XvtCockpitInstruments_BeginUpdate(playerIdx);
#endif

	if (playerIdx == g_localPlayer) {
		if (g_players[g_localPlayer].regionSessionId != 0 || g_flightMissionState.missionEndPending != 0) {
			FlightSurface_Unlock();
			fsfx_UpdateIncomingMissileWarning(0);
			FlightSurface_Lock();
		} else if (g_players[playerIdx].mapCameraState != 0) {
			Hud_DrawMapViewOverlay();
			FlightSurface_Unlock();
			fsfx_UpdateTargetingTone(0);
			FlightSurface_Lock();
			Hud_UpdateCriticalHullShieldWarning();
		} else {
			hudState = g_players[g_localPlayer].viewState.hudStateLive;
			if (hudState == HUD_STATE_MAIN) {
				Hud_UpdateHUD();
				Hud_UpdateCriticalHullShieldWarning();
			} else if (hudState == HUD_STATE_FORWARD_PANEL) {
				Hud_UpdateForwardPanel();
				Hud_UpdateCriticalHullShieldWarning();
			} else if (hudState == HUD_STATE_COMMAND_DISPLAY) {
				Hud_UpdateCMDText();
				FlightSurface_Unlock();
				fsfx_UpdateTargetingTone(0);
				FlightSurface_Lock();
				Hud_UpdateCriticalHullShieldWarning();
			} else {
				FlightSurface_Unlock();
				fsfx_UpdateTargetingTone(0);
				Hud_UpdateMfdPages();
				FlightSurface_Lock();
				Hud_UpdateCriticalHullShieldWarning();
			}
		}
	}
#ifdef XVT_MODERN
	XvtCockpit_RefreshInstruments(playerIdx);
#endif
}

// FUNCTION: XVT 0x438EF0
void Hud_DrawHudTargetInsetIfEnabled(int playerIndex) {
	enum {
		TARGET_INSET_LAYOUT_INDEX = 2,
		HUD_STATE_LIVE = 0,
		HUD_STATE_COCKPIT = 19,
		INVALID_TARGET_OBJECT_INDEX = -1,
		TARGET_INSET_FEATURE_MASK = 1,
	};

	uint8_t hudState;

#ifdef XVT_MODERN
	XvtRenderDraw_Scope(XVT_SCOPE_COCKPIT);
#endif

	if (playerIndex == g_localPlayer && g_players[g_localPlayer].regionSessionId == 0 &&
		g_flightMissionState.missionEndPending == 0) {
		if (g_players[playerIndex].mapCameraState != 0) {
			if (g_players[playerIndex].currentTargetObjectIdx == INVALID_TARGET_OBJECT_INDEX) {
				return;
			}
			Hud_Update3DCrt(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].y,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].selector,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].colorIndex,
				g_hudTargetInsetMaskRefreshPending);
			return;
		} else {
			hudState = g_players[playerIndex].viewState.hudStateLive;
			if (hudState != HUD_STATE_LIVE && hudState != HUD_STATE_COCKPIT) {
				return;
			}
			if (g_players[playerIndex].currentTargetObjectIdx == INVALID_TARGET_OBJECT_INDEX) {
				return;
			}
			if ((g_objectTable[g_players[playerIndex].objectIndex]
					 .mobj->pCraft->damageStats.activeHudFeatureMask &
				 TARGET_INSET_FEATURE_MASK) == 0) {
				return;
			}
		}

		Hud_Update3DCrt(
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].y,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].selector,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_LAYOUT_INDEX].colorIndex,
			g_hudTargetInsetMaskRefreshPending);
	}
}

// FUNCTION: XVT 0x439030
void Hud_DrawStaticCockpitText(uint16_t playerIdx) {
	CraftData* craft;
	uint16_t layoutIndex;
	uint16_t systemFlags;
	uint16_t featureMask;
	uint16_t textWidth;

	if (g_localPlayer != playerIdx) {
		return;
	}
	if (g_players[playerIdx].viewState.hudStateLive != HUD_VIEW_FORWARD &&
		g_players[playerIdx].viewState.hudStateLive != HUD_VIEW_HUD_ONLY) {
		return;
	}
	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	systemFlags = craft->systemFlags;
	featureMask = craft->damageStats.activeHudFeatureMask;

	FlightText_SetFontTier(0);
	FlightText_SetColor(0x2F);
	for (layoutIndex = 122; layoutIndex < 126; ++layoutIndex) {
		if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].selector == 0) {
			continue;
		}
		switch (layoutIndex) {
			case 122:
				if ((featureMask & 0x0200) == 0) {
					continue;
				}
				break;
			case 123:
				if ((systemFlags & CRAFT_SUBSYSTEM_FLAG_SHIELDS) == 0 || (featureMask & 0x0800) == 0) {
					continue;
				}
				break;
			case 124:
				if ((featureMask & 0x0400) == 0) {
					continue;
				}
				break;
			case 125:
				if ((systemFlags & CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM) == 0 || (featureMask & 0x1000) == 0) {
					continue;
				}
				break;
		}
		FlightText_SetBackgroundColor(
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].colorIndex);
		FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].x,
							 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].y);
		FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].x,
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].y,
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].x +
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].clipWidth,
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + layoutIndex].y +
								   g_flightFontLineHeight);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(
			(XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_POWER_LABEL_FIRST + layoutIndex - 122),
			g_strCockpitOverlayText[layoutIndex - 122 + COCKPIT_OVERLAY_STR_L], XVT_COCKPIT_ALIGN_LEFT);
#endif
		FlightText_DrawString(g_strCockpitOverlayText[layoutIndex - 122 + COCKPIT_OVERLAY_STR_L]);
	}

	if ((featureMask & 0x40) != 0) {
		if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].selector != 0) {
			if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
				FlightText_SetColor(0x4A);
			}
			FlightText_SetBackgroundColor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].colorIndex);
			FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].x,
								 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].y);
			FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].x,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].y,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].x +
									   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].clipWidth,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].y +
									   g_flightFontLineHeight);
			if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 121].selector <= 4) {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_SPEED_LABEL,
										   g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_SPD],
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_SPD]);
			} else {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_SPEED_LABEL,
										   g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_SPEED],
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_SPEED]);
			}
		}
		if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].selector != 0) {
			if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
				FlightText_SetColor(0x4A);
			}
			FlightText_SetBackgroundColor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].colorIndex);
			FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].x,
								 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].y);
			FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].x,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].y,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].x +
									   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].clipWidth,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].y +
									   g_flightFontLineHeight);
			if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 120].selector <= 4) {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_THROTTLE_LABEL,
										   g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_THTL],
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_THTL]);
			} else {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_THROTTLE_LABEL,
										   g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_THROTTLE],
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_THROTTLE]);
			}
		}
		if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
			FlightText_SetFontTier(0);
		} else {
			FlightText_SetFontTier(2);
		}
		textWidth = FlightText_MeasureStringWidth(g_countermeasureAmmoWidthText);
		FlightText_SetColor(0x4A);
		FlightText_SetBackgroundColor(0x2C);
		FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].x + textWidth,
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].y,
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].x + textWidth +
								   FlightText_MeasureStringWidth("%"),
							   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].y +
								   g_flightFontLineHeight);
		FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].x + textWidth,
							 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 41].y);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_THROTTLE_PERCENT, "%", XVT_COCKPIT_ALIGN_LEFT);
#endif
		g_flightDrawCharFn('%');
	}

	if ((featureMask & 0x20) != 0) {
		for (layoutIndex = 0; layoutIndex <= 1; ++layoutIndex) {
			if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].selector == 0) {
				continue;
			}
			FlightText_SetClipRect(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].y,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].x +
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].clipWidth,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].y +
					g_flightFontLineHeight);
			FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].x,
								 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].y);
			FlightText_SetColor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex]
									.clipHeightOrForegroundColor);
			FlightText_SetBackgroundColor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 128 + layoutIndex].colorIndex);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(
				(XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_SHIELD_LABEL_FIRST + layoutIndex),
				g_strCockpitOverlayText[layoutIndex + COCKPIT_OVERLAY_STR_F], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCockpitOverlayText[layoutIndex + COCKPIT_OVERLAY_STR_F]);
		}
	}

	FlightText_SetBackgroundColor(0x2C);
	FlightText_SetFontTier(0);
	if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240) {
		Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].objectIndex, 7);
	} else {
		Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].objectIndex, 3);
	}
	FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].x,
						 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].y);
	FlightText_SetClipRect(
		g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].x,
		g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].y,
		g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].x +
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].clipWidth,
		g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].y +
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].clipHeightOrForegroundColor);
	g_flightFillClipRectFn();
#ifdef XVT_MODERN
	XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CRAFT_STATUS, g_flightTextScratchBuffer,
							   XVT_COCKPIT_ALIGN_CENTER);
#endif
	FlightText_DrawStringCentered(g_flightTextScratchBuffer);
	if (g_players[playerIdx].viewState.hudStateLive == HUD_VIEW_FORWARD) {
		FlightText_SetFontTier(2);
		FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
		FlightText_SetBackgroundColor(0x40);
		if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240) {
			FlightText_SetColor(0x4D);
		} else {
			FlightText_SetColor(0x4E);
		}
		g_flightTextShadowEnabled = 0;
		FlightText_SetCursor(g_hudElementLayouts[46].x + FlightText_MeasureStringWidth("00"),
							 g_hudElementLayouts[46].y);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CLOCK_SEPARATOR, ":", XVT_COCKPIT_ALIGN_LEFT);
#endif
		g_flightDrawCharFn(':');
	}
}

// FUNCTION: XVT 0x439700
void nullsub_6(int playerIdx) { (void)playerIdx; }

// FUNCTION: XVT 0x439710
void Hud_UpdateHUD(void) {
	enum {
		SHIELD_DISTRIBUTION_ELEMENT = 51,
		S_FOIL_STATE_ELEMENT = 45,
		SHIELD_DISPLAY_FEATURE_MASK = 0x20,
		S_FOIL_CLOSED_MASK = 2,
	};

	int objectIndex;
	int supportedCraft;
	CraftData* craft;
	uint16_t sFoilIndicatorState;

	FlightText_SetFontTier(2);
	Hud_DrawRadarBlips();
	Hud_DrawReticle3D();
	Hud_UpdateTargetingLockIndicator();
	Hud_UpdateTargetingComputerDisplay();
	Hud_UpdateWarheadCnt();
	Hud_DrawShieldStrength2D();
	Hud_DrawBeamStrength2D();
	Hud_UpdateMissionClockDisplay();

	objectIndex = g_players[g_localPlayer].objectIndex;
	supportedCraft =
		objectIndex != -1 && (g_objectTable[objectIndex].objectType == CRAFT_SPECIES_X_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_Y_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_A_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_Z_95_HEADHUNTER ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_B_WING);
	if (supportedCraft) {
		craft = g_objectTable[objectIndex].mobj->pCraft;
		if ((craft->damageStats.activeHudFeatureMask & SHIELD_DISPLAY_FEATURE_MASK) != 0 &&
			(uint16_t)g_hudElementLayouts[SHIELD_DISTRIBUTION_ELEMENT].x +
					(uint16_t)g_hudElementLayouts[SHIELD_DISTRIBUTION_ELEMENT].y !=
				0) {
			Hud_DrawCachedSpriteElement(SHIELD_DISTRIBUTION_ELEMENT, (uint8_t)craft->shieldDistribMode);
		}
	}

	Hud_UpdateSpeedPercent();
	Hud_UpdateThrottlePercent();
	Hud_DrawPowerSettings2D();
	Hud_UpdateThreatIndicators(0);
	Hud_UpdateCountermeasureStatus();

	objectIndex = g_players[g_localPlayer].objectIndex;
	supportedCraft =
		objectIndex != -1 && (g_objectTable[objectIndex].objectType == CRAFT_SPECIES_X_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_Y_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_A_WING ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_Z_95_HEADHUNTER ||
							  g_objectTable[objectIndex].objectType == CRAFT_SPECIES_B_WING);
	if (supportedCraft && g_hudElementLayouts[S_FOIL_STATE_ELEMENT].x != 0) {
		sFoilIndicatorState = (g_objectTable[objectIndex].mobj->pCraft->sFoilState & S_FOIL_CLOSED_MASK) == 0;
		Hud_DrawCachedSpriteElement(S_FOIL_STATE_ELEMENT, sFoilIndicatorState);
	}

	Hud_UpdateMfdPages();
	Hud_DrawCraftNameFpsAndNetworkStatus();
}

// FUNCTION: XVT 0x4398B0
void Hud_UpdateForwardPanel(void) {
	Hud_DrawRadarBlips();
	Hud_DrawReticle3D();
	Hud_UpdateTargetingLockIndicator();
	Hud_UpdateWarheadCnt();
	Hud_UpdateThreatIndicators(1);
	FlightText_SetFontTier(2);
	Hud_UpdateTargetingComputerDisplay();
	Hud_DrawShieldStrength2D();
	Hud_DrawBeamStrength2D();
	Hud_UpdateSpeedPercent();
	Hud_UpdateThrottlePercent();
	Hud_DrawPowerSettings2D();
	Hud_UpdateMfdPages();
	Hud_DrawCraftNameFpsAndNetworkStatus();
}

// FUNCTION: XVT 0x439900
void Hud_DrawMapViewOverlay(void) {
	enum {
		CAMERA_FOCUS_LAYOUT = 120,
		AIM_TARGET_LAYOUT = 121,
		DEFAULT_SCREEN_WIDTH = 320,
		DEFAULT_SCREEN_HEIGHT = 200,
		OBJECT_DISPLAY_FLAGS = 3,
	};

	uint16_t objectIdx;
	const char* objectName;
	char text[80];

	Hud_UpdateTargetingComputerDisplay();
	Hud_UpdateMfdPages();
	if ((uint16_t)g_hudElementStateCache[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT] !=
		g_players[g_localPlayer].viewState.cameraFocusObjIdx) {
		FlightSurface_Lock();
		FlightSw_SetRenderTarget(NULL, DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, 0);
		FlightText_SetFontTier(2);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor('N');
		FlightText_SetClipRect(
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].y,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].x +
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].clipWidth,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].y +
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT]
					.clipHeightOrForegroundColor);
		FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].x,
							 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT].y);
		g_flightFillClipRectFn();

		objectIdx = g_players[g_localPlayer].viewState.cameraFocusObjIdx;
		if (objectIdx != UINT16_MAX) {
			Hud_AppendObjectDisplayName(objectIdx, OBJECT_DISPLAY_FLAGS);
			objectName = g_flightTextScratchBuffer;
		} else {
			objectName = g_strMeshComponentNames[MESH_COMPONENT_32_DASHES];
		}
		strcpy(text, objectName);
		FlightText_SetScratch(g_strMapRoomText[MAP_ROOM_STR_FOLLOWING]);
		FlightText_AppendScratchChar(' ');
		FlightText_AppendScratchString(text);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_MAP_FOLLOWING, g_flightTextScratchBuffer,
								   XVT_COCKPIT_ALIGN_CENTER);
#endif
		FlightText_DrawStringCentered(g_flightTextScratchBuffer);
		FlightSurface_Unlock();
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + CAMERA_FOCUS_LAYOUT] =
			g_players[g_localPlayer].viewState.cameraFocusObjIdx;
	}

	if ((uint16_t)g_hudElementStateCache[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT] !=
		g_players[g_localPlayer].viewState.aimTargetIdx) {
		FlightSurface_Lock();
		FlightSw_SetRenderTarget(NULL, DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, 0);
		FlightText_SetFontTier(2);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor('R');
		FlightText_SetClipRect(
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].y,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].x +
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].clipWidth,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].y +
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT]
					.clipHeightOrForegroundColor);
		FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].x,
							 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT].y);
		g_flightFillClipRectFn();

		objectIdx = g_players[g_localPlayer].viewState.aimTargetIdx;
		if (objectIdx != UINT16_MAX) {
			Hud_AppendObjectDisplayName(objectIdx, OBJECT_DISPLAY_FLAGS);
			objectName = g_flightTextScratchBuffer;
		} else {
			objectName = g_strMeshComponentNames[MESH_COMPONENT_32_DASHES];
		}
		strcpy(text, objectName);
		FlightText_SetScratch(g_strMapRoomText[MAP_ROOM_STR_TRACKING]);
		FlightText_AppendScratchChar(' ');
		FlightText_AppendScratchString(text);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_MAP_TRACKING, g_flightTextScratchBuffer,
								   XVT_COCKPIT_ALIGN_CENTER);
#endif
		FlightText_DrawStringCentered(g_flightTextScratchBuffer);
		FlightSurface_Unlock();
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + AIM_TARGET_LAYOUT] =
			g_players[g_localPlayer].viewState.aimTargetIdx;
	}
}

// FUNCTION: XVT 0x439C60
void Hud_UpdateCMDText(void) {
	enum {
		CMD_TEXT_LAYOUT_FIRST = 139,
		CMD_TEXT_LAYOUT_END = 143,
		CMD_TEXT_CACHE_INDEX = 143,
		MESH_COMPONENT_NAME = 17,
		COLOR_MFD_BACKGROUND = 0x2C,
		HUD_ELEMENT_STATE_DIRTY = UINT16_MAX - 1,
	};

	uint16_t layoutIndex;

	if (((const uint16_t*)g_hudElementStateCache)[CMD_TEXT_CACHE_INDEX] == HUD_ELEMENT_STATE_DIRTY) {
		Hud_UpdateMfdPages();
		FlightText_SetBackgroundColor(COLOR_MFD_BACKGROUND);
		FlightText_SetFontTier(2);
		for (layoutIndex = CMD_TEXT_LAYOUT_FIRST; layoutIndex < CMD_TEXT_LAYOUT_END; ++layoutIndex) {
			FlightText_SetCursor(g_hudElementLayouts[layoutIndex].x, g_hudElementLayouts[layoutIndex].y);
			FlightText_SetColor(g_hudElementLayouts[layoutIndex].colorIndex);
			switch (layoutIndex - CMD_TEXT_LAYOUT_FIRST) {
				case 0:
					FlightText_SetScratch(g_strCmdThreatDisplayText[1]);
					break;
				case 1:
					FlightText_SetScratch(g_strCmdThreatDisplayText[2]);
					break;
				case 2:
					FlightText_SetScratch(g_strCmdThreatDisplayText[0]);
					break;
				case 3:
					FlightText_SetScratch(g_strMeshComponentNames[MESH_COMPONENT_NAME]);
					break;
				default:
					break;
			}
			FlightText_AppendScratchChar(':');
			FlightText_SetClipRect(g_hudElementLayouts[layoutIndex].x, g_hudElementLayouts[layoutIndex].y,
								   g_hudElementLayouts[layoutIndex].x +
									   FlightText_MeasureStringWidth(g_flightTextScratchBuffer),
								   g_hudElementLayouts[layoutIndex].y + g_flightFontLineHeight);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField((XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_CMD_HEADER_FIRST +
															   layoutIndex - CMD_TEXT_LAYOUT_FIRST),
									   g_flightTextScratchBuffer, XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_flightTextScratchBuffer);
		}
		g_hudElementStateCache[CMD_TEXT_CACHE_INDEX] = 0;
	}
	Hud_DrawCmdTargetDetails();
	Hud_DrawCmdTargetStatusIndicators();
}

// FUNCTION: XVT 0x439D90
void Hud_DrawRadarBlips(void) {
	uint16_t playerObjectIdx = g_players[g_localPlayer].objectIndex;
	CraftData* playerCraft = g_objectTable[playerObjectIdx].mobj->pCraft;
	int objectIdx;

	if ((playerCraft->damageStats.activeHudFeatureMask & 0x80) == 0 ||
		(playerCraft->damageStats.activeHudFeatureMask & 0x100) == 0)
		return;

	g_radarForePrevBlipCount = g_radarForeBlipCount;
	g_radarAftPrevBlipCount = g_radarAftBlipCount;
	g_radarForeBlipCount = 0;
	g_radarAftBlipCount = 0;
	g_radarTargetMarkerRestoreX = g_radarTargetMarkerDrawX;
	g_radarTargetMarkerRestoreY = g_radarTargetMarkerDrawY;
	if (g_radarBlipBufferParity != 0) {
		g_radarForeEraseBlips = g_radarForeBlipBufferB;
		g_radarForeDrawBlips = g_radarForeBlipBufferA;
		g_radarAftEraseBlips = g_radarAftBlipBufferB;
		g_radarAftDrawBlips = g_radarAftBlipBufferA;
	} else {
		g_radarForeEraseBlips = g_radarForeBlipBufferA;
		g_radarForeDrawBlips = g_radarForeBlipBufferB;
		g_radarAftEraseBlips = g_radarAftBlipBufferA;
		g_radarAftDrawBlips = g_radarAftBlipBufferB;
	}

	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		ObjectRecord* object = &g_objectTable[objectIdx];
		if (objectIdx != playerObjectIdx && (g_modelTypeTable[object->objectType].flags & 1) != 0) {
			CraftData* craft = object->mobj->pCraft;
			if (g_players[g_localPlayer].currentTargetObjectIdx == objectIdx ||
				(!Object_HasActiveDecoyBeam((uint16_t)objectIdx) &&
				 craft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
				 craft->objectKind != CRAFT_OBJECT_KIND_EXPLODING))
				Hud_AddBlipToRadar((int16_t)objectIdx);
		}
	}
	for (objectIdx = g_projectileObjectSlotStart; objectIdx < g_projectileObjectSlotEnd; ++objectIdx) {
		if ((g_modelTypeTable[g_objectTable[objectIdx].objectType].flags & 1) != 0)
			Hud_AddBlipToRadar((int16_t)objectIdx);
	}
	for (objectIdx = g_regionMainObjectSlotEnd;
		 objectIdx < g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount; ++objectIdx) {
		if ((g_modelTypeTable[g_objectTable[objectIdx].objectType].flags & 1) != 0)
			Hud_AddBlipToRadar((int16_t)objectIdx);
	}

	if (g_radarTargetMarkerBackgroundSaved != 0)
		g_flightRestoreCursorFn();
	if (g_radarForePrevBlipCount != 0)
		g_flightDrawPointArrayMaskedFn((uint16_t*)&g_radarForeEraseBlips->x, g_radarForePrevBlipCount);
	if (g_radarForeBlipCount != 0)
		g_flightDrawPointArrayFn((uint16_t*)&g_radarForeDrawBlips->x, g_radarForeBlipCount);
	if (g_radarAftPrevBlipCount != 0)
		g_flightDrawPointArrayMaskedFn((uint16_t*)&g_radarAftEraseBlips->x, g_radarAftPrevBlipCount);
	if (g_radarAftBlipCount != 0)
		g_flightDrawPointArrayFn((uint16_t*)&g_radarAftDrawBlips->x, g_radarAftBlipCount);
#ifdef XVT_MODERN
	XvtCockpitInstruments_CompleteRadar();
#endif

	if (g_players[g_localPlayer].currentTargetObjectIdx == -1) {
		g_radarTargetMarkerBackgroundSaved = 0;
	} else {
		g_flightSaveDrawCursorFn();
		g_radarTargetMarkerBackgroundSaved = 1;
	}
	g_radarBlipBufferParity ^= 1;
}

// FUNCTION: XVT 0x43A0C0
void Hud_AddBlipToRadar(int16_t objIdx) {
	int playerObjectIdx = g_players[g_localPlayer].objectIndex;
	int objectIdx = (uint16_t)objIdx;
	int deltaX = g_objectTable[objectIdx].world_x - g_objectTable[playerObjectIdx].world_x;
	int deltaY = g_objectTable[objectIdx].world_y - g_objectTable[playerObjectIdx].world_y;
	int deltaZ = g_objectTable[objectIdx].world_z - g_objectTable[playerObjectIdx].world_z;
	int up;
	int side;
	int16_t frontBlip;
	int forward;
	MobileObject* mobileObject;
	int fwdZProduct;
	int fwdXProduct;
	int fwdYProduct;
	int sideZProduct;
	int sideYProduct;
	int sideXProduct;
	int upXProduct;
	int upYProduct;
	int upZProduct;
	int sideZ;

	if (g_objectTable[playerObjectIdx].mobj->orientMatrixDirty != 0) {
		FVIEW_calcrotatemove(g_objectTable[playerObjectIdx].pitch, g_objectTable[playerObjectIdx].yaw,
							 &g_objectTable[playerObjectIdx]);
		FVIEW_calcrotateorient(g_objectTable[playerObjectIdx].roll, 0, &g_objectTable[playerObjectIdx]);
	}
	fwdXProduct = Math_MulQ15(deltaX, g_objectTable[playerObjectIdx].mobj->cachedFwdX);
	fwdYProduct = Math_MulQ15(deltaY, g_objectTable[playerObjectIdx].mobj->cachedFwdY);
	forward = fwdXProduct + fwdYProduct;
	fwdZProduct = Math_MulQ15(deltaZ, g_objectTable[playerObjectIdx].mobj->cachedFwdZ);
	forward += fwdZProduct;
	sideXProduct = Math_MulQ15(deltaX, g_objectTable[playerObjectIdx].mobj->cachedSideX);
	sideYProduct = Math_MulQ15(deltaY, g_objectTable[playerObjectIdx].mobj->cachedSideY);
	side = sideXProduct + sideYProduct;
	sideZ = g_objectTable[playerObjectIdx].mobj->cachedSideZ;
	sideZProduct = Math_MulQ15(deltaZ, sideZ);
	side += sideZProduct;
	upXProduct = Math_MulQ15(deltaX, g_objectTable[playerObjectIdx].mobj->cachedUpX);
	upYProduct = Math_MulQ15(deltaY, g_objectTable[playerObjectIdx].mobj->cachedUpY);
	up = upXProduct + upYProduct;
	upZProduct = Math_MulQ15(deltaZ, g_objectTable[playerObjectIdx].mobj->cachedUpZ);
	up += upZProduct;
	up = -up;
	frontBlip = 1;
	if (forward < 0) {
		forward = -forward;
		frontBlip = 0;
	}

	mobileObject = g_objectTable[objectIdx].mobj;
	if (mobileObject == NULL) {
		g_radarBlipColor = 47;
	} else if (g_objectTable[objectIdx].genusId == CRAFT_GENUS_SATELLITE) {
		g_radarBlipColor = 47;
	} else if (mobileObject->state == 1) {
		if (((g_missionElapsedClock.subsecondTicks / 4) & 1) != 0)
			g_radarBlipColor = 59;
		else
			g_radarBlipColor = 55;
	} else {
		switch (mobileObject->iff) {
			case 0:
				g_radarBlipColor = 63;
				break;
			case 1:
			case 4:
				g_radarBlipColor = 55;
				break;
			case 2:
				g_radarBlipColor = 51;
				break;
			case 3:
				g_radarBlipColor = 59;
				break;
			case 5:
				g_radarBlipColor = 211;
				break;
			default:
				break;
		}
	}

	pai_ObjectRefUpdateApproxRangeScore(g_players[g_localPlayer].objectIndex, objectIdx);
	if (g_targetRangeScore > 122166) {
		if (g_radarBlipColor == 47)
			g_radarBlipColor = 45;
		else if (g_radarBlipColor == 211)
			g_radarBlipColor += 2;
		else
			g_radarBlipColor -= 2;
	} else if (g_targetRangeScore > 61083) {
		if (g_radarBlipColor == 47)
			g_radarBlipColor = 46;
		else if (g_radarBlipColor == 211)
			++g_radarBlipColor;
		else
			--g_radarBlipColor;
	}

	MATH2_getradarcoord(side, up, forward);
	if (frontBlip != 0) {
		radarx += g_hudElementLayouts[g_hudInstrumentSetBaseIndex].x;
		radary += g_hudElementLayouts[g_hudInstrumentSetBaseIndex].y;
		if (radary < 0)
			radary = 0;
		g_radarForeDrawBlips[g_radarForeBlipCount].x = (uint16_t)radarx;
		g_radarForeDrawBlips[g_radarForeBlipCount].y = (uint16_t)radary;
		g_radarForeDrawBlips[g_radarForeBlipCount].color = g_radarBlipColor;
#ifdef XVT_MODERN
		XvtCockpitInstruments_RecordRadar(objectIdx, 1, g_radarForeBlipCount, radarx, radary,
										  g_radarBlipColor);
#endif
		++g_radarForeBlipCount;
		if (g_radarForeBlipCount == 48)
			--g_radarForeBlipCount;
	} else {
		radarx += g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 1].x;
		radary += g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 1].y;
		if (radary < 0)
			radary = 0;
		g_radarAftDrawBlips[g_radarAftBlipCount].x = (uint16_t)radarx;
		g_radarAftDrawBlips[g_radarAftBlipCount].y = (uint16_t)radary;
		g_radarAftDrawBlips[g_radarAftBlipCount].color = g_radarBlipColor;
#ifdef XVT_MODERN
		XvtCockpitInstruments_RecordRadar(objectIdx, 0, g_radarAftBlipCount, radarx, radary,
										  g_radarBlipColor);
#endif
		++g_radarAftBlipCount;
		if (g_radarAftBlipCount == 48)
			--g_radarAftBlipCount;
	}

	if (g_players[g_localPlayer].currentTargetObjectIdx == objIdx) {
		g_radarTargetMarkerDrawX = (uint16_t)radarx;
		g_radarTargetMarkerDrawY = (uint16_t)radary;
	}
}

// FUNCTION: XVT 0x43A5E0
void Hud_UpdateTargetingComputerDisplay(void) {
	enum {
		TARGET_INSET_ELEMENT = 2,
		TARGET_PANEL_ELEMENT = 69,
		TARGET_SYSTEM_VALUE_ELEMENT = 82,
		TARGET_DISTANCE_VALUE_ELEMENT = 83,
		TARGET_DISTANCE_FRACTION_ELEMENT = 84,
		TARGET_SHIELD_VALUE_ELEMENT = 85,
		TARGET_HULL_VALUE_ELEMENT = 86,
		TARGET_CARGO_ELEMENT = 87,
		TARGET_UNUSED_ELEMENT = 88,
		TARGET_DETAIL_ELEMENT = 89,
		TARGET_ALT_PANEL_ELEMENT = 108,
		TARGET_SYSTEM_LABEL_ELEMENT = 111,
		TARGET_DISTANCE_LABEL_ELEMENT = 112,
		TARGET_SHIELD_LABEL_ELEMENT = 113,
		TARGET_HULL_LABEL_ELEMENT = 114,
		TARGET_NAME_ELEMENT = 115,
		TARGETING_COMPUTER_FEATURE_MASK = 1,
		HIDDEN_TARGET_DISPLAY_FLAGS = 1,
		NORMAL_TARGET_DISPLAY_FLAGS = 3,
		SHORT_TARGET_DISPLAY_FLAGS = 2,
		TARGET_NUMERIC_SCALE = 0x28F,
		MAX_STATUS_FIRST_WORD_LENGTH = 40,
		NO_SELECTED_COMPONENT = 50,
		WAYPOINT_ZERO_OBJECT_REF = 0x8000,
	};

	int localObjectIndex;
	uint16_t currentTargetObjectIndex;
	int16_t previousTargetObjectIndex;
	uint8_t mapCameraState;
	bool drawTargetDisplay;
	bool useLeftAlignedDetails;
	ObjectRecord* targetObject;
	MobileObject* targetMobileObject;
	CraftData* targetCraft;
	unsigned int shieldPercentage;
	unsigned int hullPercentage;
	unsigned int systemPercentage;
	uint16_t left;
	uint16_t top;
	uint16_t dirtyState = UINT16_MAX;
	uint16_t invalidState = UINT16_MAX - 1;
	char statusFirstWord[64];

#ifdef XVT_MODERN
	XvtCockpitReadouts_BeginTarget(0);
#endif

	g_flightTextShadowEnabled = 0;
	left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_ELEMENT].x;
	top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_INSET_ELEMENT].y;
	if (g_flightMissionState.provingGroundsModeActive != 0) {
#ifdef XVT_MODERN
		XvtCockpitReadouts_HideTarget();
#endif
		ProvingGrounds_DrawStatusPanel(left, top);
		return;
	}

	drawTargetDisplay = true;
	mapCameraState = g_players[g_localPlayer].mapCameraState;
	if (mapCameraState == 0) {
		if ((g_objectTable[g_players[g_localPlayer].objectIndex]
				 .mobj->pCraft->damageStats.activeHudFeatureMask &
			 TARGETING_COMPUTER_FEATURE_MASK) == 0) {
			drawTargetDisplay = false;
		}
	}

	localObjectIndex = g_players[g_localPlayer].objectIndex;
	useLeftAlignedDetails =
		localObjectIndex != -1 &&
		(g_objectTable[localObjectIndex].objectType == 1 || g_objectTable[localObjectIndex].objectType == 2 ||
		 g_objectTable[localObjectIndex].objectType == 3 ||
		 g_objectTable[localObjectIndex].objectType == 14 || g_objectTable[localObjectIndex].objectType == 4);

	if (mapCameraState == 0) {
		if (!drawTargetDisplay) {
#ifdef XVT_MODERN
			XvtCockpitReadouts_HideTarget();
			XvtCockpitText_ClearTargetFields();
#endif
			return;
		}
		if ((g_objectTable[localObjectIndex].mobj->pCraft->workingSubsystems &
			 CRAFT_SUBSYSTEM_FLAG_TARGETING_COMPUTER) == 0) {
			if (g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
				if (useLeftAlignedDetails) {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordTargetCover(TARGET_ALT_PANEL_ELEMENT);
#endif
					Hud_DrawCachedSpriteElement(TARGET_ALT_PANEL_ELEMENT, 0);
				} else {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordTargetCover(TARGET_PANEL_ELEMENT);
#endif
					Hud_DrawCachedSpriteElement(TARGET_PANEL_ELEMENT, 0);
				}
			} else {
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordTargetCover(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT);
#endif
				Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT, 0);
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_LABEL_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_HULL_LABEL_ELEMENT] = dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_LABEL_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT] = dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_ALT_PANEL_ELEMENT] = dirtyState;
			}
			drawTargetDisplay = false;
		}
	}
	if (!drawTargetDisplay) {
#ifdef XVT_MODERN
		XvtCockpitReadouts_HideTarget();
		XvtCockpitText_ClearTargetFields();
#endif
		return;
	}

	if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		FlightText_SetFontTier(0);
	} else {
		FlightText_SetFontTier(2);
	}
	g_hudTargetInsetMaskRefreshPending = 0;
	currentTargetObjectIndex = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	if (currentTargetObjectIndex != (uint16_t)g_hudCachedTargetObjectIdx) {
#ifdef XVT_MODERN
		XvtCockpitText_ClearTargetFields();
#endif
		previousTargetObjectIndex = g_hudCachedTargetObjectIdx;
		g_hudTargetInsetMaskRefreshPending = 1;
		g_hudCachedTargetObjectIdx = (int16_t)currentTargetObjectIndex;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_PANEL_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_SYSTEM_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_DISTANCE_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_FRACTION_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_DISTANCE_FRACTION_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_SHIELD_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_HULL_VALUE_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_CARGO_ELEMENT] = dirtyState;
#ifdef XVT_MODERN
		XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_CARGO);
#endif
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_UNUSED_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_UNUSED_ELEMENT] = dirtyState;
		g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT] = dirtyState;
		g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = dirtyState;
#ifdef XVT_MODERN
		XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_DETAIL);
#endif
		if (useLeftAlignedDetails || g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
			g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_ALT_PANEL_ELEMENT] = dirtyState;
			g_hudElementStateCache[TARGET_ALT_PANEL_ELEMENT] = dirtyState;
			if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
				FlightText_SetFontTier(0);
			} else {
				FlightText_SetFontTier(2);
			}
		} else {
			FlightText_SetFontTier(2);
		}
		FlightText_SetBackgroundColor(0x30);
		FlightText_SetClearLineBackground(1);

		if (previousTargetObjectIndex == -1 || previousTargetObjectIndex == -2 ||
			previousTargetObjectIndex == -3) {
			uint16_t percentWidth;

			if (useLeftAlignedDetails || g_flightResolutionMode != FLIGHT_RESOLUTION_320X240) {
				FlightText_SetColor(0x46);
			} else {
				FlightText_SetColor(0x45);
			}
			FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_LABEL_ELEMENT].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_LABEL_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_RANGE_LABEL,
									   g_strCmdThreatDisplayText[CMD_THREAT_STR_DIST],
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_DIST]);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_VALUE_ELEMENT].x +
					FlightText_MeasureStringWidth("00"),
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DISTANCE_VALUE_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_RANGE_SEPARATOR, ".", XVT_COCKPIT_ALIGN_LEFT);
#endif
			g_flightDrawCharFn('.');
			if (useLeftAlignedDetails) {
				FlightText_SetFontTier(0);
			}
			percentWidth = FlightText_MeasureStringWidth(g_countermeasureAmmoWidthText);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_LABEL_ELEMENT].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_LABEL_ELEMENT].y);
			if (useLeftAlignedDetails || g_flightResolutionMode != FLIGHT_RESOLUTION_320X240) {
				FlightText_SetColor(0x46);
			} else {
				FlightText_SetColor(0x45);
			}
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_SHIELD_LABEL,
									   g_strCmdThreatDisplayText[CMD_THREAT_STR_SHD], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_SHD]);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT].x +
					percentWidth,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_SHIELD_PERCENT, "%", XVT_COCKPIT_ALIGN_LEFT);
#endif
			g_flightDrawCharFn('%');
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_HULL_LABEL_ELEMENT].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_HULL_LABEL_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_HULL_LABEL,
									   g_strCmdThreatDisplayText[CMD_THREAT_STR_HULL],
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_HULL]);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT].x + percentWidth,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_HULL_PERCENT, "%", XVT_COCKPIT_ALIGN_LEFT);
#endif
			g_flightDrawCharFn('%');
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_LABEL_ELEMENT].x,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_LABEL_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_SYSTEM_LABEL,
									   g_strCmdThreatDisplayText[CMD_THREAT_STR_SYS], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_SYS]);
			FlightText_SetCursor(
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT].x +
					percentWidth,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_SYSTEM_PERCENT, "%", XVT_COCKPIT_ALIGN_LEFT);
#endif
			g_flightDrawCharFn('%');
			if (useLeftAlignedDetails && g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
				FlightText_SetFontTier(2);
			}
		}

		if (currentTargetObjectIndex != UINT16_MAX) {
			uint16_t targetObjectType;

			targetObject = &g_objectTable[currentTargetObjectIndex];
			targetObjectType = targetObject->objectType;
			targetMobileObject = targetObject->mobj;
			targetCraft = targetMobileObject != NULL ? targetMobileObject->pCraft : NULL;
			left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].x;
			top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].y;
			FlightText_SetClipRect(
				left, top,
				left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].clipWidth,
				top + g_flightFontLineHeight + 1);
			g_flightFillClipRectFn();
			if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && g_flightPlayerCount > 1) {
				int16_t displayFlags = NORMAL_TARGET_DISPLAY_FLAGS;
				if (g_objectTable[currentTargetObjectIndex].mobj != NULL && targetCraft != NULL &&
					g_flightMissionState.locatePlayersEnabled == 0) {
					int playerIff;

					playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
					if (targetCraft->iffVisibility[playerIff] == 0) {
						int flightGroupIndex;
						int team;
						int hostile;

						flightGroupIndex = g_objectTable[currentTargetObjectIndex].flightGroupIdx;
						team = g_missionFlightGroups[flightGroupIndex].fg.team;
						hostile = 0;
						if (team != playerIff) {
							hostile = g_missionTeams[playerIff].allies[team] == 0;
						}
						if (hostile == 1 && g_missionFlightGroups[flightGroupIndex].fg.playerNumber != 0) {
							displayFlags = HIDDEN_TARGET_DISPLAY_FLAGS;
						}
					}
				}
				Hud_AppendObjectDisplayName(currentTargetObjectIndex, displayFlags);
			} else {
				Hud_AppendObjectDisplayName(currentTargetObjectIndex, NORMAL_TARGET_DISPLAY_FLAGS);
			}
			targetObject = &g_objectTable[currentTargetObjectIndex];
			if (targetObject->playerOwnerIdx != -1) {
				if (g_flightMissionState.locatePlayersEnabled != 0 ||
					targetCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] != 0 ||
					!(g_missionFlightGroups[targetObject->flightGroupIdx].fg.team ==
							  (uint16_t)g_players[g_localPlayer].playerIff
						  ? 0
						  : g_missionTeams[(uint16_t)g_players[g_localPlayer].playerIff]
									.allies[g_missionFlightGroups[targetObject->flightGroupIdx].fg.team] ==
								0)) {
					FlightText_AppendScratchChar('-');
					FlightText_AppendScratchString(
						NetSession_GetPlayerName(g_objectTable[currentTargetObjectIndex].playerOwnerIdx));
				}
			}
			FlightText_SetCursor(left, top);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_NAME, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_CENTER);
#endif
			FlightText_DrawStringCentered(g_flightTextScratchBuffer);

			if (g_projectileObjectSlotStart <= currentTargetObjectIndex &&
				g_projectileObjectSlotEnd > currentTargetObjectIndex &&
				g_projectileDamageByObjectType
						.warheadClass[targetObjectType - PROJECTILE_OBJECT_TYPE_FIRST] != 0) {
				WarheadGuidanceState* guidance =
					g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx]
						.mobj->pWarheadGuidance;
				if (guidance->homingTier == 0 || guidance->targetObjIdx == UINT16_MAX) {
					FlightText_SetScratch(g_strMeshComponentNames[MESH_COMPONENT_32_DASHES]);
				} else if (g_objectTable[guidance->targetObjIdx].playerOwnerIdx == g_localPlayer) {
					FlightText_SetScratch(g_strCmdThreatDisplayText[CMD_THREAT_STR_THIS_CRAFT]);
				} else {
					Hud_AppendObjectDisplayName(guidance->targetObjIdx, SHORT_TARGET_DISPLAY_FLAGS);
				}
				left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x;
				top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y;
				FlightText_SetClipRect(
					left, top,
					left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].clipWidth,
					top + g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				FlightText_SetColor(0x4E);
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL, g_flightTextScratchBuffer,
										   XVT_COCKPIT_ALIGN_RIGHT);
#endif
				FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
			}
		} else {
			if (g_hudInstrumentSetBaseIndex != HUD_MAP_INSTRUMENT_BASE_INDEX) {
				if (useLeftAlignedDetails || g_players[g_localPlayer].mapCameraState != 0) {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordTargetCover(g_hudInstrumentSetBaseIndex +
														 TARGET_ALT_PANEL_ELEMENT);
#endif
					Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + TARGET_ALT_PANEL_ELEMENT, 0);
				} else {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordTargetCover(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT);
#endif
					Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT, 0);
				}
			} else {
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordTargetCover(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT);
#endif
				Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + TARGET_PANEL_ELEMENT, 0);
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_LABEL_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_HULL_LABEL_ELEMENT] = dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_LABEL_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT] =
					dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT] = dirtyState;
				g_hudElementStateCache[g_hudInstrumentSetBaseIndex + TARGET_ALT_PANEL_ELEMENT] = dirtyState;
			}
		}
	}

	if (g_players[g_localPlayer].currentTargetObjectIdx == -1) {
		return;
	}

	FlightText_SetBackgroundColor(0x30);
	left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].x;
	top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].y;
	FlightText_SetClipRect(
		left, top, left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_NAME_ELEMENT].clipWidth,
		top + g_flightFontLineHeight + 1);
	g_flightFillClipRectFn();
	if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && g_flightPlayerCount > 1) {
		int16_t displayFlags = NORMAL_TARGET_DISPLAY_FLAGS;
		if (g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj != NULL &&
			g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj->pCraft != NULL &&
			g_flightMissionState.locatePlayersEnabled == 0) {
			CraftData* displayCraft =
				g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj->pCraft;
			int playerIff;

			playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (displayCraft->iffVisibility[playerIff] == 0) {
				int flightGroupIndex =
					g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].flightGroupIdx;
				int team;
				int hostile;

				team = g_missionFlightGroups[flightGroupIndex].fg.team;
				hostile = 0;
				if (team != playerIff) {
					hostile = g_missionTeams[playerIff].allies[team] == 0;
				}
				if (hostile == 1 && g_missionFlightGroups[flightGroupIndex].fg.playerNumber != 0) {
					displayFlags = HIDDEN_TARGET_DISPLAY_FLAGS;
				}
			}
		}
		Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx, displayFlags);
	} else {
		Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
									NORMAL_TARGET_DISPLAY_FLAGS);
	}
	targetObject = &g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx];
	if (targetObject->playerOwnerIdx != -1) {
		if (g_flightMissionState.locatePlayersEnabled != 0 ||
			targetObject->mobj->pCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] != 0 ||
			!(g_missionFlightGroups[targetObject->flightGroupIdx].fg.team ==
					  (uint16_t)g_players[g_localPlayer].playerIff
				  ? 0
				  : g_missionTeams[(uint16_t)g_players[g_localPlayer].playerIff]
							.allies[g_missionFlightGroups[targetObject->flightGroupIdx].fg.team] == 0)) {
			FlightText_AppendScratchString(" (");
			FlightText_AppendScratchString(NetSession_GetPlayerName(
				g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].playerOwnerIdx));
			FlightText_AppendScratchChar(')');
		}
	}
	FlightText_SetCursor(left, top);
#ifdef XVT_MODERN
	XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_NAME, g_flightTextScratchBuffer,
							   XVT_COCKPIT_ALIGN_CENTER);
#endif
	FlightText_DrawStringCentered(g_flightTextScratchBuffer);

	targetCraft = NULL;
	targetMobileObject = g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj;
	if (targetMobileObject != NULL) {
		targetCraft = targetMobileObject->pCraft;
	}
	if (targetMobileObject == NULL) {
		shieldPercentage = 0;
	} else {
		unsigned int shieldTotal;
		unsigned int shieldAverage;
		unsigned int maxShield;

		if (targetCraft == NULL || targetCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
			targetCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING) {
			maxShield = 0;
		} else {
			shieldTotal = targetCraft->shieldEnergy[0] + targetCraft->shieldEnergy[1];
			shieldAverage = shieldTotal >> 1;
			maxShield = 2 * g_modelDefs[targetCraft->modelIndex].shieldStrength;
		}
		if (maxShield != 0) {
			shieldPercentage = (uint16_t)MATH2_percentage(shieldAverage, maxShield);
			shieldPercentage = 2 * (shieldPercentage / TARGET_NUMERIC_SCALE);
			if (shieldTotal != 0 && shieldPercentage == 0) {
				shieldPercentage = 1;
			}
		} else {
			shieldPercentage = 0;
		}
	}
	if (useLeftAlignedDetails) {
		FlightText_SetFontTier(0);
	}
	Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + TARGET_SHIELD_VALUE_ELEMENT,
								 (int16_t)shieldPercentage, 1);
	if (useLeftAlignedDetails && g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		FlightText_SetFontTier(2);
	}

	if ((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx < g_activeRegionCraftObjectSlotEnd &&
		targetCraft != NULL) {
		if (targetCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
			targetCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING) {
			hullPercentage = 0;
		} else if (targetCraft->hullDamage > targetCraft->hullMax) {
			hullPercentage = 1;
		} else {
			hullPercentage = (uint16_t)MATH2_percentage(targetCraft->hullMax - targetCraft->hullDamage,
														targetCraft->hullMax);
			hullPercentage = hullPercentage / TARGET_NUMERIC_SCALE;
			if (hullPercentage == 0) {
				hullPercentage = 1;
			}
		}
	} else {
		hullPercentage = 100;
	}
	if (useLeftAlignedDetails) {
		FlightText_SetFontTier(0);
	}
	Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + TARGET_HULL_VALUE_ELEMENT, hullPercentage, 1);
	if (useLeftAlignedDetails && g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		FlightText_SetFontTier(2);
	}

	targetObject = &g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx];
	if (targetObject->mobj != NULL) {
		if (targetCraft == NULL || targetCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
			targetCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING) {
			systemPercentage = 0;
		} else if (targetCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
				   targetCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING ||
				   targetCraft->workingSubsystems == 0) {
			systemPercentage = 0;
		} else {
			uint16_t systemStrength = g_modelDefs[targetCraft->modelIndex].systemStrength;
			uint16_t subsystemDamage = (uint16_t)targetCraft->subsystemDamage;
			if (systemStrength <= subsystemDamage) {
				systemPercentage = 0;
			} else {
				systemPercentage = MATH2_divide(systemStrength - subsystemDamage, systemStrength);
				systemPercentage /= TARGET_NUMERIC_SCALE;
			}
			if (systemPercentage > 25 && targetCraft->weaponFireInhibitTimer != 0) {
				systemPercentage = 25;
			}
		}
	} else {
		systemPercentage = targetObject->typeSpecificWord == 0 ? 0 : 100;
	}
	if (useLeftAlignedDetails) {
		FlightText_SetFontTier(0);
	}
	Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + TARGET_SYSTEM_VALUE_ELEMENT,
								 (int16_t)systemPercentage, 1);
	if (useLeftAlignedDetails && g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		FlightText_SetFontTier(2);
	}

	if (g_players[g_localPlayer].mapCameraState == 0 &&
		(g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].genusId ==
			 CRAFT_GENUS_STARSHIP ||
		 g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].genusId ==
			 CRAFT_GENUS_PLATFORM)) {
		Object_DirectionAndDistanceToMeshCenter(g_players[g_localPlayer].objectIndex,
												(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
												(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
	} else {
		Player_ComputePolarToObjectRef(g_localPlayer,
									   (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx);
	}
	Hud_DrawTargetDistance(trig2_polardistance);

	targetObject = &g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx];
	if (targetObject->genusId != CRAFT_GENUS_STARFIGHTER) {
		uint16_t cargoState = 2;
		const char* cargoText = g_strMeshComponentNames[MESH_COMPONENT_32_DASHES];
		int8_t componentDisplayState;

		if ((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx < g_activeRegionCraftObjectSlotEnd &&
			targetCraft != NULL &&
			g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj->state == 0) {
			if (targetCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] == 0 ||
				targetCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
				targetCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING) {
				cargoState = 0;
				cargoText = g_strWarheadUnknown;
			} else {
				cargoState = 1;
				cargoText = targetCraft->specialCargoName;
				if (targetCraft->specialCargoName[0] == '\0') {
					cargoState = 2;
					cargoText = g_strCmdThreatDisplayText[CMD_THREAT_STR_NO_CARGO];
				}
			}
		}
		if (cargoState != g_hudElementStateCache[TARGET_CARGO_ELEMENT]) {
			g_hudElementStateCache[TARGET_CARGO_ELEMENT] = cargoState;
			left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x;
			top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y;
			FlightText_SetClipRect(
				left, top,
				left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].clipWidth,
				top + g_flightFontLineHeight + 1);
			g_flightFillClipRectFn();
			FlightText_SetCursor(left, top);
			FlightText_SetColor(0x46);
			if (!useLeftAlignedDetails) {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, cargoText, XVT_COCKPIT_ALIGN_RIGHT);
#endif
				FlightText_DrawStringRightAligned(cargoText);
			} else {
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, cargoText, XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(cargoText);
			}
		}

		componentDisplayState = 0;
		if (targetMobileObject == NULL) {
			componentDisplayState = 1;
		} else if (targetCraft != NULL) {
			componentDisplayState = 2;
		}
		if (componentDisplayState > 0) {
			uint16_t selectedComponent = NO_SELECTED_COMPONENT;
			if (componentDisplayState == 2) {
				selectedComponent = g_players[g_localPlayer].selectedTargetComponent;
			}
			FlightText_SetColor(0x4E);
			if (selectedComponent != g_hudElementStateCache[TARGET_DETAIL_ELEMENT]) {
				g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = selectedComponent;
				left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x;
				top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y;
				FlightText_SetClipRect(
					left, top,
					left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].clipWidth,
					top + g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				if (selectedComponent == NO_SELECTED_COMPONENT) {
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL,
											   g_strMeshComponentNames[MESH_COMPONENT_32_DASHES],
											   XVT_COCKPIT_ALIGN_RIGHT);
#endif
					FlightText_DrawStringRightAligned(g_strMeshComponentNames[MESH_COMPONENT_32_DASHES]);
				} else {
					int meshIndex = (uint16_t)g_players[g_localPlayer].selectedTargetComponent;
					int objectType =
						g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType;
					uint16_t meshType;

					if (objectType < 73) {
						if (meshIndex < 0) {
							meshType = 0;
						} else {
							int meshCount = g_objectTypeMeshCache[objectType].meshCount;
							if (meshIndex >= meshCount) {
								meshIndex = meshCount - 1;
							}
							meshType = g_objectTypeMeshCache[objectType].meshTypes[meshIndex];
						}
					} else {
						meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIndex);
					}
					if (targetObject->genusId == CRAFT_GENUS_STARFIGHTER &&
						meshType == MESH_COMPONENT_07_BRIDGE) {
						meshType = MESH_COMPONENT_26_COCKPIT;
					}
					if ((targetObject->objectType == 38 || targetObject->objectType == 39) &&
						meshType == MESH_COMPONENT_07_BRIDGE) {
						meshType = MESH_COMPONENT_26_COCKPIT;
					}
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL,
											   g_strMeshComponentNames[meshType], XVT_COCKPIT_ALIGN_RIGHT);
#endif
					FlightText_DrawStringRightAligned(g_strMeshComponentNames[meshType]);
				}
			}
		}
		return;
	}

	{
		int16_t ownershipDisplayMode;

		if (g_activeRegionCraftObjectSlotEnd <= (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx ||
			targetObject->mobj == NULL) {
			ownershipDisplayMode = 0;
		} else if (targetObject->playerOwnerIdx == -1) {
			ownershipDisplayMode = 1;
			if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && g_flightPlayerCount > 1 &&
				targetCraft != NULL && g_flightMissionState.locatePlayersEnabled == 0) {
				int playerIff;

				playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
				if (targetCraft->iffVisibility[playerIff] == 0) {
					int team;
					int hostile;

					team = g_missionFlightGroups[targetObject->flightGroupIdx].fg.team;
					hostile = team == playerIff ? 0 : g_missionTeams[playerIff].allies[team] == 0;
					if (hostile == 1 &&
						g_missionFlightGroups[targetObject->flightGroupIdx].fg.playerNumber != 0) {
						ownershipDisplayMode = 0;
					}
				}
			}
		} else {
			ownershipDisplayMode = g_missionHeader.missionType == MISSION_TYPE_SKIRMISH ? -1 : 0;
		}

		if (ownershipDisplayMode == 1) {
			MobileObject* orderMobileObject = targetObject->mobj;
			CraftData* orderCraft = orderMobileObject->pCraft;
			AiController* aiController;
			uint16_t displayPlanId;
			uint16_t aiTargetObjectIndex;

			if (orderCraft == NULL) {
				return;
			}
			aiController = &orderCraft->aiController;
			displayPlanId = aiController->pendingPlanId;
			if (orderCraft->workingSubsystems == 0) {
				displayPlanId = (uint16_t)pai_findplanbyname("disabledpln");
			} else if (orderMobileObject->speed == 0) {
				const char* planName = g_planTable[aiController->pendingPlanId].name;
				if (strcmp(planName, "flyhomepln") == 0 || strcmp(planName, "followhomepln") == 0 ||
					strcmp(planName, "flyhomeevadepln") == 0 || strcmp(planName, "followhomeevadepln") == 0 ||
					strcmp(planName, "enterhangarpln") == 0 || strcmp(planName, "exithangarpln") == 0 ||
					strcmp(planName, "intohyperspacepln") == 0 ||
					strcmp(planName, "outofhyperspacepln") == 0 ||
					strcmp(planName, "starshipintohyperpln") == 0 ||
					strcmp(planName, "starshipfollowhomepln") == 0) {
					displayPlanId = (uint16_t)pai_findplanbyname("waitpln");
				}
			}
			if (displayPlanId != g_hudElementStateCache[TARGET_CARGO_ELEMENT]) {
				const char* statusText;
				const char* statusChar;
				uint16_t wordLength;

				g_hudElementStateCache[TARGET_CARGO_ELEMENT] = displayPlanId;
				left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x;
				top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y;
				FlightText_SetClipRect(
					left, top,
					left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].clipWidth,
					top + g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				FlightText_SetColor(0x46);
				statusText = g_strInFlightMessages[g_planReportMessageIdByPlanId[displayPlanId]];
				statusChar = statusText;
				for (wordLength = 0; wordLength < MAX_STATUS_FIRST_WORD_LENGTH; ++wordLength, ++statusChar) {
					if (*statusChar == ' ') {
						strncpy(statusFirstWord, statusText, wordLength);
						statusFirstWord[wordLength] = '\0';
						break;
					}
				}
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, statusFirstWord,
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(statusFirstWord);
			}

			aiTargetObjectIndex = aiController->targetObjIdx;
			if (aiTargetObjectIndex == 255 || aiTargetObjectIndex == UINT16_MAX) {
				if (g_hudElementStateCache[TARGET_DETAIL_ELEMENT] != invalidState) {
					g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = invalidState;
#ifdef XVT_MODERN
					XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_DETAIL);
#endif
					FlightText_SetClipRect(
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x +
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT]
								.clipWidth,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y +
							g_flightFontLineHeight + 1);
					g_flightFillClipRectFn();
				}
			} else if (aiTargetObjectIndex != g_hudElementStateCache[TARGET_DETAIL_ELEMENT]) {
				g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = aiTargetObjectIndex;
				left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x;
				top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y;
				FlightText_SetClipRect(
					left, top,
					left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].clipWidth,
					top + g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				Hud_AppendObjectDisplayName(aiTargetObjectIndex, NORMAL_TARGET_DISPLAY_FLAGS);
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL, g_flightTextScratchBuffer,
										   XVT_COCKPIT_ALIGN_RIGHT);
#endif
				FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
			}
			return;
		}

		if (ownershipDisplayMode == 0) {
			FlightText_SetBackgroundColor(0x30);
			if (g_hudElementStateCache[TARGET_CARGO_ELEMENT] != invalidState) {
				g_hudElementStateCache[TARGET_CARGO_ELEMENT] = invalidState;
#ifdef XVT_MODERN
				XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_CARGO);
#endif
				FlightText_SetClipRect(
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x +
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].clipWidth,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y +
						g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
			}
			if (g_hudElementStateCache[TARGET_DETAIL_ELEMENT] != invalidState) {
				g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = invalidState;
#ifdef XVT_MODERN
				XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_DETAIL);
#endif
				FlightText_SetClipRect(
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x +
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].clipWidth,
					g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y +
						g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
			}
		} else {
			int playerOwnerIndex = targetObject->playerOwnerIdx;
			if (g_players[playerOwnerIndex].hyperspacePhase != 0) {
				if (g_hudElementStateCache[TARGET_CARGO_ELEMENT] != IFMSG_181_HYPERING_OUT_OF_COMBAT_AREA) {
					const char* statusChar;
					uint16_t wordLength;

					g_hudElementStateCache[TARGET_CARGO_ELEMENT] = IFMSG_181_HYPERING_OUT_OF_COMBAT_AREA;
					left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x;
					top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y;
					FlightText_SetClipRect(
						left, top,
						left +
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].clipWidth,
						top + g_flightFontLineHeight + 1);
					g_flightFillClipRectFn();
					FlightText_SetCursor(left, top);
					FlightText_SetColor(0x46);
					statusChar = g_strInFlightMessages[IFMSG_181_HYPERING_OUT_OF_COMBAT_AREA];
					for (wordLength = 0; wordLength < MAX_STATUS_FIRST_WORD_LENGTH;
						 ++wordLength, ++statusChar) {
						if (*statusChar == ' ') {
							strncpy(statusFirstWord,
									g_strInFlightMessages[IFMSG_181_HYPERING_OUT_OF_COMBAT_AREA], wordLength);
							statusFirstWord[wordLength] = '\0';
							break;
						}
					}
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, statusFirstWord,
											   XVT_COCKPIT_ALIGN_LEFT);
#endif
					FlightText_DrawString(statusFirstWord);
				}
				if (g_hudElementStateCache[TARGET_DETAIL_ELEMENT] != dirtyState) {
					g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = dirtyState;
#ifdef XVT_MODERN
					XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_TARGET_DETAIL);
#endif
					FlightText_SetClipRect(
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x +
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT]
								.clipWidth,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y +
							g_flightFontLineHeight + 1);
					g_flightFillClipRectFn();
				}
			} else {
				uint16_t ownerTargetObjectIndex =
					(uint16_t)g_players[playerOwnerIndex].currentTargetObjectIdx;
				if (ownerTargetObjectIndex != UINT16_MAX) {
					if (ownerTargetObjectIndex != g_hudElementStateCache[TARGET_CARGO_ELEMENT]) {
						const char* statusChar;
						uint16_t wordLength;

						g_hudElementStateCache[TARGET_CARGO_ELEMENT] = ownerTargetObjectIndex;
						left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x;
						top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y;
						FlightText_SetClipRect(
							left, top,
							left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT]
									   .clipWidth,
							top + g_flightFontLineHeight + 1);
						g_flightFillClipRectFn();
						FlightText_SetCursor(left, top);
						FlightText_SetColor(0x46);
						statusChar = g_strInFlightMessages[IFMSG_165_ATTACKING_TARGET];
						for (wordLength = 0; wordLength < MAX_STATUS_FIRST_WORD_LENGTH;
							 ++wordLength, ++statusChar) {
							if (*statusChar == ' ') {
								strncpy(statusFirstWord, g_strInFlightMessages[IFMSG_165_ATTACKING_TARGET],
										wordLength);
								statusFirstWord[wordLength] = '\0';
								break;
							}
						}
#ifdef XVT_MODERN
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, statusFirstWord,
												   XVT_COCKPIT_ALIGN_LEFT);
#endif
						FlightText_DrawString(statusFirstWord);
					}
					if (ownerTargetObjectIndex != g_hudElementStateCache[TARGET_DETAIL_ELEMENT]) {
						g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = ownerTargetObjectIndex;
						left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x;
						top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y;
						FlightText_SetClipRect(
							left, top,
							left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT]
									   .clipWidth,
							top + g_flightFontLineHeight + 1);
						g_flightFillClipRectFn();
						FlightText_SetCursor(left, top);
						Hud_AppendObjectDisplayName(
							(uint16_t)g_players
								[g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx]
									 .playerOwnerIdx]
									.currentTargetObjectIdx,
							NORMAL_TARGET_DISPLAY_FLAGS);
#ifdef XVT_MODERN
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL, g_flightTextScratchBuffer,
												   XVT_COCKPIT_ALIGN_RIGHT);
#endif
						FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
					}
				} else {
					if (g_hudElementStateCache[TARGET_CARGO_ELEMENT] != IFMSG_160_PATROLLING) {
						const char* statusChar;
						uint16_t wordLength;

						g_hudElementStateCache[TARGET_CARGO_ELEMENT] = IFMSG_160_PATROLLING;
						left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].x;
						top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT].y;
						FlightText_SetClipRect(
							left, top,
							left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_CARGO_ELEMENT]
									   .clipWidth,
							top + g_flightFontLineHeight + 1);
						g_flightFillClipRectFn();
						FlightText_SetCursor(left, top);
						FlightText_SetColor(0x46);
						statusChar = g_strInFlightMessages[IFMSG_160_PATROLLING];
						for (wordLength = 0; wordLength < MAX_STATUS_FIRST_WORD_LENGTH;
							 ++wordLength, ++statusChar) {
							if (*statusChar == ' ') {
								strncpy(statusFirstWord, g_strInFlightMessages[IFMSG_160_PATROLLING],
										wordLength);
								statusFirstWord[wordLength] = '\0';
								break;
							}
						}
#ifdef XVT_MODERN
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, statusFirstWord,
												   XVT_COCKPIT_ALIGN_LEFT);
#endif
						FlightText_DrawString(statusFirstWord);
					}
					if ((uint16_t)g_hudElementStateCache[TARGET_DETAIL_ELEMENT] != WAYPOINT_ZERO_OBJECT_REF) {
						g_hudElementStateCache[TARGET_DETAIL_ELEMENT] = (int16_t)WAYPOINT_ZERO_OBJECT_REF;
						left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].x;
						top = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT].y;
						FlightText_SetClipRect(
							left, top,
							left + g_hudElementLayouts[g_hudInstrumentSetBaseIndex + TARGET_DETAIL_ELEMENT]
									   .clipWidth,
							top + g_flightFontLineHeight + 1);
						g_flightFillClipRectFn();
						FlightText_SetCursor(left, top);
						Hud_AppendObjectDisplayName(WAYPOINT_ZERO_OBJECT_REF, NORMAL_TARGET_DISPLAY_FLAGS);
#ifdef XVT_MODERN
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_DETAIL, g_flightTextScratchBuffer,
												   XVT_COCKPIT_ALIGN_RIGHT);
#endif
						FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
					}
				}
			}
		}
	}
}

// FUNCTION: XVT 0x43C290
void Hud_AppendObjectDisplayName(uint16_t objectRef, int16_t displayFlags) {
	int objectIndex;
	ObjectRecord* object;
	uint16_t objectType;
	int8_t iff;
	MobileObject* mobileObject;
	CraftData* craft;
	int flightGroupIdx;
	int16_t craftNumber;
	uint8_t staticIff;
	uint16_t hundredsDigit;
	uint16_t tensDigit;
	uint16_t onesDigit;

	g_flightTextScratchBuffer[0] = 0;
	if (objectRef >= 0x8000) {
		if ((displayFlags & 1) != 0) {
			FlightText_AppendScratchChar(254);
			FlightText_AppendScratchChar(67);
			FlightText_AppendScratchString(g_strWaypointNames[(uint16_t)(objectRef + 0x8000)]);
		}
		return;
	}

	objectIndex = objectRef;
	object = &g_objectTable[objectIndex];
	if (g_objectTable[objectIndex].mobj != NULL) {
		objectType = object->objectType;
		if (objectType != 0) {
			FlightText_AppendScratchChar(254);
			iff = g_objectTable[objectIndex].mobj->iff;
			if (iff == 0) {
				FlightText_AppendScratchChar(81);
			} else if (iff == 1 || iff == 4) {
				FlightText_AppendScratchChar(73);
			} else if (iff == 2) {
				FlightText_AppendScratchChar(69);
			} else if (iff == 5) {
				FlightText_AppendScratchChar(85);
			} else {
				FlightText_AppendScratchChar(77);
			}

			mobileObject = g_objectTable[objectIndex].mobj;
			if (mobileObject->state == 0) {
				craft = mobileObject->pCraft;
				if ((displayFlags & 1) != 0)
					FlightText_AppendScratchString(g_modelDefs[craft->modelIndex].name);
				if ((displayFlags & 4) != 0) {
					FlightText_AppendScratchChar(58);
				} else if ((displayFlags & 3) == 3) {
					FlightText_AppendScratchChar(58);
					FlightText_AppendScratchChar(32);
				}
				if ((displayFlags & 2) != 0) {
					FlightText_AppendScratchChar(254);
					iff = g_objectTable[objectIndex].mobj->iff;
					if (iff == 0)
						FlightText_AppendScratchChar(82);
					else if (iff == 1 || iff == 4)
						FlightText_AppendScratchChar(74);
					else if (iff == 2)
						FlightText_AppendScratchChar(70);
					else if (iff == 5)
						FlightText_AppendScratchChar(86);
					else
						FlightText_AppendScratchChar(78);
					flightGroupIdx = g_objectTable[objectIndex].flightGroupIdx;
					FlightText_AppendScratchString(g_missionFlightGroups[flightGroupIdx].fg.name);
					craftNumber = Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
					if (craftNumber != 0) {
						FlightText_AppendScratchChar(32);
						if ((uint16_t)craftNumber >= 1000)
							craftNumber = 999;
						if ((uint16_t)craftNumber >= 100) {
							hundredsDigit = (uint16_t)craftNumber / 100;
							tensDigit = (uint16_t)craftNumber % 100 / 10;
							onesDigit = (uint16_t)craftNumber % 100 % 10;
							FlightText_AppendScratchChar(hundredsDigit + 48);
							FlightText_AppendScratchChar(tensDigit + 48);
							FlightText_AppendScratchChar(onesDigit + 48);
						} else if ((uint16_t)craftNumber >= 10) {
							tensDigit = (uint16_t)craftNumber / 10;
							onesDigit = (uint16_t)craftNumber % 10;
							FlightText_AppendScratchChar(tensDigit + 48);
							FlightText_AppendScratchChar(onesDigit + 48);
						} else {
							FlightText_AppendScratchChar(craftNumber + 48);
						}
					}
				}
			} else if ((displayFlags & 1) != 0) {
				if (objectType >= 0x8f && objectType <= 0x9b)
					FlightText_AppendScratchString(g_strWarheadNames[objectType - 0x8f]);
				else if (objectType >= 0x46 && objectType <= 0x54)
					FlightText_AppendScratchString(g_strBuoyNames[objectType - 0x46]);
			}
			return;
		}
	} else {
		objectType = object->objectType;
		if (objectType != 0) {
			FlightText_AppendScratchChar(254);
			staticIff = g_missionFlightGroups[g_objectTable[objectIndex].flightGroupIdx].fg.iff;
			if (staticIff == 0) {
				FlightText_AppendScratchChar(81);
			} else if (staticIff == 1 || staticIff == 4) {
				FlightText_AppendScratchChar(73);
			} else if (staticIff == 2) {
				FlightText_AppendScratchChar(69);
			} else if (staticIff == 5) {
				FlightText_AppendScratchChar(86);
			} else {
				FlightText_AppendScratchChar(77);
			}
			if ((displayFlags & 1) != 0 && objectType >= 0x46 && objectType <= 0x55)
				FlightText_AppendScratchString(g_strBuoyNames[objectType - 0x46]);
			if ((displayFlags & 4) != 0) {
				FlightText_AppendScratchChar(58);
			} else if ((displayFlags & 3) == 3) {
				FlightText_AppendScratchChar(58);
				FlightText_AppendScratchChar(32);
			}
			if ((displayFlags & 2) != 0) {
				FlightText_AppendScratchChar(254);
				if (staticIff == 0) {
					FlightText_AppendScratchChar(82);
				} else if (staticIff == 1 || staticIff == 4) {
					FlightText_AppendScratchChar(74);
				} else if (staticIff == 2) {
					FlightText_AppendScratchChar(70);
				} else if (staticIff == 5) {
					FlightText_AppendScratchChar(86);
				} else {
					FlightText_AppendScratchChar(78);
				}
				FlightText_AppendScratchString(
					g_missionFlightGroups[g_objectTable[objectIndex].flightGroupIdx].fg.name);
			}
			return;
		}
	}
	FlightText_SetScratch(g_strMeshComponentNames[32]);
}

// FUNCTION: XVT 0x43C740
int Hud_MissionFG_GetCraftNumberIfShown(int flightGroupIdx, const CraftData* craft) {
	if (g_missionFlightGroups[flightGroupIdx].fg.disableWaveNumbering == 1 ||
		(g_missionFlightGroups[flightGroupIdx].fg.globalUnit == 0 &&
		 g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft == 1 &&
		 g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves == 0)) {
		return 0;
	}

	return craft->craftIndexInGroup;
}

// FUNCTION: XVT 0x43C890
void Hud_DrawTargetDistance(int polarDistance) {
	uint16_t distanceHundredths;
	uint16_t wholeDistance;

	distanceHundredths = (uint32_t)(polarDistance * 161) >> 16;
	if (distanceHundredths >= 10000)
		distanceHundredths = 9999;

	wholeDistance = distanceHundredths / 100;
	Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + 83, wholeDistance, 1);
	Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + 84, distanceHundredths - wholeDistance * 100,
								 2);
}

// FUNCTION: XVT 0x43C900
void Hud_UpdateTargetingLockIndicator(void) {
	uint16_t indicatorState;

	if (g_players[g_localPlayer].selectedWeaponMode == 0) {
		indicatorState = g_targetLockActive == 0 ? 0 : 4;
	} else {
		if (g_players[g_localPlayer].currentTargetObjectIdx != -1) {
			indicatorState = g_players[g_localPlayer].missileLockState + 1;
		} else {
			indicatorState = 1;
		}
		g_targetLockActive = 1;
		if (g_players[g_localPlayer].missileLockState != 2) {
			g_targetLockActive = 0;
		}
	}

	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 52, indicatorState);
	FlightSurface_Unlock();
	fsfx_UpdateTargetingTone(indicatorState);
	FlightSurface_Lock();
}

// FUNCTION: XVT 0x43C9A0
void Hud_DrawReticle3D(void) {
	enum {
		LASER_CHARGE_HALF = 64,
		LASER_CHARGE_SCALE = 6,
		LASER_CHARGE_SEGMENT_COUNT = 10,
		LASER_CHARGE_DENOMINATOR = 127,
		LASER_PERCENT_SCALE = 655,
		LASER_CHARGE_ELEMENT_BASE = 3,
		LASER_SELECTION_ELEMENT_BASE = 11,
		LASER_READY_ELEMENT_BASE = 53,
		LASER_LOCK_ELEMENT_BASE = 61,
		DEFAULT_HUD_WIDTH = 320,
		DEFAULT_HUD_HEIGHT = 200,
	};

	CraftData* craft;
	uint8_t* laserGroupLastSlot;
	uint16_t laserSlotCount;
	uint16_t laserSlot;

	g_targetLockActive = 0;
	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	laserSlot = 0;
	laserSlotCount = craft->laserSlotCount;
	if (laserSlotCount == 0)
		return;

	laserGroupLastSlot = g_modelDefs[craft->modelIndex].laserGroupLastSlot;
	for (; laserSlotCount > laserSlot; ++laserSlot) {
		uint16_t laserBank;
		int layoutIndex;
		int x;
		int y;
		uint16_t selector;
		int16_t charge;
		int16_t chargedSegmentState = 0;
		uint16_t readyState;
		uint16_t lockState;
		int objectIndex;
		int supportedCraft;

		laserBank = laserSlot > *laserGroupLastSlot;
		layoutIndex = g_hudInstrumentSetBaseIndex + laserSlot;
		x = (int16_t)g_hudElementLayouts[layoutIndex + LASER_CHARGE_ELEMENT_BASE].x;
		y = g_hudElementLayouts[layoutIndex + LASER_CHARGE_ELEMENT_BASE].y;
		if (x + y == 0 && g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX)
			continue;

		selector = g_hudElementLayouts[layoutIndex + LASER_CHARGE_ELEMENT_BASE].selector;
		charge = craft->weaponSlots[laserSlot].laserCharge;
		if (g_players[g_localPlayer].viewState.hudStateLive == HUD_VIEW_FORWARD &&
			(craft->damageStats.activeHudFeatureMask & 2) != 0 &&
			(craft->damageStats.activeHudFeatureMask & 4) != 0) {
			uint16_t chargedSegmentCount;
			int16_t emptySegmentState;

			if (charge > 0 && (craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_CANNONS) != 0) {
				++charge;
				if (charge <= LASER_CHARGE_HALF) {
					emptySegmentState = 0;
					chargedSegmentState = 1;
				} else {
					emptySegmentState = 1;
					chargedSegmentState = 2;
					charge -= LASER_CHARGE_HALF;
				}
				chargedSegmentCount = (uint16_t)charge / LASER_CHARGE_SCALE;
				if (chargedSegmentCount >= LASER_CHARGE_SEGMENT_COUNT + 1)
					chargedSegmentCount = LASER_CHARGE_SEGMENT_COUNT;
			} else {
				chargedSegmentCount = 0;
				emptySegmentState = 0;
				chargedSegmentState = 0;
			}

			if ((uint16_t)g_hudElementStateCache[layoutIndex + LASER_CHARGE_ELEMENT_BASE] !=
				chargedSegmentCount) {
				int16_t xStep;
				uint16_t reverseDirection;

				g_hudElementStateCache[layoutIndex + LASER_CHARGE_ELEMENT_BASE] = chargedSegmentCount;
				xStep = 3;
				if (g_flightResolutionMode != FLIGHT_RESOLUTION_320X240) {
					if (g_flightResolutionMode == FLIGHT_RESOLUTION_480X360)
						xStep = 4;
					else
						xStep = 6;
				}
				reverseDirection = 0;
				if (g_hudElementLayouts[layoutIndex + LASER_CHARGE_ELEMENT_BASE].colorIndex != 0) {
					xStep = -xStep;
					reverseDirection = 1;
				}

				if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
					if (selector != 0) {
						uint16_t chargePercent =
							MATH2_divide((uint16_t)charge, LASER_CHARGE_DENOMINATOR) / LASER_PERCENT_SCALE;

						FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
												 g_screenWidth * g_flight16bppBytesPerPixel);
						FlightText_SetFontTier(2);
						FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
						FlightText_SetCursor(x, y);
						FlightText_SetBackgroundColor(0);
						FlightText_SetColor(0x4A);
#ifdef XVT_MODERN
						XvtCockpitReadouts_RecordNumber(
							(XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LASER_FIRST + laserSlot), chargePercent,
							selector, 1);
#endif
						FlightText_DrawDecimalNumber(chargePercent, selector, 1);
						FlightSw_SetRenderTarget(NULL, DEFAULT_HUD_WIDTH, DEFAULT_HUD_HEIGHT, 0);
					}
				} else {
					uint16_t segment;

					for (segment = 0; segment < LASER_CHARGE_SEGMENT_COUNT; ++segment) {
						uint16_t spriteState =
							segment < chargedSegmentCount ? chargedSegmentState : emptySegmentState;

						g_flightBlitSpriteFn(g_hudPanelSpriteDataByIndex[selector + spriteState],
											 x + xStep * segment, y, 253, reverseDirection);
					}
				}
			}
		}

		lockState = 0;
		if (charge > 0 && (craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_CANNONS) != 0) {
			if (g_players[g_localPlayer].selectedWeaponMode == 0 &&
				g_players[g_localPlayer].selectedWarhead == laserBank) {
				switch (craft->laserState.linkMode[laserBank]) {
					case 0:
					case 4:
						readyState = 0;
						break;
					case 1:
						readyState = craft->laserState.nextSlot[laserBank] == laserSlot ? 3 : 1;
						break;
					case 2:
						if (craft->laserState.nextSlot[laserBank] == laserSlot ||
							(laserSlotCount >= 4 &&
							 (int)craft->laserState.nextSlot[laserBank] - laserSlot == -2)) {
							readyState = 3;
						} else {
							readyState = 1;
						}
						break;
					case 3:
						readyState = 3;
						break;
					default:
						readyState = 0;
						break;
				}
				lockState = readyState;
				if (readyState == 3 && craft->laserState.fireCooldownTicks[laserBank] != 0) {
					readyState = 5;
					lockState = 2;
				}
			} else {
				readyState = 1;
			}
		} else {
			readyState = 0;
		}

		if (chargedSegmentState == 2)
			++readyState;

		objectIndex = g_players[g_localPlayer].objectIndex;
		supportedCraft =
			objectIndex != -1 &&
			(g_objectTable[objectIndex].objectType == 1 || g_objectTable[objectIndex].objectType == 2 ||
			 g_objectTable[objectIndex].objectType == 3 || g_objectTable[objectIndex].objectType == 14 ||
			 g_objectTable[objectIndex].objectType == 4);
		if (supportedCraft && g_players[g_localPlayer].viewState.hudStateLive == HUD_VIEW_FORWARD &&
			(craft->damageStats.activeHudFeatureMask & 2) != 0 &&
			(craft->damageStats.activeHudFeatureMask & 4) != 0 &&
			(uint16_t)g_hudElementLayouts[g_hudInstrumentSetBaseIndex + laserSlot +
										  LASER_SELECTION_ELEMENT_BASE]
						.x +
					(uint16_t)g_hudElementLayouts[g_hudInstrumentSetBaseIndex + laserSlot +
												  LASER_SELECTION_ELEMENT_BASE]
						.y !=
				0) {
			Hud_DrawCachedSpriteElement(
				g_hudInstrumentSetBaseIndex + laserSlot + LASER_SELECTION_ELEMENT_BASE, readyState);
		}

		Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + laserSlot + LASER_READY_ELEMENT_BASE,
									lockState);
		if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_TARGETING_COMPUTER) != 0 && lockState == 3) {
			uint16_t targetObjectIndex = g_players[g_localPlayer].currentTargetObjectIdx;

			if (targetObjectIndex != UINT16_MAX &&
				(uint16_t)collide_targetinrange(g_players[g_localPlayer].objectIndex, targetObjectIndex,
												laserSlot) != 0) {
				g_targetLockActive = 1;
				lockState = 2;
			} else {
				lockState = 1;
			}
		} else if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_TARGETING_COMPUTER) == 0) {
			lockState = 0;
		} else if (lockState == 2) {
			lockState = 1;
		}

		if (lockState == 3)
			lockState = 0;
#ifdef XVT_MODERN
		XvtCockpitInstruments_RecordLaserLock(laserSlot, lockState);
#endif

		objectIndex = g_players[g_localPlayer].objectIndex;
		supportedCraft =
			objectIndex != -1 &&
			(g_objectTable[objectIndex].objectType == 1 || g_objectTable[objectIndex].objectType == 2 ||
			 g_objectTable[objectIndex].objectType == 3 || g_objectTable[objectIndex].objectType == 14 ||
			 g_objectTable[objectIndex].objectType == 4);
		if (supportedCraft)
			Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + laserSlot + LASER_LOCK_ELEMENT_BASE,
										lockState);
	}
}

// FUNCTION: XVT 0x43D010
void Hud_UpdateWarheadCnt(void) {
	ObjectRecord* object;
	int16_t firstLauncherSlotCount;

	object = &g_objectTable[g_players[g_localPlayer].objectIndex];
	if ((object->mobj->pCraft->damageStats.activeHudFeatureMask & 8) != 0) {
		firstLauncherSlotCount =
			g_modelDefs[GetModelIndexFromType(object->objectType)].warheadLauncherSlotCount[0];
		if ((uint16_t)(firstLauncherSlotCount +
					   g_modelDefs[GetModelIndexFromType(
									   g_objectTable[g_players[g_localPlayer].objectIndex].objectType)]
						   .warheadLauncherSlotCount[1]) != 0) {
			Hud_OutputWarheadCount(
				g_modelDefs[GetModelIndexFromType(
								g_objectTable[g_players[g_localPlayer].objectIndex].objectType)]
					.warheadLauncherFirstSlot[0],
				0, 0);
			Hud_OutputWarheadCount(
				g_modelDefs[GetModelIndexFromType(
								g_objectTable[g_players[g_localPlayer].objectIndex].objectType)]
					.warheadLauncherLastSlot[0],
				1, 0);

			if (GetModelIndexFromType(CRAFT_SPECIES_MISSILE_BOAT) ==
				GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType)) {
				Hud_OutputWarheadCount(
					g_modelDefs[GetModelIndexFromType(
									g_objectTable[g_players[g_localPlayer].objectIndex].objectType)]
						.warheadLauncherFirstSlot[1],
					2, 1);
				Hud_OutputWarheadCount(
					g_modelDefs[GetModelIndexFromType(
									g_objectTable[g_players[g_localPlayer].objectIndex].objectType)]
						.warheadLauncherLastSlot[1],
					3, 1);
			}
		}
	}
}

// FUNCTION: XVT 0x43D2B0
void Hud_OutputWarheadCount(uint16_t warheadSlotIdx, uint16_t displaySlot, uint16_t warheadBank) {
	CraftData* craft;
	uint16_t warheadCount;
	uint16_t selectionState;
	uint8_t launcherFlags;
	ModelIndex craftModelIndex;
	int objectIndex;
	int usesCompactPowerDisplay;
	int localPlayer;
	MobileObject** playerMobileObject;

	if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
								 g_screenWidth * g_flight16bppBytesPerPixel);
		localPlayer = g_localPlayer;
		playerMobileObject = &g_objectTable[g_players[localPlayer].objectIndex].mobj;
		craft = (*playerMobileObject)->pCraft;
		if (craft->warheadLauncherCount == 0) {
			warheadCount = 0;
		} else {
			warheadCount = craft->weaponSlots[warheadSlotIdx].count;
		}
		g_hudElementStateCache[displaySlot + 27 + g_hudInstrumentSetBaseIndex] = (int16_t)warheadCount;

		if (warheadCount != 0) {
			craft = (*playerMobileObject)->pCraft;
			if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_WARHEAD_LAUNCHER) == 0) {
				selectionState = 0;
			} else if (g_players[localPlayer].selectedWeaponMode == 0) {
				selectionState = 1;
			} else {
				if (warheadBank != g_players[localPlayer].selectedWarhead) {
					selectionState = 1;
				} else {
					launcherFlags =
						(uint8_t)craft->warheadLauncherFlags[g_players[localPlayer].selectedWarhead];
					if ((launcherFlags & 0x7F) == 3) {
						selectionState = 2;
					} else {
						selectionState = ((displaySlot & 1) == (launcherFlags >> 7)) + 1;
					}
				}
			}
		} else {
			selectionState = 0;
		}

		if (warheadCount != 0) {
			FlightText_SetFontTier(2);
			FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
			FlightText_SetCursor(displaySlot * (g_flightFontHalfHeight + 1) + 2, 2);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			if (selectionState == 2) {
				FlightText_SetColor(0x52);
			} else {
				FlightText_SetColor(0x4A);
			}
			g_flightTextShadowEnabled = 0;
			if (g_hudElementLayouts[displaySlot + 27 + g_hudInstrumentSetBaseIndex].selector != 0) {
				FlightText_SetFontTier(0);
				craftModelIndex =
					GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
				if (GetModelIndexFromType(CRAFT_SPECIES_MISSILE_BOAT) == craftModelIndex) {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordNumber(
						(XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + displaySlot), warheadCount,
						2, 1);
#endif
					FlightText_DrawDecimalNumber(warheadCount, 2, 1);
				} else {
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordNumber(
						(XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + displaySlot), warheadCount,
						1, 1);
#endif
					FlightText_DrawDecimalNumber(warheadCount, 1, 1);
				}
			}
		} else {
			FlightText_SetFontTier(2);
			FlightText_SetClipRect(displaySlot * (g_flightFontHalfHeight + 1) + 2, 2,
								   g_flightFontHalfHeight + displaySlot * (g_flightFontHalfHeight + 1) + 3,
								   g_flightFontLineHeight + 3);
#ifdef XVT_MODERN
			XvtCockpitReadouts_ClearLauncher(displaySlot);
#endif
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			g_flightFillClipRectFn();
		}

		FlightSw_SetRenderTarget(NULL, 320, 200, 0);
		return;
	}

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	if (craft->warheadLauncherCount == 0) {
		warheadCount = 0;
	} else {
		warheadCount = craft->weaponSlots[warheadSlotIdx].count;
	}
	if (g_hudElementStateCache[displaySlot + 27 + g_hudInstrumentSetBaseIndex] != (int16_t)warheadCount) {
		g_hudElementStateCache[displaySlot + 27 + g_hudInstrumentSetBaseIndex] = (int16_t)warheadCount;
		FlightText_SetFontTier(2);
		FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
		FlightText_SetCursor(g_hudElementLayouts[displaySlot + 27 + g_hudInstrumentSetBaseIndex].x,
							 g_hudElementLayouts[displaySlot + 27 + g_hudInstrumentSetBaseIndex].y);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor(0x4A);
		g_flightTextShadowEnabled = 0;
		craftModelIndex =
			GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
		if (GetModelIndexFromType(CRAFT_SPECIES_MISSILE_BOAT) == craftModelIndex) {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(
				(XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + displaySlot), warheadCount, 2, 1);
#endif
			FlightText_DrawDecimalNumber(warheadCount, 2, 1);
		} else {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(
				(XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + displaySlot), warheadCount, 1, 1);
#endif
			FlightText_DrawDecimalNumber(warheadCount, 1, 1);
		}
	}

	if (warheadCount != 0) {
		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_WARHEAD_LAUNCHER) == 0) {
			selectionState = 0;
		} else if (g_players[g_localPlayer].selectedWeaponMode == 0) {
			selectionState = 1;
		} else {
			if (warheadBank != g_players[g_localPlayer].selectedWarhead) {
				selectionState = 1;
			} else {
				launcherFlags =
					(uint8_t)craft->warheadLauncherFlags[g_players[g_localPlayer].selectedWarhead];
				if ((launcherFlags & 0x7F) == 3) {
					selectionState = 2;
				} else {
					selectionState = ((displaySlot & 1) == (launcherFlags >> 7)) + 1;
				}
			}
		}
	} else {
		selectionState = 0;
	}

	objectIndex = g_players[g_localPlayer].objectIndex;
	usesCompactPowerDisplay =
		objectIndex != -1 &&
		(g_objectTable[objectIndex].objectType == 1 || g_objectTable[objectIndex].objectType == 2 ||
		 g_objectTable[objectIndex].objectType == 3 || g_objectTable[objectIndex].objectType == 14 ||
		 g_objectTable[objectIndex].objectType == 4);
	if (usesCompactPowerDisplay && selectionState == 2) {
		selectionState = 4;
	}
	Hud_DrawCachedSpriteElement(displaySlot + 19, selectionState);
}

// FUNCTION: XVT 0x43D800
void Hud_DrawShieldStrength2D(void) {
	CraftData* craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	int shield;
	int maxShield;

	if ((craft->damageStats.activeHudFeatureMask & 0x20u) == 0)
		return;

	shield = craft->shieldEnergy[0];
	if (shield < 0)
		shield = 0;
	if (!(craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_SHIELDS))
		shield = 0;
	maxShield = Craft_GetObjectMaxShield(g_players[g_localPlayer].objectIndex) / 2;
	if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].colorIndex != 0xFFFFu) {
		uint16_t percentage;
		uint16_t strengthLevel;
		uint16_t secondaryLevel;
		int16_t primaryFade;
		int16_t secondaryFade;
		if (maxShield <= shield) {
			strengthLevel = 9;
			percentage = MATH2_percentage((unsigned int)(shield - maxShield), (unsigned int)maxShield);
			secondaryLevel = MATH2_longfraction(9, (uint16_t)percentage);
		} else {
			percentage = MATH2_percentage((unsigned int)shield, (unsigned int)maxShield);
			strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
			secondaryLevel = 0;
		}
		if (g_playerFlightTransientTimers[g_localPlayer].shieldHitFlashTimer != 0 &&
			g_lastShieldDamageSide == 0) {
			if (secondaryLevel == 0)
				strengthLevel = 10;
			else
				secondaryLevel = 10;
		}
		primaryFade =
			strengthLevel != 0 ? g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].clipWidth : -1;
		secondaryFade =
			secondaryLevel != 0 ? g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 36].clipWidth : -1;
		Hud_DrawCachedFadedSpriteElement((uint16_t)(g_hudInstrumentSetBaseIndex + 35),
										 (int16_t)g_hudShieldColors[strengthLevel], primaryFade);
		Hud_DrawCachedFadedSpriteElement((uint16_t)(g_hudInstrumentSetBaseIndex + 36),
										 (int16_t)g_hudShieldColors[secondaryLevel], secondaryFade);
	} else {
		uint16_t strengthLevel;
		uint16_t percentage;
		uint16_t secondaryLevel;
		if (maxShield <= shield) {
			percentage = MATH2_percentage((unsigned int)(shield - maxShield), (unsigned int)maxShield);
			strengthLevel = 9;
			secondaryLevel = (uint16_t)(percentage / 0x28Fu + 100);
		} else {
			percentage = MATH2_percentage((unsigned int)shield, (unsigned int)maxShield);
			strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
			secondaryLevel = (uint16_t)(percentage / 0x28Fu);
		}
		if ((uint16_t)g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 35] != secondaryLevel) {
			FlightText_SetFontTier(2);
			FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].x,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].y,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].x +
									   FlightText_MeasureStringWidth("123%"),
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].y +
									   g_flightFontLineHeight);
			FlightText_SetBackgroundColor(0x40);
			g_flightFillClipRectFn();
			FlightText_SetColor(g_hudShieldColors[HUD_SHIELD_TEXT_COLOR_OFFSET + strengthLevel]);
			FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].x,
								 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 35].y);
			FlightText_FormatScratchInt(secondaryLevel);
			FlightText_AppendScratchChar('%');
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_SHIELD_FORE, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_RIGHT);
#endif
			FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
			g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 35] = (int16_t)secondaryLevel;
		}
	}
	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	shield = craft->shieldEnergy[1];
	if (shield < 0)
		shield = 0;
	if (!(craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_SHIELDS))
		shield = 0;
	maxShield = Craft_GetObjectMaxShield(g_players[g_localPlayer].objectIndex) / 2;
	if (g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].colorIndex != 0xFFFFu) {
		uint16_t percentage;
		uint16_t strengthLevel;
		uint16_t secondaryLevel;
		int16_t primaryFade;
		int16_t secondaryFade;
		if (maxShield <= shield) {
			strengthLevel = 9;
			percentage = MATH2_percentage((unsigned int)(shield - maxShield), (unsigned int)maxShield);
			secondaryLevel = MATH2_longfraction(9, (uint16_t)percentage);
		} else {
			percentage = MATH2_percentage((unsigned int)shield, (unsigned int)maxShield);
			strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
			secondaryLevel = 0;
		}
		if (g_playerFlightTransientTimers[g_localPlayer].shieldHitFlashTimer != 0 &&
			g_lastShieldDamageSide == 1) {
			if (secondaryLevel == 0)
				strengthLevel = 10;
			else
				secondaryLevel = 10;
		}
		primaryFade =
			strengthLevel != 0 ? g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].clipWidth : -1;
		secondaryFade =
			secondaryLevel != 0 ? g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 38].clipWidth : -1;
		Hud_DrawCachedFadedSpriteElement((uint16_t)(g_hudInstrumentSetBaseIndex + 37),
										 (int16_t)g_hudShieldColors[strengthLevel], primaryFade);
		Hud_DrawCachedFadedSpriteElement((uint16_t)(g_hudInstrumentSetBaseIndex + 38),
										 (int16_t)g_hudShieldColors[secondaryLevel], secondaryFade);
	} else {
		uint16_t secondaryLevel;
		uint16_t strengthLevel;
		uint16_t percentage;
		if (maxShield <= shield) {
			percentage = MATH2_percentage((unsigned int)(shield - maxShield), (unsigned int)maxShield);
			strengthLevel = 9;
			secondaryLevel = (uint16_t)(percentage / 0x28Fu + 100);
		} else {
			percentage = MATH2_percentage((unsigned int)shield, (unsigned int)maxShield);
			strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
			secondaryLevel = (uint16_t)(percentage / 0x28Fu);
		}
		if ((uint16_t)g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 37] != secondaryLevel) {
			FlightText_SetFontTier(2);
			FlightText_SetClipRect(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].x,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].y,
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].x +
									   FlightText_MeasureStringWidth("123%"),
								   g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].y +
									   g_flightFontLineHeight);
			FlightText_SetBackgroundColor(0x40);
			g_flightFillClipRectFn();
			FlightText_SetColor(g_hudShieldColors[HUD_SHIELD_TEXT_COLOR_OFFSET + strengthLevel]);
			FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].x,
								 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 37].y);
			FlightText_FormatScratchInt(secondaryLevel);
			FlightText_AppendScratchChar('%');
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_SHIELD_AFT, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_RIGHT);
#endif
			FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
			g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 37] = (int16_t)secondaryLevel;
		}
	}

	{
		uint16_t hullState;
		unsigned int hullThird;
		if (g_playerFlightTransientTimers[g_localPlayer].hullHitFlashTimer != 0) {
			hullState = 3;
		} else {
			craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
			hullThird = craft->hullMax / 3;
			if (!hullThird) {
				hullState = 2;
			} else {
				hullState = craft->hullDamage / hullThird;
				if ((uint16_t)hullState > 2)
					hullState = 2;
				hullState = 2 - hullState;
			}
		}
		Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 39, hullState);
	}
}

#if 0
static void Hud_DrawShieldStrength2D_legacy(void) {
	unsigned int objectIndex = g_players[g_localPlayer].objectIndex;
	CraftData* craft = g_objectTable[objectIndex].mobj->pCraft;
	unsigned int maxShield = Craft_GetObjectMaxShield(objectIndex) / 2;
	int shieldValues[2] = { craft->shieldEnergy[0], craft->shieldEnergy[1] };
	unsigned int side;

	if ((craft->damageStats.activeHudFeatureMask & 0x20u) == 0)
		return;

	for (side = 0; side < 2; ++side) {
		unsigned int elementIndex = g_hudInstrumentSetBaseIndex + 35 + side * 2;
		int shield = shieldValues[side];
		unsigned int percentage;
		unsigned int strengthLevel;
		unsigned int secondaryLevel;
		int16_t primaryFade;
		int16_t secondaryFade;
		if (shield < 0)
			shield = 0;
		if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_SHIELDS) == 0)
			shield = 0;

		if (g_hudElementLayouts[elementIndex].colorIndex == 0xFFFFu) {
			if ((unsigned int)shield < maxShield) {
				percentage = MATH2_percentage((unsigned int)shield, maxShield);
				strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
				secondaryLevel = percentage / 0x28Fu;
			} else {
				percentage = MATH2_percentage((unsigned int)shield - maxShield, maxShield);
				strengthLevel = 9;
				secondaryLevel = percentage / 0x28Fu + 100;
			}
			if ((uint16_t)g_hudElementStateCache[elementIndex] != secondaryLevel) {
				RECT clipRect;
				FlightText_SetFontTier(2);
				clipRect.left = g_hudElementLayouts[elementIndex].x;
				clipRect.top = g_hudElementLayouts[elementIndex].y;
				clipRect.right = clipRect.left + FlightText_MeasureStringWidth("123%");
				clipRect.bottom = clipRect.top + g_flightFontLineHeight;
				FlightText_SetClipRect(clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
				FlightText_SetBackgroundColor(0x40);
				g_flightFillClipRectFn();
				FlightText_SetColor(g_hudShieldColors[HUD_SHIELD_TEXT_COLOR_OFFSET + strengthLevel]);
				FlightText_SetCursor(g_hudElementLayouts[elementIndex].x,
									 g_hudElementLayouts[elementIndex].y);
				FlightText_FormatScratchInt(secondaryLevel);
				FlightText_AppendScratchChar('%');
				FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
				g_hudElementStateCache[elementIndex] = (int16_t)secondaryLevel;
			}
		} else {
			if ((unsigned int)shield < maxShield) {
				percentage = MATH2_percentage((unsigned int)shield, maxShield);
				strengthLevel = MATH2_longfraction(9, (uint16_t)percentage);
				secondaryLevel = 0;
			} else {
				strengthLevel = 9;
				percentage = MATH2_percentage((unsigned int)shield - maxShield, maxShield);
				secondaryLevel = MATH2_longfraction(9, (uint16_t)percentage);
			}
			if (g_playerFlightTransientTimers[g_localPlayer].shieldHitFlashTimer != 0 &&
				g_lastShieldDamageSide == (int)side) {
				if (secondaryLevel != 0)
					secondaryLevel = 10;
				else
					strengthLevel = 10;
			}
			primaryFade = strengthLevel != 0 ? g_hudElementLayouts[elementIndex].clipWidth : -1;
			secondaryFade = secondaryLevel != 0 ? g_hudElementLayouts[elementIndex + 1].clipWidth : -1;
			Hud_DrawCachedFadedSpriteElement(
				(uint16_t)elementIndex, g_hudShieldColors[strengthLevel], primaryFade);
			Hud_DrawCachedFadedSpriteElement((uint16_t)(elementIndex + 1),
											 g_hudShieldColors[secondaryLevel],
											 secondaryFade);
		}
	}

	{
		unsigned int hullState;
		if (g_playerFlightTransientTimers[g_localPlayer].hullHitFlashTimer != 0) {
			hullState = 3;
		} else {
			unsigned int hullThird = craft->hullMax / 3;
			if (hullThird != 0) {
				hullState = craft->hullDamage / hullThird;
				if (hullState > 2)
					hullState = 2;
				hullState = 2 - hullState;
			} else {
				hullState = 2;
			}
		}
		Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 39, hullState);
	}
}
#endif

// FUNCTION: XVT 0x43DE10
void Hud_DrawBeamStrength2D(void) {
	uint16_t layoutIndex;
	CraftData* craft;
	int16_t beamStrength;
	int16_t beamSystemAvailable;
	uint16_t beamActiveState;
	int originalBeamStrength;
	int16_t segmentIndex;
	uint16_t fade;
	int16_t clampedStrength;
	uint16_t x;
	uint16_t y;

	layoutIndex = g_hudInstrumentSetBaseIndex + 51;
	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	if ((craft->damageStats.activeHudFeatureMask & 0x10) == 0) {
		return;
	}

	beamStrength = craft->beamPresent;
	if (beamStrength < 0) {
		beamStrength = 0;
	}
	beamSystemAvailable = craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM;
	if (beamSystemAvailable == 0) {
		beamStrength = 0;
	}
	beamActiveState = craft->beamActive != 0;
	if (beamSystemAvailable == 0) {
		beamActiveState = 0;
	}
	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 116, beamActiveState);

	originalBeamStrength = beamStrength;
	if ((uint16_t)g_hudElementStateCache[51] == beamStrength) {
		return;
	}
	g_hudElementStateCache[51] = beamStrength;

	segmentIndex = 0;
	do {
		if (200 * (5 * segmentIndex + 5) < originalBeamStrength) {
			fade = g_hudBeamSegmentFadeByChargeStep[3];
		} else {
			clampedStrength = beamStrength;
			if (beamStrength < 0) {
				fade = g_hudBeamSegmentFadeByChargeStep[0];
			} else {
				if (beamStrength > 1000) {
					clampedStrength = 1000;
				}
				fade = g_hudBeamSegmentFadeByChargeStep[clampedStrength / 333];
			}
		}

		x = g_hudElementLayouts[layoutIndex].x;
		y = g_hudElementLayouts[layoutIndex].y;
		if (g_flightResolutionMode == FLIGHT_RESOLUTION_640X480) {
			x += 3 * (8 - segmentIndex);
			y += 3 * (8 - segmentIndex);
		} else if (g_flightResolutionMode == FLIGHT_RESOLUTION_480X360) {
			x += g_hudBeamSegmentOffsets480x360[segmentIndex].x;
			y += g_hudBeamSegmentOffsets480x360[segmentIndex].y;
		} else {
			x += g_hudBeamSegmentOffsets320x240[segmentIndex].x;
			y += g_hudBeamSegmentOffsets320x240[segmentIndex].y;
		}

		beamStrength -= 1000;
		g_flightBlitSpriteFadedFn(
			g_hudPanelSpriteDataByIndex[g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 51].selector +
										segmentIndex],
			x, y, g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 51].colorIndex, (int8_t)fade,
			g_hudBeamSegmentFadeByChargeStep[0] == fade
				? 0
				: g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 51].clipWidth);
		++segmentIndex;
	} while (segmentIndex < 9);
}

// FUNCTION: XVT 0x43E050
void Hud_UpdateSpeedPercent(void) {
	int16_t speedPercent;

	if ((g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->damageStats.activeHudFeatureMask &
		 0x40) != 0) {
		FlightText_SetBackgroundColor(0x40);
		speedPercent =
			MATH2_fraction(g_objectTable[g_players[g_localPlayer].objectIndex].mobj->speed, 0x71C7);
		if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX)
			FlightText_SetFontTier(0);
		else
			FlightText_SetFontTier(2);
		Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + 40, speedPercent, 1);
	}
}

// FUNCTION: XVT 0x43E110
void Hud_UpdateThrottlePercent(void) {
	MobileObject* mobileObject;
	CraftData* craft;
	int16_t throttlePercent;

	if ((g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->damageStats.activeHudFeatureMask &
		 0x40) != 0) {
		FlightText_SetBackgroundColor(0x40);
		mobileObject = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
		craft = mobileObject->pCraft;
		throttlePercent = craft->throttleSpeed / 0x28F;
		if (craft->engineOutputScale == 0)
			throttlePercent *= 2;
		if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX)
			FlightText_SetFontTier(0);
		else
			FlightText_SetFontTier(2);
		Hud_DrawCachedNumericElement(g_hudInstrumentSetBaseIndex + 41, throttlePercent, 1);
	}
}

// FUNCTION: XVT 0x43E1F0
void Hud_UpdateMissionClockDisplay(void) {
	uint8_t seconds;
	int16_t minuteSeconds;
	int16_t totalSeconds;

	if (g_flightMissionState.provingGroundsModeActive != 0 ||
		g_flightMissionState.missionTimeLimitMinutes != 0) {
		minuteSeconds = 60 * g_missionCountdownClock.minutes;
		seconds = g_missionCountdownClock.seconds;
	} else {
		minuteSeconds = 60 * g_missionElapsedClock.minutes;
		seconds = g_missionElapsedClock.seconds;
	}
	totalSeconds = minuteSeconds + seconds;
	if (g_hudElementStateCache[46] != totalSeconds) {
		g_hudElementStateCache[46] = totalSeconds;
		FlightText_SetFontTier(2);
		FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
		FlightText_SetBackgroundColor(0x40);
		if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
			FlightText_SetColor(0x4D);
		else
			FlightText_SetColor(0x4E);
		g_flightTextShadowEnabled = 0;
		FlightText_SetCursor(g_hudElementLayouts[46].x, g_hudElementLayouts[46].y);

		if (g_flightMissionState.provingGroundsModeActive != 0 ||
			g_flightMissionState.missionTimeLimitMinutes != 0) {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_CLOCK_MINUTES, g_missionCountdownClock.minutes,
											2, 2);
#endif
			FlightText_DrawDecimalNumber(g_missionCountdownClock.minutes, 2, 2);
		} else {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_CLOCK_MINUTES, g_missionElapsedClock.minutes,
											2, 1);
#endif
			FlightText_DrawDecimalNumber(g_missionElapsedClock.minutes, 2, 1);
		}

		if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
			FlightText_SetCursor(g_hudElementLayouts[46].x +
									 FlightText_MeasureStringWidth(g_missionClockMinutesWidthText),
								 g_hudElementLayouts[46].y);
		else
			FlightText_SetCursor(g_hudElementLayouts[46].x +
									 FlightText_MeasureStringWidth(g_missionClockMinutesWidthText) + 1,
								 g_hudElementLayouts[46].y);

		if (g_flightMissionState.provingGroundsModeActive != 0 ||
			g_flightMissionState.missionTimeLimitMinutes != 0) {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_CLOCK_SECONDS, g_missionCountdownClock.seconds,
											2, 2);
#endif
			FlightText_DrawDecimalNumber(g_missionCountdownClock.seconds, 2, 2);
		} else {
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_CLOCK_SECONDS, g_missionElapsedClock.seconds,
											2, 2);
#endif
			FlightText_DrawDecimalNumber(g_missionElapsedClock.seconds, 2, 2);
		}
	}
}

// FUNCTION: XVT 0x43E390
void Hud_DrawPowerSettings2D(void) {
	int objectIndex;
	ObjectRecord* object;
	CraftData* craft;
	int usesCompactPowerDisplay;
	int16_t yStep;
	uint16_t activeHudFeatureMask;
	uint16_t segmentCount;
	uint16_t laserPower;
	uint16_t shieldPower;
	uint16_t enginePower;
	uint16_t systemFlags;

	objectIndex = g_players[g_localPlayer].objectIndex;
	object = &g_objectTable[objectIndex];
	craft = object->mobj->pCraft;
	usesCompactPowerDisplay = 0;
	if (objectIndex != -1) {
		if (object->objectType == 1 || object->objectType == 2 || object->objectType == 3 ||
			object->objectType == 14 || object->objectType == 4) {
			usesCompactPowerDisplay = 1;
		}
	}

	if (!usesCompactPowerDisplay) {
		yStep = 2;
		if (g_flightResolutionMode != FLIGHT_RESOLUTION_320X240)
			yStep = g_flightResolutionMode == FLIGHT_RESOLUTION_480X360 ? 4 : 6;
		if ((craft->damageStats.activeHudFeatureMask & 0x200) != 0) {
			Hud_DrawCachedSegmentedBar(3 * (uint8_t)craft->laserRedirect, g_hudInstrumentSetBaseIndex + 43,
									   12, yStep);
		}

		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->damageStats.activeHudFeatureMask & 0x800) != 0 &&
			(craft->systemFlags & CRAFT_SUBSYSTEM_FLAG_SHIELDS) != 0) {
			Hud_DrawCachedSegmentedBar(3 * (uint8_t)craft->shieldRedirect, g_hudInstrumentSetBaseIndex + 44,
									   12, yStep);
		}

		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->damageStats.activeHudFeatureMask & 0x1000) != 0 &&
			(craft->systemFlags & CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM) != 0) {
			Hud_DrawCachedSegmentedBar(3 * (uint8_t)craft->beamLevel, g_hudInstrumentSetBaseIndex + 45, 12,
									   yStep);
		}

		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->damageStats.activeHudFeatureMask & 0x400) != 0) {
			enginePower = (uint16_t)(8 - (uint8_t)craft->laserRedirect);
			systemFlags = craft->systemFlags;
			if ((systemFlags & CRAFT_SUBSYSTEM_FLAG_SHIELDS) != 0)
				enginePower = (uint16_t)(enginePower - (uint8_t)craft->shieldRedirect + 2);
			if ((systemFlags & CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM) != 0)
				enginePower = (uint16_t)(enginePower - (uint8_t)craft->beamLevel + 2);
			Hud_DrawCachedSegmentedBar(enginePower, g_hudInstrumentSetBaseIndex + 42, 12, yStep);
		}
	} else {
		activeHudFeatureMask = craft->damageStats.activeHudFeatureMask;
		if ((activeHudFeatureMask & 0xE00) == 0)
			return;

		segmentCount = 0;
		yStep = 0;
		switch (g_flightResolutionMode) {
			case FLIGHT_RESOLUTION_320X240:
				yStep = 2;
				segmentCount = 4;
				break;
			case FLIGHT_RESOLUTION_640X480:
				yStep = 5;
				segmentCount = 4;
				break;
			case FLIGHT_RESOLUTION_480X360:
				yStep = 3;
				segmentCount = 4;
				break;
			default:
				break;
		}

		laserPower = (uint8_t)craft->laserRedirect;
		shieldPower = (uint8_t)craft->shieldRedirect;
		enginePower = (uint16_t)(8 - shieldPower - laserPower);
		if (g_hudInstrumentSetBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
			segmentCount *= 2;
			laserPower *= 2;
			shieldPower *= 2;
			enginePower *= 2;
		}
		if ((activeHudFeatureMask & 0x400) != 0) {
			Hud_DrawCachedSegmentedBar(enginePower, g_hudInstrumentSetBaseIndex + 42, 2 * segmentCount,
									   yStep);
		}
		if ((activeHudFeatureMask & 0x800) != 0) {
			Hud_DrawCachedSegmentedBar(shieldPower, g_hudInstrumentSetBaseIndex + 44, segmentCount,
									   2 * yStep);
		}
		if ((activeHudFeatureMask & 0x200) != 0) {
			Hud_DrawCachedSegmentedBar(laserPower, g_hudInstrumentSetBaseIndex + 43, segmentCount, 2 * yStep);
		}
	}
}

// FUNCTION: XVT 0x43E6F0
void Hud_DrawCachedSegmentedBar(uint16_t filledCount, uint16_t elementIdx, uint16_t segmentCount,
								int16_t yStep) {
	uint16_t segmentIndex;
	uint16_t x;
	uint16_t y;
	uint16_t selector;
	uint16_t spriteOffset;

	if ((uint16_t)g_hudElementStateCache[elementIdx] == filledCount)
		return;

	segmentIndex = 0;
	g_hudElementStateCache[elementIdx] = (int16_t)filledCount;
	x = g_hudElementLayouts[elementIdx].x;
	y = g_hudElementLayouts[elementIdx].y;
	selector = g_hudElementLayouts[elementIdx].selector;
	if (segmentCount == segmentIndex)
		return;

	do {
		spriteOffset = segmentIndex < filledCount;
		g_flightBlitSpriteFn(g_hudPanelSpriteDataByIndex[selector + spriteOffset], x, y, 253, 0);
		y = (uint16_t)(y - yStep);
		++segmentIndex;
	} while (segmentIndex < segmentCount);
}

// FUNCTION: XVT 0x43E790
void Hud_UpdateThreatIndicators(int hudMode) {
	int objectIdx;
	uint16_t beamThreat;
	uint16_t attackThreat;
	uint16_t laserThreat;
	int playerObjectIdx;
	int16_t maxWarheadLock;
	uint16_t warningState;

	(void)hudMode;

	attackThreat = 0;
	laserThreat = 0;
	beamThreat = 0;
	playerObjectIdx = g_players[g_localPlayer].objectIndex;
	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		CraftData* craft;

		if (g_objectTable[objectIdx].objectType == 0 || g_objectTable[objectIdx].mobj->state != 0)
			continue;
		craft = g_objectTable[objectIdx].mobj->pCraft;
		if (craft->workingSubsystems == 0 || craft->objectKind != CRAFT_OBJECT_KIND_ACTIVE)
			continue;
		if (g_objectTable[objectIdx].playerOwnerIdx == -1) {
			AiController* ai = &craft->aiController;

			if (ai->targetObjIdx == playerObjectIdx && (ai->maneuverMode == AI_MANEUVER_MODE_ATTACK ||
														ai->maneuverMode == AI_MANEUVER_MODE_ROCKET_ATTACK)) {
				int cannon;

				for (cannon = 0; cannon < craft->cannonClassCount; ++cannon) {
					if ((craft->laserState.projectileTypeId[cannon] == 0x8B ||
						 craft->laserState.projectileTypeId[cannon] == 0x89) &&
						craft->laserState.linkMode[cannon] != 0)
						attackThreat = 1;
				}
				if (craft->beamActive != 0 && craft->beamPresent != 0 &&
					craft->beamTypeId != BEAM_TYPE_NONE && (uint8_t)craft->beamTypeId < BEAM_TYPE_DECOY)
					beamThreat = (uint8_t)craft->beamTypeId;
			}
		} else {
			CraftData* playerCraft = g_objectTable[playerObjectIdx].mobj->pCraft;
			int playerOwnerIdx;

			if (playerCraft->lastAttackerObjIdx == objectIdx &&
				(uint16_t)Mission_GameTimeToSeconds(g_missionElapsedClock.hours,
													g_missionElapsedClock.minutes,
													g_missionElapsedClock.seconds) -
						playerCraft->lastHitTimestamp <
					5)
				attackThreat = 1;
			playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;
			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == playerObjectIdx &&
				g_players[playerOwnerIdx].selectedWeaponMode == 0) {
				pai_ObjectRefUpdateApproxRangeScore(objectIdx, playerObjectIdx);
				if (g_targetRangeScore < 0x10000 &&
					Targeting_ScoreCandidate(playerObjectIdx, 0, playerOwnerIdx))
					attackThreat = 1;
			}
			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == playerObjectIdx &&
				craft->beamActive != 0 && craft->beamPresent != 0 && craft->beamTypeId != BEAM_TYPE_NONE &&
				(uint8_t)craft->beamTypeId < BEAM_TYPE_DECOY)
				beamThreat = (uint8_t)craft->beamTypeId;
		}
		if (g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.status1 != 5) {
			int slot;

			for (slot = 0; slot < craft->laserSlotCount; ++slot) {
				if (craft->weaponSlots[slot].projectileTypeId == 2 &&
					craft->turretTargetStates[slot].targetObjIdx == playerObjectIdx &&
					craft->componentHp[g_modelDefs[craft->modelIndex].weaponHardpoints[slot].meshIdx] != 0)
					laserThreat = 1;
			}
			if (craft->beamActive != 0 && craft->beamPresent != 0 &&
				(uint16_t)craft->beamTargetObjIdx == playerObjectIdx && craft->beamTypeId != BEAM_TYPE_NONE &&
				(uint8_t)craft->beamTypeId < BEAM_TYPE_DECOY)
				beamThreat = (uint8_t)craft->beamTypeId;
		}
	}
	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 90, attackThreat);
	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 91, laserThreat);
	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 92, beamThreat);
	maxWarheadLock = 0;
	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		CraftData* craft;
		int playerOwnerIdx;

		if (g_objectTable[objectIdx].objectType == 0 || g_objectTable[objectIdx].mobj->state != 0)
			continue;
		craft = g_objectTable[objectIdx].mobj->pCraft;
		if (craft->workingSubsystems == 0 || craft->objectKind != CRAFT_OBJECT_KIND_ACTIVE)
			continue;
		playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;
		if (playerOwnerIdx == -1) {
			AiController* ai = &craft->aiController;

			if (ai->targetObjIdx == playerObjectIdx && ai->maneuverMode == AI_MANEUVER_MODE_ROCKET_ATTACK) {
				int16_t warheadLockTicks = craft->warheadLockTicks;

				if (maxWarheadLock < warheadLockTicks)
					maxWarheadLock = warheadLockTicks;
			}
		} else if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == playerObjectIdx &&
				   g_players[playerOwnerIdx].selectedWeaponMode != 0) {
			int16_t warheadLockTicks = craft->warheadLockTicks;

			if (maxWarheadLock < warheadLockTicks)
				maxWarheadLock = warheadLockTicks;
		}
	}
	if (maxWarheadLock > 944) {
		warningState = 2;
	} else if (maxWarheadLock > 0) {
		warningState = (uint16_t)((g_missionElapsedClock.subsecondTicks / 59) & 1);
	} else {
		warningState = 0;
	}
	Hud_DrawCachedSpriteElement(g_hudInstrumentSetBaseIndex + 93, warningState);
#ifdef XVT_MODERN
	XvtCockpitInstruments_RecordThreats(attackThreat, laserThreat, beamThreat, warningState);
#endif
	FlightSurface_Unlock();
	fsfx_UpdateIncomingMissileWarning(warningState);
	FlightSurface_Lock();
}

// FUNCTION: XVT 0x43EC40
void Hud_UpdateCriticalHullShieldWarning(void) {
	int objectIdx;
	int supportedCraft;
	CraftData* craft;
	unsigned int shieldEnergy;
	unsigned int criticalHullThreshold;
	unsigned int hullDamageLevel;
	uint16_t warningState;
	unsigned int warningTextIdx;

	objectIdx = g_players[g_localPlayer].objectIndex;
	supportedCraft = objectIdx != -1 &&
					 (g_objectTable[objectIdx].objectType == 1 || g_objectTable[objectIdx].objectType == 2 ||
					  g_objectTable[objectIdx].objectType == 3 || g_objectTable[objectIdx].objectType == 14 ||
					  g_objectTable[objectIdx].objectType == 4);
	if (supportedCraft == 0 ||
		(uint16_t)g_hudElementLayouts[50].y + (uint16_t)g_hudElementLayouts[50].x == 0) {
		return;
	}

	craft = g_objectTable[objectIdx].mobj->pCraft;
	shieldEnergy = craft->shieldEnergy[0] + craft->shieldEnergy[1];
	criticalHullThreshold = craft->hullMax / 3;
	hullDamageLevel = 2;
	if (criticalHullThreshold != 0) {
		hullDamageLevel = craft->hullDamage / criticalHullThreshold;
	}
	if (shieldEnergy < 100 && hullDamageLevel == 2) {
		warningState = (g_missionElapsedClock.subsecondTicks / 59) & 1;
		if (warningState != 0) {
			FlightSurface_Unlock();
			fsfx_PlaySound(FLIGHT_SOUND_CRITICAL_WARNING, -1, g_localPlayer);
			FlightSurface_Lock();
			++g_hudElementLayouts[127].clipHeightOrForegroundColor;
			if ((uint16_t)g_hudElementLayouts[127].clipHeightOrForegroundColor >= 0x2F0) {
				g_hudElementLayouts[127].clipHeightOrForegroundColor = 1;
			}
		}
	} else {
		warningState = 0;
	}

	if (g_players[g_localPlayer].viewState.hudStateLive != HUD_VIEW_FORWARD) {
		return;
	}
	Hud_DrawCachedSpriteElement(50, warningState);
	if (g_hudElementLayouts[127].clipHeightOrForegroundColor == 0) {
		return;
	}

	FlightText_SetFontTier(0);
	FlightText_SetClipRect(g_hudElementLayouts[127].x, g_hudElementLayouts[127].y,
						   g_hudElementLayouts[127].x + g_hudElementLayouts[127].clipWidth,
						   g_hudElementLayouts[127].y + g_flightFontLineHeight);
	FlightText_SetCursor(g_hudElementLayouts[127].x, g_hudElementLayouts[127].y);
	FlightText_SetColor(warningState + g_hudElementLayouts[127].colorIndex);
	FlightText_SetBackgroundColor((uint16_t)g_hudElementLayouts[127].selector + (warningState == 0 ? 0 : 2));
	warningTextIdx = (uint16_t)g_hudElementLayouts[127].clipHeightOrForegroundColor;
	if (warningTextIdx > 0x200) {
		warningTextIdx -= 0x200;
		g_flightFillClipRectFn();
		warningTextIdx >>= 4;
	} else {
		warningTextIdx = 0;
	}
#ifdef XVT_MODERN
	XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CRITICAL_WARNING,
							   g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_EJECT + warningTextIdx],
							   XVT_COCKPIT_ALIGN_CENTER);
#endif
	FlightText_DrawStringCentered(g_strCockpitOverlayText[COCKPIT_OVERLAY_STR_EJECT + warningTextIdx]);
}

// FUNCTION: XVT 0x43EE80
void Hud_UpdateCountermeasureStatus(void) {
	uint16_t countermeasureCount;
	uint16_t left;
	uint16_t y;

	countermeasureCount = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->cmAmmoCount;
	if (g_hudInstrumentSetBaseIndex == HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
		if ((uint16_t)g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 48] != countermeasureCount) {
			left = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 48].x;
			y = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 48].y;
			if (left + y == 0)
				return;

			FlightText_SetFontTier(2);
			FlightText_SetClipRect(left, y,
								   left + FlightText_MeasureStringWidth(g_countermeasureAmmoWidthText),
								   y + g_flightFontLineHeight);
			FlightText_SetCursor(left, y);
			FlightText_SetBackgroundColor(0x2C);
			g_flightFillClipRectFn();
			FlightText_SetColor(0x4E);
#ifdef XVT_MODERN
			XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_COUNTERMEASURES, countermeasureCount, 3, 1);
#endif
			FlightText_DrawDecimalNumber(countermeasureCount, 3, 1);
			g_hudElementStateCache[g_hudInstrumentSetBaseIndex + 48] = countermeasureCount;
		}

		if ((uint8_t)g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->chaffActiveTimer != 0)
			Hud_DrawCachedSpriteElement(47, 1);
		else
			Hud_DrawCachedSpriteElement(47, 0);
	}
}

// FUNCTION: XVT 0x43F010
void Hud_ClearUnavailableCraftSystemIndicators(void) {
	ObjectRecord* object;
	int objectIndex;
	uint8_t objectType;
	int excludedCraft;
	uint8_t hudState;
	unsigned int instrumentBaseIndex;

	if (g_players[g_localPlayer].mapCameraState == 0) {
		objectIndex = g_players[g_localPlayer].objectIndex;
		excludedCraft = objectIndex != -1 &&
						((objectType = g_objectTable[objectIndex].objectType) == CRAFT_SPECIES_X_WING ||
						 objectType == CRAFT_SPECIES_Y_WING || objectType == CRAFT_SPECIES_A_WING ||
						 objectType == CRAFT_SPECIES_Z_95_HEADHUNTER || objectType == CRAFT_SPECIES_B_WING);

		if (!excludedCraft) {
			object = &g_objectTable[objectIndex];
			if (object->objectType != CRAFT_SPECIES_TIE_FIGHTER) {
				hudState = g_players[g_localPlayer].viewState.hudStateLive;
				if (hudState == 0 || hudState == 19) {
					instrumentBaseIndex = g_hudInstrumentSetBaseIndex;
					if ((object->mobj->pCraft->systemFlags & CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM) == 0) {
						Hud_DrawCachedSpriteElement(instrumentBaseIndex + 109, 0);
						Hud_DrawCachedSpriteElement(instrumentBaseIndex + 110, 0);
					}
					if ((g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->systemFlags &
						 CRAFT_SUBSYSTEM_FLAG_SHIELDS) == 0) {
						Hud_DrawCachedSpriteElement(instrumentBaseIndex + 108, 0);
					}
				}
			}
		}
	}
}

// FUNCTION: XVT 0x43F140
void Hud_UpdateCraftSystemStatusIndicators(void) {
	uint8_t hudState;
	uint16_t featureIndex;
	uint16_t featureMask;
	uint16_t indicatorState;
	int objectIndex;
	uint8_t objectType;
	int excludedCraft;
	CraftData* craft;

	hudState = g_players[g_localPlayer].viewState.hudStateLive;
	if (hudState == 0) {
		featureMask = 1;
		for (featureIndex = 0; featureIndex < 13; featureMask *= 2, ++featureIndex) {
			objectIndex = g_players[g_localPlayer].objectIndex;
			excludedCraft =
				objectIndex != -1 &&
				((objectType = g_objectTable[objectIndex].objectType) == CRAFT_SPECIES_X_WING ||
				 objectType == CRAFT_SPECIES_Y_WING || objectType == CRAFT_SPECIES_A_WING ||
				 objectType == CRAFT_SPECIES_Z_95_HEADHUNTER || objectType == CRAFT_SPECIES_B_WING);

			if (!excludedCraft) {
				indicatorState =
					(featureMask &
					 g_objectTable[objectIndex].mobj->pCraft->damageStats.activeHudFeatureMask) == 0
						? 13
						: 0;
			} else if ((featureMask &
						g_objectTable[objectIndex].mobj->pCraft->damageStats.activeHudFeatureMask) != 0) {
				continue;
			} else {
				indicatorState = 0;
			}

			if ((featureMask &
				 g_objectTable[objectIndex].mobj->pCraft->damageStats.installedHudFeatureMask) != 0) {
				Hud_DrawCachedSpriteElement(featureIndex + 69, indicatorState);
			}
		}
		g_hudCachedTargetObjectIdx = -1;
		return;
	}

	if (hudState != 19)
		return;

	featureMask = 1;
	for (featureIndex = 0; featureIndex < 13; featureMask *= 2, ++featureIndex) {
		switch (featureMask) {
			case 2:
			case 4:
			case 8:
				continue;

			case 16:
			case 4096:
				objectIndex = g_players[g_localPlayer].objectIndex;
				excludedCraft =
					objectIndex != -1 &&
					((objectType = g_objectTable[objectIndex].objectType) == CRAFT_SPECIES_X_WING ||
					 objectType == CRAFT_SPECIES_Y_WING || objectType == CRAFT_SPECIES_A_WING ||
					 objectType == CRAFT_SPECIES_Z_95_HEADHUNTER || objectType == CRAFT_SPECIES_B_WING);
				if (excludedCraft) {
					continue;
				}
				break;

			default:
				break;
		}

		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		indicatorState = (featureMask & craft->damageStats.activeHudFeatureMask) == 0 ? 13 : 0;
		if ((featureMask & craft->damageStats.installedHudFeatureMask) != 0) {
			uint16_t baseIndex = g_hudInstrumentSetBaseIndex;
			Hud_DrawCachedSpriteElement(baseIndex + featureIndex + 69, indicatorState);
		}
	}
	g_hudCachedTargetObjectIdx = -1;
}

// FUNCTION: XVT 0x43F390
void Hud_DrawCmdTargetDetails(void) {
	enum {
		CMD_TARGET_NAME_ELEMENT = 94,
		CMD_CARGO_ELEMENT = 95,
		CMD_RANGE_ELEMENT = 96,
		CMD_RANGE_FRACTION_CACHE = 97,
		CMD_ORDERS_ELEMENT = 104,
		CMD_ORDER_TARGET_ELEMENT = 105,
		CMD_ORDER_RANGE_ELEMENT = 106,
		CMD_ORDER_TIME_ELEMENT = 107,
		CMD_RANGE_LABEL_ELEMENT = 141,
		CMD_PANEL_BOUNDS_ELEMENT = 143,
		NORMAL_TARGET_DISPLAY_FLAGS = 3,
		HIDDEN_TARGET_DISPLAY_FLAGS = 1,
		HUD_CACHE_DIRTY = -1,
		HUD_CACHE_INVALID = -2,
		DISTANCE_FIXED_SCALE = 161,
		DISTANCE_DECIMAL_SCALE = 100,
		MAX_DISPLAY_DISTANCE = 9999,
		DISTANCE_PER_SPEED_SECOND = 18,
		SECONDS_PER_MINUTE = 60,
		NO_AI_TARGET = 255,
	};

	uint16_t panelLeft;
	uint16_t panelRight;
	int16_t previousTargetObjectIdx;
	uint16_t currentTargetObjectIdx;

#ifdef XVT_MODERN
	XvtCockpitReadouts_BeginTarget(1);
#endif

	g_flightTextShadowEnabled = 0;
	FlightText_SetBackgroundColor(0x2C);
	FlightText_SetFontTier(2);
	panelLeft = g_hudElementLayouts[CMD_PANEL_BOUNDS_ELEMENT].x;
	panelRight = panelLeft + g_hudElementLayouts[CMD_PANEL_BOUNDS_ELEMENT].clipWidth;
	previousTargetObjectIdx = g_hudCachedTargetObjectIdx;

	if (g_players[g_localPlayer].currentTargetObjectIdx != g_hudCachedTargetObjectIdx) {
		uint16_t left;
		uint16_t top;
		uint16_t dirtyState;
		uint16_t invalidState;

#ifdef XVT_MODERN
		XvtCockpitText_ClearTargetFields();
#endif

		previousTargetObjectIdx = g_hudCachedTargetObjectIdx;
		g_hudCachedTargetObjectIdx = g_players[g_localPlayer].currentTargetObjectIdx;
		dirtyState = UINT16_MAX;
		invalidState = UINT16_MAX - 1;
		g_hudElementStateCache[CMD_CARGO_ELEMENT] = dirtyState;
		g_hudElementStateCache[CMD_RANGE_ELEMENT] = dirtyState;
		g_hudElementStateCache[CMD_RANGE_FRACTION_CACHE] = dirtyState;
		g_hudElementStateCache[CMD_ORDERS_ELEMENT] = dirtyState;
		g_hudElementStateCache[CMD_ORDER_TARGET_ELEMENT] = invalidState;
		g_hudElementStateCache[CMD_ORDER_RANGE_ELEMENT] = invalidState;
		g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT] = dirtyState;

		left = g_hudElementLayouts[CMD_TARGET_NAME_ELEMENT].x;
		top = g_hudElementLayouts[CMD_TARGET_NAME_ELEMENT].y;
		FlightText_SetClipRect(left, top, left + g_hudElementLayouts[CMD_TARGET_NAME_ELEMENT].clipWidth,
							   top + g_flightFontLineHeight + 1);
		g_flightFillClipRectFn();
		FlightText_SetCursor(left, top);
		FlightText_SetClearLineBackground(1);

		currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (currentTargetObjectIdx != dirtyState) {
			ObjectRecord* targetObject;
			MobileObject* targetMobileObject;
			CraftData* targetCraft;

			targetObject = &g_objectTable[currentTargetObjectIdx];
			targetMobileObject = targetObject->mobj;
			targetCraft = targetMobileObject != NULL ? targetMobileObject->pCraft : NULL;
			if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && g_flightPlayerCount > 1) {
				int16_t displayFlags;

				displayFlags = NORMAL_TARGET_DISPLAY_FLAGS;
				if (targetMobileObject != NULL && targetCraft != NULL &&
					g_flightMissionState.locatePlayersEnabled == 0) {
					int playerIff;

					playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
					if (targetCraft->iffVisibility[playerIff] == 0) {
						int flightGroupIdx;
						int team;
						int hostile;

						flightGroupIdx = targetObject->flightGroupIdx;
						team = g_missionFlightGroups[flightGroupIdx].fg.team;
						hostile = team == playerIff ? 0 : g_missionTeams[playerIff].allies[team] == 0;
						if (hostile == 1 && g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0)
							displayFlags = HIDDEN_TARGET_DISPLAY_FLAGS;
					}
				}
				Hud_AppendObjectDisplayName(currentTargetObjectIdx, displayFlags);
			} else {
				Hud_AppendObjectDisplayName(currentTargetObjectIdx, NORMAL_TARGET_DISPLAY_FLAGS);
			}
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_NAME, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_CENTER);
#endif
			FlightText_DrawStringCentered(g_flightTextScratchBuffer);
		}

		if ((uint16_t)g_hudCachedTargetObjectIdx >= g_activeRegionCraftObjectSlotEnd) {
			FlightText_SetClipRect(
				panelLeft, g_hudElementLayouts[CMD_PANEL_BOUNDS_ELEMENT].y, panelRight,
				g_hudElementLayouts[CMD_PANEL_BOUNDS_ELEMENT].y +
					g_hudElementLayouts[CMD_PANEL_BOUNDS_ELEMENT].clipHeightOrForegroundColor);
			g_flightFillClipRectFn();
		}
	}

	if (g_players[g_localPlayer].currentTargetObjectIdx != -1) {
		uint16_t left;
		uint16_t top;
		uint16_t distance;
		uint16_t wholeDistance;
		uint16_t fractionalDistance;
		int16_t cargoState;
		const char* cargoText;

		left = g_hudElementLayouts[CMD_RANGE_ELEMENT].x;
		top = g_hudElementLayouts[CMD_RANGE_ELEMENT].y;
		if (previousTargetObjectIdx == -1) {
			FlightText_SetCursor((uint16_t)g_hudElementLayouts[CMD_RANGE_LABEL_ELEMENT].x,
								 (uint16_t)g_hudElementLayouts[CMD_RANGE_LABEL_ELEMENT].y);
			FlightText_SetColor(0x49);
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_DIST]);
		}
		Player_ComputePolarToObjectRef(g_localPlayer, (uint16_t)g_hudCachedTargetObjectIdx);
		FlightText_SetClipRect(left, top, left + FlightText_MeasureStringWidth("00.00"),
							   top + g_flightFontLineHeight + 1);
		FlightText_SetColor(0x4A);
		trig2_polardistance *= DISTANCE_FIXED_SCALE;
		distance = (uint16_t)(trig2_polardistance >> 16);
		if (distance >= MAX_DISPLAY_DISTANCE + 1)
			distance = MAX_DISPLAY_DISTANCE;
		wholeDistance = distance / DISTANCE_DECIMAL_SCALE;
		fractionalDistance = distance - wholeDistance * DISTANCE_DECIMAL_SCALE;
		if (wholeDistance != (uint16_t)g_hudElementStateCache[CMD_RANGE_ELEMENT] ||
			fractionalDistance != (uint16_t)g_hudElementStateCache[CMD_RANGE_FRACTION_CACHE]) {
			g_hudElementStateCache[CMD_RANGE_ELEMENT] = (int16_t)wholeDistance;
			g_hudElementStateCache[CMD_RANGE_FRACTION_CACHE] = (int16_t)fractionalDistance;
			FlightText_SetCursor(left, top);
			g_flightFillClipRectFn();
			if (fractionalDistance < 10)
				sprintf(g_flightTextScratchBuffer, "%ld.0%ld", (long)wholeDistance, (long)fractionalDistance);
			else
				sprintf(g_flightTextScratchBuffer, "%ld.%ld", (long)wholeDistance, (long)fractionalDistance);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_RANGE, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_flightTextScratchBuffer);
		}

		cargoState = 2;
		cargoText = g_strCmdThreatDisplayText[CMD_THREAT_STR_NO_CARGO];
		currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (currentTargetObjectIdx < g_activeRegionCraftObjectSlotEnd) {
			MobileObject* targetMobileObject;

			targetMobileObject = g_objectTable[currentTargetObjectIdx].mobj;
			if (targetMobileObject->state == 0) {
				CraftData* targetCraft;

				targetCraft = targetMobileObject->pCraft;
				if (targetCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] != 0) {
					cargoState = 1;
					cargoText = targetCraft->specialCargoName;
					if (cargoText[0] == '\0') {
						cargoState = 2;
						cargoText = g_strCmdThreatDisplayText[CMD_THREAT_STR_NO_CARGO];
					}
				} else {
					cargoState = 0;
					cargoText = g_strWarheadUnknown;
				}
			}
		}
		if (cargoState != g_hudElementStateCache[CMD_CARGO_ELEMENT]) {
			g_hudElementStateCache[CMD_CARGO_ELEMENT] = cargoState;
			left = g_hudElementLayouts[CMD_CARGO_ELEMENT].x;
			top = g_hudElementLayouts[CMD_CARGO_ELEMENT].y;
			FlightText_SetClipRect(left, top, left + g_hudElementLayouts[CMD_CARGO_ELEMENT].clipWidth,
								   top + g_flightFontLineHeight + 1);
			g_flightFillClipRectFn();
			FlightText_SetCursor(left, top);
			FlightText_SetColor(0x46);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_TARGET_CARGO, cargoText, XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(cargoText);
		}
	}

	currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	if (currentTargetObjectIdx < g_activeRegionCraftObjectSlotEnd &&
		g_players[g_localPlayer].currentTargetObjectIdx != -1) {
		MobileObject* targetMobileObject;
		CraftData* targetCraft;
		AiController* controller;
		int displayPlanId;

		targetMobileObject = g_objectTable[currentTargetObjectIdx].mobj;
		targetCraft = targetMobileObject->pCraft;
		controller = &targetCraft->aiController;
		if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && g_flightPlayerCount > 1 &&
			targetMobileObject != NULL && targetCraft != NULL &&
			g_flightMissionState.locatePlayersEnabled == 0) {
			int playerIff;

			playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (targetCraft->iffVisibility[playerIff] == 0) {
				int team;
				int hostile;

				team = g_missionFlightGroups[g_objectTable[currentTargetObjectIdx].flightGroupIdx].fg.team;
				hostile = team == playerIff ? 0 : g_missionTeams[playerIff].allies[team] == 0;
				if (hostile == 1)
					return;
			}
		}

		displayPlanId = controller->pendingPlanId;
		if (targetCraft->workingSubsystems == 0) {
			displayPlanId = pai_findplanbyname("disabledpln");
		} else if (targetMobileObject->speed == 0) {
			const char* planName;

			planName = g_planTable[displayPlanId].name;
			if (strcmp(planName, "flyhomepln") == 0 || strcmp(planName, "followhomepln") == 0 ||
				strcmp(planName, "flyhomeevadepln") == 0 || strcmp(planName, "followhomeevadepln") == 0 ||
				strcmp(planName, "enterhangarpln") == 0 || strcmp(planName, "exithangarpln") == 0 ||
				strcmp(planName, "intohyperspacepln") == 0 || strcmp(planName, "outofhyperspacepln") == 0 ||
				strcmp(planName, "starshipintohyperpln") == 0 ||
				strcmp(planName, "starshipfollowhomepln") == 0) {
				displayPlanId = pai_findplanbyname("waitpln");
			}
		}

		if ((uint16_t)g_hudElementStateCache[CMD_ORDERS_ELEMENT] != displayPlanId) {
			uint16_t left;
			uint16_t top;
			const char* timeLabel;

			g_hudElementStateCache[CMD_ORDERS_ELEMENT] = (int16_t)displayPlanId;
			left = g_hudElementLayouts[CMD_ORDERS_ELEMENT].x;
			top = g_hudElementLayouts[CMD_ORDERS_ELEMENT].y;
			FlightText_SetClipRect(left, top, panelRight, top + g_flightFontLineHeight + 1);
			g_flightFillClipRectFn();
			FlightText_SetCursor(left, top);
			if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
				FlightText_SetColor(0x45);
			else
				FlightText_SetColor(0x46);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_ORDERS_LABEL,
									   g_strCmdThreatDisplayText[CMD_THREAT_STR_CURRENT_ORDERS],
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_CURRENT_ORDERS]);
			if (g_hudElementLayouts[CMD_ORDERS_ELEMENT].clipWidth == 0) {
				g_hudElementLayouts[CMD_ORDERS_ELEMENT].clipWidth =
					FlightText_MeasureStringWidth(g_strCmdThreatDisplayText[CMD_THREAT_STR_CURRENT_ORDERS]);
			}
			FlightText_SetCursor((uint16_t)left + (uint16_t)g_hudElementLayouts[CMD_ORDERS_ELEMENT].clipWidth,
								 g_flightCursorY);
			FlightText_SetColor(0x4E);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_ORDERS,
									   g_strInFlightMessages[g_planReportMessageIdByPlanId[displayPlanId]],
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strInFlightMessages[g_planReportMessageIdByPlanId[displayPlanId]]);

			left = g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].x;
			top = g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].y;
			FlightText_SetClipRect(left, top, panelRight, top + g_flightFontLineHeight + 1);
			g_flightFillClipRectFn();
			FlightText_SetCursor(left, top);
			if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
				FlightText_SetColor(0x45);
			else
				FlightText_SetColor(0x46);
			if (g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj->speed == 0) {
				timeLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_TIME_REMAINING];
			} else if (controller->targetObjIdx < 0x8000) {
				timeLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_TIME_TO_TARGET];
			} else {
				timeLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_TIME_TO_DESTINATION];
			}
			FlightText_SetScratch(timeLabel);
			g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].clipWidth =
				FlightText_MeasureStringWidth(g_flightTextScratchBuffer);
#ifdef XVT_MODERN
			XvtCockpitReadouts_ClearOrderTime();
			XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_CMD_TIME_UNKNOWN);
			XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_TIME_LABEL, g_flightTextScratchBuffer,
									   XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_flightTextScratchBuffer);
		}

		{
			uint16_t orderTargetObjectIdx;
			unsigned int targetDistance;
			uint16_t valueLeft;
			uint16_t valueTop;
			uint16_t distanceWhole;
			uint16_t distanceFraction;

			if (g_objectTable[currentTargetObjectIdx].playerOwnerIdx != -1)
				orderTargetObjectIdx =
					(uint16_t)g_players[g_objectTable[currentTargetObjectIdx].playerOwnerIdx]
						.currentTargetObjectIdx;
			else
				orderTargetObjectIdx = controller->targetObjIdx;
			if (targetCraft->workingSubsystems == 0)
				orderTargetObjectIdx = UINT16_MAX;
			if (strcmp(g_planTable[displayPlanId].name, "waitpln") == 0)
				orderTargetObjectIdx = UINT16_MAX;

			if (orderTargetObjectIdx != (uint16_t)g_hudElementStateCache[CMD_ORDER_TARGET_ELEMENT]) {
				const char* targetLabel;
				const char* distanceLabel;
				uint16_t left;
				uint16_t top;

				g_hudElementStateCache[CMD_ORDER_TARGET_ELEMENT] = (int16_t)orderTargetObjectIdx;
				if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
					FlightText_SetColor(0x45);
				else
					FlightText_SetColor(0x46);
				left = g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].x;
				top = g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].y;
				FlightText_SetClipRect(left, top, panelRight, top + g_flightFontLineHeight);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				if (orderTargetObjectIdx < 0x8000)
					targetLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_CURRENT_TARGET];
				else
					targetLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_CURRENT_DESTINATION];
				FlightText_SetScratch(targetLabel);
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_TARGET_LABEL, g_flightTextScratchBuffer,
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_flightTextScratchBuffer);
				g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].clipWidth =
					FlightText_MeasureStringWidth(g_flightTextScratchBuffer);

				left = g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].x;
				top = g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].y;
				FlightText_SetClipRect(left, top, panelRight, top + g_flightFontLineHeight);
				g_flightFillClipRectFn();
				FlightText_SetCursor(left, top);
				if (orderTargetObjectIdx < 0x8000)
					distanceLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_DISTANCE_FROM_TARGET];
				else
					distanceLabel = g_strCmdThreatDisplayText[CMD_THREAT_STR_DISTANCE_TO_DESTINATION];
				FlightText_SetScratch(distanceLabel);
#ifdef XVT_MODERN
				XvtCockpitReadouts_ClearOrderRange();
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_RANGE_LABEL, g_flightTextScratchBuffer,
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				FlightText_DrawString(g_flightTextScratchBuffer);
				g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].clipWidth =
					FlightText_MeasureStringWidth(g_flightTextScratchBuffer);

				valueLeft = g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].x +
							g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].clipWidth;
				valueTop = g_hudElementLayouts[CMD_ORDER_TARGET_ELEMENT].y;
				FlightText_SetClipRect(valueLeft, valueTop, panelRight,
									   valueTop + g_flightFontLineHeight + 1);
				g_flightFillClipRectFn();
				FlightText_SetCursor(valueLeft, valueTop);
				if (orderTargetObjectIdx == NO_AI_TARGET || orderTargetObjectIdx == UINT16_MAX) {
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_TARGET,
											   g_strCmdThreatDisplayText[CMD_THREAT_STR_NONE],
											   XVT_COCKPIT_ALIGN_LEFT);
#endif
					FlightText_DrawString(g_strCmdThreatDisplayText[CMD_THREAT_STR_NONE]);
				} else {
					Hud_AppendObjectDisplayName(orderTargetObjectIdx, NORMAL_TARGET_DISPLAY_FLAGS);
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_TARGET, g_flightTextScratchBuffer,
											   XVT_COCKPIT_ALIGN_LEFT);
#endif
					FlightText_DrawString(g_flightTextScratchBuffer);
				}
			}

			targetDistance = 0;
			if (orderTargetObjectIdx != UINT16_MAX) {
				uint16_t distance;

				if (g_objectTable[currentTargetObjectIdx].playerOwnerIdx != -1) {
					pai_ObjectRefDirectionToObjectRef(currentTargetObjectIdx, orderTargetObjectIdx);
				} else {
					trig2_ctop(controller->aimPointX - g_objectTable[currentTargetObjectIdx].world_x,
							   controller->aimPointY - g_objectTable[currentTargetObjectIdx].world_y,
							   controller->aimPointZ - g_objectTable[currentTargetObjectIdx].world_z);
				}
				valueLeft = g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].x +
							g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].clipWidth;
				valueTop = g_hudElementLayouts[CMD_ORDER_RANGE_ELEMENT].y;
				targetDistance = (unsigned int)trig2_polardistance;
				FlightText_SetClipRect(valueLeft, valueTop, panelRight,
									   valueTop + g_flightFontLineHeight + 1);
				FlightText_SetColor(0x4A);
				trig2_polardistance *= DISTANCE_FIXED_SCALE;
				distance = (uint16_t)(trig2_polardistance >> 16);
				if (distance >= MAX_DISPLAY_DISTANCE + 1)
					distance = MAX_DISPLAY_DISTANCE;
				distanceWhole = distance / DISTANCE_DECIMAL_SCALE;
				distanceFraction = distance - distanceWhole * DISTANCE_DECIMAL_SCALE;
				if (distanceFraction != (uint16_t)g_hudElementStateCache[CMD_ORDER_RANGE_ELEMENT]) {
					g_hudElementStateCache[CMD_ORDER_RANGE_ELEMENT] = (int16_t)distanceFraction;
					FlightText_SetCursor(valueLeft, valueTop);
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_RANGE, distanceWhole, 2, 1);
#endif
					FlightText_DrawDecimalNumber(distanceWhole, 2, 1);
#ifdef XVT_MODERN
					XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_ORDER_RANGE_SEPARATOR, ".",
											   XVT_COCKPIT_ALIGN_LEFT);
#endif
					g_flightDrawCharFn('.');
#ifdef XVT_MODERN
					XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_RANGE_FRACTION, distanceFraction,
													2, 2);
#endif
					FlightText_DrawDecimalNumber(distanceFraction, 2, 2);
				}
			}

			valueLeft = g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].x +
						g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].clipWidth;
			valueTop = g_hudElementLayouts[CMD_ORDER_TIME_ELEMENT].y;
			FlightText_SetClipRect(valueLeft, valueTop, panelRight, valueTop + g_flightFontLineHeight + 1);
			FlightText_SetColor(0x52);
			FlightText_SetCursor(valueLeft, valueTop);
			if (g_objectTable[currentTargetObjectIdx].mobj->speed == 0) {
				uint16_t totalSeconds;
				uint16_t minutes;
				uint16_t seconds;

				if (strcmp(g_planTable[controller->pendingPlanId].name, "board2pln") != 0 &&
					strcmp(g_planTable[controller->pendingPlanId].name, "waitpln") != 0) {
					if (targetDistance == 0) {
						g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT] = 0;
						g_flightFillClipRectFn();
#ifdef XVT_MODERN
						XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_CMD_TIME_UNKNOWN);
#endif
#ifdef XVT_MODERN
						XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_MINUTES, 0, 2, 1);
#endif
						FlightText_DrawDecimalNumber(0, 2, 1);
#ifdef XVT_MODERN
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_ORDER_TIME_SEPARATOR, ":",
												   XVT_COCKPIT_ALIGN_LEFT);
#endif
						g_flightDrawCharFn(':');
#ifdef XVT_MODERN
						XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_SECONDS, 0, 2, 2);
#endif
						FlightText_DrawDecimalNumber(0, 2, 2);
					} else {
#ifdef XVT_MODERN
						XvtCockpitReadouts_ClearOrderTime();
						XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CMD_TIME_UNKNOWN, g_strWarheadUnknown,
												   XVT_COCKPIT_ALIGN_LEFT);
#endif
						FlightText_DrawString(g_strWarheadUnknown);
					}
					return;
				}
				totalSeconds = (uint16_t)(controller->maneuverTimer / SIMULATION_TICKS_PER_SECOND);
				minutes = totalSeconds / SECONDS_PER_MINUTE;
				seconds = totalSeconds - minutes * SECONDS_PER_MINUTE;
				if (seconds == (uint16_t)g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT])
					return;
				g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT] = (int16_t)seconds;
				g_flightFillClipRectFn();
#ifdef XVT_MODERN
				XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_CMD_TIME_UNKNOWN);
#endif
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_MINUTES, minutes, 2, 1);
#endif
				FlightText_DrawDecimalNumber(minutes, 2, 1);
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_ORDER_TIME_SEPARATOR, ":",
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				g_flightDrawCharFn(':');
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_SECONDS, seconds, 2, 2);
#endif
				FlightText_DrawDecimalNumber(seconds, 2, 2);
			} else {
				uint16_t totalSeconds;
				uint16_t minutes;
				uint16_t seconds;
				uint16_t distancePerSecond;

				distancePerSecond =
					(uint16_t)(DISTANCE_PER_SPEED_SECOND * g_objectTable[currentTargetObjectIdx].mobj->speed);
				totalSeconds = (uint16_t)(targetDistance / distancePerSecond);
				minutes = totalSeconds / SECONDS_PER_MINUTE;
				seconds = totalSeconds - minutes * SECONDS_PER_MINUTE;
				if (seconds == (uint16_t)g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT])
					return;
				g_hudElementStateCache[CMD_ORDER_TIME_ELEMENT] = (int16_t)seconds;
				g_flightFillClipRectFn();
#ifdef XVT_MODERN
				XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_CMD_TIME_UNKNOWN);
#endif
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_MINUTES, minutes, 2, 1);
#endif
				FlightText_DrawDecimalNumber(minutes, 2, 1);
#ifdef XVT_MODERN
				XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_ORDER_TIME_SEPARATOR, ":",
										   XVT_COCKPIT_ALIGN_LEFT);
#endif
				g_flightDrawCharFn(':');
#ifdef XVT_MODERN
				XvtCockpitReadouts_RecordNumber(XVT_COCKPIT_NUMBER_ORDER_SECONDS, seconds, 2, 2);
#endif
				FlightText_DrawDecimalNumber(seconds, 2, 2);
			}
		}
	}
}

// FUNCTION: XVT 0x440140
void Hud_DrawCmdTargetStatusIndicators(void) {
	int currentTargetObjectIdx;
	CraftData* craft;
	int16_t y;
	uint16_t width;

	currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;

	{
		unsigned int shieldPercent;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx) {
			unsigned int shield;
			unsigned int maxShield;
			unsigned int shieldPercentage;
			craft = g_objectTable[currentTargetObjectIdx].mobj->pCraft;
			shield = (unsigned int)(craft->shieldEnergy[0] + craft->shieldEnergy[1]);
			maxShield = Craft_GetObjectMaxShield(g_players[g_localPlayer].currentTargetObjectIdx);
			shield >>= 1;
			if (maxShield != 0) {
				shieldPercentage = MATH2_percentage(shield, maxShield);
				shieldPercentage &= 0xFFFFu;
				shieldPercent = 2 * (shieldPercentage / 0x28F);
			} else
				shieldPercent = 0;
		} else {
			shieldPercent = 0;
		}
		Hud_DrawCachedNumericElement(0x66, shieldPercent, 1);
	}

	{
		unsigned int hullPercent;
		unsigned int hullPercentage;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx) {
			craft = g_objectTable[currentTargetObjectIdx].mobj->pCraft;
			if (craft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
				craft->objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
				if (craft->hullMax < craft->hullDamage) {
					hullPercent = 1;
				} else {
					hullPercentage =
						(uint16_t)MATH2_percentage(craft->hullMax - craft->hullDamage, craft->hullMax);
					hullPercentage &= 0xFFFFu;
					hullPercent = hullPercentage / 0x28F;
					if (hullPercent == 0)
						hullPercent = 100;
				}
			} else {
				hullPercent = 0;
			}
		} else {
			hullPercent = 0;
		}
		Hud_DrawCachedNumericElement(0x67, hullPercent, 1);
	}

	{
		uint16_t laserState;
		uint16_t i;
		laserState = 0;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx) {
			uint8_t cannonClassCount;
			i = 0;
			cannonClassCount = craft->cannonClassCount;
			if (cannonClassCount != 0)
				do {
					uint8_t projectileType;
					projectileType = craft->laserState.projectileTypeId[i];
					if (projectileType == PROJECTILE_OBJECT_TYPE_IMPERIAL_LASER ||
						projectileType == PROJECTILE_OBJECT_TYPE_REBEL_LASER) {
						laserState = 1;
						if (craft->laserState.linkMode[i] != 0)
							laserState = (uint16_t)(((g_missionElapsedClock.subsecondTicks / 59) & 1) + 1);
					}
					++i;
				} while (i < cannonClassCount);
		}
#ifdef XVT_MODERN
		XvtCockpitReadouts_RecordArmament(0, laserState);
#endif
		Hud_DrawCachedSpriteElement(0x62, laserState);
		if (laserState != 0 && (uint16_t)g_hudElementStateCache[135] != laserState) {
			int16_t bottom;
			if (laserState == 1) {
				FlightText_SetColor(g_hudElementLayouts[135].colorIndex);
				FlightText_SetBackgroundColor(0x2C);
			} else {
				FlightText_SetColor(0x2C);
				FlightText_SetBackgroundColor(g_hudElementLayouts[135].colorIndex + 1);
			}
			y = g_hudElementLayouts[135].y;
			bottom = g_hudElementLayouts[135].y + g_flightFontLineHeight;
			width = FlightText_MeasureStringWidth(g_strThreatDisplayText[0]);
			width += g_hudElementLayouts[135].x;
			FlightText_SetClipRect(g_hudElementLayouts[135].x, y, width, bottom);
			FlightText_SetCursor(g_hudElementLayouts[135].x, g_hudElementLayouts[135].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField((XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_ARMAMENT_LABEL_FIRST + 0),
									   g_strThreatDisplayText[0], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strThreatDisplayText[0]);
		}
	}

	{
		uint16_t ionState;
		uint16_t i;
		ionState = 0;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx) {
			uint8_t cannonClassCount;
			i = 0;
			cannonClassCount = craft->cannonClassCount;
			while (i < cannonClassCount) {
				if (craft->laserState.projectileTypeId[i] == PROJECTILE_OBJECT_TYPE_ION_LASER) {
					ionState = 1;
					if (craft->laserState.linkMode[i] != 0)
						ionState = (uint16_t)(((g_missionElapsedClock.subsecondTicks / 59) & 1) + 1);
				}
				++i;
			}
		}
#ifdef XVT_MODERN
		XvtCockpitReadouts_RecordArmament(1, ionState);
#endif
		Hud_DrawCachedSpriteElement(0x63, ionState);
		if (ionState != 0 && (uint16_t)g_hudElementStateCache[136] != ionState) {
			int16_t bottom;
			if (ionState == 1) {
				FlightText_SetColor(g_hudElementLayouts[136].colorIndex);
				FlightText_SetBackgroundColor(0x2C);
			} else {
				FlightText_SetColor(0x2C);
				FlightText_SetBackgroundColor(g_hudElementLayouts[136].colorIndex + 1);
			}
			y = g_hudElementLayouts[136].y;
			bottom = g_hudElementLayouts[136].y + g_flightFontLineHeight;
			width = FlightText_MeasureStringWidth(g_strThreatDisplayText[1]);
			width += g_hudElementLayouts[136].x;
			FlightText_SetClipRect(g_hudElementLayouts[136].x, y, width, bottom);
			FlightText_SetCursor(g_hudElementLayouts[136].x, g_hudElementLayouts[136].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField((XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_ARMAMENT_LABEL_FIRST + 1),
									   g_strThreatDisplayText[1], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strThreatDisplayText[1]);
		}
	}

	{
		uint16_t warheadState;
		uint16_t i;
		warheadState = 0;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx) {
			if (g_objectTable[currentTargetObjectIdx].playerOwnerIdx == -1) {
				if (craft->aiController.maneuverMode == AI_MANEUVER_MODE_ROCKET_ATTACK) {
					uint8_t warheadLauncherCount;
					i = 0;
					warheadLauncherCount = craft->warheadLauncherCount;
					if (warheadLauncherCount != 0)
						do {
							if (craft->warheadSlotTypeIds[i] != 0) {
								warheadState = 1;
								if (craft->warheadLockTicks > 0)
									warheadState =
										(uint16_t)(((g_missionElapsedClock.subsecondTicks / 59) & 1) + 1);
							}
							++i;
						} while (i < warheadLauncherCount);
				}
			} else {
				uint8_t warheadLauncherCount;
				i = 0;
				warheadLauncherCount = craft->warheadLauncherCount;
				if (warheadLauncherCount != 0)
					do {
						if (craft->warheadSlotTypeIds[i] != 0) {
							warheadState = 1;
							if (craft->warheadLockTicks > 0)
								warheadState =
									(uint16_t)(((g_missionElapsedClock.subsecondTicks / 59) & 1) + 1);
						}
						++i;
					} while (i < warheadLauncherCount);
			}
		}
#ifdef XVT_MODERN
		XvtCockpitReadouts_RecordArmament(2, warheadState);
#endif
		Hud_DrawCachedSpriteElement(0x64, warheadState);
		if (warheadState != 0 && (uint16_t)g_hudElementStateCache[137] != warheadState) {
			int16_t bottom;
			if (warheadState == 1) {
				FlightText_SetColor(g_hudElementLayouts[137].colorIndex);
				FlightText_SetBackgroundColor(0x2C);
			} else {
				FlightText_SetColor(0x2C);
				FlightText_SetBackgroundColor(g_hudElementLayouts[137].colorIndex + 1);
			}
			y = g_hudElementLayouts[137].y;
			bottom = g_hudElementLayouts[137].y + g_flightFontLineHeight;
			width = FlightText_MeasureStringWidth(g_strThreatDisplayText[2]);
			width += g_hudElementLayouts[137].x;
			FlightText_SetClipRect(g_hudElementLayouts[137].x, y, width, bottom);
			FlightText_SetCursor(g_hudElementLayouts[137].x, g_hudElementLayouts[137].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField((XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_ARMAMENT_LABEL_FIRST + 2),
									   g_strThreatDisplayText[2], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strThreatDisplayText[2]);
		}
	}

	{
		uint16_t beamState;
		beamState = 0;
		if (g_activeRegionCraftObjectSlotEnd > currentTargetObjectIdx &&
			craft->beamTypeId != BEAM_TYPE_NONE) {
			beamState = 1;
			if (craft->beamActive != 0 && craft->beamPresent != 0)
				beamState = 2;
		}
#ifdef XVT_MODERN
		XvtCockpitReadouts_RecordArmament(3, beamState);
#endif
		Hud_DrawCachedSpriteElement(0x65, beamState);
		if (beamState != 0 && (uint16_t)g_hudElementStateCache[138] != beamState) {
			int16_t bottom;
			if (beamState == 1) {
				FlightText_SetColor(g_hudElementLayouts[138].colorIndex);
				FlightText_SetBackgroundColor(0x2C);
			} else {
				FlightText_SetColor(0x2C);
				FlightText_SetBackgroundColor(g_hudElementLayouts[138].colorIndex + 1);
			}
			y = g_hudElementLayouts[138].y;
			bottom = g_hudElementLayouts[138].y + g_flightFontLineHeight;
			width = FlightText_MeasureStringWidth(g_strThreatDisplayText[3]);
			width += g_hudElementLayouts[138].x;
			FlightText_SetClipRect(g_hudElementLayouts[138].x, y, width, bottom);
			FlightText_SetCursor(g_hudElementLayouts[138].x, g_hudElementLayouts[138].y);
#ifdef XVT_MODERN
			XvtCockpitText_RecordField((XvtCockpitTextFieldId)(XVT_COCKPIT_TEXT_ARMAMENT_LABEL_FIRST + 3),
									   g_strThreatDisplayText[3], XVT_COCKPIT_ALIGN_LEFT);
#endif
			FlightText_DrawString(g_strThreatDisplayText[3]);
		}
	}
}

// FUNCTION: XVT 0x440760
void Hud_DrawCachedSpriteElement(unsigned int elementIdx, unsigned int state) {
	if ((uint16_t)g_hudElementStateCache[elementIdx] != state) {
		g_hudElementStateCache[elementIdx] = (int16_t)state;
		g_flightBlitSpriteFn(
			g_hudPanelSpriteDataByIndex[(uint16_t)g_hudElementLayouts[elementIdx].selector + state],
			g_hudElementLayouts[elementIdx].x, g_hudElementLayouts[elementIdx].y,
			g_hudElementLayouts[elementIdx].colorIndex, 0);
	}
}

// FUNCTION: XVT 0x4407D0
void Hud_DrawCachedFadedSpriteElement(uint16_t elementIdx, int16_t state, int16_t fade) {
	if (g_hudElementStateCache[elementIdx] != state) {
		g_hudElementStateCache[elementIdx] = state;
		g_flightBlitSpriteFadedFn(g_hudPanelSpriteDataByIndex[g_hudElementLayouts[elementIdx].selector],
								  g_hudElementLayouts[elementIdx].x, g_hudElementLayouts[elementIdx].y,
								  g_hudElementLayouts[elementIdx].colorIndex, (int8_t)state, fade);
	}
}

// FUNCTION: XVT 0x440840
void Hud_DrawCachedNumericElement(uint16_t elementIdx, int16_t value, uint16_t minDigits) {
	uint16_t normalizedElementIdx;
	uint16_t selector;

	normalizedElementIdx = elementIdx;
	if (g_hudElementStateCache[elementIdx] == value)
		return;
	g_hudElementStateCache[elementIdx] = value;
	selector = g_hudElementLayouts[elementIdx].selector;
	FlightText_SetClipRect(g_hudElementLayouts[elementIdx].x, g_hudElementLayouts[elementIdx].y,
						   g_hudElementLayouts[elementIdx].x + selector * g_flightFontHalfHeight + 2,
						   g_hudElementLayouts[elementIdx].y + g_flightFontLineHeight);
	g_flightFillClipRectFn();

	if (elementIdx > HUD_INSTRUMENTS_PER_SET) {
		normalizedElementIdx =
			elementIdx - HUD_INSTRUMENTS_PER_SET * ((uint16_t)(elementIdx - 1) / HUD_INSTRUMENTS_PER_SET);
	}
	if ((uint16_t)value <= 20 &&
		(normalizedElementIdx == 85 || normalizedElementIdx == 82 || normalizedElementIdx == 102 ||
		 normalizedElementIdx == 86 || normalizedElementIdx == 103)) {
		FlightText_SetColor(74);
	} else if ((uint16_t)value <= 50 &&
			   (normalizedElementIdx == 85 || normalizedElementIdx == 82 || normalizedElementIdx == 86 ||
				normalizedElementIdx == 102 || normalizedElementIdx == 103)) {
		FlightText_SetColor(78);
	} else if ((normalizedElementIdx == 41 || normalizedElementIdx == 40) &&
			   g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->engineOutputScale == 0) {
		FlightText_SetColor(82);
	} else {
		FlightText_SetColor(g_hudElementLayouts[elementIdx].colorIndex);
	}
	FlightText_SetCursor(g_hudElementLayouts[elementIdx].x, g_hudElementLayouts[elementIdx].y);
#ifdef XVT_MODERN
	XvtCockpitReadouts_RecordCachedNumber(elementIdx, value, minDigits);
#endif
	FlightText_DrawDecimalNumber(value, selector, minDigits);
}

// FUNCTION: XVT 0x4409D0
void Hud_LoadCockpitResources(void) {
	char cockpitResourceName[16];
	const char* sourceName;
	ModelIndex modelIndex;
	unsigned int nameIndex;

	strcpy(g_hudCockpitBasePath, g_hudCockpitResolutionDirectory);
	modelIndex = GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
	sourceName = g_modelDefs[modelIndex].cockpitResourceName;
	for (nameIndex = 0; sourceName[nameIndex] != '\0'; ++nameIndex) {
		cockpitResourceName[nameIndex] = sourceName[nameIndex];
	}
	cockpitResourceName[nameIndex] = sourceName[nameIndex];
	strcat(g_hudCockpitBasePath, cockpitResourceName);
	g_hudPanelSetId = 0;
	Hud_LoadCockpitInterfaceFile(g_hudCockpitBasePath);
	Hud_LoadAuxiliaryCockpitInterfaceFile();
	modelIndex = GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
	Hud_LoadCockpitSpriteResources(modelIndex);
}

// FUNCTION: XVT 0x440B10
void Hud_LoadCockpitInterfaceFile(const char* basePath) {
	XvtFile* stream;

	strcpy(g_hudCockpitResourcePath, basePath);
	strcat(g_hudCockpitResourcePath, ".INT");
	File_OpenGlobalStream(g_hudCockpitResourcePath, "rb", 1, 0);
	stream = g_stream;
	FeDiskIo_ReadWithRetryPrompt(g_hudCockpitResourceDescriptors, sizeof(HudCockpitResourceDescriptor), 28,
								 stream);
	FeDiskIo_ReadWithRetryPrompt(g_hudElementLayouts, sizeof(HudElementLayout),
								 HUD_CRAFT_LIST_INSTRUMENT_BASE_INDEX, stream);
	FeDiskIo_ReadWithRetryPrompt(&g_hudPanelSpriteFileInfo, sizeof(HudPanelSpriteFileInfo), 1, stream);
	if (g_flightResolutionMode == FLIGHT_RESOLUTION_640X480) {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask0, 480, 1, stream);
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask1, 480, 1, stream);
	} else if (g_flightResolutionMode == FLIGHT_RESOLUTION_480X360) {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask0, 480, 1, stream);
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask1, 480, 1, stream);
	} else {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask0, 200, 1, stream);
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask1, 200, 1, stream);
	}
	FeDiskIo_CloseGlobalStream(0);
#ifdef XVT_MODERN
	XvtRenderAssets_CaptureCockpit(0);
#endif
}

// FUNCTION: XVT 0x440C50
int16_t Hud_LoadAuxiliaryCockpitInterfaceFile(void) {
	XvtFile* stream;

	strcpy(g_hudCockpitResourcePath, g_hudCockpitResolutionDirectory);
	strcat(g_hudCockpitResourcePath, g_hudCockpitResourceDescriptors[HUD_VIEW_CRAFT_LIST].lfdName);
	strcat(g_hudCockpitResourcePath, ".INT");
	File_OpenGlobalStream(g_hudCockpitResourcePath, "rb", 1, 0);
	stream = g_stream;
	FeDiskIo_ReadWithRetryPrompt(&g_hudElementLayouts[HUD_CRAFT_LIST_INSTRUMENT_BASE_INDEX],
								 sizeof(HudElementLayout), HUD_INSTRUMENTS_PER_SET, stream);
	if (g_flightResolutionMode == FLIGHT_RESOLUTION_640X480) {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask2, 480, 1, stream);
	} else if (g_flightResolutionMode == FLIGHT_RESOLUTION_480X360) {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask2, 480, 1, stream);
	} else {
		FeDiskIo_ReadWithRetryPrompt(g_hudViewportSpanMask2, 200, 1, stream);
	}
#ifdef XVT_MODERN
	XvtRenderAssets_CaptureCockpit(1);
#endif
	return FeDiskIo_CloseGlobalStream(0);
}

// FUNCTION: XVT 0x440D60
void Hud_ForcePlayerViewState(int hudViewState, int playerIdx) {
	g_players[playerIdx].viewState.hudStateLive = UINT8_MAX;
	Hud_SetHudViewState(hudViewState, playerIdx);
	if (playerIdx == g_localPlayer)
		Hud_ResetFlightMessagePanes(0);
}

// FUNCTION: XVT 0x440DA0
void Hud_RebuildDisplayForViewState(int hudViewState, int playerIdx) {
	enum {
		HUD_VIEW_RESOURCE_NAME = 17,
		HUD_RESOURCE_REFERENCE = 0x80,
		HUD_RESOURCE_MIRRORED_REFERENCE = 0xC0,
		HUD_AUXILIARY_PANEL_LAYOUT = 396,
		HUD_RESOURCE_NAME_LAYOUT = 49,
		PALETTE_COCKPIT_COLOR_COUNT = 0x40,
	};

	uint8_t mirrorHorizontal;
	int cockpitX;
	unsigned int resourceIndex;
	unsigned int viewportDescriptorIndex;
	unsigned int baseOffset;
	unsigned int pageIndex;
	int resourceLoaded;
	unsigned int textY;

	if (g_localPlayer != playerIdx)
		return;

#ifdef XVT_MODERN
	XvtCockpitText_ResetFields();
	XvtCockpitReadouts_Reset();
	XvtCockpitPages_ResetWorking();
	XvtCockpitMessages_ResetWorking();
#endif
	mirrorHorizontal = 0;
	cockpitX = 0;
	resourceIndex = g_hudCockpitResourceDescriptors[hudViewState].enabled;
	if (hudViewState != HUD_VIEW_FULL_SCREEN) {
		if (resourceIndex >= HUD_RESOURCE_MIRRORED_REFERENCE) {
			resourceIndex -= HUD_RESOURCE_MIRRORED_REFERENCE;
			mirrorHorizontal = 1;
			cockpitX = (int)(g_screenWidth - 1);
		} else if (resourceIndex < HUD_RESOURCE_REFERENCE) {
			resourceIndex = (unsigned int)hudViewState;
		} else {
			resourceIndex -= HUD_RESOURCE_REFERENCE;
		}
	} else {
		resourceIndex = (unsigned int)hudViewState;
	}

	if (g_hudLoadedPanelSetId != g_hudPanelSetId) {
		g_hudPanelSpriteDataWriteCursor = (uint8_t*)Memory_LockHandle(g_hudPanelSpriteDataHandle);
		Memory_UnlockHandle(g_hudPanelSpriteDataHandle);
		strcpy(g_hudCockpitBasePath, g_hudCockpitResolutionDirectory);
		strcat(g_hudCockpitBasePath, g_hudPanelSpriteFileInfo.baseName);
		strcat(g_hudCockpitBasePath, ".PNL");
		Hud_LoadPanelSpriteRecords(
			g_hudCockpitBasePath, 0,
			g_hudPanelSpriteFileInfo.spriteCountAddend + g_hudPanelSpriteFileInfo.spriteCount, 0);
		g_hudLoadedPanelSetId = g_hudPanelSetId;

		if (g_hudElementLayouts[HUD_AUXILIARY_PANEL_LAYOUT].selector == 0) {
			strcpy(g_hudCockpitBasePath, g_hudCockpitResolutionDirectory);
			strcat(g_hudCockpitBasePath, g_hudCockpitResourceDescriptors[HUD_VIEW_CRAFT_LIST].lfdName);
			strcat(g_hudCockpitBasePath, ".PNL");
			Hud_LoadPanelSpriteRecords(
				g_hudCockpitBasePath,
				g_hudPanelSpriteFileInfo.spriteCountAddend + g_hudPanelSpriteFileInfo.spriteCount + 1, 1, 0);
			g_hudElementLayouts[HUD_AUXILIARY_PANEL_LAYOUT].selector =
				g_hudPanelSpriteFileInfo.spriteCountAddend + g_hudPanelSpriteFileInfo.spriteCount + 1;
		}
	}

	FlightRender_InvokeTransitionHook(1);
	if (g_players[g_localPlayer].viewState.hudStateLive != HUD_VIEW_FULL_SCREEN) {
		resourceLoaded =
			g_hudCockpitResources[resourceIndex].memoryHandle != 0 && g_hudCockpitResourcesLoaded != 0;
		if (resourceLoaded == 0) {
			g_hudCockpitResourceWriteCursor = g_flightLog1Buffer;
			Hud_LoadCockpitLfdEntries(g_hudCockpitResourceDescriptors[resourceIndex].lfdName,
									  g_hudCockpitResources[resourceIndex].entries,
									  sizeof(g_hudCockpitResources[resourceIndex].entries) /
										  sizeof(g_hudCockpitResources[resourceIndex].entries[0]));
		}
		g_flightSetPaletteRangeFn((RgbTriplet*)g_hudCockpitResources[resourceIndex].entries[2], 0,
								  PALETTE_COCKPIT_COLOR_COUNT);
		FlightText_SetClipRect(0, 0, g_surfaceWidth, g_surfaceHeight);
		g_flightTextBgColor = g_unusedFlightRenderColorByte;
		g_flightFillClipRectFn();
		g_flightBlitSpriteFn(g_hudCockpitResources[resourceIndex].entries[0], cockpitX, 0, 0,
							 mirrorHorizontal);

		viewportDescriptorIndex = mirrorHorizontal == 1 ? (unsigned int)hudViewState : resourceIndex;
		baseOffset = g_flightComputePixelOffsetFn(
			g_hudCockpitResourceDescriptors[viewportDescriptorIndex].viewportOriginX,
			g_hudCockpitResourceDescriptors[viewportDescriptorIndex].viewportOriginY);
		SetFlightViewport(g_hudCockpitResourceDescriptors[viewportDescriptorIndex].viewportWidth,
						  g_hudCockpitResourceDescriptors[viewportDescriptorIndex].viewportHeight,
						  g_flightViewportMode, baseOffset);
		FlightSw_CopyViewportSpanMaskRle(g_hudCockpitResources[resourceIndex].entries[1], g_flightVpWidth,
										 g_flightVpHeight, mirrorHorizontal);
		g_projOffsetY = g_hudCockpitResourceDescriptors[viewportDescriptorIndex].projectionOffsetY;
	} else {
		FlightText_SetClipRect(0, 0, g_surfaceWidth, g_surfaceHeight);
		g_flightTextBgColor = g_unusedFlightRenderColorByte;
		g_flightFillClipRectFn();
		SetFlightViewport(g_surfaceWidth, g_surfaceHeight, g_flightViewportMode, 0);
		FlightSw_BuildFullViewportSpanMaskRle((uint16_t)g_surfaceWidth, (unsigned int)g_surfaceHeight);
		g_projOffsetY = 0;
	}

	if (g_players[playerIdx].viewState.hudStateMirror == HUD_VIEW_HUD_ONLY ||
		g_players[playerIdx].viewState.hudStateMirror == HUD_VIEW_FORWARD) {
		g_mfdSavedSecondaryPage = g_mfdSecondaryPage;
		g_mfdSavedActivePage = g_mfdActivePage;
		for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
			g_savedMfdPageStates[pageIndex] = g_mfdPageStates[pageIndex];
		}
	}

	if (g_players[playerIdx].viewState.hudStateMirror == HUD_VIEW_CRAFT_LIST) {
		switch (g_flightResolutionMode) {
			case FLIGHT_RESOLUTION_320X240:
				g_mfdCraftListBlitWidth = 112;
				g_mfdCraftListBlitHeight = 47;
				break;
			case FLIGHT_RESOLUTION_640X480:
				g_mfdCraftListBlitWidth = 225;
				g_mfdCraftListBlitHeight = 94;
				break;
			case FLIGHT_RESOLUTION_480X360:
				g_mfdCraftListBlitWidth = 168;
				g_mfdCraftListBlitHeight = 70;
				break;
		}
		for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
			if (g_mfdPageStates[pageIndex] != MFD_PAGE_STATE_CLOSED)
				g_mfdPageStates[pageIndex] = MFD_PAGE_STATE_CLOSING;
		}
		g_mfdActivePage = MFD_PAGE_NONE;
		g_mfdSecondaryPage = MFD_PAGE_NONE;
		Hud_UpdateMfdPages();
	}

	if (hudViewState == HUD_VIEW_FULL_SCREEN) {
		g_hudInstrumentSetBaseIndex = HUD_COCKPIT_INSTRUMENT_BASE_INDEX;
		if (g_players[playerIdx].viewState.externalCameraActive != 0) {
			for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
				if (g_mfdPageStates[pageIndex] != MFD_PAGE_STATE_CLOSED)
					g_mfdPageStates[pageIndex] = MFD_PAGE_STATE_CLOSING;
			}
			g_mfdActivePage = MFD_PAGE_NONE;
			g_mfdSecondaryPage = MFD_PAGE_NONE;
		}
	} else if (hudViewState == HUD_VIEW_HUD_ONLY) {
		if (g_players[playerIdx].viewState.hudStateMirror != HUD_VIEW_FORWARD) {
			g_mfdSecondaryPage = g_mfdSavedSecondaryPage;
			g_mfdActivePage = g_mfdSavedActivePage;
			for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
				g_mfdPageStates[pageIndex] = g_savedMfdPageStates[pageIndex];
			}
		}
		g_hudInstrumentSetBaseIndex = HUD_MAP_INSTRUMENT_BASE_INDEX;
	} else if (hudViewState == HUD_VIEW_CRAFT_LIST) {
		for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
			if (g_mfdPageStates[pageIndex] != MFD_PAGE_STATE_CLOSED)
				g_mfdPageStates[pageIndex] = MFD_PAGE_STATE_CLOSING;
		}
		g_hudInstrumentSetBaseIndex = HUD_CRAFT_LIST_INSTRUMENT_BASE_INDEX;
		++g_mfdPageStates[MFD_PAGE_COMMAND];
		++g_mfdPageStates[MFD_PAGE_FRIENDLY_CRAFT];
		g_mfdActivePage = MFD_PAGE_FRIENDLY_CRAFT;
		g_mfdSecondaryPage = MFD_PAGE_FRIENDLY_CRAFT;
		Hud_ResetFlightMessagePanes(1);
	} else if (hudViewState == HUD_VIEW_TARGET_CAMERA) {
		for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
			if (g_mfdPageStates[pageIndex] != MFD_PAGE_STATE_CLOSED)
				g_mfdPageStates[pageIndex] = MFD_PAGE_STATE_CLOSING;
		}
		g_hudInstrumentSetBaseIndex = HUD_COCKPIT_INSTRUMENT_BASE_INDEX;
		g_mfdActivePage = MFD_PAGE_NONE;
		g_mfdSecondaryPage = MFD_PAGE_NONE;
		Hud_ResetFlightMessagePanes(1);
	} else {
		g_hudInstrumentSetBaseIndex = HUD_COCKPIT_INSTRUMENT_BASE_INDEX;
		if (g_players[playerIdx].viewState.hudStateMirror != HUD_VIEW_HUD_ONLY) {
			Hud_ResetFlightMessagePanes(1);
			g_mfdSecondaryPage = g_mfdSavedSecondaryPage;
			g_mfdActivePage = g_mfdSavedActivePage;
			for (pageIndex = MFD_PAGE_SCOREBOARD; pageIndex < MFD_PAGE_COUNT; ++pageIndex) {
				g_mfdPageStates[pageIndex] = g_savedMfdPageStates[pageIndex];
			}
		}
	}

	Hud_InitHUD(playerIdx);
	g_flightInitialTextureCacheFlushPending = 1;
	FlightRender_ResetPalette(1);
	g_flightDisplayRebuildPending = 0;
	if (resourceIndex == HUD_VIEW_RESOURCE_NAME) {
		FlightText_SetFontTier(2);
		textY = 0;
		textY = (uint16_t)g_hudElementLayouts[HUD_RESOURCE_NAME_LAYOUT].y;
		FlightText_SetClipRect(g_hudElementLayouts[HUD_RESOURCE_NAME_LAYOUT].x, (int)textY,
							   g_hudElementLayouts[HUD_RESOURCE_NAME_LAYOUT].x +
								   g_hudElementLayouts[HUD_RESOURCE_NAME_LAYOUT].clipWidth,
							   (int)(textY + g_flightFontLineHeight + 1));
		FlightText_SetBackgroundColor(PALETTE_COCKPIT_COLOR_COUNT);
		g_flightFillClipRectFn();
		FlightText_SetColor(PALETTE_COCKPIT_COLOR_COUNT + 3);
		FlightText_SetCursor(0, textY);
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(
			XVT_COCKPIT_TEXT_RESOURCE_NAME,
			g_hudCockpitResourceDescriptors[g_players[g_localPlayer].viewState.hudStateLive].displayName,
			XVT_COCKPIT_ALIGN_CENTER);
#endif
		FlightText_DrawStringCentered(
			g_hudCockpitResourceDescriptors[g_players[g_localPlayer].viewState.hudStateLive].displayName);
	}
#ifdef XVT_MODERN
	XvtCockpit_RefreshInstruments(playerIdx);
#endif
}

// FUNCTION: XVT 0x441550
void Hud_LoadCockpitLfdEntries(const char* lfdName, uint8_t** outEntries, unsigned int entryCount) {
	XvtFile* stream;
	uint8_t** outputEntry;
	int16_t isPalette;
	uint16_t paletteTagIndex;
	size_t dataSize;
	uint16_t entryIndex;
	LfdEntryHeader header;

	strcpy(g_hudCockpitResourcePath, g_hudCockpitResolutionDirectory);
	strcat(g_hudCockpitResourcePath, lfdName);
	strcat(g_hudCockpitResourcePath, ".LFD");
	File_OpenGlobalStream(g_hudCockpitResourcePath, "rb", 1, 0);
	stream = g_stream;
	entryIndex = 0;
	while (entryIndex < entryCount) {
		outputEntry = &outEntries[entryIndex];
		*outputEntry = g_hudCockpitResourceWriteCursor;
		isPalette = 1;
		FeDiskIo_ReadWithRetryPrompt(&header, sizeof(header), 1, stream);
		paletteTagIndex = 0;
		while (paletteTagIndex < 4) {
			if (g_lfdPaletteResourceTypeTag[paletteTagIndex] != header.resourceType[paletteTagIndex]) {
				isPalette = 0;
			}
			++paletteTagIndex;
		}
		dataSize = header.dataSize;
		FeDiskIo_ReadWithRetryPrompt(g_hudCockpitResourceWriteCursor, dataSize, 1, stream);
		if (isPalette == 0) {
			g_hudCockpitResourceWriteCursor += dataSize;
		} else {
			while (dataSize-- != 0) {
				*g_hudCockpitResourceWriteCursor >>= 2;
				++g_hudCockpitResourceWriteCursor;
			}
		}
		if (isPalette != 0) {
			*outputEntry += 2;
		}
		++entryIndex;
	}
	FeDiskIo_CloseGlobalStream(0);
#ifdef XVT_MODERN
	XvtRenderAssets_RegisterLfd(g_hudCockpitResourcePath, outEntries);
#endif
}

// FUNCTION: XVT 0x4416F0
void Hud_LoadCockpitSpriteResources(unsigned int modelIndex) {
	uint16_t resourceIndex;

	(void)modelIndex;

	for (resourceIndex = 0;
		 resourceIndex < (uint16_t)(sizeof(g_hudCockpitResources) / sizeof(g_hudCockpitResources[0]));
		 ++resourceIndex) {
		uint16_t memoryHandle;
		size_t fileSize;

		g_hudCockpitResources[resourceIndex].memoryHandle = 0;
		if (g_hudCockpitResourceDescriptors[resourceIndex].enabled == 1) {
			strcpy(g_hudCockpitResourcePath, g_hudCockpitResolutionDirectory);
			strcat(g_hudCockpitResourcePath, g_hudCockpitResourceDescriptors[resourceIndex].lfdName);
			strcat(g_hudCockpitResourcePath, ".LFD");
			File_OpenGlobalStream(g_hudCockpitResourcePath, "rb", 1, 0);
			if (g_stream != NULL) {
#ifdef XVT_MODERN
				File_RawSeek((XvtFile*)g_stream, 0, SEEK_END);
				fileSize = (size_t)File_RawTell((XvtFile*)g_stream);
#else
				fileSize = (size_t)_filelength((_fileno)((XvtFile*)g_stream));
#endif
				FeDiskIo_CloseGlobalStream(0);
				FeDiskIo_UnlockGlobalBuffers();
				memoryHandle = Memory_AllocHandle(fileSize, 0);
				if (memoryHandle == 0) {
					FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
				}
				FeDiskIo_LockGlobalBuffers();
				if (memoryHandle != 0) {
					g_hudCockpitResources[resourceIndex].memoryHandle = (int16_t)memoryHandle;
					g_hudCockpitResourceWriteCursor = (uint8_t*)Memory_LockHandle(memoryHandle);
					Memory_UnlockHandle(memoryHandle);
					Hud_LoadCockpitLfdEntries(g_hudCockpitResourceDescriptors[resourceIndex].lfdName,
											  g_hudCockpitResources[resourceIndex].entries,
											  sizeof(g_hudCockpitResources[resourceIndex].entries) /
												  sizeof(g_hudCockpitResources[resourceIndex].entries[0]));
				}
			}
		}
	}

	for (resourceIndex = 0;
		 resourceIndex < (uint16_t)(sizeof(g_hudCockpitResources) / sizeof(g_hudCockpitResources[0]));
		 ++resourceIndex) {
		uint16_t memoryHandle;

		if (g_hudCockpitResourceDescriptors[resourceIndex].enabled == 1) {
			strcpy(g_hudCockpitResourcePath, g_hudCockpitResolutionDirectory);
			strcat(g_hudCockpitResourcePath, g_hudCockpitResourceDescriptors[resourceIndex].lfdName);
			strcat(g_hudCockpitResourcePath, ".LFD");
			memoryHandle = (uint16_t)g_hudCockpitResources[resourceIndex].memoryHandle;
			if (memoryHandle != 0) {
				g_hudCockpitResourceWriteCursor = (uint8_t*)Memory_LockHandle(memoryHandle);
				Memory_UnlockHandle(memoryHandle);
				Hud_LoadCockpitLfdEntries(g_hudCockpitResourceDescriptors[resourceIndex].lfdName,
										  g_hudCockpitResources[resourceIndex].entries,
										  sizeof(g_hudCockpitResources[resourceIndex].entries) /
											  sizeof(g_hudCockpitResources[resourceIndex].entries[0]));
			}
		}
	}
	g_hudCockpitResourcesLoaded = 1;
}

// FUNCTION: XVT 0x4419B0
void Hud_ReloadCockpitInterfaceFile(void) {
	g_flightRenderTransitionHook();
	g_hudPanelSetId = 0;
	Hud_LoadCockpitInterfaceFile(g_hudCockpitResourcePath);
}

// FUNCTION: XVT 0x4419D0
void Hud_UpdateMfdPages(void) {
	uint16_t page;
	int16_t pageState;
	unsigned int pageIndex;

	for (page = MFD_PAGE_SCOREBOARD; page < MFD_PAGE_COUNT; ++page) {
		pageIndex = page;
		pageState = g_mfdPageStates[pageIndex];
		switch (pageIndex) {
			case MFD_PAGE_SCOREBOARD:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawMissionScoreboardPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_SCOREBOARD_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_GOALS:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawMissionGoalsPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_GOALS_ELEMENT] = pageState;
				}
				break;
			case MFD_PAGE_MESSAGE_LOG:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawMessageLogPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_DAMAGE:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Damage_DisplayMfdPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_DAMAGE_ELEMENT] = pageState;
				}
				break;
			case MFD_PAGE_FLIGHT_GROUPS:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawCraftListPage(1);
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_FRIENDLY_CRAFT:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawCraftListPage(0);
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_COMMAND:
				if (pageState == MFD_PAGE_STATE_CLOSING) {
					Mfd_DrawCommandMenuPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT] =
						pageState;
				}
				break;
			default:
				break;
		}
		if (pageState == MFD_PAGE_STATE_CLOSING)
			g_mfdPageStates[pageIndex] = MFD_PAGE_STATE_CLOSED;
	}

	for (page = MFD_PAGE_SCOREBOARD; page < MFD_PAGE_COUNT; ++page) {
		pageIndex = page;
		pageState = g_mfdPageStates[pageIndex];
		switch (pageIndex) {
			case MFD_PAGE_SCOREBOARD:
				if (pageState) {
					Mfd_DrawMissionScoreboardPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_SCOREBOARD_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_GOALS:
				if (pageState) {
					Mfd_DrawMissionGoalsPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_GOALS_ELEMENT] = pageState;
				}
				break;
			case MFD_PAGE_MESSAGE_LOG:
				if (pageState) {
					Mfd_DrawMessageLogPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_DAMAGE:
				if (pageState) {
					if (Damage_DisplayMfdPage() != 0) {
						g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_DAMAGE_ELEMENT] =
							pageState;
					}
				}
				break;
			case MFD_PAGE_FLIGHT_GROUPS:
				if (pageState) {
					Mfd_DrawCraftListPage(1);
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_FRIENDLY_CRAFT:
				if (pageState) {
					Mfd_DrawCraftListPage(0);
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT] =
						pageState;
				}
				break;
			case MFD_PAGE_COMMAND:
				if (pageState) {
					Mfd_DrawCommandMenuPage();
					g_hudElementStateCache[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT] =
						pageState;
				}
				break;
			default:
				break;
		}
	}
}

// FUNCTION: XVT 0x441C30
void Hud_BlitSoftwareMfdPages(void) {
	int16_t pageState;
	uint16_t page;
	CraftData* craft;
	int modelIndex;
	uint16_t launcherIndex;
	uint16_t launcherCount;
	int layoutIndex;
	int16_t selector;

	for (page = MFD_PAGE_SCOREBOARD; page < MFD_PAGE_COUNT; ++page) {
		pageState = g_mfdPageStates[page];
		switch (page) {
			case MFD_PAGE_SCOREBOARD:
				if (pageState != MFD_PAGE_STATE_CLOSED) {
					if (g_players[g_localPlayer].mapCameraState != 0) {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdMapBlitSourceX,
							g_mfdMapBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.y,
							g_mfdMapBlitWidth, g_mfdMapBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					} else {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar,
							g_mfdMissionScoreboardBlitSourceX, g_mfdMissionScoreboardBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_SCOREBOARD_ELEMENT].x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_SCOREBOARD_ELEMENT].y,
							g_mfdMissionScoreboardBlitWidth, g_mfdMissionScoreboardBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					}
				}
				break;
			case MFD_PAGE_GOALS:
				if (pageState != MFD_PAGE_STATE_CLOSED) {
					if (g_players[g_localPlayer].mapCameraState != 0) {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdMapBlitSourceX,
							g_mfdMapBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.y,
							g_mfdMapBlitWidth, g_mfdMapBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					} else {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdGoalsBlitSourceX,
							g_mfdGoalsBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_GOALS_ELEMENT].x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_GOALS_ELEMENT].y,
							g_mfdGoalsBlitWidth, g_mfdGoalsBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					}
				}
				break;
			case MFD_PAGE_DAMAGE:
				if (pageState != MFD_PAGE_STATE_CLOSED && g_players[g_localPlayer].mapCameraState == 0) {
					Blit16ToFlightSurface(
						g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdDamageBlitSourceX,
						g_mfdDamageBlitSourceY,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_DAMAGE_ELEMENT].x,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_DAMAGE_ELEMENT].y,
						g_mfdDamageBlitWidth, g_mfdDamageBlitHeight,
						g_flight16bppBytesPerPixel * g_screenWidth);
				}
				break;
			case MFD_PAGE_FLIGHT_GROUPS:
				if (pageState != MFD_PAGE_STATE_CLOSED) {
					if (g_players[g_localPlayer].mapCameraState != 0) {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdMapBlitSourceX,
							g_mfdMapBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT]
								.y,
							g_mfdMapBlitWidth, g_mfdMapBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					} else {
						Blit16ToFlightSurface(
							g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdCraftListBlitSourceX,
							g_mfdCraftListBlitSourceY,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT].x,
							g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT].y,
							g_mfdCraftListBlitWidth, g_mfdCraftListBlitHeight,
							g_flight16bppBytesPerPixel * g_screenWidth);
					}
				}
				break;
			case MFD_PAGE_FRIENDLY_CRAFT:
				if (pageState != MFD_PAGE_STATE_CLOSED) {
					Blit16ToFlightSurface(
						g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdCraftListBlitSourceX,
						g_mfdCraftListBlitSourceY,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT].x,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_CRAFT_LIST_ELEMENT].y,
						g_mfdCraftListBlitWidth, g_mfdCraftListBlitHeight,
						g_flight16bppBytesPerPixel * g_screenWidth);
				}
				break;
			case MFD_PAGE_COMMAND:
				if (pageState != MFD_PAGE_STATE_CLOSED && g_players[g_localPlayer].mapCameraState != 0) {
					Blit16ToFlightSurface(
						g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_mfdMapBlitSourceX,
						g_mfdMapBlitSourceY,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT].x,
						g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MAP_OR_COMMAND_ELEMENT].y,
						g_mfdMapBlitWidth, g_mfdMapBlitHeight, g_flight16bppBytesPerPixel * g_screenWidth);
				}
				break;
			default:
				break;
		}
	}

	if (g_hudInstrumentSetBaseIndex == HUD_MAP_INSTRUMENT_BASE_INDEX) {
		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->damageStats.activeHudFeatureMask & 8) != 0) {
			modelIndex = craft->modelIndex;
			FlightText_SetFontTier(0);
			launcherIndex = 0;
			launcherCount = g_modelDefs[modelIndex].warheadLauncherSlotCount[1] +
							g_modelDefs[modelIndex].warheadLauncherSlotCount[0];
			while (launcherIndex < launcherCount) {
				layoutIndex = g_hudInstrumentSetBaseIndex + launcherIndex;
				selector = g_hudElementLayouts[layoutIndex + 27].selector;
				if (selector != 0) {
#ifdef XVT_MODERN
					XvtCockpit_LatchLauncher(launcherIndex, g_hudElementLayouts[layoutIndex + 27].x,
											 g_hudElementLayouts[layoutIndex + 27].y,
											 (g_flightFontHalfHeight + 1) * selector, g_flightFontLineHeight);
#endif
					Blit16ToFlightSurface(g_flightOffscreenBuffer, g_flightColorEscapeBypassChar,
										  (g_flightFontHalfHeight + 1) * launcherIndex + 2, 2,
										  g_hudElementLayouts[layoutIndex + 27].x,
										  g_hudElementLayouts[layoutIndex + 27].y,
										  (g_flightFontHalfHeight + 1) * selector, g_flightFontLineHeight,
										  g_flight16bppBytesPerPixel * g_screenWidth);
				}
				++launcherIndex;
			}
		}
	}
#ifdef XVT_MODERN
	XvtCockpit_LatchPages();
#endif
}

// FUNCTION: XVT 0x4422C0
void Hud_Update3DCrt(uint16_t a1, uint16_t a2, uint16_t a3, uint16_t a4, int16_t a5) {
	enum {
		LOW_RESOLUTION_MASK_SIZE = 200,
		HIGH_RESOLUTION_MASK_SIZE = 480,
		TARGET_BOX_RENDER_FLAG = 0x0200,
		TARGET_CROSS_RENDER_FLAG = 0x0400,
		CRT_RENDER_COLOR = 48,
		COMPONENT_MARKER_SIZE = 4,
		COMPONENT_MARKER_HALF_SIZE = COMPONENT_MARKER_SIZE / 2,
		COMPONENT_MARKER_COLOR = 0xCE,
	};

	int savedProjOffsetY;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;
	uint16_t savedRenderObjectRef;
	unsigned int baseOffset;
	int componentRelX;
	int componentRelY;
	int componentRelZ;
	uint16_t targetObjectIdx;
	ObjectRecord* targetObject;
	MobileObject* targetMobileObject;
	uint16_t objectIndex;

#ifdef XVT_MODERN
	XvtRenderDraw_Scope(XVT_SCOPE_CRT);
#endif
	savedProjOffsetY = g_projOffsetY;
	savedTargetX = g_players[g_localPlayer].viewState.savedTargetX;
	savedRenderObjectRef = g_renderObjectRef;
	savedTargetY = g_players[g_localPlayer].viewState.savedTargetY;
	savedTargetZ = g_players[g_localPlayer].viewState.savedTargetZ;
	g_projOffsetY = 0;
	baseOffset = (unsigned int)g_flightComputePixelOffsetFn(a1, a2);
	PushFlightViewport(a3, a4, a5, baseOffset);

	if (a5 != 0) {
		unsigned int hudInstrumentBaseIndex;
		uint8_t* destinationMask;
		const uint8_t* sourceMask;
		uint16_t maskIndex;

		destinationMask = &g_flightAuxBuffer[g_viewportSpanMaskOffset];
		hudInstrumentBaseIndex = g_hudInstrumentSetBaseIndex;
		if (hudInstrumentBaseIndex != HUD_COCKPIT_INSTRUMENT_BASE_INDEX) {
			if (hudInstrumentBaseIndex == HUD_MAP_INSTRUMENT_BASE_INDEX) {
				sourceMask = g_hudViewportSpanMask1;
			} else if (hudInstrumentBaseIndex == HUD_CRAFT_LIST_INSTRUMENT_BASE_INDEX) {
				sourceMask = g_hudViewportSpanMask2;
			} else {
				sourceMask = g_hudViewportSpanMask0;
			}
		} else {
			sourceMask = g_hudViewportSpanMask0;
		}
		if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240) {
			for (maskIndex = LOW_RESOLUTION_MASK_SIZE; maskIndex != 0; --maskIndex) {
				destinationMask[maskIndex - 1] = sourceMask[maskIndex - 1];
			}
		} else {
			for (maskIndex = HIGH_RESOLUTION_MASK_SIZE; maskIndex != 0; --maskIndex) {
				destinationMask[maskIndex - 1] = sourceMask[maskIndex - 1];
			}
		}
	}

	if (g_useHardware3D != 0) {
		std3D_FillZBufferFromViewportMask();
	}
	Hud_PointCamera(g_players[g_localPlayer].currentTargetObjectIdx, 1, g_localPlayer);
#ifdef XVT_MODERN
	XvtRenderCapture_Crt(a1, a2, a3, a4, a5);
#endif
	if (g_players[g_localPlayer].targetBoxEnabled != 0) {
		g_renderObjectRef |= TARGET_BOX_RENDER_FLAG;
	} else {
		g_renderObjectRef |= TARGET_CROSS_RENDER_FLAG;
	}
	g_unusedFlightRenderColorByte = CRT_RENDER_COLOR;
	RenderScene_Initialize(1);
	g_sceneBillboardQueueCount = 0;
	g_camRelWorldX = worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = worldlocz - g_players[g_localPlayer].viewState.savedTargetZ;

	if (g_players[g_localPlayer].targetBoxEnabled != 0) {
		targetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (targetObjectIdx < g_activeRegionCraftObjectSlotEnd) {
			g_rotatedX = ModelMesh_GetComponentFocusX(
				g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType,
				(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
			g_rotatedY = ModelMesh_GetComponentFocusZ(
				g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType,
				(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
			g_rotatedZ = -ModelMesh_GetComponentFocusY(
				g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType,
				(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
			pai_RotateLocalVectorToWorldScratch(
				&g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx], g_rotatedX,
				g_rotatedY, g_rotatedZ);
			componentRelX = g_rotatedX + worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
			componentRelY = g_rotatedY + worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
			componentRelZ = g_rotatedZ + worldlocz - g_players[g_localPlayer].viewState.savedTargetZ;
		}
	}

	viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	depthZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	targetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	targetObject = &g_objectTable[targetObjectIdx];
	targetMobileObject = targetObject->mobj;
	if (targetMobileObject != NULL) {
		switch (targetObject->genusId) {
			case CRAFT_GENUS_STARFIGHTER:
			case CRAFT_GENUS_TRANSPORT:
			case CRAFT_GENUS_UTILITY_VEHICLE:
			case CRAFT_GENUS_FREIGHTER:
			case CRAFT_GENUS_STARSHIP:
			case CRAFT_GENUS_PLATFORM:
				g_curCraft = targetMobileObject->pCraft;
				FVIEW_SetObjectTransform(
					targetObject->roll, targetObject->pitch, targetObject->yaw, 0,
					&g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx]);
				FlightLight_SetupObjectLightingByIndex(
					(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx);
				Damage_QueueCraftBillboards(g_players[g_localPlayer].currentTargetObjectIdx);
				RenderScene_DrawObjectModel(
					&g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx]);
				break;
			case CRAFT_GENUS_PLAYER_PROJECTILE:
			case CRAFT_GENUS_OTHER_PROJECTILE:
				FVIEW_SetObjectTransform(
					targetObject->roll, targetObject->pitch, targetObject->yaw, 0,
					&g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx]);
				RenderBillboard_DrawRollAlignedObjectModel(g_players[g_localPlayer].currentTargetObjectIdx);
				break;
			default:
				break;
		}
	} else if (targetObject->genusId >= CRAFT_GENUS_MINE &&
			   targetObject->genusId <= CRAFT_GENUS_SMALL_DEBRIS) {
		FVIEW_SetObjectTransform(targetObject->roll, targetObject->pitch, targetObject->yaw, 0, NULL);
		g_transformLightDirectionToObjectSpace = 1;
		RenderNonCraftSceneObject(targetObjectIdx);
	}

	for (objectIndex = 0; objectIndex < g_regionMainObjectSlotEnd; ++objectIndex) {
		int objectTableIndex;
		ObjectRecord* object;
		int genusId;
		uint16_t objectType;

		if (objectIndex == g_localTransientSlotStart &&
			(g_debrisEnabled == 0 || g_flightMissionState.provingGroundsModeActive != 0)) {
			objectIndex = (uint16_t)g_localDebrisSlotEnd;
			if (objectIndex == g_regionMainObjectSlotEnd) {
				break;
			}
		}
		if (g_players[g_localPlayer].viewState.cameraFocusObjIdx != objectIndex ||
			g_players[g_localPlayer].viewState.externalCameraActive != 0 || g_replayViewMode != 0) {
			objectTableIndex = objectIndex;
			object = &g_objectTable[objectTableIndex];
			objectType = g_objectTable[objectTableIndex].objectType;
			if (objectType != 0) {
				g_currentObjectBoundsExtent = g_modelTypeTable[objectType].maxBoundsExtent;
				genusId = object->genusId;
				if (genusId >= CRAFT_GENUS_PLAYER_PROJECTILE) {
					if (object->genusId <= CRAFT_GENUS_OTHER_PROJECTILE) {
						if (g_players[g_localPlayer].mapCameraState != 0) {
							if (object->mobj->sourceObjIdx !=
									g_players[g_localPlayer].currentTargetObjectIdx ||
								FlightView_IsObjectSphereVisible(objectIndex, g_currentObjectBoundsExtent) ==
									0) {
								continue;
							}
						} else if ((g_players[g_localPlayer].currentTargetObjectIdx !=
										object->mobj->sourceObjIdx &&
									g_players[g_localPlayer].objectIndex != object->mobj->sourceObjIdx) ||
								   FlightView_IsObjectSphereVisible(objectIndex,
																	g_currentObjectBoundsExtent) == 0) {
							continue;
						}
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						RenderBillboard_DrawRollAlignedObjectModel(objectIndex);
					} else if (genusId == CRAFT_GENUS_EXPLOSION &&
							   FlightView_IsObjectSphereVisible(objectIndex, g_currentObjectBoundsExtent) !=
								   0) {
						FVIEW_SetObjectTransform(
							g_objectTable[objectTableIndex].roll, g_objectTable[objectTableIndex].pitch,
							g_objectTable[objectTableIndex].yaw, 0, &g_objectTable[objectTableIndex]);
						SceneBillboard_QueueObjectTextured(objectIndex);
					}
				}
			}
		}
	}

	sw3d_DrawVisibleFacesToSurface();
	if (g_sceneBillboardQueueCount != 0) {
		g_flightSwRotSpriteCoeffCacheValid = 0;
		SceneBillboard_RenderQueuedTextured(0);
		g_flightSwRotSpriteCoeffCacheValid = 0;
	}
	if (g_players[g_localPlayer].targetBoxEnabled != 0) {
		targetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (targetObjectIdx < g_activeRegionCraftObjectSlotEnd &&
			g_objectTable[targetObjectIdx].genusId != CRAFT_GENUS_STARFIGHTER) {
			int markerX;
			int markerY;

			viewX = TRANSFM2_CamMatDotRow0(componentRelX, componentRelY, componentRelZ);
#ifdef XVT_MODERN
			XvtRenderCapture_CrtMarker(componentRelX, componentRelY, componentRelZ);
#endif
			viewY = TRANSFM2_CamMatDotRow1(componentRelX, componentRelY, componentRelZ);
			depthZ = TRANSFM2_CamMatDotRow2(componentRelX, componentRelY, componentRelZ);
			markerX = TRANSFM2_ProjectScreenX(viewX, depthZ);
			markerY = TRANSFM2_ProjectScreenY(viewY, depthZ);
			Hud_DrawComponentMarkerBox(markerX - COMPONENT_MARKER_HALF_SIZE,
									   markerY - COMPONENT_MARKER_HALF_SIZE, COMPONENT_MARKER_SIZE,
									   COMPONENT_MARKER_SIZE, COMPONENT_MARKER_COLOR);
		}
	}

	RenderScene_UnlockBuffers();
	g_unusedFlightRenderColorByte = g_flightColorEscapeBypassChar;
	PopFlightViewport();
	g_renderObjectRef = savedRenderObjectRef;
	g_players[g_localPlayer].viewState.savedTargetX = savedTargetX;
	g_players[g_localPlayer].viewState.savedTargetY = savedTargetY;
	g_players[g_localPlayer].viewState.savedTargetZ = savedTargetZ;
	g_projOffsetY = savedProjOffsetY;
#ifdef XVT_MODERN
	XvtRenderDraw_Scope(XVT_SCOPE_COCKPIT);
#endif
}

// FUNCTION: XVT 0x442BC0
void Hud_DrawComponentMarkerBox(int x, int y, int width, int height, uint8_t colorIdx) {
	enum { COMPONENT_MARKER_DEPTH = 1 };

	Hud_DrawBoxInXTrans(x, y, width, height, colorIdx, COMPONENT_MARKER_DEPTH);
}

// FUNCTION: XVT 0x442BF0
void Hud_PointCamera(uint16_t targetIdx, int16_t useHudLayoutScale, int playerIdx) {
	enum {
		CAMERA_SCALE_LIMIT = 0x3FFF,
		CAMERA_AIM_CENTER_Q16 = 0x4000,
		CAMERA_DIVISOR_LOW = 60,
		CAMERA_DIVISOR_HIGH = 100,
		CAMERA_DIVISOR_DEFAULT = 144,
		CAMERA_OFFSET_COUNT = 3,
	};

	int deltaX;
	int deltaY;
	int deltaZ;
	int scaledX;
	int scaledY;
	int scaledZ;
	int cameraX;
	int cameraY;
	int cameraZ;
	int maxExtent;
	unsigned int scale;
	uint16_t scaleShift;
	uint16_t colorIndex;
	uint16_t resolutionMode;
	ModelIndex modelIndex;

	Mission_ResolveObjectOrMissionPointWorldLoc(targetIdx, 0);
	if (g_players[playerIdx].mapCameraState != 0) {
		deltaX = (int32_t)((uint32_t)worldlocx - (uint32_t)g_players[playerIdx].viewState.savedTargetX);
		deltaY = (int32_t)((uint32_t)worldlocy - (uint32_t)g_players[playerIdx].viewState.savedTargetY);
		deltaZ = (int32_t)((uint32_t)worldlocz - (uint32_t)g_players[playerIdx].viewState.savedTargetZ);
	} else {
		ObjectRecord* playerObject = &g_objectTable[g_players[playerIdx].objectIndex];
		deltaX = (int32_t)((uint32_t)worldlocx - (uint32_t)playerObject->world_x);
		deltaY = (int32_t)((uint32_t)worldlocy - (uint32_t)playerObject->world_y);
		deltaZ = (int32_t)((uint32_t)worldlocz - (uint32_t)playerObject->world_z);
	}
	scaledX = (int32_t)((uint32_t)deltaX * 2u);
	scaledY = (int32_t)((uint32_t)deltaY * 2u);
	{
		int16_t highY = (int16_t)(scaledY >> 16);
		int16_t highX;
		int16_t highZ;
		scaledZ = (int32_t)((uint32_t)deltaZ * 2u);
		highX = (int16_t)(scaledX >> 16);
		highZ = (int16_t)(scaledZ >> 16);
		if ((highX & 0x8000) != 0)
			highX = (int16_t)-highX;
		if ((highY & 0x8000) != 0)
			highY = (int16_t)-highY;
		if ((highZ & 0x8000) != 0)
			highZ = (int16_t)-highZ;
		do {
			highX = (int16_t)((uint16_t)highX >> 1);
			scaledX >>= 1;
			scaledY >>= 1;
			scaledZ >>= 1;
			highY = (int16_t)((uint16_t)highY >> 1);
			highZ = (int16_t)((uint16_t)highZ >> 1);
		} while (highX != 0 || highY != 0 || highZ != 0);
	}
	scaledX >>= 1;
	scaledY >>= 1;
	scaledZ >>= 1;

	if (g_players[playerIdx].mapCameraState != 0) {
		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								g_players[playerIdx].viewState.viewPitch,
								g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL);
		cameraX = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ, g_camMatR0_X,
									  g_camMatR0_Y, g_camMatR0_Z);
		cameraY = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ, g_camMatR2_X,
									  g_camMatR2_Y, g_camMatR2_Z);
		cameraZ = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ, g_camMatR1_X,
									  g_camMatR1_Y, g_camMatR1_Z);
		trig2_ctop(cameraX, cameraY, cameraZ);
		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								g_players[playerIdx].viewState.viewPitch,
								g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)(CAMERA_AIM_CENTER_Q16 - pitchQ16), trig2_xyangle, NULL);
	} else {
		ObjectRecord* playerObject = &g_objectTable[g_players[playerIdx].objectIndex];
		if (playerObject->mobj->orientMatrixDirty != 0) {
			FVIEW_calcrotatemove(playerObject->pitch, playerObject->yaw,
								 &g_objectTable[g_players[playerIdx].objectIndex]);
			FVIEW_calcrotateorient(g_objectTable[g_players[playerIdx].objectIndex].roll, 0,
								   &g_objectTable[g_players[playerIdx].objectIndex]);
		}
		cameraX = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideX,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideY,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideZ);
		cameraY = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdX,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdY,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdZ);
		cameraZ = Math_Dot3Q15Wrapped((int16_t)scaledX, (int16_t)scaledY, (int16_t)scaledZ,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpX,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpY,
									  g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpZ);
		trig2_ctop(cameraX, cameraY, cameraZ);
		FVIEW_BuildCameraOrient(g_objectTable[g_players[playerIdx].objectIndex].roll,
								g_objectTable[g_players[playerIdx].objectIndex].pitch,
								g_objectTable[g_players[playerIdx].objectIndex].yaw, 0,
								(int16_t)(CAMERA_AIM_CENTER_Q16 - pitchQ16), trig2_xyangle, NULL);
	}

	{
		ObjectRecord* target = &g_objectTable[targetIdx];
		if (target->mobj != NULL && target->mobj->pCraft != NULL) {
			int16_t boundY;
			int16_t boundX;
			int largestA;
			int largestB;
			modelIndex = target->mobj->pCraft->modelIndex;
			boundY = g_modelDefs[modelIndex].boundSizeY;
			boundX = g_modelDefs[modelIndex].boundSizeX;
			if (boundX <= boundY && boundX <= g_modelDefs[modelIndex].boundSizeZ) {
				largestA = boundY;
				largestB = g_modelDefs[modelIndex].boundSizeZ;
			} else if (boundX >= boundY && g_modelDefs[modelIndex].boundSizeZ >= boundY) {
				largestA = boundX;
				largestB = g_modelDefs[modelIndex].boundSizeZ;
			} else {
				largestA = boundX;
				largestB = boundY;
			}
			maxExtent = (int)((unsigned int)(largestA + largestB) >> 1)
						<< g_modelDefs[modelIndex].boundSizeShift;
		} else {
			maxExtent = g_modelTypeTable[target->objectType].maxBoundsExtent;
		}
	}
	if (useHudLayoutScale != 0) {
		colorIndex = g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 2].colorIndex;
	} else {
		resolutionMode = g_players[playerIdx].network.flightResolutionMode;
		colorIndex = resolutionMode == FLIGHT_RESOLUTION_320X240
						 ? CAMERA_DIVISOR_LOW
						 : (resolutionMode == FLIGHT_RESOLUTION_480X360 ? CAMERA_DIVISOR_HIGH
																		: CAMERA_DIVISOR_DEFAULT);
	}
	resolutionMode = g_players[playerIdx].network.flightResolutionMode;
	scaleShift =
		(resolutionMode == FLIGHT_RESOLUTION_640X480 || resolutionMode == FLIGHT_RESOLUTION_480X360) ? 9 : 8;
	scale = ((unsigned int)maxExtent << scaleShift) / (unsigned int)colorIndex;
	scaleShift = 0;
	while (scale > CAMERA_SCALE_LIMIT) {
		scale >>= 1;
		++scaleShift;
	}
	if (resolutionMode == FLIGHT_RESOLUTION_640X480)
		scale = ((uint16_t)scale >> 2) + (uint16_t)scale;
	scale = (uint16_t)scale;
	g_players[playerIdx].viewState.savedTargetX = Math_MulQ15((int)scale, g_camMatR2_X);
	g_players[playerIdx].viewState.savedTargetY = Math_MulQ15((int)scale, g_camMatR2_Y);
	g_players[playerIdx].viewState.savedTargetZ = Math_MulQ15((int)scale, g_camMatR2_Z);
	if (scaleShift != 0) {
		g_players[playerIdx].viewState.savedTargetX =
			(int32_t)((uint32_t)g_players[playerIdx].viewState.savedTargetX << scaleShift);
		g_players[playerIdx].viewState.savedTargetY =
			(int32_t)((uint32_t)g_players[playerIdx].viewState.savedTargetY << scaleShift);
		g_players[playerIdx].viewState.savedTargetZ =
			(int32_t)((uint32_t)g_players[playerIdx].viewState.savedTargetZ << scaleShift);
	}
	g_players[playerIdx].viewState.savedTargetX =
		(int32_t)((uint32_t)worldlocx - (uint32_t)g_players[playerIdx].viewState.savedTargetX);
	g_players[playerIdx].viewState.savedTargetY =
		(int32_t)((uint32_t)worldlocy - (uint32_t)g_players[playerIdx].viewState.savedTargetY);
	g_players[playerIdx].viewState.savedTargetZ =
		(int32_t)((uint32_t)worldlocz - (uint32_t)g_players[playerIdx].viewState.savedTargetZ);
}

// FUNCTION: XVT 0x450260
void Hud_ResetFlightMessagePanes(int forceExpireActiveMessages) {

	if (g_readyMessagePaneLeft == -1) {
		switch (g_flightResolutionMode) {
			case FLIGHT_RESOLUTION_320X240:
				g_readyMessagePaneLeft = 42;
				g_readyMessagePaneRight = 277;
				g_systemMessagePaneLeft = 42;
				g_systemMessagePaneRight = 277;
				g_readyMessagePaneTop = 3;
				g_readyMessagePaneBottom = 26;
				g_systemMessagePaneTop = 32;
				g_systemMessagePaneBottom = 37;
				g_flightGroupMessagePaneLeft = 75;
				g_flightGroupMessagePaneTop = 44;
				g_flightGroupMessagePaneRight = 265;
				g_flightGroupMessagePaneBottom = 49;
				break;
			case FLIGHT_RESOLUTION_640X480:
				g_readyMessagePaneTop = 6;
				g_readyMessagePaneRight = 505;
				g_readyMessagePaneBottom = 53;
				g_systemMessagePaneTop = 64;
				g_systemMessagePaneRight = 555;
				g_systemMessagePaneBottom = 75;
				g_flightGroupMessagePaneLeft = 150;
				g_flightGroupMessagePaneTop = 89;
				g_flightGroupMessagePaneRight = 530;
				g_flightGroupMessagePaneBottom = 100;
				g_readyMessagePaneLeft = 85;
				g_systemMessagePaneLeft = 85;
				break;
			case FLIGHT_RESOLUTION_480X360:
				g_readyMessagePaneTop = 4;
				g_readyMessagePaneRight = 378;
				g_readyMessagePaneBottom = 39;
				g_systemMessagePaneTop = 48;
				g_systemMessagePaneRight = 415;
				g_systemMessagePaneBottom = 56;
				g_flightGroupMessagePaneLeft = 112;
				g_flightGroupMessagePaneTop = 66;
				g_flightGroupMessagePaneRight = 397;
				g_flightGroupMessagePaneBottom = 74;
				g_readyMessagePaneLeft = 63;
				g_systemMessagePaneLeft = 63;
				break;
		}

		FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
								 g_screenWidth * g_flight16bppBytesPerPixel);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(g_readyMessagePaneLeft, g_readyMessagePaneTop, g_readyMessagePaneRight,
							   g_readyMessagePaneBottom);
		g_flightFillClipRectFn();
		FlightText_SetClipRect(g_systemMessagePaneLeft, g_systemMessagePaneTop, g_systemMessagePaneRight,
							   g_systemMessagePaneBottom);
		g_flightFillClipRectFn();
		FlightText_SetClipRect(g_flightGroupMessagePaneLeft, g_flightGroupMessagePaneTop,
							   g_flightGroupMessagePaneRight, g_flightGroupMessagePaneBottom);
		g_flightFillClipRectFn();
	} else if (forceExpireActiveMessages != 0) {
		++g_flightMessagePanesForceExpire;
	}

	FlightSurface_Lock();
	FlightSw_SetRenderTarget(NULL, 320, 200, 0);
	FlightText_SetWordWrap(0);
	FlightText_SetClearLineBackground(0);
	FlightText_SetFontTier(1);
	Hud_DrawCraftNameFpsAndNetworkStatus();
	FlightText_SetColor(0x43);
	g_unusedReadyMessagePaneInitialState = g_readyMessagePaneQueue[0].stateOrMessageId;
	g_radioMessageBackupEnabled = 0;
	g_readyMessageQueueCount = 0;
	g_targetDescriptionMessageId = 331;
	g_readyMessagePaneQueue[0].stateOrMessageId = UINT16_MAX;
	g_systemMessagePane.stateOrMessageId = UINT16_MAX;
	g_flightGroupMessagePane.stateOrMessageId = UINT16_MAX;
	FlightSurface_Unlock();
#ifdef XVT_MODERN
	XvtCockpitMessages_ResetWorking();
#endif
}

// FUNCTION: XVT 0x450BC0
void Hud_ShiftReadyMessageQueueForReplacement(void) {
	uint8_t oldPendingCount;
	uint16_t destinationIndex;
	uint8_t newPendingCount;

	if (g_readyMessagePaneQueue[0].showCount < 2 && g_readyMessagePaneQueue[0].ageTicks == 0) {
		oldPendingCount = g_readyMessageQueueCount;
		destinationIndex = oldPendingCount + 1;
		if (destinationIndex != 0) {
			do {
				g_readyMessagePaneQueue[destinationIndex] = g_readyMessagePaneQueue[destinationIndex - 1];
				destinationIndex--;
			} while (destinationIndex != 0);
		}

		newPendingCount = oldPendingCount + 1;
		g_readyMessageQueueCount = newPendingCount;
		if (newPendingCount >= 10) {
			g_readyMessageQueueCount = newPendingCount - 1;
		}
	}
}

// FUNCTION: XVT 0x450C30
void Hud_AdvanceReadyMessageQueue(void) {
	uint8_t oldPendingCount = g_readyMessageQueueCount;
	uint16_t destinationIndex = 0;

	if (oldPendingCount != 0) {
		do {
			g_readyMessagePaneQueue[destinationIndex] = g_readyMessagePaneQueue[destinationIndex + 1];
			++destinationIndex;
		} while (destinationIndex < oldPendingCount);
	}

	g_readyMessageQueueCount = oldPendingCount - 1;
}

// FUNCTION: XVT 0x450C90
void Hud_ShowFlightMessagePane(int16_t paneType) {

	char* text;
	uint16_t paneWidth;
	uint16_t textWidth;
	uint8_t prefix;
	uint8_t currentChar;
	uint16_t visibleChars;
	char lastChar;

#ifdef XVT_MODERN
	lastChar = 0;
#endif

	if (g_readyMessagePaneQueue[0].stateOrMessageId == UINT16_MAX && paneType != 3 && paneType != 8 &&
		paneType != 4 && paneType != 7) {
		return;
	}

	FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
							 g_flight16bppBytesPerPixel * g_screenWidth);
	if (g_readyMessagePaneQueue[0].voiceSfxId != 0 && g_readyMessagePaneQueue[0].showCount == 0 &&
		g_gameConfig.voiceSpecialEnabled != 0) {
		fsfx_QueueVoiceSfx(g_readyMessagePaneQueue[0].voiceSfxId, 0, 0, 0, 0xFFFFu);
	}

	if (paneType == 3 || paneType == 4 || paneType == 7) {
		FlightText_SetFontTier(1);
		text = g_systemMessagePane.text;
		g_systemMessagePane.stateOrMessageId = 1;
		FlightText_SetClipRect(g_systemMessagePaneLeft, g_systemMessagePaneTop, g_systemMessagePaneRight,
							   g_systemMessagePaneBottom);
		paneWidth = g_systemMessagePaneRight - g_systemMessagePaneLeft;
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		g_flightFillClipRectFn();
		paneWidth >>= 1;
		textWidth = Hud_MeasureFlightMessagePaneText(paneType);
		textWidth >>= 1;
		paneWidth -= textWidth;
		FlightText_SetCursor(g_systemMessagePaneLeft + paneWidth, g_systemMessagePaneTop);
	} else if (paneType == 8) {
		FlightText_SetFontTier(1);
		text = g_flightGroupMessagePane.text;
		g_flightGroupMessagePane.stateOrMessageId = 1;
		FlightText_SetClipRect(g_flightGroupMessagePaneLeft, g_flightGroupMessagePaneTop,
							   g_flightGroupMessagePaneRight, g_flightGroupMessagePaneBottom);
		paneWidth = g_flightGroupMessagePaneRight - g_flightGroupMessagePaneLeft;
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		g_flightFillClipRectFn();
		paneWidth >>= 1;
		textWidth = Hud_MeasureFlightMessagePaneText(paneType);
		textWidth >>= 1;
		paneWidth -= textWidth;
		FlightText_SetCursor(g_flightGroupMessagePaneLeft + paneWidth, g_flightGroupMessagePaneTop);
	} else {
		if (g_readyMessagePaneQueue[0].stateOrMessageId == 374)
			fsfx_PlaySound(FLIGHT_SOUND_MESSAGE_READY, -1, g_localPlayer);
		if (g_mfdPageStates[MFD_PAGE_MESSAGE_LOG] != MFD_PAGE_STATE_CLOSED) {
			Hud_SetFlightMessagePaneTimer(g_readyMessagePaneQueue[0].paneType, lastChar);
			++g_readyMessagePaneQueue[0].showCount;
			FlightSw_SetRenderTarget(NULL, 320, 200, 0);
			return;
		}
		text = g_readyMessagePaneQueue[0].text;
		Hud_SetupReadyMessagePaneText();
	}

#ifdef XVT_MODERN
	XvtCockpitMessages_BeginMessage(paneType);
#endif
	prefix = (uint8_t)*text;
	if (prefix < 9) {
		++text;
		FlightText_SetColor(g_messageTextPrefixColorCodes[prefix]);
		if (prefix == 1) {
			currentChar = (uint8_t)*text;
			if (currentChar >= '0' && currentChar <= '3') {
				++text;
				FlightText_SetColor(g_messageTextPrefixColorCodes[currentChar - '(']);
			}
		} else if (prefix == 2) {
			FlightText_SetColor(g_messageSenderIffColorCodes[g_readyMessagePaneQueue[0].senderIff]);
		}
	} else {
		FlightText_SetColor(0x42);
	}

	visibleChars = 0;
	while (*text != '\0' && visibleChars < 70) {
		currentChar = (uint8_t)*text;
		if (currentChar == '[') {
			if (g_flightTextColorIndex == 0xD4)
				--g_flightTextColorIndex;
			else
				++g_flightTextColorIndex;
			++text;
		} else if (currentChar == ']') {
			if (g_flightTextColorIndex == 0xD3)
				++g_flightTextColorIndex;
			else
				--g_flightTextColorIndex;
			++text;
		} else if (currentChar == 0xFE) {
			text += 2;
		} else {
			++visibleChars;
			g_flightDrawCharFn(currentChar);
			lastChar = *text;
			++text;
		}
	}

#ifdef XVT_MODERN
	XvtCockpitMessages_RecordReveal(visibleChars);
#endif
	if (prefix == 3 || prefix == 4 || prefix == 7) {
		Hud_SetFlightMessagePaneTimer(g_systemMessagePane.paneType, lastChar);
		++g_systemMessagePane.showCount;
	} else if (prefix == 8) {
		Hud_SetFlightMessagePaneTimer(g_flightGroupMessagePane.paneType, lastChar);
		++g_flightGroupMessagePane.showCount;
	} else {
		Hud_SetFlightMessagePaneTimer(g_readyMessagePaneQueue[0].paneType, lastChar);
		++g_readyMessagePaneQueue[0].showCount;
	}
	FlightSw_SetRenderTarget(NULL, 320, 200, 0);
}

// FUNCTION: XVT 0x451060
void Hud_SetupReadyMessagePaneText(void) {
	uint16_t paneWidth;
	uint16_t textWidth;

	FlightText_SetFontTier(1);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	g_flightTextShadowEnabled = 1;
	FlightText_SetShadowColor(0x40);
	FlightText_SetClipRect(g_readyMessagePaneLeft, g_readyMessagePaneTop, g_readyMessagePaneRight,
						   g_readyMessagePaneBottom);
	g_flightFillClipRectFn();
	paneWidth = g_readyMessagePaneRight - g_readyMessagePaneLeft;
	textWidth = Hud_MeasureFlightMessagePaneText(0);
	paneWidth >>= 1;
	textWidth >>= 1;
	paneWidth -= textWidth;
	FlightText_SetCursor(g_readyMessagePaneLeft + paneWidth, g_readyMessagePaneTop);
	FlightText_SetColor(0x43);
}

// FUNCTION: XVT 0x451100
void Hud_SetFlightMessagePaneTimer(int16_t paneType, char lastChar) {
	if (g_mfdPageStates[MFD_PAGE_MESSAGE_LOG] == MFD_PAGE_STATE_CLOSED) {
		if (lastChar != '?' && lastChar != '!' && lastChar != ':' && lastChar != ' ' && lastChar != '.')
			g_flightDrawCharFn('.');
		FlightText_SetClearLineBackground(1);
		g_flightDrawCharFn('\n');
		FlightText_SetClearLineBackground(0);
	}
	if (paneType == 3 || paneType == 7)
		g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 472;
	else if (paneType == 8)
		g_playerFlightTransientTimers[g_localPlayer].flightGroupMessagePaneTimer = 1888;
	else if (paneType == 4)
		g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 1888;
	else if (g_readyMessageQueueCount != 0)
		g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 354;
	else if (paneType == 2 || paneType == 1)
		g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 1416;
	else
		g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 1652;
#ifdef XVT_MODERN
	XvtCockpitMessages_EndMessage();
#endif
	Hud_DrawCraftNameFpsAndNetworkStatus();
	FlightText_SetFontTier(2);
}

// FUNCTION: XVT 0x451210
void Hud_UpdateFlightMessagePanes(void) {

	int localPlayer = g_localPlayer;
	const int messageSurfaceWidth = 320;
	const int messageSurfaceHeight = 200;

	if (((g_playerFlightTransientTimers[localPlayer].readyMessagePaneTimer == 0 &&
		  g_readyMessagePaneQueue[0].stateOrMessageId != UINT16_MAX) ||
		 g_flightMessagePanesForceExpire != 0) &&
		g_mfdPageStates[MFD_PAGE_MESSAGE_LOG] == MFD_PAGE_STATE_CLOSED) {
		if (g_readyMessageQueueCount != 0) {
			Hud_AdvanceReadyMessageQueue();
			Hud_ShowFlightMessagePane(g_readyMessagePaneQueue[0].paneType);
		} else {
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
									 g_screenWidth * g_flight16bppBytesPerPixel);
			FlightText_SetClipRect(g_readyMessagePaneLeft, g_readyMessagePaneTop, g_readyMessagePaneRight,
								   g_readyMessagePaneBottom);
			g_flightFillClipRectFn();
			FlightSw_SetRenderTarget(NULL, messageSurfaceWidth, messageSurfaceHeight, 0);
			g_readyMessagePaneQueue[0].stateOrMessageId = UINT16_MAX;
#ifdef XVT_MODERN
			XvtCockpitMessages_Clear(XVT_COCKPIT_MESSAGE_READY);
#endif
		}
		localPlayer = g_localPlayer;
	}
	if ((g_playerFlightTransientTimers[localPlayer].systemMessagePaneTimer == 0 &&
		 g_systemMessagePane.stateOrMessageId != UINT16_MAX) ||
		g_flightMessagePanesForceExpire != 0) {
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
								 g_screenWidth * g_flight16bppBytesPerPixel);
		FlightText_SetClipRect(g_systemMessagePaneLeft, g_systemMessagePaneTop, g_systemMessagePaneRight,
							   g_systemMessagePaneBottom);
		g_flightFillClipRectFn();
		FlightSw_SetRenderTarget(NULL, messageSurfaceWidth, messageSurfaceHeight, 0);
		g_systemMessagePane.stateOrMessageId = UINT16_MAX;
#ifdef XVT_MODERN
		XvtCockpitMessages_Clear(XVT_COCKPIT_MESSAGE_SYSTEM);
#endif
		localPlayer = g_localPlayer;
	}
	if ((g_playerFlightTransientTimers[localPlayer].flightGroupMessagePaneTimer == 0 &&
		 g_flightGroupMessagePane.stateOrMessageId != UINT16_MAX) ||
		g_flightMessagePanesForceExpire != 0) {
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight,
								 g_screenWidth * g_flight16bppBytesPerPixel);
		FlightText_SetClipRect(g_flightGroupMessagePaneLeft, g_flightGroupMessagePaneTop,
							   g_flightGroupMessagePaneRight, g_flightGroupMessagePaneBottom);
		g_flightFillClipRectFn();
		FlightSw_SetRenderTarget(NULL, messageSurfaceWidth, messageSurfaceHeight, 0);
		g_flightGroupMessagePane.stateOrMessageId = UINT16_MAX;
#ifdef XVT_MODERN
		XvtCockpitMessages_Clear(XVT_COCKPIT_MESSAGE_FLIGHT_GROUP);
#endif
		localPlayer = g_localPlayer;
	}
	if (g_playerFlightTransientTimers[localPlayer].targetDescriptionRefreshTimer == 0 &&
		g_players[localPlayer].currentTargetObjectIdx != -1) {
		if (g_targetDescriptionMessageId !=
			msg_BuildTargetDescription(g_players[localPlayer].currentTargetObjectIdx, localPlayer, 0, 0)) {
			if (msg_BuildTargetDescription(g_players[g_localPlayer].currentTargetObjectIdx, g_localPlayer, 0,
										   1) != 0) {
				uint8_t hudState = g_players[g_localPlayer].viewState.hudStateLive;
				if (hudState == 19 || hudState == 0 || hudState == 20) {
					g_targetDescriptionMessageId = msg_BuildTargetDescription(
						g_players[g_localPlayer].currentTargetObjectIdx, g_localPlayer, 1, 0);
					g_playerFlightTransientTimers[g_localPlayer].targetDescriptionRefreshTimer = 1180;
				}
			}
		}
	}
	{
		int playerIndex;
		for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
			if (g_players[playerIndex].connectedFlag != 0 && g_players[playerIndex].pendingActionTimer == 0)
				g_players[playerIndex].pendingActionId = 0;
		}
	}
	if (g_flightMessagePanesForceExpire != 0)
		g_flightMessagePanesForceExpire = 0;
}

// FUNCTION: XVT 0x451560
void Hud_ClearReadyMessageQueue(void) {
	g_readyMessageQueueCount = 0;
	g_readyMessagePaneQueue[0].stateOrMessageId = UINT16_MAX;
	g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 0;
}

// FUNCTION: XVT 0x451590
void Hud_AdvanceFlightMessagePaneTimers(void) {
	if (g_readyMessagePaneQueue[0].stateOrMessageId != UINT16_MAX) {
		++g_readyMessagePaneQueue[0].ageTicks;
	}
	if (g_systemMessagePane.stateOrMessageId != UINT16_MAX) {
		++g_systemMessagePane.ageTicks;
	}
	if (g_flightGroupMessagePane.stateOrMessageId != UINT16_MAX) {
		++g_flightGroupMessagePane.ageTicks;
	}
}

// FUNCTION: XVT 0x4515D0
void Hud_DrawCraftNameFpsAndNetworkStatus(void) {

	uint8_t hudStateLive;
	int elementIndex;
	int offscreenPitchBytes;

	hudStateLive = g_players[g_localPlayer].viewState.hudStateLive;
	if (hudStateLive != HUD_VIEW_FORWARD && hudStateLive != HUD_VIEW_HUD_ONLY) {
		return;
	}

	FlightSurface_Lock();
	FlightSw_SetRenderTarget(NULL, 320, 200, 0);
	FlightText_SetBackgroundColor(44);
	FlightText_SetFontTier(0);
	if (g_flightResolutionMode == FLIGHT_RESOLUTION_320X240)
		Hud_AppendObjectDisplayName(g_players[g_localPlayer].objectIndex, 7);
	else
		Hud_AppendObjectDisplayName(g_players[g_localPlayer].objectIndex, 3);
	FlightText_SetCursor(g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].x,
						 g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 126].y);
	elementIndex = g_hudInstrumentSetBaseIndex;
	FlightText_SetClipRect(
		g_hudElementLayouts[elementIndex + 126].x, g_hudElementLayouts[elementIndex + 126].y,
		g_hudElementLayouts[elementIndex + 126].x + g_hudElementLayouts[elementIndex + 126].clipWidth,
		g_hudElementLayouts[elementIndex + 126].y +
			g_hudElementLayouts[elementIndex + 126].clipHeightOrForegroundColor);
	g_flightFillClipRectFn();
	if (g_flightConfTickCounter != 0 && g_flightTickOverlayLastLoopTicks != 0 &&
		g_flightTickOverlayWindowTicks != 0 && g_flightTickOverlaySampleCount != 0) {
		FlightText_SetColor(78);
		sprintf(g_flightTextScratchBuffer, "%d %d",
				SIMULATION_TICKS_PER_SECOND / g_flightTickOverlayLastLoopTicks,
				SIMULATION_TICKS_PER_SECOND /
					(g_flightTickOverlayWindowTicks / g_flightTickOverlaySampleCount));
	}
#ifdef XVT_MODERN
	XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_CRAFT_STATUS, g_flightTextScratchBuffer,
							   XVT_COCKPIT_ALIGN_CENTER);
#endif
	FlightText_DrawStringCentered(g_flightTextScratchBuffer);
	FlightSurface_Unlock();

	if (g_flightPlayerCount > 1) {
		g_flightTextShadowEnabled = 0;
		FlightText_SetFontTier(0);
		offscreenPitchBytes = g_flight16bppBytesPerPixel * g_screenWidth;
		FlightSw_SetRenderTarget(g_flightOffscreenBuffer, g_screenWidth, g_screenHeight, offscreenPitchBytes);
		FlightText_SetClipRect(g_readyMessagePaneLeft - 2 * g_flightFontHalfHeight, g_readyMessagePaneTop,
							   2 * g_flightFontHalfHeight + g_readyMessagePaneRight + 1,
							   g_readyMessagePaneBottom);
		FlightText_SetCursor(g_readyMessagePaneLeft - 2 * g_flightFontHalfHeight, g_readyMessagePaneTop);
		switch (g_pingIndicator) {
			case 0:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(g_flightColorEscapeBypassChar);
				FlightText_SetShadowColor(g_flightColorEscapeBypassChar);
				break;
			case 1:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(78);
				break;
			case 2:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(75);
				break;
			case 3:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(74);
				break;
		}
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_NETWORK_PING, g_strCmdThreatDisplayText[16],
								   XVT_COCKPIT_ALIGN_LEFT);
#endif
		FlightText_DrawString(g_strCmdThreatDisplayText[16]);
		switch (g_lagIndicator) {
			case 0:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(g_flightColorEscapeBypassChar);
				FlightText_SetShadowColor(g_flightColorEscapeBypassChar);
				break;
			case 1:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(78);
				break;
			case 2:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(74);
				break;
			case 3:
				FlightText_SetBackgroundColor(65);
				FlightText_SetColor(75);
				break;
		}
#ifdef XVT_MODERN
		XvtCockpitText_RecordField(XVT_COCKPIT_TEXT_NETWORK_LAG, g_strCmdThreatDisplayText[17],
								   XVT_COCKPIT_ALIGN_RIGHT);
#endif
		FlightText_DrawStringRightAligned(g_strCmdThreatDisplayText[17]);
		FlightText_SetShadowColor(64);
		FlightSw_SetRenderTarget(NULL, 320, 200, 0);
	}
}

// FUNCTION: XVT 0x451930
uint16_t Hud_GetSystemMessagePaneState(void) { return g_systemMessagePane.stateOrMessageId; }

// FUNCTION: XVT 0x452960
void Hud_BlitSoftwareHudTextPanes(void) {
	uint16_t transparentColor;

#ifdef XVT_MODERN
	XvtCockpit_BeginMessagePlacement();
#endif
	transparentColor = g_flightColorEscapeBypassChar;
	if (g_flightPlayerCount >= 1) {
		FlightText_SetFontTier(0);
#ifdef XVT_MODERN
		XvtCockpit_LatchMessage(
			XVT_COCKPIT_MESSAGE_READY, g_readyMessagePaneLeft - 2 * g_flightFontHalfHeight,
			g_readyMessagePaneTop,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].y,
			g_readyMessagePaneRight + 4 * g_flightFontHalfHeight - g_readyMessagePaneLeft + 1,
			g_readyMessagePaneBottom - g_readyMessagePaneTop);
#endif
		Blit16ToFlightSurface(
			g_flightOffscreenBuffer, transparentColor, g_readyMessagePaneLeft - 2 * g_flightFontHalfHeight,
			g_readyMessagePaneTop,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].y,
			g_readyMessagePaneRight + 4 * g_flightFontHalfHeight - g_readyMessagePaneLeft + 1,
			g_readyMessagePaneBottom - g_readyMessagePaneTop, g_screenWidth * g_flight16bppBytesPerPixel);
	} else {
#ifdef XVT_MODERN
		XvtCockpit_LatchMessage(
			XVT_COCKPIT_MESSAGE_READY, g_readyMessagePaneLeft, g_readyMessagePaneTop,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].y,
			g_readyMessagePaneRight - g_readyMessagePaneLeft,
			g_readyMessagePaneBottom - g_readyMessagePaneTop);
#endif
		Blit16ToFlightSurface(
			g_flightOffscreenBuffer, g_flightColorEscapeBypassChar, g_readyMessagePaneLeft,
			g_readyMessagePaneTop,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].x,
			g_hudElementLayouts[g_hudInstrumentSetBaseIndex + HUD_MFD_MESSAGE_LOG_ELEMENT].y,
			g_readyMessagePaneRight - g_readyMessagePaneLeft,
			g_readyMessagePaneBottom - g_readyMessagePaneTop, g_screenWidth * g_flight16bppBytesPerPixel);
	}

	if (g_players[g_localPlayer].viewState.hudStateLive != HUD_VIEW_FULL_SCREEN) {
		if (g_systemMessagePane.stateOrMessageId != UINT16_MAX) {
#ifdef XVT_MODERN
			XvtCockpit_LatchMessage(XVT_COCKPIT_MESSAGE_SYSTEM, g_systemMessagePaneLeft,
									g_systemMessagePaneTop,
									g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].x,
									g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].y,
									g_systemMessagePaneRight - g_systemMessagePaneLeft,
									g_systemMessagePaneBottom - g_systemMessagePaneTop);
#endif
			Blit16ToFlightSurface(g_flightOffscreenBuffer, transparentColor, g_systemMessagePaneLeft,
								  g_systemMessagePaneTop,
								  g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].x,
								  g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].y,
								  g_systemMessagePaneRight - g_systemMessagePaneLeft,
								  g_systemMessagePaneBottom - g_systemMessagePaneTop,
								  g_screenWidth * g_flight16bppBytesPerPixel);
		}
		if (g_flightGroupMessagePane.stateOrMessageId != UINT16_MAX) {
#ifdef XVT_MODERN
			XvtCockpit_LatchMessage(XVT_COCKPIT_MESSAGE_FLIGHT_GROUP, g_flightGroupMessagePaneLeft,
									g_flightGroupMessagePaneTop,
									g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].x,
									g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].y,
									g_flightGroupMessagePaneRight - g_flightGroupMessagePaneLeft,
									g_flightGroupMessagePaneBottom - g_flightGroupMessagePaneTop);
#endif
			Blit16ToFlightSurface(g_flightOffscreenBuffer, transparentColor, g_flightGroupMessagePaneLeft,
								  g_flightGroupMessagePaneTop,
								  g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].x,
								  g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].y,
								  g_flightGroupMessagePaneRight - g_flightGroupMessagePaneLeft,
								  g_flightGroupMessagePaneBottom - g_flightGroupMessagePaneTop,
								  g_screenWidth * g_flight16bppBytesPerPixel);
		}
	} else {
		if (g_systemMessagePane.stateOrMessageId != UINT16_MAX) {
#ifdef XVT_MODERN
			XvtCockpit_LatchMessage(XVT_COCKPIT_MESSAGE_SYSTEM, g_systemMessagePaneLeft,
									g_systemMessagePaneTop,
									g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].x,
									g_flightVpHeight - 11, g_systemMessagePaneRight - g_systemMessagePaneLeft,
									g_systemMessagePaneBottom - g_systemMessagePaneTop);
#endif
			Blit16ToFlightSurface(g_flightOffscreenBuffer, transparentColor, g_systemMessagePaneLeft,
								  g_systemMessagePaneTop,
								  g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 118].x,
								  g_flightVpHeight - 11, g_systemMessagePaneRight - g_systemMessagePaneLeft,
								  g_systemMessagePaneBottom - g_systemMessagePaneTop,
								  g_screenWidth * g_flight16bppBytesPerPixel);
		}
		if (g_flightGroupMessagePane.stateOrMessageId != UINT16_MAX) {
#ifdef XVT_MODERN
			XvtCockpit_LatchMessage(
				XVT_COCKPIT_MESSAGE_FLIGHT_GROUP, g_flightGroupMessagePaneLeft, g_flightGroupMessagePaneTop,
				g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].x, g_flightVpHeight - 22,
				g_flightGroupMessagePaneRight - g_flightGroupMessagePaneLeft,
				g_flightGroupMessagePaneBottom - g_flightGroupMessagePaneTop);
#endif
			Blit16ToFlightSurface(
				g_flightOffscreenBuffer, transparentColor, g_flightGroupMessagePaneLeft,
				g_flightGroupMessagePaneTop, g_hudElementLayouts[g_hudInstrumentSetBaseIndex + 119].x,
				g_flightVpHeight - 22, g_flightGroupMessagePaneRight - g_flightGroupMessagePaneLeft,
				g_flightGroupMessagePaneBottom - g_flightGroupMessagePaneTop,
				g_screenWidth * g_flight16bppBytesPerPixel);
		}
	}
#ifdef XVT_MODERN
	XvtCockpit_LatchMessages();
#endif
}

// FUNCTION: XVT 0x452C40
uint16_t Hud_MeasureFlightMessagePaneText(int16_t paneType) {
	const char* text;
	char measuredText[80];
	char currentChar;
	unsigned int processedCount;
	uint16_t outputLength;

	if (paneType == 3 || paneType == 4 || paneType == 7)
		text = g_systemMessagePane.text;
	else if (paneType == 8)
		text = g_flightGroupMessagePane.text;
	else
		text = g_readyMessagePaneQueue[0].text;

	if ((uint8_t)text[0] < 9)
		++text;

	processedCount = 0;
	outputLength = 0;
	if (text[processedCount] != '\0') {
		do {
			if ((uint16_t)processedCount >= 70)
				break;
			currentChar = text[processedCount];
			if (currentChar == '[' || currentChar == ']') {
				if (currentChar == (char)0xFE)
					text += 2;
			} else {
				measuredText[outputLength++] = currentChar;
			}
			++processedCount;
		} while (text[processedCount] != '\0');
	}
	measuredText[outputLength] = '\0';

	return FlightText_MeasureStringWidth(measuredText);
}

// FUNCTION: XVT 0x497E00
void Hud_DrawBoxInXTrans(int x, int y, int width, int height, int colorIdx, int depth) {

	int bottom;
	int cornerWidth;
	int left;
	int cornerHeight;
	uint8_t* span;
	float spanDepth;

	left = x;
	bottom = y + height;
	if (bottom <= 0) {
		return;
	}
	if (left + width <= 0 || left >= g_flightVpWidth || y >= g_flightVpHeight || height <= 0 || width <= 0) {
		return;
	}
	if (g_useHardware3D != 0) {
		Hud_DrawBoxOverlayHW(left, y, width, height, colorIdx, depth);
		return;
	}

	cornerWidth = width >> 3;
	cornerHeight = height >> 3;
	if (cornerWidth < 3) {
		cornerWidth = 3;
	}
	if (cornerHeight < 3) {
		cornerHeight = 3;
	}
	if (cornerWidth > width) {
		cornerWidth = width;
	}
	if (height < cornerHeight) {
		cornerHeight = height;
	}

	span = g_panelBoxSpanScratch;
	if (g_flight16bppBytesPerPixel == 2) {
		uint16_t* span16;
		uint16_t color;
		int i;

		if (left > 0) {
			span = &g_panelBoxSpanScratch[2 * left];
		}
		span16 = (uint16_t*)span;
		color = g_flightTextPalette[colorIdx];
		for (i = 0; i < cornerWidth; ++i) {
			span16[i] = color;
		}
	} else {
		int i;

		if (left > 0) {
			span = &g_panelBoxSpanScratch[left];
		}
		for (i = 0; i < cornerWidth; ++i) {
			span[i] = (uint8_t)colorIdx;
		}
	}

	if (depth < 1) {
		depth = 1;
	}
	spanDepth = (float)(uint32_t)g_projScaleInt / (float)depth;
	if (g_flightSurfaceAlreadyLocked == 0) {
		FlightSurface_Lock();
	}

	if (y >= 0) {
		int spanStart;
		int spanEnd;

		spanStart = left;
		spanEnd = left + cornerWidth;
		if (spanEnd > 0 && g_flightVpWidth > left) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, y, spanDepth);
		}

		spanStart = width - cornerWidth + left;
		spanEnd = left + width;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, y, spanDepth);
		}
	}

	if (g_flightVpHeight >= bottom) {
		int spanStart;
		int spanEnd;

		spanStart = left;
		spanEnd = left + cornerWidth;
		if (spanEnd > 0 && g_flightVpWidth > left) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, bottom - 1, spanDepth);
		}

		spanStart = width - cornerWidth + left;
		spanEnd = left + width;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, bottom - 1, spanDepth);
		}
	}

	{
		int rowOffset;

		for (rowOffset = 1; rowOffset < cornerHeight; ++rowOffset) {
			int scanY;

			scanY = y + rowOffset;
			if (scanY >= 0 && g_flightVpHeight > scanY) {
				if (left >= 0) {
					sw3d_BlitOccludedSpan(span, left, left + 1, scanY, spanDepth);
				}
				if (left + width <= g_flightVpWidth) {
					sw3d_BlitOccludedSpan(span, left + width - 1, left + width, scanY, spanDepth);
				}
			}
		}
	}

	{
		int rowOffset;
		int lastRow;

		lastRow = height - 1;
		for (rowOffset = height - cornerHeight; rowOffset < lastRow; ++rowOffset) {
			if (rowOffset >= cornerHeight) {
				int scanY;

				scanY = y + rowOffset;
				if (scanY >= 0 && g_flightVpHeight > scanY) {
					if (left >= 0) {
						sw3d_BlitOccludedSpan(span, left, left + 1, scanY, spanDepth);
					}
					if (left + width <= g_flightVpWidth) {
						sw3d_BlitOccludedSpan(span, left + width - 1, left + width, scanY, spanDepth);
					}
				}
			}
		}
	}

	if (g_flightSurfaceAlreadyLocked == 0) {
		FlightSurface_Unlock();
	}
}

// FUNCTION: XVT 0x49C250
int16_t Hud_LoadPanelSpriteRecords(const char* fileName, uint16_t firstSpriteIndex, int16_t spriteCount,
								   uint16_t recordsToSkip) {
	int16_t remainingSprites;
	int16_t recordIndex;
	int16_t byteValue;
	XvtFile* stream;

	File_OpenGlobalStream(fileName, "rb", 1, 0);
	remainingSprites = spriteCount;
	stream = g_stream;
#ifdef XVT_MODERN
	if (!stream)
		return 1;
#endif
	recordIndex = 0;
	while (remainingSprites != 0) {
		g_hudPanelSpriteDataByIndex[firstSpriteIndex] = g_hudPanelSpriteDataWriteCursor;
		if (recordIndex >= (int)recordsToSkip) {
			++firstSpriteIndex;
		}
#ifdef XVT_MODERN
		for (byteValue = (int16_t)File_Getc(stream); !File_Eof(stream) && !File_HasError(stream);
			 byteValue = (int16_t)File_Getc(stream)) {
#else
		for (byteValue = (int16_t)File_Getc(stream); (((Msvc42IconFilePrefix*)stream)->flags & 0x10) == 0;
			 byteValue = (int16_t)File_Getc(stream)) {
#endif
			if (byteValue == 0xff) {
				break;
			}
			if (recordIndex >= (int)recordsToSkip) {
				*g_hudPanelSpriteDataWriteCursor = (uint8_t)byteValue;
				++g_hudPanelSpriteDataWriteCursor;
			}
		}
#ifdef XVT_MODERN
		if (File_HasError(stream)) {
			FeDiskIo_CloseGlobalStream(0);
			XvtStorage_Fatal("Cannot read panel sprites", 1);
			return 1;
		}
#endif
		if (recordIndex >= (int)recordsToSkip) {
			--remainingSprites;
			*g_hudPanelSpriteDataWriteCursor = 0xff;
			++g_hudPanelSpriteDataWriteCursor;
		}
		++recordIndex;
	}
#ifdef XVT_MODERN
	XvtRenderAssets_RegisterPanel(fileName, (uint16_t)(firstSpriteIndex - spriteCount), (uint16_t)spriteCount,
								  recordsToSkip);
#endif
	return FeDiskIo_CloseGlobalStream(0);
}

// FUNCTION: XVT 0x49C330
int FlightIcon_LoadFrames(char* fileName, uint8_t* dataBuffer, uint8_t** framePointers) {
	int16_t frameCount;
	XvtFile* stream;
	int16_t value;
#ifndef XVT_MODERN
	int* streamFlags;
#endif

	if (File_OpenGlobalStream(fileName, "rb", 1, 0) == 0) {
		return 0;
	}
	frameCount = 0;
	stream = g_stream;
#ifdef XVT_MODERN
	while (1) {
		if (File_HasError(stream)) {
			FeDiskIo_CloseGlobalStream(0);
			XvtStorage_Fatal("Cannot read icon frames", 1);
			return 0;
		}
		if (File_Eof(stream)) {
			break;
		}
		framePointers[frameCount] = dataBuffer;
		while (1) {
			value = (int16_t)File_Getc(stream);
			if (File_HasError(stream)) {
				FeDiskIo_CloseGlobalStream(0);
				XvtStorage_Fatal("Cannot read icon frames", 1);
				return 0;
			}
			if (File_Eof(stream)) {
				break;
			}
			if (value == 0xff) {
				break;
			}
			*dataBuffer = (uint8_t)value;
			++dataBuffer;
		}
		++frameCount;
		*dataBuffer = 0xff;
		++dataBuffer;
	}
#else
	streamFlags = &((Msvc42IconFilePrefix*)stream)->flags;
	for (; (*streamFlags & 0x10) == 0; ++dataBuffer) {
		framePointers[frameCount] = dataBuffer;
		for (value = (int16_t)File_Getc(stream); (*streamFlags & 0x10) == 0;
			 value = (int16_t)File_Getc(stream)) {
			if (value == 0xff) {
				break;
			}
			*dataBuffer = (uint8_t)value;
			++dataBuffer;
		}
		++frameCount;
		*dataBuffer = 0xff;
	}
#endif
	FeDiskIo_CloseGlobalStream(0);
#ifdef XVT_MODERN
	XvtRenderAssets_RegisterIcons(fileName, framePointers, (uint16_t)frameCount);
#endif
	return frameCount;
}
