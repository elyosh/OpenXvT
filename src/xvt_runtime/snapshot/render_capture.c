#include "xvt_runtime/snapshot/render_capture.h"
#include "xvt_runtime/snapshot/cockpit_capture.h"
#include "xvt_runtime/snapshot/cockpit_messages.h"
#include "xvt_runtime/snapshot/render_camera.h"
#include "xvt_runtime/timing/flight_timing.h"

#include "xvt_runtime/runtime/presentation.h"

#include "aeron/aeron.h"
#include "xvt/assets/model_preview.h"
#include "xvt/assets/object_type.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/proving_grounds.h"
#include "xvt/flight/transfm2.h"
#include "xvt/render/backdrop.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/renderer.h"
#include "xvt/util/memory.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/snapshot/render_assets.h"
#include "xvt_runtime/snapshot/render_frontend.h"
#include "xvt_runtime/snapshot/render_hud.h"
#include "xvt_runtime/snapshot/render_map.h"
#include <string.h>

/* One in-progress view; completed views live in the root snapshot slots.
 * Failed later flips cannot overwrite an earlier successful view in the tick. */
static struct {
	XvtSnapCamera camera;
	XvtSnapLighting lighting;
	XvtSnapMap map;
	XvtSnapPreview crt;
	XvtSnapSky sky;
	XvtSnapHyperspace hyperspace;
	XvtSnapObject objects[XVT_SNAP_OBJECTS];
	XvtSnapType types[XVT_SNAP_TYPES];
	int16_t fuselage[25];
	uint32_t count, dropped;
	int32_t time;
	uint64_t component_event_serial;
	int32_t component_event_time;
	uint8_t flight_unlocked;
	int valid, sealed;
} g_pending;

typedef struct XvtAuthoritativePose {
	int32_t position[3];
	uint16_t signature, yaw, pitch, roll;
	uint8_t type, mesh[50];
} XvtAuthoritativePose;

static XvtAuthoritativePose g_authoritativePoses[XVT_SNAP_OBJECTS], g_candidatePoses[XVT_SNAP_OBJECTS];
static int g_candidateTick = -1;
static int g_authoritativeTick = -1, g_networkCorrection;
static unsigned g_networkCorrections;

static uint64_t g_mission, g_world, g_serial;
static int g_active, g_published;
static int32_t g_lastViewTime;
static int g_hasViewTime;
static uint32_t g_lastDropped;
static unsigned g_overlayDepth;

void XvtRenderCapture_BeginOverlay(void) {
	XvtPresentation_RequireClassic();
	if (!g_overlayDepth++) {
		XvtCockpit_RetainPresentedFrame();
	}
}

void XvtRenderCapture_EndOverlay(void) {
	if (g_overlayDepth)
		--g_overlayDepth;
}

void XvtRenderCapture_Init(void) {
	XvtCockpit_Reset();
	XvtCockpitMessages_ClearProgress();
	memset(&g_pending, 0, sizeof g_pending);
	g_mission = g_world = g_serial = 0;
	g_active = g_published = g_hasViewTime = 0;
	g_lastDropped = 0;
	g_overlayDepth = 0;
}

void XvtRenderCapture_BeginTick(void) { g_pending.valid = g_pending.sealed = g_published = 0; }

static void InvalidateWorldHistory(void) {
	g_authoritativeTick = g_candidateTick = -1;
	g_networkCorrection = 0;
	++g_world;
	g_pending.valid = g_pending.sealed = g_published = g_hasViewTime = 0;
	XvtRenderSnapshot* writer = XvtRenderSnapshot_Writer();
	if (writer)
		writer->flight_valid = writer->camera.valid = 0;
}

void XvtRenderCapture_WorldChanged(void) {
	XvtCockpit_Reset();
	InvalidateWorldHistory();
}

void XvtRenderCapture_BeginMission(void) {
	XvtPresentation_RequireClassic();
	XvtRenderHud_Reset();
	++g_mission;
	g_active = 1;
	XvtRenderCapture_WorldChanged();
}

void XvtRenderCapture_EndMission(void) {
	XvtPresentation_RequireClassic();
	g_active = 0;
	XvtRenderCapture_WorldChanged();
}

static XvtSnapObjectId ObjectId(unsigned slot, unsigned capacity) {
	XvtSnapObjectId id = { UINT16_MAX, 0 };
	if (slot < capacity && g_objectTable[slot].objectType) {
		id.slot = (uint16_t)slot;
		id.signature = g_objectTable[slot].objectSignature;
	}
	return id;
}

static void CaptureCamera(XvtSnapCamera* out, XvtSnapLighting* lighting, unsigned capacity) {
	const PlayerData* p = &g_players[g_localPlayer];
	const PlayerViewState* v = &p->viewState;
	memset(out, 0, sizeof *out);
	out->world_pos[0] = v->savedTargetX;
	out->world_pos[1] = v->savedTargetY;
	out->world_pos[2] = v->savedTargetZ;
	XvtRenderCamera_CopyRows(out->rows);
	out->viewport = (XvtSnapRect) { g_flightVpX, g_flightVpY, g_flightVpWidth, g_flightVpHeight };
	out->center_x = g_flightVpCenterX;
	out->center_y = g_flightVpCenterY;
	out->projection_offset_y = g_projOffsetY;
	out->screen_width = (uint16_t)g_screenWidth;
	out->screen_height = (uint16_t)g_screenHeight;
	out->aspect_y_q16 = g_projAspectY;
	out->perspective_shift = perspShift;
	out->player = ObjectId(p->objectIndex, capacity);
	out->focus = ObjectId(v->cameraFocusObjIdx, capacity);
	out->view_pitch = (uint16_t)v->viewPitch;
	out->view_yaw = (uint16_t)v->viewYaw;
	out->view_roll = (uint16_t)v->viewRoll;
	out->view_angle_d = (uint16_t)v->viewAngleD;
	out->hud_aim_x = v->hudAimX;
	out->hud_aim_y = v->hudAimY;
	out->external = v->externalCameraActive;
	out->replay_view = g_replayViewMode;
	out->map_mode = p->mapCameraState;
	out->hyperspace_phase = p->hyperspacePhase;
	out->hud_state = v->hudStateLive;
	out->valid = g_flightVpWidth && g_flightVpHeight && g_screenWidth && g_screenHeight;
	*lighting = (XvtSnapLighting) { { g_modelPreviewLightDirectionX, g_modelPreviewLightDirectionY,
									  g_modelPreviewLightDirectionZ },
									g_localLightsLevel,
									g_dirLightingEnabled };
}

static void CaptureObject(unsigned slot) {
	const ObjectRecord* o = &g_objectTable[slot];
	if (!o->objectType)
		return;
	if (g_pending.count == XVT_SNAP_OBJECTS) {
		++g_pending.dropped;
		return;
	}
	XvtSnapObject* out = &g_pending.objects[g_pending.count++];
	memset(out, 0, sizeof *out);
	out->id = (XvtSnapObjectId) { (uint16_t)slot, o->objectSignature };
	out->object_type = o->objectType;
	out->genus = o->genusId;
	out->flight_group = o->flightGroupIdx;
	out->slot_class =
		slot >= (unsigned)g_regionMainObjectSlotEnd
			? XVT_SLOT_STATIC
			: (slot >= (unsigned)g_localTransientSlotStart && slot < (unsigned)g_localDebrisSlotEnd
				   ? XVT_SLOT_LOCAL_TRANSIENT
				   : XVT_SLOT_MAIN);
	out->world_pos[0] = o->world_x;
	out->world_pos[1] = o->world_y;
	out->world_pos[2] = o->world_z;
	memcpy(out->prev_world_pos, out->world_pos, sizeof out->world_pos);
	out->player_owner = o->playerOwnerIdx;
	out->yaw = o->yaw;
	out->pitch = o->pitch;
	out->roll = o->roll;
	out->type_specific_word = o->typeSpecificWord;
	memcpy(out->type_specific, o->typeSpecificByte, sizeof out->type_specific);
	const MobileObject* m = o->mobj;
	if (!m)
		return;
	out->has_mobile = 1;
	out->state = m->state;
	out->light_scale = m->lightIntensityScale;
	out->prev_world_pos[0] = m->prevWorldX;
	out->prev_world_pos[1] = m->prevWorldY;
	out->prev_world_pos[2] = m->prevWorldZ;
	out->source_slot = m->sourceObjIdx;
	out->source_type = m->sourceObjectType;
	out->iff = m->iff;
	out->team = m->team;
	out->node_switch = m->nodeSwitchIndex;
	out->speed = m->speed;
	out->move_dirty = m->moveVectorDirty;
	out->orient_dirty = m->orientMatrixDirty;
	out->move_q15[0] = m->moveX;
	out->move_q15[1] = m->moveY;
	out->move_q15[2] = m->moveZ;
	const int16_t rows[9] = { m->cachedSideX, m->cachedSideY, m->cachedSideZ, m->cachedFwdX, m->cachedFwdY,
							  m->cachedFwdZ,  m->cachedUpX,   m->cachedUpY,   m->cachedUpZ };
	memcpy(out->cached_rows_q15, rows, sizeof rows);
	const CraftData* c = m->pCraft;
	if (!c)
		return;
	out->has_craft = 1;
	out->sfoil_state = c->sFoilState;
	out->object_kind = c->objectKind;
	out->working_subsystems = c->workingSubsystems;
	out->installed_subsystems = c->systemFlags;
	out->throttle = c->throttleSpeed;
	out->engine_output = c->engineOutputScale;
	out->max_speed = c->aiFlight.maxSpeedCache;
	out->laser_redirect = c->laserRedirect;
	out->shield_redirect = c->shieldRedirect;
	out->beam_level = c->beamLevel;
	memcpy(out->component_state, c->componentState, sizeof out->component_state);
	memcpy(out->component_hp, c->componentHp, sizeof out->component_hp);
	memcpy(out->mesh_rotation, c->meshRotation, sizeof out->mesh_rotation);
}

typedef struct Sequence {
	const int16_t* data;
	size_t count;
} Sequence;

static const Sequence g_sequences[] = {
	{ g_modelType127TextureFrameSequence, sizeof g_modelType127TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType131TextureFrameSequence, sizeof g_modelType131TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType132TextureFrameSequence, sizeof g_modelType132TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType133TextureFrameSequence, sizeof g_modelType133TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType134TextureFrameSequence, sizeof g_modelType134TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType157TextureFrameSequence, sizeof g_modelType157TextureFrameSequence / sizeof(int16_t) },
	{ g_fuselageDamageTextureFrameSequence, sizeof g_fuselageDamageTextureFrameSequence / sizeof(int16_t) },
	{ g_modelType110TextureFrameSequence, sizeof g_modelType110TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType111TextureFrameSequence, sizeof g_modelType111TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType112TextureFrameSequence, sizeof g_modelType112TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType113TextureFrameSequence, sizeof g_modelType113TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType128TextureFrameSequence, sizeof g_modelType128TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType129TextureFrameSequence, sizeof g_modelType129TextureFrameSequence / sizeof(int16_t) },
	{ g_modelType130TextureFrameSequence, sizeof g_modelType130TextureFrameSequence / sizeof(int16_t) }
};

static void CaptureTypes(void) {
	for (unsigned i = 0; i < XVT_SNAP_TYPES; ++i) {
		const ModelTypeInfo* t = &g_modelTypeTable[i];
		XvtSnapType* out = &g_pending.types[i];
		memset(out, 0, sizeof *out);
		uint64_t id = XvtRenderAssets_HandleId(t->curTexLevel);
		if (t->assetFlags & 1)
			out->model_asset_id = id;
		else if (t->assetFlags & 2)
			out->texture_asset_id = id;
		out->max_extent = t->maxBoundsExtent;
		out->half_extent = t->halfBoundsExtent;
		out->record_flags = t->recordFlags;
		out->asset_flags = t->assetFlags;
		out->flags = t->flags;
		out->model_index = t->modelIndex;
		out->family = t->familyId;
		out->genus = t->genusId;
		out->texture_group = t->textureGroup;
		out->resource_entry = t->frameCount;
		for (unsigned j = 0; j < sizeof g_sequences / sizeof g_sequences[0]; ++j) {
			if (t->textureFrameSequence != g_sequences[j].data)
				continue;
			out->sequence_count = (uint8_t)g_sequences[j].count;
			memcpy(out->sequence, g_sequences[j].data, g_sequences[j].count * sizeof(int16_t));
			break;
		}
		for (unsigned j = 0; j < 17; ++j)
			if (t->palette == g_modelTypePaletteRemaps[j])
				out->remap_count = 16;
		if (t->palette == g_modelType110Palette || t->palette == g_modelType111Palette ||
			t->palette == g_modelType112Palette || t->palette == g_modelType113Palette)
			out->remap_count = 16;
		if (out->remap_count)
			memcpy(out->remap, t->palette, out->remap_count);
		if ((t->textureFrameSequence && !out->sequence_count) || (t->palette && !out->remap_count))
			++g_pending.dropped;
	}
	memcpy(g_pending.fuselage, g_fuselageDamageTextureFrameSequence, sizeof g_pending.fuselage);
}

void XvtRenderCapture_CaptureView(void) {
	XvtRenderDraw_Scope(XVT_SCOPE_WORLD);
	g_pending.valid = g_pending.sealed = 0;
	g_pending.count = g_pending.dropped = 0;
	if (!g_active || !XvtRenderSnapshot_Writer() || !g_objectTable || !g_objectTableHandle ||
		(unsigned)g_localPlayer >= 8)
		return;
	size_t capacity = g_handleTables.sizeTable[g_objectTableHandle - 1] / sizeof(ObjectRecord);
	if (g_handleTables.ptrTable[g_objectTableHandle - 1] != g_objectTable || g_regionMainObjectSlotEnd < 0 ||
		g_regionStaticObjectSlotCount < 0)
		return;
	size_t end = (size_t)g_regionMainObjectSlotEnd + (size_t)g_regionStaticObjectSlotCount;
	if (end > capacity) {
		end = capacity;
		++g_pending.dropped;
	}
	if (end > UINT16_MAX)
		end = UINT16_MAX;
	CaptureCamera(&g_pending.camera, &g_pending.lighting, (unsigned)end);
	g_pending.component_event_serial = XvtFlightTiming_AnimationSerial();
	g_pending.component_event_time = XvtFlightTiming_AnimationTime();
	g_pending.flight_unlocked = XvtFlightTiming_IsUnlocked();
	g_pending.sky.debris_enabled = g_debrisEnabled;
	g_pending.sky.proving_grounds = g_flightMissionState.provingGroundsModeActive;
	g_pending.sky.star_density = g_starDensity;
	g_pending.sky.backdrop_enabled = g_backdropsEnabled;
	g_pending.sky.checkpoint_slot = g_provingGroundsCurrentCheckpointObjIdx;
	g_pending.sky.craft_slot_end = (uint16_t)g_activeRegionCraftObjectSlotEnd;
	memcpy(g_pending.sky.backdrop_types, g_backdropModelTypes, sizeof g_pending.sky.backdrop_types);
	memcpy(g_pending.sky.backdrop_directions, g_backdropPackedDirections,
		   sizeof g_pending.sky.backdrop_directions);
	/* Counts and records share the recovered Y, X, Z face order. */
	const uint16_t counts[6] = {
		g_backdropPositiveYCount, g_backdropNegativeYCount, g_backdropPositiveXCount,
		g_backdropNegativeXCount, g_backdropPositiveZCount, g_backdropNegativeZCount
	};
	memcpy(g_pending.sky.direction_counts, counts, sizeof counts);
	g_pending.hyperspace.phase = g_players[g_localPlayer].hyperspacePhase;
	g_pending.hyperspace.elapsed_ticks = g_players[g_localPlayer].hyperspaceRuntime.phaseElapsedTicks;
	g_pending.hyperspace.iff = g_players[g_localPlayer].iff;
	g_pending.hyperspace.valid = g_pending.hyperspace.phase == XVT_SNAP_HYPERSPACE_TRANSITION;
	g_pending.hyperspace.count = 0;
	for (unsigned i = 0; i < end; ++i)
		CaptureObject(i);
	XvtRenderMap_Capture(&g_pending.map, g_pending.objects, g_pending.count);
	if (g_pending.map.active)
		XvtRenderDraw_Scope(XVT_SCOPE_MAP);
	CaptureTypes();
	g_pending.time = g_gameTime;
	g_pending.valid = g_pending.camera.valid;
}

void XvtRenderCapture_SealView(void) {
	XvtCockpit_Seal(&g_pending.crt);
	g_pending.sealed = g_pending.valid;
}

void XvtRenderCapture_EndPresentation(void) { g_pending.valid = g_pending.sealed = 0; }

void XvtRenderCapture_Presented(int succeeded) {
	XvtRenderSnapshot* out = XvtRenderSnapshot_Writer();
	if (!succeeded || !out)
		return;
	if (!g_pending.sealed) {
		if (g_overlayDepth) {
			XvtCockpit_Presented(1);

			XvtRenderFrontend_FlightUiScene(XvtFlightTask_IsLoading() ? XVT_SCENE_LOADING : XVT_SCENE_FLIGHT);
		}
		return;
	}
	XvtCockpit_Presented(0);
	if (g_hasViewTime && g_pending.time < g_lastViewTime)
		++g_world;
	if (XvtFlightTiming_IsNetwork125() && g_candidateTick == g_pending.time) {
		memcpy(g_authoritativePoses, g_candidatePoses, sizeof g_authoritativePoses);
		g_authoritativeTick = g_candidateTick;
	}
	g_lastViewTime = g_pending.time;
	g_hasViewTime = 1;
	out->camera = g_pending.camera;
	out->lighting = g_pending.lighting;
	out->map = g_pending.map;
	XvtRenderHud_Publish(out);

	XvtRenderFrontend_PresentedScene(XVT_SCENE_FLIGHT);
	out->sky = g_pending.sky;
	out->hyperspace = g_pending.hyperspace;
	out->object_count = g_pending.count;
	memcpy(out->objects, g_pending.objects, out->object_count * sizeof out->objects[0]);
	memcpy(out->types, g_pending.types, sizeof out->types);
	memcpy(out->fuselage_sequence, g_pending.fuselage, sizeof out->fuselage_sequence);
	out->view_time_ticks = g_pending.time;
	out->component_event_serial = g_pending.component_event_serial;
	out->component_event_time = g_pending.component_event_time;
	out->flight_unlocked = g_pending.flight_unlocked;
	out->flight_frame_serial = ++g_serial;
	out->flight_valid = 1;
	out->dropped_records += g_pending.dropped;
	if (g_pending.dropped && g_pending.dropped != g_lastDropped)
		Aeron_LogWarn("xvt.snapshot", "main-view capture dropped %u records or unsupported type arrays",
					  g_pending.dropped);
	g_lastDropped = g_pending.dropped;
	g_published = 1;
	g_pending.sealed = 0;
}

void XvtRenderCapture_Commit(XvtRenderSnapshot* out, const XvtRenderSnapshot* previous) {
	XvtCockpit_Export(&out->cockpit);
	XvtCockpit_ExportResources(&out->cockpit_resources);
	out->mission_generation = g_mission;
	out->world_generation = g_world;
	out->flight_frame_serial = g_serial;

	if (!g_active) {
		out->flight_valid = out->camera.valid = 0;
		out->object_count = 0;
		return;
	}
	if (g_published)
		return;
	if (!previous || !previous->flight_valid || previous->mission_generation != g_mission ||
		previous->world_generation != g_world)
		return;
	out->camera = previous->camera;
	out->lighting = previous->lighting;
	out->map = previous->map;
	out->target_box_count = previous->target_box_count;
	memcpy(out->target_boxes, previous->target_boxes,
		   previous->target_box_count * sizeof out->target_boxes[0]);
	out->sky = previous->sky;
	out->hyperspace = previous->hyperspace;
	out->object_count = previous->object_count;
	memcpy(out->objects, previous->objects, out->object_count * sizeof out->objects[0]);
	memcpy(out->types, previous->types, sizeof out->types);
	memcpy(out->fuselage_sequence, previous->fuselage_sequence, sizeof out->fuselage_sequence);
	out->view_time_ticks = previous->view_time_ticks;
	out->component_event_serial = previous->component_event_serial;
	out->component_event_time = previous->component_event_time;
	out->flight_unlocked = previous->flight_unlocked;
	out->flight_frame_serial = previous->flight_frame_serial;
	out->flight_valid = 1;
}

void XvtRenderCapture_BeginClassicFrame(void) {
	XvtCockpit_BeginFrame();
	g_pending.crt.valid = 0;
	XvtRenderHud_BeginFrame();
}

void XvtRenderCapture_Hyperspace(unsigned count, const int* x, const int* y, const int* z, const int* width,
								 const int* roll) {
	if (!g_pending.valid || count > XVT_SNAP_STREAKS)
		return;
	g_pending.hyperspace.count = count;
	for (unsigned i = 0; i < count; ++i)
		g_pending.hyperspace.streaks[i] =
			(XvtSnapStreak) { { x[i], y[i], z[i] }, width[i], (uint16_t)roll[i] };
}

void XvtRenderCapture_FrontendPreview(uint16_t handle, const float position[3], const float orientation[9],
									  float scale, uint16_t node_switch, int x, int y, int width,
									  int height) {
	XvtRenderSnapshot* s = XvtRenderSnapshot_Writer();
	if (!s || width <= 0 || height <= 0 || (unsigned)g_localPlayer >= 8)
		return;
	if (s->preview_count == XVT_SNAP_PREVIEWS) {
		++s->dropped_records;
		return;
	}
	XvtSnapPreview* out = &s->previews[s->preview_count++];
	memset(out, 0, sizeof *out);
	out->opt_asset_id = XvtRenderAssets_HandleId(handle);
	CaptureCamera(&out->camera, &out->lighting, 0);
	out->camera.screen_width = 640;
	out->camera.screen_height = 480;
	out->camera.valid = 1;
	out->camera.projection_offset_y = g_projOffsetY;
	memcpy(out->view_pos, position, sizeof out->view_pos);
	memcpy(out->view_orient, orientation, sizeof out->view_orient);
	out->model_scale = scale;
	out->node_switch = node_switch;
	out->component = UINT16_MAX;
	out->object.slot = UINT16_MAX;
	out->mask_index = UINT8_MAX;
	out->destination = (XvtSnapRect) { x, y, width, height };

	out->valid = out->opt_asset_id != 0;
}

void XvtRenderCapture_Crt(int x, int y, int width, int height, int masked) {
	if (!g_active || !g_objectTableHandle || (unsigned)g_localPlayer >= 8 || width <= 0 || height <= 0)
		return;
	unsigned capacity = (unsigned)(g_handleTables.sizeTable[g_objectTableHandle - 1] / sizeof(ObjectRecord));
	unsigned target = g_players[g_localPlayer].currentTargetObjectIdx;
	XvtSnapPreview* out = &g_pending.crt;
	memset(out, 0, sizeof *out);
	out->object = ObjectId(target, capacity);
	if (out->object.slot == UINT16_MAX)
		return;
	const ObjectRecord* object = &g_objectTable[target];
	if (object->objectType >= XVT_SNAP_TYPES)
		return;
	out->opt_asset_id = XvtRenderAssets_HandleId(g_modelTypeTable[object->objectType].curTexLevel);
	CaptureCamera(&out->camera, &out->lighting, capacity);
	out->node_switch = object->mobj ? object->mobj->nodeSwitchIndex : 0;
	out->model_scale = 1;
	out->component = (uint16_t)g_players[g_localPlayer].selectedTargetComponent;
	/* The argument requests refreshing the mask, not disabling an existing mask. */
	(void)masked;
	out->mask_index = (uint8_t)(g_hudInstrumentSetBaseIndex / 144);
	out->destination = (XvtSnapRect) { x, y, width, height };

	out->valid = out->camera.valid;
}

void XvtRenderCapture_CrtMarker(int x, int y, int z) {
	g_pending.crt.component_marker_valid = 1;
	const int v[3] = { x, y, z };
	for (int i = 0; i < 3; ++i)
		g_pending.crt.component_marker_world[i] =
			(int32_t)((uint32_t)v[i] + (uint32_t)g_pending.crt.camera.world_pos[i]);
}

static XvtAuthoritativePose XvtRenderCapture_AuthoritativePose(unsigned slot) {
	XvtAuthoritativePose pose = { 0 };
	const ObjectRecord* o = &g_objectTable[slot];
	if (!o->objectType)
		return pose;
	pose.type = o->objectType;
	pose.signature = o->objectSignature;
	pose.position[0] = o->world_x;
	pose.position[1] = o->world_y;
	pose.position[2] = o->world_z;
	pose.yaw = o->yaw;
	pose.pitch = o->pitch;
	pose.roll = o->roll;
	if (o->mobj && o->mobj->pCraft)
		memcpy(pose.mesh, o->mobj->pCraft->meshRotation, sizeof pose.mesh);
	return pose;
}

void XvtRenderCapture_CheckNetworkCorrection(void) {
	if (!XvtFlightTiming_IsNetwork125() || g_gameTime != g_authoritativeTick)
		return;
	unsigned end = (unsigned)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount);
	if (end > XVT_SNAP_OBJECTS)
		end = XVT_SNAP_OBJECTS;
	for (unsigned slot = 0; slot < end; ++slot) {
		if (slot >= (unsigned)g_localTransientSlotStart && slot < (unsigned)g_localDebrisSlotEnd)
			continue;
		XvtAuthoritativePose pose = XvtRenderCapture_AuthoritativePose(slot);
		const XvtAuthoritativePose* previous = &g_authoritativePoses[slot];
		if (pose.type != previous->type || pose.signature != previous->signature ||
			pose.yaw != previous->yaw || pose.pitch != previous->pitch || pose.roll != previous->roll ||
			memcmp(pose.position, previous->position, sizeof pose.position) ||
			memcmp(pose.mesh, previous->mesh, sizeof pose.mesh)) {
			g_networkCorrection = 1;
			return;
		}
	}
}

void XvtRenderCapture_CompleteNetworkWorld(void) {
	if (!XvtFlightTiming_IsNetwork125())
		return;
	/* Prediction corrections invalidate world interpolation, but the retained
	 * cockpit still represents the classic HUD surface composed by the next frame. */
	if (g_networkCorrection) {
		++g_networkCorrections;
		InvalidateWorldHistory();
	}
	unsigned end = (unsigned)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount);
	if (end > XVT_SNAP_OBJECTS)
		end = XVT_SNAP_OBJECTS;
	for (unsigned slot = 0; slot < end; ++slot)
		g_candidatePoses[slot] = XvtRenderCapture_AuthoritativePose(slot);
	g_candidateTick = g_gameTime;
}

unsigned XvtRenderCapture_NetworkCorrections(void) { return g_networkCorrections; }

int XvtRenderCapture_LastViewTick(void) { return g_hasViewTime ? g_lastViewTime : -1; }
