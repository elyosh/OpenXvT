#include "xvt_remaster/hud_draw.h"
#include "xvt_remaster/ui_draw.h"
#include <math.h>

AeronDrawList2D* XvtHudDraw_SelectList(const XvtHudDraw* draw, unsigned phase) {
	return phase <= XVT_COCKPIT_BEFORE_CRT ? draw->before : draw->after;
}

static XvtSnapRect IntersectRects(XvtSnapRect a, XvtSnapRect b) {
	int right = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
	int bottom = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;
	a.x = a.x > b.x ? a.x : b.x;
	a.y = a.y > b.y ? a.y : b.y;
	a.width = right > a.x ? right - a.x : 0;
	a.height = bottom > a.y ? bottom - a.y : 0;
	return a;
}

int XvtHudDraw_Clip(const XvtHudDraw* draw, XvtSnapRect rect, AeronRectI* out) {
	if (rect.width <= 0 || rect.height <= 0)
		return 0;
	int left = (int)floorf(rect.x * draw->scale + draw->offset_x);
	int top = (int)floorf(rect.y * draw->scale + draw->offset_y);
	int right = (int)ceilf((rect.x + rect.width) * draw->scale + draw->offset_x);
	int bottom = (int)ceilf((rect.y + rect.height) * draw->scale + draw->offset_y);
	left = left > 0 ? left : 0;
	top = top > 0 ? top : 0;
	right = right < draw->width ? right : draw->width;
	bottom = bottom < draw->height ? bottom : draw->height;
	*out = (AeronRectI) { left, top, right - left, bottom - top };
	return right > left && bottom > top;
}

void XvtHudDraw_Fill(const XvtHudDraw* draw, unsigned phase, XvtSnapRect rect, uint32_t color) {
	AeronRectI clip;
	if (!(color >> 24) || !XvtHudDraw_Clip(draw, rect, &clip))
		return;
	float rgba[4];
	XvtUi_Color(color, rgba);
	AeronDrawList_AddFill(XvtHudDraw_SelectList(draw, phase), rect.x * draw->scale + draw->offset_x,
						  rect.y * draw->scale + draw->offset_y, rect.width * draw->scale,
						  rect.height * draw->scale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
}

void XvtHudDraw_TextFill(const XvtHudDraw* draw, const XvtFontAtlas* font, unsigned phase, XvtSnapRect rect,
						 uint32_t color) {
	AeronRectI clip;
	if (!(color >> 24) || !XvtHudDraw_Clip(draw, rect, &clip))
		return;
	AeronDrawList2DSprite sprite = { .texture = font->atlas.texture,
									 .src_u0 = font->white_uv[0],
									 .src_v0 = font->white_uv[1],
									 .src_u1 = font->white_uv[0],
									 .src_v1 = font->white_uv[1],
									 .dst_x = rect.x * draw->scale + draw->offset_x,
									 .dst_y = rect.y * draw->scale + draw->offset_y,
									 .dst_w = rect.width * draw->scale,
									 .dst_h = rect.height * draw->scale,
									 .blend = AERON_BLIT2D_BLEND_PMA,
									 .filter = AERON_BLIT2D_FILTER_LINEAR };
	XvtUi_Color(color, sprite.tint);
	AeronDrawList_AddSprite(XvtHudDraw_SelectList(draw, phase), &sprite);
}

void XvtHudDraw_Frame(const XvtHudDraw* draw, unsigned phase, XvtSnapRect rect, uint32_t color) {
	XvtHudDraw_FrameClipped(draw, phase, rect,
							(XvtSnapRect) { 0, 0, draw->layout->source_width, draw->layout->source_height },
							color);
}

void XvtHudDraw_FrameClipped(const XvtHudDraw* draw, unsigned phase, XvtSnapRect rect, XvtSnapRect clip,
							 uint32_t color) {
	if (rect.width <= 0 || rect.height <= 0)
		return;
	XvtSnapRect edges[] = { { rect.x, rect.y, rect.width, 1 },
							{ rect.x, rect.y + rect.height - 1, rect.width, 1 },
							{ rect.x, rect.y, 1, rect.height },
							{ rect.x + rect.width - 1, rect.y, 1, rect.height } };
	for (unsigned edge = 0; edge < 4; ++edge)
		XvtHudDraw_Fill(draw, phase, IntersectRects(edges[edge], clip), color);
}

static void AppendAtlasSprite(const XvtHudDraw* draw, const AeronRuntimeAtlas* atlas, unsigned frame, float x,
							  float y, float sx, float sy, float width, float height, int mirrored,
							  int monochrome, uint32_t color, unsigned phase) {
	const AeronSpriteRect* source = &atlas->layout.frames[frame];
	const AeronRuntimeAtlasPage* page = &atlas->pages[atlas->layout.pages[frame]];
	AeronDrawList2DSprite sprite = { .texture = page->texture,
									 .src_u0 = (source->x + sx) / page->width,
									 .src_v0 = (source->y + sy) / page->height,
									 .src_u1 = (source->x + sx + width) / page->width,
									 .src_v1 = (source->y + sy + height) / page->height,
									 .dst_x = x * draw->scale + draw->offset_x,
									 .dst_y = y * draw->scale + draw->offset_y,
									 .dst_w = width * draw->scale,
									 .dst_h = height * draw->scale,
									 .tint = { 1, 1, 1, 1 },
									 .blend = AERON_BLIT2D_BLEND_PMA,
									 .filter = AERON_BLIT2D_FILTER_LINEAR };
	if (mirrored) {
		float swap = sprite.src_u0;
		sprite.src_u0 = sprite.src_u1;
		sprite.src_u1 = swap;
	}
	if (monochrome) {
		sprite.tint[0] = sprite.tint[1] = sprite.tint[2] = 0;
		XvtUi_Color(color, sprite.bias);
		sprite.bias[3] = 0;
	}
	if (!XvtHudDraw_Clip(draw,
						 (XvtSnapRect) { 0, 0, draw->layout->source_width, draw->layout->source_height },
						 &sprite.scissor))
		return;
	AeronDrawList_AddSprite(XvtHudDraw_SelectList(draw, phase), &sprite);
}

void XvtHudDraw_Part(const XvtHudDraw* draw, XvtHudSpriteRole role, unsigned state, int offset_x,
					 int offset_y, unsigned phase) {
	const XvtHudSpriteBinding* binding = &draw->layout->sprites[role];
	if (state >= binding->part_count)
		return;
	const XvtHudPreparedPart* part = &draw->assets->bindings[binding->first_part + state];
	const AeronRuntimeAtlas* atlas = &draw->assets->parts;
	float width = atlas->layout.size[(part->atlas_frame) * 2],
		  height = atlas->layout.size[(part->atlas_frame) * 2 + 1];
	AppendAtlasSprite(draw, atlas, part->atlas_frame,
					  binding->x + offset_x - (binding->mirrored ? width - 1 : 0), binding->y + offset_y, 0,
					  0, width, height, binding->mirrored, part->monochrome,
					  draw->state->palette_argb[part->color], phase);
}

void XvtHudDraw_Base(const XvtHudDraw* draw) {
	const AeronRuntimeAtlas* atlas = &draw->assets->base;
	if (!atlas->layout.frame_count)
		return;
	for (unsigned index = 0; index < draw->assets->base_coverage.count; ++index) {
		const AeronImageCoverageRect* rect = &draw->assets->base_coverage.rects[index];
		int x = draw->layout->mirrored ? draw->layout->source_width - rect->x - rect->width : rect->x;
		AppendAtlasSprite(draw, atlas, 0, x, rect->y, rect->x, rect->y, rect->width, rect->height,
						  draw->layout->mirrored, 0, 0, XVT_COCKPIT_BEFORE_CRT);
	}
}

static void AppendGlyphPlane(const XvtHudDraw* draw, const XvtFontAtlas* font, unsigned glyph_index, float x,
							 float y, uint32_t color, const AeronRectI* clip, unsigned phase) {
	if (!(color >> 24))
		return;
	const AeronFontGlyph* glyph = &font->atlas.glyphs[glyph_index];
	const XvtFontGlyph* metrics = &font->glyphs[glyph_index];
	AeronDrawList2DSprite sprite = { .texture = font->atlas.texture,
									 .src_u0 = (float)glyph->atlas_x / font->atlas.atlas_w,
									 .src_v0 = (float)glyph->atlas_y / font->atlas.atlas_h,
									 .src_u1 = (float)(glyph->atlas_x + glyph->atlas_w) / font->atlas.atlas_w,
									 .src_v1 = (float)(glyph->atlas_y + glyph->atlas_h) / font->atlas.atlas_h,
									 .dst_x = x * draw->scale + draw->offset_x,
									 .dst_y = y * draw->scale + draw->offset_y,
									 .dst_w = metrics->width * draw->scale,
									 .dst_h = metrics->height * draw->scale,
									 .blend = AERON_BLIT2D_BLEND_PMA,
									 .filter = AERON_BLIT2D_FILTER_LINEAR };
	/* Preserve the integer scissor boundaries in geometry so adjacent glyphs
	 * can share a batch without changing their background/shadow order. */
	float left = fmaxf(sprite.dst_x, clip->x);
	float top = fmaxf(sprite.dst_y, clip->y);
	float right = fminf(sprite.dst_x + sprite.dst_w, clip->x + clip->width);
	float bottom = fminf(sprite.dst_y + sprite.dst_h, clip->y + clip->height);
	if (right <= left || bottom <= top)
		return;
	float du = (sprite.src_u1 - sprite.src_u0) / sprite.dst_w;
	float dv = (sprite.src_v1 - sprite.src_v0) / sprite.dst_h;
	sprite.src_u1 = sprite.src_u0 + (right - sprite.dst_x) * du;
	sprite.src_v1 = sprite.src_v0 + (bottom - sprite.dst_y) * dv;
	sprite.src_u0 += (left - sprite.dst_x) * du;
	sprite.src_v0 += (top - sprite.dst_y) * dv;
	sprite.dst_x = left;
	sprite.dst_y = top;
	sprite.dst_w = right - left;
	sprite.dst_h = bottom - top;
	XvtUi_Color(color, sprite.tint);
	AeronDrawList_AddSprite(XvtHudDraw_SelectList(draw, phase), &sprite);
}

int XvtHudDraw_Glyph(const XvtHudDraw* draw, const XvtCockpitGlyph* glyph, int origin_x, int origin_y,
					 XvtSnapRect pane_clip, unsigned phase) {
	const XvtFontAtlas* font = XvtHudAssets_FindFont(glyph->font_asset_id);
	if (!font || glyph->character < font->atlas.first_char ||
		glyph->character >= font->atlas.first_char + font->atlas.num_chars)
		return 0;
	unsigned glyph_index = glyph->character - font->atlas.first_char;
	int x = glyph->x + origin_x, y = glyph->y + origin_y;
	XvtSnapRect bounds = glyph->clip;
	bounds.x += origin_x;
	bounds.y += origin_y;
	bounds = IntersectRects(bounds, pane_clip);
	bounds = IntersectRects(bounds,
							(XvtSnapRect) { x, y, glyph->advance + !!glyph->shadow_enabled, glyph->height });
	AeronRectI clip;
	if (!XvtHudDraw_Clip(draw, bounds, &clip))
		return 1;
	XvtHudDraw_TextFill(draw, font, phase, bounds, glyph->background_argb);
	if (glyph->shadow_enabled)
		AppendGlyphPlane(draw, font, glyph_index, x + 1, y + 1, glyph->shadow_argb, &clip, phase);
	AppendGlyphPlane(draw, font, glyph_index, x, y, glyph->foreground_argb, &clip, phase);
	return 1;
}
