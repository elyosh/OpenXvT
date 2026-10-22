#include "xvt_runtime/config/config.h"
#include "xvt_runtime/runtime/flight_checkpoint.h"
#include "xvt_runtime/runtime/flight_diag.h"
#include "xvt_runtime/runtime/flight_internal.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/network_session.h"
#include "xvt_runtime/runtime/resync_task.h"
#include "xvt_runtime/snapshot/cockpit_capture.h"
#include "xvt_runtime/snapshot/render_capture.h"
#include "xvt_runtime/timing/flight_integration.h"
#include "xvt_runtime/timing/flight_timing.h"
#include "xvt_runtime/timing/player_timing.h"
#include "xvt_runtime/timing/reference_motion.h"

typedef enum XvtFlightPhase {
	XVT_FLIGHT_IDLE,
	XVT_FLIGHT_PREPARE,
	XVT_FLIGHT_SESSION,
	XVT_FLIGHT_DEVICES,
	XVT_FLIGHT_GLOBALS,
	XVT_FLIGHT_PALETTE,
	XVT_FLIGHT_MISSION_SETUP,
	XVT_FLIGHT_MISSION,
	XVT_FLIGHT_VOICES,
	XVT_FLIGHT_RESOURCES,
	XVT_FLIGHT_LOADING_COMPLETE,
	XVT_FLIGHT_RUNTIME,
	XVT_FLIGHT_FIRST_DELTA,
	XVT_FLIGHT_OPTIONS,
	XVT_FLIGHT_WORLD,
	XVT_FLIGHT_START,
	XVT_FLIGHT_FRAMES,
	XVT_FLIGHT_CLEANUP,
	XVT_FLIGHT_FADE,
	XVT_FLIGHT_DONE
} XvtFlightPhase;

static struct {
	XvtFlightPhase phase;
	char command[1024];
	int resources;
	int commitResults;
	int optionsFailed;
	int result;
	int released;
	int missionEntered;
} g_flight;

int XvtFlightTask_Begin(const char* command) {
	if (XvtFlightTask_IsActive() || !command)
		return 0;
	memset(&g_flight, 0, sizeof(g_flight));
	snprintf(g_flight.command, sizeof(g_flight.command), "%s", command);
	g_flight.phase = XVT_FLIGHT_PREPARE;
	XvtFlightSim_Reset();
	XvtFlightDiag_Begin();
	Aeron_LogInfo("xvt.flight", "launch started");
	return 1;
}

static void XvtFlightTask_ReleaseMission(int quitting) {
	if (g_flight.released)
		return;
	g_flight.released = 1;
	XvtResync_Reset();
	XvtFlightNetwork_BeginMission();
	XvtFlightFrame_ResetReplay();
	XvtRenderCapture_EndMission();
	XvtFlightTiming_EndSession();
	XvtFlightIntegration_Shutdown();
	XvtReferenceMotion_Shutdown();
	XvtPlayerTiming_Reset();
	Flight_FreeWorldStateBuffers();
	if (g_unusedFlightDebugLogFile) {
		File_Close(g_unusedFlightDebugLogFile);
		g_unusedFlightDebugLogFile = NULL;
	}
	if (g_flight.commitResults) {
		g_flightRenderTransitionHook();
		FeDiskIo_CommitFlightResults(0, 0);
		g_flight.commitResults = 0;
	}
	g_flightDisplaySurfacesActive = 0;
	if (g_flight.missionEntered)
		Sound_StopAllInstances();
	if (g_flight.optionsFailed)
		nullsub_10();
	if (g_flight.resources) {
		FeDiskIo_FreeModelResources();
		g_flight.resources = 0;
	}
	if (g_flight.missionEntered && !quitting && g_preFlightResolutionMode != g_flightResolutionMode)
		FlightDisplay_ApplyResolutionMode(g_preFlightResolutionMode);
	if (g_flight.optionsFailed)
		memcpy(&g_localPlayerSnapshotOnFlightExit, &g_players[g_localPlayer],
			   sizeof(g_localPlayerSnapshotOnFlightExit));
	else if (g_flight.result)
		Pilot_Save(0);
	XvtFlightSim_Reset();
}

static int XvtFlightTask_StartWorld(void) {
	int offline = atoi(g_flightLaunchArgs.arguments[FLIGHT_LAUNCH_ARG_NUM_PLAYERS]) == 1 &&
				  atoi(g_flightLaunchArgs.arguments[FLIGHT_LAUNCH_ARG_LOCAL_ID]) == 1 &&
				  !g_flightInProgressLaunch &&
				  XvtNetworkSession_GetStatus().state != XVT_NETWORK_SESSION_ESTABLISHED;
	XvtFlightTimingProfile profile =
		offline ? (XvtConfig_Settings()->flight_unlocked ? XVT_FLIGHT_TIMING_OFFLINE_UNLOCKED
														 : XVT_FLIGHT_TIMING_NATIVE)
				: XVT_FLIGHT_TIMING_NETWORK_125;
	int unlocked = profile != XVT_FLIGHT_TIMING_NATIVE;
	if (profile == XVT_FLIGHT_TIMING_NETWORK_125)
		g_gameTime = 0;
	if (g_regionMainObjectSlotEnd < 0 || g_regionStaticObjectSlotCount < 0)
		return 0;
	size_t capacity = (size_t)g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount;
	if (capacity > UINT16_MAX)
		return 0;
	XvtPlayerTiming_BeginWorld();
	if (unlocked && (!XvtFlightIntegration_Init(capacity) || !XvtReferenceMotion_Init(capacity))) {
		Aeron_LogWarn("xvt.flight.timing", "timing allocation failed%s",
					  profile == XVT_FLIGHT_TIMING_NETWORK_125 ? "; network mission cannot start"
															   : "; using native flight");
		XvtFlightIntegration_Shutdown();
		XvtReferenceMotion_Shutdown();
		if (profile == XVT_FLIGHT_TIMING_NETWORK_125)
			return 0;
		unlocked = 0;
		profile = XVT_FLIGHT_TIMING_NATIVE;
	}
	XvtFlightTiming_BeginSession(profile);
	XvtFlightNetwork_BeginMission();
	unsigned mask = 0;
	for (unsigned i = 0; i < 8; ++i)
		if (g_players[i].connectedFlag)
			mask |= 1u << i;
	XvtFlightCheckpoint_Begin((uint8_t)mask);
	g_flightStartupObjectPassState = 0;
	for (int i = 0; i < g_regionMainObjectSlotEnd; ++i)
		if (g_objectTable[i].mobj)
			g_objectTable[i].mobj->simStateTimestamp = 0;
	Flight_AllocWorldStateBuffers();
	if (!g_worldStateBuffer || !g_worldStateDupBuffer)
		return 0;
	FlightSync_ResetWorldMessageBufferCursor();
	Flight_SaveWorldState();
	if (!g_worldStateSize)
		return 0;
	if (XvtFlightTiming_IsNetwork125()) {
		Flight_ChecksumWorldState(0, 0);
		g_flightNetWorldChecksumEpoch = 0;
		FlightSync_SnapshotWorldStateForReplay();
		g_flightNetBufferWorldMessagesUntilChecksum = 1;
	}
	FlightView_RenderStartupFrame();
	NetSession_StubReturnTrue();
	return 1;
}

void XvtFlightTask_Tick(void) {
	XvtFlightPhase previous = g_flight.phase;
	if (XvtNetworkSession_IsLost() && g_flight.phase < XVT_FLIGHT_CLEANUP) {
		XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_SESSION_LOST, NetSession_GetHostDplayId());
		XvtResync_Reset();
		g_flight.result = 0;
		g_flight.phase = XVT_FLIGHT_CLEANUP;
	}
	XvtResync_Tick();
	if (XvtResync_IsActive())
		return;
	switch (g_flight.phase) {
		case XVT_FLIGHT_PREPARE: {
			int status = XvtFlightEntry_Prepare(g_flight.command);
			g_flight.phase = status == XVT_FLIGHT_NETWORK_PENDING ? XVT_FLIGHT_SESSION
							 : status                             ? XVT_FLIGHT_DEVICES
																  : XVT_FLIGHT_CLEANUP;
			break;
		}
		case XVT_FLIGHT_SESSION: {
			int status = XvtFlightNetwork_Session();
			if (status != XVT_FLIGHT_NETWORK_PENDING)
				g_flight.phase = status ? XVT_FLIGHT_DEVICES : XVT_FLIGHT_CLEANUP;
			break;
		}
		case XVT_FLIGHT_DEVICES:
			g_flight.phase = XvtFlightEntry_CreateDevices() ? XVT_FLIGHT_GLOBALS : XVT_FLIGHT_CLEANUP;
			break;
		case XVT_FLIGHT_GLOBALS:
			g_flight.missionEntered = 1;
			XvtFlightLoading_Globals();
			g_flight.phase = XVT_FLIGHT_PALETTE;
			break;
		case XVT_FLIGHT_PALETTE:
			g_flight.resources = 1;
			XvtFlightLoading_Palette();
			g_flight.phase = XVT_FLIGHT_MISSION_SETUP;
			break;
		case XVT_FLIGHT_MISSION_SETUP:
			XvtFlightLoading_MissionSetup();
			g_flight.phase = XVT_FLIGHT_MISSION;
			break;
		case XVT_FLIGHT_MISSION:
			FlightSurface_Lock();
			XvtRenderCapture_BeginMission();
			Mission_Init(g_currentMissionFile);
			FlightSurface_Unlock();
			g_flight.phase = XVT_FLIGHT_VOICES;
			break;
		case XVT_FLIGHT_VOICES:
			fsfx_LoadMissionVoiceSfx();
			g_flight.phase = XVT_FLIGHT_RESOURCES;
			break;
		case XVT_FLIGHT_RESOURCES:
			FeDiskIo_InitResources();
			g_flight.phase = XVT_FLIGHT_LOADING_COMPLETE;
			break;
		case XVT_FLIGHT_LOADING_COMPLETE:
			/* The original finishes this progress cycle without doing more loading. */
			g_flightLoadingProgressStep |= 0x7f;
			FlightLoading_PulseAndDrawProgressScreen();
			g_flight.phase = XVT_FLIGHT_RUNTIME;
			break;
		case XVT_FLIGHT_RUNTIME:
			XvtFlightLoading_Runtime();
			g_flight.phase = XVT_FLIGHT_FIRST_DELTA;
			break;
		case XVT_FLIGHT_FIRST_DELTA:
			if (!XvtCockpit_LoadingAssetsReady())
				break;
			g_inputTimestamp += Time_GetFrameDelta();
			if (g_inputTimestamp) {
				Object_RelinkMobileObjectPointers();
				g_flight.phase = XVT_FLIGHT_OPTIONS;
			}
			break;
		case XVT_FLIGHT_OPTIONS: {
			int status = FlightNet_SyncPlayerOptionsAndTaunts();
			if (status == XVT_FLIGHT_NETWORK_PENDING)
				break;
			if (status)
				g_flight.phase = XVT_FLIGHT_WORLD;
			else {
				g_flight.optionsFailed = 1;
				g_flight.phase = XVT_FLIGHT_CLEANUP;
			}
			break;
		}
		case XVT_FLIGHT_WORLD:
			g_flight.phase = XvtFlightTask_StartWorld() ? XVT_FLIGHT_START : XVT_FLIGHT_CLEANUP;
			break;
		case XVT_FLIGHT_START: {
			int status;
			g_flight.commitResults = 1;
			g_flight.result = 1;
			status = FlightNet_WaitForMissionStart();
			if (status == XVT_FLIGHT_NETWORK_PENDING)
				break;
			if (status) {
				XvtFlightFrame_Begin();
				XvtFlightDiag_LogStart();
				g_flight.phase = XVT_FLIGHT_FRAMES;
			} else
				g_flight.phase = XVT_FLIGHT_CLEANUP;
			break;
		}
		case XVT_FLIGHT_FRAMES:
			if (XvtFlightFrame_Tick())
				g_flight.phase = XVT_FLIGHT_CLEANUP;
			break;
		case XVT_FLIGHT_CLEANUP:
			XvtFlightTask_ReleaseMission(0);
			if (g_musicCdMciDeviceId && g_gameConfig.musicEnabled && g_gameConfig.musicVolume) {
				unsigned int volume = UINT16_MAX * g_gameConfig.musicVolume / 9;
				XvtCdTask_BeginFade(volume, volume / 8, 1000);
			}
			g_flight.phase = XVT_FLIGHT_FADE;
			break;
		case XVT_FLIGHT_FADE:
			if (XvtCdTask_IsFading())
				break;
			MusicCd_CloseDevice();
			XvtFlightEntry_Cleanup();
			g_flight.phase = XVT_FLIGHT_DONE;
			break;
		default:
			break;
	}
	if (previous != g_flight.phase)
		Aeron_LogInfo("xvt.flight", "phase %d -> %d", previous, g_flight.phase);
	if (previous < XVT_FLIGHT_CLEANUP && g_flight.phase == XVT_FLIGHT_CLEANUP)
		XvtFlightDiag_LogEnd(previous);
}

int XvtFlightTask_IsActive(void) {
	return g_flight.phase > XVT_FLIGHT_IDLE && g_flight.phase < XVT_FLIGHT_DONE;
}

int XvtFlightTask_IsLoading(void) {
	/* WORLD produces the first view and advances to START in the same tick. */
	return g_flight.phase > XVT_FLIGHT_IDLE && g_flight.phase <= XVT_FLIGHT_WORLD;
}

int XvtFlightTask_IsComplete(void) { return g_flight.phase == XVT_FLIGHT_DONE; }

int XvtFlightTask_GetResult(void) { return g_flight.result; }

int XvtFlightTask_ContinuesWithoutFocus(void) {
	return XvtFlightTask_IsActive() && g_activeFlightPlayerCount > 1;
}

uint64_t XvtFlightTask_NextWakeDelayUs(void) {
	if (XvtResync_IsActive() || XvtResync_HoldsInput())
		return XvtResync_NextWakeDelayUs();
	if (g_flight.phase == XVT_FLIGHT_FRAMES)
		return XvtFlightFrame_NextWakeDelayUs();
	if (g_flight.phase == XVT_FLIGHT_FIRST_DELTA)
		return XvtFlightTime_DelayForTicks(1);
	if (g_flight.phase == XVT_FLIGHT_FADE)
		return XvtCdTask_NextWakeDelayUs();
	return UINT64_MAX;
}

void XvtFlightTask_Shutdown(void) {
	Aeron_SetRelativeMouseMode(0);
	XvtResync_Reset();
	XvtFlightNetwork_Reset();
	if (XvtFlightTask_IsActive()) {
		XvtFlightTask_ReleaseMission(1);
		XvtCdTask_Cancel();
		MusicCd_CloseDevice();
		XvtFlightEntry_Cleanup();
	}
	memset(&g_flight, 0, sizeof(g_flight));
}
