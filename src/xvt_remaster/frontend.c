#include "xvt_remaster/frontend.h"
#include "xvt_remaster/preview.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/input_bridge.h"
#include "xvt_runtime/runtime/presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { TARGETS = XVT_TARGET_FRONT_SAVED_FIRST + XVT_TARGET_FRONT_SAVED_COUNT };

static AeronRenderTarget* g_targets[TARGETS];
static AeronDrawList2D* g_list;
static int g_width, g_height, g_presented, g_movie;
static float g_scale;
static int g_releasePresented;
static uint64_t g_tick = UINT64_MAX;

static XvtSnapRect g_savedBounds[TARGETS];
static AeronRenderTarget* g_cursorTarget;
static XvtSnapSprite g_cursor;
static int g_cursorVisible;

static int IsSavedTarget(unsigned id) { return id >= XVT_TARGET_FRONT_SAVED_FIRST && id < TARGETS; }

static int IsFrontendTarget(unsigned id) {
	return id <= XVT_TARGET_FRONT_BACKUP || id == XVT_TARGET_FRONT_PRESENTED ||
		   id == XVT_TARGET_FRONT_MOVIE || IsSavedTarget(id);
}

static XvtSnapRect ScaleRect(XvtSnapRect rect, float scale) {
	int left = (int)roundf(rect.x * scale), top = (int)roundf(rect.y * scale);
	return (XvtSnapRect) { left, top, (int)roundf((rect.x + rect.width) * scale) - left,
						   (int)roundf((rect.y + rect.height) * scale) - top };
}

static AeronRenderTarget* CreateTarget(unsigned id, int width, int height) {
	const char* name;
	char saved_name[48];
	switch (id) {
		case XVT_TARGET_FRONT_BACK:
			name = "xvt.frontend.back";
			break;
		case XVT_TARGET_FRONT_OFFSCREEN:
			name = "xvt.frontend.offscreen";
			break;
		case XVT_TARGET_FRONT_BACKUP:
			name = "xvt.frontend.backup";
			break;
		case XVT_TARGET_FRONT_PRESENTED:
			name = "xvt.frontend.presented";
			break;
		case XVT_TARGET_FRONT_MOVIE:
			name = "xvt.frontend.movie";
			break;
		default:
			snprintf(saved_name, sizeof saved_name, "xvt.frontend.saved.%u",
					 id - XVT_TARGET_FRONT_SAVED_FIRST);
			name = saved_name;
			break;
	}
	return Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
		.width = width, .height = height, .format = AERON_TEXTURE_FORMAT_RGBA8_SRGB, .debug_name = name });
}

static int ClearTarget(AeronCommandBuffer* cmd, unsigned id, uint32_t color) {
	float rgba[4];
	XvtUi_Color(color, rgba);
	AeronTexture* texture = Aeron_RenderTargetGetTexture(g_targets[id]);
	AeronDrawList_Begin(g_list, g_targets[id], Aeron_TextureGetWidth(texture),
						Aeron_TextureGetHeight(texture), AERON_DRAWLIST2D_CLEAR, rgba);
	AeronDrawList_Render(g_list, cmd);
	return 1;
}

static int PrepareTarget(AeronCommandBuffer* cmd, unsigned id) {
	if (!IsFrontendTarget(id))
		return 0;
	if (g_targets[id])
		return 1;
	XvtSnapRect bounds =
		IsSavedTarget(id) ? ScaleRect(g_savedBounds[id], g_scale) : (XvtSnapRect) { 0, 0, g_width, g_height };
	if (bounds.width <= 0 || bounds.height <= 0)
		return 0;
	g_targets[id] = CreateTarget(id, bounds.width, bounds.height);
	return g_targets[id] &&
		   (IsSavedTarget(id) || ClearTarget(cmd, id, id == XVT_TARGET_FRONT_MOVIE ? 0 : 0xff000000u));
}

static int ResizeTargets(AeronCommandBuffer* cmd, int width, int height, float scale) {
	AeronRenderTarget* replacements[TARGETS] = { 0 };
	for (unsigned id = 0; id < TARGETS; ++id) {
		if (!g_targets[id])
			continue;
		XvtSnapRect bounds =
			IsSavedTarget(id) ? ScaleRect(g_savedBounds[id], scale) : (XvtSnapRect) { 0, 0, width, height };
		replacements[id] = CreateTarget(id, bounds.width, bounds.height);
		if (!replacements[id])
			goto failed;
		AeronTexture* source = Aeron_RenderTargetGetTexture(g_targets[id]);
		int source_width = Aeron_TextureGetWidth(source), source_height = Aeron_TextureGetHeight(source);
		if (!XvtUi_CopyFrontend(
				cmd, replacements[id], source, &(XvtSnapRect) { 0, 0, source_width, source_height },
				&(XvtSnapRect) { 0, 0, bounds.width, bounds.height }, source_width, source_height))
			goto failed;
	}
	for (unsigned id = 0; id < TARGETS; ++id) {
		Aeron_DestroyRenderTarget(g_targets[id]);
		g_targets[id] = replacements[id];
	}
	return 1;
failed:
	for (unsigned id = 0; id < TARGETS; ++id)
		Aeron_DestroyRenderTarget(replacements[id]);
	return 0;
}

static int PrepareTargets(AeronCommandBuffer* cmd, int width, int height) {
	if (!g_list)
		g_list = AeronDrawList_Create(65536);
	if (!g_list)
		return 0;
	float scale = fminf(width / 640.0f, height / 480.0f);
	int w = (int)ceilf(640 * scale), h = (int)ceilf(480 * scale);
	if ((w != g_width || h != g_height) && !ResizeTargets(cmd, w, h, scale))
		return 0;
	g_scale = scale;
	g_width = w;
	g_height = h;
	return PrepareTarget(cmd, XVT_TARGET_FRONT_BACK) && PrepareTarget(cmd, XVT_TARGET_FRONT_PRESENTED);
}

static int CopyRegion(AeronCommandBuffer* cmd, unsigned src, unsigned dst, XvtSnapRect from, XvtSnapRect to) {
	/* Saved slots use local texture coordinates; captured rectangles stay in screen space. */
	if (IsSavedTarget(src) && !g_targets[src])
		return 0;
	if (IsSavedTarget(dst)) {
		Aeron_DestroyRenderTarget(g_targets[dst]);
		g_targets[dst] = NULL;
		g_savedBounds[dst] = to;
	}
	if (!PrepareTarget(cmd, src) || !PrepareTarget(cmd, dst))
		return 0;
	from = ScaleRect(from, g_scale);
	to = ScaleRect(to, g_scale);
	if (IsSavedTarget(src)) {
		XvtSnapRect origin = ScaleRect(g_savedBounds[src], g_scale);
		from.x -= origin.x;
		from.y -= origin.y;
	}
	if (IsSavedTarget(dst)) {
		XvtSnapRect origin = ScaleRect(g_savedBounds[dst], g_scale);
		to.x -= origin.x;
		to.y -= origin.y;
	}
	AeronTexture* source = Aeron_RenderTargetGetTexture(g_targets[src]);
	int ok = XvtUi_CopyFrontend(cmd, g_targets[dst], source, &from, &to, Aeron_TextureGetWidth(source),
								Aeron_TextureGetHeight(source));
	if (ok && IsSavedTarget(src)) {
		Aeron_DestroyRenderTarget(g_targets[src]);
		g_targets[src] = NULL;
		memset(&g_savedBounds[src], 0, sizeof g_savedBounds[src]);
	}
	return ok;
}

static int DrawSprite(const XvtSnapSprite* b, float scale) {
	const AeronRuntimeAtlas* a = XvtRemasterAssets_FindFrontendImage(b);
	if (!a)
		return 0;
	for (int i = 0; i < a->layout.frame_count; ++i)
		if ((unsigned)a->layout.ids[i] == b->frame) {
			const AeronRuntimeAtlasPage* p = &a->pages[a->layout.pages[i]];
			const AeronSpriteRect* r = &a->layout.frames[i];
			float sx = r->w / a->layout.size[i * 2], sy = r->h / a->layout.size[i * 2 + 1];
			AeronDrawList2DSprite d = { .texture = p->texture,
										.src_u0 = (r->x + b->source.x * sx) / p->width,
										.src_v0 = (r->y + b->source.y * sy) / p->height,
										.src_u1 = (r->x + (b->source.x + b->source.width) * sx) / p->width,
										.src_v1 = (r->y + (b->source.y + b->source.height) * sy) / p->height,
										.dst_x = b->destination.x * scale,
										.dst_y = b->destination.y * scale,
										.dst_w = b->destination.width * scale,
										.dst_h = b->destination.height * scale,
										.blend = AERON_BLIT2D_BLEND_PMA,
										.filter = AERON_BLIT2D_FILTER_NEAREST };
			XvtSnapRect clip = ScaleRect(b->draw.clip, scale);
			d.scissor = (AeronRectI) { clip.x, clip.y, clip.width, clip.height };
			XvtUi_Color(b->kind == XVT_SPRITE_FRONT_TRANSLUCENT ? 0x80ffffffu : 0xffffffffu, d.tint);
			AeronDrawList_AddSprite(g_list, &d);
			return 1;
		}
	return 0;
}

static void DrawPreview(unsigned i) {
	const XvtPreviewOutput* p = XvtRemasterPreview_Output(i);
	if (!p)
		return;
	XvtSnapRect r = ScaleRect(p->destination, g_scale), clip = ScaleRect(p->draw.clip, g_scale);
	AeronDrawList2DSprite d = { .texture = p->texture,
								.src_u1 = 1,
								.src_v1 = 1,
								.dst_x = r.x,
								.dst_y = r.y,
								.dst_w = r.width,
								.dst_h = r.height,
								.tint = { 1, 1, 1, 1 },
								.blend = AERON_BLIT2D_BLEND_PMA,
								.filter = AERON_BLIT2D_FILTER_LINEAR,
								.scissor = { clip.x, clip.y, clip.width, clip.height } };
	AeronDrawList_AddSprite(g_list, &d);
}

static int RenderCursor(AeronCommandBuffer* cmd, const XvtSnapSprite* cursor) {
	g_cursorVisible = 0;
	if (!cursor)
		return 1;
	int width = cursor->destination.width, height = cursor->destination.height;
	if (width <= 0 || height <= 0)
		return 0;
	AeronTexture* texture = g_cursorTarget ? Aeron_RenderTargetGetTexture(g_cursorTarget) : NULL;
	if (!texture || Aeron_TextureGetWidth(texture) != width || Aeron_TextureGetHeight(texture) != height) {
		Aeron_DestroyRenderTarget(g_cursorTarget);
		g_cursorTarget =
			Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
																.height = height,
																.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
																.debug_name = "xvt.frontend.cursor" });
		if (!g_cursorTarget)
			return 0;
	}
	/* Own the pixels with the held presentation, independently of source asset lifetime. */
	XvtSnapSprite local = *cursor;
	local.destination = local.draw.clip = (XvtSnapRect) { 0, 0, width, height };
	const float clear[4] = { 0, 0, 0, 0 };
	AeronDrawList_Begin(g_list, g_cursorTarget, width, height, AERON_DRAWLIST2D_CLEAR, clear);
	if (!DrawSprite(&local, 1))
		return 0;
	AeronDrawList_Render(g_list, cmd);
	g_cursor = *cursor;
	g_cursorVisible = 1;
	return 1;
}

static int ApplySurfaceEvent(AeronCommandBuffer* cmd, const XvtSnapSurfaceEvent* e,
							 const XvtSnapSprite* cursor) {
	if (!IsFrontendTarget(e->target))
		return 1;
	if (e->kind == XVT_SURFACE_PRESENT) {
		if (e->target == XVT_TARGET_FRONT_MOVIE) {
			g_movie = 1;
			return 1;
		}
		AeronDrawList_Begin(g_list, g_targets[XVT_TARGET_FRONT_PRESENTED], g_width, g_height,
							AERON_DRAWLIST2D_CLEAR, NULL);
		AeronDrawList2DSprite background = { .texture = Aeron_RenderTargetGetTexture(
												 g_targets[XVT_TARGET_FRONT_BACK]),
											 .src_u1 = 1,
											 .src_v1 = 1,
											 .dst_w = g_width,
											 .dst_h = g_height,
											 .tint = { 1, 1, 1, 1 },
											 .blend = AERON_BLIT2D_BLEND_NONE,
											 .filter = AERON_BLIT2D_FILTER_NEAREST };
		AeronDrawList_AddSprite(g_list, &background);
		AeronDrawList_Render(g_list, cmd);
		if (!RenderCursor(cmd, cursor))
			return 0;
		g_presented = 1;
		g_releasePresented = 0;
	} else if (e->kind == XVT_SURFACE_RESET) {
		/* Recreated classic surfaces repaint through subsequent events. The held
		 * presentation and independent screen-stack copies survive the reset. */
		for (unsigned i = 0; i < TARGETS; ++i)
			if (g_targets[i] && i != XVT_TARGET_FRONT_PRESENTED && !IsSavedTarget(i) &&
				!ClearTarget(cmd, i, i == XVT_TARGET_FRONT_MOVIE ? 0 : 0xff000000u))
				return 0;
	} else if (!PrepareTarget(cmd, e->target) || !ClearTarget(cmd, e->target, e->color_argb))
		return 0;
	return 1;
}

int XvtFrontend_AssetsNeedPreparation(const XvtRenderSnapshot* snapshot) {
	for (unsigned i = 0; i < snapshot->sprite_count; ++i) {
		const XvtSnapSprite* sprite = &snapshot->sprites[i];
		if (sprite->draw.scope != XVT_SCOPE_FRONTEND)
			continue;
		if (!XvtRemasterAssets_FindFrontendImage(sprite))
			return 1;
	}
	return 0;
}

int XvtFrontend_PrepareAssets(AeronCommandBuffer* cmd, const XvtRenderSnapshot* snapshot) {
	for (unsigned i = 0; i < snapshot->sprite_count; ++i) {
		const XvtSnapSprite* sprite = &snapshot->sprites[i];
		if (sprite->draw.scope != XVT_SCOPE_FRONTEND)
			continue;
		if (!XvtRemasterAssets_PrepareFrontendImage(cmd, sprite)) {
			Aeron_CommandBufferSetFailure(cmd, "frontend image preparation");
			return 0;
		}
	}
	return 1;
}

int XvtFrontend_NeedsReplay(const XvtRenderSnapshot* s, int w, int h) {
	if (g_list && s->presented_target != XVT_TARGET_FLIGHT_MAIN &&
		((int)ceilf(640 * fminf(w / 640.0f, h / 480.0f)) != g_width ||
		 (int)ceilf(480 * fminf(w / 640.0f, h / 480.0f)) != g_height))
		return 1;
	if (s->tick_index == g_tick)
		return 0;
	for (unsigned i = 0; i < s->sprite_count; ++i)
		if (s->sprites[i].draw.scope == XVT_SCOPE_FRONTEND)
			return 1;
	for (unsigned i = 0; i < s->glyph_count; ++i)
		if (s->glyphs[i].draw.scope == XVT_SCOPE_FRONTEND)
			return 1;
	for (unsigned i = 0; i < s->paint_count; ++i)
		if (s->paint[i].draw.scope == XVT_SCOPE_FRONTEND)
			return 1;
	for (unsigned i = 0; i < s->copy_count; ++i)
		if (s->copies[i].draw.scope == XVT_SCOPE_FRONTEND)
			return 1;
	for (unsigned i = 0; i < s->surface_event_count; ++i)
		if (IsFrontendTarget(s->surface_events[i].target))
			return 1;
	return s->preview_count != 0;
}

int XvtFrontend_Replay(AeronCommandBuffer* cmd, const XvtRenderSnapshot* s, int width, int height) {
	if (!PrepareTargets(cmd, width, height))
		return 0;
	if (g_tick == s->tick_index)
		return 1;
	unsigned indices[6] = { 0 };
	const XvtSnapSprite* cursor = NULL;
	int active = -1;
	const unsigned counts[6] = { s->sprite_count, s->glyph_count,         s->paint_count,
								 s->copy_count,   s->surface_event_count, s->preview_count };
	for (;;) {
		uint32_t order = UINT32_MAX;
		unsigned ch = 6;
		const uint32_t orders[6] = {
			indices[0] < counts[0] ? s->sprites[indices[0]].draw.z_order : UINT32_MAX,
			indices[1] < counts[1] ? s->glyphs[indices[1]].draw.z_order : UINT32_MAX,
			indices[2] < counts[2] ? s->paint[indices[2]].draw.z_order : UINT32_MAX,
			indices[3] < counts[3] ? s->copies[indices[3]].draw.z_order : UINT32_MAX,
			indices[4] < counts[4] ? s->surface_events[indices[4]].z_order : UINT32_MAX,
			indices[5] < counts[5] ? s->previews[indices[5]].draw.z_order : UINT32_MAX
		};
		for (unsigned j = 0; j < 6; ++j)
			if (orders[j] < order) {
				order = orders[j];
				ch = j;
			}
		if (ch == 6)
			break;
		unsigned i = indices[ch]++;
		if (ch == 3 || ch == 4) {
			if (active >= 0)
				AeronDrawList_Render(g_list, cmd);
			active = -1;
			if (ch == 4) {
				if (!ApplySurfaceEvent(cmd, &s->surface_events[i], cursor))
					return 0;
				if (s->surface_events[i].kind == XVT_SURFACE_PRESENT)
					cursor = NULL;
			} else {
				const XvtSnapCopyRect* c = &s->copies[i];
				if (c->draw.scope == XVT_SCOPE_FRONTEND &&
					!CopyRegion(cmd, c->source_target, c->draw.target, c->source, c->destination))
					return 0;
			}
			continue;
		}
		const XvtSnapDrawHeader* h = ch == 0   ? &s->sprites[i].draw
									 : ch == 1 ? &s->glyphs[i].draw
									 : ch == 2 ? &s->paint[i].draw
											   : &s->previews[i].draw;
		if (ch == 0 && h->target == XVT_TARGET_FRONT_CURSOR) {
			cursor = &s->sprites[i];
			continue;
		}
		if (h->scope != XVT_SCOPE_FRONTEND || !IsFrontendTarget(h->target))
			continue;
		if (active != h->target) {
			if (active >= 0)
				AeronDrawList_Render(g_list, cmd);
			if (!PrepareTarget(cmd, h->target))
				return 0;
			active = h->target;
			AeronDrawList_Begin(g_list, g_targets[active], g_width, g_height, AERON_DRAWLIST2D_LOAD, NULL);
		}
		if (ch == 0) {
			if (!DrawSprite(&s->sprites[i], g_scale))
				return 0;
		} else if (ch == 1)
			XvtUi_Glyph(g_list, &s->glyphs[i], g_scale, 0, 0);
		else if (ch == 2)
			XvtUi_Paint(g_list, &s->paint[i], g_scale);
		else
			DrawPreview(i);
	}
	if (active >= 0)
		AeronDrawList_Render(g_list, cmd);
	g_tick = s->tick_index;
	return 1;
}

AeronTexture* XvtFrontend_Output(void) {
	return g_presented ? Aeron_RenderTargetGetTexture(g_targets[XVT_TARGET_FRONT_PRESENTED]) : NULL;
}

AeronTexture* XvtFrontend_MovieOverlay(void) {
	return g_movie ? Aeron_RenderTargetGetTexture(g_targets[XVT_TARGET_FRONT_MOVIE]) : NULL;
}

void XvtFrontend_PresentCursor(float opacity) {
	if (!g_presented || !g_cursorVisible || !g_cursorTarget || opacity <= 0 || XvtInput_IsCaptured() ||
		Aeron_DebugUiVisible() || Aeron_RelativeMouseMode())
		return;
	int x = g_cursor.destination.x, y = g_cursor.destination.y;
	/* Align with the baked classic cursor during the renderer crossfade. */
	if (opacity >= 1)
		XvtInput_FrontendCursorPosition(&x, &y);
	AeronRectI classic = XvtPresentation_ClassicRect();
	XvtSnapRect clip = g_cursor.draw.clip;
	int left = clip.x > 0 ? clip.x : 0, top = clip.y > 0 ? clip.y : 0;
	int right = clip.x + clip.width < 640 ? clip.x + clip.width : 640;
	int bottom = clip.y + clip.height < 480 ? clip.y + clip.height : 480;
	if (right <= left || bottom <= top)
		return;
	AeronTextureLayerDesc layer = {
		.texture = Aeron_RenderTargetGetTexture(g_cursorTarget),
		.logical_rect = { classic.x + x, classic.y + y, g_cursor.destination.width,
						  g_cursor.destination.height },
		.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
		.color_space = AERON_COLOR_SPACE_SRGB,
		.tint_enabled = 1,
		.tint_rgba = { opacity, opacity, opacity, opacity },
		.scissor = { classic.x + left, classic.y + top, right - left, bottom - top }
	};
	if (!Aeron_SubmitTextureLayer(&layer))
		Aeron_RequestFatalRendererError("frontend cursor presentation");
}

void XvtFrontend_Shutdown(void) {
	for (unsigned i = 0; i < TARGETS; ++i) {
		Aeron_DestroyRenderTarget(g_targets[i]);
		g_targets[i] = NULL;
	}
	AeronDrawList_Destroy(g_list);
	g_list = NULL;
	g_tick = UINT64_MAX;
	g_width = g_height = g_presented = g_movie = 0;
	g_releasePresented = 0;
	Aeron_DestroyRenderTarget(g_cursorTarget);
	g_cursorTarget = NULL;
	g_cursorVisible = 0;
	memset(g_savedBounds, 0, sizeof g_savedBounds);
}

void XvtFrontend_ReleaseForFlight(void) {
	for (unsigned id = 0; id < TARGETS; ++id) {
		/* Saved screen-stack images outlive the classic surfaces and must survive a later pop. */
		if (id == XVT_TARGET_FRONT_PRESENTED || IsSavedTarget(id))
			continue;
		Aeron_DestroyRenderTarget(g_targets[id]);
		g_targets[id] = NULL;
	}
	AeronDrawList_Destroy(g_list);
	g_list = NULL;
	g_movie = 0;
	g_releasePresented = 1;
}

void XvtFrontend_ReleasePresented(void) {
	if (!g_releasePresented)
		return;
	Aeron_DestroyRenderTarget(g_targets[XVT_TARGET_FRONT_PRESENTED]);
	g_targets[XVT_TARGET_FRONT_PRESENTED] = NULL;
	g_presented = g_releasePresented = 0;
	Aeron_DestroyRenderTarget(g_cursorTarget);
	g_cursorTarget = NULL;
	g_cursorVisible = 0;
}
