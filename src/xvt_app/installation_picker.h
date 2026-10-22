#ifndef XVT_APP_INSTALLATION_PICKER_H
#define XVT_APP_INSTALLATION_PICKER_H

#include "aeron/scene/ui_file_picker.h"

#include <SDL3/SDL_platform_defines.h>

#include <stddef.h>

/* iOS stores a security-scoped bookmark and keeps the folder readable for
 * the process. Other platforms have no grant to restore. */
#if defined(SDL_PLATFORM_IOS)
int XvtInstallationPicker_OpenNative(char* error, size_t capacity);
AeronUiFilePickerResult XvtInstallationPicker_PollNative(char* path, size_t capacity, char* error,
														 size_t error_capacity);
int XvtInstallationPicker_NativeOpen(void);
void XvtInstallationPicker_CancelNative(void);
int XvtInstallationPicker_Restore(char* path, size_t capacity, char* error, size_t error_capacity);
#else
static inline int XvtInstallationPicker_Restore(char* path, size_t capacity, char* error,
												size_t error_capacity) {
	(void)path;
	(void)capacity;
	(void)error;
	(void)error_capacity;

	return 0;
}
#endif

#endif
