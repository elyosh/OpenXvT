#include "xvt_remaster/effects.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/world.h"
#include "xvt_remaster/assets.h"
#include "xvt_remaster/config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float kTau = 6.2831853071795864769f;

int XvtEffects_Frame(const XvtRenderSnapshot* s, unsigned type, unsigned frame, XvtEffectFrame* out) {
	if (type >= XVT_SNAP_TYPES)
		return 0;
	const AeronRuntimeAtlas* a =
		XvtRemasterAssets_Image(s->types[type].texture_asset_id, NULL, UINT16_MAX, UINT16_MAX);
	if (!a)
		return 0;
	for (int i = 0; i < a->layout.frame_count; ++i) {
		if ((unsigned)a->layout.ids[i] != frame)
			continue;
		const AeronRuntimeAtlasPage* page = &a->pages[a->layout.pages[i]];
		const AeronSpriteRect* rect = &a->layout.frames[i];
		*out = (XvtEffectFrame) { page->texture,
								  rect->x / page->width,
								  rect->y / page->height,
								  (rect->x + rect->w) / page->width,
								  (rect->y + rect->h) / page->height,
								  a->layout.size[i * 2],
								  a->layout.size[i * 2 + 1] };
		return 1;
	}
	return 0;
}

void XvtEffects_Quad(const XvtSnapCamera* cam, const float center[3], float hw, float hh, float angle,
					 float out[4][3]) {
	static const int sx[4] = { 1, -1, -1, 1 }, sy[4] = { 1, 1, -1, -1 };
	float c = cosf(angle), sn = sinf(angle);
	float aspect =
		cam->aspect_y_q16 && cam->aspect_y_q16 != UINT16_MAX ? (float)cam->aspect_y_q16 / 65536.0f : 1;
	for (int corner = 0; corner < 4; ++corner) {
		float x = c * sx[corner] * hw + sn * sy[corner] * hh * aspect,
			  y = c * sy[corner] * hh - sn * sx[corner] * hw / aspect;
		for (int axis = 0; axis < 3; ++axis)
			out[corner][axis] = center[axis] + cam->rows[axis] * x + cam->rows[3 + axis] * y;
	}
}

void XvtEffects_SetFrame(AeronSceneBillboardDesc* b, const XvtEffectFrame* f, float strength, float alpha) {
	b->texture = f->texture;
	b->blend = AERON_SCENE_BILLBOARD_BLEND_ALPHA;
	const float uv[4][2] = { { f->u0, f->v0 }, { f->u1, f->v0 }, { f->u1, f->v1 }, { f->u0, f->v1 } };
	memcpy(b->uv, uv, sizeof uv);
	for (int i = 0; i < 4; ++i) {
		b->colors[i][0] = b->colors[i][1] = b->colors[i][2] = strength;
		b->colors[i][3] = alpha;
	}
}

static float Angle(const XvtSnapObject* o, const XvtSnapCamera* cam) {
	float m[16], r[2][3];
	XvtRenderMath_ObjectMatrix(o, cam->world_pos, m);
	for (int col = 0; col < 2; ++col)
		for (int row = 0; row < 3; ++row)
			r[col][row] = cam->rows[row * 3] * m[col] + cam->rows[row * 3 + 1] * m[4 + col] +
						  cam->rows[row * 3 + 2] * m[8 + col];
	const float* v = fabsf(r[1][2]) > fabsf(r[0][2]) ? r[0] : r[1];
	return (v[0] < 0 ? 1.0f : -1.0f) * atan2f(v[1], fabsf(v[0]));
}

static uint16_t FrameCode(const XvtRenderSnapshot* s, const XvtSnapObject* o) {
	if (o->object_type >= XVT_SNAP_TYPES)
		return XVT_SNAP_INVALID_TEXTURE_FRAME;
	if (o->object_type == XVT_SNAP_TYPE_COMPONENT) {
		/* QueueObjectTextured tests the resolved source type again for its follow-up. */
		if (o->source_type != XVT_SNAP_TYPE_COMPONENT ||
			o->type_specific[1] >= s->types[XVT_SNAP_TYPE_COMPONENT_FOLLOWUP].sequence_count)
			return XVT_SNAP_INVALID_TEXTURE_FRAME;
		return (uint16_t)s->types[XVT_SNAP_TYPE_COMPONENT_FOLLOWUP].sequence[o->type_specific[1]];
	}
	const XvtSnapType* t = &s->types[o->object_type];
	return o->type_specific[0] < t->sequence_count ? (uint16_t)t->sequence[o->type_specific[0]]
												   : XVT_SNAP_INVALID_TEXTURE_FRAME;
}

static int Corners(const XvtRenderSnapshot* s, const XvtSnapObject* o, const XvtSnapCamera* cam,
				   const int32_t origin[3], uint16_t code, unsigned base_size, float roll,
				   float corners[4][3], XvtEffectFrame* frame) {
	if (code < XVT_SNAP_TEXTURE_FRAME_BIT || code >= XVT_SNAP_INVALID_TEXTURE_FRAME)
		return 0;
	unsigned type = (code & 0x7fff) >> 7, index = code & 0x7f;
	if (!XvtEffects_Frame(s, type, index, frame))
		return 0;
	float local[3];
	AeronWorld_LocalI32(cam->world_pos, o->world_pos, local);
	float depth = local[0] * cam->rows[6] + local[1] * cam->rows[7] + local[2] * cam->rows[8];
	if (depth <= 0 || (double)depth > INT32_MAX)
		return 0;
	int d = (int)depth >> 8;
	unsigned q = d ? (uint16_t)s->types[type].max_extent / (unsigned)d : 0;
	unsigned size = (base_size * q) >> 8;
	if (size > 1024)
		size = 1024;
	float fx = ldexpf(1, cam->perspective_shift & 31),
		  aspect =
			  cam->aspect_y_q16 && cam->aspect_y_q16 != UINT16_MAX ? (float)cam->aspect_y_q16 / 65536.0f : 1;
	float hw = (float)((size * (unsigned)frame->width) >> 9) * depth / fx,
		  hh = (float)((size * (unsigned)frame->height) >> 9) * depth / (fx * aspect);
	if (hw <= 0 || hh <= 0)
		return 0;
	AeronWorld_LocalI32(origin, o->world_pos, local);
	XvtEffects_Quad(cam, local, hw, hh, roll, corners);
	return 1;
}

static const XvtSnapObject* Previous(const XvtRenderSnapshot* p, const XvtSnapObject* o) {
	if (!p)
		return NULL;
	for (unsigned i = 0; i < p->object_count; ++i)
		if (p->objects[i].id.slot == o->id.slot && p->objects[i].id.signature == o->id.signature &&
			p->objects[i].object_type == o->object_type)
			return &p->objects[i];
	return NULL;
}

static unsigned BaseSize(const XvtSnapObject* o) {
	unsigned base = o->light_scale ? o->light_scale << 6 : 256;
	return o->light_scale && base >= 256 ? base + 256 : base;
}

static uint16_t FlameCode(const XvtRenderSnapshot* s, const XvtSnapObject* o, int ordinal) {
	if (!o->has_craft || o->object_type >= XVT_SNAP_TYPES || o->id.slot >= s->sky.craft_slot_end)
		return XVT_SNAP_INVALID_TEXTURE_FRAME;
	const XvtMeshAsset* mesh = XvtRemasterShip_Mesh(s, s->types[o->object_type].model_asset_id);
	if (!mesh || mesh->component_count >= XVT_SNAP_COMPONENTS)
		return XVT_SNAP_INVALID_TEXTURE_FRAME;
	unsigned frame = o->component_state[mesh->component_count];
	if (frame >= 25)
		return XVT_SNAP_INVALID_TEXTURE_FRAME;
	for (unsigned i = 0; i < mesh->component_count; ++i)
		if (!o->component_state[i] && mesh->mesh->component_rot[i].mesh_type == XVT_SNAP_MESH_FUSELAGE)
			if (--ordinal == 0)
				return (uint16_t)s->fuselage_sequence[frame];
	return XVT_SNAP_INVALID_TEXTURE_FRAME;
}

static void SubmitOne(AeronScene3D* scene, const XvtRenderSnapshot* s, const XvtRenderSnapshot* p,
					  const XvtSnapObject* o, const XvtSnapCamera* cam, const XvtSnapCamera* pc,
					  uint16_t code, int flame_ordinal, int regenerate) {
	AeronSceneBillboardDesc b = { .stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY };
	XvtEffectFrame frame;
	float roll = Angle(o, cam) + (flame_ordinal ? flame_ordinal * (float)o->roll * kTau / 65536.0f : 0);
	if (!Corners(s, o, cam, cam->world_pos, code, flame_ordinal ? 256 : BaseSize(o), roll, b.corners, &frame))
		return;
	float alpha = 1;
	if (o->genus == CRAFT_GENUS_EXPLOSION && o->type_specific[0] < 32) {
		static const uint8_t envelope[32] = { 208, 224, 240, 240, 224, 208, 176, 144, 112, 80, 48,
											  48,  48,  48,  48,  48,  48,  48,  48,  48,  48, 48,
											  48,  48,  48,  48,  48,  48,  48,  48,  48,  48 };
		alpha = envelope[o->type_specific[0]] / 255.0f;
	}
	float strength =
		o->genus == CRAFT_GENUS_EXPLOSION ? XvtRemasterConfig_Effective()->explosion_emissive_strength : 1;
	XvtEffects_SetFrame(&b, &frame, strength, alpha);
	float previous[4][3];
	const XvtSnapObject* old = Previous(p, o);
	if (regenerate && old && pc) {
		uint16_t old_code = flame_ordinal ? FlameCode(p, old, flame_ordinal) : FrameCode(p, old);
		float old_roll =
			Angle(old, pc) + (flame_ordinal ? flame_ordinal * (float)old->roll * kTau / 65536.0f : 0);
		XvtEffectFrame old_frame;
		if (Corners(p, old, pc, cam->world_pos, old_code, flame_ordinal ? 256 : BaseSize(old), old_roll,
					previous, &old_frame))
			b.prev_corners = previous;
	}
	AeronScene_AddBillboard(scene, &b);
}

typedef struct EffectOrder {
	unsigned index;
	float depth;
} EffectOrder;

static int Order(const void* left, const void* right) {
	const EffectOrder *a = left, *b = right;
	if (a->depth != b->depth)
		return a->depth > b->depth ? -1 : 1;
	return a->index > b->index ? -1 : a->index < b->index;
}

void XvtEffects_Submit(AeronScene3D* scene, const XvtRenderSnapshot* s, const XvtRenderSnapshot* p,
					   const XvtSnapCamera* cam, const XvtSnapCamera* pc, int regenerate,
					   const XvtSnapPreview* crt) {
	if (p && (p->world_generation != s->world_generation || p->mission_generation != s->mission_generation))
		p = NULL;
	EffectOrder order[XVT_SNAP_OBJECTS];
	for (unsigned i = 0; i < s->object_count; ++i) {
		float pos[3];
		AeronWorld_LocalI32(cam->world_pos, s->objects[i].world_pos, pos);
		order[i] = (EffectOrder) { i, pos[0] * cam->rows[6] + pos[1] * cam->rows[7] + pos[2] * cam->rows[8] };
	}
	qsort(order, s->object_count, sizeof order[0], Order);
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XvtSnapObject* o = &s->objects[order[i].index];
		if (!crt && o->id.slot == cam->focus.slot && !cam->external && !cam->replay_view)
			continue;
		if (o->slot_class == XVT_SLOT_LOCAL_TRANSIENT &&
			(!s->sky.debris_enabled || s->sky.proving_grounds ||
			 cam->hyperspace_phase == XVT_SNAP_HYPERSPACE_TRANSITION))
			continue;
		if (crt && o->id.slot != crt->object.slot && o->genus != CRAFT_GENUS_EXPLOSION)
			continue;
		if (o->genus == CRAFT_GENUS_SMALL_DEBRIS || o->genus == CRAFT_GENUS_EXPLOSION ||
			(o->slot_class == XVT_SLOT_STATIC && o->genus >= CRAFT_GENUS_MINE &&
			 o->genus <= CRAFT_GENUS_SMALL_DEBRIS))
			SubmitOne(scene, s, p, o, cam, pc, FrameCode(s, o), 0, regenerate);
		if (!o->has_craft || o->id.slot >= s->sky.craft_slot_end || o->object_type >= XVT_SNAP_TYPES)
			continue;
		if (o->genus == CRAFT_GENUS_OBSTACLE && o->id.slot != s->sky.checkpoint_slot &&
			o->id.slot != (unsigned)s->sky.checkpoint_slot + 1)
			continue;
		const XvtMeshAsset* mesh = XvtRemasterShip_Mesh(s, s->types[o->object_type].model_asset_id);
		if (!mesh || mesh->component_count >= XVT_SNAP_COMPONENTS)
			continue;
		unsigned flame = o->component_state[mesh->component_count];
		if (flame >= 25)
			continue;
		int ordinal = 0;
		for (unsigned m = 0; m < mesh->component_count; ++m)
			if (!o->component_state[m] && mesh->mesh->component_rot[m].mesh_type == XVT_SNAP_MESH_FUSELAGE)
				++ordinal;
		for (; ordinal > 0; --ordinal)
			SubmitOne(scene, s, p, o, cam, pc, (uint16_t)s->fuselage_sequence[flame], ordinal, regenerate);
	}
}

void XvtEffects_ProjectileMatrix(const XvtSnapObject* o, const int32_t camera[3], const int32_t origin[3],
								 float out[16]) {
	XvtSnapObject aligned = *o;
	float m[16], delta[3];
	XvtRenderMath_ObjectMatrix(o, origin, m);
	AeronWorld_DeltaI32(camera, o->world_pos, delta);
	float side = m[0] * delta[0] + m[4] * delta[1] + m[8] * delta[2],
		  up = m[2] * delta[0] + m[6] * delta[1] + m[10] * delta[2];
	aligned.roll = (uint16_t)(o->roll + (int)(atan2f(up, side) * 65536.0f / kTau) - 0x4000);
	aligned.orient_dirty = 1;
	XvtRenderMath_ObjectMatrix(&aligned, origin, out);
}

void XvtEffects_MapObject(AeronScene3D* scene, const XvtRenderSnapshot* s, const XvtSnapObject* o) {
	SubmitOne(scene, s, NULL, o, &s->camera, NULL, FrameCode(s, o), 0, 0);
	for (int ordinal = 1; ordinal <= XVT_SNAP_COMPONENTS; ++ordinal) {
		uint16_t code = FlameCode(s, o, ordinal);
		if (code == XVT_SNAP_INVALID_TEXTURE_FRAME)
			break;
		SubmitOne(scene, s, NULL, o, &s->camera, NULL, code, ordinal, 0);
	}
}
