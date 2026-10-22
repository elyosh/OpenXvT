#include "xvt_app/duo_pose.h"

#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>

#include <math.h>
#include <string.h>

@interface XvtHingeBox : NSObject
@property (nonatomic, assign) UIHingeStatus status;
@property (nonatomic, assign) BOOL known;
@end

@implementation XvtHingeBox

- (void)noteInteraction:(UIHingeInteraction*)interaction update:(UIHingeInteractionUpdate*)update {
	(void)interaction;
	if (!update.hinge) {
		self.known = NO;
		self.status = UIHingeStatusUnknown;

		return;
	}
	self.known = YES;
	self.status = update.hinge.status;
}

@end

static XvtHingeBox* g_hinge;
static UIHingeInteraction* g_interaction;
static __weak UIView* g_view;

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

static void XvtDuoInstall(UIView* view) {
	XvtHingeBox* box;
	if (!view || view == g_view)
		return;
	if (g_view && g_interaction)
		[g_view removeInteraction:g_interaction];
	box = g_hinge ? g_hinge : [XvtHingeBox new];
	g_hinge = box;
	g_interaction = [[UIHingeInteraction alloc] initWithUpdateHandler:^(UIHingeInteraction* interaction,
																		 UIHingeInteractionUpdate* update) {
	  [box noteInteraction:interaction update:update];
	}];
	[view addInteraction:g_interaction];
	g_view = view;
}

static int XvtDuoDivision(UIView* view, SDL_Window* window, XvtDuoSplit* out) {
	NSArray<UIViewReservedRegion*>* regions;
	UIViewReservedRegion* band = nil;
	CGRect bounds;
	CGFloat width;
	int pixel_w = 0;
	int pixel_h = 0;
	CGFloat scale_y;
	if (!SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h) || pixel_w <= 0 || pixel_h <= 0)
		return 0;
	bounds = view.bounds;
	if (bounds.size.width < 1.0 || bounds.size.height < 1.0)
		return 0;
	regions = [view reservedRegionsOfKind:[UIViewReservedRegionKind divisionRegionKind]];
	width = 0.0;
	for (UIViewReservedRegion* region in regions) {
		CGRect frame;
		if (!region.active)
			continue;
		frame = region.frame;
		if (frame.size.width < frame.size.height)
			continue;
		if (frame.size.width < bounds.size.width * 0.6)
			continue;
		if (frame.origin.y < bounds.size.height * 0.2 || frame.origin.y > bounds.size.height * 0.8)
			continue;
		if (frame.size.width > width) {
			width = frame.size.width;
			band = region;
		}
	}
	scale_y = (CGFloat)pixel_h / bounds.size.height;
	out->pixel_w = pixel_w;
	out->pixel_h = pixel_h;
	if (band) {
		CGRect frame = band.frame;
		out->active = 1;
		out->game_bottom = (int)llround(frame.origin.y * scale_y);
		out->controls_top = (int)llround((frame.origin.y + frame.size.height) * scale_y);
	} else if (g_hinge.known && g_hinge.status == UIHingeStatusPartiallyOpen) {
		out->active = 1;
		out->game_bottom = pixel_h / 2;
		out->controls_top = pixel_h / 2;
	} else {
		return 0;
	}
	if (out->game_bottom < pixel_h / 5 || out->controls_top > pixel_h - pixel_h / 5)
		return 0;
	if (out->controls_top < out->game_bottom)
		out->controls_top = out->game_bottom;

	return 1;
}

int XvtDuoPose_Query(XvtDuoSplit* out) {
	SDL_Window* window;
	UIView* view;
	if (!out)
		return 0;
	memset(out, 0, sizeof *out);
	window = XvtDuoWindow();
	view = XvtDuoRootView(window);
	if (!view)
		return 0;
	XvtDuoInstall(view);
	if (XvtDuoDivision(view, window, out))
		return 1;
	if (view.window && view.window != view && XvtDuoDivision(view.window, window, out))
		return 1;
	out->active = 0;

	return 0;
}
