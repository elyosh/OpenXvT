#include "xvt_app/settings/installation_page.h"
#include "aeron/scene/ui_file_picker.h"
#include "xvt_app/settings/settings.h"
#include "xvt_app/setup_ui.h"
#include "xvt_runtime/config/config.h"
#include <stdio.h>
#include <string.h>
static AeronUiFilePicker* g_picker;
static char g_path[XVT_PATH_CAPACITY], g_accepted[XVT_PATH_CAPACITY];

bool XvtInstallationPage_Init(char* error, size_t capacity) {
	g_picker = AeronUiFilePicker_Create();
	if (!g_picker)
		snprintf(error, capacity, "Cannot create installation picker");
	return g_picker != NULL;
}

void XvtInstallationPage_Shutdown(void) {
	AeronUiFilePicker_Destroy(g_picker);
	g_picker = NULL;
}

void XvtInstallationPage_Open(void) {
	snprintf(g_accepted, sizeof g_accepted, "%s",
			 XvtConfig_Settings()->game_data[0] ? XvtConfig_Settings()->game_data : XvtSetup_Installation());
	snprintf(g_path, sizeof g_path, "%s", g_accepted);
}

void XvtInstallationPage_CancelPicker(void) { XvtSetupUi_CancelPicker(g_picker); }

bool XvtInstallationPage_PickerOpen(void) { return XvtSetupUi_PickerIsOpen(g_picker) != 0; }

void XvtInstallationPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
	(void)input;
	AeronUi_Header(ui, "Original Installation");
	uint32_t result = AeronUi_InputTextWithAction(ui, "XvT", g_path, sizeof g_path, 0, "Browse...");
	if (result & AERON_UI_INPUT_TEXT_ACTION_ACTIVATED) {
		char error[1024];
		if (!XvtSetupUi_OpenPicker(g_picker, g_path, error, sizeof error))
			XvtSettingsMenu_ReportError(error);
	}
	if (strcmp(g_path, XvtSetup_Installation())) {
		char message[XVT_PATH_CAPACITY + 100];
		snprintf(message, sizeof message, "Current installation: %s", XvtSetup_Installation());
		AeronUi_Help(ui, message);
		AeronUi_Help(ui, "Requires restart.");
	}
}

void XvtInstallationPage_DrawPicker(AeronUiContext* ui) {
	char selected[XVT_PATH_CAPACITY], error[1024];
	AeronUiFilePickerResult result =
		XvtSetupUi_DrawPicker(g_picker, ui, selected, sizeof selected, error, sizeof error);
	if (result == AERON_UI_FILE_PICKER_SELECTED) {
		if (!XvtSetup_ResolveInstallation(selected, g_path, sizeof g_path, error, sizeof error))
			XvtSettingsMenu_ReportError(error);
	} else if (result == AERON_UI_FILE_PICKER_ERROR)
		XvtSettingsMenu_ReportError(error);
}

bool XvtInstallationPage_Flush(char* error, size_t capacity) {
	if (!strcmp(g_path, g_accepted))
		return true;
	if (!XvtSetup_ResolveInstallation(g_path, g_path, sizeof g_path, error, capacity) ||
		!XvtConfig_SetGameData(g_path, 0, error, capacity))
		return false;
	snprintf(g_accepted, sizeof g_accepted, "%s", g_path);
	return true;
}
