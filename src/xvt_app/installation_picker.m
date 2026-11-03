#import "xvt_app/installation_picker.h"

#import "aeron/paths.h"

#import <SDL3/SDL.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <stdio.h>
#import <string.h>

enum {
	XVT_PICK_IDLE = 0,
	XVT_PICK_OPEN,
	XVT_PICK_SELECTED,
	XVT_PICK_CANCELLED,
	XVT_PICK_FAILED,
};

static int g_state = XVT_PICK_IDLE;
static int g_accessing = 0;
static NSURL* g_url = nil;
static UIDocumentPickerViewController* g_picker = nil;
static char g_path[1024];
static char g_error[1024];

@interface XvtFolderPickerDelegate : NSObject <UIDocumentPickerDelegate>
@end

static XvtFolderPickerDelegate* g_delegate = nil;

static void XvtPickFail(const char* message) {
	snprintf(g_error, sizeof g_error, "%s", message ? message : "Could not open that folder.");
	g_state = XVT_PICK_FAILED;
}

static NSString* XvtBookmarkFile(void) {
	const char* root = Aeron_UserPath();
	if (!root || !root[0])
		return nil;
	NSString* base = [NSString stringWithUTF8String:root];
	if (![base hasSuffix:@"/"])
		base = [base stringByAppendingString:@"/"];

	return [base stringByAppendingString:@"installation.bookmark"];
}

static int XvtStoreBookmark(NSURL* url, NSString** message) {
	NSError* error = nil;
	NSData* bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark
					 includingResourceValuesForKeys:nil
									  relativeToURL:nil
											  error:&error];
	NSString* file = XvtBookmarkFile();
	if (!bookmark || !file || ![bookmark writeToFile:file atomically:YES]) {
		if (error.localizedDescription)
			*message = error.localizedDescription;
		else
			*message = @"Could not store access to that folder.";

		return 0;
	}

	return 1;
}

static int XvtAdoptURL(NSURL* url, NSString** message) {
	NSURL* previous = g_url;
	int previous_access = g_accessing;
	if (![url startAccessingSecurityScopedResource]) {
		*message = @"iOS did not grant access to that folder.";

		return 0;
	}
	g_url = url;
	g_accessing = 1;
	if (!XvtStoreBookmark(url, message)) {
		[url stopAccessingSecurityScopedResource];
		g_url = previous;
		g_accessing = previous_access;

		return 0;
	}
	if (previous_access && previous && previous != url)
		[previous stopAccessingSecurityScopedResource];

	return 1;
}

static int XvtCopyPath(NSURL* url) {
	const char* filesystem_path = url.fileSystemRepresentation;
	if (!filesystem_path || strlen(filesystem_path) >= sizeof g_path) {
		XvtPickFail("That folder path is too long.");

		return 0;
	}
	memcpy(g_path, filesystem_path, strlen(filesystem_path) + 1);

	return 1;
}

static UIViewController* XvtHostController(void) {
	SDL_Window* window = SDL_GetKeyboardFocus();
	if (!window) {
		int count = 0;
		SDL_Window** windows = SDL_GetWindows(&count);
		if (windows && count > 0)
			window = windows[0];
		SDL_free(windows);
	}
	if (!window)
		return nil;
	UIWindow* uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(
		SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);

	return uiwindow.rootViewController;
}

@implementation XvtFolderPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
	didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
	(void)controller;
	NSURL* url = urls.firstObject;
	NSString* message = nil;
	if (!url) {
		XvtPickFail("The picker returned no folder.");

		return;
	}
	if (!XvtAdoptURL(url, &message) || !XvtCopyPath(url)) {
		if (g_state != XVT_PICK_FAILED)
			XvtPickFail(message.UTF8String);

		return;
	}
	g_state = XVT_PICK_SELECTED;
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
	(void)controller;
	if (g_state == XVT_PICK_OPEN)
		g_state = XVT_PICK_CANCELLED;
}

@end

int XvtInstallationPicker_OpenNative(char* error, size_t capacity) {
	if (g_state == XVT_PICK_OPEN)
		return 1;
	UIViewController* host = XvtHostController();
	if (!host) {
		snprintf(error, capacity, "Cannot present the folder picker.");

		return 0;
	}
	if (!g_delegate)
		g_delegate = [XvtFolderPickerDelegate new];
	UIDocumentPickerViewController* picker =
		[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeFolder ] asCopy:NO];
	picker.delegate = g_delegate;
	picker.allowsMultipleSelection = NO;
	g_picker = picker;
	g_state = XVT_PICK_OPEN;
	g_error[0] = 0;
	dispatch_async(dispatch_get_main_queue(), ^{
	  if (g_state != XVT_PICK_OPEN || !g_picker)
		  return;
	  [host presentViewController:g_picker animated:YES completion:nil];
	});

	return 1;
}

AeronUiFilePickerResult XvtInstallationPicker_PollNative(char* path, size_t capacity, char* error,
														 size_t error_capacity) {
	AeronUiFilePickerResult result = AERON_UI_FILE_PICKER_NONE;
	if (g_state == XVT_PICK_SELECTED) {
		if (!path || strlen(g_path) >= capacity) {
			snprintf(error, error_capacity, "That folder path is too long.");
			result = AERON_UI_FILE_PICKER_ERROR;
		} else {
			memcpy(path, g_path, strlen(g_path) + 1);
			if (error_capacity)
				error[0] = 0;
			result = AERON_UI_FILE_PICKER_SELECTED;
		}
	} else if (g_state == XVT_PICK_CANCELLED) {
		result = AERON_UI_FILE_PICKER_CANCELLED;
	} else if (g_state == XVT_PICK_FAILED) {
		snprintf(error, error_capacity, "%s", g_error);
		result = AERON_UI_FILE_PICKER_ERROR;
	}
	if (result != AERON_UI_FILE_PICKER_NONE) {
		g_state = XVT_PICK_IDLE;
		g_picker = nil;
	}

	return result;
}

int XvtInstallationPicker_NativeOpen(void) { return g_state == XVT_PICK_OPEN; }

void XvtInstallationPicker_CancelNative(void) {
	if (g_state != XVT_PICK_OPEN)
		return;
	UIViewController* host = XvtHostController();
	[host dismissViewControllerAnimated:YES completion:nil];
	g_state = XVT_PICK_IDLE;
	g_picker = nil;
}

int XvtInstallationPicker_Restore(char* path, size_t capacity, char* error, size_t error_capacity) {
	NSString* file = XvtBookmarkFile();
	NSData* bookmark = file ? [NSData dataWithContentsOfFile:file] : nil;
	if (!bookmark)
		return 0;
	BOOL stale = NO;
	NSError* resolve_error = nil;
	NSURL* url = [NSURL URLByResolvingBookmarkData:bookmark
										   options:NSURLBookmarkResolutionWithoutUI
									 relativeToURL:nil
							   bookmarkDataIsStale:&stale
											 error:&resolve_error];
	NSString* message = nil;
	const char* filesystem_path;
	if (!url || ![url startAccessingSecurityScopedResource]) {
		const char* reason = resolve_error.localizedDescription.UTF8String;
		snprintf(error, error_capacity, "%s", reason ? reason : "Saved folder access has expired.");

		return 0;
	}
	filesystem_path = url.fileSystemRepresentation;
	if (!path || !filesystem_path || strlen(filesystem_path) >= capacity) {
		[url stopAccessingSecurityScopedResource];
		snprintf(error, error_capacity, "Saved folder path is too long.");

		return 0;
	}
	if (g_accessing && g_url)
		[g_url stopAccessingSecurityScopedResource];
	g_url = url;
	g_accessing = 1;
	memcpy(path, filesystem_path, strlen(filesystem_path) + 1);
	if (stale)
		XvtStoreBookmark(url, &message);
	if (error_capacity)
		error[0] = 0;

	return 1;
}
