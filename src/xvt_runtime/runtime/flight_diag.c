#include "xvt_runtime/runtime/flight_diag.h"
#include "xvt_runtime/runtime/flight_checkpoint.h"
#include "xvt_runtime/runtime/flight_internal.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/resync_task.h"
#include "xvt_runtime/timing/flight_timing.h"
#include <stdio.h>

static const char* const k_reason_names[XVT_FLIGHT_END_COUNT] = {
	"unspecified",
	"session lost (host left or link failed)",
	"host aborted the mission",
	"kicked by the host",
	"no world message from the host",
	"state request to the host timed out",
	"state recovery restarted too often",
	"state transfer from the host timed out",
	"host cannot recover its own input history",
	"checksum continuation queue exhausted",
	"world state restore failed",
	"world state checksum failed",
	"flight clock limit reached",
	"mission time limit",
	"no player still flying",
	"last flying player finished",
	"local player disconnected",
	"local player removed from the confirmed players",
	"local player left the mission",
	"proving grounds finished",
};

static const char* const k_drop_names[XVT_FLIGHT_DROP_COUNT] = {
	"invalid control packet (length, opcode or mission cookie)",
	"packet from a sender without a player slot",
	"invalid input batch (format or mission cookie)",
	"input batch from a player no longer connected",
	"input batch held during state recovery",
	"world message from a peer that is not the host",
};

static struct {
	XvtFlightEndReason reason;
	int detail, game_time, server_time;
	unsigned seen;
	unsigned drops[XVT_FLIGHT_DROP_COUNT];
	uint64_t start_us;
} g_diag;

static const char* XvtFlightDiag_Role(void) {
	if (!XvtFlightTiming_IsNetwork125())
		return "offline";
	return NetSession_GetLocalPlayerId() ? "host" : "client";
}

static const char* XvtFlightDiag_Name(int slot) {
	const char* name = (unsigned)slot < XVT_FLIGHT_PLAYERS ? NetSession_GetPlayerName(slot) : NULL;
	return name ? name : "?";
}

void XvtFlightDiag_Begin(void) {
	memset(&g_diag, 0, sizeof g_diag);
	g_diag.start_us = Aeron_NowUs();
}

void XvtFlightDiag_NoteEnd(XvtFlightEndReason reason, int detail) {
	if ((unsigned)reason >= XVT_FLIGHT_END_COUNT || reason == XVT_FLIGHT_END_NONE)
		return;
	/* Mission-end checks repeat every iteration once their condition holds. */
	if (g_diag.seen & (1u << reason))
		return;
	g_diag.seen |= 1u << reason;
	if (g_diag.reason == XVT_FLIGHT_END_NONE) {
		g_diag.reason = reason;
		g_diag.detail = detail;
		g_diag.game_time = g_gameTime;
		g_diag.server_time = g_serverTickTime;
		Aeron_LogWarn("xvt.flight", "flight ending: %s (detail %d) at tick %d, confirmed %d",
					  k_reason_names[reason], detail, g_gameTime, g_serverTickTime);
	} else {
		Aeron_LogInfo("xvt.flight", "flight ending, additional cause: %s (detail %d)", k_reason_names[reason],
					  detail);
	}
}

void XvtFlightDiag_CountDrop(XvtFlightDrop kind, int sender) {
	if ((unsigned)kind >= XVT_FLIGHT_DROP_COUNT)
		return;
	if (!g_diag.drops[kind]++)
		Aeron_LogWarn("xvt.network", "dropped %s from dpid %d (further drops counted at flight end)",
					  k_drop_names[kind], sender);
}

void XvtFlightDiag_PlayerFinished(int slot, unsigned flying) {
	Aeron_LogInfo("xvt.flight", "player %d '%s' stopped flying at tick %d; %u still flying", slot,
				  XvtFlightDiag_Name(slot), g_gameTime, flying);
}

void XvtFlightDiag_PlayerRemoved(int slot) {
	Aeron_LogInfo("xvt.flight", "player %d '%s' removed from the mission at tick %d", slot,
				  XvtFlightDiag_Name(slot), g_gameTime);
}

void XvtFlightDiag_LogStart(void) {
	g_diag.start_us = Aeron_NowUs();
	Aeron_LogInfo("xvt.flight", "flight started: %s, local slot %d, %s mode, mission cookie %08x",
				  XvtFlightDiag_Role(), g_localPlayer, g_asyncFlag ? "async" : "sync",
				  (unsigned)XvtFlightNetwork_Cookie());
	for (int slot = 0; slot < XVT_FLIGHT_PLAYERS; ++slot) {
		const PlayerData* player = &g_players[slot];
		if (!player->connectedFlag)
			continue;
		Aeron_LogInfo("xvt.flight", "  slot %d: dpid %d '%s'%s, flight group %u, object %d", slot,
					  player->network.directPlayId, XvtFlightDiag_Name(slot),
					  player->network.directPlayId == NetSession_GetHostDplayId() ? " (host)" : "",
					  (unsigned)player->boundFlightGroupIdx, player->objectIndex);
	}
}

void XvtFlightDiag_LogEnd(int phase) {
	char players[XVT_FLIGHT_PLAYERS * 32];
	size_t used = 0;
	for (int slot = 0; slot < XVT_FLIGHT_PLAYERS && used < sizeof players; ++slot) {
		const PlayerData* player = &g_players[slot];
		if (!player->connectedFlag && !g_playerAbortFlags[slot] && !player->network.directPlayId)
			continue;
		int written = snprintf(players + used, sizeof players - used, " %d:flag%u%s/silence%d", slot,
							   (unsigned)player->connectedFlag, g_playerAbortFlags[slot] ? "/aborted" : "",
							   g_flightNetPeerSilenceTicks[slot]);
		if (written < 0)
			break;
		used += (size_t)written;
	}
	players[used < sizeof players ? used : sizeof players - 1] = '\0';
	Aeron_LogWarn("xvt.flight", "flight ended from phase %d after %.1f s: %s (detail %d, noted at tick %d)",
				  phase, (double)(Aeron_NowUs() - g_diag.start_us) / 1e6, k_reason_names[g_diag.reason],
				  g_diag.detail, g_diag.game_time);
	Aeron_LogInfo("xvt.flight",
				  "  %s slot %d, tick %d, confirmed %d, input %d, host silence %d, players initial %02x "
				  "confirmed %02x, recovery %d, resync %d, queued %u/%u, mission end %d",
				  XvtFlightDiag_Role(), g_localPlayer, g_gameTime, g_serverTickTime, g_inputTimestamp,
				  g_flightNetHostTimeoutElapsedMs, XvtFlightCheckpoint_InitialMask(),
				  XvtFlightCheckpoint_ConfirmedMask(), XvtFlightNetwork_NeedsRecovery(), XvtResync_IsActive(),
				  XvtFlightMessages_Count(XVT_QUEUE_PENDING), XvtFlightMessages_Count(XVT_QUEUE_REPLAY),
				  g_flightMissionState.missionEndPending);
	Aeron_LogInfo("xvt.flight", "  players:%s", players);
	for (int kind = 0; kind < XVT_FLIGHT_DROP_COUNT; ++kind)
		if (g_diag.drops[kind])
			Aeron_LogInfo("xvt.network", "  dropped %u x %s", g_diag.drops[kind], k_drop_names[kind]);
}
