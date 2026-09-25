#include "xvt/net/flight_net.h"
#ifdef XVT_MODERN
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/resync_task.h"
#endif
#include "xvt/assets/file.h"

#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/hud/flight_alert.h"
#include "xvt/flight/player/player.h"
#include "xvt/frontend/config.h"
#include "xvt/frontend/pilot_record.h"
#include "xvt/net/flight_sync.h"
#include "xvt/net/net_reliable.h"
#include "xvt/net/net_session.h"
#include "xvt/render/flight_sw.h"
#include "xvt/util/time.h"
#include <stdio.h>
#include <string.h>

// GLOBAL: XVT 0x5242DC
int g_flightNetResyncPlayerDplayId = 0;
// GLOBAL: XVT 0x5242E0
int g_flightNetPendingAckCount = 0;
// GLOBAL: XVT 0x5242E4
int g_flightNetNextClientInputSendTimestamp;
// GLOBAL: XVT 0x5242E8
int g_flightNetLastSentWorldMessageTimestamp;
// GLOBAL: XVT 0x5242EC
static int g_unusedFlightNetMissionStartAckInitFlag;
// GLOBAL: XVT 0x52340C
int g_flightNetClockAdjustAccumTicks = 0;
// GLOBAL: XVT 0x52342C
int g_flightNetHostAbortReceived = 0;
// GLOBAL: XVT 0x523418
int dtMs = 29;
// GLOBAL: XVT 0x557358
int g_flightNetRecoveryUiActive = 0;
// GLOBAL: XVT 0x557360
FlightNetScratchPacket g_flightNetScratchPacket = { 0 };
// GLOBAL: XVT 0x556ED0
int g_flightNetClockProbeTimestamp = 0;
// GLOBAL: XVT 0x556ED8
int g_flightNetPeerSilenceTicks[8] = { 0 };
// GLOBAL: XVT 0x556EFC
int g_flightNetRecoveryUiBlinkTime = 0;
// GLOBAL: XVT 0x55734C
int g_flightNetRecoverySavedInputTimestamp = 0;
// GLOBAL: XVT 0x9A8C2C
int g_inputTimestamp = 0;
// GLOBAL: XVT 0xA07CC8
int g_serverTickTime = 0;
// GLOBAL: XVT 0xA07BB0
int g_playerConnected[8] = { 0 };
// GLOBAL: XVT 0x9D8A30
int g_playerAbortFlags[8] = { 0 };
// GLOBAL: XVT 0xA082A8
FlightInputFrameRecord g_currentInputFrame = { 0 };
// GLOBAL: XVT 0x559788
int g_lastFrameTime = 0;
// GLOBAL: XVT 0x557348
int g_lastKeyframeTime = 0;
// GLOBAL: XVT 0x557148
FlightNetInputDeltaBatchPacket g_flightNetInputDeltaBatchPacket = { 0 };
// GLOBAL: XVT 0x557354
int g_flightNetInputDeltaBatchLen = 0;
// GLOBAL: XVT 0x557560
int g_flightNetLastInputDeltaCodeByPlayer[8] = { 0 };
// GLOBAL: XVT 0x556EF8
int g_flightNetLastInputBatchSendTime = 0;
// GLOBAL: XVT 0x557580
int g_flightNetInputBatchIntervalTicks = 0;
// GLOBAL: XVT 0x5242C4
int g_flightNetSmallSessionPlayerThreshold = 3;
// GLOBAL: XVT 0x5242CC
int g_flightNetReceivedWorldMessageCount = 0;
// GLOBAL: XVT 0x5242D0
int g_inputLogEnabled = 0;
// GLOBAL: XVT 0x5242D4
XvtFile* g_inputLogFile = NULL;
// GLOBAL: XVT 0x5242C8
int g_flightNetSentWorldMessageCount = 0;
// GLOBAL: XVT 0x557350
int g_flightNetWorldChecksumResetAccumMs = 0;
// GLOBAL: XVT 0x9D77D0
int g_flightNetWorldChecksumPeerStatus[8] = { 0 };
// GLOBAL: XVT 0x5242D8
XvtFile* g_flightNetServerLogFile = NULL;
// GLOBAL: XVT 0x9A8C24
int g_flightNetHostTimeoutElapsedMs = 0;
// GLOBAL: XVT 0x556F00
int g_flightNetWorldStateAckReceivedFlag = 0;
// GLOBAL: XVT 0x556F08
int g_flightNetWorldStateChunkAcked[16] = { 0 };
// GLOBAL: XVT 0x556F48
int g_flightNetLocalResyncChecksums[126] = { 0 };
// GLOBAL: XVT 0x557588
int g_flightNetRemoteResyncChecksums[126] = { 0 };
// GLOBAL: XVT 0x557788
FlightNetWorldStateChunkPacket g_flightNetWorldStateChunkPackets[16] = { { 0 } };
// GLOBAL: XVT 0x55978C
int g_flightNetRemoteResyncChecksumsReceivedFlag = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x462A10
char* FlightNet_GetStatusPlayerName(void) {
	int playerDplayId;
	int playerSlot;

	playerDplayId = g_flightNetResyncPlayerDplayId;
	if (playerDplayId == 0) {
		playerDplayId = NetSession_GetHostDplayId();
	}
	g_flightNetResyncPlayerDplayId = playerDplayId;
	playerSlot = NetSession_FindPlayerSlotByDpid(playerDplayId);
	if (g_playerAbortFlags[playerSlot] != 0) {
		g_flightNetResyncPlayerDplayId = NetSession_GetHostDplayId();
		playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
	}
	if (g_players[playerSlot].connectedFlag == 0) {
		g_flightNetResyncPlayerDplayId = NetSession_GetHostDplayId();
		playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
	}
	return NetSession_GetPlayerName(playerSlot);
}

// FUNCTION: XVT 0x463160
int FlightNet_SyncPlayerOptionsAndTaunts(void) {
#ifdef XVT_MODERN
	return XvtFlightNetwork_Options();
#else
	int activePlayers;
	int* packet;
	int senderDpid;
	uint32_t statusUpdateTime;
	int receivedPlayerCount;
	int packetSize;
	uint32_t lastPacketTime;
	char statusText[256];
	int blinkState;
	int hostDplayId;
	int playerIndex;
	uint32_t currentTime;
	char* playerName;
	const char* loadingSuffix;

	blinkState = 1;
	playerIndex = 0;
	statusUpdateTime = playerIndex;
	hostDplayId = NetSession_GetHostDplayId();
	if (g_activeFlightPlayerCount > 1) {
		if (NetSession_GetLocalPlayerId() != 0) {
			if (g_activeFlightPlayerCount > playerIndex) {
				int remainingPlayers = g_activeFlightPlayerCount;
				do {
					g_players[playerIndex].network.flightResolutionMode = FLIGHT_RESOLUTION_320X240;
					g_players[playerIndex].pilotRating = 0;
					++playerIndex;
					--remainingPlayers;
				} while (remainingPlayers != 0);
			}
			NetSession_CountActivePlayers();
			g_players[g_localPlayer].network.flightResolutionMode = g_flightResolutionMode;
			g_players[g_localPlayer].pilotRating = g_pilotData.rating;

			FlightAlert_SaveBoxBackground();
			FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_WAITING_FOR_OTHER_PLAYERS], 0x30);
			receivedPlayerCount = 0;
			lastPacketTime = timeGetTime();
			while (NetSession_CountActivePlayers() - 1 > receivedPlayerCount) {
				packet = NetSession_WaitForGamePacket(&senderDpid, &packetSize, 60);
				currentTime = timeGetTime();
				if (packet != NULL) {
					lastPacketTime = currentTime;
					if (*packet == NET_PACKET_STILL_LOADING) {
						currentTime = timeGetTime();
						if ((int)(currentTime - statusUpdateTime) > 200) {
							statusUpdateTime = currentTime;
							playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
							playerName = NetSession_GetPlayerName(playerIndex);
							if (playerName == NULL) {
								strcpy(statusText,
									   g_strDiskIoMessages[DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING]);
							} else {
								strcpy(statusText, playerName);
								blinkState = !blinkState;
								if (blinkState) {
									loadingSuffix =
										g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_MINUS];
								} else {
									loadingSuffix =
										g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_PLUS];
								}
								strcat(statusText, loadingSuffix);
							}
							FlightAlert_DrawBox(3, statusText, 0x30);
						}
					}
					if (*packet == NET_PACKET_PLAYER_OPTIONS) {
						playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
						g_players[playerIndex].network.flightResolutionMode = packet[1];
						g_players[playerIndex].pilotRating = packet[2];
						++receivedPlayerCount;
					}
				} else if (currentTime - lastPacketTime > 60000) {
					return 0;
				}
			}
			FlightAlert_RestoreBoxBackground();

			{
				g_flightNetScratchPacket.payloadDwords[0] = g_flightConfNewNet;
				g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_OPTIONS_ROSTER;
				for (playerIndex = 0; playerIndex < g_activeFlightPlayerCount; ++playerIndex) {
					g_flightNetScratchPacket.payloadDwords[1 + playerIndex * 2] =
						g_players[playerIndex].network.flightResolutionMode;
					g_flightNetScratchPacket.payloadDwords[2 + playerIndex * 2] =
						g_players[playerIndex].pilotRating;
				}
			}

			NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket,
												8 * g_activeFlightPlayerCount + 8);
			do {
				packet = NetSession_WaitForGamePacket(&senderDpid, &packetSize, 60);
				if (packet == NULL) {
					return 0;
				}
			} while (*packet != NET_PACKET_PLAYER_OPTIONS_ROSTER);
		} else {
			g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_OPTIONS;
			g_flightNetScratchPacket.payloadDwords[0] = g_flightResolutionMode;
			g_flightNetScratchPacket.payloadDwords[1] = g_pilotData.rating;

			NetSession_SendPacket(hostDplayId, (unsigned int*)&g_flightNetScratchPacket, 12);
			FlightAlert_SaveBoxBackground();
			FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_WAITING_FOR_OTHER_PLAYERS], 0x30);
			do {
				packet = NetSession_WaitForGamePacket(&senderDpid, &packetSize, 60);
				if (packet == NULL) {
					return 0;
				}
				if (*packet == NET_PACKET_STILL_LOADING) {
					currentTime = timeGetTime();
					if ((int)(currentTime - statusUpdateTime) > 200) {
						statusUpdateTime = currentTime;
						playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
						playerName = NetSession_GetPlayerName(playerIndex);
						if (playerName == NULL) {
							strcpy(statusText, g_strDiskIoMessages[DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING]);
						} else {
							strcpy(statusText, playerName);
							blinkState = !blinkState;
							if (blinkState) {
								loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_MINUS];
							} else {
								loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_PLUS];
							}
							strcat(statusText, loadingSuffix);
						}
						FlightAlert_DrawBox(3, statusText, 0x30);
					}
				}
			} while (*packet != NET_PACKET_PLAYER_OPTIONS_ROSTER);
			FlightAlert_RestoreBoxBackground();
			{
				int remainingPlayers;
				g_flightConfNewNet = packet[1];
				remainingPlayers = g_activeFlightPlayerCount;
				playerIndex = 0;
				if (remainingPlayers > 0) {
					do {
						g_players[playerIndex].network.flightResolutionMode = packet[2 + playerIndex * 2];
						g_players[playerIndex].pilotRating = packet[3 + playerIndex * 2];
						++playerIndex;
						--remainingPlayers;
					} while (remainingPlayers != 0);
				}
			}
		}

		g_flightNetScratchPacket.payloadDwords[0] = g_localPlayer;
		g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_TAUNTS;
		memcpy(&g_flightNetScratchPacket.payloadDwords[1], g_gameConfig.taunts, sizeof(g_gameConfig.taunts));

		NetSession_SendPacket(0, (unsigned int*)&g_flightNetScratchPacket, 8 + sizeof(g_gameConfig.taunts));
		receivedPlayerCount = 0;
		NetSession_CountActivePlayers();
		for (;;) {
			activePlayers = NetSession_CountActivePlayers();
			if (receivedPlayerCount >= activePlayers) {
				break;
			}
			packet = NetSession_WaitForGamePacket(&senderDpid, &packetSize, 30);
			if (packet == NULL) {
				break;
			}
			if (*packet == NET_PACKET_STILL_LOADING) {
				currentTime = timeGetTime();
				if ((int)(currentTime - statusUpdateTime) > 200) {
					statusUpdateTime = currentTime;
					playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
					playerName = NetSession_GetPlayerName(playerIndex);
					if (playerName == NULL) {
						strcpy(statusText, g_strDiskIoMessages[DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING]);
					} else {
						strcpy(statusText, playerName);
						blinkState = !blinkState;
						if (blinkState) {
							loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_MINUS];
						} else {
							loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_PLUS];
						}
						strcat(statusText, loadingSuffix);
					}
					FlightAlert_DrawBox(3, statusText, 0x30);
				}
				continue;
			}
			if (*packet == NET_PACKET_PLAYER_TAUNTS) {
				memcpy(g_playerTauntText[packet[1]], &packet[2], sizeof(g_playerTauntText[packet[1]]));
				++receivedPlayerCount;
				continue;
			}
		}
		FlightAlert_RestoreBoxBackground();
	} else {
		g_players[0].network.flightResolutionMode = g_flightResolutionMode;
		g_players[0].pilotRating = g_pilotData.rating;
		memcpy(g_playerTauntText, g_gameConfig.taunts, sizeof(g_gameConfig.taunts));
	}
	return 1;
#endif
}

// FUNCTION: XVT 0x463790
int FlightNet_WaitForMissionStart(void) {
#ifdef XVT_MODERN
	return XvtFlightNetwork_Start();
#else
	enum {
		PACKET_WAIT_TIMEOUT_TICKS = 60,
		STATUS_UPDATE_INTERVAL_MS = 200,
		DEFAULT_CLOCK_LEAD_MS = 30,
		ASYNC_CLOCK_LEAD_MS = 130,
		MINIMUM_CLIENT_CLOCK_LEAD_MS = 35,
		MISSION_START_ACK_TIMEOUT_MS = 100,
		INPUT_BATCH_INTERVAL_TICKS = 23,
		ALERT_TEXT_COLOR = 0x30,
	};

	struct MissionStartWaitState {
		uint32_t statusUpdateTime;
		int senderDplayId;
		int activePlayerCount;
		int packetSize;
		char statusText[256];
	} waitState;

	int* packet;
	int readyPlayerCount;
	int blinkState;
	int playerSlot;
	int clockAdjustment;
	uint32_t currentTime;
	int hostDplayId;
	int serverUpdateRate;
	char* playerName;
	const char* loadingSuffix;

	waitState.statusUpdateTime = 0;
	blinkState = 1;
	memset(g_flightNetLastInputDeltaCodeByPlayer, 0, sizeof(g_flightNetLastInputDeltaCodeByPlayer));
	memset(g_flightNetPeerSilenceTicks, 0, sizeof(g_flightNetPeerSilenceTicks));
	g_lastFrameTime = 0;
	g_lastKeyframeTime = 0;
	g_flightNetResyncPlayerDplayId = 0;
	g_flightNetLastInputBatchSendTime = 0;
	memset(&g_flightNetInputDeltaBatchPacket.frameCount, 0, sizeof(int));
	g_flightNetRecoveryUiActive = 0;
	g_flightNetPendingAckCount = 0;
	g_flightNetClockAdjustAccumTicks = 0;
	g_flightNetHostTimeoutElapsedMs = 0;
	g_flightNetInputDeltaBatchLen = 5;
	g_flightNetInputDeltaBatchPacket.packetType = NET_PACKET_INPUT_BATCH;
	g_flightNetInputBatchIntervalTicks = INPUT_BATCH_INTERVAL_TICKS;

	if (g_activeFlightPlayerCount == 1) {
		g_serverTickTime = 0;
		g_flightNetClockLeadAllowanceMs = DEFAULT_CLOCK_LEAD_MS;
		g_gameTime = 0;
		g_inputTimestamp = DEFAULT_CLOCK_LEAD_MS;
		Time_GetFrameDelta();
		return 1;
	}

	waitState.activePlayerCount = NetSession_CountActivePlayers();
	hostDplayId = NetSession_GetHostDplayId();
	g_flightNetScratchPacket.packetType = NET_PACKET_MISSION_LOADING_READY;

	NetSession_SendPacket(hostDplayId, (unsigned int*)&g_flightNetScratchPacket,
						  sizeof(g_flightNetScratchPacket.packetType));
	if (NetSession_GetLocalPlayerId() != 0) {
		readyPlayerCount = 0;
		for (;;) {
			if (waitState.activePlayerCount <= readyPlayerCount) {
				break;
			}
			do {
				packet = NetSession_WaitForGamePacket(&waitState.senderDplayId, &waitState.packetSize,
													  PACKET_WAIT_TIMEOUT_TICKS);
				if (packet == NULL) {
					return 0;
				}
			} while (*packet != NET_PACKET_MISSION_LOADING_READY);
			++readyPlayerCount;
		}
		g_flightNetScratchPacket.packetType = NET_PACKET_FLIGHT_MISSION_START;

		NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket,
											sizeof(g_flightNetScratchPacket.packetType));
	}

	FlightAlert_SaveBoxBackground();
	FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_WAITING_FOR_OTHER_PLAYERS], ALERT_TEXT_COLOR);
	do {
		packet = NetSession_WaitForGamePacket(&waitState.senderDplayId, &waitState.packetSize,
											  PACKET_WAIT_TIMEOUT_TICKS);
		if (packet == NULL) {
			return 0;
		}
		if (*packet == NET_PACKET_STILL_LOADING) {
			currentTime = timeGetTime();
			if ((int)(currentTime - waitState.statusUpdateTime) > STATUS_UPDATE_INTERVAL_MS) {
				waitState.statusUpdateTime = currentTime;
				playerSlot = NetSession_FindPlayerSlotByDpid(waitState.senderDplayId);
				playerName = NetSession_GetPlayerName(playerSlot);
				if (playerName == NULL) {
					strcpy(waitState.statusText,
						   g_strDiskIoMessages[DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING]);
				} else {
					strcpy(waitState.statusText, playerName);
					blinkState = !blinkState;
					if (blinkState) {
						loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_MINUS];
					} else {
						loadingSuffix = g_strDiskIoMessages[DISK_IO_STR_PLAYER_STILL_LOADING_PLUS];
					}
					strcat(waitState.statusText, loadingSuffix);
				}
				FlightAlert_DrawBox(3, waitState.statusText, ALERT_TEXT_COLOR);
			}
		}
	} while (*packet != NET_PACKET_FLIGHT_MISSION_START);
	FlightAlert_RestoreBoxBackground();

	hostDplayId = NetSession_GetHostDplayId();
	g_flightNetScratchPacket.packetType = NET_PACKET_ACK;

	NetSession_SendPacket(hostDplayId, (unsigned int*)&g_flightNetScratchPacket,
						  sizeof(g_flightNetScratchPacket.packetType));
	Time_GetFrameDelta();
	g_serverTickTime = 0;
	g_gameTime = 0;
	g_inputTimestamp = 0;
	if (g_asyncFlag != 0) {
		g_flightNetClockLeadAllowanceMs = ASYNC_CLOCK_LEAD_MS;
	} else {
		g_flightNetClockLeadAllowanceMs = DEFAULT_CLOCK_LEAD_MS;
	}
	serverUpdateRate = g_gameConfig.serverUpdateRate;
	switch (serverUpdateRate) {
		case 4:
			dtMs = 59;
			break;
		case 6:
			dtMs = 39;
			break;
		case 8:
		default:
			dtMs = 29;
			break;
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		g_flightNetPendingAckCount = 1;
		if (waitState.activePlayerCount != 1) {
			g_flightNetPendingAckCount = 2;
		}
		FlightNet_InitMissionStartAckState();
		while (g_flightNetPendingAckCount != 0 &&
			   (unsigned int)g_inputTimestamp < MISSION_START_ACK_TIMEOUT_MS) {
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += Time_GetFrameDelta();
		}
		g_flightNetPendingAckCount = 0;
		g_inputTimestamp += Time_GetFrameDelta();
		g_flightNetClockLeadAllowanceMs = g_inputTimestamp;
		if (g_inputTimestamp < MINIMUM_CLIENT_CLOCK_LEAD_MS) {
			clockAdjustment = MINIMUM_CLIENT_CLOCK_LEAD_MS - g_inputTimestamp;
			g_flightNetClockLeadAllowanceMs += clockAdjustment;
			g_inputTimestamp += clockAdjustment;
			g_flightNetClockAdjustAccumTicks -= clockAdjustment;
		}
	}
	return 1;
#endif
}

// FUNCTION: XVT 0x463B60
int FlightNet_SendClockProbeToHost(void) {
	FlightNetScratchPacket* packet;
	int inputTimestamp;

	packet = &g_flightNetScratchPacket;
	g_flightNetScratchPacket.packetType = NET_PACKET_CLOCK_PROBE;
	inputTimestamp = g_inputTimestamp;
	packet->payloadDwords[0] = inputTimestamp + g_flightNetClockAdjustAccumTicks;
	g_flightNetClockProbeTimestamp = inputTimestamp + g_flightNetClockAdjustAccumTicks;
	packet->payloadDwords[1] = g_flightNetClockLeadAllowanceMs;
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_SendPacket
#else
		NetSession_SendPacket
#endif
		(NetSession_GetHostDplayId(), (unsigned int*)packet, 12);
}

// FUNCTION: XVT 0x463BB0
int FlightNet_BroadcastStillLoadingPulse(void) {
	g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_SendPacket
#else
		NetSession_SendPacket
#endif
		(0, (unsigned int*)&g_flightNetScratchPacket, 4);
}

// FUNCTION: XVT 0x463BD0
int FlightNet_BroadcastLocalPlayerLeft(void) {
	g_flightNetScratchPacket.packetType = NET_PACKET_SESSION_ABORT;
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_Broadcast
#else
		NetSession_BroadcastPacketToPlayers
#endif
		((unsigned int*)&g_flightNetScratchPacket, 4);
}

// FUNCTION: XVT 0x463BF0
int FlightNet_SendStillLoadingPulse(void) {
	g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_SendPacket
#else
		NetSession_SendPacket
#endif
		(NetSession_GetHostDplayId(), (unsigned int*)&g_flightNetScratchPacket, 4);
}

// FUNCTION: XVT 0x463C10
int FlightNet_BroadcastPlayerDisconnected(int playerSlot) {
	int result;
	g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_DISCONNECTED;
	g_flightNetScratchPacket.payloadDwords[0] = playerSlot;
	result =
#ifdef XVT_MODERN
		XvtFlightNetwork_Broadcast
#else
		NetSession_BroadcastPacketToPlayers
#endif
		((unsigned int*)&g_flightNetScratchPacket, 8);
	g_playerConnected[playerSlot] = 0;
	return result;
}

// FUNCTION: XVT 0x463C50
int FlightNet_BroadcastPlayerAbort(int playerSlot) {
	g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_ABORT;
	g_flightNetScratchPacket.payloadDwords[0] = playerSlot;
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_Broadcast
#else
		NetSession_BroadcastPacketToPlayers
#endif
		((unsigned int*)&g_flightNetScratchPacket, 8);
}

// FUNCTION: XVT 0x463C80
int FlightNet_FindPilotNetworkPlayerIndex(int playerIdx) {
	int networkPlayerIdx;
	int* directPlayId;
	int playerDirectPlayId;
	const uint8_t* networkPlayerEnd;

	networkPlayerIdx = 0;
	directPlayId = &g_pilotData.networkPlayers[0].directPlayId;
	playerDirectPlayId = g_players[playerIdx].network.directPlayId;
	networkPlayerEnd = (const uint8_t*)directPlayId + sizeof(g_pilotData.networkPlayers);
	while (*directPlayId != playerDirectPlayId) {
		directPlayId = (int*)((uint8_t*)directPlayId + sizeof(PilotNetworkPlayer));
		++networkPlayerIdx;
		if ((const uint8_t*)directPlayId >= networkPlayerEnd) {
			return 0;
		}
	}
	return networkPlayerIdx;
}

// FUNCTION: XVT 0x463CC0
void FlightNet_MarkPilotNetworkPlayerLeft(int playerSlot) {
	g_pilotData.networkPlayers[FlightNet_FindPilotNetworkPlayerIndex(playerSlot)].hasLeft = 1;
}

// FUNCTION: XVT 0x463D00
void FlightNet_ProcessIncomingPackets(void) {
#ifdef XVT_MODERN
	XvtFlightNetwork_ProcessPackets();
#else
	enum {
		PLAYER_COUNT = 8,
		RESYNC_CHECKSUM_COUNT = 126,
		WORLD_STATE_CHUNK_COUNT = 16,
		PACKET_CLOCK_PROBE_REPLY_SIZE = 2 * sizeof(int),
		RECOVERY_DELAY_MS = 826,
		RECOVERY_BLINK_MS = 118,
		PEER_TIMEOUT_MS = 7080,
		CLOCK_PROBE_BIAS_MS = 20,
		CLOCK_PROBE_LIMIT_MS = 472,
		FULL_TIMESTAMP_CODE = 0x7F,
		TIMESTAMP_CODE_MASK = 0x7F,
		KEY_PRESENT_FLAG = 0x80,
		RECOVERY_ALERT_COLOR = 0x34
	};

	FlightInputFrameRecord input;
	int senderDpid;

	struct {
		int decodeValue;
		int serverSendElapsed;
		int worldFrameElapsed;
		int remoteInputElapsed;
		int receiveElapsed;
		int blinkToggle;
		int startTimestamp;
		int worldMessageCount;
		int payloadSize;
	} packetState;

	int currentTimestamp;
	char statusText[80];

	if (g_players[g_localPlayer].connectedFlag == 0) {
		return;
	}

	packetState.worldMessageCount = 0;
	packetState.serverSendElapsed = 0;
	packetState.remoteInputElapsed = 0;
	packetState.receiveElapsed = 0;
	packetState.worldFrameElapsed = 0;
	packetState.blinkToggle = 0;

	if (g_flightPlayerCount == 1) {
		while (NetSession_ReceiveGamePacket(&senderDpid, &packetState.payloadSize) != NULL) {
		}
		return;
	}

	if (g_flightNetRecoveryUiActive != 0) {
		int blinkTime = g_flightNetRecoveryUiBlinkTime;

		packetState.startTimestamp = g_flightNetRecoverySavedInputTimestamp - RECOVERY_DELAY_MS;
		currentTimestamp = blinkTime + (int)Time_GetFrameDelta() + 1;
	} else {
		currentTimestamp = g_inputTimestamp;
		currentTimestamp += (int)Time_GetFrameDelta();
		packetState.startTimestamp = currentTimestamp;
	}

	for (;;) {
		int* packet;

		if (currentTimestamp - packetState.startTimestamp > RECOVERY_DELAY_MS) {
			if (g_flightNetRecoveryUiActive != 0) {
				if (FlightInput_HasKeyReady() != 0 && FlightInput_GetNextKey() == FLIGHT_KEY_ESCAPE) {
					g_flightNetRecoveryUiActive = 0;
					FlightAlert_RestoreBoxBackground();
					g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
					g_serverTickTime = g_gameTime;
					g_flightMissionState.missionEndPending = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					FlightNet_BroadcastPlayerAbort(g_localPlayer);
					if (NetSession_GetLocalPlayerId() == 0) {
						FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
					} else {
						FlightNet_BroadcastLocalPlayerLeft();
					}
					return;
				}
				if (currentTimestamp - g_flightNetRecoveryUiBlinkTime > RECOVERY_BLINK_MS) {
					g_flightNetRecoveryUiBlinkTime = currentTimestamp;
					packetState.blinkToggle = !packetState.blinkToggle;
					if (packetState.blinkToggle != 0) {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_ESC_DISCONNECT],
											RECOVERY_ALERT_COLOR);
					} else {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_RECOVERING_WAIT],
											RECOVERY_ALERT_COLOR);
					}
				}
			} else {
				char* playerName;

				packetState.blinkToggle = 1;
				FlightAlert_SaveBoxBackground();
				strcpy(statusText, g_strDiskIoMessages[DISK_IO_STR_COM_FAILURE_WAITING]);
				playerName = FlightNet_GetStatusPlayerName();
				if (playerName != NULL) {
					strcat(statusText, playerName);
				}
				FlightAlert_DrawBox(1, statusText, RECOVERY_ALERT_COLOR);
				g_flightNetRecoveryUiBlinkTime = currentTimestamp;
				g_flightNetRecoveryUiActive = 1;
				g_flightNetRecoverySavedInputTimestamp = currentTimestamp;
				if (NetSession_GetLocalPlayerId() == 0) {
					NetReliable_CompactLocalReceiveQueue();
					FlightNet_BroadcastPlayerDisconnected(g_localPlayer);
				}
			}
		}

		currentTimestamp += (int)Time_GetFrameDelta();
		packet = NetSession_ReceiveGamePacket(&senderDpid, &packetState.payloadSize);
		{
			int frameDelta = (int)Time_GetFrameDelta();

			packetState.receiveElapsed += frameDelta;
			currentTimestamp += frameDelta;
		}

		if (packet == NULL) {
			if (NetSession_GetLocalPlayerId() == 0) {
				break;
			}

			currentTimestamp += (int)Time_GetFrameDelta();
			if (FlightNet_ShouldSendClientWorldMessage(g_inputTimestamp) != 0) {
				int frameDelta;

				FlightNet_SendClientWorldMessage(g_inputTimestamp);
				frameDelta = (int)Time_GetFrameDelta();
				packetState.serverSendElapsed += frameDelta;
				currentTimestamp += frameDelta;
				continue;
			}
			{
				int frameDelta = (int)Time_GetFrameDelta();

				packetState.serverSendElapsed += frameDelta;
				currentTimestamp += frameDelta;
			}
			break;
		}

		switch (packet[0]) {
			case NET_PACKET_REMOTE_INPUT: {
				const uint8_t* cursor;
				InputFrame* inserted;
				int frameDelta;
				int playerIndex;
				unsigned int timestamp;

				currentTimestamp += (int)Time_GetFrameDelta();
				playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
				if (g_players[playerIndex].connectedFlag != 0) {
					if (g_flightNetPeerSilenceTicks[playerIndex] > 0) {
						g_flightNetPeerSilenceTicks[playerIndex] = 0;
					}
					FlightSync_DiscardPredictedInputFrames(playerIndex);
					cursor = (const uint8_t*)&packet[1];
					memset(&input, 0, sizeof(input));
					packetState.decodeValue = *cursor;
					if ((packetState.decodeValue & TIMESTAMP_CODE_MASK) == FULL_TIMESTAMP_CODE) {
						timestamp = *(const unsigned int*)(cursor + 1);
						if ((packetState.decodeValue & KEY_PRESENT_FLAG) != 0) {
							input.key = cursor[5];
							cursor += 6;
						} else {
							cursor += 5;
						}
					} else {
						int previousCode = g_flightNetLastInputDeltaCodeByPlayer[playerIndex];
						int lowCode = packetState.decodeValue & TIMESTAMP_CODE_MASK;

						if ((previousCode & TIMESTAMP_CODE_MASK) > lowCode) {
							previousCode += TIMESTAMP_CODE_MASK + 1;
						}
						timestamp =
							(unsigned int)lowCode | ((unsigned int)previousCode & ~TIMESTAMP_CODE_MASK);
						if ((packetState.decodeValue & KEY_PRESENT_FLAG) != 0) {
							input.key = cursor[1];
							cursor += 2;
						} else {
							cursor += 1;
						}
					}
					g_flightNetLastInputDeltaCodeByPlayer[playerIndex] = (int)timestamp;
					input.axisX = (int8_t)(cursor[0] & (uint8_t)~1u);
					input.axisY = (int8_t)(cursor[1] & (uint8_t)~1u);
					input.keyMods = cursor[1] & 1u;
					input.keyMods += input.keyMods;
					input.keyMods |= cursor[0] & 1u;
					inserted = FlightSync_InsertInputFrame(playerIndex, (int)timestamp, &input);
					if (inserted != NULL) {
						int localPlayerId = NetSession_GetLocalPlayerId();

						inserted->applied = 1;
						if (localPlayerId == 0) {
							inserted->applied = 0;
						}
						inserted->valid = 1;
					}
				} else {
					g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_ABORT;
					g_flightNetScratchPacket.payloadDwords[0] = playerIndex;

					NetSession_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
										  2 * sizeof(int));
				}
				frameDelta = (int)Time_GetFrameDelta();
				packetState.remoteInputElapsed += frameDelta;
				currentTimestamp += frameDelta;
				continue;
			}
			case NET_PACKET_WORLD_MESSAGE: {
				int frameDelta;
				int playerIndex;

				currentTimestamp += (int)Time_GetFrameDelta();
				g_flightNetHostTimeoutElapsedMs = 0;
				++g_flightNetReceivedWorldMessageCount;
				FlightSync_ApplyWorldMessagePacket((uint8_t*)packet);
				if (NetSession_GetLocalPlayerId() != 0) {
					for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
						if (playerIndex != g_localPlayer && g_players[playerIndex].connectedFlag != 0 &&
							g_flightNetPeerSilenceTicks[playerIndex] != -1) {
							g_flightNetPeerSilenceTicks[playerIndex] += dtMs;
							if (g_flightNetPeerSilenceTicks[playerIndex] > PEER_TIMEOUT_MS) {
								FlightNet_BroadcastPlayerAbort(playerIndex);
								g_flightNetPeerSilenceTicks[playerIndex] = 0;
							}
						}
					}
				}
				if (g_flightMissionState.missionEndPending == 1) {
					if (g_flightNetRecoveryUiActive != 0) {
						g_flightNetRecoveryUiActive = 0;
						FlightAlert_RestoreBoxBackground();
						g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
					}
					return;
				}
				frameDelta = (int)Time_GetFrameDelta();
				packetState.worldFrameElapsed += frameDelta;
				currentTimestamp += frameDelta;
				++packetState.worldMessageCount;
				continue;
			}
			case NET_PACKET_PLAYER_DISCONNECTED: {
				int playerIndex = packet[1];

				if (playerIndex >= 0 && playerIndex < PLAYER_COUNT) {
					g_playerConnected[playerIndex] = 0;
				}
				continue;
			}
			case NET_PACKET_WORLD_CHECKSUM: {
				int playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);

				if (g_players[playerIndex].connectedFlag != 0) {
					FlightSync_HandleWorldChecksumPacket(senderDpid, packet);
				}
				continue;
			}
			case NET_PACKET_SESSION_ABORT:
				g_flightNetHostAbortReceived = 1;
				g_flightMissionState.missionEndPending = 1;
				g_players[g_localPlayer].connectedFlag = 0;
				if (g_flightNetRecoveryUiActive != 0) {
					g_flightNetRecoveryUiActive = 0;
					FlightAlert_RestoreBoxBackground();
					g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
				}
				return;
			case NET_PACKET_RESYNC_CHUNK_ACK: {
				unsigned int chunkIndex;

				g_flightNetWorldStateAckReceivedFlag = 1;
				chunkIndex = (unsigned int)packet[1];
				if (chunkIndex < WORLD_STATE_CHUNK_COUNT) {
					g_flightNetWorldStateChunkAcked[chunkIndex] = 1;
				}
				continue;
			}
			case NET_PACKET_PLAYER_ABORT: {
				int playerIndex = packet[1];

				if (playerIndex >= 0 && playerIndex < PLAYER_COUNT) {
					g_playerAbortFlags[playerIndex] = 1;
				}
				if (playerIndex != g_localPlayer) {
					continue;
				}
				g_flightMissionState.missionEndPending = 1;
				g_players[g_localPlayer].connectedFlag = 0;
				g_playerAbortFlags[g_localPlayer] = 1;
				FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
				if (g_flightNetRecoveryUiActive != 0) {
					g_flightNetRecoveryUiActive = 0;
					FlightAlert_RestoreBoxBackground();
					g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
				}
				return;
			}
			case NET_PACKET_INPUT_BATCH: {
				const uint8_t* cursor;
				int frameCount;
				int playerIndex;

				playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
				if (g_players[playerIndex].connectedFlag != 0) {
					if (g_flightNetPeerSilenceTicks[playerIndex] > 0) {
						g_flightNetPeerSilenceTicks[playerIndex] = 0;
					}
					FlightSync_DiscardPredictedInputFrames(playerIndex);
					cursor = (const uint8_t*)packet + sizeof(int);
					frameCount = *cursor++;
					if (frameCount > 0) {
						packetState.decodeValue = frameCount;
						do {
							InputFrame* inserted;
							char timestampCode;
							uint8_t lowCode;
							unsigned int timestamp;

							memset(&input, 0, sizeof(input));
							timestampCode = (int8_t)*cursor;
							lowCode = (uint8_t)timestampCode & TIMESTAMP_CODE_MASK;
							if (lowCode == FULL_TIMESTAMP_CODE) {
								timestamp = *(const unsigned int*)(cursor + 1);
								if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
									input.key = cursor[5];
									cursor += 6;
								} else {
									cursor += 5;
								}
							} else {
								int previousCode = g_flightNetLastInputDeltaCodeByPlayer[playerIndex];
								if ((previousCode & TIMESTAMP_CODE_MASK) > lowCode) {
									previousCode += TIMESTAMP_CODE_MASK + 1;
								}
								timestamp = (unsigned int)lowCode |
											((unsigned int)previousCode & ~TIMESTAMP_CODE_MASK);
								if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
									input.key = cursor[1];
									cursor += 2;
								} else {
									cursor += 1;
								}
							}
							g_flightNetLastInputDeltaCodeByPlayer[playerIndex] = (int)timestamp;
							input.axisX = (int8_t)(cursor[0] & (uint8_t)~1u);
							input.axisY = (int8_t)(cursor[1] & (uint8_t)~1u);
							input.keyMods = cursor[1] & 1u;
							input.keyMods += input.keyMods;
							input.keyMods |= cursor[0] & 1u;
							cursor += 2;
							inserted = FlightSync_InsertInputFrame(playerIndex, (int)timestamp, &input);
							if (inserted != NULL) {
								int localPlayerId = NetSession_GetLocalPlayerId();

								inserted->applied = 1;
								if (localPlayerId == 0) {
									inserted->applied = 0;
								}
								inserted->valid = 1;
							}
						} while (--packetState.decodeValue != 0);
					}
				} else {
					g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_ABORT;
					g_flightNetScratchPacket.payloadDwords[0] = playerIndex;

					NetSession_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
										  2 * sizeof(int));
				}
				continue;
			}
			case NET_PACKET_RESYNC_NOTICE:
				g_flightNetResyncPlayerDplayId = packet[1];
				continue;
			case NET_PACKET_SERVER_CHECKSUM:
				FlightSync_HandleServerChecksumPacket((uint8_t*)packet);
				FlightNet_SendClockProbeToHost();
				continue;
			case NET_PACKET_ACK:
				if (g_flightNetPendingAckCount != 0) {
					--g_flightNetPendingAckCount;
					if (g_flightNetPendingAckCount == 0) {
						g_flightNetNextClientInputSendTimestamp = 0;
						if (g_flightNetRecoveryUiActive != 0) {
							g_flightNetRecoveryUiActive = 0;
							FlightAlert_RestoreBoxBackground();
							g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
						}
						return;
					}
				}
				continue;
			case NET_PACKET_CLOCK_LEAD:
				g_flightNetClockLeadAllowanceMs = packet[1];
				continue;
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
				continue;
			case NET_PACKET_CLOCK_PROBE: {
				int adjustment;
				int targetLead;

				g_flightNetScratchPacket.packetType = NET_PACKET_CLOCK_PROBE_REPLY;
				g_flightNetScratchPacket.payloadDwords[0] = packet[1];

				NetSession_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
									  PACKET_CLOCK_PROBE_REPLY_SIZE);
				targetLead = packet[2];
				if (g_asyncFlag == 0 || g_flightNetSmallSessionPlayerThreshold > g_activeFlightPlayerCount) {
					targetLead >>= 1;
				}
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
				continue;
			}
			case NET_PACKET_CLOCK_PROBE_REPLY:
				if (NetSession_GetLocalPlayerId() == 0 && packet[1] == g_flightNetClockProbeTimestamp) {
					int adjustment;
					int targetLead;

					targetLead = g_flightNetClockAdjustAccumTicks;
					targetLead += g_inputTimestamp;
					targetLead -= packet[1];
					targetLead += CLOCK_PROBE_BIAS_MS;

					if (targetLead < CLOCK_PROBE_LIMIT_MS) {
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
				continue;
			case NET_PACKET_RESYNC_CHECKSUMS:
				g_flightNetRemoteResyncChecksumsReceivedFlag = 1;
				memcpy(g_flightNetRemoteResyncChecksums, &packet[1],
					   sizeof(g_flightNetRemoteResyncChecksums[0]) * RESYNC_CHECKSUM_COUNT);
				continue;
			case NET_PACKET_RESYNC_REQUEST:
			case NET_PACKET_RESYNC_APPLY:
			case NET_PACKET_RESYNC_CHUNK:
				if ((unsigned int)packet[1] != g_flightNetWorldChecksumEpoch) {
					continue;
				}
				FlightNet_HandleWorldStateResyncPacket(packet);
				if (g_flightNetHostTimeoutElapsedMs > PEER_TIMEOUT_MS ||
					g_players[g_localPlayer].connectedFlag == 0) {
					if (g_flightNetRecoveryUiActive != 0) {
						g_flightNetRecoveryUiActive = 0;
						FlightAlert_RestoreBoxBackground();
						g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
					}
					return;
				}
				continue;
			default:
				continue;
		}
	}

	if (g_flightNetRecoveryUiActive != 0) {
		g_flightNetRecoveryUiActive = 0;
		FlightAlert_RestoreBoxBackground();
		currentTimestamp = g_flightNetRecoverySavedInputTimestamp;
		g_inputTimestamp = currentTimestamp;
	}
	currentTimestamp += (int)Time_GetFrameDelta();
	g_inputTimestamp = currentTimestamp;
	{
		int allInputElapsed = g_inputTimestamp - packetState.startTimestamp;
		int miscellaneousElapsed = allInputElapsed - packetState.serverSendElapsed -
								   packetState.remoteInputElapsed - packetState.receiveElapsed -
								   packetState.worldFrameElapsed;

		sprintf(statusText,
				"RcvMsg:%-2d In2Svr:%-2d SvrSnd:%-2d SvrFrm:%-2d NumFrm:%-2d AllPIN:%-2d Misc:%-2d\n",
				packetState.receiveElapsed, packetState.remoteInputElapsed, packetState.serverSendElapsed,
				packetState.worldFrameElapsed, packetState.worldMessageCount, allInputElapsed,
				miscellaneousElapsed);
	}
#endif
}

// FUNCTION: XVT 0x464900
int32_t FlightNet_SampleAndSendInput(void) {
#ifdef XVT_MODERN
	InputFrame* inserted;

	FlightInput_Read(-2);
	memset(&g_currentInputFrame, 0, sizeof g_currentInputFrame);
	g_currentInputFrame.key = (uint8_t)g_actionKey;
	g_currentInputFrame.axisX = (int8_t)(g_ctrlAxisX & 0xfe);
	g_currentInputFrame.axisY = (int8_t)(g_ctrlAxisY & 0xfe);
	g_currentInputFrame.axisR = (int8_t)(g_xvtControlRoll & 0xfe);
	g_currentInputFrame.keyMods = (uint8_t)(g_keyMods & 3u);
	XvtFlightControls_SampleThrottle(&g_currentInputFrame);
	inserted = FlightSync_InsertInputFrame(g_localPlayer, g_inputTimestamp, &g_currentInputFrame);
	if (inserted != NULL) {
		inserted->applied = 0;
		inserted->valid = 1;
	}
	return Flight_PumpWindowMessages();
#else
	static uint8_t encodedTime;
	int packetLength;
	int directPlayerIndex;
	uint8_t* packetBytes = (uint8_t*)&g_flightNetScratchPacket;
	InputFrame* inserted;

	FlightInput_Read(-2);
	g_currentInputFrame.key = (uint8_t)g_actionKey;
	g_currentInputFrame.axisX = (int8_t)(g_ctrlAxisX & 0xfe);
	g_currentInputFrame.axisY = (int8_t)(g_ctrlAxisY & 0xfe);
	g_currentInputFrame.keyMods = (uint8_t)(g_keyMods & 3u);
	g_flightNetScratchPacket.packetType = NET_PACKET_REMOTE_INPUT;
	g_inputTimestamp += Time_GetFrameDelta();

	packetLength = g_inputTimestamp - g_lastFrameTime;
	if (packetLength >= 127 || packetLength < 0 || g_lastFrameTime == 0)
		packetLength = 127;
	else
		packetLength = g_inputTimestamp & 0x7f;
	if (g_lastFrameTime - g_lastKeyframeTime > SIMULATION_TICKS_PER_SECOND)
		packetLength = 127;
	if (packetLength == 127) {
		g_lastKeyframeTime = g_inputTimestamp;
		packetBytes[4] = 127;
		memcpy(&packetBytes[5], &g_inputTimestamp, sizeof(g_inputTimestamp));
		packetLength = 9;
	} else {
		packetBytes[4] = (uint8_t)packetLength;
		packetLength = 5;
	}
	g_lastFrameTime = g_inputTimestamp;
	if (g_currentInputFrame.key != 0) {
		encodedTime = packetBytes[4];
		++packetLength;
		packetBytes[4] = (uint8_t)(encodedTime | 0x80u);
		packetBytes[packetLength - 1] = g_currentInputFrame.key;
	}
	packetBytes[packetLength] = (uint8_t)g_currentInputFrame.axisX;
	packetBytes[packetLength + 1] = (uint8_t)g_currentInputFrame.axisY;
	if ((g_currentInputFrame.keyMods & 1u) != 0)
		packetBytes[packetLength] |= 1u;
	if ((g_currentInputFrame.keyMods & 2u) != 0)
		packetBytes[packetLength + 1] |= 1u;
	packetLength += 2;

	if (g_flightPlayerCount > 1) {
		if (g_inputLogEnabled == 1) {
			if (g_inputLogFile == NULL)
				g_inputLogFile = File_RawOpen("inputlog.txt", "w");
			if (g_inputLogFile != NULL) {
				File_Printf(g_inputLogFile, "%8x %2x %2x %2x %2x\n",
							g_flightNetScratchPacket.payloadDwords[0], g_currentInputFrame.key,
							(uint8_t)g_currentInputFrame.axisX, (uint8_t)g_currentInputFrame.axisY,
							g_currentInputFrame.keyMods);
				File_Flush(g_inputLogFile);
			}
		}
		if (g_asyncFlag == 0) {
			for (directPlayerIndex = 0; directPlayerIndex < 8; ++directPlayerIndex) {
				if (g_players[directPlayerIndex].connectedFlag != 0 &&
					(g_playerConnected[directPlayerIndex] != 0 || directPlayerIndex == g_localPlayer)) {

					NetSession_SendPacket(g_players[directPlayerIndex].network.directPlayId,
										  (unsigned int*)&g_flightNetScratchPacket, packetLength);
				}
			}
		} else {
			int batchPlayerIndex;
			uint8_t* batchFrameCount;

			batchFrameCount = &g_flightNetInputDeltaBatchPacket.frameCount;
			g_flightNetInputDeltaBatchPacket.packetType = NET_PACKET_INPUT_BATCH;
			++*batchFrameCount;
			memcpy(&((uint8_t*)&g_flightNetInputDeltaBatchPacket)[g_flightNetInputDeltaBatchLen],
				   g_flightNetScratchPacket.payloadDwords, (size_t)(packetLength - 4));
			g_flightNetInputDeltaBatchLen += packetLength - 4;
			if ((unsigned int)(g_inputTimestamp - g_flightNetLastInputBatchSendTime) >
				(unsigned int)g_flightNetInputBatchIntervalTicks) {
				g_flightNetLastInputBatchSendTime = g_inputTimestamp;

				NetSession_SendPacket(NetSession_GetHostDplayId(),
									  (unsigned int*)&g_flightNetInputDeltaBatchPacket,
									  g_flightNetInputDeltaBatchLen);
				if (g_flightNetSmallSessionPlayerThreshold > g_activeFlightPlayerCount) {
					for (batchPlayerIndex = 0; batchPlayerIndex < 8; ++batchPlayerIndex) {
						if (g_players[batchPlayerIndex].connectedFlag != 0 &&
							batchPlayerIndex != g_localPlayer &&
							NetSession_GetHostDplayId() != g_players[batchPlayerIndex].network.directPlayId &&
							g_playerConnected[batchPlayerIndex] != 0) {

							NetSession_SendPacket(g_players[batchPlayerIndex].network.directPlayId,
												  (unsigned int*)&g_flightNetInputDeltaBatchPacket,
												  g_flightNetInputDeltaBatchLen);
						}
					}
				}
				g_flightNetInputDeltaBatchLen = 5;
				g_flightNetInputDeltaBatchPacket.packetType = NET_PACKET_INPUT_BATCH;
				g_flightNetInputDeltaBatchPacket.frameCount = 0;
			}
		}
	}

	inserted = FlightSync_InsertInputFrame(g_localPlayer, g_inputTimestamp, &g_currentInputFrame);
	if (inserted != NULL) {
		inserted->applied = 0;
		inserted->valid = 1;
	}
	return Flight_PumpWindowMessages();
#endif
}

// FUNCTION: XVT 0x464C60
void FlightNet_InitMissionStartAckState(void) {
	g_unusedFlightNetMissionStartAckInitFlag = 1;
	g_flightNetNextClientInputSendTimestamp = 0;
	g_flightNetLastSentWorldMessageTimestamp = 0;
}

// FUNCTION: XVT 0x464C80
int FlightNet_ShouldSendClientWorldMessage(int inputTimestamp) {
#ifdef XVT_MODERN
	return XvtFlightNetwork_ShouldSend(inputTimestamp);
#else

	int adjustedTimestamp;
	int elapsedTimestamp;
	int oldestInputTimestamp;
	int playerIdx;
	uint8_t* connectedFlag;
	const uint8_t* playersEnd;

	if (g_flightNetPendingAckCount != 0) {
		return 0;
	}
	if (g_flightNetRecoveryUiActive != 0) {
		return 0;
	}

	adjustedTimestamp = inputTimestamp;
	adjustedTimestamp += g_flightNetClockAdjustAccumTicks;
	if (g_flightNetNextClientInputSendTimestamp == 0) {
		g_flightNetNextClientInputSendTimestamp = adjustedTimestamp + (g_flightNetClockLeadAllowanceMs >> 3);
	}
	elapsedTimestamp = adjustedTimestamp - g_flightNetNextClientInputSendTimestamp;
	if (elapsedTimestamp < dtMs) {
		return 0;
	}
	if (elapsedTimestamp > 5 * dtMs) {
		g_flightNetNextClientInputSendTimestamp += dtMs;
		return 1;
	}

	oldestInputTimestamp = 0x7FFFFFFF;
	playerIdx = 0;
	connectedFlag = &g_players[0].connectedFlag;
	playersEnd = (const uint8_t*)(g_players + 8);
	while (connectedFlag < playersEnd) {
		if (*connectedFlag != 0) {
			InputFrame* inputFrame;

			inputFrame = FlightSync_FindLastNonzeroInputFrame(playerIdx);
			if (inputFrame == NULL) {
				oldestInputTimestamp = 0;
				break;
			}
			if (inputFrame->timestamp < oldestInputTimestamp) {
				oldestInputTimestamp = inputFrame->timestamp;
			}
		}
		connectedFlag += sizeof(PlayerData);
		++playerIdx;
	}

	if (g_flightNetLastSentWorldMessageTimestamp + dtMs < oldestInputTimestamp) {
		g_flightNetNextClientInputSendTimestamp += dtMs;
		return 1;
	}
	return 0;

#endif
}

// FUNCTION: XVT 0x464D70
void FlightNet_SendClientWorldMessage(int inputTimestamp) {
#ifdef XVT_MODERN
	(void)inputTimestamp;
	XvtFlightNetwork_SendWorld();
#else
	enum {
		PLAYER_SLOT_COUNT = 8,
		CHECKSUM_RESET_INTERVAL = 472,
		PACKET_PLAYER_COUNT_OFFSET = 8,
		PACKET_HEADER_SIZE = 9,
		BANDWIDTH_BYTES_PER_SECOND = 3000,
		BANDWIDTH_WINDOW_MS = 236,
		MAX_PACKET_PAYLOAD = 508,
		MAX_ENCODED_INPUT_RECORD_SIZE = 8,
		PACKET_STREAM_LIMIT = 504,
		FULL_TIMESTAMP_DELTA = 65536,
		SHORT_DELTA_THRESHOLD = 368,
		BYTE_DELTA_THRESHOLD = 125,
		FULL_TIMESTAMP_CODE = 127,
		SHORT_DELTA_CODE = 126,
		BYTE_DELTA_CODE = 125,
		KEY_PRESENT_FLAG = 0x80,
		DELTA_CODE_MASK = 0x7F,
		LOGGED_RECORD_SIZE = 10
	};

	uint8_t* packetBytes;
	uint8_t* dest;
	int currentTick;
	int packetLength;
	int playerIndex;
	int frameIndex;
	InputFrame* frame;
	int code;
	uint8_t* recordCount;
	int bandwidthBudget;
	unsigned int bytesPerPlayer;
	unsigned int maxRecordsPerPlayer;
	const uint8_t* logCursor;
	int loggedCount;
	int logRecordIndex;

	(void)inputTimestamp;

	++g_flightNetSentWorldMessageCount;
	if (g_flightPlayerCount <= 1)
		return;
	currentTick = g_flightNetLastSentWorldMessageTimestamp + dtMs;
	g_flightNetLastSentWorldMessageTimestamp = currentTick;
	g_flightNetWorldChecksumResetAccumMs += currentTick - g_serverTickTime;
	g_flightNetScratchPacket.payloadDwords[0] = currentTick;
	g_flightNetScratchPacket.packetType = NET_PACKET_WORLD_MESSAGE;
	if (g_flightNetWorldChecksumResetAccumMs > CHECKSUM_RESET_INTERVAL) {
		g_flightNetWorldChecksumResetAccumMs = 0;
		g_flightNetScratchPacket.payloadDwords[0] = currentTick | (int)0x80000000u;
		memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof(g_flightNetWorldChecksumPeerStatus));
	}

	packetBytes = (uint8_t*)&g_flightNetScratchPacket;
	packetBytes[PACKET_PLAYER_COUNT_OFFSET] = 0;
	bandwidthBudget = dtMs * BANDWIDTH_BYTES_PER_SECOND / BANDWIDTH_WINDOW_MS;
	if (bandwidthBudget > MAX_PACKET_PAYLOAD)
		bandwidthBudget = MAX_PACKET_PAYLOAD;
	bytesPerPlayer =
		(bandwidthBudget - PACKET_HEADER_SIZE - g_activeFlightPlayerCount) / g_activeFlightPlayerCount;
	maxRecordsPerPlayer = bytesPerPlayer / LOGGED_RECORD_SIZE;
	/* The original computes this budget but limits packets by encoded byte count. */
	(void)maxRecordsPerPlayer;
	dest = &packetBytes[PACKET_HEADER_SIZE];
	packetLength = PACKET_HEADER_SIZE;
	for (playerIndex = 0; playerIndex < PLAYER_SLOT_COUNT; ++playerIndex) {
		if (g_players[playerIndex].connectedFlag == 0)
			continue;
		++packetBytes[PACKET_PLAYER_COUNT_OFFSET];
		recordCount = dest;
		*dest++ = 0;
		++packetLength;
		for (frameIndex = 0; frameIndex < g_inputFrameCount[playerIndex]; ++frameIndex) {
			frame = &g_inputHistory[playerIndex][frameIndex];
			if (frame->applied == 0 || frame->timestamp > currentTick)
				continue;
			if ((int)(dest - packetBytes) + MAX_ENCODED_INPUT_RECORD_SIZE >
				PACKET_STREAM_LIMIT - g_activeFlightPlayerCount)
				break;
			++*recordCount;
			code = currentTick - frame->timestamp;
			if (code >= FULL_TIMESTAMP_DELTA)
				code = FULL_TIMESTAMP_CODE;
			else if (code >= SHORT_DELTA_THRESHOLD)
				code = SHORT_DELTA_CODE;
			else if (code >= BYTE_DELTA_THRESHOLD)
				code = BYTE_DELTA_CODE;
			if (frame->input.key != 0)
				code |= KEY_PRESENT_FLAG;
			*dest++ = (uint8_t)code;
			++packetLength;
			if ((code & DELTA_CODE_MASK) == FULL_TIMESTAMP_CODE) {
				*(int*)dest = frame->timestamp;
				dest += sizeof(int);
				packetLength += sizeof(int);
			} else if ((code & DELTA_CODE_MASK) == SHORT_DELTA_CODE) {
				*(uint16_t*)dest = (uint16_t)(currentTick - frame->timestamp);
				dest += sizeof(uint16_t);
				packetLength += sizeof(uint16_t);
			} else if ((code & DELTA_CODE_MASK) == BYTE_DELTA_CODE) {
				*dest++ = (uint8_t)(currentTick - frame->timestamp - BYTE_DELTA_THRESHOLD);
				++packetLength;
			}
			if ((code & KEY_PRESENT_FLAG) != 0) {
				*dest++ = frame->input.key;
				++packetLength;
			}
			dest[0] = (uint8_t)frame->input.axisX;
			dest[1] = (uint8_t)frame->input.axisY;
			dest[0] &= (uint8_t)~1u;
			dest[1] &= (uint8_t)~1u;
			dest[0] |= frame->input.keyMods & 1u;
			dest[1] |= (frame->input.keyMods & 2u) >> 1;
			dest += 2;
			packetLength += 2;
			frame->applied = 0;
		}
	}

	NetSession_SendPacket(0, (unsigned int*)&g_flightNetScratchPacket, packetLength);
	if (g_inputLogEnabled == 1) {
		if (g_flightNetServerLogFile == NULL)
			g_flightNetServerLogFile = File_RawOpen("serverlog.txt", "w");
		if (g_flightNetServerLogFile != NULL) {
			File_Printf(g_flightNetServerLogFile, "%8x\n", g_flightNetScratchPacket.payloadDwords[0]);
			logCursor = &packetBytes[PACKET_HEADER_SIZE];
			for (playerIndex = 0; playerIndex < PLAYER_SLOT_COUNT; ++playerIndex) {
				if (g_players[playerIndex].connectedFlag == 0)
					continue;
				loggedCount = *logCursor++;
				File_Printf(g_flightNetServerLogFile, " %2x\n", loggedCount);
				for (logRecordIndex = 0; logRecordIndex < loggedCount; ++logRecordIndex) {
					const FlightInputFrameRecord* loggedInput =
						(const FlightInputFrameRecord*)(logCursor + sizeof(int));

					File_Printf(g_flightNetServerLogFile, "  %8x %2x %2x %2x %2x\n", *(const int*)logCursor,
								loggedInput->key, (uint8_t)loggedInput->axisX, (uint8_t)loggedInput->axisY,
								loggedInput->keyMods);
					logCursor = (const uint8_t*)(loggedInput + 1);
				}
			}
			File_Flush(g_flightNetServerLogFile);
		}
	}
#endif
}

// FUNCTION: XVT 0x4650E0
int FlightNet_SendWorldChecksumToLocalPlayer(const int* worldChecksum, const int* peerChecksum,
											 int checksumDwordCount) {
	g_flightNetScratchPacket.packetType = NET_PACKET_WORLD_CHECKSUM;
	g_flightNetScratchPacket.payloadDwords[0] = g_serverTickTime;
	memcpy(&g_flightNetScratchPacket.payloadDwords[1], worldChecksum,
		   (size_t)checksumDwordCount * sizeof(int));
	memcpy(&g_flightNetScratchPacket.payloadDwords[checksumDwordCount + 1], peerChecksum,
		   (size_t)checksumDwordCount * sizeof(int));
#ifdef XVT_MODERN
	g_flightNetScratchPacket.payloadDwords[checksumDwordCount * 2 + 1] = 0;
	return XvtFlightNetwork_SendPacket(NetSession_GetHostDplayId(), (unsigned*)&g_flightNetScratchPacket,
									   checksumDwordCount * 8 + 12);
#else
	return NetSession_SendPacket(NetSession_GetHostDplayId(), (unsigned int*)&g_flightNetScratchPacket,
								 checksumDwordCount * 8 + 8);
#endif
}

// FUNCTION: XVT 0x465150
int FlightNet_BroadcastWorldChecksum(const int* worldChecksum, const int* peerChecksum,
									 int checksumDwordCount) {
	g_flightNetScratchPacket.packetType = NET_PACKET_SERVER_CHECKSUM;
	g_flightNetScratchPacket.payloadDwords[0] = g_serverTickTime;
	memcpy(&g_flightNetScratchPacket.payloadDwords[1], worldChecksum,
		   (size_t)checksumDwordCount * sizeof(int));
	memcpy(&g_flightNetScratchPacket.payloadDwords[checksumDwordCount + 1], peerChecksum,
		   (size_t)checksumDwordCount * sizeof(int));
	return
#ifdef XVT_MODERN
		XvtFlightNetwork_Broadcast
#else
		NetSession_BroadcastPacketToPlayers
#endif
		((unsigned int*)&g_flightNetScratchPacket, checksumDwordCount * 8 + 8);
}

// FUNCTION: XVT 0x4651F0
void FlightNet_SendWorldStateResyncApplyRequest(int directPlayId, int worldStateSize) {
#ifdef XVT_MODERN
	XvtResync_BeginApply(directPlayId, worldStateSize);
#else
	enum {
		RESYNC_REQUEST_SIZE = 4 * sizeof(int),
		RESYNC_NOTICE_SIZE = 2 * sizeof(int),
		ACK_WAIT_MS = 236,
		ACK_RETRY_COUNT = 10
	};

	int retriesRemaining;
	int stillLoadingElapsed;

	g_flightNetScratchPacket.payloadDwords[0] = (int)g_flightNetWorldChecksumEpoch;
	g_flightNetScratchPacket.payloadDwords[1] = worldStateSize;
	g_flightNetScratchPacket.payloadDwords[2] = g_inputTimestamp;
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_APPLY;

	NetSession_SendPacket(directPlayId, (unsigned int*)&g_flightNetScratchPacket, RESYNC_REQUEST_SIZE);
	Time_GetFrameDelta();

	for (retriesRemaining = ACK_RETRY_COUNT, stillLoadingElapsed = 0; retriesRemaining != 0;
		 --retriesRemaining) {
		int passStartTimestamp;

		g_flightNetPendingAckCount = 1;
		passStartTimestamp = g_inputTimestamp;
		while ((unsigned int)(g_inputTimestamp - passStartTimestamp) < (unsigned int)ACK_WAIT_MS) {
			if (FlightInput_HasKeyReady() != 0 && FlightInput_GetNextKey() == FLIGHT_KEY_ESCAPE) {
				g_inputTimestamp += ACK_WAIT_MS;
				retriesRemaining = 1;
				break;
			}

			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += (int)Time_GetFrameDelta();
			if (g_flightNetWorldStateAckReceivedFlag != 0) {
				g_flightNetWorldStateAckReceivedFlag = 0;
				g_inputTimestamp = passStartTimestamp;
			}
			if (g_flightNetPendingAckCount == 0) {
				break;
			}
		}

		stillLoadingElapsed += g_inputTimestamp - passStartTimestamp;
		if (stillLoadingElapsed >= ACK_WAIT_MS) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;

			NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket, sizeof(int));
		}
		if (g_flightNetPendingAckCount == 0) {
			break;
		}
	}

	if (g_flightNetPendingAckCount == 1) {
		int playerIndex = NetSession_FindPlayerSlotByDpid(directPlayId);

		FlightNet_BroadcastPlayerAbort(playerIndex);
		g_inputTimestamp += (int)Time_GetFrameDelta();
		g_flightNetPendingAckCount = 0;
	} else {
		g_inputTimestamp += (int)Time_GetFrameDelta();
	}

	g_inputTimestamp = g_flightNetClockLeadAllowanceMs + g_serverTickTime;
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_NOTICE;
	g_flightNetScratchPacket.payloadDwords[0] = 0;

	NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket, RESYNC_NOTICE_SIZE);
#endif
}

// FUNCTION: XVT 0x465390
int FlightNet_SendWorldStateResyncToPlayer(int directPlayId, uint8_t* worldState, int worldStateSize) {
#ifdef XVT_MODERN
	return XvtResync_BeginSend(directPlayId, worldState, worldStateSize);
#else
	enum {
		CHECKSUM_RETRY_COUNT = 10,
		CHECKSUM_POLL_INTERVAL_MS = 236,
		CHUNK_RECORD_HEADER_SIZE = sizeof(FlightNetWorldStateChunkRecordHeader),
		CHUNK_FREE_BYTES = sizeof(g_flightNetWorldStateChunkPackets[0].payload) - CHUNK_RECORD_HEADER_SIZE,
		CHUNK_FLUSH_THRESHOLD = 8 * sizeof(int),
		CHUNK_FINAL_SEND_THRESHOLD = sizeof(g_flightNetWorldStateChunkPackets[0].payload) - sizeof(int),
		CHUNK_PACKET_SEND_BASE_SIZE = sizeof(FlightNetWorldStateChunkPacket) - sizeof(int),
		CHUNK_BATCH_SIZE =
			sizeof(g_flightNetWorldStateChunkPackets) / sizeof(g_flightNetWorldStateChunkPackets[0]),
		ALERT_TEXT_COLOR = 0x30,
	};

	int stillLoadingElapsed;
	int alertToggle;
	char statusText[256];
	int chunkSlot;
	int segmentIndex;
	int worldOffset;
	int packetFreeBytes;
	int buildResult;
	int segmentSize;
	int result;
	uint8_t* payload;
	char* playerName;

	result = 1;
	buildResult = Time_GetFrameDelta();
	stillLoadingElapsed = 0;
	g_inputTimestamp += buildResult;
	FlightAlert_SaveBoxBackground();

	strcpy(statusText, g_strDiskIoMessages[DISK_IO_STR_COM_FAILURE_SENDING]);
	playerName = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(directPlayId));
	if (playerName != NULL) {
		strcat(statusText, playerName);
	}
	FlightAlert_DrawBox(1, statusText, ALERT_TEXT_COLOR);

	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_NOTICE;
	g_flightNetScratchPacket.payloadDwords[0] = directPlayId;

	NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket, 2 * sizeof(int));

	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_REQUEST;
	g_flightNetScratchPacket.payloadDwords[0] = (int)g_flightNetWorldChecksumEpoch;
	buildResult = Flight_BuildWorldStateObjectPresenceMap(
		(uint8_t*)&g_flightNetScratchPacket.payloadDwords[1], worldState);

	NetSession_SendPacket(directPlayId, (unsigned int*)&g_flightNetScratchPacket,
						  buildResult + 2 * sizeof(int));

	{
		int retryCount;

		g_flightNetPendingAckCount = 1;
		alertToggle = 0;
		retryCount = CHECKSUM_RETRY_COUNT;
		do {
			int elapsedThisPass;
			int savedInputTimestamp;

			elapsedThisPass = 0;
			g_flightNetRemoteResyncChecksumsReceivedFlag = 0;
			while (elapsedThisPass < CHECKSUM_POLL_INTERVAL_MS) {
				if (FlightInput_HasKeyReady() != 0 && FlightInput_GetNextKey() == FLIGHT_KEY_ESCAPE) {
					retryCount = 1;
					elapsedThisPass = CHECKSUM_POLL_INTERVAL_MS;
					break;
				}

				savedInputTimestamp = g_inputTimestamp;
				FlightNet_ProcessIncomingPackets();
				elapsedThisPass -= savedInputTimestamp;
				g_inputTimestamp += Time_GetFrameDelta();
				elapsedThisPass += g_inputTimestamp;
				g_inputTimestamp = savedInputTimestamp;
				if (g_flightNetRemoteResyncChecksumsReceivedFlag != 0) {
					break;
				}
			}

			stillLoadingElapsed += elapsedThisPass;
			if (stillLoadingElapsed >= CHECKSUM_POLL_INTERVAL_MS) {
				stillLoadingElapsed = 0;
				g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;

				NetSession_SendPacket(directPlayId, (unsigned int*)&g_flightNetScratchPacket, sizeof(int));

				NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket, sizeof(int));
				alertToggle = !alertToggle;
				if (alertToggle != 0) {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_ESC_BOOT_PLAYER],
										ALERT_TEXT_COLOR);
				} else {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_RECOVERING_WAIT],
										ALERT_TEXT_COLOR);
				}
			}
			if (g_flightNetRemoteResyncChecksumsReceivedFlag != 0) {
				break;
			}
			--retryCount;
		} while (retryCount != 0 && g_flightNetRemoteResyncChecksumsReceivedFlag == 0);
	}

	if (g_flightNetRemoteResyncChecksumsReceivedFlag == 0) {
		FlightNet_BroadcastPlayerAbort(NetSession_FindPlayerSlotByDpid(directPlayId));
		FlightAlert_RestoreBoxBackground();
		Time_GetFrameDelta();
		g_flightNetPendingAckCount = 0;
		return 0;
	}

	chunkSlot = 0;
	buildResult = Flight_BuildWorldStateResyncSegmentChecksums(g_flightNetLocalResyncChecksums, worldState,
															   worldStateSize);
	memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
	worldOffset = 0;
	segmentSize = Flight_ComputeWorldStateResyncSegmentSize(worldStateSize);
	g_flightNetWorldStateChunkPackets[0].packetType = NET_PACKET_RESYNC_CHUNK;
	packetFreeBytes = CHUNK_FREE_BYTES;
	g_flightNetWorldStateChunkPackets[0].baseChecksum = (int)g_flightNetWorldChecksumEpoch;
	payload = g_flightNetWorldStateChunkPackets[0].payload;
	g_flightNetWorldStateChunkPackets[0].chunkIndex = 0;

	for (segmentIndex = 0; segmentIndex < buildResult; ++segmentIndex) {
		int remainingSegmentBytes;

		if (g_flightNetRemoteResyncChecksums[segmentIndex] == g_flightNetLocalResyncChecksums[segmentIndex]) {
			worldOffset += segmentSize;
			continue;
		}

		remainingSegmentBytes = segmentSize;
		if (worldOffset + remainingSegmentBytes > worldStateSize) {
			remainingSegmentBytes = worldStateSize - worldOffset;
			if (remainingSegmentBytes < 0) {
				remainingSegmentBytes = 0;
			}
		}

		while (remainingSegmentBytes != 0) {
			FlightNetWorldStateChunkRecordHeader* recordHeader;
			int recordBytes;

			recordBytes = remainingSegmentBytes + CHUNK_RECORD_HEADER_SIZE;
			if (recordBytes > packetFreeBytes) {
				recordBytes = packetFreeBytes;
			}
			recordHeader = (FlightNetWorldStateChunkRecordHeader*)payload;
			recordHeader->worldOffset = worldOffset;
			recordHeader->dataSize = recordBytes - CHUNK_RECORD_HEADER_SIZE;
			memcpy(payload + CHUNK_RECORD_HEADER_SIZE, &worldState[worldOffset],
				   (size_t)recordHeader->dataSize);
			remainingSegmentBytes -= recordHeader->dataSize;
			worldOffset += recordHeader->dataSize;
			packetFreeBytes -= recordBytes;
			payload += recordBytes;

			if ((unsigned int)packetFreeBytes < CHUNK_FLUSH_THRESHOLD) {
				memset(payload, UINT8_MAX, sizeof(int));

				NetSession_SendPacket(directPlayId,
									  (unsigned int*)&g_flightNetWorldStateChunkPackets[chunkSlot],
									  CHUNK_PACKET_SEND_BASE_SIZE - packetFreeBytes);
				++chunkSlot;
				if (chunkSlot == CHUNK_BATCH_SIZE) {
					if (FlightNet_WaitForWorldStateChunkAcks(directPlayId, chunkSlot) == 0) {
						FlightAlert_RestoreBoxBackground();
						Time_GetFrameDelta();
						g_flightNetPendingAckCount = 0;
						return 0;
					}
					g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;

					NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket,
														sizeof(int));
					alertToggle = !alertToggle;
					if (alertToggle != 0) {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_ESC_BOOT_PLAYER],
											ALERT_TEXT_COLOR);
					} else {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_RECOVERING_WAIT],
											ALERT_TEXT_COLOR);
					}
					chunkSlot = 0;
					memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
				}

				packetFreeBytes = CHUNK_FREE_BYTES;
				g_flightNetWorldStateChunkPackets[chunkSlot].packetType = NET_PACKET_RESYNC_CHUNK;
				g_flightNetWorldStateChunkPackets[chunkSlot].baseChecksum =
					(int)g_flightNetWorldChecksumEpoch;
				g_flightNetWorldStateChunkPackets[chunkSlot].chunkIndex = chunkSlot;
				payload = g_flightNetWorldStateChunkPackets[chunkSlot].payload;
			}
		}
	}

	if ((unsigned int)packetFreeBytes < CHUNK_FINAL_SEND_THRESHOLD) {
		memset(payload, UINT8_MAX, sizeof(int));

		NetSession_SendPacket(directPlayId, (unsigned int*)&g_flightNetWorldStateChunkPackets[chunkSlot],
							  CHUNK_PACKET_SEND_BASE_SIZE - packetFreeBytes);
		if (FlightNet_WaitForWorldStateChunkAcks(directPlayId, chunkSlot + 1) == 0) {
			result = 0;
		}
	}

	FlightAlert_RestoreBoxBackground();
	Time_GetFrameDelta();
	g_flightNetPendingAckCount = 0;
	return result;
#endif
}

// FUNCTION: XVT 0x4658C0
int FlightNet_WaitForWorldStateChunkAcks(int directPlayId, int chunkCount) {
#ifdef XVT_MODERN
	return XvtResync_WaitAcks(directPlayId, chunkCount);
#else
	enum { ACK_POLL_INTERVAL_MS = 236, UNCHANGED_ACK_RETRY_COUNT = 20, ALERT_BACKGROUND_COLOR = 0x30 };

	int ackCount;
	int alertToggle;

	int retryCountdown;
	int stillLoadingElapsed;
	int lastAckCount;

	alertToggle = 0;
	stillLoadingElapsed = alertToggle;
	lastAckCount = alertToggle;
	retryCountdown = UNCHANGED_ACK_RETRY_COUNT;
	do {
		int elapsedThisPass = 0;
		int savedInputTimestamp;

		while (elapsedThisPass < ACK_POLL_INTERVAL_MS) {

			if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == FLIGHT_KEY_ESCAPE) {
				elapsedThisPass = ACK_POLL_INTERVAL_MS;
				ackCount = lastAckCount;
				retryCountdown = 1;
				break;
			}

			savedInputTimestamp = g_inputTimestamp;
			FlightNet_ProcessIncomingPackets();
			elapsedThisPass -= savedInputTimestamp;
			g_inputTimestamp += Time_GetFrameDelta();
			elapsedThisPass += g_inputTimestamp;
			g_inputTimestamp = savedInputTimestamp;

			for (ackCount = 0; ackCount < chunkCount; ++ackCount) {
				if (g_flightNetWorldStateChunkAcked[ackCount] == 0) {
					break;
				}
			}
			if (ackCount == chunkCount) {
				break;
			}
		}

		if (ackCount == chunkCount) {
			break;
		}

		stillLoadingElapsed += elapsedThisPass;
		if (stillLoadingElapsed >= ACK_POLL_INTERVAL_MS) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;

			NetSession_BroadcastPacketToPlayers((unsigned int*)&g_flightNetScratchPacket, sizeof(int));
			alertToggle = !alertToggle;
			if (alertToggle != 0) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_ESC_BOOT_PLAYER],
									ALERT_BACKGROUND_COLOR);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_RESENDING_PACKET_WAIT],
									ALERT_BACKGROUND_COLOR);
			}
		}

		if (lastAckCount == ackCount) {
			--retryCountdown;
		} else {
			retryCountdown = UNCHANGED_ACK_RETRY_COUNT;
		}
		lastAckCount = ackCount;
	} while (retryCountdown != 0);

	if (retryCountdown == 0) {
		FlightNet_BroadcastPlayerAbort(NetSession_FindPlayerSlotByDpid(directPlayId));
		return 0;
	}
	return 1;
#endif
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x465A20
void FlightNet_HandleWorldStateResyncPacket(const int* packet) {
	enum {
		PLAYER_COUNT = 8,
		FULL_TIMESTAMP_CODE = 0x7F,
		TIMESTAMP_CODE_MASK = 0x7F,
		KEY_PRESENT_FLAG = 0x80,
		ALERT_BACKGROUND_COLOR = 0x30,
		HOST_TIMEOUT_MS = 7080,
		COUNTDOWN_TICK_MS = 118,
		COUNTDOWN_SECONDS_THRESHOLD = 50,
		COUNTDOWN_HALF_SECOND_MS = 5
	};

	FlightInputFrameRecord input;
	int decodeValue;
	int senderDpid;
	int countdownValue;
	int receivedMatchingPacket;
	int receivedPayloadSize;
	char statusText[80];

	countdownValue = 0;
	if (packet[0] != NET_PACKET_RESYNC_REQUEST) {
		return;
	}

	g_inputTimestamp += Time_GetFrameDelta();
	FlightAlert_SaveBoxBackground();
	FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_COM_FAILURE_RECEIVING], ALERT_BACKGROUND_COLOR);
	Flight_ApplyWorldStateObjectPresenceMap((const uint8_t*)packet + 2 * sizeof(int));
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_CHECKSUMS;
	receivedPayloadSize =
		(int)(sizeof(int) * Flight_BuildWorldStateResyncSegmentChecksums(
								g_flightNetScratchPacket.payloadDwords, Flight_GetDuplicateWorldStateBuffer(),
								Flight_GetSerializedWorldStateSize()));

	NetSession_SendPacket(NetSession_GetHostDplayId(), (unsigned int*)&g_flightNetScratchPacket,
						  receivedPayloadSize + sizeof(int));

	for (;;) {
		int* receivedPacket;

		if (FlightInput_HasKeyReady() != 0 && FlightInput_GetNextKey() == FLIGHT_KEY_ESCAPE) {
			break;
		}

		receivedMatchingPacket = 0;
		do {
			int elapsedMs;
			int savedInputTimestamp;
			int timeoutBucket;

			savedInputTimestamp = g_inputTimestamp;
			g_inputTimestamp += Time_GetFrameDelta();
			elapsedMs = g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;
			g_flightNetHostTimeoutElapsedMs += elapsedMs;
			if (g_flightNetHostTimeoutElapsedMs > HOST_TIMEOUT_MS) {
				FlightAlert_RestoreBoxBackground();
				return;
			}

			timeoutBucket = (HOST_TIMEOUT_MS - g_flightNetHostTimeoutElapsedMs) / COUNTDOWN_TICK_MS;
			if (countdownValue != timeoutBucket) {
				countdownValue = timeoutBucket;
				if (timeoutBucket >= COUNTDOWN_SECONDS_THRESHOLD) {
					if ((timeoutBucket & 1) != 0) {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_ESC_DISCONNECT],
											ALERT_BACKGROUND_COLOR);
					} else {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[DISK_IO_STR_RECOVERING_WAIT],
											ALERT_BACKGROUND_COLOR);
					}
				} else {
					sprintf(statusText, g_strDiskIoMessages[DISK_IO_STR_DISCONNECT_COUNTDOWN],
							timeoutBucket / 2, COUNTDOWN_HALF_SECOND_MS * (timeoutBucket & 1));
					FlightAlert_DrawBox(3, statusText, ALERT_BACKGROUND_COLOR);
				}
			}

			receivedPacket = NetSession_ReceiveGamePacket(&senderDpid, &receivedPayloadSize);
			if (receivedPacket == NULL) {
				continue;
			}

			switch (receivedPacket[0]) {
				case NET_PACKET_REMOTE_INPUT: {
					unsigned int timestamp;
					const uint8_t* cursor;
					InputFrame* inserted;
					int playerIndex;
					uint8_t timestampCode;
					unsigned int lowCode;

					playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
					if (g_players[playerIndex].connectedFlag != 0) {
						if (g_flightNetPeerSilenceTicks[playerIndex] > 0) {
							g_flightNetPeerSilenceTicks[playerIndex] = 0;
						}
						FlightSync_DiscardPredictedInputFrames(playerIndex);
						memset(&input, 0, sizeof(input));
						cursor = (const uint8_t*)receivedPacket + sizeof(int);
						timestampCode = *cursor;
						lowCode = (unsigned int)timestampCode & TIMESTAMP_CODE_MASK;
						if (lowCode == FULL_TIMESTAMP_CODE) {
							timestamp = *(const unsigned int*)(cursor + 1);
							if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
								input.key = cursor[5];
								cursor += 6;
							} else {
								cursor += 5;
							}
						} else {
							int previousCode = g_flightNetLastInputDeltaCodeByPlayer[playerIndex];

							decodeValue = lowCode;
							if ((previousCode & TIMESTAMP_CODE_MASK) > decodeValue) {
								previousCode += TIMESTAMP_CODE_MASK + 1;
							}
							timestamp = (unsigned int)decodeValue |
										((unsigned int)previousCode & ~TIMESTAMP_CODE_MASK);
							if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
								input.key = cursor[1];
								cursor += 2;
							} else {
								cursor += 1;
							}
						}
						g_flightNetLastInputDeltaCodeByPlayer[playerIndex] = (int)timestamp;
						input.axisX = (int8_t)(cursor[0] & (uint8_t)~1u);
						input.axisY = (int8_t)(cursor[1] & (uint8_t)~1u);
						input.keyMods = cursor[1] & 1u;
						input.keyMods += input.keyMods;
						input.keyMods |= cursor[0] & 1u;
						inserted = FlightSync_InsertInputFrame(playerIndex, (int)timestamp, &input);
						if (inserted != NULL) {
							int localPlayerId = NetSession_GetLocalPlayerId();

							inserted->applied = 1;
							if (localPlayerId == 0) {
								inserted->applied = 0;
							}
							inserted->valid = 1;
						}
					} else {
						g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_ABORT;
						g_flightNetScratchPacket.payloadDwords[0] = playerIndex;

						NetSession_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
											  2 * sizeof(int));
					}
					break;
				}
				case NET_PACKET_WORLD_MESSAGE: {
					int savedTimestamp = g_inputTimestamp;

					FlightSync_ApplyWorldMessagePacket((uint8_t*)receivedPacket);
					Time_GetFrameDelta();
					g_inputTimestamp = savedTimestamp;
					if (g_flightMissionState.missionEndPending != 0) {
						return;
					}
					break;
				}
				case NET_PACKET_SESSION_ABORT:
					g_flightMissionState.missionEndPending = 1;
					g_flightNetHostAbortReceived = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					return;
				case NET_PACKET_PLAYER_ABORT: {
					int playerIndex = receivedPacket[1];

					if (playerIndex >= 0 && playerIndex < PLAYER_COUNT) {
						g_playerAbortFlags[playerIndex] = 1;
					}
					if (playerIndex == g_localPlayer) {
						g_flightMissionState.missionEndPending = 1;
						g_players[g_localPlayer].connectedFlag = 0;
						g_playerAbortFlags[g_localPlayer] = 1;
						FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
						return;
					}
					break;
				}
				case NET_PACKET_INPUT_BATCH: {
					const uint8_t* cursor;
					int playerIndex;

					playerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
					if (g_players[playerIndex].connectedFlag != 0) {
						if (g_flightNetPeerSilenceTicks[playerIndex] > 0) {
							g_flightNetPeerSilenceTicks[playerIndex] = 0;
						}
						cursor = (const uint8_t*)receivedPacket + sizeof(int);
						FlightSync_DiscardPredictedInputFrames(playerIndex);
						memset(&input, 0, sizeof(input));
						decodeValue = *cursor++;
						while (decodeValue > 0) {
							InputFrame* inserted;
							uint8_t lowCode;
							uint8_t timestampCode;
							unsigned int timestamp;

							timestampCode = *cursor;
							lowCode = timestampCode & TIMESTAMP_CODE_MASK;
							if (lowCode == FULL_TIMESTAMP_CODE) {
								timestamp = *(const unsigned int*)(cursor + 1);
								if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
									input.key = cursor[5];
									cursor += 6;
								} else {
									cursor += 5;
								}
							} else {
								int previousCode = g_flightNetLastInputDeltaCodeByPlayer[playerIndex];

								if ((previousCode & TIMESTAMP_CODE_MASK) > lowCode) {
									previousCode += TIMESTAMP_CODE_MASK + 1;
								}
								timestamp = (unsigned int)lowCode |
											((unsigned int)previousCode & ~TIMESTAMP_CODE_MASK);
								if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
									input.key = cursor[1];
									cursor += 2;
								} else {
									cursor += 1;
								}
							}
							g_flightNetLastInputDeltaCodeByPlayer[playerIndex] = (int)timestamp;
							input.axisX = (int8_t)(cursor[0] & (uint8_t)~1u);
							input.axisY = (int8_t)(cursor[1] & (uint8_t)~1u);
							input.keyMods = cursor[1] & 1u;
							input.keyMods += input.keyMods;
							input.keyMods |= cursor[0] & 1u;
							cursor += 2;
							inserted = FlightSync_InsertInputFrame(playerIndex, (int)timestamp, &input);
							if (inserted != NULL) {
								int localPlayerId = NetSession_GetLocalPlayerId();

								inserted->applied = 1;
								if (localPlayerId == 0) {
									inserted->applied = 0;
								}
								inserted->valid = 1;
							}
							--decodeValue;
						}
					} else {
						g_flightNetScratchPacket.packetType = NET_PACKET_PLAYER_ABORT;
						g_flightNetScratchPacket.payloadDwords[0] = playerIndex;

						NetSession_SendPacket(senderDpid, (unsigned int*)&g_flightNetScratchPacket,
											  2 * sizeof(int));
					}
					break;
				}
				case NET_PACKET_RESYNC_NOTICE:
					g_flightNetResyncPlayerDplayId = receivedPacket[1];
					break;
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
					break;
				case NET_PACKET_RESYNC_REQUEST:
					FlightNet_BroadcastPlayerAbort(g_localPlayer);
					g_flightMissionState.missionEndPending = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					g_playerAbortFlags[g_localPlayer] = 1;
					FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
					return;
				case NET_PACKET_RESYNC_APPLY:
				case NET_PACKET_RESYNC_CHUNK:
					if (receivedPacket[1] == (int)g_flightNetWorldChecksumEpoch) {
						receivedMatchingPacket = 1;
					}
					break;
				default:
					break;
			}
		} while (receivedMatchingPacket == 0);

		if (receivedPacket[0] == NET_PACKET_RESYNC_APPLY) {
			if (g_playerAbortFlags[g_localPlayer] != 0) {
				g_playerAbortFlags[g_localPlayer] = 1;
				g_flightMissionState.missionEndPending = 1;
				g_players[g_localPlayer].connectedFlag = 0;
				FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
			} else {
				Time_GetFrameDelta();
				FlightSync_ReplayResyncMessages((unsigned int)receivedPacket[2], receivedPacket[1]);
				g_flightNetScratchPacket.packetType = NET_PACKET_ACK;

				NetSession_SendPacket(NetSession_GetHostDplayId(), (unsigned int*)&g_flightNetScratchPacket,
									  sizeof(int));
				g_inputTimestamp = g_flightNetClockLeadAllowanceMs + g_serverTickTime;
				FlightAlert_RestoreBoxBackground();
			}
			return;
		}

		{
			int chunkRequestId = receivedPacket[2];

			while (receivedPacket[3] != -1) {
				FlightSync_CopyWorldStateResyncChunk((const uint8_t*)&receivedPacket[5], receivedPacket[3],
													 (unsigned int)receivedPacket[4]);
				receivedPacket = (int*)((uint8_t*)receivedPacket + 2 * sizeof(int) + receivedPacket[4]);
			}
			g_flightNetScratchPacket.payloadDwords[0] = chunkRequestId;
			g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_CHUNK_ACK;
		}

		NetSession_SendPacket(NetSession_GetHostDplayId(), (unsigned int*)&g_flightNetScratchPacket,
							  2 * sizeof(int));
	}

	FlightNet_BroadcastPlayerAbort(g_localPlayer);
	g_flightMissionState.missionEndPending = 1;
	g_players[g_localPlayer].connectedFlag = 0;
	g_playerAbortFlags[g_localPlayer] = 1;
	FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
}
#endif
