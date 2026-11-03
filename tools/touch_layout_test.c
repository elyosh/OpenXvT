/* Standalone host test, not part of the CMake build. Run from the repository root:
 *   cc -std=c11 -Wall -Wextra -Werror -Isrc tools/touch_layout_test.c
 *      src/xvt_runtime/input/touch_layout.c -lm -o build/touch_layout_test
 *   ./build/touch_layout_test
 */
#include "xvt_runtime/input/touch_layout.h"
#include "xvt_runtime/input/touch_tuning.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
	CTL_LEFT = 0,
	CTL_RIGHT,
	CTL_FIRE,
	CTL_FRONT,
	CTL_NEXT,
	CTL_ATTACKER,
	CTL_WEAPONS,
	CTL_RATE,
	CTL_SHIELDS,
	CTL_WEAPON_ENERGY,
	CTL_SHIELD_ENERGY,
	CTL_MAP,
	CTL_THROTTLE_1,
	CTL_THROTTLE_2,
	CTL_THROTTLE_FULL,
	CTL_ESC,
	CTL_EXTRA,
	CTL_COUNT = CTL_EXTRA + 20
};

typedef struct OracleControl {
	float x, y, w, h;
	float cx, cy, radius;
	int visible;
} OracleControl;

typedef struct Pattern {
	const char* name;
	int esc;
	int sticks;
	int fire;
	int throttle[3];
	int top[5];
	int low[4];
	int extras;
} Pattern;

typedef struct Built {
	XvtTouchLayoutPlan plan;
	int throttle[3];
	int top[5];
	int low[4];
	int extras[20];
} Built;

typedef struct Origin {
	float x, y;
} Origin;

static const int k_top_ids[] = { CTL_FRONT, CTL_NEXT, CTL_ATTACKER, CTL_WEAPONS, CTL_RATE };
static const int k_low_ids[] = { CTL_SHIELDS, CTL_WEAPON_ENERGY, CTL_SHIELD_ENERGY, CTL_MAP };
static const int k_throttle_ids[] = { CTL_THROTTLE_1, CTL_THROTTLE_2, CTL_THROTTLE_FULL };
static const float k_widths[] = { 320.0f, 480.0f, 640.0f, 844.0f, 932.0f, 1024.0f,
								  1194.0f, 1366.0f, 1600.0f, 2048.0f, 2556.0f };
static const float k_heights[] = { 320.0f, 390.0f, 430.0f, 600.0f, 768.0f, 844.0f, 1179.0f, 1366.0f,
								   2556.0f };
/* 40 sits under the app's 96 floor, so the margin and gap floors are compared too. */
static const float k_radii[] = { 40.0f, 96.0f, 120.0f, 150.0f, 170.0f };
static const Origin k_origins[] = { { 0.0f, 0.0f }, { 48.0f, 20.0f } };
static const Pattern k_patterns[] = {
	{ "all", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 0 },
	{ "no_fire", 1, 1, 0, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 0 },
	{ "no_throttle", 1, 1, 1, { 0, 0, 0 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 0 },
	{ "sticks_hidden", 1, 0, 0, { 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0 },
	{ "no_esc", 0, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 0 },
	{ "extras_1", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 1 },
	{ "extras_6", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 6 },
	{ "extras_7", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 7 },
	{ "extras_13", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 13 },
	{ "extras_20", 1, 1, 1, { 1, 1, 1 }, { 1, 1, 1, 1, 1 }, { 1, 1, 1, 1 }, 20 },
	{ "mixed", 1, 1, 1, { 1, 0, 1 }, { 1, 0, 0, 1, 0 }, { 0, 1, 0, 1 }, 3 }
};

static int g_failed;
static int g_shown;
static unsigned g_rng = 1u;

static void Failf(const char* fmt, ...) {
	va_list args;
	if (g_shown >= 12) {
		g_failed = 1;

		return;
	}
	fprintf(stderr, "FAIL ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	++g_shown;
	g_failed = 1;
}

static void Expect(int ok, const char* name) {
	if (ok)

		return;
	Failf("%s", name);
}

static int Near(float value, float expected) {
	return fabsf(value - expected) <= 1e-3f;
}

static unsigned Rand(void) {
	g_rng = g_rng * 1664525u + 1013904223u;

	return g_rng;
}

static float Sanitize(float scale) {
	if (!isfinite(scale))

		return (float)XVT_TOUCH_SCALE_DEFAULT;
	if (scale < (float)XVT_TOUCH_SCALE_MIN)

		return (float)XVT_TOUCH_SCALE_MIN;
	if (scale > (float)XVT_TOUCH_SCALE_MAX)

		return (float)XVT_TOUCH_SCALE_MAX;

	return scale;
}

static float MarginOf(float radius) {
	float margin = radius * 0.2f;
	if (margin < 16.0f)
		margin = 16.0f;

	return margin;
}

static float GapOf(float radius) {
	float gap = radius * 0.14f;
	if (gap < 8.0f)
		gap = 8.0f;

	return gap;
}

static int Listed(const int* ids, int count, int index) {
	int i;
	for (i = 0; i < count; ++i)
		if (ids[i] == index)

			return 1;

	return 0;
}

static int RectHits(const XvtTouchLayoutRect* a, const XvtTouchLayoutRect* b) {
	return a->x < b->x + b->w - 0.5f && b->x < a->x + a->w - 0.5f && a->y < b->y + b->h - 0.5f &&
		   b->y < a->y + a->h - 0.5f;
}

static int FindHit(const XvtTouchLayoutRect* rects, const float* scales, int scaled_only, int* ia,
				   int* ib) {
	int i;
	int j;
	for (i = 0; i < CTL_COUNT; ++i) {
		if (!rects[i].placed)
			continue;
		for (j = i + 1; j < CTL_COUNT; ++j) {
			float si;
			float sj;
			if (!rects[j].placed)
				continue;
			si = Sanitize(scales[i]);
			sj = Sanitize(scales[j]);
			if (scaled_only && si == 1.0f && sj == 1.0f)
				continue;
			if (!RectHits(&rects[i], &rects[j]))
				continue;
			if (ia)
				*ia = i;
			if (ib)
				*ib = j;

			return 1;
		}
	}

	return 0;
}

static int Cleared(const XvtTouchLayoutRect* rects) {
	int i;
	for (i = 0; i < CTL_COUNT; ++i)
		if (rects[i].placed || rects[i].x != 0.0f || rects[i].y != 0.0f || rects[i].w != 0.0f ||
			rects[i].h != 0.0f)

			return 0;

	return 1;
}

static int SameRects(const XvtTouchLayoutRect* a, const XvtTouchLayoutRect* b) {
	int i;
	for (i = 0; i < CTL_COUNT; ++i) {
		if (a[i].placed != b[i].placed)
			return 0;
		if (!Near(a[i].x, b[i].x) || !Near(a[i].y, b[i].y) || !Near(a[i].w, b[i].w) ||
			!Near(a[i].h, b[i].h))
			return 0;
	}

	return 1;
}

static void Dirty(XvtTouchLayoutRect* rects) {
	int i;
	for (i = 0; i < CTL_COUNT; ++i) {
		rects[i].x = 5.0f;
		rects[i].y = 6.0f;
		rects[i].w = 7.0f;
		rects[i].h = 8.0f;
		rects[i].placed = 1;
	}
}

static void FillScales(float* scales, float value) {
	int i;
	for (i = 0; i < CTL_COUNT; ++i)
		scales[i] = value;
}

static int Collect(int* out, const int* ids, const int* on, int count) {
	int n = 0;
	int i;
	for (i = 0; i < count; ++i)
		if (on[i])
			out[n++] = ids[i];

	return n;
}

static void Build(const Pattern* pattern, Built* built) {
	int i;
	int n = 0;
	built->plan.esc = pattern->esc ? CTL_ESC : -1;
	built->plan.left = pattern->sticks ? CTL_LEFT : -1;
	built->plan.right = pattern->sticks ? CTL_RIGHT : -1;
	built->plan.fire = pattern->sticks && pattern->fire ? CTL_FIRE : -1;
	built->plan.throttle = built->throttle;
	built->plan.throttle_count = Collect(built->throttle, k_throttle_ids, pattern->throttle, 3);
	built->plan.row_top = built->top;
	built->plan.row_top_count = Collect(built->top, k_top_ids, pattern->top, 5);
	built->plan.row_low = built->low;
	built->plan.row_low_count = Collect(built->low, k_low_ids, pattern->low, 4);
	built->plan.extras = built->extras;
	if (pattern->extras > 0) {
		n = pattern->extras;
		if (n > 20)
			n = 20;
		for (i = 0; i < n; ++i)
			built->extras[i] = CTL_EXTRA + i;
	}
	built->plan.extras_count = n;
}

static int PlaceBuilt(const Built* built, float left, float top, float right, float bottom, float radius,
					  const float* scales, XvtTouchLayoutRect* rects) {
	XvtTouchLayoutArea area;
	area.left = left;
	area.top = top;
	area.right = right;
	area.bottom = bottom;

	return XvtTouchLayout_Place(&area, radius, &built->plan, scales, rects, CTL_COUNT);
}

static void OraclePut(OracleControl* controls, int index, float x, float y, float w, float h, int visible) {
	OracleControl* control = &controls[index];
	control->x = x;
	control->y = y;
	control->w = w;
	control->h = h;
	control->cx = x + w * 0.5f;
	control->cy = y + h * 0.5f;
	control->radius = w < h ? w * 0.5f : h * 0.5f;
	control->visible = visible;
}

static void OracleRow(OracleControl* controls, const int* ids, int count, float right, float y, float bw,
					  float bh, float gap) {
	float x;
	int i;
	if (count <= 0)

		return;
	x = right - (bw * (float)count + gap * (float)(count - 1));
	for (i = 0; i < count; ++i)
		OraclePut(controls, ids[i], x + (bw + gap) * (float)i, y, bw, bh, 1);
}

static void OracleSpan(OracleControl* controls, const int* ids, int count, float left, float right, float y,
					   float bh, float gap) {
	float width = right - left;
	float bw;
	float x;
	float max_bw = bh * 2.4f;
	int i;
	if (count <= 0 || width < 1.0f)

		return;
	bw = (width - gap * (float)(count - 1)) / (float)count;
	if (bw < 1.0f)
		bw = 1.0f;
	if (bw > max_bw)
		bw = max_bw;
	x = left + (width - (bw * (float)count + gap * (float)(count - 1))) * 0.5f;
	for (i = 0; i < count; ++i)
		OraclePut(controls, ids[i], x + (bw + gap) * (float)i, y, bw, bh, 1);
}

static void OracleExtras(OracleControl* controls, const int* ids, int count, float right, float bw, float bh,
						 float gap) {
	float anchor = controls[CTL_RIGHT].y;
	float y;
	int placed;
	int control;
	for (control = 0; control < CTL_COUNT; ++control) {
		if (!controls[control].visible || control == CTL_LEFT || control == CTL_RIGHT || control == CTL_ESC)
			continue;
		if (controls[control].y < anchor)
			anchor = controls[control].y;
	}
	y = anchor - gap - bh;
	for (placed = 0; placed < count;) {
		int row = count - placed;
		if (row > 6)
			row = 6;
		OracleRow(controls, ids + placed, row, right, y, bw, bh, gap);
		placed += row;
		y -= bh + gap;
	}
}

static void OraclePlace(OracleControl* controls, float left, float top, float right, float bottom,
						float radius, int show_esc, int show_sticks, int fire_present, const int* throttle,
						int throttle_count, const int* row_top, int top_count, const int* row_low,
						int low_count, const int* extras, int extras_count) {
	float margin = radius * 0.2f;
	float esc_w;
	float esc_h;
	float stick;
	float gap;
	float bw;
	float bh;
	float fire;
	float width;
	float height;
	float row_right;
	int i;
	if (margin < 16.0f)
		margin = 16.0f;
	esc_w = radius * 1.85f;
	esc_h = radius * 0.82f;
	memset(controls, 0, sizeof(OracleControl) * (size_t)CTL_COUNT);
	if (show_esc)
		OraclePut(controls, CTL_ESC, left + margin, top + margin, esc_w, esc_h, 1);
	if (!show_sticks)

		return;
	stick = radius * 2.0f;
	gap = radius * 0.14f;
	if (gap < 8.0f)
		gap = 8.0f;
	bw = radius * 1.2f;
	bh = radius * 0.74f;
	fire = radius * 1.35f;
	width = right - left;
	height = bottom - top;
	OraclePut(controls, CTL_LEFT, left + margin, bottom - margin - stick, stick, stick, 1);
	OraclePut(controls, CTL_RIGHT, right - margin - stick, bottom - margin - stick, stick, stick, 1);
	for (i = 0; i < throttle_count; ++i)
		OraclePut(controls, throttle[i], controls[CTL_LEFT].x + (bw + gap) * (float)i,
				  controls[CTL_LEFT].y - gap - bh, bw, bh, 1);
	if (fire_present)
		OraclePut(controls, CTL_FIRE, controls[CTL_RIGHT].cx - fire * 0.5f,
				  controls[CTL_RIGHT].y - gap - fire, fire, fire, 1);
	row_right = controls[CTL_RIGHT].x + controls[CTL_RIGHT].w;
	if (width > height * 1.35f) {
		float inner_left = controls[CTL_LEFT].x + controls[CTL_LEFT].w + gap;
		float inner_right = controls[CTL_RIGHT].x - gap;
		float occupied = controls[CTL_RIGHT].y;
		float top_limit = top + margin;
		float y0;
		float y1;
		if (controls[CTL_FIRE].visible) {
			inner_right = controls[CTL_FIRE].x - gap;
			occupied = controls[CTL_FIRE].y;
		}
		for (i = 0; i < throttle_count; ++i) {
			float edge;
			int index = throttle[i];
			if (!controls[index].visible)
				continue;
			edge = controls[index].x + controls[index].w + gap;
			if (edge > inner_left)
				inner_left = edge;
			if (controls[index].y < occupied)
				occupied = controls[index].y;
		}
		if (show_esc && controls[CTL_ESC].visible)
			top_limit = controls[CTL_ESC].y + controls[CTL_ESC].h + gap;
		y1 = occupied - gap - bh;
		y0 = y1 - gap - bh;
		if (y0 < top_limit) {
			y0 = top_limit;
			y1 = y0 + bh + gap;
		}
		if (inner_right > inner_left + bh * 4.0f) {
			OracleSpan(controls, row_top, top_count, inner_left, inner_right, y0, bh, gap);
			OracleSpan(controls, row_low, low_count, inner_left, inner_right, y1, bh, gap);
		} else {
			OracleRow(controls, row_low, low_count, row_right, y1, bw, bh, gap);
			OracleRow(controls, row_top, top_count, row_right, y0, bw, bh, gap);
		}
	} else {
		float anchor_y = controls[CTL_FIRE].visible ? controls[CTL_FIRE].y : controls[CTL_RIGHT].y;
		float y_low = anchor_y - gap - bh;
		OracleRow(controls, row_low, low_count, row_right, y_low, bw, bh, gap);
		OracleRow(controls, row_top, top_count, row_right, y_low - gap - bh, bw, bh, gap);
	}
	OracleExtras(controls, extras, extras_count, row_right, bw, bh, gap);
}

static int RectMatch(const OracleControl* oracle, const XvtTouchLayoutRect* rect) {
	if ((oracle->visible != 0) != (rect->placed != 0))

		return 0;
	if (!rect->placed)
		return oracle->x == 0.0f && oracle->y == 0.0f && oracle->w == 0.0f && oracle->h == 0.0f &&
			   rect->x == 0.0f && rect->y == 0.0f && rect->w == 0.0f && rect->h == 0.0f;

	return Near(oracle->x, rect->x) && Near(oracle->y, rect->y) && Near(oracle->w, rect->w) &&
		   Near(oracle->h, rect->h);
}

static int CountOf(size_t bytes, size_t one) {
	return (int)(bytes / one);
}

static int GateParity(void) {
	int compared = 0;
	int p;
	int w;
	int h;
	int r;
	int o;
	int patterns = CountOf(sizeof k_patterns, sizeof k_patterns[0]);
	int widths = CountOf(sizeof k_widths, sizeof k_widths[0]);
	int heights = CountOf(sizeof k_heights, sizeof k_heights[0]);
	int radii = CountOf(sizeof k_radii, sizeof k_radii[0]);
	int origins = CountOf(sizeof k_origins, sizeof k_origins[0]);
	for (p = 0; p < patterns; ++p) {
		for (w = 0; w < widths; ++w) {
			for (h = 0; h < heights; ++h) {
				for (r = 0; r < radii; ++r) {
					for (o = 0; o < origins; ++o) {
						Built built;
						float scales[CTL_COUNT];
						XvtTouchLayoutRect rects[CTL_COUNT];
						OracleControl oracle[CTL_COUNT];
						float left = k_origins[o].x;
						float top = k_origins[o].y;
						float right = left + k_widths[w];
						float bottom = top + k_heights[h];
						int rc;
						int i;
						Build(&k_patterns[p], &built);
						FillScales(scales, 1.0f);
						Dirty(rects);
						rc = PlaceBuilt(&built, left, top, right, bottom, k_radii[r], scales, rects);
						OraclePlace(oracle, left, top, right, bottom, k_radii[r], built.plan.esc >= 0,
									built.plan.left >= 0, built.plan.fire >= 0, built.plan.throttle,
									built.plan.throttle_count, built.plan.row_top, built.plan.row_top_count,
									built.plan.row_low, built.plan.row_low_count, built.plan.extras,
									built.plan.extras_count);
						++compared;
						if (rc != 1) {
							Failf("parity rc %s %.0fx%.0f r=%.0f origin %.0f,%.0f rc %d", k_patterns[p].name,
								  k_widths[w], k_heights[h], k_radii[r], left, top, rc);
							continue;
						}
						for (i = 0; i < CTL_COUNT; ++i) {
							if (RectMatch(&oracle[i], &rects[i]))
								continue;
							Failf("parity %s %.0fx%.0f r=%.0f at %.0f,%.0f idx %d "
								  "exp %.3f %.3f %.3f %.3f vis %d got %.3f %.3f %.3f %.3f placed %d",
								  k_patterns[p].name, k_widths[w], k_heights[h], k_radii[r], left, top, i,
								  oracle[i].x, oracle[i].y, oracle[i].w, oracle[i].h, oracle[i].visible,
								  rects[i].x, rects[i].y, rects[i].w, rects[i].h, rects[i].placed);
							break;
						}
					}
				}
			}
		}
	}

	return compared;
}

static float EqualWidth(float width, int count, float gap, float bh) {
	float bw_eq;
	if (count <= 0)

		return 0.0f;
	bw_eq = (width - gap * (float)(count - 1)) / (float)count;
	if (bw_eq < 1.0f)
		bw_eq = 1.0f;
	if (bw_eq > bh * 2.4f)
		bw_eq = bh * 2.4f;

	return bw_eq;
}

static void RowMetrics(const Built* built, const float* scales, float left, float right, float top,
					   float bottom, float radius, int* spanned, float* eq_top, float* eq_low) {
	float margin = MarginOf(radius);
	float gap = GapOf(radius);
	float width = right - left;
	float height = bottom - top;
	float bh = radius * 0.74f;
	float bw = radius * 1.2f;
	float stick = radius * 2.0f;
	float left_w = stick * Sanitize(scales[built->plan.left]);
	float right_w = stick * Sanitize(scales[built->plan.right]);
	float left_x = left + margin;
	float right_x = right - margin - right_w;
	float inner_left = left_x + left_w + gap;
	float inner_right = right_x - gap;
	float x = left_x;
	int i;
	*spanned = 0;
	*eq_top = bw;
	*eq_low = bw;
	if (!(width > height * 1.35f))

		return;
	if (built->plan.fire >= 0) {
		float fire_w = radius * 1.35f * Sanitize(scales[built->plan.fire]);
		float fire_x = right_x + right_w * 0.5f - fire_w * 0.5f;
		inner_right = fire_x - gap;
	}
	for (i = 0; i < built->plan.throttle_count; ++i) {
		float button = bw * Sanitize(scales[built->plan.throttle[i]]);
		float edge = x + button + gap;
		if (edge > inner_left)
			inner_left = edge;
		x += button + gap;
	}
	if (!(inner_right > inner_left + bh * 4.0f))

		return;
	*spanned = 1;
	*eq_top = EqualWidth(inner_right - inner_left, built->plan.row_top_count, gap, bh);
	*eq_low = EqualWidth(inner_right - inner_left, built->plan.row_low_count, gap, bh);
}

static int ShouldPlace(const Built* built, int index) {
	if (built->plan.left < 0)

		return index == built->plan.esc && index >= 0;
	if (index == built->plan.esc || index == built->plan.left || index == built->plan.right ||
		index == built->plan.fire)

		return index >= 0;

	return Listed(built->plan.throttle, built->plan.throttle_count, index) ||
		   Listed(built->plan.row_top, built->plan.row_top_count, index) ||
		   Listed(built->plan.row_low, built->plan.row_low_count, index) ||
		   Listed(built->plan.extras, built->plan.extras_count, index);
}

static void ExpectSizes(const char* name, const Built* built, float left, float top, float right,
						float bottom, float radius, const float* scales) {
	XvtTouchLayoutRect rects[CTL_COUNT];
	int spanned = 0;
	float eq_top = 0.0f;
	float eq_low = 0.0f;
	int i;
	Dirty(rects);
	PlaceBuilt(built, left, top, right, bottom, radius, scales, rects);
	if (built->plan.left >= 0)
		RowMetrics(built, scales, left, right, top, bottom, radius, &spanned, &eq_top, &eq_low);
	for (i = 0; i < CTL_COUNT; ++i) {
		float scale = Sanitize(scales[i]);
		float expect_w = 0.0f;
		float expect_h = 0.0f;
		if (!ShouldPlace(built, i)) {
			if (rects[i].placed)
				Failf("size %s idx %d placed while absent", name, i);
			continue;
		}
		if (!rects[i].placed) {
			Failf("size %s idx %d missing", name, i);
			continue;
		}
		if (i == built->plan.esc) {
			expect_w = radius * 1.85f * scale;
			expect_h = radius * 0.82f * scale;
		} else if (i == built->plan.left || i == built->plan.right) {
			expect_w = radius * 2.0f * scale;
			expect_h = expect_w;
		} else if (i == built->plan.fire) {
			expect_w = radius * 1.35f * scale;
			expect_h = expect_w;
		} else if (Listed(built->plan.row_top, built->plan.row_top_count, i)) {
			expect_w = eq_top * scale;
			expect_h = radius * 0.74f * scale;
		} else if (Listed(built->plan.row_low, built->plan.row_low_count, i)) {
			expect_w = eq_low * scale;
			expect_h = radius * 0.74f * scale;
		} else {
			expect_w = radius * 1.2f * scale;
			expect_h = radius * 0.74f * scale;
		}
		if (!Near(rects[i].w, expect_w) || !Near(rects[i].h, expect_h))
			Failf("size %s idx %d w %.4f h %.4f expect %.4f %.4f", name, i, rects[i].w, rects[i].h,
				  expect_w, expect_h);
	}
}

static void GateSizes(void) {
	Pattern pattern = k_patterns[0];
	Built built;
	float scales[CTL_COUNT];
	float fixed[4];
	int i;
	int area;
	pattern.extras = 4;
	Build(&pattern, &built);
	fixed[0] = 0.6f;
	fixed[1] = 1.0f;
	fixed[2] = 1.25f;
	fixed[3] = 1.6f;
	for (i = 0; i < 4; ++i) {
		FillScales(scales, fixed[i]);
		ExpectSizes("portrait uniform", &built, 0.0f, 0.0f, 1179.0f, 2556.0f, 96.0f, scales);
		ExpectSizes("portrait uniform 150", &built, 0.0f, 0.0f, 390.0f, 844.0f, 150.0f, scales);
		ExpectSizes("landscape uniform", &built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales);
	}
	FillScales(scales, 1.0f);
	scales[CTL_ESC] = 0.8f;
	scales[CTL_LEFT] = 0.7f;
	scales[CTL_RIGHT] = 1.4f;
	scales[CTL_FIRE] = 1.25f;
	scales[CTL_THROTTLE_1] = 0.6f;
	scales[CTL_THROTTLE_2] = 1.0f;
	scales[CTL_THROTTLE_FULL] = 1.6f;
	scales[CTL_FRONT] = 0.6f;
	scales[CTL_NEXT] = 0.8f;
	scales[CTL_ATTACKER] = 1.0f;
	scales[CTL_WEAPONS] = 1.2f;
	scales[CTL_RATE] = 1.6f;
	scales[CTL_SHIELDS] = 0.7f;
	scales[CTL_WEAPON_ENERGY] = 1.1f;
	scales[CTL_SHIELD_ENERGY] = 1.4f;
	scales[CTL_MAP] = 1.5f;
	for (i = 0; i < 4; ++i)
		scales[CTL_EXTRA + i] = 0.6f + 0.3f * (float)i;
	ExpectSizes("portrait mixed", &built, 0.0f, 0.0f, 1179.0f, 2556.0f, 96.0f, scales);
	ExpectSizes("landscape mixed", &built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales);
	g_rng = 0x4F1A7u;
	for (area = 0; area < 2; ++area) {
		float right = area == 0 ? 1179.0f : 2556.0f;
		float bottom = area == 0 ? 2556.0f : 1179.0f;
		for (i = 0; i < CTL_COUNT; ++i)
			scales[i] = 0.6f + (float)(Rand() % 10001u) / 10000.0f;
		ExpectSizes(area == 0 ? "portrait random" : "landscape random", &built, 0.0f, 0.0f, right, bottom,
					120.0f, scales);
	}
}

static void RandomScales(float* scales) {
	int i;
	for (i = 0; i < CTL_COUNT; ++i) {
		if ((Rand() % 5u) == 0u)
			scales[i] = 1.0f;
		else
			scales[i] = 0.6f + (float)(Rand() % 10001u) / 10000.0f;
	}
}

static void RandomBuilt(Built* built) {
	Pattern pattern;
	int i;
	memset(&pattern, 0, sizeof pattern);
	pattern.name = "random";
	pattern.esc = (int)(Rand() % 2u);
	pattern.sticks = Rand() % 5u != 0u;
	pattern.fire = Rand() % 4u != 0u;
	for (i = 0; i < 3; ++i)
		pattern.throttle[i] = (int)(Rand() % 2u);
	for (i = 0; i < 5; ++i)
		pattern.top[i] = (int)(Rand() % 2u);
	for (i = 0; i < 4; ++i)
		pattern.low[i] = (int)(Rand() % 2u);
	pattern.extras = (int)(Rand() % 13u);
	Build(&pattern, built);
}

static void ExpectFlag(const char* name, float left, float top, float right, float bottom, float radius,
					   const Built* built, const float* scales) {
	XvtTouchLayoutRect rects[CTL_COUNT];
	int ia = -1;
	int ib = -1;
	int hit;
	int rc;
	Dirty(rects);
	rc = PlaceBuilt(built, left, top, right, bottom, radius, scales, rects);
	hit = FindHit(rects, scales, 1, &ia, &ib);
	if ((rc == 1) == (hit == 0))

		return;
	Failf("flag %s rc %d hit %d pair %d %d", name, rc, hit, ia, ib);
}

static void GateStack(void) {
	Pattern pattern = k_patterns[0];
	Built built;
	float scales[CTL_COUNT];
	XvtTouchLayoutRect rects[CTL_COUNT];
	int trial;
	int area;
	static const float k_right[] = { 2556.0f, 1179.0f, 844.0f, 480.0f };
	static const float k_bottom[] = { 1179.0f, 2556.0f, 320.0f, 390.0f };
	Build(&pattern, &built);
	FillScales(scales, 1.6f);
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, rects) == 1,
		   "landscape 1.6 places");
	Expect(!FindHit(rects, scales, 0, NULL, NULL), "landscape 1.6 no overlap");
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 1179.0f, 2556.0f, 96.0f, scales, rects) == 1,
		   "portrait 1.6 places");
	Expect(!FindHit(rects, scales, 0, NULL, NULL), "portrait 1.6 no overlap");
	g_rng = 0x4F1A7u;
	for (area = 0; area < 4; ++area) {
		for (trial = 0; trial < 24; ++trial) {
			RandomBuilt(&built);
			RandomScales(scales);
			ExpectFlag("random", 0.0f, 0.0f, k_right[area], k_bottom[area], 96.0f, &built, scales);
		}
	}
}

static void GateFlag(void) {
	Pattern pattern = k_patterns[0];
	Built built;
	float scales[CTL_COUNT];
	XvtTouchLayoutRect rects[CTL_COUNT];
	int ia = -1;
	int ib = -1;
	Build(&pattern, &built);
	FillScales(scales, 1.0f);
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 320.0f, 800.0f, 96.0f, scales, rects) == 1,
		   "narrow scale 1 returns 1");
	Expect(FindHit(rects, scales, 0, &ia, &ib), "narrow scale 1 overlaps geometrically");
	Expect(!FindHit(rects, scales, 1, &ia, &ib), "narrow scale 1 ignores the overlap");
	scales[CTL_LEFT] = 1.6f;
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 320.0f, 800.0f, 96.0f, scales, rects) == 0,
		   "narrow scaled stick returns 0");
	Expect(FindHit(rects, scales, 1, &ia, &ib), "narrow scaled stick overlaps a pair");
	FillScales(scales, 1.0f);
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 844.0f, 320.0f, 96.0f, scales, rects) == 1,
		   "short scale 1 returns 1");
	scales[CTL_LEFT] = 1.6f;
	Dirty(rects);
	Expect(PlaceBuilt(&built, 0.0f, 0.0f, 844.0f, 320.0f, 96.0f, scales, rects) == 0,
		   "short scaled stick returns 0");
	Expect(FindHit(rects, scales, 1, &ia, &ib), "short scaled stick overlaps a pair");
}

static void GatePinch(void) {
	Pattern pattern = k_patterns[0];
	Built built;
	float unit[CTL_COUNT];
	float grown[CTL_COUNT];
	XvtTouchLayoutRect base[CTL_COUNT];
	XvtTouchLayoutRect next[CTL_COUNT];
	float radius = 96.0f;
	float stick = radius * 2.0f;
	float bottom = 1179.0f;
	float margin = MarginOf(radius);
	int i;
	Build(&pattern, &built);
	FillScales(unit, 1.0f);
	FillScales(grown, 1.0f);
	grown[CTL_LEFT] = 1.6f;
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, bottom, radius, unit, base);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, bottom, radius, grown, next);
	Expect(Near(base[CTL_LEFT].y + base[CTL_LEFT].h, bottom - margin), "stick bottom at scale 1");
	Expect(Near(next[CTL_LEFT].y + next[CTL_LEFT].h, bottom - margin), "stick bottom at scale 1.6");
	Expect(Near(next[CTL_LEFT].w, stick * 1.6f), "left stick width tracks scale");
	for (i = 0; i < 3; ++i) {
		int index = k_throttle_ids[i];
		float base_line = base[index].y + base[index].h;
		float next_line = next[index].y + next[index].h;
		Expect(Near(base_line - next_line, (1.6f - 1.0f) * stick), "throttle baseline follows the stick");
	}
}

static void ExpectInvalid(const char* name, const Built* built, float left, float top, float right,
						  float bottom, float radius, const float* scales, const XvtTouchLayoutPlan* plan) {
	XvtTouchLayoutRect rects[CTL_COUNT];
	XvtTouchLayoutArea area;
	const XvtTouchLayoutPlan* used = plan ? plan : &built->plan;
	int rc;
	area.left = left;
	area.top = top;
	area.right = right;
	area.bottom = bottom;
	Dirty(rects);
	rc = XvtTouchLayout_Place(&area, radius, used, scales, rects, CTL_COUNT);
	if (rc == 0 && Cleared(rects))

		return;
	Failf("sanity %s rc %d", name, rc);
}

static void GateSanity(void) {
	Pattern pattern = k_patterns[0];
	Built built;
	Built bad;
	float scales[CTL_COUNT];
	float reference[CTL_COUNT];
	XvtTouchLayoutRect got[CTL_COUNT];
	XvtTouchLayoutRect expect[CTL_COUNT];
	int bad_throttle[1];
	int bad_row[1];
	int bad_extra[1];
	Build(&pattern, &built);
	FillScales(scales, 1.0f);
	ExpectInvalid("radius 0", &built, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, scales, NULL);
	ExpectInvalid("radius -5", &built, 0.0f, 0.0f, 800.0f, 600.0f, -5.0f, scales, NULL);
	ExpectInvalid("radius nan", &built, 0.0f, 0.0f, 800.0f, 600.0f, NAN, scales, NULL);
	ExpectInvalid("width 0", &built, 0.0f, 0.0f, 0.0f, 600.0f, 96.0f, scales, NULL);
	ExpectInvalid("height 0", &built, 0.0f, 0.0f, 600.0f, 0.0f, 96.0f, scales, NULL);
	bad = built;
	bad.plan.esc = CTL_COUNT;
	ExpectInvalid("esc past end", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad = built;
	bad.plan.esc = -2;
	ExpectInvalid("esc -2", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad = built;
	bad.plan.left = CTL_COUNT;
	ExpectInvalid("left past end", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad = built;
	bad.plan.fire = -3;
	ExpectInvalid("fire -3", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad = built;
	bad.plan.right = -1;
	ExpectInvalid("right missing", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad_throttle[0] = -1;
	bad = built;
	bad.plan.throttle = bad_throttle;
	bad.plan.throttle_count = 1;
	ExpectInvalid("throttle -1", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad_throttle[0] = CTL_COUNT;
	ExpectInvalid("throttle past end", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad_row[0] = 1000;
	bad = built;
	bad.plan.row_top = bad_row;
	bad.plan.row_top_count = 1;
	ExpectInvalid("row past end", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);
	bad_extra[0] = -5;
	bad = built;
	bad.plan.extras = bad_extra;
	bad.plan.extras_count = 1;
	ExpectInvalid("extra -5", &built, 0.0f, 0.0f, 800.0f, 600.0f, 96.0f, scales, &bad.plan);

	FillScales(scales, NAN);
	FillScales(reference, (float)XVT_TOUCH_SCALE_DEFAULT);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, got);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, reference, expect);
	Expect(SameRects(got, expect), "nan scale matches 1");
	FillScales(scales, INFINITY);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, got);
	Expect(SameRects(got, expect), "inf scale matches 1");
	FillScales(scales, 1.0f);
	scales[CTL_LEFT] = NAN;
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, got);
	Expect(SameRects(got, expect), "one nan scale matches 1");
	FillScales(scales, -1.0f);
	FillScales(reference, (float)XVT_TOUCH_SCALE_MIN);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, got);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, reference, expect);
	Expect(SameRects(got, expect), "scale -1 clamps to the minimum");
	Expect(Near(got[CTL_LEFT].w, 96.0f * 2.0f * (float)XVT_TOUCH_SCALE_MIN), "clamped stick width");
	FillScales(scales, 99.0f);
	FillScales(reference, (float)XVT_TOUCH_SCALE_MAX);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, scales, got);
	PlaceBuilt(&built, 0.0f, 0.0f, 2556.0f, 1179.0f, 96.0f, reference, expect);
	Expect(SameRects(got, expect), "scale 99 clamps to the maximum");
	Expect(Near(got[CTL_LEFT].w, 96.0f * 2.0f * (float)XVT_TOUCH_SCALE_MAX), "capped stick width");
}

int main(void) {
	int compared = GateParity();
	int expect = CountOf(sizeof k_patterns, sizeof k_patterns[0]);
	expect *= CountOf(sizeof k_widths, sizeof k_widths[0]);
	expect *= CountOf(sizeof k_heights, sizeof k_heights[0]);
	expect *= CountOf(sizeof k_radii, sizeof k_radii[0]);
	expect *= CountOf(sizeof k_origins, sizeof k_origins[0]);
	Expect(compared == expect, "configuration count");
	GateSizes();
	GateStack();
	GateFlag();
	GatePinch();
	GateSanity();
	if (g_failed)

		return 1;
	printf("compared %d configurations\n", compared);

	return 0;
}
