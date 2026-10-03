#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/runtime/flight_checkpoint.h"
#include "xvt_runtime/runtime/flight_internal.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/runtime/network_session.h"
#include "xvt_runtime/runtime/resync_task.h"
#include "xvt_runtime/timing/host_clock.h"

typedef struct XvtFlightTauntsWire {
	XvtFlightSlotWire header;
	uint8_t text[sizeof(g_gameConfig.taunts)];
	XvtFlightAgreementWire agreement;
} XvtFlightTauntsWire;

static void XvtFlightNetwork_WriteAgreement(XvtFlightAgreementWire* agreement);
static int XvtFlightNetwork_MatchesAgreement(const XvtFlightAgreementWire* agreement);

enum {
	SYNC_IDLE,
	SYNC_HOST_PLAYERS,
	SYNC_ROSTER_SEND,
	SYNC_ROSTER_COUNT,
	SYNC_ROSTER_PLAYERS,
	SYNC_OPTIONS_HOST,
	SYNC_OPTIONS_ROSTER,
	SYNC_TAUNTS,
	SYNC_START_HOST,
	SYNC_START_PACKET,
	SYNC_START_ACKS
};

static struct {
	unsigned acknowledgements;
	unsigned taunts_seen;
	XvtFlightAgreementWire taunt_agreement[XVT_FLIGHT_PLAYERS];
	int phase, count, expected, roster_index, packet_type, alert, blink;
	uint64_t packet_deadline, status_time;
} g_sync;

static uint32_t g_cookieCounter, g_missionCookie;

uint32_t XvtFlightNetwork_Cookie(void) { return g_missionCookie; }

void XvtFlightNetwork_CloseSession(void) { g_cookieCounter = g_missionCookie = 0; }

static int XvtFlightNetwork_BeginAgreement(void) {
	if (g_cookieCounter == UINT32_MAX) {
		XvtNetworkSession_Leave();
		return 0;
	}
	g_missionCookie = ++g_cookieCounter;
	return 1;
}

static int XvtFlightNetwork_Acknowledge(int sender, int slot) {
	if ((unsigned)slot >= 8 || NetSession_FindPlayerSlotByDpid(sender) != slot ||
		g_players[slot].network.directPlayId != sender || (g_sync.acknowledgements & (1u << slot)))
		return 0;
	g_sync.acknowledgements |= 1u << slot;
	++g_sync.count;
	return 1;
}

static void XvtFlightNetwork_Wait(int seconds);

static void XvtFlightNetwork_WriteAgreement(XvtFlightAgreementWire* agreement) {
	XvtWire_Set32(agreement->schema, XVT_TIMING_SCHEMA);
	XvtWire_Set32(agreement->profile, XVT_WIRE_PROFILE_NETWORK_125);
	XvtWire_Set32(agreement->cookie, g_missionCookie);
}

static int XvtFlightNetwork_MatchesAgreement(const XvtFlightAgreementWire* agreement) {
	return XvtWire_Get32(agreement->schema) == XVT_TIMING_SCHEMA &&
		   XvtWire_Get32(agreement->profile) == XVT_WIRE_PROFILE_NETWORK_125 &&
		   XvtWire_Get32(agreement->cookie) == g_missionCookie;
}

static void XvtFlightNetwork_Taunts(void) {
	XvtFlightTauntsWire packet;
	XvtWire_Set32(packet.header.opcode, NET_PACKET_PLAYER_TAUNTS);
	XvtWire_Set32(packet.header.player, g_localPlayer);
	memcpy(packet.text, g_gameConfig.taunts, sizeof packet.text);
	XvtFlightNetwork_WriteAgreement(&packet.agreement);
	g_sync.count = 0;
	g_sync.acknowledgements = 0;
	for (unsigned player = 0; player < XVT_FLIGHT_PLAYERS; ++player) {
		if ((g_sync.taunts_seen & (1u << player)) &&
			XvtFlightNetwork_MatchesAgreement(&g_sync.taunt_agreement[player])) {
			g_sync.acknowledgements |= 1u << player;
			++g_sync.count;
		}
	}
	XvtFlightNetwork_SendWire(0, &packet, sizeof packet);
	g_sync.phase = SYNC_TAUNTS;
	XvtFlightNetwork_Wait(30);
}

static int XvtFlightNetwork_Finish(int result) {
	if (g_sync.alert || g_sync.phase == SYNC_TAUNTS)
		FlightAlert_RestoreBoxBackground();
	memset(&g_sync, 0, sizeof(g_sync));
	return result;
}

static void XvtFlightNetwork_Wait(int seconds) {
	g_sync.packet_deadline = XvtTime_GetElapsedUs() / 1000 + (unsigned)seconds * 1000;
}

static int* XvtFlightNetwork_Poll(int* sender, int* size, int seconds) {
	int* packet = NetSession_ReceiveGamePacket(sender, size);
	if (packet)
		XvtFlightNetwork_Wait(seconds);
	return packet;
}

static int XvtFlightNetwork_Expired(void) { return XvtTime_GetElapsedUs() / 1000 > g_sync.packet_deadline; }

static void XvtFlightNetwork_Alert(void) {
	FlightAlert_SaveBoxBackground();
	g_sync.alert = 1;
	g_sync.blink = 1;
	FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_WAITING_FOR_OTHER_PLAYERS], 0x30);
}

static void XvtFlightNetwork_LoadingStatus(int sender) {
	uint64_t now = XvtTime_GetElapsedUs() / 1000;
	char text[256];
	char* name;
	if (now - g_sync.status_time <= 200)
		return;
	g_sync.status_time = now;
	name = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(sender));
	if (!name)
		strcpy(text, g_strDiskIoMessages[DISK_IO_STR_OTHER_PLAYERS_STILL_LOADING]);
	else {
		strcpy(text, name);
		g_sync.blink = !g_sync.blink;
		strcat(text, g_strDiskIoMessages[g_sync.blink ? DISK_IO_STR_PLAYER_STILL_LOADING_MINUS
													  : DISK_IO_STR_PLAYER_STILL_LOADING_PLUS]);
	}
	FlightAlert_DrawBox(3, text, 0x30);
}

int XvtFlightNetwork_BeginSession(int players, int in_progress) {
	XvtNetworkSession_FlightReady();
	memset(&g_sync, 0, sizeof(g_sync));
	g_sync.expected = players;
	XvtFlightNetwork_Wait(60);
	if (NetSession_GetLocalPlayerId()) {
		g_sync.phase = players ? SYNC_HOST_PLAYERS : SYNC_ROSTER_SEND;
	} else {
		if (in_progress)
			return 1;
		g_sync.phase = SYNC_ROSTER_COUNT;
	}
	return XVT_FLIGHT_NETWORK_PENDING;
}

static void XvtFlightNetwork_SendRosterRecord(void) {
	int index = g_sync.roster_index;
	if (index < 0) {
		g_netSessionScratchPacket.packetType = NET_PACKET_ROSTER_COUNT;
		g_netSessionScratchPacket.payloadDwords[0] = g_netSession.playerCount;
		XvtFlightNetwork_SendPacket(0, (unsigned*)&g_netSessionScratchPacket, 8);
		g_sync.packet_type = NET_PACKET_ROSTER_COUNT;
	} else {
		g_netSessionScratchPacket.packetType = NET_PACKET_ROSTER_ENTRY;
		g_netSessionScratchPacket.payloadDwords[0] = index;
		memcpy(&g_netSessionScratchPacket.payloadDwords[1], &g_netSession.players[index],
			   sizeof(SessionPlayerInfo));
		XvtFlightNetwork_SendPacket(0, (unsigned*)&g_netSessionScratchPacket, 48);
		g_sync.packet_type = NET_PACKET_ROSTER_ENTRY;
	}
	XvtFlightNetwork_Wait(60);
}

int XvtFlightNetwork_Session(void) {
	int sender, size;
	int* packet;
	if (g_sync.phase == SYNC_HOST_PLAYERS && g_sync.count >= g_sync.expected) {
		g_sync.phase = SYNC_ROSTER_SEND;
		g_sync.roster_index = -1;
	}
	if (g_sync.phase == SYNC_ROSTER_SEND) {
		if (g_sync.expected <= 1)
			return XvtFlightNetwork_Finish(1);
		if (!g_sync.packet_type)
			XvtFlightNetwork_SendRosterRecord();
	}
	packet = XvtFlightNetwork_Poll(&sender, &size, 60);
	if (!packet) {
		if (!XvtFlightNetwork_Expired())
			return XVT_FLIGHT_NETWORK_PENDING;
		if (g_sync.phase != SYNC_ROSTER_PLAYERS)
			g_netSession.dplayInterface = NULL;
		return XvtFlightNetwork_Finish(0);
	}
	if (size < 4)
		return XVT_FLIGHT_NETWORK_PENDING;
	if (g_sync.phase == SYNC_HOST_PLAYERS && packet[0] == NET_PACKET_STARTUP_READY)
		++g_sync.count;
	else if (g_sync.phase == SYNC_ROSTER_SEND && packet[0] == g_sync.packet_type) {
		++g_sync.roster_index;
		g_sync.packet_type = NET_PACKET_NONE;
		if (g_sync.roster_index >= g_netSession.playerCount) {
			g_netSessionScratchPacket.packetType = NET_PACKET_NOP;
			XvtFlightNetwork_SendPacket(0, (unsigned*)&g_netSessionScratchPacket, 4);
			return XvtFlightNetwork_Finish(1);
		}
	} else if (g_sync.phase == SYNC_ROSTER_COUNT && packet[0] == NET_PACKET_ROSTER_COUNT && size >= 8 &&
			   packet[1] >= 0 && packet[1] <= 8) {
		g_netSession.playerCount = packet[1];
		g_sync.phase = SYNC_ROSTER_PLAYERS;
		g_sync.count = 0;
		if (!g_netSession.playerCount)
			return XvtFlightNetwork_Finish(1);
	} else if (g_sync.phase == SYNC_ROSTER_PLAYERS && packet[0] == NET_PACKET_ROSTER_ENTRY &&
			   size >= 8 + (int)sizeof(SessionPlayerInfo) && (unsigned)packet[1] < 8) {
		memcpy(&g_netSession.players[packet[1]], packet + 2, sizeof(SessionPlayerInfo));
		if (++g_sync.count >= g_netSession.playerCount)
			return XvtFlightNetwork_Finish(1);
	}
	return XVT_FLIGHT_NETWORK_PENDING;
}

static void XvtFlightNetwork_SendOptions(void) {
	uint8_t bytes[sizeof(XvtFlightRosterHeader) + XVT_FLIGHT_PLAYERS * sizeof(XvtFlightRosterPlayerWire) +
				  sizeof(XvtFlightAgreementWire)];
	XvtFlightRosterHeader header;
	XvtWire_Set32(header.opcode, NET_PACKET_PLAYER_OPTIONS_ROSTER);
	XvtWire_Set32(header.new_net, g_flightConfNewNet);
	memcpy(bytes, &header, sizeof header);
	size_t offset = sizeof header;
	for (int player = 0; player < g_activeFlightPlayerCount; ++player) {
		XvtFlightRosterPlayerWire record;
		XvtWire_Set32(record.resolution, g_players[player].network.flightResolutionMode);
		XvtWire_Set32(record.rating, g_players[player].pilotRating);
		memcpy(bytes + offset, &record, sizeof record);
		offset += sizeof record;
	}
	XvtFlightAgreementWire agreement;
	XvtFlightNetwork_WriteAgreement(&agreement);
	memcpy(bytes + offset, &agreement, sizeof agreement);
	offset += sizeof agreement;
	XvtFlightNetwork_BroadcastWire(bytes, offset);
	g_sync.phase = SYNC_OPTIONS_ROSTER;
	XvtFlightNetwork_Wait(60);
}

static int XvtFlightNetwork_ReadTaunts(int sender, const void* bytes, unsigned size) {
	if (size != sizeof(XvtFlightTauntsWire))
		return 0;
	XvtFlightTauntsWire packet;
	memcpy(&packet, bytes, sizeof packet);
	unsigned slot = XvtWire_Get32(packet.header.player);
	if (slot >= XVT_FLIGHT_PLAYERS || NetSession_FindPlayerSlotByDpid(sender) != (int)slot ||
		g_players[slot].network.directPlayId != sender ||
		XvtWire_Get32(packet.agreement.schema) != XVT_TIMING_SCHEMA ||
		XvtWire_Get32(packet.agreement.profile) != XVT_WIRE_PROFILE_NETWORK_125 ||
		!XvtWire_Get32(packet.agreement.cookie))
		return 0;
	if (g_sync.phase == SYNC_OPTIONS_ROSTER) {
		g_sync.taunts_seen |= 1u << slot;
		g_sync.taunt_agreement[slot] = packet.agreement;
		memcpy(g_playerTauntText[slot], packet.text, sizeof packet.text);
	} else if (g_sync.phase == SYNC_TAUNTS && XvtFlightNetwork_MatchesAgreement(&packet.agreement) &&
			   XvtFlightNetwork_Acknowledge(sender, slot)) {
		memcpy(g_playerTauntText[slot], packet.text, sizeof packet.text);
	}
	return 1;
}

static int XvtFlightNetwork_ReadRoster(int sender, const void* packet, unsigned size) {
	size_t offset =
		sizeof(XvtFlightRosterHeader) + g_activeFlightPlayerCount * sizeof(XvtFlightRosterPlayerWire);
	if (sender != NetSession_GetHostDplayId() || size != offset + sizeof(XvtFlightAgreementWire))
		return 0;
	XvtFlightAgreementWire agreement;
	const uint8_t* bytes = packet;
	memcpy(&agreement, bytes + offset, sizeof agreement);
	if (XvtWire_Get32(agreement.schema) != XVT_TIMING_SCHEMA ||
		XvtWire_Get32(agreement.profile) != XVT_WIRE_PROFILE_NETWORK_125 || !XvtWire_Get32(agreement.cookie))
		return 0;
	g_missionCookie = XvtWire_Get32(agreement.cookie);
	if (!NetSession_GetLocalPlayerId()) {
		FlightAlert_RestoreBoxBackground();
		g_sync.alert = 0;
		XvtFlightRosterHeader header;
		memcpy(&header, bytes, sizeof header);
		g_flightConfNewNet = XvtWire_Get32(header.new_net);
		for (int player = 0; player < g_activeFlightPlayerCount; ++player) {
			XvtFlightRosterPlayerWire record;
			memcpy(&record, bytes + sizeof header + player * sizeof record, sizeof record);
			g_players[player].network.flightResolutionMode = XvtWire_Get32(record.resolution);
			g_players[player].pilotRating = XvtWire_Get32(record.rating);
		}
	}
	XvtFlightNetwork_Taunts();
	return 1;
}

int XvtFlightNetwork_Options(void) {
	int sender, size;
	int* packet;
	if (!g_sync.phase) {
		if (NetSession_GetLocalPlayerId() && !XvtFlightNetwork_BeginAgreement())
			return 0;
		if (g_activeFlightPlayerCount <= 1) {
			g_players[0].network.flightResolutionMode = g_flightResolutionMode;
			g_players[0].pilotRating = g_pilotData.rating;
			memcpy(g_playerTauntText, g_gameConfig.taunts, sizeof(g_gameConfig.taunts));
			return 1;
		}
		if (NetSession_GetLocalPlayerId()) {
			for (int i = 0; i < g_activeFlightPlayerCount; ++i) {
				g_players[i].network.flightResolutionMode = FLIGHT_RESOLUTION_320X240;
				g_players[i].pilotRating = 0;
			}
			NetSession_CountActivePlayers();
			g_players[g_localPlayer].network.flightResolutionMode = g_flightResolutionMode;
			g_players[g_localPlayer].pilotRating = g_pilotData.rating;
			g_sync.phase = SYNC_OPTIONS_HOST;
		} else {
			XvtFlightOptionsWire options;
			XvtWire_Set32(options.opcode, NET_PACKET_PLAYER_OPTIONS);
			XvtWire_Set32(options.resolution, g_flightResolutionMode);
			XvtWire_Set32(options.rating, g_pilotData.rating);
			XvtWire_Set32(options.schema, XVT_TIMING_SCHEMA);
			XvtFlightNetwork_SendWire(NetSession_GetHostDplayId(), &options, sizeof options);
			g_sync.phase = SYNC_OPTIONS_ROSTER;
		}
		XvtFlightNetwork_Alert();
		XvtFlightNetwork_Wait(60);
	}
	if (g_sync.phase == SYNC_OPTIONS_HOST && g_sync.count >= NetSession_CountActivePlayers() - 1) {
		FlightAlert_RestoreBoxBackground();
		g_sync.alert = 0;
		XvtFlightNetwork_SendOptions();
	}
	if (g_sync.phase == SYNC_TAUNTS && g_sync.count >= NetSession_CountActivePlayers())
		return XvtFlightNetwork_Finish(1);
	packet = XvtFlightNetwork_Poll(&sender, &size, g_sync.phase == SYNC_TAUNTS ? 30 : 60);
	if (!packet)
		return XvtFlightNetwork_Expired() ? XvtFlightNetwork_Finish(0) : XVT_FLIGHT_NETWORK_PENDING;
	if (size < 4)
		return XVT_FLIGHT_NETWORK_PENDING;
	if (packet[0] == NET_PACKET_STILL_LOADING)
		XvtFlightNetwork_LoadingStatus(sender);
	if ((g_sync.phase == SYNC_OPTIONS_ROSTER || g_sync.phase == SYNC_TAUNTS) &&
		packet[0] == NET_PACKET_PLAYER_TAUNTS) {
		XvtFlightNetwork_ReadTaunts(sender, packet, size);
	} else if (g_sync.phase == SYNC_OPTIONS_HOST && packet[0] == NET_PACKET_PLAYER_OPTIONS &&
			   size == sizeof(XvtFlightOptionsWire)) {
		XvtFlightOptionsWire options;
		memcpy(&options, packet, sizeof options);
		int player = NetSession_FindPlayerSlotByDpid(sender);
		if (XvtWire_Get32(options.schema) == XVT_TIMING_SCHEMA &&
			XvtFlightNetwork_Acknowledge(sender, player)) {
			g_players[player].network.flightResolutionMode = XvtWire_Get32(options.resolution);
			g_players[player].pilotRating = XvtWire_Get32(options.rating);
		}
	} else if (g_sync.phase == SYNC_OPTIONS_ROSTER && packet[0] == NET_PACKET_PLAYER_OPTIONS_ROSTER) {
		XvtFlightNetwork_ReadRoster(sender, packet, size);
	}
	return XVT_FLIGHT_NETWORK_PENDING;
}

int XvtFlightNetwork_Start(void) {
	int sender, size;
	int* packet;
	if (!g_sync.phase) {
		FlightNet_InitMissionStartAckState();
		dtMs = XVT_WORLD_MESSAGE_TICKS;
		g_flightNetWorldChecksumResetAccumMs = 0;
		memset(g_flightNetPeerSilenceTicks, 0, sizeof(g_flightNetPeerSilenceTicks));
		g_flightNetResyncPlayerDplayId = 0;
		g_flightNetPendingAckCount = g_flightNetClockAdjustAccumTicks = 0;
		g_flightNetHostTimeoutElapsedMs = 0;
		if (g_activeFlightPlayerCount == 1) {
			g_serverTickTime = g_gameTime = 0;
			g_inputTimestamp = g_flightNetClockLeadAllowanceMs = 30;
			Time_GetFrameDelta();
			return 1;
		}
		g_sync.expected = NetSession_CountActivePlayers();
		g_flightNetScratchPacket.packetType = NET_PACKET_MISSION_LOADING_READY;
		XvtFlightNetwork_SendPacket(NetSession_GetHostDplayId(), (unsigned*)&g_flightNetScratchPacket, 4);
		g_sync.phase = NetSession_GetLocalPlayerId() ? SYNC_START_HOST : SYNC_START_PACKET;
		if (g_sync.phase == SYNC_START_PACKET)
			XvtFlightNetwork_Alert();
		XvtFlightNetwork_Wait(60);
	}
	if (g_sync.phase == SYNC_START_HOST && g_sync.count >= g_sync.expected) {
		g_flightNetScratchPacket.packetType = NET_PACKET_FLIGHT_MISSION_START;
		XvtFlightNetwork_Broadcast((unsigned*)&g_flightNetScratchPacket, 4);
		XvtFlightNetwork_Alert();
		g_sync.phase = SYNC_START_PACKET;
	}
	if (g_sync.phase == SYNC_START_ACKS) {
		FlightNet_ProcessIncomingPackets();
		g_inputTimestamp += Time_GetFrameDelta();
		if (g_flightNetPendingAckCount && (unsigned)g_inputTimestamp < 100)
			return XVT_FLIGHT_NETWORK_PENDING;
		g_flightNetPendingAckCount = 0;
		g_inputTimestamp += Time_GetFrameDelta();
		if (g_inputTimestamp < 35) {
			int adjustment = 35 - g_inputTimestamp;
			g_inputTimestamp += adjustment;
			g_flightNetClockAdjustAccumTicks -= adjustment;
		}
		/* The host keeps a fixed lead instead of the start-up delay measured above.
		 * Its clock starts at that lead; the adjustment accumulator keeps the world
		 * message schedule on the measured start, as clock nudges do. */
		g_flightNetClockLeadAllowanceMs = XVT_HOST_CLOCK_LEAD_TICKS;
		g_flightNetClockAdjustAccumTicks += g_inputTimestamp - XVT_HOST_CLOCK_LEAD_TICKS;
		g_inputTimestamp = XVT_HOST_CLOCK_LEAD_TICKS;
		return XvtFlightNetwork_Finish(1);
	}
	packet = XvtFlightNetwork_Poll(&sender, &size, 60);
	if (!packet)
		return XvtFlightNetwork_Expired() ? XvtFlightNetwork_Finish(0) : XVT_FLIGHT_NETWORK_PENDING;
	if (!XvtFlightNetwork_DecodeControl((const uint8_t*)packet, &size))
		return XVT_FLIGHT_NETWORK_PENDING;
	if (g_sync.phase == SYNC_START_HOST && packet[0] == NET_PACKET_MISSION_LOADING_READY)
		XvtFlightNetwork_Acknowledge(sender, NetSession_FindPlayerSlotByDpid(sender));
	else if (g_sync.phase == SYNC_START_PACKET) {
		if (packet[0] == NET_PACKET_STILL_LOADING)
			XvtFlightNetwork_LoadingStatus(sender);
		if (packet[0] == NET_PACKET_FLIGHT_MISSION_START && sender == NetSession_GetHostDplayId()) {
			FlightAlert_RestoreBoxBackground();
			g_sync.alert = 0;
			g_flightNetScratchPacket.packetType = NET_PACKET_ACK;
			XvtFlightNetwork_SendPacket(NetSession_GetHostDplayId(), (unsigned*)&g_flightNetScratchPacket, 4);
			Time_GetFrameDelta();
			g_serverTickTime = g_gameTime = g_inputTimestamp = 0;
			/* Unlike the original's 130-tick async lead, both modes start at 30; startup
			 * probes replace it within a few round trips. */
			g_flightNetClockLeadAllowanceMs = 30;
			if (!NetSession_GetLocalPlayerId()) {
				XvtFlightNetwork_BeginClockProbes();
				return XvtFlightNetwork_Finish(1);
			}
			g_flightNetPendingAckCount = g_sync.expected == 1 ? 1 : 2;
			FlightNet_InitMissionStartAckState();
			g_sync.phase = SYNC_START_ACKS;
		}
	}
	return XVT_FLIGHT_NETWORK_PENDING;
}

void XvtFlightNetwork_Reset(void) {
	XvtFlightNetwork_Finish(0);
	g_missionCookie = 0;
}

static int XvtFlightNetwork_HasCookie(unsigned opcode) {
	switch (opcode) {
		case NET_PACKET_PLAYER_DISCONNECTED:
		case NET_PACKET_WORLD_CHECKSUM:
		case NET_PACKET_SESSION_ABORT:
		case NET_PACKET_RESYNC_CHUNK_ACK:
		case NET_PACKET_FLIGHT_MISSION_START:
		case NET_PACKET_MISSION_LOADING_READY:
		case NET_PACKET_PLAYER_ABORT:
		case NET_PACKET_RESYNC_NOTICE:
		case NET_PACKET_SERVER_CHECKSUM:
		case NET_PACKET_ACK:
		case NET_PACKET_CLOCK_LEAD:
		case NET_PACKET_STILL_LOADING:
		case NET_PACKET_CLOCK_PROBE:
		case NET_PACKET_CLOCK_PROBE_REPLY:
		case NET_PACKET_RESYNC_CHECKSUMS:
		case NET_PACKET_RESYNC_REQUEST:
		case NET_PACKET_RESYNC_APPLY:
		case NET_PACKET_RESYNC_CHUNK:
			return 1;
		default:
			return 0;
	}
}

int XvtFlightNetwork_SendPacket(int dpid, const unsigned* packet, int size) {
	if (!g_missionCookie || size < 4 || !XvtFlightNetwork_HasCookie(packet[0]))
		return NetSession_SendPacket(dpid, (unsigned*)packet, size);
	unsigned copy[XVT_FLIGHT_PACKET_BYTES / sizeof(unsigned)];
	if (size > XVT_FLIGHT_PACKET_BYTES - (int)sizeof(XvtWireU32))
		return 0;
	memcpy(copy, packet, size);
	XvtWire_Set32((uint8_t*)copy + size, g_missionCookie);
	return NetSession_SendPacket(dpid, copy, size + (int)sizeof(XvtWireU32));
}

int XvtFlightNetwork_Broadcast(const unsigned* packet, int size) {
	if (!g_missionCookie || size < 4 || !XvtFlightNetwork_HasCookie(packet[0]))
		return NetSession_BroadcastPacketToPlayers((unsigned*)packet, size);
	unsigned copy[XVT_FLIGHT_PACKET_BYTES / sizeof(unsigned)];
	if (size > XVT_FLIGHT_PACKET_BYTES - (int)sizeof(XvtWireU32))
		return 0;
	memcpy(copy, packet, size);
	XvtWire_Set32((uint8_t*)copy + size, g_missionCookie);
	return NetSession_BroadcastPacketToPlayers(copy, size + (int)sizeof(XvtWireU32));
}

int XvtFlightNetwork_DecodeControl(const uint8_t* packet, int* size) {
	if (*size < (int)sizeof(XvtWireU32) || *size > XVT_FLIGHT_PACKET_BYTES)
		return 0;
	unsigned opcode = XvtWire_Get32(packet);
	if (XvtFlightNetwork_HasCookie(opcode)) {
		if (!g_missionCookie || *size < (int)(2 * sizeof(XvtWireU32)) ||
			XvtWire_Get32(packet + *size - sizeof(XvtWireU32)) != g_missionCookie)
			return 0;
		*size -= sizeof(XvtWireU32);
	}
	size_t minimum = sizeof(XvtWireU32);
	switch (opcode) {
		case NET_PACKET_PLAYER_DISCONNECTED:
		case NET_PACKET_RESYNC_CHUNK_ACK:
		case NET_PACKET_PLAYER_ABORT:
		case NET_PACKET_RESYNC_NOTICE:
		case NET_PACKET_CLOCK_LEAD:
		case NET_PACKET_CLOCK_PROBE_REPLY:
			minimum = sizeof(XvtFlightEpochWire);
			break;
		case NET_PACKET_CLOCK_PROBE:
			minimum = sizeof(XvtFlightClockProbeWire);
			break;
		case NET_PACKET_RESYNC_REQUEST:
			minimum = sizeof(XvtFlightResyncRequestWire);
			break;
		case NET_PACKET_RESYNC_APPLY:
			minimum = sizeof(XvtFlightResyncApplyWire);
			break;
		case NET_PACKET_RESYNC_CHUNK:
			minimum = sizeof(XvtFlightChunkHeader) + sizeof(XvtWireU32);
			break;
		case NET_PACKET_WORLD_CHECKSUM:
			minimum = sizeof(XvtFlightChecksumReportWire);
			break;
		case NET_PACKET_SERVER_CHECKSUM:
			minimum = sizeof(XvtFlightChecksumWire);
			break;
		case NET_PACKET_RESYNC_CHECKSUMS:
			minimum = sizeof(XvtFlightEpochWire);
			break;
		default:
			break;
	}
	return (size_t)*size >= minimum;
}

int XvtFlightNetwork_SendWire(int dpid, const void* packet, size_t size) {
	unsigned aligned[XVT_FLIGHT_PACKET_BYTES / sizeof(unsigned)];
	if (size < sizeof(XvtWireU32) || size > sizeof aligned)
		return 0;
	memcpy(aligned, packet, size);
	return XvtFlightNetwork_SendPacket(dpid, aligned, (int)size);
}

int XvtFlightNetwork_BroadcastWire(const void* packet, size_t size) {
	unsigned aligned[XVT_FLIGHT_PACKET_BYTES / sizeof(unsigned)];
	if (size < sizeof(XvtWireU32) || size > sizeof aligned)
		return 0;
	memcpy(aligned, packet, size);
	return XvtFlightNetwork_Broadcast(aligned, (int)size);
}

/* Flight-data staging is separate from transport sequencing. */

static struct {
	XvtFlightMessage outgoing;
	unsigned part, parts, batch_count, batch_sends, part_sends, packets_received;
	XvtFlightInputWire batch[XVT_INPUT_STAGED_RECORDS];
	int sampled, last_flush, recovery;
	unsigned departures;
	uint64_t iteration;
	FlightInputFrameRecord held;
} g_io;

static int XvtFlightNetwork_RecordInput(unsigned player, int tick, const FlightInputFrameRecord* input,
										int authoritative) {
	XvtInputInsertStatus result = XvtFlightHistory_InsertReal(player, tick, input, authoritative);
	if (result == XVT_INPUT_FULL || result == XVT_INPUT_INVALID || result == XVT_INPUT_CONFLICT) {
		XvtFlightNetwork_RequestRecovery();
		return 0;
	}
	return 1;
}

int XvtFlightNetwork_NeedsRecovery(void) { return g_io.recovery; }

void XvtFlightNetwork_RequestRecovery(void) {
	if (!g_io.recovery)
		Aeron_LogWarn("xvt.network", "flight history/message capacity or continuity requires state recovery");
	g_io.recovery = 1;
}

void XvtFlightNetwork_BeginRecovery(void) { g_io.recovery = 0; }

void XvtFlightNetwork_Recovered(void) {
	g_io.recovery = 0;
	g_io.sampled = 0;
	unsigned retained = 0;
	for (unsigned i = 0; i < g_io.batch_count; ++i)
		if (XvtWire_Get32(g_io.batch[i].tick) > (unsigned)g_gameTime)
			g_io.batch[retained++] = g_io.batch[i];
	g_io.batch_count = retained;
}

void XvtFlightNetwork_BeginIteration(void) {
	uint64_t iteration = XvtTime_GetElapsedUs();
	if (g_io.iteration == iteration)
		return;
	g_io.iteration = iteration;
	g_io.sampled = 0;
	g_io.batch_sends = g_io.part_sends = g_io.packets_received = 0;
}

void XvtFlightNetwork_BeginMission(void) {
	XvtFlightMessages_Reset();
	memset(&g_io, 0, sizeof g_io);
	g_io.iteration = UINT64_MAX;
}

void XvtFlightNetwork_FlushInput(int now) {
	while (g_io.batch_count && g_io.batch_sends < XVT_INPUT_BATCHES_PER_ITERATION &&
		   (now - g_io.last_flush >= XVT_INPUT_BATCH_TICKS || g_io.batch_count >= XVT_INPUT_BATCH_RECORDS)) {
		unsigned count =
			g_io.batch_count < XVT_INPUT_BATCH_RECORDS ? g_io.batch_count : XVT_INPUT_BATCH_RECORDS;
		unsigned
			packet[(sizeof(XvtFlightBatchHeader) + XVT_INPUT_BATCH_RECORDS * sizeof(XvtFlightInputWire)) /
				   sizeof(unsigned)];
		size_t size =
			XvtFlightMessages_EncodeBatch((uint8_t*)packet, XvtFlightNetwork_Cookie(), g_io.batch, count);
		int host = NetSession_GetHostDplayId();
		for (unsigned i = 0; i < XVT_FLIGHT_PLAYERS; ++i) {
			if (i == (unsigned)g_localPlayer || !g_players[i].connectedFlag)
				continue;
			int dpid = g_players[i].network.directPlayId;
			int send =
				!g_asyncFlag
					? g_playerConnected[i]
					: dpid == host || (g_flightNetSmallSessionPlayerThreshold > g_activeFlightPlayerCount &&
									   g_playerConnected[i]);
			if (send)
				NetSession_SendPacket(dpid, packet, (int)size);
		}
		g_io.batch_count -= count;
		memmove(g_io.batch, g_io.batch + count, g_io.batch_count * sizeof g_io.batch[0]);
		++g_io.batch_sends;
		g_io.last_flush = now;
	}
}

int XvtFlightNetwork_AdmitInput(int tick) {
	if (g_io.recovery || !XvtFlightWire_ValidTick((unsigned)tick) ||
		g_io.batch_count == XVT_INPUT_STAGED_RECORDS)
		return 0;
	/* Replay of retained local history must not consume a second device command. */
	for (int i = 0; i < g_inputFrameCount[g_localPlayer]; ++i)
		if (g_inputHistory[g_localPlayer][i].timestamp == tick)
			return 1;
	if (g_inputFrameCount[g_localPlayer] >= XVT_INPUT_HISTORY_CAPACITY) {
		XvtFlightNetwork_RequestRecovery();
		return 0;
	}
	if (!g_io.sampled) {
		XvtFlightControls_SampleRecorded(&g_io.held);
		g_io.sampled = 1;
	}
	if (!XvtFlightNetwork_RecordInput(g_localPlayer, tick, &g_io.held, 0))
		return 0;
	XvtFlightWire_EncodeInput(&g_io.batch[g_io.batch_count++], tick, &g_io.held);
	g_io.held.key = 0;
	g_io.held.flags = 0;
	g_io.held.throttle = 0;
	return 1;
}

int XvtFlightNetwork_Outgoing(void) { return g_io.parts != 0; }

void XvtFlightNetwork_FlushWorld(void) {
	while (g_io.parts && g_io.part_sends < XVT_WORLD_PARTS_PER_ITERATION) {
		unsigned packet[XVT_FLIGHT_PACKET_BYTES / sizeof(unsigned)];
		size_t size = XvtFlightMessages_EncodePart((uint8_t*)packet, &g_io.outgoing,
												   XvtFlightNetwork_Cookie(), g_io.part);
		for (unsigned i = 0; i < XVT_FLIGHT_PLAYERS; ++i)
			if (i != (unsigned)g_localPlayer && (g_io.outgoing.mask & (1u << i)))
				NetSession_SendPacket(g_players[i].network.directPlayId, packet, (int)size);
		++g_io.part_sends;
		if (++g_io.part == g_io.parts) {
			g_io.parts = 0;
			g_io.part = 0;
		}
	}
}

void XvtFlightNetwork_SendWorld(void) {
	if (g_io.parts || g_io.recovery)
		return;
	XvtFlightMessage* message = &g_io.outgoing;
	memset(message, 0, sizeof *message);
	if (g_flightNetLastSentWorldMessageTimestamp > INT32_MAX - XVT_WORLD_MESSAGE_TICKS - 1) {
		g_flightMissionState.missionEndPending = 1;
		return;
	}
	int tick = g_flightNetLastSentWorldMessageTimestamp + XVT_WORLD_MESSAGE_TICKS;
	if (!XvtFlightWire_ValidTick((unsigned)tick)) {
		g_flightMissionState.missionEndPending = 1;
		return;
	}
	message->target_flags = (unsigned)tick;
	for (unsigned player = 0; player < XVT_FLIGHT_PLAYERS; ++player) {
		if (!g_players[player].connectedFlag || g_playerAbortFlags[player] ||
			(g_io.departures & (1u << player)))
			continue;
		message->mask |= 1u << player;
		for (int i = 0; i < g_inputFrameCount[player]; ++i) {
			InputFrame* frame = &g_inputHistory[player][i];
			if (!frame->applied || frame->timestamp > tick)
				continue;
			XvtFlightWorldInputWire* record = &message->records[message->count++];
			record->player = player;
			XvtFlightWire_EncodeInput(&record->input, frame->timestamp, &frame->input);
		}
	}
	if (!message->mask)
		return;
	g_flightNetWorldChecksumResetAccumMs += XVT_WORLD_MESSAGE_TICKS;
	if (g_flightNetWorldChecksumResetAccumMs > XVT_WORLD_CHECKSUM_TICKS) {
		g_flightNetWorldChecksumResetAccumMs = 0;
		message->target_flags |= XVT_WORLD_CHECKSUM_FLAG;
		memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof g_flightNetWorldChecksumPeerStatus);
	}
	if (!XvtFlightMessages_Enqueue(message, XVT_QUEUE_PENDING)) {
		XvtFlightNetwork_RequestRecovery();
		return;
	}
	for (unsigned player = 0; player < XVT_FLIGHT_PLAYERS; ++player)
		for (int i = 0; i < g_inputFrameCount[player]; ++i) {
			InputFrame* frame = &g_inputHistory[player][i];
			if ((message->mask & (1u << player)) && frame->applied && frame->timestamp <= tick)
				frame->applied = 0;
		}
	g_flightNetLastSentWorldMessageTimestamp = tick;
	++g_flightNetSentWorldMessageCount;
	for (unsigned player = 0; player < XVT_FLIGHT_PLAYERS; ++player) {
		if (player == (unsigned)g_localPlayer || !(message->mask & (1u << player)) ||
			g_flightNetPeerSilenceTicks[player] == -1)
			continue;
		g_flightNetPeerSilenceTicks[player] += XVT_WORLD_MESSAGE_TICKS;
		if (g_flightNetPeerSilenceTicks[player] > XVT_PEER_TIMEOUT_TICKS) {
			FlightNet_BroadcastPlayerAbort(player);
			g_io.departures |= 1u << player;
		}
	}
	g_io.parts = XvtFlightMessages_PartCount(message->count);
	XvtFlightNetwork_FlushWorld();
}

int XvtFlightNetwork_InsertWorld(const XvtFlightMessage* message) {
	for (unsigned i = 0; i < message->count; ++i) {
		const XvtFlightWorldInputWire* record = &message->records[i];
		int tick;
		FlightInputFrameRecord input;
		if (!XvtFlightWire_DecodeInput(&record->input, &tick, &input) ||
			!XvtFlightNetwork_RecordInput(record->player, tick, &input, 1))
			return 0;
	}
	return 1;
}

int XvtFlightNetwork_Receive(int sender, const uint8_t* bytes, size_t size) {
	if (size < 4)
		return 0;
	unsigned opcode = XvtWire_Get32(bytes);
	if (opcode == NET_PACKET_INPUT_BATCH) {
		int player = NetSession_FindPlayerSlotByDpid(sender);
		if ((unsigned)player >= XVT_FLIGHT_PLAYERS || player == g_localPlayer ||
			!g_players[player].connectedFlag ||
			!XvtFlightMessages_DecodeBatch(bytes, size, XvtFlightNetwork_Cookie()))
			return 1;
		if (XvtResync_HoldsInput())
			return 1;
		FlightSync_DiscardPredictedInputFrames(player);
		g_flightNetPeerSilenceTicks[player] = 0;
		XvtFlightBatchHeader header;
		memcpy(&header, bytes, sizeof header);
		unsigned count = XvtWire_Get16(header.count);
		for (unsigned i = 0; i < count; ++i) {
			int tick;
			FlightInputFrameRecord input;
			XvtFlightInputWire record;
			memcpy(&record, bytes + sizeof header + i * sizeof record, sizeof record);
			XvtFlightWire_DecodeInput(&record, &tick, &input);
			if (!XvtFlightNetwork_RecordInput(player, tick, &input, 0))
				break;
		}
		return 1;
	}
	if (opcode != NET_PACKET_WORLD_MESSAGE)
		return opcode == NET_PACKET_REMOTE_INPUT;
	if (sender != NetSession_GetHostDplayId())
		return 1;
	static XvtFlightMessage message;
	int result = XvtFlightMessages_ReceivePart(bytes, size, XvtFlightNetwork_Cookie(),
											   XvtResync_ReceiveFloor(), &message);
	if (result < 0)
		XvtFlightNetwork_RequestRecovery();
	if (result == 1) {
		if ((message.mask & ~XvtFlightCheckpoint_InitialMask()) ||
			!XvtFlightMessages_Enqueue(&message, XvtResync_IsActive() && !NetSession_GetLocalPlayerId()
													 ? XVT_QUEUE_REPLAY
													 : XVT_QUEUE_PENDING))
			XvtFlightNetwork_RequestRecovery();
		else {
			g_flightNetHostTimeoutElapsedMs = 0;
			++g_flightNetReceivedWorldMessageCount;
		}
	}
	return 1;
}

int XvtFlightNetwork_PlayerAbort(unsigned player) {
	if (player >= XVT_FLIGHT_PLAYERS)
		return 0;
	if (NetSession_GetLocalPlayerId())
		g_io.departures |= 1u << player;
	return player != (unsigned)g_localPlayer;
}

uint64_t XvtFlightNetwork_NextWakeDelayUs(int now) {
	if (g_io.parts || (XvtFlightMessages_Count(XVT_QUEUE_PENDING) && !g_io.recovery))
		return 0;
	if (g_io.batch_count) {
		int remaining = XVT_INPUT_BATCH_TICKS - (now - g_io.last_flush);
		return remaining > 0 ? XvtFlightTime_DelayForTicks((unsigned)remaining) : 0;
	}
	return XvtFlightTime_DelayForTicks(XVT_WORLD_MESSAGE_TICKS);
}

int XvtFlightNetwork_ShouldSend(int inputTimestamp) {
	int interval = XVT_WORLD_MESSAGE_TICKS;
	if (XvtFlightNetwork_Outgoing() || XvtFlightNetwork_NeedsRecovery() || XvtResync_HasStateRequest() ||
		!XvtFlightMessages_HasRoom(XVT_QUEUE_PENDING, sizeof(XvtFlightMessage)))
		return 0;
	if (g_flightNetPendingAckCount)
		return 0;
	int adjusted = inputTimestamp + g_flightNetClockAdjustAccumTicks;
	if (!g_flightNetNextClientInputSendTimestamp)
		g_flightNetNextClientInputSendTimestamp =
			adjusted + (g_flightNetClockLeadAllowanceMs >> XVT_WORLD_START_LEAD_SHIFT);
	int elapsed = adjusted - g_flightNetNextClientInputSendTimestamp;
	if (elapsed < interval)
		return 0;
	if (elapsed > XVT_WORLD_LATE_INTERVALS * interval) {
		g_flightNetNextClientInputSendTimestamp += interval;
		return 1;
	}
	int oldest = INT32_MAX;
	for (unsigned player = 0; player < XVT_FLIGHT_PLAYERS; ++player) {
		if (!g_players[player].connectedFlag)
			continue;
		const InputFrame* input = FlightSync_FindLastNonzeroInputFrame(player);
		if (!input) {
			oldest = 0;
			break;
		}
		if (input->timestamp < oldest)
			oldest = input->timestamp;
	}
	if (g_flightNetLastSentWorldMessageTimestamp + interval >= oldest)
		return 0;
	g_flightNetNextClientInputSendTimestamp += interval;
	return 1;
}

int XvtFlightNetwork_TakePacketBudget(void) {
	XvtFlightNetwork_BeginIteration();
	if (g_io.packets_received >= XVT_NETWORK_PACKETS_PER_ITERATION)
		return 0;
	++g_io.packets_received;
	return 1;
}
