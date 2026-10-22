#include "xvt/net/net_session.h"
#ifdef XVT_MODERN
#include "aeron/aeron.h"
#include "xvt/net/frontend_net.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/network_session.h"
#endif

#include "aeron/compat/dplay.h"
#include "xvt/flight/flight_loading.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/frontend/config.h"
#include "xvt/net/net_reliable.h"
#include "xvt/util/time.h"

#include <string.h>

#pragma pack(push, 1)

typedef struct NetSessionCompactEncodedPacket {
	int16_t packetTypeHeader;
	int16_t payloadSize;
	uint8_t payload[1020];
} NetSessionCompactEncodedPacket;

typedef struct NetSessionSequencedEncodedPacket {
	int16_t packetTypeHeader;
	uint8_t localSequence;
	int16_t payloadSize;
	uint8_t payload[1019];
} NetSessionSequencedEncodedPacket;

#pragma pack(pop)
typedef char
	xvt_size_NetSessionCompactEncodedPacket[(sizeof(NetSessionCompactEncodedPacket) == 1024) ? 1 : -1];
typedef char
	xvt_size_NetSessionSequencedEncodedPacket[(sizeof(NetSessionSequencedEncodedPacket) == 1024) ? 1 : -1];

// GLOBAL: XVT 0x52701C
int g_netSessionRecvHistoryCount = 0;
// GLOBAL: XVT 0x527020
int g_netSessionRecvQueueHighWater = 0;
// GLOBAL: XVT 0x5DD9B8
NetQueuedPacket g_netSessionRecvHistory[128] = { 0 };
// GLOBAL: XVT 0x5EE1B8
NetQueuedPacket g_netSessionExportRecvQueue[256] = { 0 };
// GLOBAL: XVT 0x9994C0
NetSessionState g_netSession = { 0 };
// GLOBAL: XVT 0x5597A0
NetSessionScratchState g_netSessionScratchPacket = { 0 };
// GLOBAL: XVT 0x9EC608
uint8_t g_netSessionFlightHandshakeActive = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x46C220
void NetSession_DebugTrace(const char* message) {
#ifdef XVT_MODERN
	/* The original discards these reliability-layer traces. The ones signalling
	 * lost or overflowing packets are logged, each at most once per second. */
	static const struct {
		const char* trace;
		const char* text;
	} k_traces[] = {
		{ "Ran out of receive buffers!!!",
		  "receive queue full; discarding queued packets not from the host" },
		{ "(RTO) ", "reliable packet overdue; requesting a resend" },
		{ "(TMR) ", "reliable packet resent too often; skipping the gap" },
		{ "(NMR) ", "receive queue full; delivering a reliable packet out of order" },
	};

	static uint64_t lastUs[sizeof k_traces / sizeof k_traces[0]];
	static unsigned suppressed[sizeof k_traces / sizeof k_traces[0]];
	unsigned i;
	uint64_t now;

	for (i = 0; i < sizeof k_traces / sizeof k_traces[0]; ++i)
		if (!strcmp(message, k_traces[i].trace))
			break;
	if (i == sizeof k_traces / sizeof k_traces[0])
		return;
	now = Aeron_NowUs();
	if (lastUs[i] && now - lastUs[i] < 1000000) {
		++suppressed[i];
		return;
	}
	if (suppressed[i])
		Aeron_LogWarn("xvt.net", "%s (%u more in the last second)", k_traces[i].text, suppressed[i]);
	else
		Aeron_LogWarn("xvt.net", "%s", k_traces[i].text);
	lastUs[i] = now;
	suppressed[i] = 0;
#else
	(void)message;
#endif
}

// FUNCTION: XVT 0x46C230
int NetSession_InitGameSession(const char* sessionName, const char* pilotName, int localId,
							   const char* mpGameName, NetworkTransportType networkType, int numHumanPlayers,
							   int inProgressLaunch, const char* connectionAddress) {
	DPCAPS directPlayCaps;
	char dialNumber[32] = "Dial a New Number.";
	int playerIndex;
	int success;
#ifndef XVT_MODERN
	int outAux;
	int outDpid;
	int* packet;
	int receivedPlayerCount;
	int rosterIndex;
	int* rosterEntry;
#endif

	(void)dialNumber;
	(void)mpGameName;
	(void)connectionAddress;
	NetSession_DebugTrace("Init network");
	memset(&g_netSession, 0, sizeof(g_netSession));
	g_netSessionFlightHandshakeActive = 1;
	g_netSessionScratchPacket.trailingState = 0;
	g_netSession.networkType = networkType;
	g_netSession.broadcastSeqCounter = 0;
	g_netSession.broadcastPendingFlush = 1;
	g_netSession.broadcastPayload[0] = NET_PACKET_NOP;
	g_netSession.broadcastPayloadLength = 1;
	g_netSession.groupSeqCounter = 0;
	g_netSession.groupPendingFlush = 1;
	g_netSession.groupPayload[0] = NET_PACKET_NOP;
	g_netSession.groupPayloadLength = 1;
	g_netSession.reliableUseFixedResendTimeouts = 0;
	for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
		g_netSession.reliablePeerSlots[playerIndex].prevRecvSeqDefault = 127;
		g_netSession.reliablePeerSlots[playerIndex].prevRecvSeqChannelA = 127;
		g_netSession.reliablePeerSlots[playerIndex].prevRecvSeqChannelB = 127;
		g_netSession.reliablePeerSlots[playerIndex].recvSeqDefault = 127;
		g_netSession.reliablePeerSlots[playerIndex].recvSeqChannelA = 127;
		g_netSession.reliablePeerSlots[playerIndex].recvSeqChannelB = 127;
		g_netSession.reliablePeerSlots[playerIndex].sendSeq = 0;
		g_netSession.reliablePeerSlots[playerIndex].directPlayId = 0;
		g_netSession.reliablePeerSlots[playerIndex].lastPiggybackType = NET_PACKET_NOP;
		g_netSession.reliablePeerSlots[playerIndex].piggybackLength = 1;
		g_netSession.reliablePeerSlots[playerIndex].lastActivityMs = 0;
		g_netSession.reliablePeerSlots[playerIndex].packetCount = 0;
		g_netSession.reliablePeerSlots[playerIndex].packetDropCount = 0;
	}
	g_netSessionRecvQueueHighWater = 0;
	memset(g_netSessionExportRecvQueue, 0, sizeof(g_netSessionExportRecvQueue));
	g_netRecvQueueCount = 0;
	g_netSession.reliablePeerSlotCount = 0;
	success = 1;

	if (numHumanPlayers == success && localId == success) {
		/* The single-player path reuses the persisted DirectPlay snapshot. */
		NetSession_ImportRuntimeState(
			(void**)&g_netSession.dplayInterface, &g_netSession.appGuid, &g_netSession.instanceGuid,
			(int32_t*)&g_netSession.groupDplayId, &g_netSession.hostDplayId,
			(NetPlayerInfo*)&g_netSession.localPlayerInfo, g_netSessionRecvQueue, &g_netRecvQueueReadIndex,
			(int*)&g_netRecvQueueCount, &g_netRecvQueueWriteIndex, g_netSession.reliablePeerSlots,
			&g_netSession.reliablePeerSlotCount, &g_netSession.broadcastSeqCounter,
			(char*)g_netSession.broadcastPayload, &g_netSession.broadcastPayloadLength,
			&g_netSession.broadcastPendingFlush, &g_netSession.groupSeqCounter,
			(char*)g_netSession.groupPayload, &g_netSession.groupPayloadLength,
			&g_netSession.groupPendingFlush, g_netSessionRecvHistory, &g_netSessionRecvHistoryCount);
		if (g_netSession.dplayInterface == NULL) {
			g_netSession.localPlayerInfo.directPlayId = success;
			g_netSession.localPlayerInfo.activeFlag = success;
		} else {
			memset(&directPlayCaps, 0, sizeof(directPlayCaps));
			directPlayCaps.dwSize = sizeof(directPlayCaps);
			g_netSession.dplayInterface->lpVtbl->GetCaps(g_netSession.dplayInterface, &directPlayCaps, 0);
		}
		strncpy(g_netSession.localPlayerInfo.sessionName, sessionName,
				sizeof(g_netSession.localPlayerInfo.sessionName));
		strncpy(g_netSession.localPlayerInfo.playerName, pilotName,
				sizeof(g_netSession.localPlayerInfo.playerName));
		g_netSession.players[0] = g_netSession.localPlayerInfo;
		g_netSession.localPlayerId = localId;
		g_netSession.playerCount = success;
		return success;
	}

	NetSession_ImportRuntimeState(
		(void**)&g_netSession.dplayInterface, &g_netSession.appGuid, &g_netSession.instanceGuid,
		(int32_t*)&g_netSession.groupDplayId, &g_netSession.hostDplayId,
		(NetPlayerInfo*)&g_netSession.localPlayerInfo, g_netSessionRecvQueue, &g_netRecvQueueReadIndex,
		(int*)&g_netRecvQueueCount, &g_netRecvQueueWriteIndex, g_netSession.reliablePeerSlots,
		&g_netSession.reliablePeerSlotCount, &g_netSession.broadcastSeqCounter,
		(char*)g_netSession.broadcastPayload, &g_netSession.broadcastPayloadLength,
		&g_netSession.broadcastPendingFlush, &g_netSession.groupSeqCounter, (char*)g_netSession.groupPayload,
		&g_netSession.groupPayloadLength, &g_netSession.groupPendingFlush, g_netSessionRecvHistory,
		&g_netSessionRecvHistoryCount);
	memset(&directPlayCaps, 0, sizeof(directPlayCaps));
	directPlayCaps.dwSize = sizeof(directPlayCaps);
	g_netSession.dplayInterface->lpVtbl->GetCaps(g_netSession.dplayInterface, &directPlayCaps, 0);
	g_netSession.playerCount = 0;
	g_netSession.localPlayerId = localId;
	NetSession_EnumeratePlayers();
#ifndef XVT_MODERN
	receivedPlayerCount = numHumanPlayers;
#endif
	g_netSessionFlightHandshakeActive = 1;
	timeGetTime();
	g_netSessionScratchPacket.packetType = NET_PACKET_STARTUP_READY;
	NetSession_SendPacket(g_netSession.hostDplayId, (unsigned int*)&g_netSessionScratchPacket, 4);
	g_netSessionScratchPacket.packetType = NET_PACKET_NOP;
	NetSession_SendPacket(g_netSession.hostDplayId, (unsigned int*)&g_netSessionScratchPacket, 4);

#ifdef XVT_MODERN
	return XvtFlightNetwork_BeginSession(numHumanPlayers, inProgressLaunch);
#else
	if (NetSession_GetLocalPlayerId() != 0 && numHumanPlayers != 0) {
		while (receivedPlayerCount != 0) {
			packet = NetSession_WaitForGamePacket(&outDpid, &outAux, 60);
			if (packet == NULL) {
				g_netSession.dplayInterface = NULL;
				return 0;
			}
			if (*packet == NET_PACKET_STARTUP_READY)
				--receivedPlayerCount;
		}
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		if (numHumanPlayers > 1) {
			NetSession_BroadcastPlayerRoster(0);
			g_netSessionScratchPacket.packetType = NET_PACKET_NOP;
			NetSession_SendPacket(0, (unsigned int*)&g_netSessionScratchPacket, 4);
		}
	} else if (inProgressLaunch == 0) {
		do {
			packet = NetSession_WaitForGamePacket(&outDpid, &outAux, 60);
			if (packet == NULL) {
				g_netSession.dplayInterface = NULL;
				return 0;
			}
		} while (*packet != NET_PACKET_ROSTER_COUNT);

		g_netSession.playerCount = packet[1];
		receivedPlayerCount = 0;
		while (g_netSession.playerCount > receivedPlayerCount) {
			do {
				packet = NetSession_WaitForGamePacket(&outDpid, &outAux, 60);
				if (packet == NULL)
					return 0;
			} while (*packet != NET_PACKET_ROSTER_ENTRY);
			rosterEntry = packet + 2;
			rosterIndex = packet[1];
			memcpy(&g_netSession.players[rosterIndex], rosterEntry, sizeof(SessionPlayerInfo));
			++receivedPlayerCount;
		}
	}
	return success;
#endif
}

// FUNCTION: XVT 0x46C6A0
int NetSession_Shutdown(void) {
	NetSession_ExportRuntimeState(
		(void**)&g_netSession.dplayInterface, &g_netSession.appGuid, &g_netSession.instanceGuid,
		(int*)&g_netSession.groupDplayId, &g_netSession.hostDplayId, &g_netSession.localPlayerInfo,
		g_netSessionRecvQueue, &g_netRecvQueueReadIndex, (int*)&g_netRecvQueueCount,
		&g_netRecvQueueWriteIndex, g_netSession.reliablePeerSlots, (int*)&g_netSession.reliablePeerSlotCount,
		(int*)&g_netSession.broadcastSeqCounter, g_netSession.broadcastPayload,
		&g_netSession.broadcastPayloadLength, &g_netSession.broadcastPendingFlush,
		(int*)&g_netSession.groupSeqCounter, g_netSession.groupPayload, &g_netSession.groupPayloadLength,
		&g_netSession.groupPendingFlush, g_netSessionRecvHistory, &g_netSessionRecvHistoryCount,
		g_netSessionExportRecvQueue, &g_netSessionRecvQueueHighWater);
	return 1;
}

// FUNCTION: XVT 0x46C730
int NetSession_EnumeratePlayers(void) {
	g_netSession.dplayInterface->lpVtbl->EnumPlayers(g_netSession.dplayInterface, 0,
													 NetSession_EnumPlayersCallback, 0, 0);
	return 1;
}

// FUNCTION: XVT 0x46C750
int AERON_DXAPI NetSession_EnumPlayersCallback(DPID dplayId, uint32_t playerType, const DPNAME* nameInfo,
											   uint32_t flags, void* context) {
	char* nameEnd;
	int* playerValue;

	(void)flags;
	(void)context;

	if (playerType == 0) {
		return 1;
	}
	if (g_netSession.playerCount >= 8) {
		return 0;
	}
	if (PilotData_HasNetworkPlayerDpid(dplayId) != 0) {
		strncpy(g_netSession.players[g_netSession.playerCount].sessionName, nameInfo->lpszLongNameA, 16);
		strncpy(g_netSession.players[g_netSession.playerCount].playerName, nameInfo->lpszShortNameA, 16);
		nameEnd = &g_netSession.players[g_netSession.playerCount].sessionName[15];
		*nameEnd = '\0';
		nameEnd = &g_netSession.players[g_netSession.playerCount].playerName[15];
		*nameEnd = '\0';
		playerValue = &g_netSession.players[g_netSession.playerCount].directPlayId;
		*playerValue = dplayId;
		playerValue = &g_netSession.players[g_netSession.playerCount].activeFlag;
		*playerValue = 1;
		g_netSession.playerCount++;
	}
	return 1;
}

enum {
	RECEIVE_QUEUE_CAPACITY = 1024,
	RECEIVE_QUEUE_LIMIT = RECEIVE_QUEUE_CAPACITY - 1,
	HISTORY_CAPACITY = 128,
	WORLD_HISTORY_CAPACITY = 256,
	MAX_PAYLOAD_SIZE = 508,
	SEQUENCE_MODULUS = 128,
	SEQUENCE_NONE = 128
};

// FUNCTION: XVT 0x46C830
void NetSession_PumpIncomingPackets(void) {
	static int receiveSuppressCount;
	DPID fromId;
	DPID toId;
	struct {
		uint16_t header;
		uint8_t data[1022];
	} wirePacket;
	unsigned int responsePacket[2];
	uint32_t wireSize;
	uint8_t* packetData;
	uint32_t packetSize;
	unsigned int packetType;
	int sequence;
	int groupChannel;
	int broadcastChannel;
	int hasLength;
	unsigned int peerSlot;
	int duplicate;
	int expectedSequence;
	uint8_t* piggyback;
	uint32_t piggybackSize;
	unsigned int piggybackType;
	if (g_netSession.dplayInterface == NULL)
		return;
	g_netSession.receivePumpState = 0;
	for (;;) {
		if ((int)g_netRecvQueueCount >= RECEIVE_QUEUE_LIMIT) {
			NetSession_DebugTrace("Ran out of receive buffers!!!");
			NetReliable_CompactLocalReceiveQueue();
			return;
		}
		wireSize = sizeof(wirePacket);
		if (g_netSession.dplayInterface->lpVtbl->Receive(g_netSession.dplayInterface, &fromId, &toId, 1,
														 &wirePacket, &wireSize) != 0)
			return;
		if (fromId == 0) {
			NetSession_DebugTrace("(RSM)");
			if (wireSize > sizeof(g_netSessionRecvQueue[0].payload))
				wireSize = sizeof(g_netSessionRecvQueue[0].payload);
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].directPlayId = fromId;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payloadSize = wireSize;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].queuedFlag = 0;
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, &wirePacket.header, wireSize);
			++g_netRecvQueueWriteIndex;
			++g_netRecvQueueCount;
			if (g_netRecvQueueWriteIndex >= RECEIVE_QUEUE_CAPACITY)
				g_netRecvQueueWriteIndex = 0;
			continue;
		}
		if (receiveSuppressCount != 0) {
			--receiveSuppressCount;
			continue;
		}
		if (toId != (DPID)g_netSession.localPlayerInfo.directPlayId)
			continue;
		packetType = wirePacket.header & 0x7F;
		groupChannel = (wirePacket.header & 0x80) != 0;
		broadcastChannel = (wirePacket.header & 0x8000) == 0;
		sequence = (wirePacket.header >> 8) & 0x7F;
		packetData = wirePacket.data;
		packetSize = wireSize - sizeof(wirePacket.header);
		hasLength = packetType < NET_PACKET_RESYNC_CHECKSUMS || packetType > NET_PACKET_RESYNC_CHUNK;
		if (hasLength && !(broadcastChannel && groupChannel) &&
			NetSession_ExitStub(packetType) == 0 && packetSize >= sizeof(uint16_t)) {
			uint16_t encodedSize;
			memcpy(&encodedSize, packetData, sizeof(encodedSize));
			packetData += sizeof(encodedSize);
			packetSize = encodedSize;
		}
		if (packetSize > MAX_PAYLOAD_SIZE)
			packetSize = MAX_PAYLOAD_SIZE;
		if (packetType == NET_PACKET_PING) {
			responsePacket[0] = NET_PACKET_PONG;
			NetSession_SendPacket(fromId, responsePacket, sizeof(uint32_t));
			continue;
		}
		if (packetType == NET_PACKET_KEEPALIVE_ACK)
			continue;
		if (packetType == NET_PACKET_WORLD_NACK) {
			if (packetSize >= 2 * sizeof(int)) {
				unsigned int searchCount;
				unsigned int worldCursor;
				int timestamp;
				int requestedSequence;
				memcpy(&timestamp, packetData, sizeof(timestamp));
				memcpy(&requestedSequence, packetData + sizeof(timestamp), sizeof(requestedSequence));
				peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40)
					++g_netSession.reliablePeerSlots[peerSlot].packetDropCount;
				worldCursor = (unsigned int)g_netSessionRecvQueueHighWater;
				for (searchCount = 0; searchCount < WORLD_HISTORY_CAPACITY; ++searchCount) {
					if (g_netSessionExportRecvQueue[worldCursor].payloadSize != 0 &&
						((*((uint32_t*)&g_netSessionExportRecvQueue[worldCursor].payload[4]) & 0x7FFFFFFF) ==
						 (uint32_t)timestamp))
						break;
					if (++worldCursor >= WORLD_HISTORY_CAPACITY)
						worldCursor = 0;
				}
				if (searchCount < WORLD_HISTORY_CAPACITY) {
					NetSession_SendSequencedGamePacket(
						fromId, 0, g_netSessionExportRecvQueue[worldCursor].sequenceByte,
						(const unsigned int*)g_netSessionExportRecvQueue[worldCursor].payload,
						g_netSessionExportRecvQueue[worldCursor].payloadSize);
				} else {
					responsePacket[0] = NET_PACKET_NOP;
					NetSession_SendSequencedGamePacket(fromId, 0, (uint8_t)requestedSequence,
													   responsePacket,
													   sizeof(uint32_t));
				}
			}
			continue;
		}
		if (packetType == NET_PACKET_NACK) {
			if (packetSize >= 2 * sizeof(int)) {
				int requestedSequence;
				int requestedClass;
				unsigned int searchCount;
				unsigned int historyCursor;
				memcpy(&requestedSequence, packetData, sizeof(requestedSequence));
				memcpy(&requestedClass, packetData + sizeof(requestedSequence), sizeof(requestedClass));
				peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40)
					++g_netSession.reliablePeerSlots[peerSlot].packetDropCount;
				historyCursor = (unsigned int)g_netSessionRecvHistoryCount;
				for (searchCount = 0; searchCount < HISTORY_CAPACITY; ++searchCount) {
					if (g_netSessionRecvHistory[historyCursor].payloadSize != 0 &&
						g_netSessionRecvHistory[historyCursor].sequenceByte == requestedSequence &&
						g_netSessionRecvHistory[historyCursor].packetClass == requestedClass &&
						(requestedClass == 0 || requestedClass == 2 ||
						 g_netSessionRecvHistory[historyCursor].directPlayId == fromId))
						break;
					if (++historyCursor >= HISTORY_CAPACITY)
						historyCursor = 0;
				}
				if (searchCount < HISTORY_CAPACITY) {
					NetSession_SendSequencedGamePacket(
						fromId, (uint8_t)requestedClass, g_netSessionRecvHistory[historyCursor].sequenceByte,
						(const unsigned int*)g_netSessionRecvHistory[historyCursor].payload,
						g_netSessionRecvHistory[historyCursor].payloadSize);
				} else {
					responsePacket[0] = NET_PACKET_NOP;
					NetSession_SendSequencedGamePacket(
						fromId, (uint8_t)requestedClass, (uint8_t)requestedSequence,
						responsePacket, sizeof(uint32_t));
				}
			}
			continue;
		}
		if (packetType == NET_PACKET_KEEPALIVE) {
			if (packetSize >= 3 * sizeof(int)) {
				unsigned int searchCount;
				unsigned int historyCursor;
				uint8_t packetClass;
				int expectedBroadcast;
				int expectedGroup;
				int expectedDirected;
				memcpy(&expectedBroadcast, packetData, sizeof(expectedBroadcast));
				memcpy(&expectedGroup, packetData + sizeof(expectedBroadcast), sizeof(expectedGroup));
				memcpy(&expectedDirected, packetData + 2 * sizeof(expectedBroadcast),
					   sizeof(expectedDirected));
				if (expectedBroadcast == (int)g_netSession.broadcastSeqCounter)
					expectedBroadcast = SEQUENCE_NONE;
				if (expectedGroup == (int)g_netSession.groupSeqCounter)
					expectedGroup = SEQUENCE_NONE;
				peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
				if (peerSlot >= g_netSession.reliablePeerSlotCount ||
					g_netSession.reliablePeerSlots[peerSlot].sendSeq == expectedDirected)
					expectedDirected = SEQUENCE_NONE;
				historyCursor = (unsigned int)g_netSessionRecvHistoryCount;
				for (searchCount = 0; searchCount < HISTORY_CAPACITY &&
									  (expectedBroadcast != SEQUENCE_NONE || expectedGroup != SEQUENCE_NONE ||
									   expectedDirected != SEQUENCE_NONE);
					 ++searchCount) {
					if (g_netSessionRecvHistory[historyCursor].payloadSize != 0) {
						packetClass = g_netSessionRecvHistory[historyCursor].packetClass;
						if (packetClass == 0) {
							if (g_netSessionRecvHistory[historyCursor].sequenceByte == expectedBroadcast) {
								NetSession_SendSequencedGamePacket(fromId, 0, expectedBroadcast,
									(const unsigned int*)g_netSessionRecvHistory[historyCursor].payload,
									g_netSessionRecvHistory[historyCursor].payloadSize);
								expectedBroadcast = SEQUENCE_NONE;
							}
						} else if (packetClass == 2) {
							if (g_netSessionRecvHistory[historyCursor].sequenceByte == expectedGroup) {
								NetSession_SendSequencedGamePacket(fromId, 2, expectedGroup,
									(const unsigned int*)g_netSessionRecvHistory[historyCursor].payload,
									g_netSessionRecvHistory[historyCursor].payloadSize);
								expectedGroup = SEQUENCE_NONE;
							}
						} else if (g_netSessionRecvHistory[historyCursor].directPlayId == fromId &&
							g_netSessionRecvHistory[historyCursor].sequenceByte == expectedDirected) {
							NetSession_SendSequencedGamePacket(fromId, 1, expectedDirected,
								(const unsigned int*)g_netSessionRecvHistory[historyCursor].payload,
								g_netSessionRecvHistory[historyCursor].payloadSize);
							expectedDirected = SEQUENCE_NONE;
						}
					}
					if (++historyCursor >= HISTORY_CAPACITY)
						historyCursor = 0;
				}
			}
			continue;
		}
		peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
		if (broadcastChannel && groupChannel) {
			uint8_t channelMarker;
			const uint8_t* appPayload;
			uint32_t appPayloadSize;
			channelMarker = packetSize != 0 ? packetData[0] : 0;
			appPayload = packetSize != 0 ? packetData + 1 : packetData;
			appPayloadSize = packetSize != 0 ? packetSize - 1 : 0;
			if (hasLength && NetSession_ExitStub(packetType) == 0 &&
				appPayloadSize >= sizeof(uint16_t)) {
				uint16_t encodedSize;
				memcpy(&encodedSize, appPayload, sizeof(encodedSize));
				appPayload += sizeof(encodedSize);
				appPayloadSize = encodedSize;
			}
			if (appPayloadSize > MAX_PAYLOAD_SIZE)
				appPayloadSize = MAX_PAYLOAD_SIZE;
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, &packetType, sizeof(packetType));
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload + sizeof(packetType), appPayload, appPayloadSize);
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].directPlayId = fromId;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payloadSize = appPayloadSize + sizeof(packetType);
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].aux = 0;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].meta0 = 0;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].queuedFlag = 1;
			peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
			if (channelMarker == 0) {
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 0;
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40)
					NetSession_AdvanceReceivedSequence(&g_netSession.reliablePeerSlots[peerSlot].recvSeqChannelA, sequence);
			} else if (channelMarker == 2) {
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 2;
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40)
					NetSession_AdvanceReceivedSequence(&g_netSession.reliablePeerSlots[peerSlot].recvSeqChannelB, sequence);
			} else {
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 1;
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40)
					NetSession_AdvanceReceivedSequence(&g_netSession.reliablePeerSlots[peerSlot].recvSeqDefault, sequence);
			}
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].sequenceByte = (uint8_t)sequence;
			++g_netRecvQueueCount;
			++g_netRecvQueueWriteIndex;
			if (g_netRecvQueueWriteIndex >= RECEIVE_QUEUE_CAPACITY)
				g_netRecvQueueWriteIndex = 0;
			continue;
		}
		peerSlot = NetReliable_FindOrCreatePeerSlot(fromId);
		if (hasLength) {
			expectedSequence = sequence == 0 ? SEQUENCE_MODULUS - 1 : sequence - 1;
			duplicate =
				NetReliable_IsDuplicateRecvSequence(fromId, expectedSequence, broadcastChannel, groupChannel);
			if (!duplicate) {
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 40 && !groupChannel)
					++g_netSession.reliablePeerSlots[peerSlot].packetDropCount;
				piggyback = packetData + packetSize;
				piggybackSize = wireSize - (unsigned int)(piggyback - (uint8_t*)&wirePacket.header);
				if (piggybackSize > 0) {
					piggybackType = piggyback[0];
					if (piggybackType != NET_PACKET_NOP) {
						if (piggybackSize - 1 > MAX_PAYLOAD_SIZE)
							piggybackSize = MAX_PAYLOAD_SIZE + 1;
						memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, &piggybackType, sizeof(piggybackType));
						memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload + sizeof(uint32_t), piggyback + 1, piggybackSize - 1);
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].directPlayId = fromId;
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payloadSize = piggybackSize + sizeof(uint32_t) - 1;
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].aux = 0;
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].meta0 = 0;
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].queuedFlag = 0;
						if (broadcastChannel)
							g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 0;
						else {
							g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 2;
							if (!groupChannel)
								g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 1;
						}
						g_netSessionRecvQueue[g_netRecvQueueWriteIndex].sequenceByte = (uint8_t)expectedSequence;
						++g_netRecvQueueWriteIndex;
						++g_netRecvQueueCount;
						if (g_netRecvQueueWriteIndex >= RECEIVE_QUEUE_CAPACITY)
							g_netRecvQueueWriteIndex = 0;
					}
				}
			}
		}
		duplicate = NetReliable_IsDuplicateRecvSequence(fromId, sequence, broadcastChannel, groupChannel);
		if (!groupChannel || !duplicate) {
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, &packetType, sizeof(packetType));
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload + sizeof(packetType), packetData, packetSize);
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].directPlayId = fromId;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payloadSize = packetSize + sizeof(packetType);
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].aux = 0;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].meta0 = 0;
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].queuedFlag = 1;
			if (!duplicate)
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].queuedFlag = 0;
			if (broadcastChannel)
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 0;
			else {
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 2;
				if (!groupChannel)
					g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 1;
			}
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].sequenceByte = (uint8_t)sequence;
			++g_netRecvQueueWriteIndex;
			++g_netRecvQueueCount;
			if (g_netRecvQueueWriteIndex >= RECEIVE_QUEUE_CAPACITY)
				g_netRecvQueueWriteIndex = 0;
		}
	}
}


// FUNCTION: XVT 0x46D300
int NetSession_BroadcastPacketToPlayers(unsigned int* payload, int payloadSize) {
	int result;
	int playerIndex;

	result = g_netSession.playerCount;
	if (result > 0) {
		for (playerIndex = 0; playerIndex < g_netSession.playerCount; ++playerIndex) {
			result = g_netSession.players[playerIndex].directPlayId;
			if (result != 0 && g_netSession.players[playerIndex].activeFlag != 0)
				result = NetSession_SendPacket(result, payload, payloadSize);
		}
	}
	return result;
}

// FUNCTION: XVT 0x46D350
int NetSession_SendPacket(int directPlayId, unsigned int* payload, signed int payloadSize) {
	unsigned int packetType;
	int appendPending;
	uint16_t packetHeader;
	int sendResult;
	NetSessionCompactEncodedPacket encodedPacket;
	uint8_t* encodedPayload;
	int encodedHeaderSize;
	int encodedSize;
	uint8_t packetTypeByte[4];

	sendResult = 0;
	if (payloadSize < 4)
		return 0;

	packetType = *payload;
	packetHeader = packetTypeByte[0] = packetType & 0x7F;
	if (packetType < NET_PACKET_RESYNC_CHECKSUMS || packetType >= NET_PACKET_RESYNC_CHUNK + 1)
		appendPending = 1;
	else
		appendPending = 0;

	if (packetType == NET_PACKET_REMOTE_INPUT && g_gameConfig.asyncFlag == 1) {
		packetHeader |= (g_netSession.groupSeqCounter & 0x7F) << 8;
		++g_netSession.groupSeqCounter;
		packetHeader |= 0x8080;
		if ((int)g_netSession.groupSeqCounter > 127)
			g_netSession.groupSeqCounter = 0;
		encodedPacket.packetTypeHeader = packetHeader;
		encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
		encodedHeaderSize = 2;
		if (appendPending && NetSession_ExitStub(packetType) == 0) {
			encodedPacket.payloadSize = payloadSize - 4;
			encodedPayload = encodedPacket.payload;
			encodedHeaderSize = 4;
		}
		memcpy(encodedPayload, payload + 1, payloadSize - 4);
		encodedPayload += payloadSize - 4;
		encodedSize = payloadSize + encodedHeaderSize - 4;
		if (appendPending) {
			if (g_netSession.groupPendingFlush != 0) {
				*encodedPayload = NET_PACKET_NOP;
				g_netSession.groupPendingFlush = 0;
				++encodedSize;
			} else {
				memcpy(encodedPayload, g_netSession.groupPayload, g_netSession.groupPayloadLength);
				encodedSize += g_netSession.groupPayloadLength;
			}
		}
		g_netSession.groupPayload[0] = packetTypeByte[0];
		memcpy(g_netSession.groupPayload + 1, payload + 1, payloadSize - 4);
		g_netSession.groupPayloadLength = payloadSize - 3;
	} else if (directPlayId == 0) {
		packetHeader |= (g_netSession.broadcastSeqCounter & 0x7F) << 8;
		++g_netSession.broadcastSeqCounter;
		if ((int)g_netSession.broadcastSeqCounter > 127)
			g_netSession.broadcastSeqCounter = 0;
		encodedPacket.packetTypeHeader = packetHeader;
		encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
		encodedHeaderSize = 2;
		if (appendPending && NetSession_ExitStub(packetType) == 0) {
			encodedPacket.payloadSize = payloadSize - 4;
			encodedPayload = encodedPacket.payload;
			encodedHeaderSize = 4;
		}
		memcpy(encodedPayload, payload + 1, payloadSize - 4);
		encodedPayload += payloadSize - 4;
		encodedSize = payloadSize + encodedHeaderSize - 4;
		if (appendPending) {
			if (g_netSession.broadcastPendingFlush != 0) {
				*encodedPayload = NET_PACKET_NOP;
				g_netSession.broadcastPendingFlush = 0;
				++encodedSize;
			} else {
				memcpy(encodedPayload, g_netSession.broadcastPayload, g_netSession.broadcastPayloadLength);
				encodedSize += g_netSession.broadcastPayloadLength;
			}
		}
		g_netSession.broadcastPayload[0] = packetTypeByte[0];
		memcpy(g_netSession.broadcastPayload + 1, payload + 1, payloadSize - 4);
		g_netSession.broadcastPayloadLength = payloadSize - 3;
	} else if (directPlayId == g_netSession.groupDplayId) {
		packetHeader |= (g_netSession.groupSeqCounter & 0x7F) << 8;
		++g_netSession.groupSeqCounter;
		packetHeader |= 0x8080;
		if ((int)g_netSession.groupSeqCounter > 127)
			g_netSession.groupSeqCounter = 0;
		encodedPacket.packetTypeHeader = packetHeader;
		encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
		encodedHeaderSize = 2;
		if (appendPending && NetSession_ExitStub(packetType) == 0) {
			encodedPacket.payloadSize = payloadSize - 4;
			encodedPayload = encodedPacket.payload;
			encodedHeaderSize = 4;
		}
		memcpy(encodedPayload, payload + 1, payloadSize - 4);
		encodedPayload += payloadSize - 4;
		encodedSize = payloadSize + encodedHeaderSize - 4;
		if (appendPending) {
			if (g_netSession.groupPendingFlush != 0) {
				*encodedPayload = NET_PACKET_NOP;
				g_netSession.groupPendingFlush = 0;
				++encodedSize;
			} else {
				memcpy(encodedPayload, g_netSession.groupPayload, g_netSession.groupPayloadLength);
				encodedSize += g_netSession.groupPayloadLength;
			}
		}
		g_netSession.groupPayload[0] = packetTypeByte[0];
		memcpy(g_netSession.groupPayload + 1, payload + 1, payloadSize - 4);
		g_netSession.groupPayloadLength = payloadSize - 3;
	} else {
		unsigned int sequenceIndex = NetReliable_FindOrCreatePeerSlot(directPlayId);
		if (g_netSession.reliablePeerSlotCount > sequenceIndex && sequenceIndex < 40) {
			int sendSequence;
			sendSequence = g_netSession.reliablePeerSlots[sequenceIndex].sendSeq;
			packetHeader |= (sendSequence++ & 0x7F) << 8;
			g_netSession.reliablePeerSlots[sequenceIndex].sendSeq = sendSequence;
			if (sendSequence > 127)
				g_netSession.reliablePeerSlots[sequenceIndex].sendSeq = 0;
		}
		packetHeader |= 0x8000;
		encodedPacket.packetTypeHeader = packetHeader;
		encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
		encodedHeaderSize = 2;
		if (appendPending && NetSession_ExitStub(packetType) == 0) {
			encodedPacket.payloadSize = payloadSize - 4;
			encodedPayload = encodedPacket.payload;
			encodedHeaderSize = 4;
		}
		memcpy(encodedPayload, payload + 1, payloadSize - 4);
		encodedPayload += payloadSize - 4;
		encodedSize = payloadSize + encodedHeaderSize - 4;
		if (appendPending) {
			memcpy(encodedPayload, &g_netSession.reliablePeerSlots[sequenceIndex].lastPiggybackType,
				   g_netSession.reliablePeerSlots[sequenceIndex].piggybackLength);
			encodedSize += g_netSession.reliablePeerSlots[sequenceIndex].piggybackLength;
		}
		g_netSession.reliablePeerSlots[sequenceIndex].lastPiggybackType = packetTypeByte[0];
		memcpy(g_netSession.reliablePeerSlots[sequenceIndex].piggybackPayload, payload + 1, payloadSize - 4);
		g_netSession.reliablePeerSlots[sequenceIndex].piggybackLength = payloadSize - 3;
	}

	if ((packetType != NET_PACKET_REMOTE_INPUT || g_gameConfig.asyncFlag != 1) &&
		g_netSession.localPlayerInfo.directPlayId != directPlayId) {
		if (packetType == NET_PACKET_WORLD_MESSAGE) {
			NetQueuedPacket* queuedPacket;
			memcpy(g_netSessionExportRecvQueue[g_netSessionRecvQueueHighWater].payload, payload, payloadSize);
			queuedPacket = &g_netSessionExportRecvQueue[g_netSessionRecvQueueHighWater];
			queuedPacket->directPlayId = directPlayId;
			queuedPacket->payloadSize = payloadSize;
			queuedPacket->aux = 0;
			queuedPacket->meta0 = 0;
			queuedPacket->packetClass = 0;
			queuedPacket->sequenceByte = (encodedPacket.packetTypeHeader & 0x7F00) >> 8;
			++g_netSessionRecvQueueHighWater;
			if (g_netSessionRecvQueueHighWater >= 256)
				g_netSessionRecvQueueHighWater = 0;
		}

		{
			int historyIndex;
			memcpy(g_netSessionRecvHistory[g_netSessionRecvHistoryCount].payload, payload, payloadSize);
			historyIndex = g_netSessionRecvHistoryCount;
			g_netSessionRecvHistory[historyIndex].directPlayId = directPlayId;
			g_netSessionRecvHistory[historyIndex].payloadSize = payloadSize;
			g_netSessionRecvHistory[historyIndex].aux = 0;
			g_netSessionRecvHistory[historyIndex].meta0 = 0;
			if (directPlayId == 0)
				g_netSessionRecvHistory[historyIndex].packetClass = 0;
			else if (directPlayId == g_netSession.groupDplayId)
				g_netSessionRecvHistory[historyIndex].packetClass = 2;
			else
				g_netSessionRecvHistory[historyIndex].packetClass = 1;
			++g_netSessionRecvHistoryCount;
			g_netSessionRecvHistory[historyIndex].sequenceByte =
				(encodedPacket.packetTypeHeader & 0x7F00) >> 8;
		}
		if (g_netSessionRecvHistoryCount >= 128)
			g_netSessionRecvHistoryCount = 0;
	}

	if (g_netSession.localPlayerInfo.directPlayId == directPlayId || directPlayId == 0 ||
		g_netSession.dplayInterface == NULL || directPlayId == g_netSession.groupDplayId) {
		if ((int)g_netRecvQueueCount >= 1024)
			NetReliable_CompactLocalReceiveQueue();
		if ((int)g_netRecvQueueCount < 1024) {
			unsigned int queueIndex;
			unsigned int sequenceIndex;
			int peerSlotAvailable;
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, payload, payloadSize);
			queueIndex = (unsigned int)g_netRecvQueueWriteIndex;
			g_netSessionRecvQueue[queueIndex].directPlayId = (DPID)g_netSession.localPlayerInfo.directPlayId;
			g_netSessionRecvQueue[queueIndex].payloadSize = payloadSize;
			g_netSessionRecvQueue[queueIndex].aux = 0;
			g_netSessionRecvQueue[queueIndex].meta0 = 0;
			g_netSessionRecvQueue[queueIndex].queuedFlag = 0;
			sequenceIndex = NetReliable_FindOrCreatePeerSlot(g_netSession.localPlayerInfo.directPlayId);
			if (packetType == NET_PACKET_REMOTE_INPUT && g_gameConfig.asyncFlag == 1) {
				peerSlotAvailable = sequenceIndex < g_netSession.reliablePeerSlotCount;
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 2;
#ifdef XVT_MODERN
				if (peerSlotAvailable && sequenceIndex < 40) {
#else
				if (peerSlotAvailable < 40) {
#endif
					packetHeader &= 0x7F00;
					packetHeader >>= 8;
					g_netSession.reliablePeerSlots[sequenceIndex].recvSeqChannelB = packetHeader;
				}
			} else if (directPlayId == 0) {
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 0;
				if (sequenceIndex < g_netSession.reliablePeerSlotCount && sequenceIndex < 40) {
					packetHeader &= 0x7F00;
					packetHeader >>= 8;
					g_netSession.reliablePeerSlots[sequenceIndex].recvSeqChannelA = packetHeader;
				}
			} else if (directPlayId == g_netSession.groupDplayId) {
				peerSlotAvailable = sequenceIndex < g_netSession.reliablePeerSlotCount;
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 2;
#ifdef XVT_MODERN
				if (peerSlotAvailable && sequenceIndex < 40) {
#else
				if (peerSlotAvailable < 40) {
#endif
					packetHeader &= 0x7F00;
					packetHeader >>= 8;
					g_netSession.reliablePeerSlots[sequenceIndex].recvSeqChannelB = packetHeader;
				}
			} else {
				peerSlotAvailable = sequenceIndex < g_netSession.reliablePeerSlotCount;
				g_netSessionRecvQueue[g_netRecvQueueWriteIndex].packetClass = 1;
#ifdef XVT_MODERN
				if (peerSlotAvailable && sequenceIndex < 40) {
#else
				if (peerSlotAvailable < 40) {
#endif
					packetHeader &= 0x7F00;
					packetHeader >>= 8;
					g_netSession.reliablePeerSlots[sequenceIndex].recvSeqDefault = packetHeader;
				}
			}
			g_netSessionRecvQueue[g_netRecvQueueWriteIndex].sequenceByte =
				(encodedPacket.packetTypeHeader & 0x7F00) >> 8;
			++g_netRecvQueueCount;
			++g_netRecvQueueWriteIndex;
			if (g_netRecvQueueWriteIndex >= 1024)
				g_netRecvQueueWriteIndex = 0;
		}
	}

	if (g_netSession.dplayInterface == NULL)
		return 1;
	if (g_netSession.localPlayerInfo.directPlayId != directPlayId)
		sendResult = g_netSession.dplayInterface->lpVtbl->Send(
			g_netSession.dplayInterface, (DPID)g_netSession.localPlayerInfo.directPlayId, (DPID)directPlayId,
			0, &encodedPacket.packetTypeHeader, encodedSize);
	return sendResult == 0;
}

// FUNCTION: XVT 0x46DD80
int NetSession_SendSequencedGamePacket(int destDplayId, uint8_t localSeq, uint8_t remoteSeq,
									   const unsigned int* packet, unsigned int packetSize) {
	int appendTerminator;
	HRESULT sendResult;
	NetSessionSequencedEncodedPacket encodedPacket;
	unsigned int packetType;
	uint16_t packetFlags;
	uint8_t* encodedPayload;
	int encodedSize;
	unsigned int packetDataSize;
	unsigned int queueIndex;
	unsigned int queueCount;
	int exitStubResult;

	sendResult = 0;
	if (g_netSession.dplayInterface == NULL) {
		return 1;
	}

	packetType = *packet;
	packetFlags = (uint8_t)packetType & 0x7F;
	appendTerminator = packetType < NET_PACKET_RESYNC_CHECKSUMS || packetType >= NET_PACKET_RESYNC_CHUNK + 1;
	packetFlags |= (uint16_t)(remoteSeq & 0x7F) << 8;
	packetFlags |= 0x80;
	packetFlags &= 0x7FFF;
	encodedPacket.packetTypeHeader = (int16_t)packetFlags;
	encodedPacket.localSequence = localSeq;
	encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
	encodedSize = 3;
	if (appendTerminator) {
		exitStubResult = NetSession_ExitStub((int)packetType);
		packetDataSize = packetSize;
		if (exitStubResult == 0) {
			encodedPayload = encodedPacket.payload;
			encodedPacket.payloadSize = (int16_t)(packetDataSize - 4);
			encodedSize = 5;
		}
	} else {
		packetDataSize = packetSize;
	}

	memcpy(encodedPayload, packet + 1, packetDataSize - 4);
	encodedPayload += packetDataSize - 4;
	encodedSize += (int)packetDataSize - 4;
	if (appendTerminator) {
		*encodedPayload = NET_PACKET_NOP;
		++encodedSize;
	}

	if (g_netSession.localPlayerInfo.directPlayId != destDplayId) {
		sendResult = g_netSession.dplayInterface->lpVtbl->Send(
			g_netSession.dplayInterface, (DPID)g_netSession.localPlayerInfo.directPlayId, (DPID)destDplayId,
			0, &encodedPacket.packetTypeHeader, (uint32_t)encodedSize);
	} else {
		if ((int)g_netRecvQueueCount >= 1024) {
			NetReliable_CompactLocalReceiveQueue();
		}
		if ((int)g_netRecvQueueCount < 1024) {
			memcpy(g_netSessionRecvQueue[g_netRecvQueueWriteIndex].payload, packet, packetDataSize);
			queueIndex = (unsigned int)g_netRecvQueueWriteIndex;
			g_netSessionRecvQueue[queueIndex].directPlayId = (DPID)g_netSession.localPlayerInfo.directPlayId;
			g_netSessionRecvQueue[queueIndex].payloadSize = packetDataSize;
			queueCount = g_netRecvQueueCount;
			g_netSessionRecvQueue[queueIndex].packetClass = localSeq;
			++queueCount;
			g_netSessionRecvQueue[queueIndex].sequenceByte = remoteSeq;
			g_netRecvQueueCount = queueCount;
			g_netSessionRecvQueue[queueIndex].queuedFlag = 1;
			g_netSessionRecvQueue[queueIndex].aux = 0;
			g_netSessionRecvQueue[queueIndex].meta0 = 0;
			++g_netRecvQueueWriteIndex;
			if (g_netRecvQueueWriteIndex >= 1024) {
				g_netRecvQueueWriteIndex = 0;
			}
		}
	}

	return sendResult == 0;
}

// FUNCTION: XVT 0x46DFA0
SessionPlayerInfo* NetSession_GetPlayerRoster(int* outCount) {
	*outCount = g_netSession.playerCount;
	return g_netSession.players;
}

// FUNCTION: XVT 0x46DFC0
SessionPlayerInfo* NetSession_GetLocalPlayerInfo(void) { return &g_netSession.localPlayerInfo; }

// FUNCTION: XVT 0x46DFD0
int NetSession_SetPlayerRoster(const SessionPlayerInfo* players, int playerCount) {
	memcpy(g_netSession.players, players, (size_t)playerCount * sizeof(*players));
	g_netSession.playerCount = playerCount;
	return 1;
}

// FUNCTION: XVT 0x46E000
int NetSession_GetPlayerCount(void) {
	if (g_netSession.playerCount == 0) {
		return 1;
	}
	return g_netSession.playerCount;
}

// FUNCTION: XVT 0x46E040
int NetSession_GetLocalPlayerId(void) { return g_netSession.localPlayerId; }

// FUNCTION: XVT 0x46E050
int* NetSession_ReceiveGamePacket(int* outSenderDpid, int* outPayloadSize) {
	int* packet;

	for (;;) {
		packet = (int*)NetSession_ReceivePacket(outSenderDpid, outPayloadSize);
		if (packet == NULL || *outSenderDpid != 0)
			return packet;
		NetSession_HandleHandshakePacket(*packet, packet);
	}
}

// FUNCTION: XVT 0x46E080
int NetSession_HandleHandshakePacket(int packetOpcode, int* packet) {
	enum {
		RELIABLE_SEQUENCE_SENTINEL = 127,
		PEER_SNAPSHOT_METADATA_DWORD_COUNT = 3,
		PEER_SNAPSHOT_STRIDE = 8,
		PEER_PREV_CHANNEL_A_OFFSET = 4,
		PEER_PREV_CHANNEL_B_OFFSET = 5,
		PEER_CHANNEL_A_OFFSET = 6,
		PEER_CHANNEL_B_OFFSET = 7,
		PEER_SLOT_QWORD_COUNT = sizeof(NetReliablePeerSlot) / sizeof(uint64_t)
	};

	int result = packetOpcode;
	int playerIndex;
	int peerIndex;
	uint8_t handshakeActive;
	NetReliablePeerSlot* peer;
	uint8_t* encodedPeer;

	switch (packetOpcode) {
		case DPSYS_CREATEPLAYERORGROUP:
			result = NetSession_GetLocalPlayerId();
			if (result == 0)
				return result;
			handshakeActive = g_netSessionFlightHandshakeActive;
			result = packet[1];
			if (handshakeActive != 0) {
				if (result == 1 && packet[2] != g_netSession.localPlayerInfo.directPlayId) {
					g_netSessionScratchPacket.payloadDwords[0] = 0;
					g_netSessionScratchPacket.packetType = NET_PACKET_SEQUENCE_STATUS;
					g_netSessionScratchPacket.payloadDwords[1] = (int)g_netSession.reliablePeerSlotCount;
					g_netSessionScratchPacket.payloadDwords[2] = (int)timeGetTime();
					encodedPeer = (uint8_t*)&g_netSessionScratchPacket
									  .payloadDwords[PEER_SNAPSHOT_METADATA_DWORD_COUNT];
					for (peerIndex = 0; peerIndex < (int)g_netSession.reliablePeerSlotCount; ++peerIndex) {
						peer = &g_netSession.reliablePeerSlots[peerIndex];
						memcpy(&encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE], &peer->directPlayId,
							   sizeof(peer->directPlayId));
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_PREV_CHANNEL_A_OFFSET] =
							(uint8_t)peer->prevRecvSeqChannelA;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_PREV_CHANNEL_B_OFFSET] =
							(uint8_t)peer->prevRecvSeqChannelB;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_CHANNEL_A_OFFSET] =
							(uint8_t)peer->recvSeqChannelA;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_CHANNEL_B_OFFSET] =
							(uint8_t)peer->recvSeqChannelB;
					}
					NetSession_SendPacket(packet[2], (unsigned int*)&g_netSessionScratchPacket,
										  8 * (int)g_netSession.reliablePeerSlotCount + 16);
					g_netSessionScratchPacket.packetType = NET_PACKET_FLIGHT_SESSION_STATUS;
					g_netSessionScratchPacket.payloadDwords[0] = Mission_GetElapsedClockSeconds();
					g_netSessionScratchPacket.payloadDwords[1] =
#ifdef XVT_MODERN
						FRONTEND_NET_PROTOCOL_VERSION;
#else
						101;
#endif
					g_netSessionScratchPacket.payloadDwords[2] = 0;
					return NetSession_SendPacket(packet[2], (unsigned int*)&g_netSessionScratchPacket, 16);
				}
			} else if (result == 1) {
				if (packet[2] != g_netSession.localPlayerInfo.directPlayId) {
					g_netSessionScratchPacket.payloadDwords[0] = 0;
					g_netSessionScratchPacket.packetType = NET_PACKET_SEQUENCE_STATUS;
					g_netSessionScratchPacket.payloadDwords[1] = (int)g_netSession.reliablePeerSlotCount;
					g_netSessionScratchPacket.payloadDwords[2] = (int)timeGetTime();
					encodedPeer = (uint8_t*)&g_netSessionScratchPacket
									  .payloadDwords[PEER_SNAPSHOT_METADATA_DWORD_COUNT];
					for (peerIndex = 0; peerIndex < (int)g_netSession.reliablePeerSlotCount; ++peerIndex) {
						peer = &g_netSession.reliablePeerSlots[peerIndex];
						memcpy(&encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE], &peer->directPlayId,
							   sizeof(peer->directPlayId));
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_PREV_CHANNEL_A_OFFSET] =
							(uint8_t)peer->prevRecvSeqChannelA;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_PREV_CHANNEL_B_OFFSET] =
							(uint8_t)peer->prevRecvSeqChannelB;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_CHANNEL_A_OFFSET] =
							(uint8_t)peer->recvSeqChannelA;
						encodedPeer[peerIndex * PEER_SNAPSHOT_STRIDE + PEER_CHANNEL_B_OFFSET] =
							(uint8_t)peer->recvSeqChannelB;
					}
					NetSession_SendPacket(packet[2], (unsigned int*)&g_netSessionScratchPacket,
										  8 * (int)g_netSession.reliablePeerSlotCount + 16);
					g_netSessionScratchPacket.packetType = NET_PACKET_FLIGHT_SESSION_STATUS;
					g_netSessionScratchPacket.payloadDwords[0] = 0;
					NetSession_SendPacket(packet[2], (unsigned int*)&g_netSessionScratchPacket, 8);
				}
				g_netSession.playerCount = 0;
				NetSession_EnumeratePlayers();
				for (playerIndex = 0; playerIndex < g_netSession.playerCount; ++playerIndex) {
					if (g_netSession.players[playerIndex].directPlayId !=
						g_netSession.localPlayerInfo.directPlayId)
						NetSession_AddPlayerToGroup(&g_netSession.players[playerIndex]);
				}
			}
			return result;

		case DPSYS_DESTROYPLAYERORGROUP:
#ifdef XVT_MODERN
			Aeron_LogInfo("xvt.net", "DirectPlay %s %d destroyed%s",
						  packet[1] == DPPLAYERTYPE_PLAYER ? "player" : "group", packet[2],
						  packet[2] == g_netSession.hostDplayId ? " (host)" : "");
			if (packet[1] == DPPLAYERTYPE_PLAYER && packet[2] == g_netSession.hostDplayId)
				XvtNetworkSession_HostLost();
#endif
			result = NetSession_GetLocalPlayerId();
			if (result == 0)
				return result;
			if (g_netSessionFlightHandshakeActive != 0) {
				result = packet[1];
				if (result == 1) {
					result = g_netSession.playerCount;
					for (playerIndex = 0; playerIndex < g_netSession.playerCount; ++playerIndex) {
						if (g_netSession.players[playerIndex].directPlayId == packet[2]) {
							if (g_netSession.players[playerIndex].activeFlag != 0) {
								g_netSessionScratchPacket.packetType = NET_PACKET_STARTUP_READY;
								NetSession_SendPacket(g_netSession.localPlayerInfo.directPlayId,
													  (unsigned int*)&g_netSessionScratchPacket, 4);
							}
							result = NetSession_RemovePlayerFromGroup(packet[2]);
							g_netSession.players[playerIndex].activeFlag = 0;
							break;
						}
					}
					playerIndex = 0;
					if ((int)g_netSession.reliablePeerSlotCount <= playerIndex)
						return result;
					result = packet[2];
					do {
						if (g_netSession.reliablePeerSlots[playerIndex].directPlayId == (DPID)packet[2]) {
							--g_netSession.reliablePeerSlotCount;
							memcpy(&g_netSession.reliablePeerSlots[playerIndex],
								   &g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount],
								   sizeof(g_netSession.reliablePeerSlots[playerIndex]));
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].directPlayId =
								0;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.prevRecvSeqDefault = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.prevRecvSeqChannelA = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.prevRecvSeqChannelB = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.recvSeqDefault = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.recvSeqChannelA = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.recvSeqChannelB = RELIABLE_SEQUENCE_SENTINEL;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].sendSeq = 0;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.lastPiggybackType = NET_PACKET_NOP;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.piggybackLength = 1;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.lastActivityMs = 0;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].packetCount =
								0;
							g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount]
								.packetDropCount = 0;
							return PEER_SLOT_QWORD_COUNT * (int)g_netSession.reliablePeerSlotCount;
						}
						++playerIndex;
					} while (playerIndex < (int)g_netSession.reliablePeerSlotCount);
					return result;
				}
				return result;
			}
			result = packet[1];
			if (result != 1)
				return result;
			g_netSession.playerCount = 0;
			NetSession_EnumeratePlayers();
			result = NetSession_RemovePlayerFromGroup(packet[2]);
			playerIndex = 0;
			if ((int)g_netSession.reliablePeerSlotCount <= playerIndex)
				return result;
			result = packet[2];
			do {
				if (g_netSession.reliablePeerSlots[playerIndex].directPlayId == (DPID)packet[2]) {
					--g_netSession.reliablePeerSlotCount;
					memcpy(&g_netSession.reliablePeerSlots[playerIndex],
						   &g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount],
						   sizeof(g_netSession.reliablePeerSlots[playerIndex]));
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].directPlayId = 0;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].prevRecvSeqDefault =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].prevRecvSeqChannelA =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].prevRecvSeqChannelB =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].recvSeqDefault =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].recvSeqChannelA =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].recvSeqChannelB =
						RELIABLE_SEQUENCE_SENTINEL;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].sendSeq = 0;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].lastPiggybackType =
						NET_PACKET_NOP;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].piggybackLength = 1;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].lastActivityMs = 0;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].packetCount = 0;
					g_netSession.reliablePeerSlots[g_netSession.reliablePeerSlotCount].packetDropCount = 0;
					return PEER_SLOT_QWORD_COUNT * (int)g_netSession.reliablePeerSlotCount;
				}
				++playerIndex;
			} while (playerIndex < (int)g_netSession.reliablePeerSlotCount);
			return result;

		case DPSYS_SETPLAYERORGROUPNAME:
			result = ((const NetPlayerNameMessage*)packet)->header.dwPlayerType;
			if (result == 1) {
				result = g_netSession.playerCount;
				for (playerIndex = 0; playerIndex < g_netSession.playerCount; ++playerIndex) {
					if (g_netSession.players[playerIndex].directPlayId ==
						(int)((const NetPlayerNameMessage*)packet)->header.dpId) {
#ifdef XVT_MODERN
						if (!XvtNetworkSession_CopyPlayerNames(
								(const NetPlayerNameMessage*)packet,
								g_netSession.players[playerIndex].playerName,
								sizeof(g_netSession.players[playerIndex].playerName),
								g_netSession.players[playerIndex].sessionName,
								sizeof(g_netSession.players[playerIndex].sessionName)))
							return 0;
#else
						strcpy(g_netSession.players[playerIndex].playerName,
							   ((const NetPlayerNameMessage*)packet)->names);
						strcpy(g_netSession.players[playerIndex].sessionName,
							   &((const NetPlayerNameMessage*)packet)
									->names[strlen(g_netSession.players[playerIndex].playerName) + 1]);
#endif
						g_netSession.players[playerIndex].playerName[12] = '\0';
						g_netSession.players[playerIndex].sessionName[12] = '\0';
						return 0;
					}
				}
			}
			return result;

		default:
			return result;
	}
}

// FUNCTION: XVT 0x46E780
void* NetSession_ReceivePacket(int* outSenderDpid, int* outPayloadSize) {
	int sequence;
	int expectedSequence;
	int queueIndex;

	struct {
		int wantChannelA;
		int wantChannelB;
		unsigned int remaining;
	} channels;

	int nextSequence;
	int sequenceDistance;
	int queuedIndex;
	int payloadType;
	int resendDelay;
	uint8_t* payload;
	int sentRetry;
	int unusedSearchIndex;
	int remoteSequence;
	uint8_t inspected[40];
	uint8_t retryCounts[40];
	uint8_t lastSequences[40][3];
	uint8_t inspectionLimits[40];
	NetSessionScratchPacket retryPacket;
	NetQueuedPacket* packet;
	unsigned int oldPeerCount;
	unsigned int peerIndex;
	int historySourceIndex;
	int historyPeerIndex;
	int delta;
	uint32_t now;
	unsigned int timeout;
	unsigned int retryLimit;
	int searchSequence;
	int directPlayId;

	extern int dtMs;

	NetSession_PumpIncomingPackets();
	NetSession_SendReliableKeepalives();
	if (g_netRecvQueueCount == 0)
		return NULL;

	memset(retryCounts, 0, sizeof(retryCounts));
	memset(inspected, 0, sizeof(inspected));
	memset(inspectionLimits, 90, sizeof(inspectionLimits));
	memset(lastSequences, 0, sizeof(lastSequences));
	historyPeerIndex = g_netSession.reliablePeerSlotCount;
	if ((int)g_netSession.reliablePeerSlotCount > 0) {
		historySourceIndex = 0;
		do {
			lastSequences[historySourceIndex][0] =
				(uint8_t)g_netSession.reliablePeerSlots[historySourceIndex].prevRecvSeqChannelA;
			lastSequences[historySourceIndex][1] =
				(uint8_t)g_netSession.reliablePeerSlots[historySourceIndex].prevRecvSeqDefault;
			lastSequences[historySourceIndex][2] =
				(uint8_t)g_netSession.reliablePeerSlots[historySourceIndex].prevRecvSeqChannelB;
			++historySourceIndex;
			--historyPeerIndex;
		} while (historyPeerIndex != 0);
	}

	queueIndex = g_netRecvQueueReadIndex;
	channels.remaining = g_netRecvQueueCount;
	while ((int)channels.remaining > 0) {
		directPlayId = g_netSessionRecvQueue[queueIndex].directPlayId;
		packet = &g_netSessionRecvQueue[queueIndex];
		if (directPlayId == 0) {
			if (queueIndex == g_netRecvQueueReadIndex) {
				memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[queueIndex],
					   sizeof(g_netSession.recvScratchPacket));
				*outSenderDpid = g_netSessionRecvQueue[queueIndex].directPlayId;
				*outPayloadSize = g_netSessionRecvQueue[queueIndex].payloadSize;
				--g_netRecvQueueCount;
				++g_netRecvQueueReadIndex;
				if (g_netRecvQueueReadIndex >= 1024)
					g_netRecvQueueReadIndex = 0;
				return g_netSession.recvScratchPacket.payload;
			}
			++queueIndex;
			if (queueIndex >= 1024)
				queueIndex = 0;
			--channels.remaining;
			continue;
		}
		sequence = g_netSessionRecvQueue[queueIndex].sequenceByte;
		oldPeerCount = g_netSession.reliablePeerSlotCount;
		channels.wantChannelA = g_netSessionRecvQueue[queueIndex].packetClass == 0;
		channels.wantChannelB = g_netSessionRecvQueue[queueIndex].packetClass == 2;
		peerIndex = NetReliable_FindOrCreatePeerSlot(directPlayId);
		if (oldPeerCount != g_netSession.reliablePeerSlotCount && peerIndex < 40) {
			lastSequences[peerIndex][0] = 127;
			lastSequences[peerIndex][1] = 127;
			lastSequences[peerIndex][2] = 127;
		}
		if (peerIndex < g_netSession.reliablePeerSlotCount) {
			if (inspectionLimits[peerIndex] < inspected[peerIndex]) {
				++queueIndex;
				if (queueIndex >= 1024)
					queueIndex = 0;
				--channels.remaining;
				continue;
			}
			if (g_netSessionRecvQueue[queueIndex].queuedFlag == 0)
				++inspected[peerIndex];

			if (channels.wantChannelA) {
				expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA + 1;
				if (expectedSequence > 127)
					expectedSequence = 0;
				nextSequence = (unsigned int)lastSequences[peerIndex][0] + 1;
				if (nextSequence > 127)
					nextSequence = 0;
			} else if (channels.wantChannelB) {
				expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB + 1;
				if (expectedSequence > 127)
					expectedSequence = 0;
				nextSequence = (unsigned int)lastSequences[peerIndex][2] + 1;
				if (nextSequence > 127)
					nextSequence = 0;
			} else {
				expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault + 1;
				if (expectedSequence > 127)
					expectedSequence = 0;
				nextSequence = (unsigned int)lastSequences[peerIndex][1] + 1;
				if (nextSequence > 127)
					nextSequence = 0;
			}
		}
#ifdef XVT_MODERN
		else {
			expectedSequence = 0;
			nextSequence = 0;
		}
#endif

		payload = g_netSessionRecvQueue[queueIndex].payload;
#ifdef XVT_MODERN
		memcpy(&payloadType, payload, sizeof(payloadType));
#else
		payloadType = *(int*)payload;
#endif
		delta = sequence - expectedSequence;
		if (peerIndex >= g_netSession.reliablePeerSlotCount ||
			(delta >= -28 && (delta < 0 || delta >= 100))) {
			if (NetReliable_RemoveQueuedPacket(queueIndex) != 0) {
				++queueIndex;
				if (queueIndex >= 1024)
					queueIndex = 0;
			}
			--channels.remaining;
			continue;
		}

		if (expectedSequence == sequence && nextSequence == expectedSequence) {
			g_netSession.reliablePeerSlots[peerIndex].lastActivityMs = timeGetTime();
			if (channels.wantChannelA)
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA = sequence;
			else if (channels.wantChannelB)
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB = sequence;
			else
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault = sequence;
			g_netLastDeliveredRecvSequence = sequence;
			if (payloadType != NET_PACKET_REMOTE_INPUT || g_gameConfig.asyncFlag != 1)
				++g_netSession.reliablePeerSlots[peerIndex].packetCount;
			memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[queueIndex],
				   sizeof(g_netSession.recvScratchPacket));
			NetReliable_RemoveQueuedPacket(queueIndex);
			*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
			*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
			return g_netSession.recvScratchPacket.payload;
		}

		if (payloadType == NET_PACKET_REMOTE_INPUT && g_gameConfig.asyncFlag == 1) {
			g_netSession.reliablePeerSlots[peerIndex].lastActivityMs = timeGetTime();
			if (channels.wantChannelA)
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA = sequence;
			else if (channels.wantChannelB)
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB = sequence;
			else
				g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault = sequence;
			g_netLastDeliveredRecvSequence = sequence;
			memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[queueIndex],
				   sizeof(g_netSession.recvScratchPacket));
			NetReliable_RemoveQueuedPacket(queueIndex);
			*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
			*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
			return g_netSession.recvScratchPacket.payload;
		}

		if (g_netSessionRecvQueue[queueIndex].queuedFlag == 0) {
			int selectedChannel = channels.wantChannelA ? 0 : (channels.wantChannelB ? 2 : 1);
			remoteSequence = (unsigned int)lastSequences[peerIndex][selectedChannel] + 1;
			lastSequences[peerIndex][selectedChannel] = (uint8_t)sequence;
			if (remoteSequence > 127)
				remoteSequence = 0;
			sequenceDistance = sequence - remoteSequence;
			if (sequenceDistance < 0)
				sequenceDistance += 128;
			searchSequence = remoteSequence;
			resendDelay = sequenceDistance;
			resendDelay *= dtMs;
			sentRetry = 0;
			if (inspected[peerIndex] < 127)
				inspected[peerIndex] = (uint8_t)(inspected[peerIndex] + sequenceDistance);
			if (retryCounts[peerIndex] > 25) {
				++queueIndex;
				if (queueIndex >= 1024)
					queueIndex = 0;
				--channels.remaining;
				continue;
			}

			while (sequence != remoteSequence) {
				unusedSearchIndex = queueIndex;
				queuedIndex =
					NetReliable_FindQueuedRecvPacket(unusedSearchIndex, remoteSequence, channels.wantChannelA,
													 channels.wantChannelB, (int)peerIndex);
				if (queuedIndex < 1024 && queuedIndex >= 0) {
					if (expectedSequence == remoteSequence) {
						g_netSession.reliablePeerSlots[peerIndex].lastActivityMs = timeGetTime();
						memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[queuedIndex],
							   sizeof(g_netSession.recvScratchPacket));
						NetReliable_RemoveQueuedPacket(queuedIndex);
						if (channels.wantChannelA)
							g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA = expectedSequence;
						else if (channels.wantChannelB)
							g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB = expectedSequence;
						else
							g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault = expectedSequence;
						g_netLastDeliveredRecvSequence = expectedSequence;
						++g_netSession.reliablePeerSlots[peerIndex].packetCount;
						*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
						*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
						if (sequenceDistance <= 1) {
							g_netSessionRecvQueue[queueIndex].meta0 = 0;
							g_netSessionRecvQueue[queueIndex].aux = 0;
						}
						return g_netSession.recvScratchPacket.payload;
					}
					--sequenceDistance;
				} else {
					++retryCounts[peerIndex];
					if (g_netSessionRecvQueue[queueIndex].meta0 == 0) {
						int retryChannel = channels.wantChannelA ? 0 : (channels.wantChannelB ? 2 : 1);
						if (payloadType == NET_PACKET_WORLD_MESSAGE
#ifdef XVT_MODERN
							&& channels.wantChannelA
#endif
						) {
							retryPacket.packetType = NET_PACKET_WORLD_NACK;
#ifdef XVT_MODERN
							memcpy(&retryPacket.payloadDwords[0], payload + 4,
								   sizeof(retryPacket.payloadDwords[0]));
							retryPacket.payloadDwords[0] =
								(retryPacket.payloadDwords[0] & 0x7fffffff) - resendDelay;
#else
							retryPacket.payloadDwords[0] = (*(int*)(payload + 4) & 0x7fffffff) - resendDelay;
#endif
							retryPacket.payloadDwords[1] = remoteSequence;
							NetSession_SendCompactGamePacket(packet->directPlayId,
															 (unsigned int*)&retryPacket, 12, 1);
						} else {
							int retryDirectPlayId = packet->directPlayId;
							retryPacket.payloadDwords[0] = remoteSequence;
							retryPacket.packetType = NET_PACKET_NACK;
							retryPacket.payloadDwords[1] = retryChannel;
							NetSession_SendCompactGamePacket(retryDirectPlayId, (unsigned int*)&retryPacket,
															 12, 1);
						}
						++g_netSession.reliablePeerSlots[peerIndex].packetRetryCount;
						sentRetry = 1;
					} else {
						int timeoutPayloadType;
						now = timeGetTime();
						timeoutPayloadType = payloadType;
						if (g_netSession.reliableUseFixedResendTimeouts != 0) {
							retryLimit = 0;
							timeout = timeoutPayloadType == 2 ? 40000 : 3000;
						} else {
							retryLimit = timeoutPayloadType == 2 ? 5 : 3;
							timeout = 1000 << g_netSessionRecvQueue[queueIndex].meta0;
						}
						if (now - (uint32_t)g_netSessionRecvQueue[queueIndex].aux > timeout) {
							NetSession_DebugTrace("(RTO) ");
							if (g_netSessionRecvQueue[queueIndex].meta0 > retryLimit) {
								NetSession_DebugTrace("(TMR) ");
								g_netSession.reliablePeerSlots[peerIndex].lastActivityMs = timeGetTime();
								remoteSequence = searchSequence;
								for (;;) {
									queuedIndex = NetReliable_FindQueuedRecvPacket(
										unusedSearchIndex, remoteSequence, channels.wantChannelA,
										channels.wantChannelB, (int)peerIndex);
									if (queuedIndex >= 0 && queuedIndex <= 1024)
										break;
									if (sequence == remoteSequence) {
										if (channels.wantChannelA)
											g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA =
												sequence;
										else if (channels.wantChannelB)
											g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB =
												sequence;
										else
											g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault =
												sequence;
										g_netLastDeliveredRecvSequence = sequence;
										++g_netSession.reliablePeerSlots[peerIndex].packetCount;
										memcpy(&g_netSession.recvScratchPacket,
											   &g_netSessionRecvQueue[queueIndex],
											   sizeof(g_netSession.recvScratchPacket));
										NetReliable_RemoveQueuedPacket((unsigned int)unusedSearchIndex);
										*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
										*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
										return g_netSession.recvScratchPacket.payload;
									}
									++remoteSequence;
									if (remoteSequence > 127)
										remoteSequence = 0;
								}
								memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[queuedIndex],
									   sizeof(g_netSession.recvScratchPacket));
								NetReliable_RemoveQueuedPacket(queuedIndex);
								if (channels.wantChannelA)
									g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA =
										remoteSequence;
								else if (channels.wantChannelB)
									g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB =
										remoteSequence;
								else
									g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault =
										remoteSequence;
								g_netLastDeliveredRecvSequence = remoteSequence;
								++g_netSession.reliablePeerSlots[peerIndex].packetCount;
								*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
								*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
								return g_netSession.recvScratchPacket.payload;
							}
							{
								int retryChannel =
									channels.wantChannelA ? 0 : (channels.wantChannelB ? 2 : 1);
								if (payloadType == NET_PACKET_WORLD_MESSAGE
#ifdef XVT_MODERN
									&& channels.wantChannelA
#endif
								) {
									retryPacket.packetType = NET_PACKET_WORLD_NACK;
#ifdef XVT_MODERN
									memcpy(&retryPacket.payloadDwords[0], payload + 4,
										   sizeof(retryPacket.payloadDwords[0]));
									retryPacket.payloadDwords[0] =
										(retryPacket.payloadDwords[0] & 0x7fffffff) - resendDelay;
#else
									retryPacket.payloadDwords[0] =
										(*(int*)(payload + 4) & 0x7fffffff) - resendDelay;
#endif
									retryPacket.payloadDwords[1] = remoteSequence;
									NetSession_SendCompactGamePacket(packet->directPlayId,
																	 (unsigned int*)&retryPacket, 12, 1);
								} else {
									int retryDirectPlayId = packet->directPlayId;
									retryPacket.payloadDwords[0] = remoteSequence;
									retryPacket.packetType = NET_PACKET_NACK;
									retryPacket.payloadDwords[1] = retryChannel;
									NetSession_SendCompactGamePacket(retryDirectPlayId,
																	 (unsigned int*)&retryPacket, 12, 1);
								}
								sentRetry = 1;
							}
						}
					}
				}
				resendDelay -= dtMs;
				++remoteSequence;
				if (remoteSequence > 127)
					remoteSequence = 0;
			}

			if (sequenceDistance <= 0) {
				g_netSessionRecvQueue[queueIndex].meta0 = 0;
				g_netSessionRecvQueue[queueIndex].aux = 0;
			} else if (sentRetry == 1) {
				++g_netSessionRecvQueue[queueIndex].meta0;
				g_netSessionRecvQueue[queueIndex].aux = timeGetTime();
			}
		} else {
			if (g_netRecvQueueReadIndex == queueIndex) {
				if (NetReliable_RemoveQueuedPacket(queueIndex) == 0) {
					--channels.remaining;
					continue;
				}
			}
			++queueIndex;
			if (queueIndex >= 1024)
				queueIndex = 0;
			--channels.remaining;
			continue;
		}
		++queueIndex;
		if (queueIndex >= 1024)
			queueIndex = 0;
		--channels.remaining;
	}

	if ((int)g_netRecvQueueCount < 1023)
		return NULL;
	{
		unsigned int fullQueueIndex = g_netRecvQueueReadIndex;
		int fullRemaining = g_netRecvQueueCount;
		while (fullRemaining > 0) {
			if (g_netSessionRecvQueue[fullQueueIndex].directPlayId != 0) {
				sequence = g_netSessionRecvQueue[fullQueueIndex].sequenceByte;
				channels.wantChannelA = g_netSessionRecvQueue[fullQueueIndex].packetClass == 0;
				channels.wantChannelB = g_netSessionRecvQueue[fullQueueIndex].packetClass == 2;
				peerIndex =
					NetReliable_FindOrCreatePeerSlot(g_netSessionRecvQueue[fullQueueIndex].directPlayId);
				if (peerIndex < g_netSession.reliablePeerSlotCount && peerIndex < 40) {
					if (channels.wantChannelA) {
						expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA + 1;
						if (expectedSequence > 127)
							expectedSequence = 0;
					} else if (channels.wantChannelB) {
						expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB + 1;
						if (expectedSequence > 127)
							expectedSequence = 0;
					} else {
						expectedSequence = g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault + 1;
						if (expectedSequence > 127)
							expectedSequence = 0;
					}
				}
#ifdef XVT_MODERN
				else {
					expectedSequence = 0;
				}
#endif
				delta = sequence - expectedSequence;
				if (peerIndex >= g_netSession.reliablePeerSlotCount ||
					(delta >= -27 && (delta < 0 || delta >= 100))) {
					if (NetReliable_RemoveQueuedPacket(fullQueueIndex) == 0) {
						--fullRemaining;
						continue;
					}
				} else if (g_netSessionRecvQueue[fullQueueIndex].meta0 == 0) {
					++fullQueueIndex;
					if ((int)fullQueueIndex >= 1024)
						fullQueueIndex = 0;
					--fullRemaining;
					continue;
				} else {
					NetSession_DebugTrace("(NMR) ");
					if (channels.wantChannelA)
						g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelA = sequence;
					else if (channels.wantChannelB)
						g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqChannelB = sequence;
					else
						g_netSession.reliablePeerSlots[peerIndex].prevRecvSeqDefault = sequence;
					g_netLastDeliveredRecvSequence = sequence;
					++g_netSession.reliablePeerSlots[peerIndex].packetCount;
					memcpy(&g_netSession.recvScratchPacket, &g_netSessionRecvQueue[fullQueueIndex],
						   sizeof(g_netSession.recvScratchPacket));
					NetReliable_RemoveQueuedPacket(fullQueueIndex);
					*outSenderDpid = g_netSession.recvScratchPacket.directPlayId;
					*outPayloadSize = g_netSession.recvScratchPacket.payloadSize;
					return g_netSession.recvScratchPacket.payload;
				}
			}
			++fullQueueIndex;
			if ((int)fullQueueIndex >= 1024)
				fullQueueIndex = 0;
			--fullRemaining;
		}
	}
	return NULL;
}

// FUNCTION: XVT 0x46F470
int NetSession_SendCompactGamePacket(int directPlayId, unsigned int* payload, int payloadSize, ...) {
	int appendTerminator;
	HRESULT sendResult;
	NetSessionCompactEncodedPacket encodedPacket;
	unsigned int packetType;
	uint16_t packetFlags;
	int deliveryMode;
	uint8_t* encodedPayload;
	int encodedSize;
	int packetDataSize;
	int exitStubResult;

	sendResult = 0;
	if (g_netSession.dplayInterface == NULL) {
		return 1;
	}
	packetType = *payload;
	packetFlags = (uint8_t)packetType & 0x7F;
	appendTerminator = packetType < NET_PACKET_RESYNC_CHECKSUMS || packetType >= NET_PACKET_RESYNC_CHUNK + 1;
	if (packetType == NET_PACKET_REMOTE_INPUT && g_gameConfig.asyncFlag == 1) {
		deliveryMode = 2;
	} else {
		if (directPlayId == 0) {
			deliveryMode = 0;
		} else if (directPlayId == g_netSession.groupDplayId) {
			deliveryMode = 2;
		} else {
			deliveryMode = 0;
		}
	}
	if (deliveryMode == 2) {
		packetFlags |= 0x8080;
	} else if (deliveryMode == 1) {
		packetFlags |= 0x8000;
	}
	encodedPacket.packetTypeHeader = (int16_t)packetFlags;
	encodedPayload = (uint8_t*)&encodedPacket.payloadSize;
	encodedSize = 2;
	if (appendTerminator) {
		exitStubResult = NetSession_ExitStub((int)packetType);
		packetDataSize = payloadSize;
		if (exitStubResult == 0) {
			encodedPayload = encodedPacket.payload;
			encodedPacket.payloadSize = (int16_t)(packetDataSize - 4);
			encodedSize = 4;
		}
	} else {
		packetDataSize = payloadSize;
	}
	memcpy(encodedPayload, payload + 1, (size_t)(packetDataSize - 4));
	encodedPayload += packetDataSize - 4;
	encodedSize += packetDataSize - 4;
	if (appendTerminator) {
		++encodedSize;
		*encodedPayload = NET_PACKET_NOP;
	}
	if (directPlayId != g_netSession.localPlayerInfo.directPlayId) {
		sendResult = g_netSession.dplayInterface->lpVtbl->Send(
			g_netSession.dplayInterface, (DPID)g_netSession.localPlayerInfo.directPlayId, (DPID)directPlayId,
			0, &encodedPacket.packetTypeHeader, (uint32_t)encodedSize);
	}

	return sendResult == 0;
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x46F5E0
int* NetSession_WaitForGamePacket(int* outDpid, int* outAux, int timeoutSeconds) {
	uint32_t startTime;
	uint32_t timeoutMilliseconds;
	int senderDpid;
	int payloadSize;
	int copiedPayloadSize;
	int* packet;

	timeoutMilliseconds = (uint32_t)(timeoutSeconds * 1000);
	startTime = timeGetTime();
	while (timeGetTime() - startTime <= timeoutMilliseconds) {
		packet = NetSession_ReceiveGamePacket(&senderDpid, &payloadSize);
		if (packet != NULL) {
			copiedPayloadSize = payloadSize;
			*outDpid = senderDpid;
			*outAux = copiedPayloadSize;
			return packet;
		}
	}
	return NULL;
}
#endif

// FUNCTION: XVT 0x46F660
int NetSession_FindPlayerSlotByDpid(int dpid) {
	int playerSlot;

	for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
		if (g_netSession.players[playerSlot].directPlayId == dpid) {
			return playerSlot;
		}
	}

	return playerSlot;
}

// FUNCTION: XVT 0x46F680
int NetSession_GetPlayerDplayId(int playerIndex) { return g_netSession.players[playerIndex].directPlayId; }

// FUNCTION: XVT 0x46F690
int NetSession_GetDplayIdByActivePlayerIndex(int activePlayerIndex) {
	int activeIndex;
	unsigned int playerSlot;

	activeIndex = 0;
	for (playerSlot = 0; playerSlot < 8; playerSlot++) {
		if (g_netSession.players[playerSlot].activeFlag != 0) {
			if (activeIndex == activePlayerIndex) {
				break;
			}
			activeIndex++;
		}
	}

	if (playerSlot < 8) {
		return g_netSession.players[playerSlot].directPlayId;
	}
	return 0;
}

// FUNCTION: XVT 0x46F6D0
int NetSession_FindActivePlayerIndexByDpid(int dpid) {
	int activePlayerIndex;
	int playerSlot;

	activePlayerIndex = 0;
	for (playerSlot = 0; playerSlot < 8; playerSlot++) {
		if (g_netSession.players[playerSlot].activeFlag != 0) {
			if (g_netSession.players[playerSlot].directPlayId == dpid) {
				break;
			}
			activePlayerIndex++;
		}
	}

	return activePlayerIndex;
}

// FUNCTION: XVT 0x46F700
int NetSession_GetHostDplayId(void) { return g_netSession.hostDplayId; }

// FUNCTION: XVT 0x46F710
int NetSession_GetLocalDplayId(void) { return g_netSession.localPlayerInfo.directPlayId; }

// FUNCTION: XVT 0x46F720
char* NetSession_GetPlayerName(int playerSlot) {
	int rosterIndex;

	if (g_netSession.playerCount == 1) {
		return g_netSession.localPlayerInfo.playerName;
	}

	rosterIndex = 0;
	if (g_netSession.playerCount > rosterIndex) {
		do {
			if (NetSession_FindPlayerSlotByDpid(g_netSession.players[rosterIndex].directPlayId) ==
				playerSlot) {
				return g_netSession.players[rosterIndex].playerName;
			}
			++rosterIndex;
		} while (rosterIndex < g_netSession.playerCount);
	}
#ifdef XVT_MODERN
	return NULL;
#endif
}

// FUNCTION: XVT 0x46F780
SessionPlayerInfo* NetSession_PeekQueuedPlayerInfo(void) {
	return g_netSession.playerInfoQueueCount > 0 ? g_netSession.playerInfoQueue : 0;
}

#ifndef XVT_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XVT 0x46F7A0
void NetSession_DiscardFirstQueuedPlayerInfo(void) {
	if (g_netSession.playerInfoQueueCount > 0) {
		memcpy(g_netSession.playerInfoQueue, &g_netSession.playerInfoQueue[1],
			   (size_t)(g_netSession.playerInfoQueueCount - 1));
		g_netSession.playerInfoQueueCount--;
	}
}
#ifndef XVT_MODERN
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XVT 0x46F7D0
int NetSession_CountLeadingActivePlayers(void) {
	int playerCount;

	for (playerCount = 0; playerCount < 8; ++playerCount) {
		if (g_netSession.players[playerCount].activeFlag == 0) {
			break;
		}
	}
	return playerCount;
}

// FUNCTION: XVT 0x46F7F0
void NetSession_SetPlayerCount(int playerCount) { g_netSession.playerCount = playerCount; }

// FUNCTION: XVT 0x46F800
void NetSession_AddPlayerToRosterSlot(int playerSlot, const SessionPlayerInfo* playerInfo) {
	SessionPlayerInfo* rosterSlot;

	g_netSession.players[playerSlot] = *playerInfo;
	rosterSlot = &g_netSession.players[playerSlot];
	rosterSlot->activeFlag = 1;
	g_netSession.playerCount++;
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x46F840
int NetSession_BroadcastPlayerRoster(int toPlayerId) {
	int outDpid;
	int outAux;
	int* packet;
	int playerIndex;

	g_netSessionScratchPacket.packetType = NET_PACKET_ROSTER_COUNT;
	g_netSessionScratchPacket.payloadDwords[0] = g_netSession.playerCount;
	NetSession_SendPacket(toPlayerId, (unsigned int*)&g_netSessionScratchPacket, 8);

	if (toPlayerId == 0) {
		do {
			packet = NetSession_WaitForGamePacket(&outDpid, &outAux, 60);
			if (packet == NULL)
				return 0;
		} while (*packet != NET_PACKET_ROSTER_COUNT);
	}

	for (playerIndex = 0; playerIndex < g_netSession.playerCount; ++playerIndex) {
		g_netSessionScratchPacket.packetType = NET_PACKET_ROSTER_ENTRY;
		g_netSessionScratchPacket.payloadDwords[0] = playerIndex;
		memcpy(&g_netSessionScratchPacket.payloadDwords[1], &g_netSession.players[playerIndex],
			   sizeof(SessionPlayerInfo));
		NetSession_SendPacket(toPlayerId, (unsigned int*)&g_netSessionScratchPacket, 48);
		if (toPlayerId == 0) {
			do {
				packet = NetSession_WaitForGamePacket(&outDpid, &outAux, 60);
				if (packet == NULL)
					return 0;
			} while (*packet != NET_PACKET_ROSTER_ENTRY);
		}
	}
	return 1;
}
#endif

// FUNCTION: XVT 0x46F9D0
int NetSession_GetQueuedPlayerInfoCount(void) { return g_netSession.playerInfoQueueCount; }

// FUNCTION: XVT 0x46F9E0
void NetSession_QueuePlayerInfo(const SessionPlayerInfo* playerInfo) {
	g_netSession.playerInfoQueue[g_netSession.playerInfoQueueCount] = *playerInfo;
	g_netSession.playerInfoQueueCount++;
}

// FUNCTION: XVT 0x46FA10
int NetSession_AddPlayerToGroup(const SessionPlayerInfo* playerInfo) {
	return g_netSession.dplayInterface->lpVtbl->AddPlayerToGroup(
		g_netSession.dplayInterface, g_netSession.groupDplayId, playerInfo->directPlayId);
}

// FUNCTION: XVT 0x46FA30
int NetSession_CountActivePlayers(void) {
	int activePlayerCount = 0;
	int playerIndex;

	for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
		if (g_netSession.players[playerIndex].activeFlag == 1)
			++activePlayerCount;
	}
	return activePlayerCount;
}

// FUNCTION: XVT 0x46FA50
int NetSession_RemovePlayerFromGroup(int playerDplayId) {
	return g_netSession.dplayInterface->lpVtbl->DeletePlayerFromGroup(
		g_netSession.dplayInterface, g_netSession.groupDplayId, (DPID)playerDplayId);
}

// FUNCTION: XVT 0x46FA70
int NetSession_SelectFirstActivePlayerAsHost(void) {
	int playerIndex;

	playerIndex = 0;
	while (g_netSession.players[playerIndex].activeFlag == 0) {
		playerIndex++;
		if (playerIndex >= 8) {
			return 0;
		}
	}
	g_netSession.hostDplayId = g_netSession.players[playerIndex].directPlayId;
	return 1;
}

// FUNCTION: XVT 0x46FAB0
int NetSession_UnusedStubReturnTrue(void) { return 1; }

// FUNCTION: XVT 0x46FAC0
int NetSession_StubReturnTrue(void) { return 1; }

// FUNCTION: XVT 0x46FAD0
int NetSession_ExitStub(int packetType) {
	(void)packetType;
	return 0;
}

// FUNCTION: XVT 0x46FAE0
int NetSession_SendReliableKeepalives(void) {
	SessionPlayerInfo* player;
	SessionPlayerInfo* playerRoster;
	unsigned int playerIndex;
	unsigned int peerSlot;
	unsigned int reliablePeerIndex;
	uint32_t currentTime;
	unsigned int nextSequence;
	int playerCount;

	playerIndex = 0;
	playerRoster = NetSession_GetPlayerRoster(&playerCount);
	if (playerCount != 0) {
		player = playerRoster;
		do {
			currentTime = timeGetTime();
			if (g_netSession.localPlayerInfo.directPlayId != player->directPlayId) {
				peerSlot = NetReliable_FindOrCreatePeerSlot(player->directPlayId);
				if (peerSlot < g_netSession.reliablePeerSlotCount && peerSlot < 8) {
					reliablePeerIndex = peerSlot;
					if (currentTime - g_netSession.reliablePeerSlots[peerSlot].lastActivityMs > 3000) {
						g_netSession.reliablePeerSlots[peerSlot].lastActivityMs = currentTime;
						NetSession_DebugTrace("(Sending RRA) ");
						g_netSessionScratchPacket.packetType = NET_PACKET_KEEPALIVE;
						nextSequence = g_netSession.reliablePeerSlots[reliablePeerIndex].recvSeqChannelA + 1;
						if (nextSequence > 127) {
							nextSequence = 0;
						}
						g_netSessionScratchPacket.payloadDwords[0] = (int)nextSequence;
						nextSequence = g_netSession.reliablePeerSlots[reliablePeerIndex].recvSeqChannelB + 1;
						if (nextSequence > 127) {
							nextSequence = 0;
						}
						g_netSessionScratchPacket.payloadDwords[1] = (int)nextSequence;
						nextSequence = g_netSession.reliablePeerSlots[reliablePeerIndex].recvSeqDefault + 1;
						if (nextSequence > 127) {
							nextSequence = 0;
						}
						g_netSessionScratchPacket.payloadDwords[2] = (int)nextSequence;
						g_netSessionScratchPacket.payloadDwords[3] = (int)timeGetTime();
						NetSession_SendCompactGamePacket(
							g_netSession.reliablePeerSlots[reliablePeerIndex].directPlayId,
							(unsigned int*)&g_netSessionScratchPacket, 20, 0);
					}
				}
			}
			++player;
			++playerIndex;
		} while ((unsigned int)playerCount > playerIndex);
	}
	return 1;
}
