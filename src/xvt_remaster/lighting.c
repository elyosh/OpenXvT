#include "xvt_remaster/lighting.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/world.h"
#include "xvt/flight/object/laser.h"
#include "xvt_remaster/config.h"
#include "xvt_remaster/hyperspace.h"
#include <math.h>
#include <string.h>

/* FS b1 — mirrors cbuffer PbrLightFS (scene_pbr_lighting.hlsli). */
typedef struct PbrLightFS {
	/* Probe-grid key-light shadowing; XvT has no probe sets. */
	float key_shadow_in_probes;
	float key_probe_shadow;
	float debug_isolate_term;
	float key_shadow_facing;
	float _reserved_xvt;
	float ssao_intensity;
	float ssao_power;
	float _reserved_ssao_rt[2];
	float ssao_direct;
	float spec_geom_adapt;
	float _pad_tuning;
	float camera_pos_world[3], _pad0;
	float directional_dir[3], _pad1; /* surface -> light */
	float sun_color[3], _pad2;
	float amb_pos_x[3], _pad3;
	float amb_neg_x[3], _pad4;
	float amb_pos_y[3], _pad5;
	float amb_neg_y[3], _pad6;
	float amb_pos_z[3], _pad7;
	float amb_neg_z[3], _pad8;
	/* Additional diffuse-only directionals (backdrop suns/planets;
	 * the classic sums plain Lambert per light). */
	float extra_dir[3][4];
	float extra_col[3][4];
	/* Least diffuse light from the environment (rgb, 1 / luminance); zero
	 * means no minimum. */
	float ambient_minimum[4];
	/* The environment moved to the scene lighting block (b2). */
	float _reserved_environment[4][4];
} PbrLightFS;

typedef char XvtPbrSizeCheck[sizeof(PbrLightFS) == 368 ? 1 : -1];

/* The radius of the sphere a point light shines from, as OpenXWA's. */
#define XVT_POINT_LIGHT_SOURCE_RADIUS_M 0.25f

void XvtLighting_AddPoint(AeronScene3D* scene, const float position[3], const float color[3],
						  float intensity) {
	const XvtPointLightSettings* p = &XvtRemasterConfig_Effective()->point_lights;
	if (!p->enabled || !(intensity > 0) || !isfinite(intensity) || p->scale <= 0)
		return;
	/* The classic law lights a surface by 0.5 * c / d, as a directional light
	 * of brightness 0.5 * c / d would. Such a light gives an illuminance of
	 * G times its brightness (lighting.classic_illuminance), and a point
	 * light I / d^2: equal at the match distance D (world units) when
	 * I = 0.5 * c * D * G, brighter closer and dimmer farther. */
	const float scale = 0.5f * p->match_distance_m * AERON_OPT_UNITS_PER_METER *
						XvtRemasterConfig_Effective()->lighting.classic_illuminance * p->scale;
	AeronSceneLight light = { .source_radius = XVT_POINT_LIGHT_SOURCE_RADIUS_M * AERON_OPT_UNITS_PER_METER };
	memcpy(light.pos, position, sizeof light.pos);
	float peak = 0;
	for (int c = 0; c < 3; ++c) {
		light.color[c] = color[c] * intensity * scale;
		peak = fmaxf(peak, light.color[c]);
	}
	/* The light ends where its illuminance falls under the cutoff; the
	 * flight's exposure is 1. */
	light.radius = sqrtf(peak / AERON_SCENE_LIGHT_CUTOFF) * p->range_scale;
	if (!(light.radius > 0) || !isfinite(light.radius))
		return;
	for (int c = 0; c < 3; ++c)
		if (!isfinite(light.pos[c]) || !isfinite(light.color[c]))
			return;
	AeronScene_AddLight(scene, &light);
}

static float ExplosionIntensity(const XvtSnapObject* o) {
	/* Original explosion curves with neutral legacy brightness, before receiver culling. */
	static const uint16_t large[] = { 16, 16, 192, 320, 480, 320, 320, 320, 320, 192, 96, 48 };
	static const uint16_t small[] = { 16, 16, 48, 96, 64, 32 };
	unsigned frame = o->type_specific[0];
	int intensity;
	if (o->object_type >= XVT_SNAP_TYPE_EXPLOSION_FIRST && o->object_type <= XVT_SNAP_TYPE_EXPLOSION_LAST) {
		intensity = frame < sizeof large / sizeof *large ? large[frame] : 16;
		if (o->has_mobile && o->light_scale >= 4)
			intensity *= (o->light_scale + 4) / 4;
	} else if (o->object_type == XVT_SNAP_TYPE_SMALL_EXPLOSION ||
			   o->object_type == XVT_SNAP_TYPE_COMPONENT_FOLLOWUP)
		intensity = frame < sizeof small / sizeof *small ? small[frame] : 16;
	else
		return 0;
	return (float)intensity * 8;
}

static float ProjectileLight(unsigned type, float color[3]) {
	/* XWA's projectile policy, matched to XvT's actual object-type IDs. */
	static const float rebel[3] = { 1, .2f, 0 };
	static const float imperial[3] = { 0, 1, .2f };
	static const float ion[3] = { .2f, .2f, 1 };
	static const float torpedo[3] = { .4f, .2f, 1 };
	static const float magnetic_pulse[3] = { 1, .2f, 1 };
	static const float warhead[3] = { 1, .4f, .2f };
	static const float countermeasure[3] = { 1, .4f, 1 };
	const float* rgb;
	float intensity = 200;
	switch (type) {
		case PROJECTILE_OBJECT_TYPE_REBEL_LASER:
		case PROJECTILE_OBJECT_TYPE_REBEL_TURBO_LASER:
		case PROJECTILE_OBJECT_TYPE_REBEL_TURBO_LASER_2:
		case WARHEAD_OBJECT_TYPE_LASER_3:
			rgb = rebel;
			break;
		case PROJECTILE_OBJECT_TYPE_IMPERIAL_LASER:
		case PROJECTILE_OBJECT_TYPE_IMPERIAL_TURBO_LASER:
		case PROJECTILE_OBJECT_TYPE_IMPERIAL_TURBO_LASER_2:
			rgb = imperial;
			break;
		case PROJECTILE_OBJECT_TYPE_ION_LASER:
		case PROJECTILE_OBJECT_TYPE_ION_TURBO_LASER:
		case PROJECTILE_OBJECT_TYPE_ION_TURBO_LASER_2:
			rgb = ion;
			break;
		case WARHEAD_OBJECT_TYPE_PROTON_TORPEDO:
		case WARHEAD_OBJECT_TYPE_ADVANCED_PROTON_TORPEDO:
			intensity = 250;
			rgb = torpedo;
			break;
		case WARHEAD_OBJECT_TYPE_MAGNETIC_PULSE:
			intensity = 250;
			rgb = magnetic_pulse;
			break;
		case WARHEAD_OBJECT_TYPE_ION_PULSE:
			intensity = 250;
			rgb = ion;
			break;
		case WARHEAD_OBJECT_TYPE_CONCUSSION_MISSILE:
		case WARHEAD_OBJECT_TYPE_ADVANCED_CONCUSSION_MISSILE:
		case WARHEAD_OBJECT_TYPE_SPACE_BOMB:
		case WARHEAD_OBJECT_TYPE_HEAVY_ROCKET:
			intensity = 250;
			rgb = warhead;
			break;
		case COUNTERMEASURE_PROJECTILE_OBJECT_TYPE:
			rgb = countermeasure;
			break;
		default:
			return 0;
	}
	for (int c = 0; c < 3; ++c)
		color[c] = rgb[c] <= .04045f ? rgb[c] / 12.92f : powf((rgb[c] + .055f) / 1.055f, 2.4f);
	return intensity;
}

static void Shadows(AeronScene3D* scene, const XvtRenderSnapshot* s) {
	const AeronSceneShadowSettings* p = &XvtRemasterConfig_Effective()->scene.shadows;
	AeronSceneDirectionalShadowDesc d = { .enabled = p->enabled && s->lighting.directional_enabled,
										  .atlas_size = p->atlas_size,
										  .cascade_count = p->cascade_count,
										  .fit_mode = p->fit_mode,
										  .max_distance = p->max_distance,
										  .split_lambda = p->split_lambda,
										  .explicit_splits = p->explicit_splits,
										  .light_angular_radius_degrees = p->light_angular_radius_degrees,
										  .max_filter_radius = p->max_filter_radius,
										  .pcss_min_filter_radius = p->pcss_min_filter_radius,
										  .normal_bias_texels = p->normal_bias_texels,
										  .depth_bias_texels = p->depth_bias_texels,
										  .transition_fraction = p->transition_fraction,
										  .distance_fade_fraction = p->distance_fade_fraction,
										  .debug_cascades = p->debug_cascades };
	memcpy(d.split_positions, p->split_positions, sizeof d.split_positions);
	float length = 0;
	for (int c = 0; c < 3; ++c) {
		d.world_origin[c] = s->camera.world_pos[c];
		d.light_dir[c] = (float)s->lighting.direction_q15[c] / 32768;
		length += d.light_dir[c] * d.light_dir[c];
	}
	XvtHyperLighting hyper;
	if (XvtHyperspace_Lighting(scene, &hyper)) {
		memcpy(d.light_dir, hyper.direction, sizeof d.light_dir);
		d.enabled = p->enabled;
		length = 1;
	}
	if (length <= 1e-10f)
		d.enabled = 0;
	else
		for (int c = 0; c < 3; ++c)
			d.light_dir[c] /= sqrtf(length);
	AeronScene_SetDirectionalShadow(scene, &d);
}

int XvtLighting_Begin(AeronScene3D* scene, const XvtRenderSnapshot* s) {
	const XvtPointLightSettings* p = &XvtRemasterConfig_Effective()->point_lights;
	Shadows(scene, s);
	if (!AeronScene_SetClusteredLights(
			scene, &(AeronSceneClusteredLightDesc) { .depth_slices = (uint32_t)p->cluster_depth_slices,
													 .debug_view = p->cluster_debug }))
		return 0;
	if (!p->enabled)
		return 1;
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XvtSnapObject* o = &s->objects[i];
		if (o->slot_class == XVT_SLOT_STATIC)
			continue;
		float intensity = 0, color[3] = { 1, 1, 1 }, position[3];
		if (o->genus == CRAFT_GENUS_EXPLOSION) {
			intensity = ExplosionIntensity(o);
			/* OpenTIE's sRGB explosion color (0.9, 0.5, 0.2), converted to linear. */
			color[0] = .7874123f;
			color[1] = .2140411f;
			color[2] = .03310477f;
		} else if (o->genus == CRAFT_GENUS_PLAYER_PROJECTILE || o->genus == CRAFT_GENUS_OTHER_PROJECTILE)
			intensity = ProjectileLight(o->object_type, color);
		AeronWorld_LocalI32(s->camera.world_pos, o->world_pos, position);
		XvtLighting_AddPoint(scene, position, color, intensity);
	}
	return 1;
}

void XvtRemasterShip_SetEnvironment(AeronScene3D* scene, const XvtSnapLighting* light,
									const XvtSnapCamera* eye_camera, const float position[3]) {
	const XvtLightingSettings* settings = &XvtRemasterConfig_Effective()->lighting;
	PbrLightFS env = { 0 };
	const XvtRenderSettings* config = XvtRemasterConfig_Effective();
	env.ssao_intensity = config->scene.ssao.ssao_quality ? config->scene.ssao.ssao_intensity : 0;
	env.ssao_power = config->scene.ssao.ssao_power;
	env.ssao_direct = config->scene.ssao.ssao_direct;
	AeronScene_SetPbrDebugViews(scene, !settings->spec_geom_adapt);
	AeronScene_SetPbrEnvironment(scene, NULL);
	env.spec_geom_adapt = settings->spec_geom_adapt;
	if (position)
		memcpy(env.camera_pos_world, position, sizeof env.camera_pos_world);
	float direction[3];
	/* XvT dots this direction with the normal and traces toward it for occlusion. */
	for (int i = 0; i < 3; ++i)
		direction[i] = (float)light->direction_q15[i] / 32768.0f;
	for (int i = 0; i < 3; ++i) {
		env.directional_dir[i] = eye_camera ? (eye_camera->rows[i * 3] * direction[0] +
											   eye_camera->rows[i * 3 + 1] * direction[1] +
											   eye_camera->rows[i * 3 + 2] * direction[2])
											: direction[i];
		/* The game's sun has no brightness of its own: brightness 1. */
		env.sun_color[i] = light->directional_enabled ? settings->classic_illuminance : 0;
		env.amb_pos_x[i] = env.amb_neg_x[i] = env.amb_pos_y[i] = env.amb_neg_y[i] = env.amb_pos_z[i] =
			env.amb_neg_z[i] = settings->ambient[i];
	}
	float length = sqrtf(env.directional_dir[0] * env.directional_dir[0] +
						 env.directional_dir[1] * env.directional_dir[1] +
						 env.directional_dir[2] * env.directional_dir[2]);
	if (length > 1e-5f)
		for (int i = 0; i < 3; ++i)
			env.directional_dir[i] /= length;
	XvtHyperLighting hyper;
	if (!eye_camera && XvtHyperspace_Lighting(scene, &hyper)) {
		memcpy(env.directional_dir, hyper.direction, sizeof hyper.direction);
		for (int i = 0; i < 3; ++i)
			env.sun_color[i] = hyper.color[i] * settings->classic_illuminance;
		/* Rows are the cube's right, up and forward axes in world space. */
		AeronScene_SetPbrEnvironment(
			scene, &(AeronScenePbrEnvironment) { .diffuse = hyper.texture,
												 .sampler = hyper.sampler,
												 .diffuse_strength = config->hyperspace.mesh_ambient_strength,
												 .world_to_local = { 1, 0, 0, 0, 0, 1, 0, 1, 0 } });
	}
	AeronScene_SetFrameUniformData(scene, AERON_SHADER_STAGE_FRAGMENT, 1, &env, sizeof env);
}
