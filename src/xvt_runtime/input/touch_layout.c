#include "xvt_runtime/input/touch_layout.h"

#include "xvt_runtime/input/touch_tuning.h"

#include <math.h>

static float LayoutScale(const float* scales, int index) {
	float scale = scales[index];
	if (!isfinite(scale))

		return (float)XVT_TOUCH_SCALE_DEFAULT;
	if (scale < (float)XVT_TOUCH_SCALE_MIN)

		return (float)XVT_TOUCH_SCALE_MIN;
	if (scale > (float)XVT_TOUCH_SCALE_MAX)

		return (float)XVT_TOUCH_SCALE_MAX;

	return scale;
}

static float LayoutRowScale(const int* ids, int count, const float* scales) {
	float max_scale;
	int i;
	if (count <= 0)

		return 1.0f;
	max_scale = LayoutScale(scales, ids[0]);
	for (i = 1; i < count; ++i) {
		float scale = LayoutScale(scales, ids[i]);
		if (scale > max_scale)
			max_scale = scale;
	}

	return max_scale;
}

static void LayoutClear(XvtTouchLayoutRect* rects, int count) {
	int i;
	for (i = 0; i < count; ++i) {
		rects[i].x = 0.0f;
		rects[i].y = 0.0f;
		rects[i].w = 0.0f;
		rects[i].h = 0.0f;
		rects[i].placed = 0;
	}
}

static void LayoutPut(XvtTouchLayoutRect* rects, int index, float x, float y, float w, float h) {
	rects[index].x = x;
	rects[index].y = y;
	rects[index].w = w;
	rects[index].h = h;
	rects[index].placed = 1;
}

static void LayoutRow(const int* ids, int count, float right, float baseline, float bw, float bh,
					  float gap, const float* scales, XvtTouchLayoutRect* rects) {
	float total;
	float x;
	int i;
	if (count <= 0)

		return;
	total = gap * (float)(count - 1);
	for (i = 0; i < count; ++i)
		total += bw * LayoutScale(scales, ids[i]);
	x = right - total;
	for (i = 0; i < count; ++i) {
		float scale = LayoutScale(scales, ids[i]);
		float w = bw * scale;
		float h = bh * scale;
		LayoutPut(rects, ids[i], x, baseline - h, w, h);
		x += w + gap;
	}
}

static void LayoutSpan(const int* ids, int count, float left, float right, float baseline, float bh,
					   float gap, const float* scales, XvtTouchLayoutRect* rects) {
	float width = right - left;
	float bw_eq;
	float max_bw;
	float total;
	float x;
	int i;
	if (count <= 0 || width < 1.0f)

		return;
	bw_eq = (width - gap * (float)(count - 1)) / (float)count;
	if (bw_eq < 1.0f)
		bw_eq = 1.0f;
	max_bw = bh * 2.4f;
	if (bw_eq > max_bw)
		bw_eq = max_bw;
	total = gap * (float)(count - 1);
	for (i = 0; i < count; ++i)
		total += bw_eq * LayoutScale(scales, ids[i]);
	x = left + (width - total) * 0.5f;
	for (i = 0; i < count; ++i) {
		float scale = LayoutScale(scales, ids[i]);
		float w = bw_eq * scale;
		float h = bh * scale;
		LayoutPut(rects, ids[i], x, baseline - h, w, h);
		x += w + gap;
	}
}

static int LayoutHits(const XvtTouchLayoutRect* a, const XvtTouchLayoutRect* b) {
	if (a->x < b->x + b->w - 0.5f && b->x < a->x + a->w - 0.5f && a->y < b->y + b->h - 0.5f &&
		b->y < a->y + a->h - 0.5f)

		return 1;

	return 0;
}

static int LayoutScaleClear(const XvtTouchLayoutRect* rects, const float* scales, int count) {
	int i;
	int j;
	for (i = 0; i < count; ++i) {
		float left_scale;
		if (!rects[i].placed)
			continue;
		left_scale = LayoutScale(scales, i);
		for (j = i + 1; j < count; ++j) {
			float right_scale;
			if (!rects[j].placed)
				continue;
			right_scale = LayoutScale(scales, j);
			/* Scale 1 keeps the shipped pairs, overlap included. */
			if (left_scale == 1.0f && right_scale == 1.0f)
				continue;
			if (LayoutHits(&rects[i], &rects[j]))

				return 0;
		}
	}

	return 1;
}

static int LayoutIndex(int index, int count) {
	return index >= 0 && index < count;
}

static int LayoutOptional(int index, int count) {
	return index == -1 || LayoutIndex(index, count);
}

static int LayoutIds(const int* ids, int count, int control_count) {
	int i;
	if (count < 0)

		return 0;
	if (count == 0)

		return 1;
	if (!ids)

		return 0;
	for (i = 0; i < count; ++i)
		if (!LayoutIndex(ids[i], control_count))

			return 0;

	return 1;
}

static int LayoutPlanOk(const XvtTouchLayoutPlan* plan, int count) {
	if (!LayoutOptional(plan->esc, count) || !LayoutOptional(plan->left, count) ||
		!LayoutOptional(plan->right, count) || !LayoutOptional(plan->fire, count))

		return 0;
	/* The stick pair is placed together. A left stick with no right stick is not a layout. */
	if (plan->left >= 0 && !LayoutIndex(plan->right, count))

		return 0;
	if (!LayoutIds(plan->throttle, plan->throttle_count, count) ||
		!LayoutIds(plan->row_top, plan->row_top_count, count) ||
		!LayoutIds(plan->row_low, plan->row_low_count, count) ||
		!LayoutIds(plan->extras, plan->extras_count, count))

		return 0;

	return 1;
}

int XvtTouchLayout_Place(const XvtTouchLayoutArea* area, float radius, const XvtTouchLayoutPlan* plan,
						 const float* scales, XvtTouchLayoutRect* rects, int count) {
	float margin;
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
	if (count < 0 || !rects)

		return 0;
	LayoutClear(rects, count);
	if (!area || !plan || (count > 0 && !scales))

		return 0;
	if (!isfinite(radius) || !(radius > 0.0f))

		return 0;
	width = area->right - area->left;
	height = area->bottom - area->top;
	if (!(width >= 1.0f) || !(height >= 1.0f))

		return 0;
	if (!LayoutPlanOk(plan, count))

		return 0;
	if (count == 0)

		return 1;

	margin = radius * 0.2f;
	if (margin < 16.0f)
		margin = 16.0f;
	esc_w = radius * 1.85f;
	esc_h = radius * 0.82f;
	stick = radius * 2.0f;
	gap = radius * 0.14f;
	if (gap < 8.0f)
		gap = 8.0f;
	bw = radius * 1.2f;
	bh = radius * 0.74f;
	fire = radius * 1.35f;
	if (plan->esc >= 0) {
		float scale = LayoutScale(scales, plan->esc);
		LayoutPut(rects, plan->esc, area->left + margin, area->top + margin, esc_w * scale,
				  esc_h * scale);
	}
	if (plan->left < 0)

		return LayoutScaleClear(rects, scales, count);

	{
		float left_w = stick * LayoutScale(scales, plan->left);
		float right_w = stick * LayoutScale(scales, plan->right);
		LayoutPut(rects, plan->left, area->left + margin, area->bottom - margin - left_w, left_w,
				  left_w);
		LayoutPut(rects, plan->right, area->right - margin - right_w, area->bottom - margin - right_w,
				  right_w, right_w);
	}
	{
		float x = rects[plan->left].x;
		float baseline = rects[plan->left].y - gap;
		int t;
		for (t = 0; t < plan->throttle_count; ++t) {
			int index = plan->throttle[t];
			float scale = LayoutScale(scales, index);
			float w = bw * scale;
			float h = bh * scale;
			LayoutPut(rects, index, x, baseline - h, w, h);
			x += w + gap;
		}
	}
	if (plan->fire >= 0) {
		float scale = LayoutScale(scales, plan->fire);
		float w = fire * scale;
		float x = rects[plan->right].x + rects[plan->right].w * 0.5f - w * 0.5f;
		LayoutPut(rects, plan->fire, x, rects[plan->right].y - gap - w, w, w);
	}
	row_right = rects[plan->right].x + rects[plan->right].w;
	if (width > height * 1.35f) {
		float inner_left = rects[plan->left].x + rects[plan->left].w + gap;
		float inner_right = rects[plan->right].x - gap;
		float occupied = rects[plan->right].y;
		float top_limit = area->top + margin;
		float h_top;
		float h_low;
		float low_baseline;
		float low_top;
		float top_baseline;
		float top_top;
		int t;
		if (plan->fire >= 0 && rects[plan->fire].placed) {
			inner_right = rects[plan->fire].x - gap;
			occupied = rects[plan->fire].y;
		}
		for (t = 0; t < plan->throttle_count; ++t) {
			int index = plan->throttle[t];
			float edge;
			if (!rects[index].placed)
				continue;
			edge = rects[index].x + rects[index].w + gap;
			if (edge > inner_left)
				inner_left = edge;
			if (rects[index].y < occupied)
				occupied = rects[index].y;
		}
		if (plan->esc >= 0 && rects[plan->esc].placed)
			top_limit = rects[plan->esc].y + rects[plan->esc].h + gap;
		h_top = bh * LayoutRowScale(plan->row_top, plan->row_top_count, scales);
		h_low = bh * LayoutRowScale(plan->row_low, plan->row_low_count, scales);
		low_baseline = occupied - gap;
		low_top = low_baseline - h_low;
		top_baseline = low_top - gap;
		top_top = top_baseline - h_top;
		if (top_top < top_limit) {
			top_top = top_limit;
			low_top = top_top + h_top + gap;
			low_baseline = low_top + h_low;
			top_baseline = top_top + h_top;
		}
		if (inner_right > inner_left + bh * 4.0f) {
			LayoutSpan(plan->row_top, plan->row_top_count, inner_left, inner_right, top_baseline, bh,
					   gap, scales, rects);
			LayoutSpan(plan->row_low, plan->row_low_count, inner_left, inner_right, low_baseline, bh,
					   gap, scales, rects);
		} else {
			LayoutRow(plan->row_low, plan->row_low_count, row_right, low_baseline, bw, bh, gap, scales,
					  rects);
			LayoutRow(plan->row_top, plan->row_top_count, row_right, top_baseline, bw, bh, gap, scales,
					  rects);
		}
	} else {
		float y_low_baseline = rects[plan->right].y - gap;
		float h_low = bh * LayoutRowScale(plan->row_low, plan->row_low_count, scales);
		float low_top;
		float top_baseline;
		if (plan->fire >= 0 && rects[plan->fire].placed)
			y_low_baseline = rects[plan->fire].y - gap;
		low_top = y_low_baseline - h_low;
		top_baseline = low_top - gap;
		LayoutRow(plan->row_low, plan->row_low_count, row_right, y_low_baseline, bw, bh, gap, scales,
				  rects);
		LayoutRow(plan->row_top, plan->row_top_count, row_right, top_baseline, bw, bh, gap, scales,
				  rects);
	}
	{
		float anchor = rects[plan->right].y;
		float baseline;
		int offset = 0;
		for (i = 0; i < count; ++i) {
			if (!rects[i].placed || i == plan->left || i == plan->right || i == plan->esc)
				continue;
			if (rects[i].y < anchor)
				anchor = rects[i].y;
		}
		baseline = anchor - gap;
		while (offset < plan->extras_count) {
			int row = plan->extras_count - offset;
			float row_h;
			if (row > 6)
				row = 6;
			row_h = bh * LayoutRowScale(plan->extras + offset, row, scales);
			LayoutRow(plan->extras + offset, row, row_right, baseline, bw, bh, gap, scales, rects);
			baseline = baseline - row_h - gap;
			offset += row;
		}
	}

	return LayoutScaleClear(rects, scales, count);
}
