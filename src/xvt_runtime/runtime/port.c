#include "xvt_runtime/runtime/port.h"

#include "aeron/aeron.h"
#include "aeron/compat/dplay.h"
#include "aeron/compat/host.h"
#include "xvt/flight/flight.h"
#include "xvt/net/net.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/input_bridge.h"
#include "xvt_runtime/runtime/campaign_task.h"
#include "xvt_runtime/runtime/cd_task.h"
#include "xvt_runtime/runtime/dialog_task.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/frontend_task.h"
#include "xvt_runtime/runtime/launch_task.h"
#include "xvt_runtime/runtime/movie_task.h"
#include "xvt_runtime/runtime/network_session.h"
#include "xvt_runtime/runtime/network_task.h"
#include "xvt_runtime/runtime/presentation.h"
#include "xvt_runtime/snapshot/render_snapshot.h"
#include "xvt_runtime/storage/storage.h"
#include "xvt_runtime/timing/host_clock.h"

static int g_xvtInitialized;
static int g_xvtPaused;
static int g_xvtRebaseClock;
static int g_xvtExitCode;
static int g_skipIntro;
static int g_quitting;
static int g_settingsOpen, g_settingsRequested, g_simulationHeld;

void XvtPort_SetSettingsOpen(int open) { g_settingsOpen = open != 0; }

void XvtPort_SetSimulationHeld(int held) { g_simulationHeld = held != 0; }

void XvtPort_RequestSettings(void) { g_settingsRequested = 1; }

int XvtPort_ConsumeSettingsRequest(void) {
	int requested = g_settingsRequested;
	g_settingsRequested = 0;
	return requested;
}

int XvtPort_NetworkRequiresProgress(void) {
	return AeronDplay_IsActive() || XvtNetworkTask_IsActive() || XvtNetworkTask_BrowserVisible() ||
		   XvtFlightTask_ContinuesWithoutFocus();
}

void XvtPort_SetSkipIntro(int skip_intro) {
	if (!g_xvtInitialized)
		g_skipIntro = skip_intro != 0;
}

static void XvtPort_CommitSnapshot(int movie_presented) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	XvtSceneKind kind = XVT_SCENE_FRONTEND;
	if (g_quitting)
		kind = XVT_SCENE_NONE;
	else if (movie_presented || XvtMovieTask_IsActive())
		kind = XVT_SCENE_MOVIE;
	else if (XvtFlightTask_IsActive()) {
		if (XvtDialog_IsActive())
			kind = XVT_SCENE_FRONTEND_MODAL;
		else
			kind = XvtFlightTask_IsLoading() ? XVT_SCENE_LOADING : XVT_SCENE_FLIGHT;
	}
	XvtRenderSnapshot_SetSceneKind(kind);
	XvtRenderSnapshot_Commit(g_gameTime, input && input->has_focus, g_xvtPaused);
}

int XvtPort_Init(void) {
	int width;
	int height;
	if (g_xvtInitialized)
		return 1;
	g_xvtExitCode = 0;
	g_quitting = 0;
	if (!Aeron_GetLogicalSize(&width, &height) || width != XVT_CLASSIC_WIDTH ||
		height != XVT_CLASSIC_HEIGHT) {
		Aeron_LogError("xvt.port", "runtime requires an initialized 640x480 Aeron host");
		g_xvtExitCode = 1;
		return 0;
	}
	XvtTime_Reset();
	XvtPresentation_Init();
	AeronCompat_SetJoystickSource(NULL, NULL);
	AeronCompat_SetRumbleProvider(NULL);
	g_xvtPaused = 0;
	/* Startup and focus-resume intervals do not belong to virtual game time. */
	g_xvtRebaseClock = 1;
	g_xvtInitialized = 1;
	XvtInput_Init();
	XvtRenderSnapshot_BeginTick();
	XvtRenderSnapshot_SetSceneKind(XVT_SCENE_FRONTEND);
	if (!XvtFrontendTask_Init(g_skipIntro)) {
		XvtPort_CommitSnapshot(0);
		g_xvtExitCode = 1;
		XvtPort_Shutdown();
		return 0;
	}
	XvtPort_CommitSnapshot(0);
	Aeron_LogInfo("xvt.port", "runtime initialized");
	return 1;
}

int XvtPort_IsInitialized(void) { return g_xvtInitialized; }

void XvtPort_PausedFrame(void) {
	int movieActive;
	if (!g_xvtInitialized)
		return;
	XvtRenderSnapshot_BeginTick();
	if (!g_xvtPaused) {
		g_xvtPaused = 1;
		Aeron_AudioSetPaused(1);
		Aeron_LogInfo("xvt.port", "paused");
	}
	g_xvtRebaseClock = 1;
	AeronCompat_Update(1);
	XvtInput_Update(1);
	movieActive = XvtMovieTask_IsActive();
	if (movieActive)
		XvtMovieTask_PausedFrame();
	XvtPresentation_EndFrame(movieActive);
	XvtPort_CommitSnapshot(movieActive);
}

void XvtPort_Tick(int32_t delta_us) {
	const AeronInputSnapshot* input;
	int movieActive;
	if (XvtPort_ShouldQuit())
		return;
	XvtRenderSnapshot_BeginTick();
	input = Aeron_InputSnapshot();
	XvtInput_UpdateMouseCapture(input);
	AeronDplay_Update();
	if (g_quitting) {
		XvtNetworkSession_Service();
		XvtPort_CommitSnapshot(0);
		return;
	}
	XvtNetworkTask_ServiceBrowser();
	XvtMovieTask_ReapFinished();
	if (!XvtPort_NetworkRequiresProgress() &&
		(g_settingsOpen || g_simulationHeld ||
		 ((!input || !input->has_focus) && !XvtMovieTask_ContinuesWithoutFocus() &&
		  !XvtCampaignTask_ContinuesWithoutFocus()))) {
		XvtNetworkSession_Service();
		XvtPort_PausedFrame();
		return;
	}
	if (g_xvtPaused) {
		g_xvtPaused = 0;
		Aeron_AudioSetPaused(0);
		Aeron_LogInfo("xvt.port", "resumed");
	}
	/* Capture once per host frame; discard stale edges on startup and resume. */
	AeronCompat_Update(g_xvtRebaseClock || XvtInput_IsCaptured() || !input || !input->has_focus);
	if (XvtFlightTask_IsActive())
		XvtInput_UpdateFlight(g_xvtRebaseClock);
	else
		XvtInput_Update(g_xvtRebaseClock || XvtDialog_HasResult());
	if (g_xvtRebaseClock)
		g_xvtRebaseClock = 0;
	else
		XvtTime_AdvanceHostClock(delta_us);
	if (XvtFlightTask_IsActive())
		XvtCdTask_Tick();
	else
		XvtFrontendTask_ServiceFrameSystems();
	movieActive = XvtMovieTask_IsActive();
	if (movieActive)
		XvtMovieTask_Tick();
	else if (XvtFlightTask_IsActive())
		XvtFlightTask_Tick();
	else
		XvtFrontendTask_Tick();
	if (XvtFlightTask_IsComplete()) {
		int result = XvtFlightTask_GetResult();
		XvtFlightTask_Shutdown();
		XvtLaunchTask_Complete(result);
		g_xvtRebaseClock = 1;
	}
	if (XvtLaunchTask_HasPendingLaunch()) {
		XvtNetworkSession_BeginFlight();
		const char* command = XvtLaunchTask_BeginPendingLaunch();
		if (!XvtFlightTask_Begin(command))
			XvtLaunchTask_Complete(0);
		g_xvtRebaseClock = 1;
	}
	XvtNetworkSession_Service();
	XvtPresentation_EndFrame(movieActive);
	XvtPort_CommitSnapshot(movieActive);
}

int XvtPort_ShouldQuit(void) {
	if (!g_xvtInitialized || Aeron_FatalErrorRequested())
		return 1;
	if (!g_quitting && (Aeron_QuitRequested() || XvtFrontendTask_ShouldQuit())) {
		g_quitting = 1;
		XvtNetworkTask_Shutdown();
		Net_ShutdownDirectPlaySessionForQuit();
	}
	return g_quitting && !AeronDplay_IsActive();
}

int XvtPort_GetExitCode(void) { return Aeron_FatalErrorRequested() ? 1 : g_xvtExitCode; }

uint64_t XvtPort_NextWakeDelayUs(void) {
	uint64_t task;
	uint64_t cd;
	if (g_quitting)
		return AeronDplay_NextWakeDelayUs();
	if (g_xvtPaused)
		return UINT64_MAX;
	task = XvtMovieTask_IsActive()    ? XvtMovieTask_NextWakeDelayUs()
		   : XvtFlightTask_IsActive() ? XvtFlightTask_NextWakeDelayUs()
									  : XvtFrontendTask_NextWakeDelayUs();
	cd = XvtCdTask_NextWakeDelayUs();
	if (cd < task)
		task = cd;
	cd = AeronDplay_NextWakeDelayUs();
	return cd < task ? cd : task;
}

void XvtPort_Shutdown(void) {
	XvtPresentation_RequireClassic();
	if (!g_xvtInitialized)
		return;
	XvtRenderSnapshot_BeginTick();
	XvtRenderSnapshot_SetSceneKind(XVT_SCENE_NONE);
	XvtMovieTask_Shutdown();
	XvtFlightTask_Shutdown();
	XvtFrontendTask_Shutdown();
	AeronDplay_Shutdown();
	XvtNetworkSession_Shutdown();
	XvtInput_Shutdown();
	AeronCompat_SetJoystickSource(NULL, NULL);
	AeronCompat_SetRumbleProvider(NULL);
	AeronWinmm_Shutdown();
	XvtPresentation_Shutdown();
	Aeron_SetRelativeMouseMode(0);
	Aeron_SetHostCursorVisible(1);
	Aeron_AudioSetPaused(0);
	XvtRenderSnapshot_Commit(g_gameTime, 0, 0);
	XvtTime_Reset();
	g_xvtInitialized = 0;
	g_xvtPaused = 0;
	g_xvtRebaseClock = 0;
	g_settingsOpen = g_settingsRequested = g_simulationHeld = 0;
	Aeron_LogInfo("xvt.port", "runtime shut down");
}
