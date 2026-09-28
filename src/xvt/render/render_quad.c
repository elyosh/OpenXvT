#include "xvt/render/render_quad.h"

#ifdef XVT_MODERN
#include "aeron/compat/host.h"
#endif

#include "xvt/assets/object_type.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_view.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/transfm2.h"
#include "xvt/math/math.h"
#include "xvt/math/trig2.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/render_clip.h"
#include "xvt/render/render_scene.h"
#include "xvt/render/render_texture.h"
#include "xvt/render/renderer.h"
#include "xvt/render/scene_billboard.h"
#include "xvt/render/std3d.h"
#include "xvt/render/tex_level.h"
#include "xvt/util/debug_console.h"
#include "xvt/util/memory.h"

#include <string.h>

// GLOBAL: XVT 0x51A558
const uint32_t g_explosionBillboardColorByFrame[32] = {
	0xd0ffffff, 0xe0ffffff, 0xf0ffffff, 0xf0ffffff, 0xe0ffffff, 0xd0ffffff, 0xb0ffffff, 0x90ffffff,
	0x70ffffff, 0x50ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff,
	0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff,
	0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff, 0x30ffffff,
};

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x401450
void RenderQuad_DrawModelTexture(SceneBillboardQueueEntry* quadRecord) {
	uint16_t frame;
	uint16_t modelType;
	uint16_t screenSize;
	uint16_t handle;
	int savedTargetY;
	ObjectRecord* object;
	const uint8_t* modelData;
	const TexLevelHeader* textureHeader;
	SpritePayload* sprite;

	frame = (uint16_t)quadRecord->frame & 0x7FFFu;
	g_flightSwRotSpriteSpanRunsEnabled = 1;
	modelType = frame >> 7;
	g_billboardObjectOrTypeIndex = quadRecord->objectOrTypeIndex;
	object = &g_objectTable[g_billboardObjectOrTypeIndex];
	savedTargetY = g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldX = object->world_x - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = object->world_y - savedTargetY;
	g_camRelWorldZ = object->world_z - g_players[g_localPlayer].viewState.savedTargetZ;
	depthZ = quadRecord->depthZ;
	screenSize = (uint16_t)SceneBillboard_ComputeProjectedSize(
		quadRecord->depthZ, (uint16_t)g_modelTypeTable[modelType].maxBoundsExtent,
		(uint16_t)quadRecord->screenSize);
	handle = g_modelTypeTable[modelType].curTexLevel;
	modelData = (const uint8_t*)Memory_LockHandle(handle);
	frame &= 0x7Fu;
	Memory_UnlockHandle(handle);
	textureHeader = (const TexLevelHeader*)modelData;
	sprite =
		(SpritePayload*)(modelData + *(const uint32_t*)(modelData + textureHeader->imageOffsetTableOffset +
														frame * sizeof(uint32_t)));
	if (g_useHardware3D != 0)
		RenderQuad_DrawRotatedSprite(quadRecord->rotationAngle, quadRecord->screenX, quadRecord->screenY,
									 screenSize, sprite);
	else {
		FlightSw_PrepareSpriteRotationTables(quadRecord->rotationAngle, FLIGHT_SW_16BPP_BYTES_PER_PIXEL);
		FlightSw_BuildSpriteTintRemapTables(sprite);
		FlightSw_DrawRotatedSpriteQuad(quadRecord->screenX, quadRecord->screenY, screenSize, sprite);
	}
}

// FUNCTION: XVT 0x40BBF0
void RenderQuad_DrawRotatedSprite(int angle, int screenX, int screenY, uint16_t screenSize,
								  const void* textureLevel) {
	enum {
		EXPLOSION_FRAME_COUNT = 32,
		CLIP_VERTEX_CAPACITY = 40,
		MAX_TEXTURE_DIMENSION = 256,
		TEXTURE_DIMENSION_STEPS = 8,
		TEXTURE_SCALE_SHIFT = 9,
		INITIAL_QUAD_VERTEX_COUNT = 4,
		MIN_TRIANGLE_VERTEX_COUNT = 3,
		TRIANGLE_FAN_FIRST_INDEX = 2,
		SPRITE_BASE_RENDER_FLAGS = 2066,
		BILINEAR_RENDER_FLAGS = 384,
		SPRITE_ALPHA_RENDER_FLAGS = 512
	};

	const uint8_t* textureBytes;
	const TexLevelImageHeader* imageHeader;
	uint32_t color;
	float computedDepth;
	float depth;
	int sourceWidth;
	int sourceHeight;
	int powerOfTwoWidth;
	int powerOfTwoHeight;
	int width;
	int height;
	float maxU;
	float maxV;
	int halfWidth;
	int halfHeight;
	int negativeHalfHeight;
	int negativeHalfWidth;
	int xOffset;
	int yOffset;
	RenderClipVertex vertices[CLIP_VERTEX_CAPACITY];
	int previousIndex;
	int vertexIndex;
	uint32_t vertexColor;
	uint32_t vertexSpecular;
	uint16_t* palette;
	const uint8_t* pixels;
	int rleFormat;
	Std3DTexCacheNode* texture;

#ifdef XVT_MODERN
	/* Suppress before texture lookup so hidden classic draws do not refill the cache. */
	if (AeronDx5_IsClassicFlightRenderingSuppressed())
		return;
#endif

	textureBytes = (const uint8_t*)textureLevel;
	imageHeader = (const TexLevelImageHeader*)textureLevel;
	screenY = g_flightVpHeight - screenY;
#ifdef XVT_MODERN
	color = UINT32_MAX;
#endif
	if (g_billboardObjectOrTypeIndex >= 0 &&
		(unsigned int)g_regionMainObjectSlotEnd > (unsigned int)g_billboardObjectOrTypeIndex) {
		ObjectRecord* object;
		int frame;

		object = &g_objectTable[g_billboardObjectOrTypeIndex];
		if (object->genusId == CRAFT_GENUS_EXPLOSION) {
			frame = object->typeSpecificByte[0];
			if (frame >= 0 && frame < EXPLOSION_FRAME_COUNT)
				color = g_explosionBillboardColorByFrame[frame];
			else
				color = UINT32_MAX;
		} else {
			color = UINT32_MAX;
		}
	}

	if ((unsigned int)depthZ > 0x1000000u) {
		color = UINT32_MAX;
		computedDepth = 0.00012205541f;
		if (g_std3DZBufferBitDepth == 2)
			computedDepth = 0.99987793f;
	} else {
		computedDepth = g_renderUnitFloat / ((float)depthZ * g_invDepthProjScale + g_renderUnitFloat);
		if (g_std3DZBufferBitDepth == 2)
			computedDepth = g_renderUnitFloat - computedDepth;
	}
	depth = computedDepth;
	sourceWidth = (int32_t)imageHeader->width;
	sourceHeight = (int32_t)imageHeader->height;
	if (sourceWidth > MAX_TEXTURE_DIMENSION) {
		sourceWidth = MAX_TEXTURE_DIMENSION;
		DebugPrintf("TRUNCATING BITMAP TO 256 WIDE!!!\n");
	}
	if (sourceHeight > MAX_TEXTURE_DIMENSION) {
		sourceHeight = MAX_TEXTURE_DIMENSION;
		DebugPrintf("TRUNCATING BITMAP TO 256 HIGH!!!\n");
	}
	maxU = (float)sourceWidth;
	maxV = (float)sourceHeight;

	powerOfTwoWidth = 1;
	vertexIndex = 0;
	do {
		powerOfTwoWidth *= 2;
		if (powerOfTwoWidth >= sourceWidth)
			break;
		++vertexIndex;
	} while (vertexIndex < TEXTURE_DIMENSION_STEPS);
	powerOfTwoHeight = 1;
	for (vertexIndex = 0; vertexIndex < TEXTURE_DIMENSION_STEPS; ++vertexIndex) {
		powerOfTwoHeight *= 2;
		if (powerOfTwoHeight >= sourceHeight)
			break;
	}
	if (g_pStd3DCurDevice->caps.bSquareOnlyTexture != 0) {
		if (powerOfTwoWidth > powerOfTwoHeight)
			powerOfTwoHeight = powerOfTwoWidth;
		else if (powerOfTwoHeight > powerOfTwoWidth)
			powerOfTwoWidth = powerOfTwoHeight;
	}
	width = powerOfTwoWidth;
	height = powerOfTwoHeight;

	maxU /= (float)width;
	maxV /= (float)height;
	halfWidth = (screenSize * (int32_t)imageHeader->width) >> TEXTURE_SCALE_SHIFT;
	halfHeight = (screenSize * (int32_t)imageHeader->height) >> TEXTURE_SCALE_SHIFT;
	angle = (uint16_t)angle;
	xOffset = trig2_cosinedwordmult(halfWidth, angle) + trig2_sinedwordmult(halfHeight, angle);
	yOffset = trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(halfWidth, angle);

	g_clipCountA = INITIAL_QUAD_VERTEX_COUNT;
	g_clipVertCursor = INITIAL_QUAD_VERTEX_COUNT;
	g_clipIdxA[0] = 0;
	g_clipIdxA[1] = 1;
	g_clipIdxA[2] = 2;
	g_clipIdxA[3] = 3;

	vertices[0].x = (float)(screenX + xOffset);
	vertices[0].y = (float)(screenY + yOffset);
	vertices[0].z = depth;
	memset(&vertices[0].rhw, 0, sizeof(float) * 3);
	negativeHalfWidth = -halfWidth;
	xOffset = trig2_cosinedwordmult(negativeHalfWidth, angle) + trig2_sinedwordmult(halfHeight, angle);
	yOffset = trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(negativeHalfWidth, angle);
	vertices[1].x = (float)(screenX + xOffset);
	vertices[1].y = (float)(screenY + yOffset);
	vertices[1].z = depth;
	vertices[1].rhw = 0.0f;
	vertices[1].u = maxU;
	vertices[1].v = 0.0f;
	negativeHalfHeight = -halfHeight;
	xOffset =
		trig2_cosinedwordmult(negativeHalfWidth, angle) + trig2_sinedwordmult(negativeHalfHeight, angle);
	yOffset =
		trig2_cosinedwordmult(negativeHalfHeight, angle) - trig2_sinedwordmult(negativeHalfWidth, angle);
	vertices[2].x = (float)(screenX + xOffset);
	vertices[2].y = (float)(screenY + yOffset);
	vertices[2].z = depth;
	vertices[2].rhw = 0.0f;
	vertices[2].u = maxU;
	vertices[2].v = maxV;
	xOffset = trig2_cosinedwordmult(halfWidth, angle) + trig2_sinedwordmult(negativeHalfHeight, angle);
	yOffset = trig2_cosinedwordmult(negativeHalfHeight, angle) - trig2_sinedwordmult(halfWidth, angle);
	vertices[3].x = (float)(screenX + xOffset);
	vertices[3].y = (float)(screenY + yOffset);
	vertices[3].z = depth;
	vertices[3].rhw = 0.0f;
	vertices[3].u = 0.0f;
	vertices[3].v = maxV;

	g_clipCountB = 0;
	previousIndex = g_clipIdxA[g_clipCountA - 1];
	for (vertexIndex = 0; vertexIndex < g_clipCountA; ++vertexIndex) {
		int currentIndex;

		currentIndex = g_clipIdxA[vertexIndex];
		RenderClip_ClipPolyTop(previousIndex, currentIndex, vertices);
		previousIndex = currentIndex;
	}
	g_clipCountA = 0;
	/* A clipping pass can discard every vertex before the next pass. */
#ifdef XVT_MODERN
	if (g_clipCountB > 0) {
#endif
		previousIndex = g_clipIdxB[g_clipCountB - 1];
		for (vertexIndex = 0; vertexIndex < g_clipCountB; ++vertexIndex) {
			int currentIndex;

			currentIndex = g_clipIdxB[vertexIndex];
			RenderClip_ClipPolyBottom(previousIndex, currentIndex, vertices);
			previousIndex = currentIndex;
		}
#ifdef XVT_MODERN
	}
#endif
	g_clipCountB = 0;
#ifdef XVT_MODERN
	if (g_clipCountA > 0) {
#endif
		previousIndex = g_clipIdxA[g_clipCountA - 1];
		for (vertexIndex = 0; vertexIndex < g_clipCountA; ++vertexIndex) {
			int currentIndex;

			currentIndex = g_clipIdxA[vertexIndex];
			RenderClip_ClipPolyLeft(previousIndex, currentIndex, vertices);
			previousIndex = currentIndex;
		}
#ifdef XVT_MODERN
	}
#endif
	g_clipCountA = 0;
#ifdef XVT_MODERN
	if (g_clipCountB > 0) {
#endif
		previousIndex = g_clipIdxB[g_clipCountB - 1];
		for (vertexIndex = 0; vertexIndex < g_clipCountB; ++vertexIndex) {
			int currentIndex;

			currentIndex = g_clipIdxB[vertexIndex];
			RenderClip_ClipPolyRight(previousIndex, currentIndex, vertices);
			previousIndex = currentIndex;
		}
#ifdef XVT_MODERN
	}
#endif
	if (g_clipCountA < MIN_TRIANGLE_VERTEX_COUNT)
		return;

	if (g_clipCountA + g_d3dVertexCount > g_maxBatchVerts ||
		g_clipCountA + g_d3dIndexCount > g_maxBatchTris) {
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
	if (g_capVertexAlpha != 0) {
		color = 0xfeffffff;
		g_capVertexAlpha = 0;
	}
	vertexColor = color;
	vertexSpecular = 0;
	for (vertexIndex = 0; vertexIndex < g_clipCountA; ++vertexIndex) {
		int sourceIndex;
		float sourceY;
		float sourceDepth;
		float sourceU;
		float sourceV;

		sourceIndex = g_clipIdxA[vertexIndex];
		sourceY = vertices[sourceIndex].y;
		sourceU = vertices[sourceIndex].u;
		sourceV = vertices[sourceIndex].v;
		sourceDepth = vertices[sourceIndex].z;
		g_flightVertexBuffer[g_d3dVertexCount].sx = vertices[sourceIndex].x + g_flightVpOriginX;
		g_flightVertexBuffer[g_d3dVertexCount].sy = sourceY + g_flightVpOriginY;
		g_flightVertexBuffer[g_d3dVertexCount].sz = sourceDepth;
		g_flightVertexBuffer[g_d3dVertexCount].rhw = sourceDepth;
		g_flightVertexBuffer[g_d3dVertexCount].tu = sourceU;
		g_flightVertexBuffer[g_d3dVertexCount].tv = sourceV;
		g_flightVertexBuffer[g_d3dVertexCount].color = vertexColor;
		g_flightVertexBuffer[g_d3dVertexCount].specular = vertexSpecular;
		g_clipIdxA[vertexIndex] = g_d3dVertexCount;
		++g_d3dVertexCount;
	}
	palette = (uint16_t*)(textureBytes + imageHeader->palette8Offset);
	pixels = textureBytes + imageHeader->encodedImageOffset + 16;
	rleFormat = (int32_t)imageHeader->packingMode;
	texture = RenderTexture_GetOrCreateBitmap(width, height, palette, pixels, rleFormat);
	for (vertexIndex = TRIANGLE_FAN_FIRST_INDEX; vertexIndex < g_clipCountA; ++vertexIndex) {
		g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
		g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[vertexIndex - 1];
		g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[vertexIndex];
		g_triBuffer[g_d3dIndexCount].texture = texture;
		g_triBuffer[g_d3dIndexCount].flags = (Std3DRenderStateFlags)SPRITE_BASE_RENDER_FLAGS;
		if (g_bilinearEnabled != 0)
			g_triBuffer[g_d3dIndexCount].flags += BILINEAR_RENDER_FLAGS;
		g_triBuffer[g_d3dIndexCount++].flags += SPRITE_ALPHA_RENDER_FLAGS;
	}
}
