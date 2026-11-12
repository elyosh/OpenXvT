#include "xvt_remaster/preview.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/present.h"
#include "xvt_remaster/component_animation.h"
#include "xvt_remaster/config.h"
#include "xvt_remaster/effects.h"
#include "xvt_remaster/flight_pipeline.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* Frontend previews share a fixed scene; the independent CRT follows its displayed size. */
enum { PREVIEW_SLOTS = XVT_SNAP_PREVIEWS + 1, PREVIEW_WIDTH = 2048, PREVIEW_HEIGHT = 1536 };

static AeronScene3D *g_scene, *g_crtScene;

static struct {
	AeronScene3D* scene;
	int width, height, samples;
} g_crtViews[3];

static AeronRenderTarget* g_targets[PREVIEW_SLOTS];
static XvtPreviewOutput g_outputs[PREVIEW_SLOTS];
static AeronScenePresentChain* g_chain;
static AeronSampler* g_sampler;
static int g_sceneWidth, g_sceneHeight, g_samples;
static AeronSceneMeshTable g_table;

typedef struct CrtDependencies {
	uint64_t world, mission, config, models, textures, component_pose;
	XvtSnapPreview preview;
	XvtSnapLighting effect_lighting;
	XvtSnapType types[XVT_SNAP_TYPES];
	int16_t fuselage[25];
	uint16_t craft_slot_end, checkpoint;
	uint8_t debris, proving_grounds;
	int width, height;
	unsigned object_count;
	XvtSnapObject objects[XVT_SNAP_OBJECTS];
} CrtDependencies;

static CrtDependencies g_crtDependencies, g_crtCandidate;
static int g_crtValid;

static int EnsureScene(AeronScene3D** scene, int* old_width, int* old_height, int* old_samples, int width,
					   int height) {
	int samples = XvtRemasterConfig_Effective()->msaa_samples;
	if (*scene && width == *old_width && height == *old_height && samples == *old_samples)
		return 1;
	AeronScene3D* next =
		AeronScene_Create(&(AeronScene3DDesc) { .rt_width = width,
												.rt_height = height,
												.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
												.with_normal_rt = 1,
												.sample_count = (AeronSampleCount)samples,
												.view_space_to_meters = AERON_OPT_METERS_PER_UNIT });
	if (!next)
		return 0;
	AeronScene_SetClearColor(next, (const float[4]) { 0, 0, 0, 0 });
	AeronScene_Destroy(*scene);
	*scene = next;
	*old_width = width;
	*old_height = height;
	*old_samples = samples;
	return 1;
}

static int Ensure(unsigned slot, int width, int height) {
	if (slot == XVT_SNAP_PREVIEWS) {
		for (unsigned index = 0; index < 3; ++index)
			if (g_crtViews[index].scene && g_crtViews[index].width == width &&
				g_crtViews[index].height == height) {
				g_crtScene = g_crtViews[index].scene;
				return 1;
			}
		return 0;
	}
	if (!EnsureScene(&g_scene, &g_sceneWidth, &g_sceneHeight, &g_samples, width, height))
		return 0;
	if (!g_chain)
		g_chain = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
	if (!g_sampler)
		g_sampler = Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
															  .mag_filter = AERON_FILTER_LINEAR,
															  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
															  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	if (!g_targets[slot] || width != g_outputs[slot].width || height != g_outputs[slot].height) {
		AeronRenderTarget* target =
			Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
																.height = height,
																.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
																.debug_name = "xvt.preview.present" });
		if (!target)
			return 0;
		Aeron_DestroyRenderTarget(g_targets[slot]);
		g_targets[slot] = target;
		g_outputs[slot].width = width;
		g_outputs[slot].height = height;
	}
	return g_chain && g_sampler;
}

static const XvtSnapObject* Target(const XvtRenderSnapshot* s, XvtSnapObjectId id) {
	for (unsigned i = 0; i < s->object_count; ++i)
		if (s->objects[i].id.slot == id.slot && s->objects[i].id.signature == id.signature)
			return &s->objects[i];
	return NULL;
}

static int IncludesCrtProjectile(const XvtSnapObject* object, const XvtSnapPreview* preview) {
	if (object->genus != CRAFT_GENUS_PLAYER_PROJECTILE && object->genus != CRAFT_GENUS_OTHER_PROJECTILE)
		return 0;
	if (object->id.slot == preview->camera.focus.slot && !preview->camera.external &&
		!preview->camera.replay_view)
		return 0;
	return object->source_slot == preview->object.slot ||
		   (!preview->camera.map_mode && object->source_slot == preview->camera.player.slot);
}

void XvtRemasterPreview_InvalidateCrt(void) {
	g_crtValid = 0;
	g_outputs[XVT_SNAP_PREVIEWS].texture = NULL;
}

int XvtRemasterPreview_PrepareCrtResources(const XvtCockpitResources* resources, int width, int height) {
	if (!resources->view.screen_width || !resources->view.screen_height)
		return 0;
	float scale =
		fminf((float)width / resources->view.screen_width, (float)height / resources->view.screen_height);
	unsigned count = 0;
	for (unsigned index = 0; index < 3; ++index) {
		const XvtSnapHudElement* element =
			&resources->definition.layout.elements[index * HUD_INSTRUMENTS_PER_SET + 2];
		if (!element->selector || !element->color_index)
			continue;
		int w = (int)ceilf(element->selector * scale), h = (int)ceilf(element->color_index * scale);
		unsigned existing = 0;
		for (; existing < count; ++existing)
			if (g_crtViews[existing].width == w && g_crtViews[existing].height == h)
				break;
		if (existing < count)
			continue;
		if (!g_crtViews[count].scene || g_crtViews[count].width != w || g_crtViews[count].height != h ||
			g_crtViews[count].samples != XvtRemasterConfig_Effective()->msaa_samples)
			XvtRemasterPreview_InvalidateCrt();
		AeronScene3D* previous = g_crtViews[count].scene;
		if (!EnsureScene(&g_crtViews[count].scene, &g_crtViews[count].width, &g_crtViews[count].height,
						 &g_crtViews[count].samples, w, h))
			return 0;
		if (previous != g_crtViews[count].scene &&
			!XvtFlightPipeline_PrepareSceneResources(g_crtViews[count].scene, 0))
			return 0;
		++count;
	}
	for (unsigned index = count; index < 3; ++index) {
		if (g_crtViews[index].scene)
			XvtRemasterPreview_InvalidateCrt();
		AeronScene_Destroy(g_crtViews[index].scene);
		memset(&g_crtViews[index], 0, sizeof g_crtViews[index]);
	}
	return 1;
}

int XvtRemasterPreview_CrtNeedsRender(const XvtRenderSnapshot* s, int width, int height) {
	const XvtSnapPreview* preview = &s->cockpit.crt;
	if (!preview->valid || !Target(s, preview->object)) {
		XvtRemasterPreview_InvalidateCrt();
		return 0;
	}
	CrtDependencies* key = &g_crtCandidate;
	memset(key, 0, offsetof(CrtDependencies, objects));
	key->component_pose = s->flight_unlocked ? XvtComponentAnimation_ObjectRevision(preview->object.slot) : 0;
	key->world = s->world_generation;
	key->mission = s->mission_generation;
	key->config = XvtRemasterConfig_Generation();
	key->models = s->opt_asset_generation;
	key->textures = s->texture_asset_generation;
	key->preview = *preview;
	memset(&key->preview.draw, 0, sizeof key->preview.draw);
	key->preview.component_marker_valid = 0;
	memset(key->preview.component_marker_world, 0, sizeof key->preview.component_marker_world);
	key->preview.component = 0;
	key->preview.destination.x = key->preview.destination.y = 0;
	key->effect_lighting = s->lighting;
	memcpy(key->types, s->types, sizeof key->types);
	memcpy(key->fuselage, s->fuselage_sequence, sizeof key->fuselage);
	key->craft_slot_end = s->sky.craft_slot_end;
	key->checkpoint = s->sky.checkpoint_slot;
	key->debris = s->sky.debris_enabled;
	key->proving_grounds = s->sky.proving_grounds;
	key->width = width;
	key->height = height;
	/* Animation is already resolved into type/component frame state. No host tick
	 * or wall clock participates in these transparent CRT draws. */
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XvtSnapObject* object = &s->objects[i];
		if ((object->id.slot == preview->object.slot && object->id.signature == preview->object.signature) ||
			IncludesCrtProjectile(object, preview) || object->genus == CRAFT_GENUS_EXPLOSION)
			key->objects[key->object_count++] = *object;
	}
	size_t bytes = offsetof(CrtDependencies, objects) + key->object_count * sizeof *key->objects;
	return !g_crtValid || memcmp(&g_crtDependencies, key, bytes) != 0;
}

static void CrtProjectiles(AeronScene3D* scene, const XvtRenderSnapshot* s, const XvtSnapPreview* p) {
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XvtSnapObject* o = &s->objects[i];
		if (!IncludesCrtProjectile(o, p))
			continue;
		XvtShipSelection selection;
		if (!XvtRemasterShip_Select(s, o, &selection))
			continue;
		const XvtMeshAsset* mesh = XvtRemasterShip_Mesh(s, selection.asset_id);
		if (!mesh)
			continue;
		AeronSceneMeshInstance instance = {
			.mesh = mesh->mesh,
			.variant = o->node_switch,
			.zero_velocity = 1,
			.base_color_emissive_strength =
				XvtRemasterConfig_Effective()->models.opt_projectile_emissive_strength,
			.cull_mode = AERON_CULL_NONE
		};
		XvtEffects_ProjectileMatrix(o, p->camera.world_pos, p->camera.world_pos, instance.transform);
		memcpy(instance.prev_transform, instance.transform, sizeof instance.transform);
		AeronScene_AddMeshInstance(scene, &instance);
	}
}

static int RenderOne(AeronCommandBuffer* cmd, const XvtRenderSnapshot* s, const XvtSnapPreview* p,
					 unsigned slot, int tw, int th, int crt) {
	if (!p->valid || p->destination.width <= 0 || p->destination.height <= 0)
		return 1;
	const XvtMeshAsset* mesh = XvtRemasterShip_Mesh(s, p->opt_asset_id);
	if (!mesh && !crt)
		return 1;
	XvtLayoutTransform layout;
	if (!XvtRenderMath_Layout(p->camera.screen_width, p->camera.screen_height, (float)tw, (float)th, &layout))
		return 0;
	float scale = fminf((float)tw / p->camera.screen_width, (float)th / p->camera.screen_height);
	int width = crt ? (int)ceilf(p->destination.width * scale) : PREVIEW_WIDTH;
	int height = crt ? (int)ceilf(p->destination.height * scale) : PREVIEW_HEIGHT;
	if (!Ensure(slot, width, height))
		return 0;
	AeronScene3D* scene = crt ? g_crtScene : g_scene;
	AeronSceneCamera camera = { 0 };
	AeronSceneMeshInstance instance = { .mesh = mesh ? mesh->mesh : NULL,
										.variant = p->node_switch,
										.zero_velocity = 1,
										.cull_mode = AERON_CULL_BACK,
										.mesh_table = &g_table };
	const XvtSnapObject* object = crt ? Target(s, p->object) : NULL;
	if (crt) {
		if (!object)
			return 1;
		XvtRenderView view;
		if (!XvtRenderMath_BuildView(&p->camera, p->camera.world_pos, p->destination.width,
									 p->destination.height, &view))
			return 0;
		camera = view.camera;
		camera.viewport = (AeronRectI) { 0, 0, width, height };
		XvtRenderMath_ObjectMatrix(object, p->camera.world_pos, instance.transform);
		if (object->genus == CRAFT_GENUS_PLAYER_PROJECTILE || object->genus == CRAFT_GENUS_OTHER_PROJECTILE)
			XvtEffects_ProjectileMatrix(object, p->camera.world_pos, p->camera.world_pos, instance.transform);
	} else {
		camera.ori[0] = 1;
		float focal = ldexpf(1.0f, p->camera.perspective_shift & 31);
		float aspect = !p->camera.aspect_y_q16 || p->camera.aspect_y_q16 == UINT16_MAX
						   ? 1
						   : (float)p->camera.aspect_y_q16 / 65536.0f;
		camera.h_half_rad = atanf((float)p->destination.width / (2 * focal));
		camera.v_half_rad = atanf((float)p->destination.height / (2 * focal * aspect));
		camera.near_z = 1;
		camera.proj_x_offset =
			((float)p->camera.center_x - p->destination.width * 0.5f) / (p->destination.width * 0.5f);
		camera.proj_y_offset =
			(p->destination.height * 0.5f - p->camera.center_y - p->camera.projection_offset_y) /
			(p->destination.height * 0.5f);
		camera.viewport = (AeronRectI) { 0, 0, width, height };
		float scale = p->model_scale * AERON_OPT_UNITS_PER_METER;
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col)
				instance.transform[row * 4 + col] = scale * p->view_orient[col * 3 + row];
			instance.transform[row * 4 + 3] = p->view_pos[row];
		}
		instance.transform[15] = 1;
	}
	if (mesh) {
		float visual[XVT_SNAP_COMPONENTS];
		XvtRemasterShip_BuildMeshTable(mesh, object, UINT16_MAX,
									   crt ? XvtComponentAnimation_Angles(s, object, mesh, visual) : NULL,
									   &g_table);
	}
	memcpy(instance.prev_transform, instance.transform, sizeof instance.transform);
	if (!AeronScene_Begin(scene, &camera))
		return 0;
	if (!AeronScene_SetMeshSampler(scene, XvtRemasterConfig_MeshSampler()))
		return 0;
	XvtFlightPipeline_Post(scene, 0, 0);
	XvtRemasterShip_SetEnvironment(scene, &p->lighting, crt ? NULL : &p->camera, camera.pos);
	if (mesh)
		AeronScene_AddMeshInstance(scene, &instance);
	if (crt) {
		CrtProjectiles(scene, s, p);
		XvtEffects_Submit(scene, s, NULL, &p->camera, NULL, 0, p);
	}
	if (!AeronScene_Render(scene, cmd))
		return 0;
	if (!crt) {
		AeronRenderPass* pass =
			Aeron_BeginRenderPass(&(AeronRenderPassDesc) { .color_target = g_targets[slot],
														   .clear_color = 1,
														   .clear_color_rgba = { 0, 0, 0, 0 },
														   .command_buffer = cmd,
														   .debug_label = "XvT model preview tonemap" });
		if (!pass)
			return 0;
		AeronScenePresentChain_Draw(g_chain, pass, Aeron_RenderTargetGetTexture(AeronScene_SceneRt(scene)),
									g_sampler, NULL, 0, 1, NULL, width, height, 1,
									(const float[4]) { 1, 1, 1, 1 }, 1);
		Aeron_EndRenderPass(pass);
	}
	XvtPreviewOutput* out = &g_outputs[slot];
	out->tick_index = s->tick_index;
	out->texture = crt ? AeronScene_ColorTexture(scene) : Aeron_RenderTargetGetTexture(g_targets[slot]);
	out->width = width;
	out->height = height;
	out->draw = p->draw;
	out->mask_index = p->mask_index;
	out->destination = p->destination;
	return 1;
}

int XvtRemasterPreview_Render(AeronCommandBuffer* cmd, const XvtRenderSnapshot* s, int width, int height) {
	XvtRemasterPreview_BeginFrame();
	for (unsigned i = 0; i < s->preview_count; ++i)
		if (!RenderOne(cmd, s, &s->previews[i], i, width, height, 0))
			return 0;
	return 1;
}

int XvtRemasterPreview_RenderCrt(AeronCommandBuffer* cmd, const XvtRenderSnapshot* s, int width, int height) {
	if (!XvtRemasterPreview_CrtNeedsRender(s, width, height))
		return 1;
	if (!RenderOne(cmd, s, &s->cockpit.crt, XVT_SNAP_PREVIEWS, width, height, 1)) {
		XvtRemasterPreview_InvalidateCrt();
		return 0;
	}
	size_t bytes =
		offsetof(CrtDependencies, objects) + g_crtCandidate.object_count * sizeof *g_crtCandidate.objects;
	memcpy(&g_crtDependencies, &g_crtCandidate, bytes);
	g_crtValid = 1;
	return 1;
}

void XvtRemasterPreview_BeginFrame(void) {
	for (unsigned i = 0; i < XVT_SNAP_PREVIEWS; ++i)
		g_outputs[i].texture = NULL;
}

AeronTexture* XvtRemasterPreview_CrtLinear(void) { return g_outputs[XVT_SNAP_PREVIEWS].texture; }

const XvtPreviewOutput* XvtRemasterPreview_Output(unsigned slot) {
	return slot < PREVIEW_SLOTS && g_outputs[slot].texture ? &g_outputs[slot] : NULL;
}

void XvtRemasterPreview_ReleaseFrontend(void) {
	AeronScene_Destroy(g_scene);
	g_scene = NULL;
	AeronScenePresentChain_Destroy(g_chain);
	g_chain = NULL;
	Aeron_DestroySampler(g_sampler);
	g_sampler = NULL;
	for (unsigned i = 0; i < XVT_SNAP_PREVIEWS; ++i) {
		Aeron_DestroyRenderTarget(g_targets[i]);
		g_targets[i] = NULL;
		memset(&g_outputs[i], 0, sizeof g_outputs[i]);
	}
	g_sceneWidth = g_sceneHeight = g_samples = 0;
}

void XvtRemasterPreview_Shutdown(void) {
	XvtRemasterPreview_ReleaseFrontend();
	XvtRemasterPreview_InvalidateCrt();
	for (unsigned index = 0; index < 3; ++index)
		AeronScene_Destroy(g_crtViews[index].scene);
	memset(g_crtViews, 0, sizeof g_crtViews);
	g_crtScene = NULL;
	memset(&g_outputs[XVT_SNAP_PREVIEWS], 0, sizeof g_outputs[XVT_SNAP_PREVIEWS]);
}
