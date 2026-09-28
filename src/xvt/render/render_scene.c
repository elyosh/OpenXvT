#include "xvt/render/render_scene.h"
#ifdef XVT_MODERN
#include "aeron/compat/host.h"
#include "xvt_runtime/assets/opt_native.h"
#endif

#include "xvt/assets/model_mesh.h"
#include "xvt/assets/model_preview.h"
#include "xvt/assets/model_texture.h"
#include "xvt/assets/opt_model.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight_display.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/targeting.h"
#include "xvt/flight/transfm2.h"
#include "xvt/math/math.h"
#include "xvt/math/math3d.h"
#include "xvt/render/flight_light.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/render_clip.h"
#include "xvt/render/render_texture.h"
#include "xvt/render/renderer.h"
#include "xvt/render/scene_billboard.h"
#include "xvt/render/std3d.h"
#include "xvt/render/sw3d.h"
#include "xvt/util/debug_console.h"
#include "xvt/util/memory.h"
#include <string.h>
#ifndef XVT_MODERN
#include <float.h>
#endif

enum {
	OPT_INDEXED_SHADE_TABLE_SIZE = 4096,
	DEFAULT_WHITE_TEXTURE_DIMENSION = 8,
	DEFAULT_WHITE_TEXTURE_RGB_SIZE = DEFAULT_WHITE_TEXTURE_DIMENSION * DEFAULT_WHITE_TEXTURE_DIMENSION * 3,
	B_WING_OBJECT_TYPE = 4,
	COMPONENT_OBJECT_TYPE = 89,
};

// GLOBAL: XVT 0x51A520
int g_sceneFlushDrawTargetMarkers = 0;

// GLOBAL: XVT 0x5272A0
int g_bwingBridgeMeshIndexCache = -1;
// GLOBAL: XVT 0x5272A4
static int g_vertexLightOcclusionEnabled;
// GLOBAL: XVT 0x5181D0
const float g_meshRotationByteToRadiansScale = 0.024543673f;
// GLOBAL: XVT 0x5181D8
const float g_renderMatrixQ15ToFloatScale = 0.000030518509f;
// GLOBAL: XVT 0x5181DC
const float g_optAxisQ15ToFloatScale = 0.000030517578125f;
// GLOBAL: XVT 0x51807C
const float g_renderDistantDepth = 100000.0f;
// GLOBAL: XVT 0x518054
const float g_renderProjectionZeroFloat = 0.0f;
// GLOBAL: XVT 0x518060
const float g_renderUnitFloat = 1.0f;
// GLOBAL: XVT 0x518070
const float g_renderTextureUvHalfScale = 0.5f;
// GLOBAL: XVT 0x518074
const float g_renderTriangleCornerCount = 3.0f;
// GLOBAL: XVT 0x518078
const float g_renderQuadCornerCount = 4.0f;
// GLOBAL: XVT 0x518080
const float g_invDepthProjScale = 0.00048828125f;
// GLOBAL: XVT 0x518098
const float g_renderLightDirectionUnitScale = 0.000030517578125f;
// GLOBAL: XVT 0x51809C
const float g_renderDirectionalLightIntensityScale = 0.80000001f;
// GLOBAL: XVT 0x5180A0
const float g_renderZeroFloat = 0.0f;
// GLOBAL: XVT 0x5180A4
const float g_renderAmbientLightIntensity = 0.40000001f;
// GLOBAL: XVT 0x5180A8
const float g_renderRoughDistanceScale = 0.29409999f;
// GLOBAL: XVT 0x5180AC
const float g_renderPointLightFacingThreshold = -0.30000001f;
// GLOBAL: XVT 0x5180B0
const double g_renderHalfDouble = 0.5;
// GLOBAL: XVT 0x5180B8
const float g_renderHalfFloat = 0.5f;
// GLOBAL: XVT 0x5180BC
const float g_renderSpecularApproxOtherComponentsScale = 0.1936f;
// GLOBAL: XVT 0x5180C0
const float g_renderSpecularApproxMaxComponentScale = 0.4632f;
// GLOBAL: XVT 0xA68748
IDirectDrawSurface* g_std3DZBufferSurface;
// GLOBAL: XVT 0x51A54C
int g_capVertexAlpha = 1;
// GLOBAL: XVT 0x51A550
int g_d3dVertexAlphaStateResetSlot = 0;
// GLOBAL: XVT 0x52F868
int g_maxBatchTris = 0;
// GLOBAL: XVT 0x53F970
D3DTLVERTEX* g_flightVertexBuffer = NULL;
// GLOBAL: XVT 0x53F974
float g_flightVpOriginY = 0.0f;
// GLOBAL: XVT 0x54F988
Std3DRenderTri* g_triBuffer = NULL;
// GLOBAL: XVT 0x54F98C
int g_clipInputProjVertEndIndex = 0;
// GLOBAL: XVT 0x54F990
int g_maxBatchVerts = 0;
// GLOBAL: XVT 0x54F994
int g_d3dVertexCount = 0;
// GLOBAL: XVT 0x54F9A0
int g_d3dIndexCount = 0;
// GLOBAL: XVT 0x54F9A4
float g_flightVpOriginX = 0.0f;
// GLOBAL: XVT 0x999420
static SceneSpan* g_sceneSpanDataBase = NULL;
// GLOBAL: XVT 0x5270B4
static uint8_t g_bBackdropMeshMode = 0;
// GLOBAL: XVT 0x5271D4
int g_curLayerId = 0;
// GLOBAL: XVT 0x5271D8
OptTextureData* g_defaultWhiteTextureDescPtr = NULL;
// GLOBAL: XVT 0x5271E0
uint8_t g_defaultWhiteTextureRgb24[DEFAULT_WHITE_TEXTURE_RGB_SIZE] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
// GLOBAL: XVT 0x60F1EC
OptTextureData* g_curTextureDesc = NULL;
// GLOBAL: XVT 0x60F210
ModelTextureDefaultTexture g_defaultWhiteTexture = { 0 };
// GLOBAL: XVT 0x999446
static int g_phongSlotIndex = 0;
// GLOBAL: XVT 0x999410
static int g_phongSlotStride = 0;
// GLOBAL: XVT 0x999424
uint16_t g_sceneSpanDataHandle = 0;
// GLOBAL: XVT 0x999426
int g_sceneSpanDataCapacity = 0;
// GLOBAL: XVT 0x99942A
SceneSpan* g_pSceneSpanDataCur = NULL;
// GLOBAL: XVT 0x99942E
SceneSpan* g_pSceneSpanDataEnd = NULL;
// GLOBAL: XVT 0x999432
SceneSpan** g_sceneSpanPtrList = NULL;
// GLOBAL: XVT 0x999436
uint16_t g_sceneSpanPtrListHandle = 0;
// GLOBAL: XVT 0x999438
static int g_sceneSpanPtrCapacity = 0;
// GLOBAL: XVT 0x99943C
int g_sceneSpanPtrAvail = 0;
// GLOBAL: XVT 0x999440
static uint8_t* g_scenePhongData = NULL;
// GLOBAL: XVT 0x999444
uint16_t g_scenePhongDataHandle = 0;
// GLOBAL: XVT 0x99944A
SceneFace* g_visFaceList = NULL;
// GLOBAL: XVT 0x55635C
int g_renderSceneResetPending = 0;
// GLOBAL: XVT 0x999450
int g_visFaceCount = 0;
// GLOBAL: XVT 0x999454
int g_visFacePassStart = 0;
// GLOBAL: XVT 0x999458
static int g_sceneFaceMax = 0;
// GLOBAL: XVT 0x99944E
uint16_t g_visFaceListHandle = 0;
// GLOBAL: XVT 0x99945C
ProjVertex* g_projVertList = NULL;
// GLOBAL: XVT 0x999462
int g_projVertCount = 0;
// GLOBAL: XVT 0x999466
static int g_projVertMax = 0;
// GLOBAL: XVT 0x999460
uint16_t g_projVertListHandle = 0;
// GLOBAL: XVT 0x99946A
SceneEdge* g_sceneEdgeList = NULL;
// GLOBAL: XVT 0x99946E
uint16_t g_sceneEdgeListHandle = 0;
// GLOBAL: XVT 0x999470
int g_sceneEdgeCursor = 0;
// GLOBAL: XVT 0x999474
static int g_sceneEdgeMax = 0;
// GLOBAL: XVT 0x999478
int* g_vertexRemap = NULL;
// GLOBAL: XVT 0x99947C
uint16_t g_vertexRemapHandle = 0;
// GLOBAL: XVT 0x99947E
int g_vertexRemapCapacity = 0;
// GLOBAL: XVT 0x999482
int* g_sceneEdgeFlags = NULL;
// GLOBAL: XVT 0x999486
uint16_t g_sceneEdgeFlagsHandle = 0;
// GLOBAL: XVT 0x999488
int g_sceneEdgeFlagsCapacity = 0;
// GLOBAL: XVT 0x99948C
static SceneEdge** g_sceneSclEdgeList = NULL;
// GLOBAL: XVT 0x999490
uint16_t g_sceneSclEdgeListHandle = 0;
// GLOBAL: XVT 0x999492
SceneSpan** g_scanlineSpanHeads = NULL;
// GLOBAL: XVT 0x999496
uint16_t g_scanlineSpanHeadsHandle = 0;
// GLOBAL: XVT 0x999498
static SceneMesh* g_meshQueue = NULL;
// GLOBAL: XVT 0x99949C
uint16_t g_meshQueueHandle = 0;
// GLOBAL: XVT 0x99949E
static int g_meshQueueMax = 0;
// GLOBAL: XVT 0x9994A2
static int g_meshQueueIndex = 0;
// GLOBAL: XVT 0x9994B0
OptVector g_meshEyePos = { 0.0f, 0.0f, 0.0f };
// GLOBAL: XVT 0x9994BC
uint8_t* g_activeRgb565ToPaletteIndexLut = NULL;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x4084E0
void RenderScene_ProjectMeshVertices(SceneMesh* mesh) {
	SceneFace* face = &g_visFaceList[mesh->faceBaseIndex];
	ProjVertex* output;
	int vertexIndex;
	int faceIndex;

	mesh->vertBaseIndex = g_projVertCount;
	output = &g_projVertList[mesh->vertBaseIndex];
	mesh->projVertCursor = 0;
	for (vertexIndex = 0; vertexIndex < mesh->vertexCount; ++vertexIndex) {
		g_vertexRemap[vertexIndex] = -1;
	}
	for (faceIndex = 0; faceIndex < mesh->visFaceCount; ++face, ++faceIndex) {
		OptVector transformed;
		const FaceRecord* geometry;
		float totalW;
		float c00;
		float c01;
		float c02;
		float c10;
		float c11;
		float c12;
		float c20;
		float c21;
		float c22;
		float inverse;
		float scaled;
		float area;
		int cornerIndex;

		RenderScene_TransformFaceTextureGradients(face, &mesh->pFaceTexturing[face->faceIndex],
												  &mesh->viewPosX);
		geometry = &mesh->pFaceGeom[face->faceIndex];
		face->maxVertW = 0.0f;
		totalW = 0.0f;
		face->minVertW = (float)(unsigned int)g_projScaleInt;
		for (cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
			const int modelVertexIndex = geometry->vertexIdx[cornerIndex];
			const int uvIndex = geometry->uvIdx[cornerIndex];
			const int normalIndex = geometry->normalIdx[cornerIndex];
			int remappedVertex;
			float vertexW;

			if (modelVertexIndex == -1) {
				break;
			}
			remappedVertex = g_vertexRemap[modelVertexIndex];
			if (remappedVertex == -1) {
				g_vertexRemap[modelVertexIndex] = mesh->projVertCursor;
				++mesh->projVertCursor;
				transformed.x = mesh->pModelVerts[modelVertexIndex].x;
				transformed.y = mesh->pModelVerts[modelVertexIndex].y;
				transformed.z = mesh->pModelVerts[modelVertexIndex].z;
				Math3D_RotateVec3(&transformed.x, mesh->viewOrient);
				transformed.x += mesh->viewPosX;
				transformed.y += mesh->viewPosY;
				transformed.z += mesh->viewPosZ;
				if (transformed.z < g_renderUnitFloat) {
					output->w = transformed.z - g_renderUnitFloat;
					output->sx = transformed.x;
					output->sy = transformed.y;
					face->nearClipState = -1;
					vertexW = (float)(unsigned int)g_projScaleInt;
				} else {
					output->w = (float)(unsigned int)g_projScaleInt / transformed.z;
					output->sx = output->w * transformed.x;
					output->sy = output->w * transformed.y;
					output->sx += (float)(g_flightVpWidth >> 1);
					output->sy += (float)(g_projOffsetY + (g_flightVpHeight >> 1));
					vertexW = output->w;
				}
				RenderScene_ComputeVertexLighting(mesh, output, &mesh->pVertNormals[normalIndex],
												  &mesh->pModelVerts[modelVertexIndex], &g_meshEyePos);
				output->tu = mesh->pUVs[uvIndex].u;
				output->tv = mesh->pUVs[uvIndex].v;
				++output;
			} else {
				const ProjVertex* projected = &g_projVertList[mesh->vertBaseIndex + remappedVertex];
				if (projected->w < 0.0f) {
					face->nearClipState = -1;
					vertexW = (float)(unsigned int)g_projScaleInt;
				} else {
					vertexW = projected->w;
				}
			}
			totalW += vertexW;
			if (face->maxVertW < vertexW) {
				face->maxVertW = vertexW;
			}
			if (face->minVertW > vertexW) {
				face->minVertW = vertexW;
			}
		}

		if (mesh->pUVs != NULL) {
			const int uvIndex = geometry->uvIdx[0];
			const int modelVertexIndex = geometry->vertexIdx[0];

			transformed.x = mesh->pModelVerts[modelVertexIndex].x;
			transformed.y = mesh->pModelVerts[modelVertexIndex].y;
			transformed.z = mesh->pModelVerts[modelVertexIndex].z;
			Math3D_RotateVec3(&transformed.x, mesh->viewOrient);
			transformed.x += mesh->viewPosX;
			transformed.y += mesh->viewPosY;
			transformed.z += mesh->viewPosZ;
			face->gradients[6] = transformed.x - mesh->pUVs[uvIndex].v * face->gradients[3] -
								 mesh->pUVs[uvIndex].u * face->gradients[0];
			face->gradients[7] = transformed.y - mesh->pUVs[uvIndex].v * face->gradients[4] -
								 mesh->pUVs[uvIndex].u * face->gradients[1];
			face->gradients[8] = transformed.z - mesh->pUVs[uvIndex].v * face->gradients[5] -
								 mesh->pUVs[uvIndex].u * face->gradients[2];
			c00 = face->gradients[8] * face->gradients[4] - face->gradients[5] * face->gradients[7];
			c01 = face->gradients[5] * face->gradients[6] - face->gradients[8] * face->gradients[3];
			c02 = face->gradients[7] * face->gradients[3] - face->gradients[4] * face->gradients[6];
			c10 = face->gradients[2] * face->gradients[7] - face->gradients[8] * face->gradients[1];
			c11 = face->gradients[8] * face->gradients[0] - face->gradients[2] * face->gradients[6];
			c12 = face->gradients[6] * face->gradients[1] - face->gradients[7] * face->gradients[0];
			c20 = face->gradients[5] * face->gradients[1] - face->gradients[2] * face->gradients[4];
			c21 = face->gradients[2] * face->gradients[3] - face->gradients[5] * face->gradients[0];
			c22 = face->gradients[4] * face->gradients[0] - face->gradients[1] * face->gradients[3];
			if (c20 == 0.0f && c21 == 0.0f && c22 == 0.0f) {
				c22 = 1.0f;
			}
			inverse = g_renderUnitFloat /
					  (c20 * face->gradients[6] + c21 * face->gradients[7] + c22 * face->gradients[8]);
			scaled = inverse * g_invProjScale;
			face->gradients[0] = scaled * c00;
			face->gradients[1] = scaled * c01;
			face->gradients[2] = inverse * c02;
			face->gradients[3] = scaled * c10;
			face->gradients[4] = scaled * c11;
			face->gradients[5] = inverse * c12;
			face->gradients[6] = scaled * c20;
			face->gradients[7] = scaled * c21;
			face->gradients[8] = inverse * c22;
			face->gradients[2] -= (float)(g_flightVpWidth >> 1) * face->gradients[0];
			face->gradients[2] -= (float)(g_projOffsetY + (g_flightVpHeight >> 1)) * face->gradients[1];
			face->gradients[5] -= (float)(g_flightVpWidth >> 1) * face->gradients[3];
			face->gradients[5] -= (float)(g_projOffsetY + (g_flightVpHeight >> 1)) * face->gradients[4];
			face->gradients[8] -= (float)(g_flightVpWidth >> 1) * face->gradients[6];
			face->gradients[8] -= (float)(g_projOffsetY + (g_flightVpHeight >> 1)) * face->gradients[7];
			area = face->gradients[0] * face->gradients[4] - face->gradients[3] * face->gradients[1];
			if (area < g_renderProjectionZeroFloat) {
				area = -area;
			}
			{
				const OptTextureData* material = (const OptTextureData*)mesh->pMaterial;
				float lodScale;

				if (geometry->vertexIdx[3] == -1) {
					totalW = g_renderTriangleCornerCount / totalW;
				} else {
					totalW = g_renderQuadCornerCount / totalW;
				}
				lodScale = (float)(unsigned int)g_projScaleInt * totalW;
				face->mipLevel = (int)((float)((material->width * material->height) << 8) *
									   (area * (lodScale * lodScale)));
			}
		}
	}
	g_projVertCount += mesh->projVertCursor;
}

// FUNCTION: XVT 0x408BC0
void RenderScene_ProjectDistantMeshVertices(SceneMesh* mesh) {
	float projectionScale;
	SceneFace* face;
	int vertexBaseIndex;
	ProjVertex* output;
	int vertexIndex;
	int faceIndex;

	projectionScale = (float)((double)(unsigned int)g_projScaleInt / mesh->viewPosZ * g_renderDistantDepth);
	face = &g_visFaceList[mesh->faceBaseIndex];
	vertexBaseIndex = g_projVertCount;
	mesh->vertBaseIndex = vertexBaseIndex;
	output = &g_projVertList[vertexBaseIndex];
	mesh->projVertCursor = 0;
	for (vertexIndex = 0; mesh->vertexCount > vertexIndex; ++vertexIndex) {
		g_vertexRemap[vertexIndex] = -1;
	}
	for (faceIndex = 0; mesh->visFaceCount > faceIndex; ++face, ++faceIndex) {
		const FaceRecord* geometry = &mesh->pFaceGeom[face->faceIndex];
		int cornerIndex;

		face->maxVertW = 0.0f;
		face->minVertW = (float)g_projScaleInt;
		for (cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
			const int modelVertexIndex = geometry->vertexIdx[cornerIndex];
			const int uvIndex = geometry->uvIdx[cornerIndex];
			const int normalIndex = geometry->normalIdx[cornerIndex];
			float vertexW;

			if (modelVertexIndex == -1) {
				break;
			}
			if (g_vertexRemap[modelVertexIndex] == -1) {
				OptVector transformed;

				g_vertexRemap[modelVertexIndex] = mesh->projVertCursor++;
				transformed.x = mesh->pModelVerts[modelVertexIndex].x;
				transformed.y = mesh->pModelVerts[modelVertexIndex].y;
				transformed.z = mesh->pModelVerts[modelVertexIndex].z;
				Math3D_RotateVec3(&transformed.x, mesh->viewOrient);
				transformed.x += mesh->viewPosX;
				transformed.y += mesh->viewPosY;
				transformed.z += mesh->viewPosZ;
				transformed.z += g_renderDistantDepth;
				output->w = projectionScale / transformed.z;
				output->sx = output->w * transformed.x;
				output->sy = output->w * transformed.y;
				output->sx += (float)(g_flightVpWidth >> 1);
				output->sy += (float)(g_projOffsetY + (g_flightVpHeight >> 1));
				vertexW = output->w;
				RenderScene_ComputeVertexLighting(mesh, output, &mesh->pVertNormals[normalIndex],
												  &mesh->pModelVerts[modelVertexIndex], &g_meshEyePos);
				output->tu = mesh->pUVs[uvIndex].u;
				output->tv = mesh->pUVs[uvIndex].v;
				++output;
			} else {
				vertexW = g_projVertList[mesh->vertBaseIndex + g_vertexRemap[modelVertexIndex]].w;
			}
			if (face->maxVertW < vertexW) {
				face->maxVertW = vertexW;
			}
			if (face->minVertW > vertexW) {
				face->minVertW = vertexW;
			}
		}
	}
	g_projVertCount += mesh->projVertCursor;
}

// FUNCTION: XVT 0x408E70
void RenderScene_DrawMeshFaces(const SceneMesh* mesh) {
	RenderClipVertex* vertices;
	int* emittedVertexByProjection;
	int* clipOutput;
	SceneFace* face;
	const uint8_t* previousTexels;
	Std3DTexCacheNode* opaqueTexture;
	Std3DTexCacheNode* colorKeyTexture;
	int faceIndex;
	int vertexIndex;
	int clipIndex;
	int previousVertexIndex;
	int currentVertexIndex;
	int textureWidth;
	int textureHeight;
	int texelOffset;
	int mipLevel;
	int triangleCorner;
	int colorKeyVertexBase;

	enum {
		TRIANGLE_FIRST_NEW_CORNER = 2,
		MIP_LEVEL_REDUCTION_SHIFT = 2,
		MIP_LEVEL_REDUCTION_THRESHOLD = 256,
		MIP_MINIMUM_DIMENSION = 8,
		OPAQUE_PALETTE_OFFSET = 2048,
		PALETTE_TRANSPARENT_INDEX_SLOT = 256,
		BASE_MESH_TRIANGLE_FLAGS = 0x9813,
		BILINEAR_TRIANGLE_FLAGS = STD3D_RS_TEXTURE_MAG_LINEAR | STD3D_RS_TEXTURE_MIN_LINEAR,
		COLOR_KEY_TRIANGLE_FLAGS = STD3D_RS_ALPHA_BLEND
	};

#ifdef XVT_MODERN
	/* Suppress before texture lookup so hidden classic draws do not refill the cache. */
	if (AeronDx5_IsClassicFlightRenderingSuppressed())
		return;
#endif

	vertexIndex = mesh->vertBaseIndex;
	previousTexels = NULL;
	vertices = (RenderClipVertex*)&g_projVertList[vertexIndex];
	g_clipInputProjVertEndIndex = vertexIndex + mesh->projVertCursor;
	g_clipVertCursor = g_clipInputProjVertEndIndex;
	face = &g_visFaceList[mesh->faceBaseIndex];
	emittedVertexByProjection = (int*)g_sceneEdgeList;
	for (vertexIndex = 0; vertexIndex < g_clipInputProjVertEndIndex; ++vertexIndex)
		emittedVertexByProjection[vertexIndex] = -1;
#ifdef XVT_MODERN
	opaqueTexture = NULL;
	colorKeyTexture = NULL;
#endif

	faceIndex = 0;
	if (mesh->visFaceCount <= 0)
		return;
	do {
		SceneFace* currentFace;
		const FaceRecord* geometry;
		int cornerCount;

		currentFace = face++;
		geometry = &mesh->pFaceGeom[currentFace->faceIndex];
		cornerCount = geometry->edgeIdx[3] == -1 ? 3 : 4;
		g_clipCountA = cornerCount;
		if (g_pStd3DCurDevice->caps.bSquareOnlyTexture != 0) {
			const OptTextureData* material;
			float uvScale;
			int scaledWidth;
			int scaledHeight;

			material = (const OptTextureData*)mesh->pMaterial;
			uvScale = 1.0f;
			scaledWidth = material->width;
			scaledHeight = material->height;
			if (scaledHeight < scaledWidth) {
				while (scaledHeight < scaledWidth) {
					uvScale *= g_renderTextureUvHalfScale;
					scaledHeight *= 2;
				}
				scaledHeight = material->height;
			} else if (scaledHeight > scaledWidth) {
				while (scaledWidth < scaledHeight) {
					uvScale *= g_renderTextureUvHalfScale;
					scaledWidth *= 2;
				}
				scaledWidth = material->width;
			}
			clipOutput = g_clipIdxA;
			for (vertexIndex = 0; vertexIndex < cornerCount; ++vertexIndex) {
				OptTexCoord uv;
				RenderClipVertex* source;
				RenderClipVertex* duplicate;
				int projectedVertexIndex;

				uv = mesh->pUVs[geometry->uvIdx[vertexIndex]];
				if (scaledHeight < scaledWidth)
					uv.v *= uvScale;
				else if (scaledHeight > scaledWidth)
					uv.u *= uvScale;
				projectedVertexIndex = g_vertexRemap[geometry->vertexIdx[vertexIndex]];
				*clipOutput = projectedVertexIndex;
				source = &vertices[projectedVertexIndex];
				if (source->u != uv.u || source->v != uv.v) {
					duplicate = &vertices[g_clipVertCursor];
					duplicate->x = source->x;
					duplicate->y = source->y;
					duplicate->rhw = source->rhw;
					duplicate->z = source->z;
					duplicate->u = uv.u;
					duplicate->v = uv.v;
					*clipOutput = g_clipVertCursor++;
				}
				++clipOutput;
			}
		} else {
			for (vertexIndex = 0; vertexIndex < cornerCount; ++vertexIndex) {
				const OptTexCoord* uv;
				RenderClipVertex* source;
				RenderClipVertex* duplicate;
				int projectedVertexIndex;

				projectedVertexIndex = g_vertexRemap[geometry->vertexIdx[vertexIndex]];
				g_clipIdxA[vertexIndex] = projectedVertexIndex;
				uv = &mesh->pUVs[geometry->uvIdx[vertexIndex]];
				source = &vertices[projectedVertexIndex];
				if (source->u != uv->u || source->v != uv->v) {
					duplicate = &vertices[g_clipVertCursor];
					duplicate->x = source->x;
					duplicate->y = source->y;
					duplicate->rhw = source->rhw;
					duplicate->z = source->z;
					duplicate->u = mesh->pUVs[geometry->uvIdx[vertexIndex]].u;
					duplicate->v = mesh->pUVs[geometry->uvIdx[vertexIndex]].v;
					g_clipIdxA[vertexIndex] = g_clipVertCursor++;
				}
			}
		}

		if (currentFace->nearClipState == -1) {
			if (g_clipCountA > 0)
				memcpy(g_clipIdxB, g_clipIdxA, (size_t)g_clipCountA * sizeof(g_clipIdxA[0]));
			g_clipCountB = g_clipCountA;
			g_clipCountA = 0;
			if (g_clipCountB > 0) {
				previousVertexIndex = g_clipIdxB[g_clipCountB - 1];
				for (clipIndex = 0; clipIndex < g_clipCountB; ++clipIndex) {
					currentVertexIndex = g_clipIdxB[clipIndex];
					RenderClip_ClipPolyNear(previousVertexIndex, currentVertexIndex, vertices);
					previousVertexIndex = currentVertexIndex;
				}
			}
		}

		currentFace->nearClipState = g_flightVpHeight;
		g_clipCountB = 0;
		if (g_clipCountA > 0) {
			previousVertexIndex = g_clipIdxA[g_clipCountA - 1];
			for (clipIndex = 0; clipIndex < g_clipCountA; ++clipIndex) {
				currentVertexIndex = g_clipIdxA[clipIndex];
				RenderClip_ClipPolyTop(previousVertexIndex, currentVertexIndex, vertices);
				previousVertexIndex = currentVertexIndex;
			}
		}
		g_clipCountA = 0;
		if (g_clipCountB > 0) {
			previousVertexIndex = g_clipIdxB[g_clipCountB - 1];
			for (clipIndex = 0; clipIndex < g_clipCountB; ++clipIndex) {
				currentVertexIndex = g_clipIdxB[clipIndex];
				RenderClip_ClipPolyBottom(previousVertexIndex, currentVertexIndex, vertices);
				previousVertexIndex = currentVertexIndex;
			}
		}
		g_clipCountB = 0;
		if (g_clipCountA > 0) {
			previousVertexIndex = g_clipIdxA[g_clipCountA - 1];
			for (clipIndex = 0; clipIndex < g_clipCountA; ++clipIndex) {
				currentVertexIndex = g_clipIdxA[clipIndex];
				RenderClip_ClipPolyLeft(previousVertexIndex, currentVertexIndex, vertices);
				previousVertexIndex = currentVertexIndex;
			}
		}
		g_clipCountA = 0;
		if (g_clipCountB > 0) {
			previousVertexIndex = g_clipIdxB[g_clipCountB - 1];
			for (clipIndex = 0; clipIndex < g_clipCountB; ++clipIndex) {
				currentVertexIndex = g_clipIdxB[clipIndex];
				RenderClip_ClipPolyRight(previousVertexIndex, currentVertexIndex, vertices);
				previousVertexIndex = currentVertexIndex;
			}
		}

		for (clipIndex = 0; clipIndex < g_clipCountA; ++clipIndex) {
			int emittedVertexIndex;

			currentVertexIndex = g_clipIdxA[clipIndex];
			if (currentVertexIndex < g_clipInputProjVertEndIndex) {
				if (emittedVertexByProjection[currentVertexIndex] == -1)
					emittedVertexByProjection[currentVertexIndex] =
						RenderScene_EmitFlightVertex(currentVertexIndex, vertices, currentFace);
				emittedVertexIndex = emittedVertexByProjection[currentVertexIndex];
			} else {
				emittedVertexIndex = RenderScene_EmitFlightVertex(currentVertexIndex, vertices, currentFace);
			}
			g_clipIdxA[clipIndex] = emittedVertexIndex;
		}

		if (g_clipCountA > TRIANGLE_FIRST_NEW_CORNER) {
			const OptTextureData* material;
			const uint8_t* texels;

			material = (const OptTextureData*)mesh->pMaterial;
			textureWidth = material->width;
			textureHeight = material->height;
			texelOffset = 0;
			if (textureWidth * textureHeight == material->textureSize) {
				mipLevel = (int)((float)currentFace->mipLevel * g_mipLodScale);
				while (mipLevel > MIP_LEVEL_REDUCTION_THRESHOLD && textureWidth != MIP_MINIMUM_DIMENSION &&
					   textureHeight != MIP_MINIMUM_DIMENSION) {
					mipLevel >>= MIP_LEVEL_REDUCTION_SHIFT;
					texelOffset += textureWidth * textureHeight;
					textureWidth >>= 1;
					textureHeight >>= 1;
				}
			}
			texels = (const uint8_t*)mesh->pTexels + texelOffset;
			if (texels != previousTexels) {
				uint16_t* opaquePalette;

				previousTexels = texels;
				opaquePalette = mesh->pColorKeyPalette + OPAQUE_PALETTE_OFFSET;
				opaqueTexture =
					RenderTexture_GetOrCreateOpaque(textureWidth, textureHeight, opaquePalette, texels);
				colorKeyTexture = NULL;
				if (opaquePalette[PALETTE_TRANSPARENT_INDEX_SLOT] != 0) {
					uint8_t genusId;

					genusId = mesh->pObject->genusId;
					if (genusId == CRAFT_GENUS_PLAYER_PROJECTILE || genusId == CRAFT_GENUS_OTHER_PROJECTILE) {
						opaquePalette[PALETTE_TRANSPARENT_INDEX_SLOT] = 0;
					} else if (mesh->pTextureName != NULL && *mesh->pTextureName != '_') {
						colorKeyTexture = RenderTexture_GetOrCreateColorKey(textureWidth, textureHeight,
																			mesh->pColorKeyPalette, texels);
						if (colorKeyTexture == NULL && material->width == textureWidth &&
							material->height == textureHeight)
							*mesh->pTextureName = '_';
					}
				}
			}
		}

		if (colorKeyTexture != NULL) {
			for (vertexIndex = 0; vertexIndex < g_clipCountA; ++vertexIndex) {
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex] =
					g_flightVertexBuffer[g_clipIdxA[vertexIndex]];
				g_flightVertexBuffer[g_d3dVertexCount + vertexIndex].color = UINT32_MAX;
			}
			colorKeyVertexBase = g_d3dVertexCount;
			triangleCorner = TRIANGLE_FIRST_NEW_CORNER;
			g_d3dVertexCount += g_clipCountA;
			if (g_clipCountA > triangleCorner) {
				do {
					g_triBuffer[g_d3dIndexCount].v0 = colorKeyVertexBase;
					g_triBuffer[g_d3dIndexCount].v1 = colorKeyVertexBase + triangleCorner - 1;
					g_triBuffer[g_d3dIndexCount].v2 = colorKeyVertexBase + triangleCorner;
					g_triBuffer[g_d3dIndexCount].texture = colorKeyTexture;
					g_triBuffer[g_d3dIndexCount].flags = (Std3DRenderStateFlags)BASE_MESH_TRIANGLE_FLAGS;
					if (g_bilinearEnabled != 0)
						g_triBuffer[g_d3dIndexCount].flags += BILINEAR_TRIANGLE_FLAGS;
					++triangleCorner;
					g_triBuffer[g_d3dIndexCount].flags += COLOR_KEY_TRIANGLE_FLAGS;
					++g_d3dIndexCount;
				} while (triangleCorner < g_clipCountA);
			}
		}

		triangleCorner = TRIANGLE_FIRST_NEW_CORNER;
		if (g_clipCountA > triangleCorner) {
			int* triangleVertex = &g_clipIdxA[1];

			do {
				g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
				g_triBuffer[g_d3dIndexCount].v1 = *triangleVertex++;
				g_triBuffer[g_d3dIndexCount].v2 = *triangleVertex;
				g_triBuffer[g_d3dIndexCount].texture = opaqueTexture;
				g_triBuffer[g_d3dIndexCount].flags = (Std3DRenderStateFlags)BASE_MESH_TRIANGLE_FLAGS;
				if (g_bilinearEnabled != 0)
					g_triBuffer[g_d3dIndexCount].flags += BILINEAR_TRIANGLE_FLAGS;
				if (g_capVertexAlpha != 0) {
					g_triBuffer[g_d3dIndexCount].flags += COLOR_KEY_TRIANGLE_FLAGS;
					g_capVertexAlpha = 0;
				}
				++triangleCorner;
				++g_d3dIndexCount;
			} while (triangleCorner < g_clipCountA);
		}
		++faceIndex;
	} while (faceIndex < mesh->visFaceCount);
}

// FUNCTION: XVT 0x40B010
void RenderScene_DrawMesh(const SceneMesh* mesh) {
	SceneMesh* queuedMesh;
	int previousVisibleFaceCount;

	g_projVertCount = 0;
	previousVisibleFaceCount = g_visFaceCount;
	g_sceneEdgeCursor = 0;
	if (g_meshQueueIndex == g_meshQueueMax || g_visFaceCount + mesh->faceCount > g_sceneFaceMax ||
		mesh->vertexCount > g_projVertMax || mesh->edgeCount > g_sceneEdgeMax)
		return;
	memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(SceneMesh));
	queuedMesh = &g_meshQueue[g_meshQueueIndex];
	RenderScene_CullMeshFacesFromView(queuedMesh);
	if (queuedMesh->visFaceCount == 0)
		return;
	if (g_d3dVertexCount + 8 * queuedMesh->visFaceCount > g_maxBatchVerts ||
		g_d3dIndexCount + 2 * queuedMesh->visFaceCount > g_maxBatchTris) {
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
	if (g_bBackdropMeshMode != 0)
		RenderScene_ProjectDistantMeshVertices(queuedMesh);
	else
		RenderScene_ProjectMeshVertices(queuedMesh);
	RenderScene_DrawMeshFaces(queuedMesh);
	g_visFaceCount = previousVisibleFaceCount;
}

// FUNCTION: XVT 0x40B180
void RenderScene_InitHardwareFrame(void) {
	unsigned int spanBytes;
	int viewportOriginX;
	int viewportOriginY;

	viewportOriginX = width - g_surfaceWidth;
	viewportOriginY = height - g_surfaceHeight;
	viewportOriginX = g_flightVpX + ((unsigned int)viewportOriginX >> 1);
	viewportOriginY = g_flightVpY + ((unsigned int)viewportOriginY >> 1);
	g_flightVpOriginX = (float)(unsigned int)viewportOriginX;
	g_d3dIndexCount = 0;
	g_flightVpOriginY = (float)(unsigned int)viewportOriginY;
	g_d3dVertexCount = 0;
	g_d3dVertexAlphaStateResetSlot = 0;
	g_capVertexAlpha = 1;

	spanBytes = sizeof(SceneSpan) * g_sceneSpanDataCapacity;
	g_maxBatchVerts = spanBytes >> 7;
	g_maxBatchTris = spanBytes / sizeof(Std3DRenderTri) >> 2;
	if (g_pStd3DCurDevice->caps.maxVertexCount < (unsigned int)g_maxBatchVerts) {
		g_maxBatchVerts = g_pStd3DCurDevice->caps.maxVertexCount;
	}
	if (g_maxBatchVerts > 256) {
		g_maxBatchVerts = 256;
	}
	if (g_maxBatchTris > 256) {
		g_maxBatchTris = 256;
	}
	if ((int)((g_pStd3DCurDevice->caps.maxBufferSize - ((unsigned int)g_maxBatchVerts << 6)) /
			  sizeof(SceneSpan)) < g_maxBatchTris) {
		g_maxBatchTris = (g_pStd3DCurDevice->caps.maxBufferSize - ((unsigned int)g_maxBatchVerts << 6)) /
						 sizeof(SceneSpan);
	}
	g_flightVertexBuffer = (D3DTLVERTEX*)g_sceneSpanDataBase;
	g_triBuffer = (Std3DRenderTri*)&g_sceneSpanDataBase[g_sceneSpanDataCapacity / 2];
}

// FUNCTION: XVT 0x40B2C0
void RenderScene_FlushGeometry(void) {
	enum {
		SKIP_TARGET_MARKERS = 0,
		DRAW_TARGET_MARKERS = 1,
	};

	if (g_sceneFlushDrawTargetMarkers != 0) {
		SceneBillboard_RenderQueuedTextured(DRAW_TARGET_MARKERS);
		Targeting_DrawSceneObjectBoxes();
	} else {
		SceneBillboard_RenderQueuedTextured(SKIP_TARGET_MARKERS);
	}
	g_sceneBillboardQueueCount = 0;

	if (g_d3dVertexCount == 0 || g_d3dIndexCount == 0) {
		return;
	}
	Math_SetFpuExtendedPrecisionMode();
	std3D_StartScene();
	std3D_LockExecuteBuffer();
	std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
	std3D_BeginInstructions();
	std3D_AddTriangles(g_triBuffer, g_d3dIndexCount);
	std3D_ExecuteBuffer();
	std3D_EndScene();
	Math_SetFpuSinglePrecisionMode();
}

// FUNCTION: XVT 0x40B350
int RenderScene_EmitFlightVertex(int vertexIndex, const RenderClipVertex* vertices, const SceneFace* face) {
	const RenderClipVertex* source;
	uint32_t zBits;
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
	float depth;
	int intensity;
	uint32_t color;

	(void)face;
	source = &vertices[vertexIndex];
	rhw = source->rhw;
	x = source->x;
	y = source->y;
	z = source->z;
	u = source->u;
	v = source->v;
	memcpy(&zBits, &z, sizeof(zBits));
	if (zBits > 0x80000000u) {
		z = (float)(unsigned int)g_projScaleInt;
	}
	depth = 1.0f / ((float)(unsigned int)g_projScaleInt / z * g_invDepthProjScale + 1.0f);
	if (g_std3DZBufferBitDepth == 2) {
		depth = 1.0f - depth;
	}
	g_flightVertexBuffer[g_d3dVertexCount].sx = x + g_flightVpOriginX;
	g_flightVertexBuffer[g_d3dVertexCount].sy = y + g_flightVpOriginY;
	g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
	g_flightVertexBuffer[g_d3dVertexCount].rhw = z;
	g_flightVertexBuffer[g_d3dVertexCount].tu = u;
	g_flightVertexBuffer[g_d3dVertexCount].tv = v;
	intensity = (int)(rhw * 320.0f) + 48;
	if (intensity > 255) {
		intensity = 255;
	}
	if (g_capVertexAlpha != 0) {
		color = 65793 * intensity - 0x2000000;
	} else {
		color = 65793 * intensity - 0x1000000;
	}
	g_flightVertexBuffer[g_d3dVertexCount].color = color;
	g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
	return g_d3dVertexCount++;
}

// FUNCTION: XVT 0x40B520
void nullsub_2(void) {}

// FUNCTION: XVT 0x40B530
void std3D_FillZBufferFromViewportMask(void) {
	DDSURFACEDESC surfaceDesc;
	HRESULT lockResult;
	uint8_t foregroundByte;
	uint8_t backgroundByte;
	uint8_t* lockedSurface;
	uint8_t* destinationRow;
	uint8_t* destination;
	uint8_t* maskCursor;
	int runLength;
	int8_t runType;
	unsigned int decodedWidth;
	unsigned int row;

	if (g_std3DZBufferBitDepth == 16) {
		foregroundByte = 0xFF;
		backgroundByte = 0;
	} else {
		foregroundByte = 0;
		backgroundByte = 0xFF;
	}

	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	surfaceDesc.dwSize = sizeof(surfaceDesc);
	while (1) {
		lockResult = g_std3DZBufferSurface->lpVtbl->Lock(g_std3DZBufferSurface, NULL, &surfaceDesc, 0, NULL);
		if (lockResult == 0) {
			break;
		}
		if (lockResult != DX_DDERR_WASSTILLDRAWING) {
			DebugPrintf("ERROR!(%x) failed to lock D3D z buffer!\n", lockResult);
			return;
		}
	}

	destinationRow = surfaceDesc.lpSurface;
	lockedSurface = surfaceDesc.lpSurface;
	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	surfaceDesc.dwSize = sizeof(surfaceDesc);
	g_std3DZBufferSurface->lpVtbl->GetSurfaceDesc(g_std3DZBufferSurface, &surfaceDesc);
	destinationRow += ((width - g_surfaceWidth) & ~1) + 2 * g_flightVpX;
	destinationRow += ((unsigned int)(height - g_surfaceHeight) / 2 + g_flightVpY) * surfaceDesc.lPitch;
	maskCursor = g_flightAuxBuffer + g_viewportSpanMaskOffset;

	for (row = 0; row < g_flightVpHeight; ++row) {
		destination = destinationRow;
		runType = (int8_t)*maskCursor++;
		decodedWidth = 0;
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
				memset(destination, foregroundByte, 2 * runLength);
			} else {
				memset(destination, backgroundByte, 2 * runLength);
			}
			destination += 2 * runLength;
			runType = -runType;
			decodedWidth += runLength;
		}
		destinationRow += surfaceDesc.lPitch;
	}

	g_std3DZBufferSurface->lpVtbl->Unlock(g_std3DZBufferSurface, lockedSurface);
}

// FUNCTION: XVT 0x40B750
int RenderScene_ClearFrameBuffers(void) {
	DDBLTFX effects;

	memset(&effects, 0, sizeof(effects));
	effects.dwSize = sizeof(effects);
	effects.dwROP = DDROP_SRCCOPY;
	effects.dwFillColor = g_flightTextPalette[g_flightColorEscapeBypassChar];
	g_flightBackBuffer->lpVtbl->Blt(g_flightBackBuffer, NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL,
									&effects);
	return std3D_ClearZBuffer();
}

// FUNCTION: XVT 0x40BBC0
void std3D_DetachAndReleaseZBufferSurface(void) {
	if (g_std3DZBufferSurface != 0) {
		g_flightBackBuffer->lpVtbl->DeleteAttachedSurface(g_flightBackBuffer, 0, g_std3DZBufferSurface);
		g_std3DZBufferSurface->lpVtbl->Release(g_std3DZBufferSurface);
		g_std3DZBufferSurface = 0;
	}
}

// FUNCTION: XVT 0x4201F0
void RenderScene_ComputeVertexLighting(SceneMesh* mesh, ProjVertex* outVert, const OptVector* normal,
									   const OptVector* pos, const OptVector* eyePos) {
	OptVector lightPosition;
	int genusId;
	int lightIndex;

	genusId = mesh->pObject->genusId;
	if (genusId == CRAFT_GENUS_OTHER_PROJECTILE || genusId == CRAFT_GENUS_PLAYER_PROJECTILE) {
		outVert->lightIntensity = 1.0f;
		return;
	}
	if (g_dirLightingEnabled != 0) {
		float lightDirectionY;
		float lightDirectionZ;
		float lightDirectionX;

		lightDirectionY = (float)g_objectLightDirectionY * g_renderLightDirectionUnitScale;
		lightDirectionZ = (float)g_objectLightDirectionZ * g_renderLightDirectionUnitScale;
		lightDirectionX = (float)g_objectLightDirectionX * g_renderLightDirectionUnitScale;
		outVert->lightIntensity =
			(lightDirectionZ * normal->z + (lightDirectionX * normal->x + lightDirectionY * normal->y)) *
			g_renderDirectionalLightIntensityScale;
		if (outVert->lightIntensity < 0.0f) {
			outVert->lightIntensity = 0.0f;
		} else {
			lightPosition.x = (float)g_objectLightDirectionX + pos->x;
			lightPosition.y = (float)g_objectLightDirectionY + pos->y;
			lightPosition.z = (float)g_objectLightDirectionZ + pos->z;
			if (RenderScene_IsSegmentOccludedByObjectModel(mesh->pObject, pos, &lightPosition)) {
				outVert->lightIntensity = 0.0f;
			}
		}
	} else {
		outVert->lightIntensity = 0.40000001f;
	}

	for (lightIndex = 0; g_objectPointLightCount > lightIndex; ++lightIndex) {
		const ObjectPointLight* light = &g_objectPointLights[lightIndex];
		float dx;
		float dy;
		float dz;
		float lightDot;
		float componentX;
		float componentY;
		float componentZ;
		float distance;
		float diffuse;
		float specular;
		float contribution;

		{
			componentX = (float)light->x;
			componentY = (float)light->y;
			componentZ = (float)light->z;
			dx = componentX - pos->x;
			dy = componentY - pos->y;
			dz = componentZ - pos->z;
			lightDot = normal->z * dz + (normal->y * dy + normal->x * dx);

			if (g_useHardware3D == 0 && lightDot < 0.0f) {
				continue;
			}
			lightPosition.x = componentX;
			lightPosition.y = componentY;
			lightPosition.z = componentZ;
		}
		if (RenderScene_IsSegmentOccludedByObjectModel(mesh->pObject, pos, &lightPosition)) {
			continue;
		}
		componentX = dx;
		componentY = dy;
		componentZ = dz;
		if (dx < 0.0f) {
			componentX = -dx;
		}
		if (dy < 0.0f) {
			componentY = -dy;
		}
		if (dz < 0.0f) {
			componentZ = -dz;
		}
		if (componentX >= componentY && componentX >= componentZ) {
			distance = componentX + (componentY + componentZ) * g_renderRoughDistanceScale;
		} else if (componentY >= componentX && componentY >= componentZ) {
			distance = componentY + (componentX + componentZ) * g_renderRoughDistanceScale;
		} else {
			distance = componentZ + (componentX + componentY) * g_renderRoughDistanceScale;
		}
		if (g_useHardware3D != 0) {
			if (lightDot / distance < g_renderPointLightFacingThreshold) {
				continue;
			}
			lightDot = (float)(distance * g_renderHalfDouble);
		}
		diffuse = lightDot / (distance * distance);
		if (g_specularEnabled != 0) {
			float halfX;
			float halfY;
			float halfZ;
			float halfDot;
			float cosine;

			halfX = eyePos->x - pos->x + dx;
			halfY = eyePos->y - pos->y + dy;
			halfZ = eyePos->z - pos->z + dz;
			halfDot = (normal->z * halfZ + (normal->y * halfY + normal->x * halfX)) * g_renderHalfFloat;
			componentX = halfX;
			componentY = halfY;
			componentZ = halfZ;
			if (halfX < 0.0f) {
				componentX = -halfX;
			}
			if (halfY < 0.0f) {
				componentY = -halfY;
			}
			if (halfZ < 0.0f) {
				componentZ = -halfZ;
			}
			if (componentX >= componentY && componentX >= componentZ) {
				distance = componentX * g_renderSpecularApproxMaxComponentScale +
						   (componentY + componentZ) * g_renderSpecularApproxOtherComponentsScale;
			} else if (componentY >= componentX && componentY >= componentZ) {
				distance = componentY * g_renderSpecularApproxMaxComponentScale +
						   (componentX + componentZ) * g_renderSpecularApproxOtherComponentsScale;
			} else {
				distance = componentZ * g_renderSpecularApproxMaxComponentScale +
						   (componentX + componentY) * g_renderSpecularApproxOtherComponentsScale;
			}
			cosine = halfDot / distance;
			if (cosine >= g_renderHalfFloat) {
				specular = cosine * cosine * cosine;
				specular *= specular;
				specular *= specular;
				specular *= specular;
				specular *= specular;
			} else {
				specular = 0.0f;
			}
		} else {
			specular = 0.0f;
		}
		contribution = diffuse + specular;
		if (contribution > g_renderZeroFloat) {
			outVert->lightIntensity += (float)light->intensity * contribution;
			if (outVert->lightIntensity >= 1.0f) {
				outVert->lightIntensity = 1.0f;
				return;
			}
		}
	}
}

// FUNCTION: XVT 0x420DB0
void RenderScene_TransformFaceTextureGradients(SceneFace* face, const FaceTextureGradients* faceTexGradients,
											   const float* viewPosAndOrient) {
	face->gradients[0] = faceTexGradients->gradient0.x;
	face->gradients[1] = faceTexGradients->gradient0.y;
	face->gradients[2] = faceTexGradients->gradient0.z;
	Math3D_RotateVec3(&face->gradients[0], viewPosAndOrient + 3);

	face->gradients[3] = faceTexGradients->gradient1.x;
	face->gradients[4] = faceTexGradients->gradient1.y;
	face->gradients[5] = faceTexGradients->gradient1.z;
	Math3D_RotateVec3(&face->gradients[3], viewPosAndOrient + 3);
}

// FUNCTION: XVT 0x420E10
void RenderScene_TransformProjectLegacyPoint(float outProjected[3], const float point[3],
											 const float viewPosAndOrient[12]) {
	float viewPoint[3];

	viewPoint[0] = point[0];
	viewPoint[1] = point[1];
	viewPoint[2] = point[2];
	Math3D_RotateVec3(viewPoint, viewPosAndOrient + 3);
	viewPoint[0] += viewPosAndOrient[0];
	viewPoint[1] += viewPosAndOrient[1];
	viewPoint[2] += viewPosAndOrient[2];

	outProjected[2] = (float)((double)g_projScaleInt / viewPoint[2]);
	outProjected[0] = outProjected[2] * viewPoint[0];
	outProjected[1] = outProjected[2] * viewPoint[1];
	outProjected[0] += (int)(g_flightVpWidth >> 1);
	outProjected[1] += g_projOffsetY + (int)(g_flightVpHeight >> 1);
}

// FUNCTION: XVT 0x420EE0
void RenderScene_TransformProjectLegacyDistantPoint(float outProjected[3], const float point[3],
													const float viewPosAndOrient[12]) {
	float viewPoint[3];
	float distantProjectScale;

	distantProjectScale = (float)g_projScaleInt / viewPosAndOrient[2] * (float)100000.0;
	viewPoint[0] = point[0];
	viewPoint[1] = point[1];
	viewPoint[2] = point[2];
	Math3D_RotateVec3(viewPoint, viewPosAndOrient + 3);
	viewPoint[0] += viewPosAndOrient[0];
	viewPoint[1] += viewPosAndOrient[1];
	viewPoint[2] += viewPosAndOrient[2];
	viewPoint[2] += (float)100000.0;

	outProjected[2] = distantProjectScale / viewPoint[2];
	outProjected[0] = outProjected[2] * viewPoint[0];
	outProjected[1] = outProjected[2] * viewPoint[1];
	outProjected[0] += (int)(g_flightVpWidth >> 1);
	outProjected[1] += g_projOffsetY + (int)(g_flightVpHeight >> 1);
}

// FUNCTION: XVT 0x470140
void RenderScene_CullMeshFacesFromView(SceneMesh* mesh) {
	OptVector* faceNormal;
	FaceRecord* faceRecord;
	SceneFace* outFace;
	float* modelVerts;
	int faceIndex;

	mesh->faceBaseIndex = g_visFaceCount;
	g_meshEyePos.x = mesh->posX;
	g_meshEyePos.y = mesh->posY;
	g_meshEyePos.z = mesh->posZ;
	if (g_bBackdropMeshMode) {
		g_meshEyePos.x = 0.0f;
		g_meshEyePos.y = 0.0f;
		g_meshEyePos.z = -100000.0f;
		Math3D_RotateVec3(&g_meshEyePos.x, mesh->orient);
		g_meshEyePos.x += mesh->posX;
		g_meshEyePos.y += mesh->posY;
		g_meshEyePos.z += mesh->posZ;
	}

	faceNormal = mesh->pFaceNormals;
	faceRecord = mesh->pFaceGeom;
	modelVerts = &mesh->pModelVerts->x;
	outFace = &g_visFaceList[g_visFaceCount];
	faceIndex = 0;
	if (mesh->faceCount > 0) {
		do {
			float viewVec[3];
			int phongOffset;
			int vertexIndex;

			vertexIndex = 3 * faceRecord[faceIndex].vertexIdx[0];
			viewVec[0] = g_meshEyePos.x - modelVerts[vertexIndex];
			viewVec[1] = g_meshEyePos.y - modelVerts[vertexIndex + 1];
			viewVec[2] = g_meshEyePos.z - modelVerts[vertexIndex + 2];
			if (Math3D_Dot3(viewVec, &faceNormal[faceIndex].x) >= g_sw3dZeroFloat) {
				outFace->faceIndex = faceIndex;
				outFace->pMesh = mesh;
				phongOffset = g_phongSlotStride;
				phongOffset *= g_phongSlotIndex;
				outFace->pPhongData = g_scenePhongData + 12 * phongOffset;
				if (g_phongSlotIndex < 199) {
					++g_phongSlotIndex;
				}
				outFace->faceAndLayerId = faceIndex + (g_curLayerId << 16);
				outFace->pScanEdge = NULL;
				++outFace;
				++g_visFaceCount;
			}

			++faceIndex;
		} while (mesh->faceCount > faceIndex);
	}

	mesh->visFaceCount = g_visFaceCount - mesh->faceBaseIndex;
}

// FUNCTION: XVT 0x471E00
void RenderScene_DrawSceneMesh(SceneMesh* mesh) {
	SceneMesh* queuedMesh;

	if (g_useHardware3D != 0) {
		RenderScene_DrawMesh(mesh);
		return;
	}
	g_projVertCount = 0;
	g_sceneEdgeCursor = 0;
	if (g_meshQueueIndex != g_meshQueueMax && g_visFaceCount + mesh->faceCount <= g_sceneFaceMax &&
		mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
		memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(SceneMesh));
		queuedMesh = &g_meshQueue[g_meshQueueIndex];
		RenderScene_CullMeshFacesFromView(queuedMesh);
		if (queuedMesh->visFaceCount != 0) {
			if (g_bBackdropMeshMode != 0)
				sw3d_ProjectMeshVerticesDistant(queuedMesh);
			else
				sw3d_ProjectMeshVertices(queuedMesh);
			sw3d_RasterizeMeshFaces(queuedMesh);
			++g_meshQueueIndex;
		}
	}
}

// FUNCTION: XVT 0x472360
void RenderScene_ApplyBwingBridgeRotation(OptimizedPolyObject* unusedModel, ObjectRecord* obj,
										  SceneMesh* mesh, int bridgeMeshIndex) {
	int bridgeRotationByte;
	float axisAngle[4];
	float rotationMatrix[16];

	(void)unusedModel;

	bridgeRotationByte = obj->mobj->pCraft->meshRotation[bridgeMeshIndex];
	axisAngle[3] = bridgeRotationByte * g_meshRotationByteToRadiansScale;
	axisAngle[0] = 0.0f;
	axisAngle[2] = 0.0f;
	axisAngle[1] = -1.0f;
	Math3D_BuildAxisAngleMatrix(rotationMatrix, axisAngle);
	Math3D_MulMatrix3x3(mesh->orient, rotationMatrix);
	Math3D_RotateVec3(&mesh->posX, rotationMatrix);
	Math3D_MulMatrix3x3T(mesh->viewOrient, rotationMatrix);
}

// FUNCTION: XVT 0x472400
void RenderScene_DrawObjectModel(ObjectRecord* obj) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	int restoreMesh;
	float objectViewR0X;
	float objectViewR0Y;
	float objectViewR0Z;
	float objectViewR1X;
	float objectViewR1Y;
	float objectViewR1Z;
	OptNode* node;
	float objectViewR2X;
	float objectViewR2Y;
	float objectViewR2Z;
	SceneMesh mesh;
	SceneMesh savedMesh;
	int meshOrdinal;
	int rootIndex;

	modelHandle = g_loadedModels[obj->objectType];
	if (obj->mobj != NULL) {
		g_nodeSwitchIndex = obj->mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	if (model->selfMarker != model) {
		OptModel_AdjustOptimizedPolyObjectPointers(model);
	}

	memset(&mesh, 0, sizeof(mesh));
	mesh.pObject = obj;
	mesh.viewPosX = (float)(obj->world_x - g_players[g_localPlayer].viewState.savedTargetX);
	mesh.viewPosY = (float)(obj->world_y - g_players[g_localPlayer].viewState.savedTargetY);
	mesh.viewPosZ = (float)(obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
	mesh.viewOrient[0] = (float)g_camMatR0_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[1] = (float)g_camMatR1_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[2] = (float)g_camMatR2_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[3] = (float)g_camMatR0_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[4] = (float)g_camMatR1_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[5] = (float)g_camMatR2_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[6] = (float)g_camMatR0_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[7] = (float)g_camMatR1_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[8] = (float)g_camMatR2_Z * g_renderMatrixQ15ToFloatScale;
	Math3D_RotateVec3(&mesh.viewPosX, mesh.viewOrient);

	objectViewR0X = (float)g_objViewMat_R0_X * g_renderMatrixQ15ToFloatScale;
	objectViewR0Y = (float)g_objViewMat_R0_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[0] = objectViewR0X;
	mesh.viewOrient[1] = objectViewR0Y;
	objectViewR0Z = (float)g_objViewMat_R0_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[2] = objectViewR0Z;
	objectViewR1X = (float)g_objViewMat_R1_X * g_renderMatrixQ15ToFloatScale;
	objectViewR1Y = (float)g_objViewMat_R1_Y * g_renderMatrixQ15ToFloatScale;
	objectViewR1Z = (float)g_objViewMat_R1_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[3] = objectViewR1X;
	mesh.viewOrient[4] = objectViewR1Y;
	mesh.viewOrient[5] = objectViewR1Z;
	objectViewR2X = (float)g_objViewMat_R2_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[6] = objectViewR2X;
	objectViewR2Y = (float)g_objViewMat_R2_Y * g_renderMatrixQ15ToFloatScale;
	objectViewR2Z = (float)g_objViewMat_R2_Z * g_renderMatrixQ15ToFloatScale;
	mesh.posX = -mesh.viewPosX;
	mesh.posY = -mesh.viewPosY;
	mesh.posZ = -mesh.viewPosZ;
	mesh.orient[0] = objectViewR0X;
	mesh.viewOrient[7] = objectViewR2Y;
	mesh.orient[1] = objectViewR1X;
	mesh.orient[2] = objectViewR2X;
	mesh.viewOrient[8] = objectViewR2Z;
	mesh.orient[3] = objectViewR0Y;
	mesh.orient[4] = objectViewR1Y;
	mesh.orient[5] = objectViewR2Y;
	mesh.orient[6] = objectViewR0Z;
	mesh.orient[7] = objectViewR1Z;
	mesh.orient[8] = objectViewR2Z;
	Math3D_RotateVec3(&mesh.posX, mesh.orient);

	g_modelNodeWalkUnusedScratch0 = NULL;
	if (g_defaultWhiteTextureDescPtr == NULL) {
		g_defaultWhiteTextureDescPtr = &g_defaultWhiteTexture.header;
		g_defaultWhiteTexture.header.height = DEFAULT_WHITE_TEXTURE_DIMENSION;
		g_defaultWhiteTextureDescPtr->width = DEFAULT_WHITE_TEXTURE_DIMENSION;
		g_defaultWhiteTextureDescPtr->paletteType = 16;
		g_defaultWhiteTextureDescPtr->palette = (uint16_t*)(uintptr_t)256;
		ModelTexture_BuildPalettedShadeTable(g_defaultWhiteTexture.data.baseTexels,
											 g_defaultWhiteTextureRgb24, DEFAULT_WHITE_TEXTURE_DIMENSION,
											 DEFAULT_WHITE_TEXTURE_DIMENSION);
	}
	g_modelNodeWalkUnusedScratch1 = NULL;
	g_curVertNormals = NULL;
	g_modelNodeWalkUnusedScratch2 = NULL;
	restoreMesh = 0;
	g_curTextureDesc = g_defaultWhiteTextureDescPtr;
	rootIndex = 0;
	g_curMeshFlags = NULL;
	g_curVertexCount = 0;
	meshOrdinal = 0;

	for (; rootIndex < model->rootNodeCount; ++rootIndex) {
		mesh.rotAngle = 0.0f;
		node = model->rootNodes[rootIndex];
		if (node->nodeType != OPT_TEXTURE) {
			CraftData* craft;
			int rotationByte;

			++meshOrdinal;
			if (obj->mobj != NULL && obj->mobj->pCraft != NULL) {
				craft = obj->mobj->pCraft;
				if (craft->componentState[meshOrdinal - 1] != 0) {
					continue;
				}
				rotationByte = craft->meshRotation[meshOrdinal - 1];
				if (obj->objectType == B_WING_OBJECT_TYPE) {
					if (g_bwingBridgeMeshIndexCache == -1) {
						g_bwingBridgeMeshIndexCache = ModelMesh_FindBridgeIndex(model);
					}
					if (g_bwingBridgeMeshIndexCache != -1 &&
						obj->mobj->pCraft->meshRotation[g_bwingBridgeMeshIndexCache] != 0) {
						savedMesh = mesh;
						restoreMesh = 1;
						RenderScene_ApplyBwingBridgeRotation(model, obj, &mesh, g_bwingBridgeMeshIndexCache);
					}
				}
				mesh.rotAngle = rotationByte * g_meshRotationByteToRadiansScale;
			}
		}
		++g_curLayerId;
		RenderScene_DrawModelNode(model, node, &mesh);
		if (restoreMesh != 0) {
			mesh = savedMesh;
			restoreMesh = 0;
		}
	}
	Memory_UnlockHandle(modelHandle);
}

// FUNCTION: XVT 0x4728D0
void RenderScene_DrawNoAssetSourceModel(ObjectRecord* obj, int nodeSwitchIndex) {
	int objectType;
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	float objectViewR0X;
	float objectViewR0Y;
	float objectViewR0Z;
	float objectViewR1X;
	float objectViewR1Y;
	float objectViewR1Z;
	float objectViewR2X;
	float objectViewR2Y;
	float objectViewR2Z;
	SceneMesh mesh;
	int rootIndex;

	objectType = obj->objectType;
	if (objectType == COMPONENT_OBJECT_TYPE && obj->mobj != NULL) {
		objectType = obj->mobj->sourceObjectType;
	}
	if (obj->mobj != NULL) {
		g_nodeSwitchIndex = obj->mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}

	modelHandle = g_loadedModels[objectType];
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	if (model->selfMarker != model) {
		OptModel_AdjustOptimizedPolyObjectPointers(model);
	}

	memset(&mesh, 0, sizeof(mesh));
	mesh.pObject = obj;
	mesh.viewPosX = (float)(obj->world_x - g_players[g_localPlayer].viewState.savedTargetX);
	mesh.viewPosY = (float)(obj->world_y - g_players[g_localPlayer].viewState.savedTargetY);
	mesh.viewPosZ = (float)(obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
	mesh.viewOrient[0] = (float)g_camMatR0_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[1] = (float)g_camMatR1_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[2] = (float)g_camMatR2_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[3] = (float)g_camMatR0_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[4] = (float)g_camMatR1_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[5] = (float)g_camMatR2_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[6] = (float)g_camMatR0_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[7] = (float)g_camMatR1_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[8] = (float)g_camMatR2_Z * g_renderMatrixQ15ToFloatScale;
	Math3D_RotateVec3(&mesh.viewPosX, mesh.viewOrient);

	objectViewR0X = (float)g_objViewMat_R0_X * g_renderMatrixQ15ToFloatScale;
	objectViewR0Y = (float)g_objViewMat_R0_Y * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[0] = objectViewR0X;
	mesh.viewOrient[1] = objectViewR0Y;
	objectViewR0Z = (float)g_objViewMat_R0_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[2] = objectViewR0Z;
	objectViewR1X = (float)g_objViewMat_R1_X * g_renderMatrixQ15ToFloatScale;
	objectViewR1Y = (float)g_objViewMat_R1_Y * g_renderMatrixQ15ToFloatScale;
	objectViewR1Z = (float)g_objViewMat_R1_Z * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[3] = objectViewR1X;
	mesh.viewOrient[4] = objectViewR1Y;
	mesh.viewOrient[5] = objectViewR1Z;
	objectViewR2X = (float)g_objViewMat_R2_X * g_renderMatrixQ15ToFloatScale;
	mesh.viewOrient[6] = objectViewR2X;
	objectViewR2Y = (float)g_objViewMat_R2_Y * g_renderMatrixQ15ToFloatScale;
	objectViewR2Z = (float)g_objViewMat_R2_Z * g_renderMatrixQ15ToFloatScale;
	mesh.posX = -mesh.viewPosX;
	mesh.posY = -mesh.viewPosY;
	mesh.posZ = -mesh.viewPosZ;
	mesh.orient[0] = objectViewR0X;
	mesh.viewOrient[7] = objectViewR2Y;
	mesh.orient[1] = objectViewR1X;
	mesh.orient[2] = objectViewR2X;
	mesh.viewOrient[8] = objectViewR2Z;
	mesh.orient[3] = objectViewR0Y;
	mesh.orient[4] = objectViewR1Y;
	mesh.orient[5] = objectViewR2Y;
	mesh.orient[6] = objectViewR0Z;
	mesh.orient[7] = objectViewR1Z;
	mesh.orient[8] = objectViewR2Z;
	Math3D_RotateVec3(&mesh.posX, mesh.orient);

	g_modelNodeWalkUnusedScratch0 = NULL;
	if (g_defaultWhiteTextureDescPtr == NULL) {
		g_defaultWhiteTextureDescPtr = &g_defaultWhiteTexture.header;
		g_defaultWhiteTexture.header.height = DEFAULT_WHITE_TEXTURE_DIMENSION;
		g_defaultWhiteTextureDescPtr->width = DEFAULT_WHITE_TEXTURE_DIMENSION;
		g_defaultWhiteTextureDescPtr->paletteType = 16;
		g_defaultWhiteTextureDescPtr->palette = (uint16_t*)(uintptr_t)256;
		ModelTexture_BuildPalettedShadeTable(g_defaultWhiteTexture.data.baseTexels,
											 g_defaultWhiteTextureRgb24, DEFAULT_WHITE_TEXTURE_DIMENSION,
											 DEFAULT_WHITE_TEXTURE_DIMENSION);
	}
	g_curTextureDesc = g_defaultWhiteTextureDescPtr;
	g_modelNodeWalkUnusedScratch1 = NULL;
	g_curVertNormals = NULL;
	g_modelNodeWalkUnusedScratch2 = NULL;
	g_curMeshFlags = NULL;
	g_curVertexCount = 0;

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode = model->rootNodes[rootIndex];

		if (rootNode->nodeType == OPT_TEXTURE) {
			++nodeSwitchIndex;
			++g_curLayerId;
			RenderScene_DrawModelNode(model, rootNode, &mesh);
			continue;
		}
		if (rootIndex == nodeSwitchIndex) {
			++g_curLayerId;
			RenderScene_DrawModelNode(model, rootNode, &mesh);
		}
	}
	Memory_UnlockHandle(modelHandle);
}

// FUNCTION: XVT 0x472C90
void RenderScene_DrawModelNode(OptimizedPolyObject* object, OptNode* node, SceneMesh* mesh) {
	struct ModelNodeSelectionState {
		float lodThreshold;
		int nodeSwitchSelection;
	} selection;

	OptNode* currentNode;
	void* nodeData;
	int lodChildSelection;
	float axisAngle[4];
	float rotationMatrix[16];
	SceneMesh childMesh;
	int childIndex;

	currentNode = node;
	if (currentNode == NULL)
		return;
	lodChildSelection = 0;
	selection.nodeSwitchSelection = 0;
	while (currentNode->nodeType == OPT_NODEREF) {
		if (g_cacheResolvedOptNodeRefs != 0) {
#ifdef XVT_MODERN
			currentNode = XvtOpt_ResolveCached(object, currentNode);
#else

			char** referenceName;

			referenceName = (char**)&currentNode->param2;
			if (**referenceName == '\0') {
				currentNode = (OptNode*)currentNode->pName;
			} else {
				currentNode->pName = (char*)OptModel_ResolveNodeRef(object, *referenceName);
				**referenceName = '\0';
				currentNode = (OptNode*)currentNode->pName;
			}
#endif
		} else {
			currentNode = OptModel_ResolveNodeRef(object, (const char*)currentNode->param2);
		}
		if (currentNode == NULL)
			return;
	}

	nodeData = currentNode->param2;
	if (nodeData != NULL) {
		OptVector* parameters;

		parameters = (OptVector*)nodeData;
		switch (currentNode->nodeType) {
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17: {
				OptPackedFaceData* faceData;
				FaceRecord* faceGeometry;
				OptVector* faceNormals;
				FaceTextureGradients* faceTexturing;
				OptVector* generatedNormals;

				faceData = (OptPackedFaceData*)nodeData;
				mesh->faceCount = currentNode->param1;
				mesh->edgeCount = faceData->edgeCount;
				faceGeometry = (FaceRecord*)faceData->records;
				mesh->pFaceGeom = faceGeometry;
				faceNormals = (OptVector*)&faceGeometry[currentNode->param1];
				mesh->pFaceNormals = faceNormals;
				faceTexturing = (FaceTextureGradients*)&faceNormals[currentNode->param1];
				mesh->pFaceTexturing = faceTexturing;
				generatedNormals = &faceTexturing[currentNode->param1].gradient0;
				if (mesh->pMaterial == NULL) {
					int paletteOffset;

					mesh->pMaterial = g_curTextureDesc;
					mesh->pTexels = mesh->pMaterial;
					mesh->pTexels = (uint8_t*)mesh->pTexels + sizeof(OptTextureData);
					if (g_curTextureDesc->paletteType != 0) {
						mesh->pPalette = mesh->pTexels;
						paletteOffset = ((OptTextureData*)mesh->pMaterial)->width *
										((OptTextureData*)mesh->pMaterial)->height;
						if (((OptTextureData*)mesh->pMaterial)->textureSize == paletteOffset)
							mesh->pPalette =
								(uint8_t*)mesh->pTexels + ((OptTextureData*)mesh->pMaterial)->dataSize;
						else
							mesh->pPalette = (uint8_t*)mesh->pTexels + paletteOffset;
					} else {
						mesh->pPalette = g_curTextureDesc->palette;
					}
					mesh->pPalette = (uint8_t*)mesh->pPalette + OPT_INDEXED_SHADE_TABLE_SIZE;
					mesh->pColorKeyPalette = (uint16_t*)mesh->pPalette;
					mesh->pPalette = (uint8_t*)mesh->pPalette - OPT_INDEXED_SHADE_TABLE_SIZE;
				}
				if (mesh->pVertNormals == NULL) {
					mesh->pVertNormals = generatedNormals;
					RenderScene_DrawSceneMesh(mesh);
					mesh->pVertNormals = NULL;
				} else {
					RenderScene_DrawSceneMesh(mesh);
				}
				break;
			}
			case OPT_TYPE_2:
				Math3D_MulMatrix3x3(mesh->viewOrient, &parameters[1].x);
				Math3D_RotateVec3(&mesh->viewPosX, &parameters[1].x);
				mesh->viewPosX += parameters->x;
				mesh->viewPosY += parameters->y;
				mesh->viewPosZ += parameters->z;
				Math3D_MulMatrix3x3T(mesh->orient, &parameters[1].x);
				mesh->posX -= Math3D_RotateVec3X(&parameters->x, mesh->orient);
				mesh->posY -= Math3D_RotateVec3Y(&parameters->x, mesh->orient);
				mesh->posZ -= Math3D_RotateVec3Z(&parameters->x, mesh->orient);
				break;
			case OPT_MESHVERTS:
				mesh->vertexCount = currentNode->param1;
				mesh->pModelVerts = parameters;
				break;
			case OPT_TYPE_4:
				mesh->viewPosX += parameters->x;
				mesh->viewPosY += parameters->y;
				mesh->viewPosZ += parameters->z;
				mesh->posX -= Math3D_RotateVec3X(&parameters->x, mesh->orient);
				mesh->posY -= Math3D_RotateVec3Y(&parameters->x, mesh->orient);
				mesh->posZ -= Math3D_RotateVec3Z(&parameters->x, mesh->orient);
				break;
			case OPT_TYPE_5:
				Math3D_MulMatrix3x3(mesh->viewOrient, (const float*)nodeData);
				Math3D_RotateVec3(&mesh->viewPosX, (const float*)nodeData);
				Math3D_MulMatrix3x3T(mesh->orient, (const float*)nodeData);
				break;
			case OPT_TYPE_6: {
				float* scaleX;
				float* scaleY;
				float* scaleZ;
				float inverseScale;

				scaleX = &parameters->x;
				scaleY = &parameters->y;
				scaleZ = &parameters->z;
				mesh->viewOrient[0] = mesh->viewOrient[0] * *scaleX;
				mesh->viewOrient[1] = mesh->viewOrient[1] * *scaleY;
				mesh->viewOrient[2] = mesh->viewOrient[2] * *scaleZ;
				mesh->viewOrient[3] = mesh->viewOrient[3] * *scaleX;
				mesh->viewOrient[4] = mesh->viewOrient[4] * *scaleY;
				mesh->viewOrient[5] = mesh->viewOrient[5] * *scaleZ;
				mesh->viewOrient[6] = mesh->viewOrient[6] * *scaleX;
				mesh->viewOrient[7] = mesh->viewOrient[7] * *scaleY;
				mesh->viewOrient[8] = mesh->viewOrient[8] * *scaleZ;
				mesh->viewPosX = mesh->viewPosX * *scaleX;
				mesh->viewPosY = mesh->viewPosY * *scaleY;
				mesh->viewPosZ = mesh->viewPosZ * *scaleZ;

				inverseScale = 1.0f / *scaleX;
				mesh->orient[0] = mesh->orient[0] * inverseScale;
				mesh->orient[1] = mesh->orient[1] * inverseScale;
				mesh->orient[2] = mesh->orient[2] * inverseScale;
				inverseScale = 1.0f / *scaleY;
				mesh->orient[3] = mesh->orient[3] * inverseScale;
				mesh->orient[4] = mesh->orient[4] * inverseScale;
				mesh->orient[5] = mesh->orient[5] * inverseScale;
				inverseScale = 1.0f / *scaleZ;
				mesh->orient[6] = mesh->orient[6] * inverseScale;
				mesh->orient[7] = mesh->orient[7] * inverseScale;
				mesh->orient[8] = mesh->orient[8] * inverseScale;
				break;
			}
			case OPT_TYPE_10:
				if (currentNode->param1 == 8 || currentNode->param1 == 7)
					memcpy(&mesh->nodeType10Flags78, &g_curMeshFlags, sizeof(mesh->nodeType10Flags78));
				else if (currentNode->param1 == 6 || currentNode->param1 == 5)
					memcpy(&mesh->nodeType10Flags56, &g_curMeshFlags, sizeof(mesh->nodeType10Flags56));
				else
					memcpy(&mesh->nodeFlags[3], &g_curMeshFlags, sizeof(mesh->nodeFlags[3]));
				break;
			case OPT_VERTNORMALS:
				g_curVertNormals = parameters;
				mesh->pVertNormals = parameters;
				break;
			case OPT_TEXCOORDS:
				mesh->pUVs = (OptTexCoord*)nodeData;
				break;
			case OPT_TYPE_19:
				mesh->nodeFlags[0] = ((int*)nodeData)[0];
				mesh->nodeFlags[1] = ((int*)nodeData)[1];
				mesh->nodeFlags[2] = ((int*)nodeData)[2];
				break;
			case OPT_TEXTURE: {
				int paletteOffset;

				mesh->pTextureName = currentNode->pName;
				mesh->pMaterial = currentNode->param2;
				g_curTextureDesc = (OptTextureData*)mesh->pMaterial;
				mesh->pTexels = mesh->pMaterial;
				mesh->pTexels = (uint8_t*)mesh->pTexels + sizeof(OptTextureData);
				if (g_curTextureDesc->paletteType != 0) {
					mesh->pPalette = mesh->pTexels;
					paletteOffset = ((OptTextureData*)mesh->pMaterial)->width *
									((OptTextureData*)mesh->pMaterial)->height;
					if (((OptTextureData*)mesh->pMaterial)->textureSize == paletteOffset)
						paletteOffset = ((OptTextureData*)mesh->pMaterial)->dataSize;
					mesh->pPalette = (uint8_t*)mesh->pTexels + paletteOffset;
				} else {
					mesh->pPalette = g_curTextureDesc->palette;
				}
				mesh->pPalette = (uint8_t*)mesh->pPalette + OPT_INDEXED_SHADE_TABLE_SIZE;
				mesh->pColorKeyPalette = (uint16_t*)mesh->pPalette;
				mesh->pPalette = (uint8_t*)mesh->pPalette - OPT_INDEXED_SHADE_TABLE_SIZE;
				break;
			}
			case OPT_FACEGROUP:
				if (depthZ <= 0 || g_forcedLodLevel != 0) {
					lodChildSelection = g_forcedLodLevel;
					if (g_forcedLodLevel == 0) {
						lodChildSelection = 1;
					} else if (currentNode->childCount < g_forcedLodLevel) {
						lodChildSelection = -1;
					}
				} else {
					selection.lodThreshold = 1.0f;
					if (g_lodDistanceScale > 0.0f)
						selection.lodThreshold = g_sw3dUnitFloat / ((float)depthZ * g_lodDistanceScale);
					lodChildSelection = 1;
					while (lodChildSelection <= currentNode->childCount &&
						   ((float*)nodeData)[lodChildSelection - 1] > selection.lodThreshold)
						++lodChildSelection;
					if (lodChildSelection > currentNode->childCount)
						lodChildSelection = -1;
				}
				break;
			case OPT_ROTSCALE:
				if (mesh->rotAngle != 0.0f) {
					OptVector* pivot;
					OptVector* axis;
					float* pivotY;
					float* pivotZ;

					pivot = parameters;
					axis = pivot + 1;
					pivotY = &pivot->y;
					pivotZ = &pivot->z;
					mesh->posX -= pivot->x;
					mesh->posY -= *pivotY;
					mesh->posZ -= *pivotZ;
					mesh->viewPosX += Math3D_RotateVec3X(&pivot->x, mesh->viewOrient);
					mesh->viewPosY += Math3D_RotateVec3Y(&pivot->x, mesh->viewOrient);
					mesh->viewPosZ += Math3D_RotateVec3Z(&pivot->x, mesh->viewOrient);
					axisAngle[0] = axis->x * g_optAxisQ15ToFloatScale;
					axisAngle[1] = axis->y * g_optAxisQ15ToFloatScale;
					axisAngle[2] = axis->z * g_optAxisQ15ToFloatScale;
					axisAngle[3] = mesh->rotAngle;
					Math3D_BuildAxisAngleMatrix(rotationMatrix, axisAngle);
					Math3D_MulMatrix3x3(mesh->orient, rotationMatrix);
					Math3D_RotateVec3(&mesh->posX, rotationMatrix);
					Math3D_MulMatrix3x3T(mesh->viewOrient, rotationMatrix);
					mesh->posX += pivot->x;
					mesh->posY += *pivotY;
					mesh->posZ += *pivotZ;
					mesh->viewPosX -= Math3D_RotateVec3X(&pivot->x, mesh->viewOrient);
					mesh->viewPosY -= Math3D_RotateVec3Y(&pivot->x, mesh->viewOrient);
					mesh->viewPosZ -= Math3D_RotateVec3Z(&pivot->x, mesh->viewOrient);
				}
				break;
			case OPT_NODESWITCH:
				selection.nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (selection.nodeSwitchSelection > currentNode->childCount)
					selection.nodeSwitchSelection = currentNode->childCount;
				break;
			default:
				break;
		}
	} else {
		switch (currentNode->nodeType) {
			case OPT_TYPE_10:
				if (currentNode->param1 == 8 || currentNode->param1 == 7)
					memcpy(&mesh->nodeType10Flags78, &g_curMeshFlags, sizeof(mesh->nodeType10Flags78));
				else if (currentNode->param1 == 6 || currentNode->param1 == 5)
					memcpy(&mesh->nodeType10Flags56, &g_curMeshFlags, sizeof(mesh->nodeType10Flags56));
				else
					memcpy(&mesh->nodeFlags[3], &g_curMeshFlags, sizeof(mesh->nodeFlags[3]));
				break;
			case OPT_TEXTURE: {
				int paletteOffset;

				mesh->pTextureName = currentNode->pName;
				mesh->pMaterial = currentNode->param2;
				g_curTextureDesc = (OptTextureData*)mesh->pMaterial;
				mesh->pTexels = mesh->pMaterial;
				mesh->pTexels = (uint8_t*)mesh->pTexels + sizeof(OptTextureData);
				if (g_curTextureDesc->paletteType != 0) {
					mesh->pPalette = mesh->pTexels;
					paletteOffset = ((OptTextureData*)mesh->pMaterial)->width *
									((OptTextureData*)mesh->pMaterial)->height;
					if (((OptTextureData*)mesh->pMaterial)->textureSize == paletteOffset)
						paletteOffset = ((OptTextureData*)mesh->pMaterial)->dataSize;
					mesh->pPalette = (uint8_t*)mesh->pTexels + paletteOffset;
				} else {
					mesh->pPalette = g_curTextureDesc->palette;
				}
				mesh->pPalette = (uint8_t*)mesh->pPalette + OPT_INDEXED_SHADE_TABLE_SIZE;
				mesh->pColorKeyPalette = (uint16_t*)mesh->pPalette;
				mesh->pPalette = (uint8_t*)mesh->pPalette - OPT_INDEXED_SHADE_TABLE_SIZE;
				break;
			}
			case OPT_NODESWITCH:
				selection.nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (selection.nodeSwitchSelection > currentNode->childCount)
					selection.nodeSwitchSelection = currentNode->childCount;
				break;
			case OPT_TYPE_14:
			default:
				break;
		}
	}

	if (currentNode->childCount == 0)
		return;
	if (selection.nodeSwitchSelection != 0) {
		++g_curLayerId;
		RenderScene_DrawModelNode(object, currentNode->pChildren[selection.nodeSwitchSelection - 1], mesh);
	} else if (lodChildSelection != 0) {
		if (lodChildSelection != -1) {
			++g_curLayerId;
			RenderScene_DrawModelNode(object, currentNode->pChildren[lodChildSelection - 1], mesh);
		}
	} else {
		childMesh = *mesh;
		g_modelNodeWalkUnusedScratch0 = NULL;
		g_modelNodeWalkUnusedScratch1 = NULL;
		g_curVertNormals = NULL;
		g_modelNodeWalkUnusedScratch2 = NULL;
		g_curMeshFlags = NULL;
		g_curVertexCount = 0;
		for (childIndex = 0; childIndex < currentNode->childCount; ++childIndex) {
			++g_curLayerId;
			RenderScene_DrawModelNode(object, currentNode->pChildren[childIndex], &childMesh);
		}
	}
}

// FUNCTION: XVT 0x473550
void RenderScene_ToggleVertexLightOcclusion(void) {
	g_vertexLightOcclusionEnabled = !g_vertexLightOcclusionEnabled;
}

// FUNCTION: XVT 0x473570
int RenderScene_GetVertexLightOcclusionEnabled(void) { return g_vertexLightOcclusionEnabled; }

// FUNCTION: XVT 0x473580
int RenderScene_IsSegmentOccludedByObjectModel(ObjectRecord* object, const OptVector* segmentStart,
											   const OptVector* segmentEnd) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	SceneMesh mesh;
	int rootIndex;

	if (!g_vertexLightOcclusionEnabled) {
		return 0;
	}
	modelHandle = g_loadedModels[object->objectType];
	Memory_UnlockHandle(modelHandle);
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	if (model->selfMarker != model) {
		OptModel_AdjustOptimizedPolyObjectPointers(model);
	}
	memset(&mesh, 0, sizeof(mesh));
	mesh.pObject = object;
	mesh.viewOrient[0] = 1.0f;
	mesh.viewOrient[1] = 0.0f;
	mesh.viewOrient[2] = 0.0f;
	mesh.viewOrient[3] = 0.0f;
	mesh.viewOrient[4] = 1.0f;
	mesh.viewOrient[5] = 0.0f;
	mesh.viewOrient[6] = 0.0f;
	mesh.viewOrient[7] = 0.0f;
	mesh.viewOrient[8] = 1.0f;
	mesh.orient[0] = 1.0f;
	mesh.orient[1] = 0.0f;
	mesh.orient[2] = 0.0f;
	mesh.orient[3] = 0.0f;
	mesh.orient[4] = 1.0f;
	mesh.orient[5] = 0.0f;
	mesh.orient[6] = 0.0f;
	mesh.orient[7] = 0.0f;
	mesh.orient[8] = 1.0f;
	g_modelNodeWalkUnusedScratch0 = NULL;
	g_modelNodeWalkUnusedScratch1 = NULL;
	g_curVertNormals = NULL;
	g_modelNodeWalkUnusedScratch2 = NULL;
	g_curMeshFlags = NULL;
	g_curVertexCount = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		if (RenderScene_TestSegmentAgainstModelNode(model, model->rootNodes[rootIndex], &mesh, segmentStart,
													segmentEnd)) {
			return 1;
		}
	}
	return 0;
}

// FUNCTION: XVT 0x4736B0
int RenderScene_TestSegmentAgainstModelNode(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh,
											const OptVector* segmentStart, const OptVector* segmentEnd) {
	OptVector* param2;
	int childIndex;
	SceneMesh childMesh;

	if (node == NULL) {
		return 0;
	}
	while (node->nodeType == OPT_NODEREF) {
		node = OptModel_ResolveNodeRef(model, (const char*)node->param2);
		if (node == NULL) {
			return 0;
		}
	}
	param2 = (OptVector*)node->param2;
	if (param2 != NULL) {
		switch (node->nodeType) {
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17: {
				OptPackedFaceData* faceData = (OptPackedFaceData*)param2;
				FaceRecord* faceGeometry;
				int hit;
				OptVector* faceNormals;
				FaceTextureGradients* texturing;
				OptVector* generatedNormals;
				OptVector** vertexNormals;

				mesh->faceCount = node->param1;
				mesh->edgeCount = faceData->edgeCount;
				param2 = (OptVector*)faceData->records;
				faceGeometry = (FaceRecord*)param2;
				mesh->pFaceGeom = faceGeometry;
				faceNormals = (OptVector*)&faceGeometry[node->param1];
				mesh->pFaceNormals = faceNormals;
				texturing = (FaceTextureGradients*)&faceNormals[node->param1];
				mesh->pFaceTexturing = texturing;
				generatedNormals = &texturing[node->param1].gradient0;
				vertexNormals = &mesh->pVertNormals;
				if (*vertexNormals == NULL) {
					mesh->pVertNormals = generatedNormals;
					hit = RenderScene_TestSegmentAgainstMeshFaces(mesh, segmentStart, segmentEnd);
					if (hit) {
						return 1;
					}
					mesh->pVertNormals = NULL;
				} else {
					hit = RenderScene_TestSegmentAgainstMeshFaces(mesh, segmentStart, segmentEnd);
					if (hit) {
						return 1;
					}
				}
				break;
			}
			case OPT_TYPE_2:
				Math3D_MulMatrix3x3(mesh->viewOrient, &param2[1].x);
				Math3D_RotateVec3(&mesh->viewPosX, &param2[1].x);
				mesh->viewPosX = RenderScene_AddTranslation(mesh->viewPosX, param2->x);
				mesh->viewPosY += param2->y;
				mesh->viewPosZ += param2->z;
				Math3D_MulMatrix3x3T(mesh->orient, &param2[1].x);
				mesh->posX -= Math3D_RotateVec3X(&param2->x, mesh->orient);
				mesh->posY -= Math3D_RotateVec3Y(&param2->x, mesh->orient);
				mesh->posZ -= Math3D_RotateVec3Z(&param2->x, mesh->orient);
				break;
			case OPT_MESHVERTS:
				mesh->vertexCount = node->param1;
				mesh->pModelVerts = param2;
				break;
			case OPT_TYPE_4:
				mesh->viewPosX = RenderScene_AddTranslation(mesh->viewPosX, param2->x);
				mesh->viewPosY += param2->y;
				mesh->viewPosZ += param2->z;
				mesh->posX -= Math3D_RotateVec3X(&param2->x, mesh->orient);
				mesh->posY -= Math3D_RotateVec3Y(&param2->x, mesh->orient);
				mesh->posZ -= Math3D_RotateVec3Z(&param2->x, mesh->orient);
				break;
			case OPT_TYPE_5:
				Math3D_MulMatrix3x3(mesh->viewOrient, (const float*)param2);
				Math3D_RotateVec3(&mesh->viewPosX, &param2->x);
				Math3D_MulMatrix3x3T(mesh->orient, &param2->x);
				break;
			case OPT_TYPE_6: {
				float* scaleX = &param2->x;
				float* scaleY = &param2->y;
				float* scaleZ = &param2->z;
				float* orientation = mesh->orient;
				float inverseScale;

				mesh->viewOrient[0] *= *scaleX;
				mesh->viewOrient[1] *= *scaleY;
				mesh->viewOrient[2] *= *scaleZ;
				mesh->viewOrient[3] *= *scaleX;
				mesh->viewOrient[4] *= *scaleY;
				mesh->viewOrient[5] *= *scaleZ;
				mesh->viewOrient[6] *= *scaleX;
				mesh->viewOrient[7] *= *scaleY;
				mesh->viewOrient[8] *= *scaleZ;
				mesh->viewPosX *= *scaleX;
				mesh->viewPosY *= *scaleY;
				mesh->viewPosZ *= *scaleZ;
				inverseScale = 1.0f / *scaleX;
				mesh->orient[0] = orientation[0] * inverseScale;
				mesh->orient[1] = orientation[1] * inverseScale;
				mesh->orient[2] = orientation[2] * inverseScale;
				inverseScale = 1.0f / *scaleY;
				mesh->orient[3] = orientation[3] * inverseScale;
				mesh->orient[4] = orientation[4] * inverseScale;
				mesh->orient[5] = orientation[5] * inverseScale;
				inverseScale = 1.0f / *scaleZ;
				mesh->orient[6] = orientation[6] * inverseScale;
				mesh->orient[7] = orientation[7] * inverseScale;
				mesh->orient[8] = orientation[8] * inverseScale;
				break;
			}
			case OPT_VERTNORMALS:
				g_curVertNormals = param2;
				mesh->pVertNormals = param2;
				break;
			default:
				break;
		}
	}

	if (node->childCount != 0) {
		childMesh = *mesh;
		g_modelNodeWalkUnusedScratch0 = NULL;
		g_modelNodeWalkUnusedScratch1 = NULL;
		g_curVertNormals = NULL;
		g_modelNodeWalkUnusedScratch2 = NULL;
		g_curMeshFlags = NULL;
		g_curVertexCount = 0;
		childIndex = 0;
		if (node->childCount > 0) {
			do {
				if (RenderScene_TestSegmentAgainstModelNode(model, node->pChildren[childIndex], &childMesh,
															segmentStart, segmentEnd)) {
					return 1;
				}
				++childIndex;
			} while (childIndex < node->childCount);
		}
	}
	return 0;
}

// FUNCTION: XVT 0x473AD0
int RenderScene_TestSegmentAgainstMeshFaces(const SceneMesh* mesh, const OptVector* segmentStart,
											const OptVector* segmentEnd) {
	/* A face with vertexIdx[3] == -1 is a triangle, so its scaled base index is -3. */
	const FaceRecord* faces = mesh->pFaceGeom;
	const OptVector* normals = mesh->pFaceNormals;
	const float* coordinates = &mesh->pModelVerts[0].x;
	OptVector start;
	OptVector end;
	int faceIndex;

	start.x = segmentStart->x;
	start.y = segmentStart->y;
	start.z = segmentStart->z;
	end.x = segmentEnd->x;
	end.y = segmentEnd->y;
	end.z = segmentEnd->z;

	for (faceIndex = 0; faceIndex < mesh->faceCount; ++faceIndex, ++faces) {
		int base0 = faces->vertexIdx[0] * 3;
		int base1 = faces->vertexIdx[1] * 3;
		int base2 = faces->vertexIdx[2] * 3;
		int base3 = faces->vertexIdx[3] * 3;
		const OptVector* normal = normals++;
		float distanceStart;
		float distanceEnd;
		int vIndex0;
		int vIndex1;
		int vIndex2;
		int vIndex3;
		float hitU;
		float hitV;
		float cross0;
		float cross1;
		float cross2;
		float cross3;

		if (coordinates[base0] <= start.x && coordinates[base0] <= end.x) {
			if (coordinates[base1] <= start.x && coordinates[base1] <= end.x &&
				coordinates[base2] <= start.x && coordinates[base2] <= end.x &&
				(base3 == -3 || (coordinates[base3] <= start.x && coordinates[base3] <= end.x))) {
				continue;
			}
		} else if (coordinates[base0] >= start.x && coordinates[base0] >= end.x &&
				   coordinates[base1] >= start.x && coordinates[base1] >= end.x &&
				   coordinates[base2] >= start.x && coordinates[base2] >= end.x &&
				   (base3 == -3 || (coordinates[base3] >= start.x && coordinates[base3] >= end.x))) {
			continue;
		}
		++base0;
		++base1;
		++base2;
		++base3;
		if (coordinates[base0] <= start.y && coordinates[base0] <= end.y) {
			if (coordinates[base1] <= start.y && coordinates[base1] <= end.y &&
				coordinates[base2] <= start.y && coordinates[base2] <= end.y &&
				(base3 == -2 || (coordinates[base3] <= start.y && coordinates[base3] <= end.y))) {
				continue;
			}
		} else if (coordinates[base0] >= start.y && coordinates[base0] >= end.y &&
				   coordinates[base1] >= start.y && coordinates[base1] >= end.y &&
				   coordinates[base2] >= start.y && coordinates[base2] >= end.y &&
				   (base3 == -2 || (coordinates[base3] >= start.y && coordinates[base3] >= end.y))) {
			continue;
		}
		++base0;
		++base1;
		++base2;
		++base3;
		if (coordinates[base0] <= start.z && coordinates[base0] <= end.z) {
			if (coordinates[base1] <= start.z && coordinates[base1] <= end.z &&
				coordinates[base2] <= start.z && coordinates[base2] <= end.z &&
				(base3 == -1 || (coordinates[base3] <= start.z && coordinates[base3] <= end.z))) {
				continue;
			}
		} else if (coordinates[base0] >= start.z && coordinates[base0] >= end.z &&
				   coordinates[base1] >= start.z && coordinates[base1] >= end.z &&
				   coordinates[base2] >= start.z && coordinates[base2] >= end.z &&
				   (base3 == -1 || (coordinates[base3] >= start.z && coordinates[base3] >= end.z))) {
			continue;
		}

		distanceStart = (start.x - coordinates[base0 - 2]) * normal->x +
						normal->z * (start.z - coordinates[base0]) +
						normal->y * (start.y - coordinates[base0 - 1]);
		distanceEnd = (end.x - coordinates[base0 - 2]) * normal->x +
					  normal->z * (end.z - coordinates[base0]) + normal->y * (end.y - coordinates[base0 - 1]);
		if (distanceStart >= 0.0f) {
			if (distanceStart < 40.0f || distanceEnd >= 0.0f) {
				continue;
			}
		} else if (distanceStart > -40.0f || distanceEnd <= 0.0f) {
			continue;
		}

		distanceStart = (-distanceStart) / distanceEnd;

		if (normal->x < normal->z && normal->y < normal->z) {
			hitU = (end.x - start.x) * distanceStart + start.x;
			hitV = (end.y - start.y) * distanceStart + start.y;
			base0 -= 2;
			base1 -= 2;
			base2 -= 2;
			base3 -= 2;
			vIndex0 = base0 + 1;
			vIndex1 = base1 + 1;
			vIndex2 = base2 + 1;
			vIndex3 = base3 + 1;
		} else if (normal->x < normal->y && normal->y > normal->z) {
			hitU = (end.x - start.x) * distanceStart + start.x;
			hitV = (end.z - start.z) * distanceStart + start.z;
			base0 -= 2;
			base1 -= 2;
			base2 -= 2;
			base3 -= 2;
			vIndex0 = base0 + 2;
			vIndex1 = base1 + 2;
			vIndex2 = base2 + 2;
			vIndex3 = base3 + 2;
		} else {
			hitU = (end.y - start.y) * distanceStart + start.y;
			hitV = (end.z - start.z) * distanceStart + start.z;
			base0 -= 1;
			base1 -= 1;
			base2 -= 1;
			base3 -= 1;
			vIndex0 = base0 + 1;
			vIndex1 = base1 + 1;
			vIndex2 = base2 + 1;
			vIndex3 = base3 + 1;
		}

		cross0 = (hitU - coordinates[base0]) * (coordinates[vIndex1] - coordinates[vIndex0]) -
				 (coordinates[base1] - coordinates[base0]) * (hitV - coordinates[vIndex0]);
		cross1 = (hitU - coordinates[base1]) * (coordinates[vIndex2] - coordinates[vIndex1]) -
				 (coordinates[base2] - coordinates[base1]) * (hitV - coordinates[vIndex1]);
		if (cross0 < 0.0f) {
			if (cross1 >= 0.0f) {
				continue;
			}
		} else if (cross1 < 0.0f) {
			continue;
		}
		if (base3 < 0) {
			cross2 = (hitU - coordinates[base2]) * (coordinates[vIndex0] - coordinates[vIndex2]) -
					 (coordinates[base0] - coordinates[base2]) * (hitV - coordinates[vIndex2]);
			if (cross0 < 0.0f) {
				if (cross2 >= 0.0f) {
					continue;
				}
			} else if (cross2 < 0.0f) {
				continue;
			}
		} else {
			cross2 = (hitU - coordinates[base2]) * (coordinates[vIndex3] - coordinates[vIndex2]) -
					 (coordinates[base3] - coordinates[base2]) * (hitV - coordinates[vIndex2]);
			if (cross0 < 0.0f) {
				if (cross2 >= 0.0f) {
					continue;
				}
			} else if (cross2 < 0.0f) {
				continue;
			}
			cross3 = (hitU - coordinates[base3]) * (coordinates[vIndex0] - coordinates[vIndex3]) -
					 (coordinates[base0] - coordinates[base3]) * (hitV - coordinates[vIndex3]);
			if (cross0 < 0.0f) {
				if (cross3 >= 0.0f) {
					continue;
				}
			} else if (cross3 < 0.0f) {
				continue;
			}
		}
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x485CD0
void RenderScene_AllocateBuffers(void) {
	void* codeAddress[1];
	void* codeAddressValue;
	int edgeMax;
#ifndef XVT_MODERN
	int savedEdgeMax;
#endif

	g_sceneSpanDataCapacity = 20000;
	g_sceneSpanDataHandle = Memory_AllocHandle(sizeof(SceneSpan) * g_sceneSpanDataCapacity, 0);
	if (g_sceneSpanDataHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_sceneSpanPtrCapacity = 20000;
	g_sceneSpanPtrListHandle = Memory_AllocHandle(sizeof(SceneSpan*) * g_sceneSpanPtrCapacity, 0);
	if (g_sceneSpanPtrListHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_sceneFaceMax = 5000;
	g_visFaceListHandle = Memory_AllocHandle(sizeof(SceneFace) * g_sceneFaceMax, 0);
	if (g_visFaceListHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_projVertMax = 2 * g_vertexRemapCapacity;
	g_projVertListHandle = Memory_AllocHandle(sizeof(ProjVertex) * g_projVertMax, 0);
	if (g_projVertListHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	edgeMax = 2 * g_sceneEdgeFlagsCapacity;
#ifdef XVT_MODERN
	g_sceneEdgeMax = edgeMax;
	g_sceneEdgeListHandle = Memory_AllocHandle(sizeof(*g_sceneEdgeList) * g_sceneEdgeMax, 0);
#else
	savedEdgeMax = edgeMax;
	g_sceneEdgeMax = edgeMax;
	edgeMax <<= 3;
	edgeMax -= savedEdgeMax;
	edgeMax <<= 2;
	g_sceneEdgeListHandle = Memory_AllocHandle(edgeMax, 0);
#endif
	if (g_sceneEdgeListHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_vertexRemapHandle = Memory_AllocHandle(sizeof(*g_vertexRemap) * g_vertexRemapCapacity, 0);
	if (g_vertexRemapHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_sceneEdgeFlagsHandle = Memory_AllocHandle(sizeof(*g_sceneEdgeFlags) * g_sceneEdgeFlagsCapacity, 0);
	if (g_sceneEdgeFlagsHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_sceneSclEdgeListHandle = Memory_AllocHandle(sizeof(*g_sceneSclEdgeList) * 768, 0);
	if (g_sceneSclEdgeListHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_scanlineSpanHeadsHandle = Memory_AllocHandle(sizeof(*g_scanlineSpanHeads) * 768, 0);
	if (g_scanlineSpanHeadsHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_sw3dLightSampleBlockSize = 16;
	g_sw3dLightSampleInvBlockSize = g_sw3dSpanLengthReciprocal[16];
	g_sw3dLightSampleBlockShift = 4;
	g_sw3dLightSampleBlockMask = 15;
	g_sw3dLightSampleBlockSizeFloat = 16.0f;
	g_scenePhongDataHandle = Memory_AllocHandle(0x25800, 0);
	if (g_scenePhongDataHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	g_meshQueueMax = 500;
	g_meshQueueHandle = Memory_AllocHandle(sizeof(SceneMesh) * g_meshQueueMax, 0);
	if (g_meshQueueHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
	}

	/* The original inline assembly captured the address of the following code label. */
	codeAddressValue = (uint8_t*)(void*)RenderScene_AllocateBuffers + 0x217;
	memcpy(codeAddress, &codeAddressValue, sizeof(codeAddressValue));
	Memory_SetRegionExecuteReadWrite(codeAddress[0], 0x80000);
}

// FUNCTION: XVT 0x485F00
void RenderScene_Initialize(int resetSceneState) {
	uint8_t* mask;
	int8_t runType;
	SceneSpan* previousSpan;
	unsigned int scanX;
	unsigned int scanY;
	int scanline;

#ifndef XVT_MODERN
	g_sw3dInitializeSceneSavedFpuControl = _control87(0, 0);
	_control87(0, 0x30000);
#else
	g_sw3dFpuControlWordScratch &= 0xFFFFFCFF;
#endif
	g_sw3dCockpitMaskSentinelFace.maxVertW = 1.0e32f;
	g_sw3dCockpitMaskSentinelFace.minVertW = 1.0e32f;
	g_sw3dCockpitMaskSentinelFace.gradients[8] = 1.0e32f;
	g_sw3dCockpitMaskSentinelFace.gradients[6] = 0.0f;
	g_sw3dCockpitMaskSentinelFace.gradients[7] = 0.0f;
	g_sceneSpanDataBase = Memory_LockHandle(g_sceneSpanDataHandle);
	g_sceneSpanPtrList = Memory_LockHandle(g_sceneSpanPtrListHandle);
	g_visFaceList = Memory_LockHandle(g_visFaceListHandle);
	g_projVertList = Memory_LockHandle(g_projVertListHandle);
	g_sceneEdgeList = Memory_LockHandle(g_sceneEdgeListHandle);
	g_vertexRemap = Memory_LockHandle(g_vertexRemapHandle);
	g_sceneEdgeFlags = Memory_LockHandle(g_sceneEdgeFlagsHandle);
	g_sceneSclEdgeList = Memory_LockHandle(g_sceneSclEdgeListHandle);
	g_scanlineSpanHeads = Memory_LockHandle(g_scanlineSpanHeadsHandle);
	g_scenePhongData = Memory_LockHandle(g_scenePhongDataHandle);
	g_meshQueue = Memory_LockHandle(g_meshQueueHandle);
	if (resetSceneState != 0) {
		g_visFacePassStart = 0;
		g_sceneSpanPtrAvail = g_sceneSpanPtrCapacity;
		g_pSceneSpanDataCur = g_sceneSpanDataBase;
		g_visFaceCount = 0;
		g_phongSlotIndex = 0;
		g_meshQueueIndex = 0;
		mask = &g_flightAuxBuffer[g_viewportSpanMaskOffset];
		scanY = 0;
		g_pSceneSpanDataEnd = &g_sceneSpanDataBase[g_sceneSpanDataCapacity - 1];
		if (g_flightVpHeight != 0) {
			scanline = 0;
			do {
				scanX = 0;
				g_scanlineSpanHeads[scanline] = NULL;
				runType = (int8_t)*mask++;
				previousSpan = g_scanlineSpanHeads[scanline];
				if (g_flightVpWidth != 0) {
					do {
						int runLength;

						runLength = *mask++;
						if (runLength == 0) {
							runLength = *mask++;
							if (runLength == 0)
								runLength = *mask++ + 256;
							runLength += 255;
						}
						if (runType < 0) {
							if (previousSpan != NULL)
								previousSpan->next = g_pSceneSpanDataCur;
							else
								g_scanlineSpanHeads[scanline] = g_pSceneSpanDataCur;
							previousSpan = g_pSceneSpanDataCur++;
							previousSpan->xStart = scanX;
							previousSpan->xEnd = scanX + runLength;
							previousSpan->face = &g_sw3dCockpitMaskSentinelFace;
							previousSpan->next = NULL;
						}
						runType = -runType;
						scanX += runLength;
					} while (scanX < g_flightVpWidth);
				}
				++scanline;
				++scanY;
			} while (scanY < g_flightVpHeight);
		}
	} else {
		g_visFacePassStart = g_visFaceCount;
	}
	g_phongSlotStride =
		((unsigned int)g_flightVpWidth + g_sw3dLightSampleBlockSize - 1) / g_sw3dLightSampleBlockSize;
	g_sw3dLightSampleCacheSceneStampBase += g_flightVpHeight;
	g_invProjScale = 1.0f / (float)(unsigned int)g_projScaleInt;
	if (g_useHardware3D != 0)
		RenderScene_InitHardwareFrame();
}

// FUNCTION: XVT 0x486200
int RenderScene_UnlockBuffers(void) {
	Memory_UnlockHandle(g_sceneSpanDataHandle);
	Memory_UnlockHandle(g_sceneSpanPtrListHandle);
	Memory_UnlockHandle(g_visFaceListHandle);
	Memory_UnlockHandle(g_projVertListHandle);
	Memory_UnlockHandle(g_sceneEdgeListHandle);
	Memory_UnlockHandle(g_vertexRemapHandle);
	Memory_UnlockHandle(g_sceneEdgeFlagsHandle);
	Memory_UnlockHandle(g_sceneSclEdgeListHandle);
	Memory_UnlockHandle(g_scanlineSpanHeadsHandle);
	Memory_UnlockHandle(g_scenePhongDataHandle);
	Memory_UnlockHandle(g_meshQueueHandle);
	g_sceneSpanDataBase = NULL;
	g_sceneSpanPtrList = NULL;
	g_visFaceList = NULL;
	g_projVertList = NULL;
	g_sceneEdgeList = NULL;
	g_vertexRemap = NULL;
	g_sceneEdgeFlags = NULL;
	g_sceneSclEdgeList = NULL;
	g_scanlineSpanHeads = NULL;
	g_scenePhongData = NULL;
	g_meshQueue = NULL;
	return 0;
}

// FUNCTION: XVT 0x4862E0
void RenderScene_FreeBuffers(void) {
	if (g_sceneSpanDataHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_sceneSpanDataHandle));
	}
	g_sceneSpanDataHandle = 0;
	if (g_sceneSpanPtrListHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_sceneSpanPtrListHandle));
	}
	g_sceneSpanPtrListHandle = 0;
	if (g_visFaceListHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_visFaceListHandle));
	}
	g_visFaceListHandle = 0;
	if (g_projVertListHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_projVertListHandle));
	}
	g_projVertListHandle = 0;
	if (g_sceneEdgeListHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_sceneEdgeListHandle));
	}
	g_sceneEdgeListHandle = 0;
	if (g_vertexRemapHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_vertexRemapHandle));
	}
	g_vertexRemapHandle = 0;
	if (g_sceneEdgeFlagsHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_sceneEdgeFlagsHandle));
	}
	g_sceneEdgeFlagsHandle = 0;
	if (g_sceneSclEdgeListHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_sceneSclEdgeListHandle));
	}
	g_sceneSclEdgeListHandle = 0;
	if (g_scanlineSpanHeadsHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_scanlineSpanHeadsHandle));
	}
	g_scanlineSpanHeadsHandle = 0;
	if (g_scenePhongDataHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_scenePhongDataHandle));
	}
	g_scenePhongDataHandle = 0;
	if (g_meshQueueHandle != 0) {
		Memory_FreeHandle(RenderScene_GetMemoryHandle(&g_meshQueueHandle));
	}
	g_meshQueueHandle = 0;
}
