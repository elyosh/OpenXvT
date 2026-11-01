#include "xvt_app/duo_pose.h"

#include "aeron/log.h"

#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
	XVT_DUO_HOLD_FRAMES = 30,
	XVT_DUO_REGION_CAP = 8,
};

/* One reserved region as UIKit reports it, in view points. Kind 0 is a
 * division. Kind 1 is an occlusion. */
typedef struct XvtDuoRegion {
	int kind;
	int active;
	CGRect frame;
} XvtDuoRegion;

static XvtDuoHold g_hold;
static XvtDuoRegion g_logged[XVT_DUO_REGION_CAP];
static int g_logged_count = -1;
static int g_logged_orientation = -1;
static int g_logged_w = -1;
static int g_logged_h = -1;

static SDL_Window* XvtDuoWindow(void) {
	SDL_Window* window = SDL_GetKeyboardFocus();
	int count = 0;
	SDL_Window** windows;
	if (window)
		return window;
	windows = SDL_GetWindows(&count);
	if (windows && count > 0)
		window = windows[0];
	SDL_free(windows);

	return window;
}

static UIView* XvtDuoRootView(SDL_Window* window) {
	SDL_PropertiesID props;
	UIWindow* uiwindow;
	if (!window)
		return nil;
	props = SDL_GetWindowProperties(window);
	uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
	if (!uiwindow)
		return nil;
	if (uiwindow.rootViewController.view)
		return uiwindow.rootViewController.view;

	return uiwindow;
}

static int XvtDuoEdge(CGFloat value, CGFloat scale, int limit) {
	int edge = (int)llround(value * scale);
	if (edge < 0)
		return 0;
	if (edge > limit)
		return limit;

	return edge;
}

static UIInterfaceOrientation XvtDuoOrientation(UIView* view) {
	UIWindowScene* scene = view.window.windowScene;
	if (!scene)
		return UIInterfaceOrientationUnknown;
	/* TODO: Modernization - Window bounds cannot tell landscape-left from landscape-right, and
	 * XvtDuoUpperHigh needs that difference to pick the upper screen. Keep this current API unless
	 * the device orientation is the preferred source. */

	return scene.effectiveGeometry.interfaceOrientation;
}

/* The Duo is upright when its top is the top of the window. The upper screen
 * lies past the band when the device top points right or down. */
static int XvtDuoUpperHigh(UIInterfaceOrientation orientation) {
	return orientation == UIInterfaceOrientationLandscapeRight ||
		   orientation == UIInterfaceOrientationPortraitUpsideDown;
}

/* Reads the division and occlusion regions in view points, inactive ones too,
 * so the log can show a region that is about to appear or is fading. */
static int XvtDuoReadRegions(UIView* view, XvtDuoRegion* regions, int cap) {
	int count = 0;
	if (@available(iOS 27.1, *)) {
		UIViewReservedRegionKind* kinds[2];
		kinds[0] = [UIViewReservedRegionKind divisionRegionKind];
		kinds[1] = [UIViewReservedRegionKind occlusionRegionKind];
		for (int kind = 0; kind < 2; ++kind) {
			NSArray<UIViewReservedRegion*>* found = [view reservedRegionsOfKind:kinds[kind]
																		options:UIViewReservedRegionQueryOptionsIncludeInactive];
			for (UIViewReservedRegion* region in found) {
				if (count >= cap)

					return count;
				regions[count].kind = kind;
				regions[count].active = region.active;
				regions[count].frame = region.frame;
				count += 1;
			}
		}
	}

	return count;
}

/* Converts the active regions to drawable pixels. Thin bands only survive
 * the split rules, so the conversion keeps every active region. */
static int XvtDuoBandsFrom(const XvtDuoRegion* regions, int count, CGRect bounds, int pixel_w, int pixel_h,
						   XvtDuoBand* bands, int cap) {
	CGFloat scale_x;
	CGFloat scale_y;
	int n = 0;
	if (bounds.size.width < 1.0 || bounds.size.height < 1.0)

		return 0;
	scale_x = (CGFloat)pixel_w / bounds.size.width;
	scale_y = (CGFloat)pixel_h / bounds.size.height;
	for (int i = 0; i < count && n < cap; ++i) {
		CGRect frame = regions[i].frame;
		int x0, y0, x1, y1;
		if (!regions[i].active)
			continue;
		x0 = XvtDuoEdge(frame.origin.x, scale_x, pixel_w);
		y0 = XvtDuoEdge(frame.origin.y, scale_y, pixel_h);
		x1 = XvtDuoEdge(frame.origin.x + frame.size.width, scale_x, pixel_w);
		y1 = XvtDuoEdge(frame.origin.y + frame.size.height, scale_y, pixel_h);
		if (x1 <= x0 || y1 <= y0)
			continue;
		bands[n++] = (XvtDuoBand) { x0, y0, x1 - x0, y1 - y0 };
	}

	return n;
}

/* Logs the region set only when it changes, so a device log shows each pose
 * the user passes through and the regions UIKit reported in it. */
static void XvtDuoLogRegions(const XvtDuoRegion* regions, int count, UIInterfaceOrientation orientation, int pixel_w,
							 int pixel_h) {
	char text[1024] = "";
	size_t used = 0;
	int same = count == g_logged_count && (int)orientation == g_logged_orientation && pixel_w == g_logged_w &&
			   pixel_h == g_logged_h;
	if (same && count > 0)
		same = memcmp(regions, g_logged, sizeof(XvtDuoRegion) * (size_t)count) == 0;
	if (same)

		return;
	for (int i = 0; i < count; ++i) {
		int n = snprintf(text + used, sizeof text - used, " %s %s %g %g %g %g",
						 regions[i].kind == 0 ? "division" : "occlusion", regions[i].active ? "active" : "inactive",
						 regions[i].frame.origin.x, regions[i].frame.origin.y, regions[i].frame.size.width,
						 regions[i].frame.size.height);
		if (n < 0 || (size_t)n >= sizeof text - used)
			break;
		used += (size_t)n;
	}
	Aeron_LogInfo("xvt.input", "duo regions orientation %d window %d x %d px count %d%s", (int)orientation, pixel_w,
				  pixel_h, count, text);
	memcpy(g_logged, regions, sizeof(XvtDuoRegion) * (size_t)count);
	g_logged_count = count;
	g_logged_orientation = (int)orientation;
	g_logged_w = pixel_w;
	g_logged_h = pixel_h;
}

int XvtDuoPose_Query(XvtDuoSplit* out) {
	SDL_Window* window;
	UIView* view = nil;
	XvtDuoRegion regions[XVT_DUO_REGION_CAP];
	XvtDuoBand bands[XVT_DUO_REGION_CAP];
	XvtDuoSplit raw;
	UIInterfaceOrientation orientation = UIInterfaceOrientationUnknown;
	int pixel_w = 0;
	int pixel_h = 0;
	int count = 0;
	int band_count = 0;
	if (!out)

		return 0;
	memset(&raw, 0, sizeof raw);
	window = XvtDuoWindow();
	if (window && SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h) && pixel_w > 0 && pixel_h > 0)
		view = XvtDuoRootView(window);
	if (view) {
		CGRect bounds = view.bounds;
		count = XvtDuoReadRegions(view, regions, XVT_DUO_REGION_CAP);
		orientation = XvtDuoOrientation(view);
		band_count = XvtDuoBandsFrom(regions, count, bounds, pixel_w, pixel_h, bands, XVT_DUO_REGION_CAP);
		if (band_count == 0 && view.window && view.window != view) {
			count = XvtDuoReadRegions(view.window, regions, XVT_DUO_REGION_CAP);
			band_count = XvtDuoBandsFrom(regions, count, bounds, pixel_w, pixel_h, bands, XVT_DUO_REGION_CAP);
		}
		XvtDuoLogRegions(regions, count, orientation, pixel_w, pixel_h);
		XvtDuoSplit_FromBands(pixel_w, pixel_h, bands, band_count, XvtDuoUpperHigh(orientation), &raw);
	} else {
		raw.pixel_w = pixel_w;
		raw.pixel_h = pixel_h;
	}
	XvtDuoHold_Step(&g_hold, &raw, XVT_DUO_HOLD_FRAMES, out);

	return out->active;
}
