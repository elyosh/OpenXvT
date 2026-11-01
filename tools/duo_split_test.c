#include "xvt_app/duo_pose.h"
#include "xvt_runtime/runtime/presentation_ratio.h"

#include <stdio.h>
#include <string.h>

static int g_failed;

static void Expect(int ok, const char* name) {
	if (ok)
		return;
	fprintf(stderr, "FAIL %s\n", name);
	g_failed = 1;
}

static int RectEq(XvtDuoRect rect, int x, int y, int width, int height) {
	return rect.x == x && rect.y == y && rect.width == width && rect.height == height;
}

static void Fit(int avail_w, int avail_h, int logical_w, int logical_h, int* out_w, int* out_h) {
	long content_w = avail_w;
	long content_h = avail_h;
	if (content_w * logical_h > content_h * logical_w)
		content_w = (content_h * logical_w) / logical_h;
	else
		content_h = (content_w * logical_h) / logical_w;
	if ((avail_w - content_w) * 256 <= avail_w)
		content_w = avail_w;
	if ((avail_h - content_h) * 256 <= avail_h)
		content_h = avail_h;
	*out_w = (int)content_w;
	*out_h = (int)content_h;
}

static void ExpectSplit(const char* name, const XvtDuoSplit* split, int gx, int gy, int gw, int gh, int cx, int cy,
						int cw, int ch) {
	Expect(split->active == 1, name);
	Expect(RectEq(split->game, gx, gy, gw, gh), name);
	Expect(RectEq(split->controls, cx, cy, cw, ch), name);
}

static void ExpectInsets(const char* name, const XvtDuoSplit* split, int left, int top, int right, int bottom) {
	int l = -1, t = -1, r = -1, b = -1;
	XvtDuoSplit_Insets(split, &l, &t, &r, &b);
	Expect(l == left && t == top && r == right && b == bottom, name);
}

static void CheckPaneFill(const char* name, int pane_w, int pane_h) {
	int logical = XvtPresentation_WidthForBox(pane_w, pane_h, 0);
	int fit_w = 0;
	int fit_h = 0;
	Fit(pane_w, pane_h, logical, 480, &fit_w, &fit_h);
	Expect(fit_w == pane_w && fit_h == pane_h, name);
}

int main(void) {
	/* Landscape window: the fold is a vertical band. */
	XvtDuoBand landscape_fold = { 1367, 0, 120, 2007 };
	/* Portrait window: the same fold is a horizontal band. */
	XvtDuoBand portrait_fold = { 0, 1426, 2007, 40 };
	XvtDuoBand thick_landscape = { 1000, 0, 900, 2007 };
	XvtDuoBand thick_portrait = { 0, 900, 2007, 900 };
	XvtDuoBand keyboard_like = { 0, 1700, 2007, 1153 };
	XvtDuoBand narrow = { 1000, 0, 100, 400 };
	XvtDuoBand edge = { 0, 0, 2007, 40 };
	XvtDuoBand rejected[] = { thick_landscape, keyboard_like, narrow, edge };
	XvtDuoBand both[] = { narrow, landscape_fold };
	XvtDuoSplit split;
	XvtDuoSplit bad;
	XvtDuoHold hold;
	XvtDuoSplit out;
	int i;

	memset(&split, 0x5a, sizeof split);
	Expect(XvtDuoSplit_FromBands(2853, 2007, NULL, 0, 0, &split) == 0, "empty count");
	Expect(split.active == 0 && split.game.height == 0 && split.controls.height == 0, "empty zeroes the split");
	Expect(XvtDuoSplit_FromBands(2853, 2007, &landscape_fold, 1, 0, NULL) == 0, "null out");
	Expect(XvtDuoSplit_FromBands(0, 2007, &landscape_fold, 1, 0, &split) == 0, "nonpositive window");
	Expect(XvtDuoSplit_FromBands(2853, 2007, NULL, 1, 0, &split) == 0, "null bands");

	Expect(XvtDuoSplit_FromBands(2853, 2007, &landscape_fold, 1, 0, &split) == 1, "landscape fold returns active");
	ExpectSplit("landscape fold, upper before band", &split, 0, 0, 1367, 2007, 1487, 0, 1366, 2007);
	ExpectInsets("landscape insets, upper before band", &split, 0, 0, 1486, 0);
	CheckPaneFill("landscape game pane fills", 1367, 2007);

	Expect(XvtDuoSplit_FromBands(2853, 2007, &landscape_fold, 1, 1, &split) == 1, "landscape fold, upper high");
	ExpectSplit("landscape fold, upper past band", &split, 1487, 0, 1366, 2007, 0, 0, 1367, 2007);
	ExpectInsets("landscape insets, upper past band", &split, 1487, 0, 0, 0);
	CheckPaneFill("landscape game pane past band fills", 1366, 2007);

	Expect(XvtDuoSplit_FromBands(2007, 2853, &portrait_fold, 1, 0, &split) == 1, "portrait fold returns active");
	ExpectSplit("portrait fold, upper above band", &split, 0, 0, 2007, 1426, 0, 1466, 2007, 1387);
	ExpectInsets("portrait insets, upper above band", &split, 0, 0, 0, 1427);
	CheckPaneFill("portrait game pane fills", 2007, 1426);

	Expect(XvtDuoSplit_FromBands(2007, 2853, &portrait_fold, 1, 1, &split) == 1, "portrait fold, upper low");
	ExpectSplit("portrait fold, upper below band", &split, 0, 1466, 2007, 1387, 0, 0, 2007, 1426);
	ExpectInsets("portrait insets, upper below band", &split, 0, 1466, 0, 0);
	CheckPaneFill("portrait game pane below band fills", 2007, 1387);

	Expect(XvtDuoSplit_FromBands(2853, 2007, rejected, 4, 0, &split) == 0, "rejected bands");
	Expect(split.active == 0, "rejected split is inactive");
	Expect(split.pixel_w == 2853 && split.pixel_h == 2007, "refused split keeps the drawable size");
	Expect(XvtDuoSplit_FromBands(2007, 2853, &thick_portrait, 1, 0, &split) == 0, "thick portrait band rejected");

	Expect(XvtDuoSplit_FromBands(2853, 2007, both, 2, 0, &split) == 1, "narrow band does not hide the fold");
	ExpectSplit("fold wins over narrow band", &split, 0, 0, 1367, 2007, 1487, 0, 1366, 2007);

	/* Hold: a brief gap keeps the split. A long gap or a resize ends it. */
	memset(&hold, 0, sizeof hold);
	memset(&out, 0x5a, sizeof out);
	XvtDuoHold_Step(&hold, NULL, 30, &out);
	Expect(out.active == 0, "hold starts inactive");
	XvtDuoSplit_FromBands(2853, 2007, &landscape_fold, 1, 0, &split);
	XvtDuoHold_Step(&hold, &split, 30, &out);
	Expect(out.active == 1 && RectEq(out.game, 0, 0, 1367, 2007), "hold passes an active split");
	XvtDuoSplit_FromBands(2853, 2007, rejected, 4, 0, &bad);
	for (i = 0; i < 30; ++i) {
		XvtDuoHold_Step(&hold, &bad, 30, &out);
		if (out.active != 1)
			break;
	}
	Expect(i == 30 && out.active == 1, "hold keeps the split for 30 missing frames");
	XvtDuoHold_Step(&hold, &bad, 30, &out);
	Expect(out.active == 0, "hold ends after 31 missing frames");
	Expect(out.pixel_w == 2853 && out.pixel_h == 2007, "ended hold keeps the drawable size");

	XvtDuoSplit_FromBands(2853, 2007, &landscape_fold, 1, 0, &split);
	XvtDuoHold_Step(&hold, &split, 30, &out);
	XvtDuoSplit_FromBands(2007, 2853, &portrait_fold, 1, 0, &bad);
	XvtDuoHold_Step(&hold, &bad, 30, &out);
	Expect(out.active == 1 && out.pixel_w == 2007 && out.pixel_h == 2853 && RectEq(out.game, 0, 0, 2007, 1426),
		   "rotation hands over to the new fold at once");
	XvtDuoSplit_FromBands(2007, 2853, rejected, 4, 0, &bad);
	XvtDuoHold_Step(&hold, &bad, 30, &out);
	Expect(out.active == 1, "rotation keeps the new split through a gap");
	XvtDuoSplit_FromBands(2853, 2007, rejected, 4, 0, &bad);
	XvtDuoHold_Step(&hold, &bad, 30, &out);
	Expect(out.active == 0, "rotation to a flat screen ends the hold at once");

	ExpectInsets("null split insets", NULL, 0, 0, 0, 0);
	memset(&bad, 0, sizeof bad);
	bad.active = 1;
	bad.pixel_w = 100;
	bad.pixel_h = 100;
	bad.game = (XvtDuoRect) { 0, 0, 120, 100 };
	ExpectInsets("negative complement insets", &bad, 0, 0, 0, 0);

	Expect(XvtPresentation_WidthForBox(2853, 2007, 1) == 682, "unsplit inner window keeps its logical width");
	Expect(XvtPresentation_WidthForBox(951, 669, 1) == 682, "point window and pixel window agree");
	Expect(XvtPresentation_WidthForBox(480, 900, 1) == 640, "unsplit floor stays");

	if (g_failed)
		return 1;
	printf("duo split ok\n");

	return 0;
}
