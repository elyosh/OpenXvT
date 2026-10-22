#include "xvt_runtime/snapshot/render_frontend.h"

#include "xvt/frontend/front_image.h"
#include "xvt/frontend/frontend_mouse.h"
#include "xvt/frontend/frontend_state.h"
#include "xvt_runtime/runtime/dialog_task.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/presentation.h"
#include "xvt_runtime/snapshot/render_assets.h"
#include "xvt_runtime/snapshot/render_capture.h"
#include <stdlib.h>
#include <string.h>

static unsigned g_target, g_suppress;
static uint64_t g_serial, g_generation;
static XvtSceneKind g_presentedScene;
static unsigned g_presentedTarget;
static int g_textEntry;
static int g_textFieldSelected;
static int g_textFieldHit;
static int g_textFieldClick;
static int g_cursorDrawing, g_cursorVisible;
static int g_surfacesReleased;
static XvtSnapSprite g_cursorSprite;

typedef struct FrontendGlyphIdentity {
	uintptr_t pixels;
	uint16_t character, width, height;
} FrontendGlyphIdentity;

static FrontendGlyphIdentity g_fontGlyphs[10][256];
static uint64_t g_fontAssetIds[10];

static int CompareGlyphIdentity(const void* left, const void* right) {
	const FrontendGlyphIdentity* a = left;
	const FrontendGlyphIdentity* b = right;
	if (a->pixels != b->pixels)
		return a->pixels < b->pixels ? -1 : 1;
	return (int)a->character - b->character;
}

void XvtRenderFrontend_FontLoaded(const BitmapFont* font) {
	for (unsigned slot = 0; slot < 10; ++slot) {
		if (font != &g_frontState.fontSlots[slot])
			continue;
		g_fontAssetIds[slot] = XvtRenderAssets_ImageId(font);
		for (unsigned ch = 0; ch < 256; ++ch)
			g_fontGlyphs[slot][ch] =
				(FrontendGlyphIdentity) { (uintptr_t)(font->pGlyphBits + font->glyphBitOffset[ch]), ch,
										  font->glyphWidth[ch], font->glyphHeight[ch] };
		qsort(g_fontGlyphs[slot], 256, sizeof g_fontGlyphs[slot][0], CompareGlyphIdentity);
		return;
	}
}

static int FindFontGlyph(unsigned slot, const ImageResource* glyph) {
	const FrontendGlyphIdentity* entries = g_fontGlyphs[slot];
	uintptr_t pixels = (uintptr_t)glyph->pixels;
	unsigned first = 0, end = 256;
	while (first < end) {
		unsigned middle = first + (end - first) / 2;
		if (entries[middle].pixels < pixels)
			first = middle + 1;
		else
			end = middle;
	}
	for (; first < 256 && entries[first].pixels == pixels; ++first)
		if (entries[first].width == glyph->width && entries[first].height == glyph->height)
			return entries[first].character;
	return -1;
}

void XvtRenderFrontend_BeginDraw(void) {
	g_textEntry = 0;
	g_textFieldHit = 0;
	g_textFieldClick = FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick();
}

void XvtRenderFrontend_TextEntry(int field) {
	if (field == g_activeTextFieldId)
		g_textEntry = 1;
}

void XvtRenderFrontend_TextFieldPressed(void) {
	g_textFieldHit = 1;
	g_textFieldSelected = 1;
}

void XvtRenderFrontend_Init(void) {
	g_target = XVT_TARGET_FRONT_BACK;
	g_suppress = 0;
	g_serial = g_generation = 0;
	g_presentedScene = XVT_SCENE_NONE;
	g_presentedTarget = XVT_TARGET_FRONT_BACK;
	g_textEntry = 0;
	g_textFieldSelected = 0;
	g_textFieldHit = 0;
	g_textFieldClick = 0;
	g_cursorDrawing = g_cursorVisible = 0;
	g_surfacesReleased = 0;
	memset(g_fontGlyphs, 0, sizeof g_fontGlyphs);
	memset(g_fontAssetIds, 0, sizeof g_fontAssetIds);
}

unsigned XvtRenderFrontend_Target(void) { return g_target; }

void XvtRenderFrontend_Select(unsigned target) {
	if (!g_suppress)
		g_target = target;
}

void XvtRenderFrontend_Suppress(int begin) {
	if (begin)
		++g_suppress;
	else if (g_suppress)
		--g_suppress;
}

static XvtRenderSnapshot* Writer(void) { return g_suppress ? NULL : XvtRenderSnapshot_Writer(); }

static uint32_t Color(unsigned c) {
	unsigned r, g, b;
	if (g_frontState.displayBpp == 8) {
		const FrontendPaletteEntry* p = &g_frontState.displayPalette[c & 255];
		return 0xff000000u | ((unsigned)p->red << 16) | ((unsigned)p->green << 8) | p->blue;
	}
	b = c & 31;
	if (g_frontState.pixelFormat555) {
		r = (c >> 10) & 31;
		g = (c >> 5) & 31;
		g = (g << 3) | (g >> 2);
	} else {
		r = (c >> 11) & 31;
		g = (c >> 5) & 63;
		g = (g << 2) | (g >> 4);
	}
	return 0xff000000u | (((r << 3) | (r >> 2)) << 16) | (g << 8) | (b << 3) | (b >> 2);
}

static XvtSnapDrawHeader Header(void) {
	return (XvtSnapDrawHeader) { XvtRenderSnapshot_NextOrder(),
								 g_target,
								 XVT_SCOPE_FRONTEND,
								 { g_frontState.clipMinX, g_frontState.clipMinY,
								   g_frontState.clipMaxX - g_frontState.clipMinX + 1,
								   g_frontState.clipMaxY - g_frontState.clipMinY + 1 } };
}

static unsigned GlyphColor(unsigned color) {
	unsigned cached = g_frontState.glyphScratchBuffer[color & 65535];
	if (cached)
		return cached;
	unsigned total = g_frontState.glyphScratchReload;
	if (!total)
		return color;
	unsigned fade = total - g_frontState.glyphScratchTtl;
	unsigned shift = g_frontState.pixelFormat555 ? 10 : 11;
	unsigned green = g_frontState.pixelFormat555 ? 31 : 63;
	return (((((color >> shift) & 31) * fade / total) & 31) << shift) |
		   (((((color >> 5) & green) * fade / total) & green) << 5) | (((color & 31) * fade / total) & 31);
}

void XvtRenderFrontend_Image(const ImageResource* image, int sx, int sy, int x, int y, int width, int height,
							 unsigned kind, unsigned tint) {
	XvtRenderSnapshot* s = Writer();
	if (!s || !image || width <= 0 || height <= 0)
		return;
	uint64_t id = XvtRenderAssets_ImageId(image);
	if (!id || s->sprite_count == XVT_SNAP_SPRITES) {
		++s->dropped_records;
		return;
	}
	XvtSnapSprite* b = g_cursorDrawing ? &g_cursorSprite : &s->sprites[s->sprite_count++];
	memset(b, 0, sizeof *b);
	b->draw = Header();
	b->asset_id = id;
	b->kind = kind;
	b->source = (XvtSnapRect) { sx, sy, width, height };
	b->destination = (XvtSnapRect) { x, y, width, height };
	b->tint_color = kind == XVT_SPRITE_FRONT_TINTED ? tint : 0;
	if (g_cursorDrawing) {
		b->draw.target = XVT_TARGET_FRONT_CURSOR;
		/* Keep the whole cursor; presentation clips it at the live pointer position. */
		b->source = (XvtSnapRect) { 0, 0, image->width, image->height };
		b->destination = (XvtSnapRect) { x - sx, y - sy, image->width, image->height };
		g_cursorVisible = 1;
	}
}

void XvtRenderFrontend_Glyph(const ImageResource* glyph, int x, int y, unsigned color, int remap) {
	XvtRenderSnapshot* s = Writer();
	if (!s || !glyph)
		return;
	/* All string layouts construct a view into a live ABP font. Resolve it here
	 * after wrapping/clipping, including direct glyph calls, without nested emits. */
	for (unsigned f = 0; f < 10; ++f) {
		const BitmapFont* font = &g_frontState.fontSlots[f];
		if (!font->inUse)
			continue;
		int ch = FindFontGlyph(f, glyph);
		if (ch < 0)
			continue;
		if (s->glyph_count == XVT_SNAP_GLYPHS) {
			++s->dropped_records;
			return;
		}
		if (remap && g_frontState.displayBpp == 16 && g_frontState.glyphScratchTtl)
			color = GlyphColor(color);
		XvtSnapGlyph* g = &s->glyphs[s->glyph_count++];
		memset(g, 0, sizeof *g);
		g->draw = Header();
		g->font_asset_id = g_fontAssetIds[f];
		g->character = ch;
		g->x = x;
		g->y = y;
		g->advance = font->charSpacing + glyph->width;
		g->foreground_argb = Color(color);
		return;
	}
	++s->dropped_records;
}

void XvtRenderFrontend_Paint(unsigned kind, int x0, int y0, int x1, int y1, unsigned color) {
	XvtRenderSnapshot* s = Writer();
	if (!s)
		return;
	if (s->paint_count == XVT_SNAP_PAINT) {
		++s->dropped_records;
		return;
	}
	XvtSnapPaint* p = &s->paint[s->paint_count++];
	*p = (XvtSnapPaint) { Header(), kind, Color(color), x0, y0, x1, y1 };
	if (kind == XVT_PAINT_TRANSLUCENT && g_frontState.displayBpp == 16)
		p->color_argb = (p->color_argb & 0xffffffu) | 0x80000000u;
}

static void CopyRect(unsigned source, unsigned target, XvtSnapRect rect) {
	XvtRenderSnapshot* s = Writer();
	if (!s)
		return;
	if (s->copy_count == XVT_SNAP_COPIES) {
		++s->dropped_records;
		return;
	}
	XvtSnapCopyRect* c = &s->copies[s->copy_count++];
	*c = (XvtSnapCopyRect) {
		.draw = Header(), .source_target = (uint16_t)source, .source = rect, .destination = rect
	};
	c->draw.target = target;
}

void XvtRenderFrontend_Copy(unsigned source, unsigned target) {
	CopyRect(source, target, (XvtSnapRect) { 0, 0, 640, 480 });
	if (!g_suppress && target == XVT_TARGET_FRONT_BACK)
		g_cursorVisible = 0;
}

void XvtRenderFrontend_Screen(int slot, int restore) {
	if ((unsigned)slot >= XVT_TARGET_FRONT_SAVED_COUNT)
		return;
	const RECT* r = &g_frontState.screenStates[slot].savedRect;
	unsigned target =
		g_frontState.offscreenRestoreEnabled ? XVT_TARGET_FRONT_OFFSCREEN : XVT_TARGET_FRONT_BACK;
	unsigned saved = XVT_TARGET_FRONT_SAVED_FIRST + slot;
	CopyRect(restore ? saved : target, restore ? target : saved,
			 (XvtSnapRect) { r->left, r->top, r->right - r->left + 1, r->bottom - r->top + 1 });
}

static void Event(unsigned kind, unsigned target, uint32_t color) {
	XvtRenderSnapshot* s = Writer();
	if (!s)
		return;
	if (s->surface_event_count == XVT_SNAP_SURFACE_EVENTS) {
		++s->dropped_records;
		return;
	}
	s->surface_events[s->surface_event_count++] = (XvtSnapSurfaceEvent) {
		XvtRenderSnapshot_NextOrder(), kind, target, 0, { 0, 0, 640, 480 }, color, 0
	};
}

void XvtRenderFrontend_Clear(unsigned target, unsigned color) {
	Event(XVT_SURFACE_CLEAR, target, Color(color));
	if (!g_suppress && target == XVT_TARGET_FRONT_BACK)
		g_cursorVisible = 0;
}

void XvtRenderFrontend_PresentedScene(XvtSceneKind kind) {
	++g_serial;
	g_presentedScene = kind;
	g_presentedTarget = kind == XVT_SCENE_FLIGHT  ? XVT_TARGET_FLIGHT_MAIN
						: kind == XVT_SCENE_MOVIE ? XVT_TARGET_FRONT_MOVIE
												  : XVT_TARGET_FRONT_BACK;
}

void XvtRenderFrontend_FlightUiScene(XvtSceneKind kind) {
	XvtRenderFrontend_PresentedScene(kind);
	g_presentedTarget = XVT_TARGET_FLIGHT_MAIN;
}

void XvtRenderFrontend_Present(void) {
	XvtRenderSnapshot* s = Writer();
	if (!s)
		return;
	/* Cursor drawing belongs to this presentation, not the persistent background. */
	s->cursor.visible = 0;
	if (g_cursorVisible) {
		if (s->sprite_count == XVT_SNAP_SPRITES) {
			++s->dropped_records;
			return;
		}
		XvtSnapSprite cursor = g_cursorSprite;
		cursor.draw.z_order = XvtRenderSnapshot_NextOrder();
		s->sprites[s->sprite_count++] = cursor;
		s->cursor =
			(XvtSnapCursor) { cursor.asset_id,          cursor.destination.x,      cursor.destination.y,
							  cursor.destination.width, cursor.destination.height, 1 };
	}
	Event(XVT_SURFACE_PRESENT, XVT_TARGET_FRONT_BACK, 0);
	XvtRenderFrontend_PresentedScene(XvtDialog_IsActive()        ? XVT_SCENE_FRONTEND_MODAL
									 : XvtFlightTask_IsLoading() ? XVT_SCENE_LOADING
																 : XVT_SCENE_FRONTEND);
}

void XvtRenderFrontend_Reset(void) {
	XvtPresentation_RequireClassic();
	g_surfacesReleased = 0;
	++g_generation;
	g_cursorVisible = g_cursorDrawing = 0;
	Event(XVT_SURFACE_RESET, XVT_TARGET_FRONT_BACK, 0xff000000u);
	g_target = XVT_TARGET_FRONT_BACK;
}

void XvtRenderFrontend_ReleaseSurfaces(void) { g_surfacesReleased = 1; }

void XvtRenderFrontend_Cursor(int restore) {
	if (!Writer())
		return;
	g_cursorVisible = 0;
	if (restore)
		return;
	g_cursorDrawing = 1;
	if (g_frontState.cursorSpriteName[0]) {
		/* Capture even when the classic blitter rejects a fully clipped cursor. */
		int index = FrontImage_FindResourceByName(g_frontState.cursorSpriteName);
		if (index >= 0) {
			const ImageResource* image = g_frontState.resourceTable[index].image;
			if (image)
				XvtRenderFrontend_Image(image, 0, 0, g_frontState.mouseX, g_frontState.mouseY, image->width,
										image->height, XVT_SPRITE_FRONT_KEYED, 0);
		}
		return;
	}
	memset(&g_cursorSprite, 0, sizeof g_cursorSprite);
	g_cursorSprite.draw = Header();
	g_cursorSprite.draw.target = XVT_TARGET_FRONT_CURSOR;
	g_cursorSprite.asset_id = XvtRenderAssets_ImageId(XvtRenderAssets_DefaultCursor());
	g_cursorSprite.kind = XVT_SPRITE_FRONT_KEYED;
	g_cursorSprite.source = (XvtSnapRect) { 0, 0, g_frontState.cursorWidth, g_frontState.cursorHeight };
	g_cursorSprite.destination = (XvtSnapRect) { g_frontState.mouseX, g_frontState.mouseY,
												 g_frontState.cursorWidth, g_frontState.cursorHeight };
	g_cursorVisible = 1;
}

void XvtRenderFrontend_EndCursor(void) { g_cursorDrawing = 0; }

void XvtRenderFrontend_Movie(int begin) {
	if (begin) {
		g_target = XVT_TARGET_FRONT_MOVIE;
		Event(XVT_SURFACE_CLEAR, g_target, 0);
	} else {
		Event(XVT_SURFACE_PRESENT, XVT_TARGET_FRONT_MOVIE, 0);
		XvtRenderFrontend_PresentedScene(XVT_SCENE_MOVIE);
		g_target = XVT_TARGET_FRONT_BACK;
	}
}

void XvtRenderFrontend_Commit(XvtRenderSnapshot* s) {
	s->presentation_serial = g_serial;
	s->presented_scene = g_presentedScene;
	s->frontend_generation = g_generation;
	s->frontend_surfaces_released = g_surfacesReleased;
	s->presented_target = g_presentedTarget;
	/* Field id 0 is both "nothing selected" and the roster name box, so id
	 * equality is not a tap. A click that misses every box clears the tap. */
	if (g_textFieldClick && !g_textFieldHit)
		g_textFieldSelected = 0;
	g_textFieldClick = 0;
	s->text_entry_active =
		(s->scene_kind == XVT_SCENE_FRONTEND || s->scene_kind == XVT_SCENE_FRONTEND_MODAL) && g_textEntry;
	s->text_field_selected = s->text_entry_active && g_textFieldSelected;
}
