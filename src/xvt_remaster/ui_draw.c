#include "xvt_remaster/ui_draw.h"
#include "aeron/aeron.h"
#include <math.h>
#include <string.h>

static AeronShader *g_vs, *g_fs;
static AeronGraphicsPipeline* g_copy;
static AeronSampler* g_nearest;

void XvtUi_Color(uint32_t argb, float out[4]) {
	out[3] = (float)(argb >> 24) / 255;
	for (int c = 0; c < 3; ++c) {
		float s = (float)((argb >> (16 - c * 8)) & 255) / 255;
		out[c] = (s <= .04045f ? s / 12.92f : powf((s + .055f) / 1.055f, 2.4f)) * out[3];
	}
}

static AeronRectI Clip(XvtSnapRect r, float s) {
	return (AeronRectI) { (int)floorf(r.x * s), (int)floorf(r.y * s), (int)ceilf(r.width * s),
						  (int)ceilf(r.height * s) };
}

static void GlyphPlane(AeronDrawList2D* list, const XvtFontAtlas* font, unsigned ch, float x, float y,
					   float scale, uint32_t color, const AeronRectI* clip) {
	if (!font || ch < font->atlas.first_char || ch >= font->atlas.first_char + font->atlas.num_chars)
		return;
	const AeronFontGlyph* g = &font->atlas.glyphs[ch - font->atlas.first_char];
	const XvtFontGlyph* metrics = &font->glyphs[ch - font->atlas.first_char];
	AeronDrawList2DSprite d = { .texture = font->atlas.texture,
								.src_u0 = (float)g->atlas_x / font->atlas.atlas_w,
								.src_v0 = (float)g->atlas_y / font->atlas.atlas_h,
								.src_u1 = (float)(g->atlas_x + g->atlas_w) / font->atlas.atlas_w,
								.src_v1 = (float)(g->atlas_y + g->atlas_h) / font->atlas.atlas_h,
								.dst_x = x,
								.dst_y = y,
								.dst_w = metrics->width * scale,
								.dst_h = metrics->height * scale,
								.blend = AERON_BLIT2D_BLEND_PMA,
								.filter = AERON_BLIT2D_FILTER_LINEAR };
	if (clip)
		d.scissor = *clip;
	XvtUi_Color(color, d.tint);
	AeronDrawList_AddSprite(list, &d);
}

void XvtUi_Glyph(AeronDrawList2D* list, const XvtSnapGlyph* g, float scale, float ox, float oy) {
	const XvtFontAtlas* font = XvtRemasterAssets_Font(g->font_asset_id, 0);
	if (!font || g->character < font->atlas.first_char ||
		g->character >= font->atlas.first_char + font->atlas.num_chars)
		return;
	AeronRectI clip = Clip(g->draw.clip, scale);
	clip.x += (int)ox;
	clip.y += (int)oy;
	float x = g->x * scale + ox, y = g->y * scale + oy;
	if (g->background_enabled) {
		float rgba[4];
		XvtUi_Color(g->background_argb, rgba);
		AeronDrawList_AddFill(list, x, y, (g->advance + (g->shadow_enabled ? 1 : 0)) * scale,
							  font->glyphs[g->character - font->atlas.first_char].height * scale, rgba,
							  AERON_BLIT2D_BLEND_NONE, &clip);
	}
	if (g->shadow_enabled) {
		/* XvT shifts the previous foreground row right one bit; it does not read the stored shadow plane. */
		AeronRectI shadow_clip = clip;
		int bottom = (int)ceilf(y + font->glyphs[g->character - font->atlas.first_char].height * scale);
		if (shadow_clip.y + shadow_clip.height > bottom)
			shadow_clip.height = bottom - shadow_clip.y;
		if (shadow_clip.height > 0)
			GlyphPlane(list, font, g->character, x + scale, y + scale, scale, g->shadow_argb, &shadow_clip);
	}
	GlyphPlane(list, font, g->character, x, y, scale, g->foreground_argb, &clip);
}

void XvtUi_Text(AeronDrawList2D* list, uint64_t id, const char* text, float x, float y, float scale,
				uint32_t color, int centered) {
	const XvtFontAtlas* font = XvtRemasterAssets_Font(id, 0);
	if (!font || !text)
		return;
	float width = 0;
	for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
		if (*p >= font->atlas.first_char && *p < font->atlas.first_char + font->atlas.num_chars)
			width += font->glyphs[*p - font->atlas.first_char].advance * scale;
	if (centered)
		x -= width / 2;
	for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
		GlyphPlane(list, font, *p, x, y, scale, color, NULL);
		if (*p >= font->atlas.first_char && *p < font->atlas.first_char + font->atlas.num_chars)
			x += font->glyphs[*p - font->atlas.first_char].advance * scale;
	}
}

int XvtUi_MapIcon(AeronDrawList2D* list, AeronCommandBuffer* cmd, uint64_t asset_id, unsigned frame,
				  int remap, const uint32_t palette[256], float x, float y, float scale, int width,
				  int height) {
	const AeronRuntimeAtlas* atlas = XvtRemasterAssets_PrepareMapIcons(cmd, asset_id, palette, remap);
	if (!atlas)
		return 0;
	for (int index = 0; index < atlas->layout.frame_count; ++index) {
		if ((unsigned)atlas->layout.ids[index] != frame)
			continue;
		const AeronSpriteRect* rect = &atlas->layout.frames[index];
		const AeronRuntimeAtlasPage* page = &atlas->pages[atlas->layout.pages[index]];
		AeronDrawList2DSprite sprite = { .texture = page->texture,
										 .src_u0 = rect->x / page->width,
										 .src_v0 = rect->y / page->height,
										 .src_u1 = (rect->x + rect->w) / page->width,
										 .src_v1 = (rect->y + rect->h) / page->height,
										 .dst_x = x,
										 .dst_y = y,
										 .dst_w = atlas->layout.size[index * 2] * scale,
										 .dst_h = atlas->layout.size[index * 2 + 1] * scale,
										 .tint = { 1, 1, 1, 1 },
										 .blend = AERON_BLIT2D_BLEND_PMA,
										 .filter = AERON_BLIT2D_FILTER_LINEAR,
										 .scissor = { 0, 0, width, height } };
		AeronDrawList_AddSprite(list, &sprite);
		break;
	}
	return 1;
}

void XvtUi_Paint(AeronDrawList2D* list, const XvtSnapPaint* p, float scale) {
	float rgba[4];
	XvtUi_Color(p->color_argb, rgba);
	AeronRectI clip = Clip(p->draw.clip, scale);
	if (p->kind == XVT_PAINT_LINE)
		AeronDrawList_AddLine(list, ((p->x0 + .5f) * scale), ((p->y0 + .5f) * scale), ((p->x1 + .5f) * scale),
							  ((p->y1 + .5f) * scale), scale, rgba, AERON_BLIT2D_BLEND_NONE, &clip);
	else if (p->kind == XVT_PAINT_FRAME)
		AeronDrawList_AddFrame(list, (p->x0 * scale), (p->y0 * scale), (p->x1 - p->x0) * scale,
							   (p->y1 - p->y0) * scale, scale, rgba, AERON_BLIT2D_BLEND_NONE, &clip);
	else
		AeronDrawList_AddFill(
			list, (p->x0 * scale), (p->y0 * scale), (p->x1 - p->x0) * scale, (p->y1 - p->y0) * scale, rgba,
			p->kind == XVT_PAINT_TRANSLUCENT ? AERON_BLIT2D_BLEND_PMA : AERON_BLIT2D_BLEND_NONE, &clip);
}

int XvtUi_CopyFrontend(AeronCommandBuffer* cmd, AeronRenderTarget* dst, AeronTexture* src,
					   const XvtSnapRect* from, const XvtSnapRect* to, int sw, int sh) {
	if (!g_copy) {
		g_vs = Aeron_CreateShader(
			&(AeronShaderDesc) { .name = "scene_fullscreen_quad.vert", .stage = AERON_SHADER_STAGE_VERTEX });
		g_fs = Aeron_CreateShader(&(AeronShaderDesc) { .name = "frontend_copy.frag",
													   .stage = AERON_SHADER_STAGE_FRAGMENT,
													   .sampler_count = 1,
													   .uniform_buffer_count = 1 });
		AeronColorTargetStateDesc color = { .format = AERON_TEXTURE_FORMAT_RGBA8_SRGB };
		g_copy = Aeron_CreateGraphicsPipeline(
			&(AeronGraphicsPipelineDesc) { .vertex_shader = g_vs,
										   .fragment_shader = g_fs,
										   .primitive_type = AERON_PRIMITIVE_TRIANGLE_STRIP,
										   .cull_mode = AERON_CULL_NONE,
										   .color_target_count = 1,
										   .color_targets = &color });
		g_nearest = Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_NEAREST,
															  .mag_filter = AERON_FILTER_NEAREST,
															  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
															  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	}
	if (!g_copy || !g_nearest || !dst || !src || sw <= 0 || sh <= 0 || to->width <= 0 || to->height <= 0)
		return 0;
	AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.command_buffer = cmd, .color_target = dst, .debug_label = "XvT retained surface copy" });
	if (!pass)
		return 0;
	float uniform[4] = { (float)from->x / sw, (float)from->y / sh, (float)from->width / sw,
						 (float)from->height / sh };
	Aeron_SetViewport(pass, &(AeronRectI) { to->x, to->y, to->width, to->height });
	Aeron_BindGraphicsPipeline(pass, g_copy);
	Aeron_BindTextureSampler(pass, AERON_SHADER_STAGE_FRAGMENT, 0, src, g_nearest);
	Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, uniform, sizeof uniform);
	Aeron_Draw(pass, 4, 0);
	Aeron_EndRenderPass(pass);
	return 1;
}

void XvtUi_Shutdown(void) {
	Aeron_DestroyGraphicsPipeline(g_copy);
	Aeron_DestroyShader(g_vs);
	Aeron_DestroyShader(g_fs);
	Aeron_DestroySampler(g_nearest);
	g_copy = NULL;
	g_vs = g_fs = NULL;
	g_nearest = NULL;
}
