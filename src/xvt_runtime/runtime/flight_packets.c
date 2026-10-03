#include "xvt_runtime/runtime/flight_internal.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/resync_task.h"

enum { XVT_STARTUP_CLOCK_PROBES = 3 };

/* Startup probes measure the lead before the first checksum-driven probe. The
 * smallest sample is least affected by slow first flight frames. */
static struct {
	int active, samples, valid, lead;
} g_startupProbes;

void XvtFlightNetwork_BeginClockProbes(void) {
	g_startupProbes.active = 1;
	g_startupProbes.samples = g_startupProbes.valid = 0;
	FlightNet_SendClockProbeToHost();
}

static int XvtFlightNetwork_Control(int senderDpid, int* packet) {
	enum {
		PLAYER_COUNT = XVT_FLIGHT_PLAYERS,
		WORLD_STATE_CHUNK_COUNT = XVT_RESYNC_CHUNKS_PER_BATCH,
		PACKET_CLOCK_PROBE_REPLY_SIZE = 2 * sizeof(int),
		CLOCK_PROBE_LIMIT_MS = 472
	};

	switch (packet[0]) {
		case NET_PACKET_PLAYER_DISCONNECTED: {
			int playerIndex = packet[1];

			if (playerIndex >= 0 && playerIndex < PLAYER_COUNT) {
				g_playerConnected[playerIndex] = 0;
			}
			return 0;
		}
		case NET_PACKET_WORLD_CHECKSUM:
			if (g_players[NetSession_FindPlayerSlotByDpid(senderDpid)].connectedFlag)
				XvtResync_DeferChecksum(senderDpid, packet);
			return 0;
		case NET_PACKET_SESSION_ABORT:
			g_flightNetHostAbortReceived = 1;
			g_flightMissionState.missionEndPending = 1;
			g_players[g_localPlayer].connectedFlag = 0;
			return 1;
		case NET_PACKET_RESYNC_CHUNK_ACK: {
			unsigned int chunkIndex;

			g_flightNetWorldStateAckReceivedFlag = 1;
			chunkIndex = (unsigned int)packet[1];
			if (chunkIndex < WORLD_STATE_CHUNK_COUNT) {
				g_flightNetWorldStateChunkAcked[chunkIndex] = 1;
			}
			return 0;
		}
		case NET_PACKET_PLAYER_ABORT: {
			int playerIndex = packet[1];
			if (senderDpid != NetSession_GetHostDplayId() &&
				((unsigned)playerIndex >= XVT_FLIGHT_PLAYERS ||
				 g_players[playerIndex].network.directPlayId != senderDpid))
				return 0;
			if (XvtFlightNetwork_PlayerAbort((unsigned)playerIndex))
				return 0;

			if (playerIndex >= 0 && playerIndex < PLAYER_COUNT) {
				g_playerAbortFlags[playerIndex] = 1;
			}
			if (playerIndex != g_localPlayer) {
				return 0;
			}
			g_flightMissionState.missionEndPending = 1;
			g_players[g_localPlayer].connectedFlag = 0;
			g_playerAbortFlags[g_localPlayer] = 1;
			FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
			return 1;
		}
		case NET_PACKET_RESYNC_NOTICE:
			g_flightNetResyncPlayerDplayId = packet[1];
			return 0;
		case NET_PACKET_SERVER_CHECKSUM:
			FlightSync_HandleServerChecksumPacket((uint8_t*)packet);
			/* The host receives its own checksum broadcast; probing itself would move its lead. */
			if (!NetSession_GetLocalPlayerId())
				FlightNet_SendClockProbeToHost();
			return 0;
		case NET_PACKET_ACK:
			if (g_flightNetPendingAckCount != 0) {
				--g_flightNetPendingAckCount;
				if (g_flightNetPendingAckCount == 0) {
					g_flightNetNextClientInputSendTimestamp = 0;
					return 1;
				}
			}
			return 0;
		case NET_PACKET_CLOCK_LEAD:
			g_flightNetClockLeadAllowanceMs = packet[1];
			return 0;
		case NET_PACKET_STILL_LOADING:
			if (NetSession_GetHostDplayId() == senderDpid) {
				g_flightNetHostTimeoutElapsedMs = 0;
			} else {
				int playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);

				if (g_players[playerIndex].connectedFlag != 0 &&
					g_flightNetPeerSilenceTicks[playerIndex] > 0) {
					g_flightNetPeerSilenceTicks[playerIndex] = 0;
				}
			}
			return 0;
		case NET_PACKET_CLOCK_PROBE:
			/* The host keeps its fixed lead; it only echoes the probe so clients can
			 * measure their round trip. */
			g_flightNetScratchPacket.packetType = NET_PACKET_CLOCK_PROBE_REPLY;
			g_flightNetScratchPacket.payloadDwords[0] = packet[1];
			XvtFlightNetwork_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
										PACKET_CLOCK_PROBE_REPLY_SIZE);
			return 0;
		case NET_PACKET_CLOCK_PROBE_REPLY:
			if (NetSession_GetLocalPlayerId() == 0 && packet[1] == g_flightNetClockProbeTimestamp) {
				int adjustment;
				int targetLead;

				targetLead = g_flightNetClockAdjustAccumTicks;
				targetLead += g_inputTimestamp;
				targetLead -= packet[1];
				targetLead += XVT_CLOCK_PROBE_BIAS_TICKS;

				if (g_startupProbes.active) {
					if (targetLead < CLOCK_PROBE_LIMIT_MS &&
						(!g_startupProbes.valid || targetLead < g_startupProbes.lead)) {
						g_startupProbes.lead = targetLead;
						g_startupProbes.valid = 1;
					}
					if (++g_startupProbes.samples < XVT_STARTUP_CLOCK_PROBES) {
						FlightNet_SendClockProbeToHost();
					} else {
						g_startupProbes.active = 0;
						if (g_startupProbes.valid)
							g_flightNetClockLeadAllowanceMs = g_startupProbes.lead;
					}
				} else if (targetLead < CLOCK_PROBE_LIMIT_MS) {
					if (g_flightNetClockLeadAllowanceMs < targetLead) {
						adjustment = (targetLead - g_flightNetClockLeadAllowanceMs) >> 1;
						if (adjustment == 0) {
							adjustment = 1;
						}
						g_flightNetClockLeadAllowanceMs += adjustment;
					} else if (g_flightNetClockLeadAllowanceMs > targetLead) {
						adjustment = (g_flightNetClockLeadAllowanceMs - targetLead) >> 1;
						if (adjustment == 0) {
							adjustment = 1;
						}
						g_flightNetClockLeadAllowanceMs -= adjustment;
					}
				}
			}
			return 0;
		default:
			return 0;
	}
}

void XvtFlightNetwork_ProcessPackets(void) {
	if (!XvtFlightNetwork_Cookie() || !g_players[g_localPlayer].connectedFlag)
		return;
	int currentTimestamp = g_inputTimestamp + (int)Time_GetFrameDelta();
	while (XvtFlightNetwork_TakePacketBudget()) {
		int sender, size;
		int* packet = NetSession_ReceiveGamePacket(&sender, &size);
		currentTimestamp += (int)Time_GetFrameDelta();
		if (!packet) {
			if (!NetSession_GetLocalPlayerId() || !XvtFlightNetwork_ShouldSend(g_inputTimestamp))
				break;
			XvtFlightNetwork_SendWorld();
			currentTimestamp += (int)Time_GetFrameDelta();
			continue;
		}
		if (!XvtFlightNetwork_DecodeControl((const uint8_t*)packet, &size) ||
			(unsigned)NetSession_FindPlayerSlotByDpid(sender) >= XVT_FLIGHT_PLAYERS)
			continue;
		if (XvtFlightNetwork_Receive(sender, (const uint8_t*)packet, size) ||
			XvtResync_ReceivePacket(sender, (const uint8_t*)packet, size))
			continue;
		if (XvtFlightNetwork_Control(sender, packet))
			return;
	}
	g_inputTimestamp = currentTimestamp + (int)Time_GetFrameDelta();
}
