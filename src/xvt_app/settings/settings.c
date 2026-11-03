#include "xvt_app/settings/settings.h"
#include "xvt_app/touch_controls.h"
#include "xvt_app/settings/controller_page.h"
#include "xvt_app/settings/installation_page.h"
#include "xvt_app/settings/keyboard_page.h"
#include "xvt_app/settings/mouse_page.h"
#include "xvt_app/settings/video_options.h"
#include "xvt_app/settings/video_page.h"
#include "xvt_remaster/config.h"
#include "xvt_runtime/config/config.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/controller_mapping.h"
#include "xvt_runtime/runtime/dialog_task.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/port.h"
#include "xvt_runtime/timing/flight_timing.h"
#include <stdio.h>
#include <string.h>

static struct {
	AeronUiContext* ui;
	XvtSettingsPageFn pages[5];
	bool open, close_requested, captures_controller, ready;
	int page;
	int exit_confirmation_open;
	char error[1024];
	XvtControllerSettings controller;
	XvtKeyboardSettings keyboard;

	struct {
		uint32_t instance;
		bool down, armed;
	} start[AERON_CONTROLLER_MAX];
} g_menu;

static void XvtSettingsMenu_DrawGame(AeronUiContext* ui, const AeronInputSnapshot* input) {
	static const char* const rates[] = { "Native (31.25 Hz)", "Unlocked" };
	int skip_intro = XvtConfig_Settings()->skip_intro;
	AeronUi_Header(ui, "Startup");
	if (AeronUi_Toggle(ui, "Skip intro cutscenes on launch", &skip_intro)) {
		char error[1024];
		if (!XvtConfig_SetSkipIntro(skip_intro != 0, error, sizeof error))
			XvtSettingsMenu_ReportError(error);
	}
	AeronUi_Spacer(ui, 8.0f);
	int unlocked = XvtConfig_Settings()->flight_unlocked;
	AeronUi_Header(ui, "Flight");
	if (AeronUi_Selector(ui, "Flight Rate", &unlocked, rates, 2)) {
		char error[1024];
		if (!XvtConfig_SetFlightRate(unlocked != 0, error, sizeof error))
			XvtSettingsMenu_ReportError(error);
	}
	AeronUi_Help(ui, "Applies to the next mission.");
	if (XvtFlightTask_IsActive() && !XvtFlightTask_IsLoading()) {
		XvtFlightTimingProfile profile = XvtFlightTiming_Profile();
		AeronUi_Help(ui, profile == XVT_FLIGHT_TIMING_NETWORK_125        ? "Active mission: 125 Hz"
						 : profile == XVT_FLIGHT_TIMING_OFFLINE_UNLOCKED ? "Active mission: Unlocked"
																		 : "Active mission: 31.25 Hz");
	}
	AeronUi_Spacer(ui, 8.0f);
	XvtInstallationPage_Draw(ui, input);
}

static void XvtSettingsMenu_DrawController(AeronUiContext* ui, const AeronInputSnapshot* input) {
	XvtTouchControls_DrawMenu(ui, input);
	XvtControllerSettings_Draw(&g_menu.controller, ui, input);
}

static void XvtSettingsMenu_DrawKeyboard(AeronUiContext* ui, const AeronInputSnapshot* input) {
	(void)input;
	XvtKeyboardSettings_Draw(&g_menu.keyboard, ui);
}

bool XvtSettingsMenu_Init(XvtAppUi* ui, char* error, size_t capacity) {
	memset(&g_menu, 0, sizeof g_menu);
	g_menu.ui = XvtAppUi_Context(ui);
	g_menu.pages[0] = XvtSettingsMenu_DrawGame;
	g_menu.pages[1] = XvtVideoPage_Draw;
	g_menu.pages[2] = XvtSettingsMenu_DrawController;
	g_menu.pages[3] = XvtSettingsMenu_DrawKeyboard;
	g_menu.pages[4] = XvtMousePage_Draw;
	XvtVideoOptions_Configure(XvtRemasterConfig_ApplyVideo);
	g_menu.ready = XvtInstallationPage_Init(error, capacity);
	return g_menu.ready;
}

void XvtSettingsMenu_FlushForExit(void) {
	if (!g_menu.ready)
		return;
	char error[1024];
	if (!XvtVideoOptions_Flush(true, error, sizeof error))
		Aeron_LogError("xvt.settings", "%s", error);
	if (g_menu.open) {
		if (!XvtKeyboardSettings_Commit(&g_menu.keyboard, error, sizeof error))
			Aeron_LogError("xvt.settings", "%s", error);
		if (!XvtControllerSettings_Commit(&g_menu.controller, error, sizeof error))
			Aeron_LogError("xvt.settings", "%s", error);
		if (!XvtInstallationPage_Flush(error, sizeof error))
			Aeron_LogError("xvt.settings", "%s", error);
	}
	if (!XvtConfig_Save(error, sizeof error))
		Aeron_LogError("xvt.settings", "%s", error);
}

void XvtSettingsMenu_Shutdown(void) {
	XvtInstallationPage_Shutdown();
	XvtKeyboardSettings_CancelCapture(&g_menu.keyboard, g_menu.ui);
	if (g_menu.ui)
		AeronUi_CancelControllerCapture(g_menu.ui);
	memset(&g_menu, 0, sizeof g_menu);
}

void XvtSettingsMenu_SetPage(int page, XvtSettingsPageFn draw) {
	if ((unsigned)page < 5)
		g_menu.pages[page] = draw;
}

bool XvtSettingsMenu_Open(void) { return g_menu.open; }

bool XvtSettingsMenu_CapturesKeyboard(void) {
	return g_menu.open && AeronUi_KeyboardCaptureActive(g_menu.ui);
}

bool XvtSettingsMenu_CapturesController(void) { return g_menu.open && g_menu.captures_controller; }

void XvtSettingsMenu_Show(void) {
	if (g_menu.ui && !g_menu.open) {
		g_menu.open = true;
		XvtInstallationPage_Open();
		XvtControllerSettings_Open(&g_menu.controller, XvtConfig_Settings());
		XvtKeyboardSettings_Open(&g_menu.keyboard, XvtConfig_Settings());
		g_menu.error[0] = 0;
		XvtInput_SetCaptured(true);
		XvtPort_SetSettingsOpen(1);
		Aeron_SetHostCursorVisible(1);
	}
}

void XvtSettingsMenu_RequestClose(void) {
	if (g_menu.open)
		g_menu.close_requested = true;
}

bool XvtSettingsMenu_CloseRequested(void) { return g_menu.close_requested; }

void XvtSettingsMenu_CompleteClose(void) {
	XvtKeyboardSettings_CancelCapture(&g_menu.keyboard, g_menu.ui);
	XvtControllerSettings_CancelCapture(&g_menu.controller, g_menu.ui);
	XvtInstallationPage_CancelPicker();
	AeronUi_CancelControllerCapture(g_menu.ui);
	g_menu.open = g_menu.close_requested = g_menu.captures_controller = false;
	g_menu.exit_confirmation_open = 0;
	XvtPort_SetSettingsOpen(0);
}

void XvtSettingsMenu_ReportError(const char* error) {
	snprintf(g_menu.error, sizeof g_menu.error, "%s", error);
	g_menu.close_requested = false;
}

static void XvtSettingsMenu_DrawExitConfirmation(AeronUiContext* ui) {
	if (!AeronUi_BeginModal(ui, "EXIT GAME", &g_menu.exit_confirmation_open, NULL))
		return;
	AeronUi_Help(ui, "Exit OpenXvT and return to the desktop?");
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Cancel"))
		g_menu.exit_confirmation_open = 0;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Exit Game##exit-confirmation")) {
		g_menu.exit_confirmation_open = 0;
		Aeron_RequestQuit();
	}
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
}

void XvtSettingsMenu_Frame(const AeronInputSnapshot* input, float seconds) {
	if (!g_menu.open || !input)
		return;
	static const char* pages[] = { "Game", "Video", "Controller", "Keyboard", "Mouse" };
	AeronUi_BeginFrame(g_menu.ui, &(AeronUiFrameDesc) { .input = input, .dt_seconds = seconds });
	if (AeronUi_BeginWindow(g_menu.ui, "OpenXvT SETTINGS",
							&(AeronUiWindowDesc) { .width_ref = 980, .height_ref = 1000, .centered = 1 })) {
		AeronUi_BeginTabBar(g_menu.ui, "pages", pages, 5, &g_menu.page);
		if (g_menu.page != 2) {
			XvtControllerSettings_CancelCapture(&g_menu.controller, g_menu.ui);
			g_menu.controller.editor.binding_modal_open = 0;
			g_menu.controller.restore_modal_open = 0;
		}
		if (g_menu.page != 3)
			XvtKeyboardSettings_CancelCapture(&g_menu.keyboard, g_menu.ui);
		{
			const AeronUiTheme* theme = AeronUi_GetTheme(g_menu.ui);
			const float gap = theme ? theme->item_spacing : 10.0f;
			const float row = theme ? theme->row_height : 40.0f;
			/* Tabs stay put. Close and Exit stay on the last rows. The page
			 * body scrolls instead of running off the window. */
			float footer = 4.0f + gap + 9.0f + gap + row + gap + gap;
			float body;
			if (XvtPort_NetworkRequiresProgress())
				footer += AeronUi_MeasureHelpHeight(g_menu.ui, "Multiplayer continues while settings are open.",
													0.0f) +
						  gap;
			if (g_menu.error[0])
				footer += AeronUi_MeasureHelpHeight(g_menu.ui, g_menu.error, 0.0f) + gap;
			body = AeronUi_AvailableHeight(g_menu.ui) - footer;
			if (body < 120.0f)
				body = 120.0f;
			if (AeronUi_BeginScroll(g_menu.ui, "settings-page", body)) {
				if (g_menu.pages[g_menu.page])
					g_menu.pages[g_menu.page](g_menu.ui, input);
				AeronUi_EndScroll(g_menu.ui);
			}
		}
		AeronUi_EndTabBar(g_menu.ui);
		if (XvtPort_NetworkRequiresProgress())
			AeronUi_Help(g_menu.ui, "Multiplayer continues while settings are open.");
		if (g_menu.error[0])
			AeronUi_Error(g_menu.ui, g_menu.error);
		AeronUi_Separator(g_menu.ui);
		if (g_menu.page == 0) {
			AeronUi_BeginColumns(g_menu.ui, 2, NULL);
			if (AeronUi_Button(g_menu.ui, "Exit Game"))
				g_menu.exit_confirmation_open = 1;
			AeronUi_NextColumn(g_menu.ui);
			if (AeronUi_Button(g_menu.ui, "Close"))
				XvtSettingsMenu_RequestClose();
			AeronUi_EndColumns(g_menu.ui);
		} else if (AeronUi_Button(g_menu.ui, "Close")) {
			XvtSettingsMenu_RequestClose();
		}
		if (g_menu.page == 2)
			XvtControllerSettings_DrawModals(&g_menu.controller, g_menu.ui, input, XvtConfig_Settings());
		if (g_menu.page == 3)
			XvtKeyboardSettings_DrawModals(&g_menu.keyboard, g_menu.ui);
		if (g_menu.exit_confirmation_open)
			XvtSettingsMenu_DrawExitConfirmation(g_menu.ui);
		AeronUi_EndWindow(g_menu.ui);
	}
	XvtInstallationPage_DrawPicker(g_menu.ui);
	AeronUiOutput output = AeronUi_EndFrame(g_menu.ui);
	g_menu.captures_controller = output.capture_all != 0;
	if (output.cancel_pressed)
		XvtSettingsMenu_RequestClose();
	AeronUi_Submit(g_menu.ui);
}

static bool XvtSettingsMenu_ControllerStart(const AeronInputSnapshot* input) {
	if (!input) {
		memset(g_menu.start, 0, sizeof g_menu.start);
		return false;
	}
	const XvtControllerOptions* options = XvtControllerMapping_Options();
	bool pressed = false;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		const AeronControllerSnapshot* device = &input->controllers[i];
		int model = device->connected ? XvtControllerOptions_FindModel(options, device->guid) : -1;
		bool eligible = model >= 0 && options->models[model].kind == device->kind &&
						device->kind == AERON_CONTROLLER_KIND_GAMEPAD;
		uint32_t instance = eligible ? device->instance_id : 0;
		bool down = eligible && (device->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_START));
		if (instance != g_menu.start[i].instance || !input->has_focus) {
			g_menu.start[i].instance = instance;
			g_menu.start[i].armed = !down;
		} else if (!down)
			g_menu.start[i].armed = true;
		else if (g_menu.start[i].armed && !g_menu.start[i].down)
			pressed = true;
		g_menu.start[i].down = down;
	}
	return pressed;
}

bool XvtSettingsMenu_BeginFrame(const AeronInputSnapshot* input) {
	bool was_open = g_menu.open;
	if (g_menu.open)
		XvtControllerSettings_Discover(&g_menu.controller, g_menu.ui, input,
									   &XvtConfig_DefaultSettings()->gamepad_defaults);
	XvtControllerMapping_ApplyPending();
	char apply_error[1024];
	if (!XvtVideoOptions_ApplyPending(apply_error, sizeof apply_error))
		XvtSettingsMenu_ReportError(apply_error);
	if (g_menu.close_requested) {
		char error[1024];
		if (XvtVideoOptions_Flush(false, error, sizeof error) &&
			XvtControllerSettings_Commit(&g_menu.controller, error, sizeof error) &&
			XvtKeyboardSettings_Commit(&g_menu.keyboard, error, sizeof error) &&
			XvtInstallationPage_Flush(error, sizeof error) && XvtConfig_Save(error, sizeof error))
			XvtSettingsMenu_CompleteClose();
		else
			XvtSettingsMenu_ReportError(error);
	}
	bool opened = false;
	bool start = XvtSettingsMenu_ControllerStart(input);
	if (input && input->has_focus && !XvtDialog_IsActive()) {

		if (!g_menu.close_requested && start && !g_menu.captures_controller &&
			!XvtSettingsMenu_CapturesKeyboard() && !XvtInstallationPage_PickerOpen() &&
			(g_menu.open || !XvtFlightTask_IsActive())) {
			if (g_menu.open)
				XvtSettingsMenu_RequestClose();
			else if (!was_open) {
				XvtSettingsMenu_Show();
				opened = true;
			}
		}
	}
	if (input && input->has_focus && !was_open && !g_menu.open &&
		XvtKeyboardMapping_Trigger(input, XVT_KEYBOARD_SHORTCUT_SETTINGS) >= 0) {
		XvtSettingsMenu_Show();
		opened = true;
	}
	XvtInput_BeginCaptureFrame(input, was_open || g_menu.open || !input || !input->has_focus);
	XvtControllerMapping_Update(input);
	if (was_open && !g_menu.open)
		Aeron_SetHostCursorVisible(!XvtFlightTask_IsActive());
	return opened;
}

bool XvtSettingsMenu_ConsumeRuntimeRequest(void) {
	if (!XvtPort_ConsumeSettingsRequest())
		return false;
	bool was_open = g_menu.open;
	XvtSettingsMenu_Show();
	return !was_open && g_menu.open;
}
