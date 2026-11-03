#include "xvt_app/setup_ui.h"

#include "xvt_app/installation_picker.h"
#include "xvt_runtime/config/config.h"
#include <stdio.h>

typedef enum XvtSetupFrameResult {
	XVT_SETUP_FRAME_PENDING,
	XVT_SETUP_FRAME_SUCCESS,
	XVT_SETUP_FRAME_CANCELLED,
} XvtSetupFrameResult;

#if !defined(SDL_PLATFORM_IOS)
static int XvtSetupUi_AcceptInstallation(const char* path, void* user, char* error, size_t capacity) {
	(void)user;
	char resolved[XVT_PATH_CAPACITY];

	return XvtSetup_ResolveInstallation(path, resolved, sizeof resolved, error, capacity);
}
#endif

int XvtSetupUi_OpenPicker(AeronUiFilePicker* picker, const char* path, char* error, size_t capacity) {
#if defined(SDL_PLATFORM_IOS)
	(void)picker;
	(void)path;

	return XvtInstallationPicker_OpenNative(error, capacity);
#else
	const AeronUiFilePickerDesc desc = {
		.mode = AERON_UI_FILE_PICKER_SELECT_DIRECTORY,
		.title = "SELECT XvT INSTALLATION",
		.instructions = "Select the XvT installation folder or its BalanceOfPower folder.",
		.accept_label = "Use This Folder",
		.cancel_label = "Cancel",
		.initial_path = path && path[0] ? path : NULL,
		.accept_fn = XvtSetupUi_AcceptInstallation,
	};
	return AeronUiFilePicker_Open(picker, &desc, error, capacity);
#endif
}

AeronUiFilePickerResult XvtSetupUi_DrawPicker(AeronUiFilePicker* picker, AeronUiContext* ui, char* path,
											  size_t path_capacity, char* error, size_t error_capacity) {
#if defined(SDL_PLATFORM_IOS)
	(void)picker;
	(void)ui;

	return XvtInstallationPicker_PollNative(path, path_capacity, error, error_capacity);
#else
	return AeronUiFilePicker_Draw(picker, ui, path, path_capacity, error, error_capacity);
#endif
}

int XvtSetupUi_PickerIsOpen(const AeronUiFilePicker* picker) {
#if defined(SDL_PLATFORM_IOS)
	(void)picker;

	return XvtInstallationPicker_NativeOpen();
#else
	return AeronUiFilePicker_IsOpen(picker);
#endif
}

void XvtSetupUi_CancelPicker(AeronUiFilePicker* picker) {
#if defined(SDL_PLATFORM_IOS)
	(void)picker;
	XvtInstallationPicker_CancelNative();
#else
	AeronUiFilePicker_Cancel(picker);
#endif
}

static XvtSetupFrameResult XvtSetupUi_DrawRecovery(AeronUiContext* ui, char* error, size_t capacity) {
	XvtSetupFrameResult result = XVT_SETUP_FRAME_PENDING;
	const AeronUiWindowDesc window = { .width_ref = 920.0f, .centered = 1 };
	if (!AeronUi_BeginWindow(ui, "OpenXvT configuration", &window))
		return result;
	AeronUi_Help(ui, "The saved configuration could not be loaded.");
	AeronUi_Error(ui, error);
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Keep file and quit"))
		result = XVT_SETUP_FRAME_CANCELLED;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Reset to defaults") && XvtConfig_Replace(error, capacity))
		result = XVT_SETUP_FRAME_SUCCESS;
	AeronUi_EndColumns(ui);
	AeronUi_EndWindow(ui);
	return result;
}

static XvtSetupFrameResult XvtSetupUi_DrawInstallation(AeronUiContext* ui, AeronUiFilePicker* picker,
													   char* path, size_t path_capacity, int* valid,
													   char* error, size_t capacity) {
	XvtSetupFrameResult result = XVT_SETUP_FRAME_PENDING;
	const AeronUiWindowDesc window = { .width_ref = 920.0f, .centered = 1 };
	if (!AeronUi_BeginWindow(ui, "Welcome to OpenXvT", &window))
		return result;
	AeronUi_Help(ui, "Select your original X-Wing vs. TIE Fighter installation with Balance of Power.");
	AeronUi_Spacer(ui, 8.0f);
	AeronUi_Header(ui, "Original Installation");
	uint32_t path_result = AeronUi_InputTextWithAction(ui, "XvT", path, path_capacity,
													   AERON_UI_INPUT_TEXT_READ_ONLY, "Browse...");
	if (path_result & AERON_UI_INPUT_TEXT_ACTION_ACTIVATED)
		XvtSetupUi_OpenPicker(picker, path, error, capacity);
	AeronUi_Help(ui, "Choose the XvT installation folder or its BalanceOfPower folder.");
	if (*valid)
		AeronUi_Help(ui, "Installation validated.");
	if (error[0])
		AeronUi_Error(ui, error);
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Quit"))
		result = XVT_SETUP_FRAME_CANCELLED;
	AeronUi_NextColumn(ui);
	if (AeronUi_ButtonEnabled(ui, "Continue", *valid)) {
		*valid = XvtSetup_ResolveInstallation(path, path, path_capacity, error, capacity);
		if (*valid && XvtConfig_SetGameData(path, 1, error, capacity))
			result = XVT_SETUP_FRAME_SUCCESS;
	}
	AeronUi_EndColumns(ui);
	AeronUi_EndWindow(ui);
	return result;
}

XvtSetupResult XvtSetupUi_Run(XvtAppUi* ui, char* path, size_t path_capacity, char* error, size_t capacity) {
	AeronUiContext* context = XvtAppUi_Context(ui);
	AeronUiFilePicker* picker = path ? AeronUiFilePicker_Create() : NULL;
	if (path && !picker) {
		snprintf(error, capacity, "Cannot create installation picker.");
		return XVT_SETUP_ERROR;
	}
	int valid = path && path[0] && XvtSetup_ResolveInstallation(path, path, path_capacity, error, capacity);
	XvtSetupResult outcome = XVT_SETUP_ERROR;
	Aeron_SetHostCursorVisible(1);
	while (!Aeron_QuitRequested() && !Aeron_FatalErrorRequested()) {
		const int32_t delta_us = Aeron_BeginFrame();
		if (Aeron_QuitRequested() || Aeron_FatalErrorRequested())
			break;
		AeronUi_BeginFrame(context, &(AeronUiFrameDesc) {
										.input = Aeron_InputSnapshot(),
										.dt_seconds = (float)delta_us * 1e-6f,
									});
		const XvtSetupFrameResult frame_result =
			path ? XvtSetupUi_DrawInstallation(context, picker, path, path_capacity, &valid, error, capacity)
				 : XvtSetupUi_DrawRecovery(context, error, capacity);
		if (picker) {
			const AeronUiFilePickerResult picked =
				XvtSetupUi_DrawPicker(picker, context, path, path_capacity, error, capacity);
			if (picked == AERON_UI_FILE_PICKER_SELECTED) {
				valid = XvtSetup_ResolveInstallation(path, path, path_capacity, error, capacity);
			}
		}
		const AeronUiOutput output = AeronUi_EndFrame(context);
		if (!AeronUi_Submit(context) || !Aeron_Present()) {
			snprintf(error, capacity, "Could not render the installation setup dialog.");
			break;
		}
		if (frame_result != XVT_SETUP_FRAME_PENDING) {
			outcome = frame_result == XVT_SETUP_FRAME_SUCCESS ? XVT_SETUP_SUCCESS : XVT_SETUP_CANCELLED;
			break;
		}
		if (output.cancel_pressed) {
			outcome = XVT_SETUP_CANCELLED;
			break;
		}
		Aeron_WaitForNextFrame(16667);
	}
	if (Aeron_FatalErrorRequested()) {
		snprintf(error, capacity, "Setup interrupted by a fatal host error.");
		outcome = XVT_SETUP_ERROR;
	} else if (Aeron_QuitRequested()) {
		outcome = XVT_SETUP_CANCELLED;
	}
	AeronUiFilePicker_Destroy(picker);
	if (outcome == XVT_SETUP_SUCCESS)
		error[0] = 0;
	return outcome;
}
