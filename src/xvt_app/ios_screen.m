#include "xvt_app/ios_screen.h"

#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>
#include <math.h>

static int XvtIosScreen_SameSize(CGSize a, CGSize b) {
	return fabs(a.width - b.width) < 1.0 && fabs(a.height - b.height) < 1.0;
}

static int XvtIosScreen_FillsDisplay(CGRect on_screen, CGRect display) {
	return fabs(on_screen.origin.x - display.origin.x) < 2.0 &&
		   fabs(on_screen.origin.y - display.origin.y) < 2.0 &&
		   XvtIosScreen_SameSize(on_screen.size, display.size);
}

/* A full-screen scene can resize before the root view follows, so keep the
 * root view on the window bounds. */
void XvtIosScreen_Fill(void) {
	SDL_Window* sdl_window;
	SDL_Window** windows;
	SDL_PropertiesID props;
	UIWindow* uiwindow;
	UIWindowScene* scene;
	UIView* view;
	CGRect display;
	CGRect on_screen;
	int count = 0;

	sdl_window = SDL_GetKeyboardFocus();
	if (!sdl_window) {
		windows = SDL_GetWindows(&count);
		if (windows && count > 0)
			sdl_window = windows[0];
		SDL_free(windows);
	}
	if (!sdl_window)
		return;
	props = SDL_GetWindowProperties(sdl_window);
	uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
	scene = uiwindow.windowScene;
	if (!uiwindow || !scene || !scene.screen)
		return;

	display = scene.screen.bounds;
	on_screen = [scene.screen.coordinateSpace convertRect:uiwindow.bounds fromCoordinateSpace:uiwindow];
	if (XvtIosScreen_FillsDisplay(on_screen, display)) {
		view = uiwindow.rootViewController.view;
		if (view && !XvtIosScreen_SameSize(view.bounds.size, uiwindow.bounds.size)) {
			view.frame = uiwindow.bounds;
			[view layoutIfNeeded];
		}
	}
}
