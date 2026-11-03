#include "xvt_app/duo_pose.h"

#include <string.h>

static int PaneFits(int pane, int window) {
	if (pane <= 0)

		return 0;

	return pane >= window / 5;
}

/* A fold band is thin. Observed bands are about 4 to 7 percent of the long
 * side of the window. A taller band is an occluder, such as the keyboard. */
static int BandThin(int thickness, int pixel_w, int pixel_h) {
	int long_side = pixel_w > pixel_h ? pixel_w : pixel_h;

	return thickness * 100 <= long_side * 12;
}

static int BandHorizontal(const XvtDuoBand* band, int pixel_w, int pixel_h) {
	if (band->width <= band->height)

		return 0;
	if (band->width * 5 < pixel_w * 3)

		return 0;
	if (!BandThin(band->height, pixel_w, pixel_h))

		return 0;

	return PaneFits(band->y, pixel_h) && PaneFits(pixel_h - (band->y + band->height), pixel_h);
}

static int BandVertical(const XvtDuoBand* band, int pixel_w, int pixel_h) {
	if (band->height <= band->width)

		return 0;
	if (band->height * 5 < pixel_h * 3)

		return 0;
	if (!BandThin(band->width, pixel_w, pixel_h))

		return 0;

	return PaneFits(band->x, pixel_w) && PaneFits(pixel_w - (band->x + band->width), pixel_w);
}

int XvtDuoSplit_FromBands(int pixel_w, int pixel_h, const XvtDuoBand* bands, int count, int upper_high,
						  XvtDuoSplit* out) {
	int best = -1;
	int best_span = -1;
	int i;
	XvtDuoRect low;
	XvtDuoRect high;
	if (!out)

		return 0;
	memset(out, 0, sizeof *out);
	out->pixel_w = pixel_w;
	out->pixel_h = pixel_h;
	if (pixel_w <= 0 || pixel_h <= 0 || count < 1 || !bands)

		return 0;
	for (i = 0; i < count; ++i) {
		const XvtDuoBand* band = &bands[i];
		int span;
		if (band->width <= 0 || band->height <= 0)
			continue;
		if (BandHorizontal(band, pixel_w, pixel_h))
			span = band->width;
		else if (BandVertical(band, pixel_w, pixel_h))
			span = band->height;
		else
			continue;
		if (span > best_span) {
			best_span = span;
			best = i;
		}
	}
	if (best < 0)

		return 0;
	out->active = 1;
	out->pixel_w = pixel_w;
	out->pixel_h = pixel_h;
	if (BandHorizontal(&bands[best], pixel_w, pixel_h)) {
		int y_end = bands[best].y + bands[best].height;
		low = (XvtDuoRect) { 0, 0, pixel_w, bands[best].y };
		high = (XvtDuoRect) { 0, y_end, pixel_w, pixel_h - y_end };
	} else {
		int x_end = bands[best].x + bands[best].width;
		low = (XvtDuoRect) { 0, 0, bands[best].x, pixel_h };
		high = (XvtDuoRect) { x_end, 0, pixel_w - x_end, pixel_h };
	}
	out->game = upper_high ? high : low;
	out->controls = upper_high ? low : high;

	return 1;
}

void XvtDuoSplit_Insets(const XvtDuoSplit* split, int* left, int* top, int* right, int* bottom) {
	int inset_left = 0;
	int inset_top = 0;
	int inset_right = 0;
	int inset_bottom = 0;
	if (split && split->active) {
		inset_left = split->game.x;
		inset_top = split->game.y;
		inset_right = split->pixel_w - (split->game.x + split->game.width);
		inset_bottom = split->pixel_h - (split->game.y + split->game.height);
		if (inset_left < 0 || inset_top < 0 || inset_right < 0 || inset_bottom < 0) {
			inset_left = 0;
			inset_top = 0;
			inset_right = 0;
			inset_bottom = 0;
		}
	}
	if (left)
		*left = inset_left;
	if (top)
		*top = inset_top;
	if (right)
		*right = inset_right;
	if (bottom)
		*bottom = inset_bottom;
}

void XvtDuoHold_Step(XvtDuoHold* hold, const XvtDuoSplit* raw, int hold_frames, XvtDuoSplit* out) {
	if (!hold || !out)
		return;
	if (raw && raw->active) {
		hold->last = *raw;
		hold->missing = 0;
		*out = *raw;

		return;
	}
	memset(out, 0, sizeof *out);
	if (raw) {
		out->pixel_w = raw->pixel_w;
		out->pixel_h = raw->pixel_h;
	}
	if (!hold->last.active)

		return;
	if (!raw || raw->pixel_w != hold->last.pixel_w || raw->pixel_h != hold->last.pixel_h) {
		memset(hold, 0, sizeof *hold);

		return;
	}
	hold->missing += 1;
	if (hold->missing > hold_frames) {
		memset(hold, 0, sizeof *hold);

		return;
	}
	*out = hold->last;
}
