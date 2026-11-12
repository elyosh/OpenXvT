#include "xvt_remaster/ship.h"
#include "xvt_remaster/config.h"
#include <math.h>
#include <string.h>

int XvtRemasterShip_Select(const XvtRenderSnapshot* s, const XvtSnapObject* o, XvtShipSelection* out) {
	if (!s || !o || !out || o->object_type >= XVT_SNAP_TYPES)
		return 0;
	out->component = UINT16_MAX;
	unsigned type = o->object_type;
	if (type == XVT_SNAP_TYPE_COMPONENT && o->has_mobile) {
		type = o->source_type;
		out->component = o->type_specific[0] >> 1;
		if (type >= XVT_SNAP_TYPES)
			return 0;
	} else if ((o->genus >= CRAFT_GENUS_MINE && o->genus <= CRAFT_GENUS_SMALL_DEBRIS) ||
			   o->genus == CRAFT_GENUS_EXPLOSION) {
		const XvtSnapType* t = &s->types[type];
		if (!t->sequence_count) {
			if (o->slot_class != XVT_SLOT_STATIC || o->type_specific[0])
				return 0;
		} else {
			if (o->type_specific[0] >= t->sequence_count)
				return 0;
			uint16_t frame = (uint16_t)t->sequence[o->type_specific[0]];
			if (frame >= XVT_SNAP_TEXTURE_FRAME_BIT)
				return 0;
			/* The mobile debris path selects a component; statics draw the whole model. */
			if (o->slot_class != XVT_SLOT_STATIC)
				out->component = frame;
		}
	} else if (o->genus > CRAFT_GENUS_OTHER_PROJECTILE && o->genus != CRAFT_GENUS_OBSTACLE)
		return 0;
	out->asset_id = s->types[type].model_asset_id;
	return out->asset_id != 0;
}

static void ship_mat3x4_identity(float out[3][4]) {
	memset(out, 0, 12 * sizeof(float));
	out[0][0] = out[1][1] = out[2][2] = 1.0f;
}

static void ship_mat3x4_rotation_about_pivot(float out[3][4], const float axis[3], const float pivot[3],
											 float angle) {
	float ax = axis[0], ay = axis[1], az = axis[2];
	const float len = sqrtf(ax * ax + ay * ay + az * az);
	if (len < 1e-4f) {
		ship_mat3x4_identity(out);
		return;
	}
	ax /= len;
	ay /= len;
	az /= len;
	const float c = cosf(angle), s = sinf(angle), omc = 1.0f - c;
	out[0][0] = c + ax * ax * omc;
	out[0][1] = ax * ay * omc - az * s;
	out[0][2] = ax * az * omc + ay * s;
	out[1][0] = ay * ax * omc + az * s;
	out[1][1] = c + ay * ay * omc;
	out[1][2] = ay * az * omc - ax * s;
	out[2][0] = az * ax * omc - ay * s;
	out[2][1] = az * ay * omc + ax * s;
	out[2][2] = c + az * az * omc;
	for (int r = 0; r < 3; r++) {
		out[r][3] = pivot[r] - (out[r][0] * pivot[0] + out[r][1] * pivot[1] + out[r][2] * pivot[2]);
	}
}

/* Compose two affine transforms using the shader's row-major convention:
 * out(v) = lhs(rhs(v)). */
static void ship_mat3x4_mul(float out[3][4], const float lhs[3][4], const float rhs[3][4]) {
	float result[3][4];
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			result[r][c] = lhs[r][0] * rhs[0][c] + lhs[r][1] * rhs[1][c] + lhs[r][2] * rhs[2][c];
		}
		result[r][3] = lhs[r][0] * rhs[0][3] + lhs[r][1] * rhs[1][3] + lhs[r][2] * rhs[2][3] + lhs[r][3];
	}
	memcpy(out, result, sizeof result);
}

void XvtRemasterShip_BuildMeshTable(const XvtMeshAsset* asset, const XvtSnapObject* object,
									uint16_t component, const float* visual_angles,
									AeronSceneMeshTable* out) {
	memset(out, 0, sizeof *out);
	float bridge[3][4];
	ship_mat3x4_identity(bridge);
	const float byte_angle = -6.2831853071795864769f / 256.0f;
	int bwing = object && object->has_craft && object->object_type == XVT_SNAP_TYPE_B_WING &&
				asset->bridge_component >= 0 && asset->bridge_component < XVT_SNAP_COMPONENTS;
	if (bwing)
		ship_mat3x4_rotation_about_pivot(bridge, (const float[3]) { 0, -1, 0 }, (const float[3]) { 0, 0, 0 },
										 (visual_angles ? visual_angles[asset->bridge_component]
														: object->mesh_rotation[asset->bridge_component]) *
											 byte_angle);
	for (unsigned i = 0; i < AERON_MAX_MESH_COMPONENTS; ++i) {
		float local[3][4];
		ship_mat3x4_identity(local);
		int visible = i < asset->component_count && (component == UINT16_MAX || component == i);
		if (object && object->has_craft && component == UINT16_MAX) {
			if (object->component_state[i])
				visible = 0;
			const AeronMeshRot* r = &asset->mesh->component_rot[i];
			/* The recovered transpose chain nets -rotation on model vertices. */
			if (r->has_rotation && (visual_angles ? visual_angles[i] : object->mesh_rotation[i]))
				ship_mat3x4_rotation_about_pivot(
					local, r->axis, r->pivot,
					(visual_angles ? visual_angles[i] : object->mesh_rotation[i]) * byte_angle);
		}
		ship_mat3x4_mul(out->rows[i], bridge, local);
		out->visibility_packed[i >> 2][i & 3] = visible ? 1 : 0;
		out->emissive_packed[i >> 2][i & 3] = 1;
	}
}

float XvtRemasterShip_Radius(const XvtMeshAsset* asset, const AeronSceneMeshTable* table, const float m[16]) {
	float center[3], r2 = 0;
	for (int i = 0; i < 3; ++i) {
		center[i] = (asset->mesh->bound_min[i] + asset->mesh->bound_max[i]) * 0.5f;
		float half = (asset->mesh->bound_max[i] - asset->mesh->bound_min[i]) * 0.5f;
		r2 += half * half;
	}
	float radius = 0, base_radius = sqrtf(r2);
	for (unsigned i = 0; i < asset->component_count && i < AERON_MAX_MESH_COMPONENTS; ++i) {
		if (!table->visibility_packed[i >> 2][i & 3])
			continue;
		float distance = 0;
		for (int row = 0; row < 3; ++row) {
			const float* t = table->rows[i][row];
			float value = t[0] * center[0] + t[1] * center[1] + t[2] * center[2] + t[3];
			distance += value * value;
		}
		radius = fmaxf(radius, sqrtf(distance) + base_radius);
	}
	float scale = 0;
	for (int i = 0; i < 3; ++i)
		scale = fmaxf(scale, sqrtf(m[i] * m[i] + m[4 + i] * m[4 + i] + m[8 + i] * m[8 + i]));
	return radius * scale;
}

int XvtRemasterShip_Visible(const XvtRenderView* view, const float m[16], float radius) {
	const float* p = view->view_proj;
	/* Left/right/top/bottom, near and infinite reversed-Z far planes. */
	for (int plane = 0; plane < 6; ++plane) {
		float v[4];
		for (int i = 0; i < 4; ++i) {
			if (plane < 4)
				v[i] = p[12 + i] + (plane & 1 ? -1.0f : 1.0f) * p[(plane / 2) * 4 + i];
			else
				v[i] = plane == 4 ? p[12 + i] - p[8 + i] : p[8 + i];
		}
		float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		if (length && v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] < -radius * length)
			return 0;
	}
	return 1;
}
