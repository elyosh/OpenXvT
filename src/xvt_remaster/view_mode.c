#include "xvt_remaster/view_mode.h"
#include "aeron/compat/host.h"
#include "aeron/scene/blend_ramp.h"
#include "xvt_remaster/flight.h"
#include "xvt_remaster/flight_pipeline.h"
#include "xvt_remaster/frontend.h"
#include "xvt_remaster/hud_renderer.h"
#include "xvt_remaster/preview.h"
#include "xvt_remaster/xvt_remaster.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/input_bridge.h"
#include "xvt_runtime/input/keyboard_mapping.h"
#include "xvt_runtime/runtime/movie_task.h"
#include "xvt_runtime/runtime/presentation.h"

static AeronBlendRamp g_blend;
static int g_waitClassic, g_consumedTab, g_worldReady;
static uint64_t g_classicSerial, g_worldSerial, g_mission;
static int g_width, g_height;

enum { DISPLAY_NONE, DISPLAY_FRONTEND, DISPLAY_FLIGHT, DISPLAY_LOADING };

static unsigned g_display;

void XvtRemasterView_Init(void) {
	Aeron_BlendRampInit(&g_blend);
	g_blend.alpha = 0;
	g_waitClassic = g_consumedTab = g_worldReady = 0;
	g_worldSerial = g_mission = g_classicSerial = 0;
	g_width = g_height = 0;
	g_display = DISPLAY_NONE;
	XvtInput_SuppressRendererTab(0);
}

static int FlightReady(const XvtRenderSnapshot* s) {
	return s && s->flight_valid && s->scene_kind == XVT_SCENE_FLIGHT &&
		   s->presented_target == XVT_TARGET_FLIGHT_MAIN && g_worldReady &&
		   s->flight_frame_serial == g_worldSerial && s->mission_generation == g_mission;
}

void XvtRemasterView_BeginFrame(const AeronInputSnapshot* in) {
	const XvtRenderSnapshot* s = XvtRenderSnapshot_Current();
	if (in)
		XvtPresentation_SyncToAvailable(in->window_width, in->window_height);
	if (g_consumedTab && in && !in->key_down[AERON_KEY_TAB] && !in->key_released[AERON_KEY_TAB])
		g_consumedTab = 0;
	if (in && !g_consumedTab && in->has_focus &&
		XvtKeyboardMapping_Trigger(in, XVT_KEYBOARD_SHORTCUT_RENDERER) >= 0 && s &&
		s->scene_kind != XVT_SCENE_NONE && s->scene_kind != XVT_SCENE_MOVIE && XvtRemaster_Output() &&
		XvtInput_RendererShortcutAllowed()) {
		g_consumedTab = 1;
		Aeron_BlendRampToggle(&g_blend);
		if (g_blend.target > 0) {
			g_waitClassic = 0;
			g_worldReady = 0;
			XvtRemasterFlight_Invalidate();
			XvtHudRenderer_Invalidate();
			XvtRemasterPreview_InvalidateCrt();
		} else if (AeronDx5_IsClassicFlightRenderingSuppressed()) {
			g_waitClassic = 1;
			g_classicSerial = AeronDx5_GetClassicFlightFrameSerial();
		}
		Aeron_LogInfo("xvt.remaster", "renderer requested: %s", g_blend.target > 0 ? "modern" : "classic");
	}
	XvtInput_SuppressRendererTab(g_consumedTab);
	int width = 0, height = 0;
	Aeron_GetPresentationPixelSize(&width, &height);
	int suppress = in && in->has_focus && g_blend.target > 0 && g_blend.alpha >= 1 && !g_waitClassic &&
				   FlightReady(s) && width == g_width && height == g_height;
	AeronDx5_SetClassicFlightRenderingSuppressed(suppress);
}

int XvtRemasterView_NeedsWorld(const XvtRenderSnapshot* s) {
	return !(s->presented_target == XVT_TARGET_FLIGHT_MAIN && g_blend.target == 0 && g_blend.alpha == 0 &&
			 !g_waitClassic);
}

int XvtRemasterView_Direct(const XvtRenderSnapshot* s, int width, int height) {
	return s->flight_valid && s->scene_kind == XVT_SCENE_FLIGHT && g_worldReady &&
		   s->mission_generation == g_mission && g_blend.target > 0 && g_blend.alpha >= 1 && !g_waitClassic &&
		   AeronDx5_IsClassicFlightRenderingSuppressed() && XvtFlightPipeline_SetDirect(1, width, height);
}

void XvtRemasterView_Present(const XvtRenderSnapshot* s, int32_t delta_us, int world_ready, int direct) {
	if (world_ready && s->flight_valid) {
		g_worldReady = 1;
		g_worldSerial = s->flight_frame_serial;
		g_mission = s->mission_generation;
		const XvtPreparedFlight* frame = XvtRemasterFlight_Current();
		if (frame) {
			g_width = frame->view.camera.viewport.width;
			g_height = frame->view.camera.viewport.height;
		}
	}
	if (g_waitClassic && AeronDx5_GetClassicFlightFrameSerial() != g_classicSerial)
		g_waitClassic = 0;
	if (s->scene_kind == XVT_SCENE_MOVIE) {
		AeronTexture* overlay = XvtRemaster_MovieOverlay();
		if (g_blend.target > 0 && overlay) {
			AeronTextureLayerDesc layer = { .texture = overlay,
											.logical_rect = XvtPresentation_ClassicRect(),
											.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
											.color_space = AERON_COLOR_SPACE_SRGB };
			if (!Aeron_SubmitTextureLayer(&layer))
				Aeron_RequestFatalRendererError("movie overlay presentation");
			else
				XvtMovieTask_SuppressClassicSubtitles();
		}
		return;
	}
	int requested_flight =
		s->presented_target == XVT_TARGET_FLIGHT_MAIN && s->presented_scene == XVT_SCENE_FLIGHT;
	int ready = XvtRemaster_Output() && (!requested_flight || FlightReady(s) || !s->flight_valid);
	if (ready)
		g_display = requested_flight                                ? DISPLAY_FLIGHT
					: s->presented_target == XVT_TARGET_FLIGHT_MAIN ? DISPLAY_LOADING
																	: DISPLAY_FRONTEND;
	/* Select retained owners, not borrowed pointers that resize can replace. */
	if (g_display != DISPLAY_FRONTEND)
		XvtFrontend_ReleasePresented();
	AeronTexture* output = g_display == DISPLAY_FLIGHT     ? XvtFlightPipeline_Output()
						   : g_display == DISPLAY_FRONTEND ? XvtFrontend_Output()
						   : g_display == DISPLAY_LOADING  ? XvtFlightPipeline_Output()
														   : NULL;
	int flight = g_display == DISPLAY_FLIGHT;
	float target = g_waitClassic ? 1 : (g_blend.target > 0 && ready ? 1 : 0);
	if (!ready && AeronDx5_IsClassicFlightRenderingSuppressed()) {
		/* Keep the last complete modern frame opaque while classic resumes. */
		target = 1;
		g_worldReady = 0;
		AeronDx5_SetClassicFlightRenderingSuppressed(0);
	}
	Aeron_BlendRampAdvance(&g_blend, delta_us, target);
	if (g_blend.alpha <= 0) {
		XvtFrontend_ReleasePresented();
		return;
	}
	if (!output)
		return;
	if (direct && g_blend.alpha >= 1) {
		if (!XvtFlightPipeline_SubmitDirect())
			Aeron_RequestFatalRendererError("direct flight presentation");
		return;
	}
	AeronTextureLayerDesc layer = {
		.texture = output,
		.logical_rect =
			g_display == DISPLAY_FRONTEND ? XvtPresentation_ClassicRect() : XvtPresentation_Frame(),
		.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
		.color_space = flight                          ? AERON_COLOR_SPACE_LINEAR_SRGB
					   : g_display == DISPLAY_FRONTEND ? AERON_COLOR_SPACE_SRGB
													   : AERON_COLOR_SPACE_LINEAR_DISPLAY,
		.tint_enabled = 1,
		.tint_rgba = { g_blend.alpha, g_blend.alpha, g_blend.alpha, g_blend.alpha }
	};
	if (!Aeron_SubmitTextureLayer(&layer))
		Aeron_RequestFatalRendererError("modern texture presentation");
	else if (g_display == DISPLAY_FRONTEND)
		XvtFrontend_PresentCursor(g_blend.alpha);
}

void XvtRemasterView_Shutdown(void) {
	AeronDx5_SetClassicFlightRenderingSuppressed(0);
	XvtInput_SuppressRendererTab(0);
	g_waitClassic = g_consumedTab = g_worldReady = 0;
}
