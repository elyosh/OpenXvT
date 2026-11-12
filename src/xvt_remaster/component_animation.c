#include "xvt_remaster/component_animation.h"
#include <string.h>

typedef struct ComponentPose {
	uint64_t frame, event, model, revision;
	uint16_t signature;
	uint8_t type, valid;
	uint8_t from[XVT_SNAP_COMPONENTS], to[XVT_SNAP_COMPONENTS];
	uint8_t hp[XVT_SNAP_COMPONENTS], state[XVT_SNAP_COMPONENTS];
	float current[XVT_SNAP_COMPONENTS], previous[XVT_SNAP_COMPONENTS];
} ComponentPose;

static ComponentPose g_poses[XVT_SNAP_OBJECTS];
static uint64_t g_mission, g_world, g_frame;
static int g_changed;

void XvtComponentAnimation_Reset(void) {
	memset(g_poses, 0, sizeof g_poses);
	g_mission = g_world = g_frame = 0;
	g_changed = 0;
}

void XvtComponentAnimation_Prepare(const XvtRenderSnapshot* s) {
	if (!s->flight_valid || !s->flight_unlocked) {
		XvtComponentAnimation_Reset();
		return;
	}
	if (s->mission_generation != g_mission || s->world_generation != g_world)
		XvtComponentAnimation_Reset();
	g_mission = s->mission_generation;
	g_world = s->world_generation;
	if (g_frame == s->flight_frame_serial)
		return;
	g_changed = 0;
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XvtSnapObject* o = &s->objects[i];
		if (!o->has_craft || o->id.slot >= XVT_SNAP_OBJECTS)
			continue;
		ComponentPose* p = &g_poses[o->id.slot];
		int reset =
			!p->valid || p->frame != g_frame || p->signature != o->id.signature || p->type != o->object_type;
		uint64_t model = o->object_type < XVT_SNAP_TYPES ? s->types[o->object_type].model_asset_id : 0;
		reset |= p->model != model;
		int object_changed = 0;
		int event = p->event != s->component_event_serial;
		if (event && p->event + 1 != s->component_event_serial)
			reset = 1;
		int64_t age = (int64_t)s->view_time_ticks - s->component_event_time;
		float alpha = age < 0 ? 0 : age >= 32 ? 1 : (float)age / 32;
		for (unsigned j = 0; j < XVT_SNAP_COMPONENTS; ++j) {
			int discontinuity =
				reset || p->hp[j] != o->component_hp[j] || p->state[j] != o->component_state[j];
			if (!event && p->to[j] != o->mesh_rotation[j])
				discontinuity = 1;
			p->previous[j] = p->current[j];
			if (discontinuity)
				p->from[j] = p->to[j] = o->mesh_rotation[j];
			else if (event) {
				p->from[j] = p->to[j];
				p->to[j] = o->mesh_rotation[j];
			}
			int delta = (int8_t)(uint8_t)(p->to[j] - p->from[j]);
			p->current[j] = p->from[j] + delta * alpha;
			if (discontinuity)
				p->previous[j] = p->current[j];
			if (p->previous[j] != p->current[j])
				object_changed = g_changed = 1;
			p->hp[j] = o->component_hp[j];
			p->state[j] = o->component_state[j];
		}
		if (object_changed)
			++p->revision;
		p->model = model;
		p->event = s->component_event_serial;
		p->frame = s->flight_frame_serial;
		p->signature = o->id.signature;
		p->type = o->object_type;
		p->valid = 1;
	}
	g_frame = s->flight_frame_serial;
}

int XvtComponentAnimation_Changed(void) { return g_changed; }

const float* XvtComponentAnimation_Angles(const XvtRenderSnapshot* s, const XvtSnapObject* o,
										  const XvtMeshAsset* asset, float output[XVT_SNAP_COMPONENTS]) {
	if (!s || !o || !asset || !s->flight_unlocked || o->id.slot >= XVT_SNAP_OBJECTS)
		return NULL;
	const ComponentPose* p = &g_poses[o->id.slot];
	if (!p->valid || p->signature != o->id.signature || p->type != o->object_type)
		return NULL;
	const float* angles = s->flight_frame_serial == g_frame ? p->current : p->previous;
	for (unsigned i = 0; i < XVT_SNAP_COMPONENTS; ++i) {
		output[i] = o->mesh_rotation[i];
		if (i >= asset->component_count || o->component_state[i] || !o->component_hp[i])
			continue;
		unsigned type = asset->mesh->component_rot[i].mesh_type;
		int foil = (type == 20 && (o->object_type == 1 || o->object_type == 4)) ||
				   (type == 7 && o->object_type == 4);
		int rotate =
			type == 21 || type == 11 || type == 12 || type == 13 || type == 23 || type == 24 || type == 25;
		if (rotate || (foil && (o->sfoil_state & 1)))
			output[i] = angles[i];
	}
	return output;
}

uint64_t XvtComponentAnimation_ObjectRevision(unsigned slot) {
	return slot < XVT_SNAP_OBJECTS && g_poses[slot].valid ? g_poses[slot].revision : 0;
}
