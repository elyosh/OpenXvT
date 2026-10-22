#include "xvt_app/application.h"

#include "aeron/compat/host.h"
#include "xvt_app/ios_screen.h"
#include "aeron/log.h"
#include "xvt_app/settings/settings.h"
#include "xvt_app/setup.h"
#include "xvt_app/touch_controls.h"
#include "xvt_app/ui.h"
#include "xvt_remaster/xvt_remaster.h"
#include "xvt_runtime/config/config.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/controller_mapping.h"
#include "xvt_runtime/input/keyboard_mapping.h"
#include "xvt_runtime/runtime/port.h"
#include "xvt_runtime/snapshot/render_snapshot.h"
#include "xvt_runtime/storage/storage.h"
#include <stdio.h>

static uint64_t XvtApplication_PresentationIntervalUs(void) {
	double rate = Aeron_PresentationRate();
	/* Aeron normally supplies the display rate, including its 60 Hz fallback. */
	if (!(rate >= 1.0 && rate <= 1000.0))
		rate = 60.0;
	return (uint64_t)(1000000.0 / rate + 0.5);
}

static void XvtApplication_DiscoverControllers(const AeronInputSnapshot* input) {
	static char previous_error[512];
	const XvtSettings* settings = XvtConfig_Settings();
	if (!settings || XvtSettingsMenu_Open())
		return;
	XvtControllerOptions candidate = settings->controller;
	AeronInputSnapshot filtered;
	char error[512] = { 0 };
	XvtTouchControls_Omit(&filtered, input);
	bool ok = XvtControllerOptions_InitializeGamepads(
		&candidate, &XvtConfig_DefaultSettings()->gamepad_defaults, &filtered, error, sizeof error);
	if (!XvtControllerOptions_Equals(&candidate, &settings->controller)) {
		char save_error[512];
		if (XvtConfig_SetController(&candidate, save_error, sizeof save_error))
			XvtControllerMapping_SetOptions(&XvtConfig_Settings()->controller);
		else {
			snprintf(error, sizeof error, "%s", save_error);
			ok = false;
		}
	}
	if (!ok && strcmp(previous_error, error))
		Aeron_LogWarn("xvt.input", "%s", error);
	snprintf(previous_error, sizeof previous_error, "%s", ok ? "" : error);
}

static int XvtApplication_FrameLoop(void) {
	while (!XvtPort_ShouldQuit()) {
		XvtIosScreen_Fill();
		int32_t delta_us = Aeron_BeginFrame();
		uint64_t wake_delay_us;
		uint64_t task_delay_us;
		if (XvtPort_ShouldQuit())
			break;
		const AeronInputSnapshot* input = Aeron_InputSnapshot();
		XvtApplication_DiscoverControllers(input);
		XvtTouchControls_Sync(input);
		bool menu_opened = XvtSettingsMenu_BeginFrame(input);
		int debug_key = XvtKeyboardMapping_Trigger(input, XVT_KEYBOARD_SHORTCUT_DEBUG);
		if (debug_key >= 0 && !XvtSettingsMenu_CapturesKeyboard()) {
			Aeron_DebugUiToggle();
			XvtInput_SuppressKey(debug_key);
		}
		XvtRemaster_BeginFrame(input);
		XvtPort_Tick(delta_us);
		menu_opened |= XvtSettingsMenu_ConsumeRuntimeRequest();
		if (XvtPort_ShouldQuit())
			break;
		XvtRemaster_Frame(delta_us);
		if (!XvtSettingsMenu_Open())
			XvtTouchControls_Draw();
		if (!menu_opened)
			XvtSettingsMenu_Frame(input, (float)delta_us / 1000000.0f);
		if (!Aeron_Present()) {
			Aeron_RequestFatalRendererError("frame presentation");
			break;
		}
		wake_delay_us = XvtApplication_PresentationIntervalUs();
		task_delay_us = XvtPort_NextWakeDelayUs();
		if (task_delay_us < wake_delay_us)
			wake_delay_us = task_delay_us;
		Aeron_WaitForNextFrame(wake_delay_us);
	}
	return XvtPort_GetExitCode();
}

int XvtApplication_Run(const XvtLaunchOptions* options) {
	AeronConfig config;
	XvtAppUi ui = { 0 };
	int exit_code = 1;
	char error[1024] = { 0 };
	if (options->check_installation) {
		char resource_root[XVT_PATH_CAPACITY];
		if (!XvtHostConfig_ResolveResourceRoot(options, resource_root, sizeof(resource_root))) {
			fprintf(stderr, "OpenXvT: cannot resolve application resources.\n");
			return 1;
		}
		AeronVfsConfig vfs_config = { .org_name = "TotallyOpen",
									  .app_name = "OpenXvT",
									  .resource_root = resource_root };
		AeronVfs* vfs = AeronVfs_Create(&vfs_config);
		XvtStorage_Bind(vfs);
		int success = vfs && (XvtSetup_Run(options, NULL, error, sizeof(error)) == XVT_SETUP_SUCCESS);
		if (!success)
			fprintf(stderr, "OpenXvT: %s\n", error);
		XvtConfig_Shutdown();
		XvtStorage_Bind(NULL);
		AeronVfs_Destroy(vfs);
		return success ? 0 : 1;
	}
	XvtHostConfig_InitAeron(options, &config);
	Aeron_LogInfo("xvt.app", "initializing OpenXvT %s host", OPENXVT_VERSION);
	if (!Aeron_Init(&config)) {
		/* Aeron unwinds its own partial initialization; shutdown is idempotent. */
		Aeron_Shutdown();
		return 1;
	}
	XvtStorage_Bind(Aeron_GetVfs());
	XvtIosScreen_Fill();
	XvtSetupResult setup_result = XvtSetup_Run(options, &ui, error, sizeof error);
	if (setup_result != XVT_SETUP_SUCCESS) {
		if (setup_result == XVT_SETUP_CANCELLED)
			exit_code = 0;
		else
			Aeron_LogError("xvt.setup", "%s", error);
		goto cleanup;
	}
	XvtRenderSnapshot_Init();
	if (!XvtSettingsMenu_Init(&ui, error, sizeof error)) {
		Aeron_LogError("xvt.settings", "%s", error);
		goto cleanup;
	}
	XvtTouchControls_Init(&ui.font);
	if (!XvtRemaster_Init())
		goto cleanup;
	AeronWinmmCdAudioDesc cd = { Aeron_GetVfs(), AERON_VFS_ROOT_ASSET, "BalanceOfPower/MUSIC" };
	if (!AeronWinmm_ConfigureCdAudio(&cd)) {
		Aeron_LogError("xvt.setup", "Cannot configure installed CD music tracks");
		goto cleanup;
	}
	XvtPort_SetSkipIntro(options->skip_intro || XvtConfig_Settings()->skip_intro);
	if (XvtPort_Init()) {
		XvtIosScreen_Fill();
		Aeron_LogInfo("xvt.app", "host ready");
		exit_code = XvtApplication_FrameLoop();
	}
cleanup:
	XvtTouchControls_Shutdown();
	XvtSettingsMenu_FlushForExit();
	XvtSettingsMenu_Shutdown();
	XvtPort_Shutdown();
	XvtRemaster_Shutdown();
	XvtRenderSnapshot_Shutdown();
	XvtAppUi_Shutdown(&ui);
	if (XvtPort_GetExitCode())
		exit_code = XvtPort_GetExitCode();
	AeronWinmm_Shutdown();
	XvtConfig_Shutdown();
	XvtStorage_Bind(NULL);
	Aeron_Shutdown();
	Aeron_LogInfo("xvt.app", "host stopped (exit %d)", exit_code);
	return exit_code;
}
