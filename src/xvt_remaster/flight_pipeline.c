#include "xvt_remaster/flight_pipeline.h"
#include "aeron/aeron.h"
#include "aeron/scene/bloom.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/present.h"
#include "xvt_remaster/config.h"
#include "xvt_remaster/hud_renderer.h"

static AeronSceneBloom* g_bloom;
static AeronScenePresentChain* g_present;
static AeronSampler* g_sampler;
static AeronRenderTarget* g_target;
static int g_width, g_height;
static uint64_t g_lastHostUs;
static AeronScenePresentChain* g_direct;
static AeronTextureFormat g_directFormat;
static AeronTexture *g_color, *g_bloomTexture;
static float g_intensity;
static int g_directWanted, g_retained;
static AeronDrawList2D* g_bars;
static int g_barsVisible;

void XvtFlightPipeline_ForgetSources(void) { g_color = g_bloomTexture = NULL; }

int XvtFlightPipeline_SetDirect(int enabled, int width, int height) {
	g_directWanted = 0;
	if (!enabled || !Aeron_CanRenderDirectToSwapchain(width, height))
		return 0;
	AeronTextureFormat format = Aeron_SwapchainFormat();
	if (!g_direct || format != g_directFormat) {
		AeronScenePresentChain_Destroy(g_direct);
		g_direct = AeronScenePresentChain_Create(format);
		g_directFormat = format;
		if (!g_direct) {
			Aeron_RequestFatalRendererError("direct flight tonemap creation");
			return 0;
		}
	}
	g_directWanted = 1;
	return 1;
}

static int Ensure(int width, int height) {
	if (!g_present)
		g_present = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
	if (!g_sampler)
		g_sampler = Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
															  .mag_filter = AERON_FILTER_LINEAR,
															  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
															  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	if (!g_present || !g_sampler)
		return 0;
	if (g_target && g_width == width && g_height == height)
		return 1;
	AeronSceneBloom* bloom = AeronSceneBloom_Create(width, height);
	AeronRenderTarget* target =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
															.height = height,
															.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
															.debug_name = "xvt.flight.present" });
	if (!bloom || !target) {
		AeronSceneBloom_Destroy(bloom);
		Aeron_DestroyRenderTarget(target);
		return 0;
	}
	AeronSceneBloom_Destroy(g_bloom);
	Aeron_DestroyRenderTarget(g_target);
	g_color = g_bloomTexture = NULL;
	g_retained = 0;
	g_bloom = bloom;
	g_target = target;
	g_width = width;
	g_height = height;
	return 1;
}

void XvtFlightPipeline_Post(AeronScene3D* scene, float shutter, int motion) {
	const XvtRenderSettings* settings = XvtRemasterConfig_Effective();
	const AeronSceneSsaoSettings* a = &settings->scene.ssao;
	const XvtMotionBlurSettings* m = &settings->motion_blur;
	AeronScene_SetPost(scene, &(AeronScenePostDesc) { .ssao_quality = a->ssao_quality,
													  .ssao_intensity = a->ssao_intensity,
													  .ssao_power = a->ssao_power,
													  .ssao_radius_view = a->ssao_radius_view,
													  .ssao_bias_view = a->ssao_bias_view,
													  .ssao_direct = a->ssao_direct,
													  .ssao_debug_viz = a->ssao_debug_viz,
													  .ssao_min_screen_frac = a->ssao_min_screen_frac,
													  .ssao_max_screen_frac = a->ssao_max_screen_frac,
													  .ssao_sample_jitter = a->ssao_sample_jitter,
													  .mb_quality = motion ? m->quality : 0,
													  .mb_shutter = shutter,
													  .mb_camera_blur = m->camera_blur,
													  .mb_velocity_viz = motion && m->velocity_viz });
}

int XvtFlightPipeline_Begin(AeronScene3D* scene, const XvtRenderSnapshot* s, const XvtPreparedFlight* frame,
							int reset) {
	const XvtRenderSettings* settings = XvtRemasterConfig_Effective();
	uint64_t now = Aeron_NowUs();
	float delta_ms = g_lastHostUs && now > g_lastHostUs ? (float)(now - g_lastHostUs) / 1000 : 16.6667f;
	g_lastHostUs = now;
	AeronScene_SetTemporal(scene, &(AeronSceneTemporalDesc) { .mode = settings->temporal_mode,
															  .frame_time_delta_ms = delta_ms,
															  .sharpness = settings->temporal_sharpness,
															  .reset_history = reset });
	if (!AeronScene_Begin(scene, &frame->view.camera) ||
		!AeronScene_SetMeshSampler(scene, XvtRemasterConfig_MeshSampler()))
		return 0;
	/* XWA's 32 ms reference exposure: retained motion spans whole captured poses. */
	float shutter = !reset && frame->velocity_span_us
						? settings->motion_blur.shutter * 32000.0f / (float)frame->velocity_span_us
						: 0;
	if ((s->paused || !frame->regenerate_motion) && !settings->motion_blur.pause_keep_blur)
		shutter = 0;
	XvtFlightPipeline_Post(scene, shutter, 1);
	/* FSR sees a stationary current frame while Aeron retains the last blur vectors. */
	AeronScene_SetMotionContext(
		scene,
		reset ? NULL : (frame->regenerate_motion ? frame->previous_view.view_proj : frame->view.view_proj),
		NULL, reset || frame->regenerate_motion);
	return 1;
}

int XvtFlightPipeline_PrepareSceneResources(AeronScene3D* scene, int flight) {
	int width, height;
	AeronScene_RtDims(scene, &width, &height);
	const XvtRenderSettings* settings = XvtRemasterConfig_Effective();
	AeronScene_SetTemporal(
		scene, &(AeronSceneTemporalDesc) { .mode = flight ? settings->temporal_mode : AERON_TEMPORAL_OFF,
										   .frame_time_delta_ms = 16.6667f,
										   .sharpness = settings->temporal_sharpness,
										   .reset_history = 1 });
	XvtFlightPipeline_Post(scene, 0, flight);
	AeronSceneCamera camera = { .ori = { 1, 0, 0, 0 },
								.h_half_rad = .785398163f,
								.v_half_rad = .785398163f,
								.near_z = 1,
								.viewport = { 0, 0, width, height } };
	return AeronScene_Begin(scene, &camera) && AeronScene_PrepareResources(scene, AERON_CULL_BACK);
}

int XvtFlightPipeline_Finish(AeronCommandBuffer* cmd, AeronScene3D* scene) {
	int width, height;
	AeronScene_RtDims(scene, &width, &height);
	if (!Ensure(width, height) || !AeronScene_Render(scene, cmd))
		return 0;
	AeronTexture* color = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(scene));
	return XvtFlightPipeline_Resolve(cmd, color, width, height, 1);
}

static int PrepareBars(AeronCommandBuffer* cmd, int width, int height) {
	const XvtPreparedFlight* frame = XvtRemasterFlight_Current();
	g_barsVisible = frame && (frame->content_rect.width != width || frame->content_rect.height != height);
	if (!g_barsVisible)
		return 1;
	if (!g_bars)
		g_bars = AeronDrawList_Create(4);
	if (!g_bars)
		return 0;
	/* Retain this mask with the resolved sources, including across idle frames. */
	AeronRectI r = frame->content_rect;
	const float black[4] = { 0, 0, 0, 1 };
	AeronDrawList_Begin(g_bars, NULL, width, height, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddFill(g_bars, 0, 0, r.x, height, black, AERON_BLIT2D_BLEND_NONE, NULL);
	AeronDrawList_AddFill(g_bars, r.x + r.width, 0, width - r.x - r.width, height, black,
						  AERON_BLIT2D_BLEND_NONE, NULL);
	AeronDrawList_AddFill(g_bars, r.x, 0, r.width, r.y, black, AERON_BLIT2D_BLEND_NONE, NULL);
	AeronDrawList_AddFill(g_bars, r.x, r.y + r.height, r.width, height - r.y - r.height, black,
						  AERON_BLIT2D_BLEND_NONE, NULL);
	return AeronDrawList_Prepare(g_bars, cmd);
}

int XvtFlightPipeline_Resolve(AeronCommandBuffer* cmd, AeronTexture* color, int width, int height,
							  int enable_bloom) {
	if (!Ensure(width, height) || !PrepareBars(cmd, width, height))
		return 0;
	float intensity = enable_bloom ? XvtRemasterConfig_Effective()->bloom_intensity : 0;
	AeronTexture* bloom = NULL;
	if (intensity > 0) {
		if (!AeronSceneBloom_Apply(g_bloom, cmd, color, width, height, height))
			return 0;
		bloom = Aeron_RenderTargetGetTexture(AeronSceneBloom_ColorRt(g_bloom));
		if (!bloom)
			return 0;
	}
	g_color = color;
	g_bloomTexture = bloom;
	g_intensity = intensity;
	g_retained = 0;
	return g_directWanted || XvtFlightPipeline_Retain(cmd);
}

int XvtFlightPipeline_Retain(AeronCommandBuffer* cmd) {
	if (!g_color || g_retained)
		return 1;
	AeronRenderPass* pass =
		Aeron_BeginRenderPass(&(AeronRenderPassDesc) { .color_target = g_target,
													   .clear_color = 1,
													   .clear_color_rgba = { 0, 0, 0, 1 },
													   .command_buffer = cmd,
													   .debug_label = "XvT flight tonemap" });
	if (!pass)
		return 0;
	AeronScenePresentChain_Draw(g_present, pass, g_color, g_sampler, g_bloomTexture, g_intensity, 1, NULL,
								g_width, g_height, 1, (const float[4]) { 1, 1, 1, 1 }, 0);
	/* Mask after tonemapping so bloom cannot brighten the bars. */
	if (g_barsVisible)
		AeronDrawList_RenderIntoPass(g_bars, cmd, pass, g_target);
	Aeron_EndRenderPass(pass);
	g_retained = 1;
	return 1;
}

int XvtFlightPipeline_NeedsRetain(void) { return g_color && !g_retained && !g_directWanted; }

int XvtFlightPipeline_DrawStandaloneHud(AeronCommandBuffer* cmd, int width, int height) {
	if (!Ensure(width, height))
		return 0;
	AeronRenderPass* pass =
		Aeron_BeginRenderPass(&(AeronRenderPassDesc) { .command_buffer = cmd,
													   .color_target = g_target,
													   .clear_color = 1,
													   .clear_color_rgba = { 0, 0, 0, 1 },
													   .debug_label = "XvT standalone flight UI" });
	if (!pass)
		return 0;
	XvtHudRenderer_Draw(cmd, pass, g_target);
	Aeron_EndRenderPass(pass);
	g_color = Aeron_RenderTargetGetTexture(g_target);
	g_bloomTexture = NULL;
	g_directWanted = 0;
	g_retained = 1;
	g_barsVisible = 0;
	return 1;
}

static void Direct(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target, int width,
				   int height, void* user) {
	(void)user;
	if (!g_directWanted || !g_color || width != g_width || height != g_height)
		return;
	AeronRectI full = { 0, 0, width, height };
	Aeron_SetViewport(pass, &full);
	Aeron_SetScissor(pass, &full);
	AeronScenePresentChain_Draw(g_direct, pass, g_color, g_sampler, g_bloomTexture, g_intensity, 1, NULL,
								width, height, 1, (const float[4]) { 1, 1, 1, 1 }, 0);
	if (g_barsVisible)
		AeronDrawList_RenderIntoPass(g_bars, cmd, pass, target);
}

int XvtFlightPipeline_SubmitDirect(void) {
	if (!g_directWanted || !g_color)
		return 0;
	return Aeron_SubmitSwapchainRenderLayer(
		&(AeronSwapchainRenderLayerDesc) { .callback = Direct,
										   .required_width = g_width,
										   .required_height = g_height,
										   .debug_label = "XvT flight direct presentation" });
}

AeronTexture* XvtFlightPipeline_Output(void) {
	return g_color && g_target ? Aeron_RenderTargetGetTexture(g_target) : NULL;
}

void XvtFlightPipeline_Shutdown(void) {
	AeronDrawList_Destroy(g_bars);
	g_bars = NULL;
	g_barsVisible = 0;
	g_directWanted = g_retained = 0;
	g_color = g_bloomTexture = NULL;
	AeronScenePresentChain_Destroy(g_direct);
	g_direct = NULL;
	AeronSceneBloom_Destroy(g_bloom);
	AeronScenePresentChain_Destroy(g_present);
	Aeron_DestroySampler(g_sampler);
	Aeron_DestroyRenderTarget(g_target);
	g_bloom = NULL;
	g_present = NULL;
	g_sampler = NULL;
	g_target = NULL;
	g_lastHostUs = 0;
	g_width = g_height = 0;
}
