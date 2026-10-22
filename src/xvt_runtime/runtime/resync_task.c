#include "xvt_runtime/runtime/resync_task.h"
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/runtime/flight_checkpoint.h"
#include "xvt_runtime/runtime/flight_diag.h"
#include "xvt_runtime/runtime/flight_internal.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/snapshot/render_capture.h"
#include "xvt_runtime/snapshot/world_state.h"
#include "xvt_runtime/timing/flight_timing.h"
#include <stdio.h>
#include <stdlib.h>

enum {
	RESYNC_IDLE,
	RESYNC_CHECKSUMS,
	RESYNC_BUILD,
	RESYNC_ACKS,
	RESYNC_APPLY,
	RESYNC_FULL_RECEIVE,
	RESYNC_REPLAY
};

static struct {
	int phase, peer_dpid, image_size, elapsed, pulse, retries, blink;
	int offset, slot, free_bytes, payload_offset;
	int ack_count, ack_previous, ack_retries, ack_elapsed, final_batch, owns_alert;
	uint8_t* world;
	uint8_t* pinned;
	unsigned checksums[XVT_WORLD_CHECKSUM_REGIONS], lengths[XVT_WORLD_CHECKSUM_REGIONS], epoch;
	int completed_tick, restart_requested, escaped;
} g_resync;

static struct {
	int sender;
	XvtFlightChecksumReportWire packet;
} g_checksums[XVT_DEFERRED_CHECKSUMS];

static unsigned g_checksum_read, g_checksum_count;

void XvtResync_DeferChecksum(int sender, const int* packet) {
	/* Other peers' checksum continuations resume after the current transfer. */
	if (g_checksum_count == XVT_DEFERRED_CHECKSUMS) {
		Aeron_LogError("xvt.network", "checksum continuation queue exhausted");
		XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_CHECKSUM_QUEUE, sender);
		g_flightMissionState.missionEndPending = 1;
		return;
	}
	unsigned index = (g_checksum_read + g_checksum_count++) % XVT_DEFERRED_CHECKSUMS;
	g_checksums[index].sender = sender;
	memcpy(&g_checksums[index].packet, packet, sizeof(XvtFlightChecksumReportWire));
}

static int XvtResync_Escape(void) {
	if (!FlightInput_HasKeyReady() || FlightInput_GetNextKey() != FLIGHT_KEY_ESCAPE)
		return 0;
	g_resync.escaped = 1;
	return 1;
}

/* Host side: the peer receiving the world state is dropped from the mission. */
static void XvtResync_AbandonSend(int dpid, const char* stage) {
	int slot = NetSession_FindPlayerSlotByDpid(dpid);
	Aeron_LogWarn("xvt.network",
				  "world state transfer to player %d (dpid %d) abandoned: %s%s; removing the player", slot,
				  dpid, stage, g_resync.escaped ? " (host pressed ESC)" : "");
	FlightNet_BroadcastPlayerAbort(slot);
}

static void XvtResync_CompleteChecksum(void) {
	if (NetSession_GetLocalPlayerId()) {
		int index = NetSession_FindPlayerSlotByDpid(g_resync.peer_dpid);
		int status = 3;
		if ((unsigned)index < 8)
			g_flightNetWorldChecksumPeerStatus[index] = 2;
		for (int i = 0; i < 8; ++i)
			if (g_players[i].connectedFlag)
				status &= g_flightNetWorldChecksumPeerStatus[i];
		if (status & 1)
			g_flightNetBufferWorldMessagesUntilChecksum = 0;
	}
	free(g_resync.pinned);
	memset(&g_resync, 0, sizeof(g_resync));
}

static void XvtResync_Pulse(int resending) {
	g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;
	XvtFlightNetwork_Broadcast((unsigned*)&g_flightNetScratchPacket, sizeof(int));
	g_resync.blink = !g_resync.blink;
	FlightAlert_DrawBox(3,
						g_strDiskIoMessages[g_resync.blink ? DISK_IO_STR_ESC_BOOT_PLAYER
											: resending    ? DISK_IO_STR_RESENDING_PACKET_WAIT
														   : DISK_IO_STR_RECOVERING_WAIT],
						0x30);
}

static void XvtResync_EndSend(int success) {
	if (!success)
		Aeron_LogWarn("xvt.network", "world state transfer to dpid %d failed", g_resync.peer_dpid);
	FlightAlert_RestoreBoxBackground();
	g_resync.owns_alert = 0;
	Time_GetFrameDelta();
	g_flightNetPendingAckCount = 0;
	if (success)
		XvtResync_BeginApply(g_resync.peer_dpid, g_resync.image_size);
	else
		XvtResync_CompleteChecksum();
}

static struct {
	unsigned checksums[XVT_WORLD_CHECKSUM_REGIONS], lengths[XVT_WORLD_CHECKSUM_REGIONS], epoch;
	int table_valid, size, tick, request_sent, restarts, restart_pending;
	uint64_t deadline;
} g_receive;

static void XvtResync_SendRequest(void) {
	XvtFlightChecksumWire table;
	XvtWire_Set32(table.opcode, NET_PACKET_SERVER_CHECKSUM);
	XvtWire_Set32(table.epoch, g_resync.epoch);
	for (unsigned region = 0; region < XVT_WORLD_CHECKSUM_REGIONS; ++region) {
		XvtWire_Set32(table.checksums[region], g_resync.checksums[region]);
		XvtWire_Set32(table.lengths[region], g_resync.lengths[region]);
	}
	XvtFlightNetwork_SendWire(g_resync.peer_dpid, &table, sizeof table);
	XvtFlightResyncRequestWire request;
	XvtWire_Set32(request.opcode, NET_PACKET_RESYNC_REQUEST);
	XvtWire_Set32(request.epoch, g_resync.epoch);
	XvtWire_Set32(request.image_bytes, g_resync.image_size);
	XvtWire_Set32(request.completed_tick, g_resync.completed_tick);
	XvtFlightNetwork_SendWire(g_resync.peer_dpid, &request, sizeof request);
}

int XvtResync_BeginSend(int player, uint8_t* world, int size) {
	char text[256];
	char* name;
	if (g_resync.phase)
		return -1;
	free(g_resync.pinned);
	memset(&g_resync, 0, sizeof(g_resync));
	g_resync.peer_dpid = player;
	g_resync.world = world;
	g_resync.image_size = size;
	g_resync.phase = RESYNC_CHECKSUMS;
	g_resync.retries = XVT_RESYNC_RETRIES;
	g_inputTimestamp += Time_GetFrameDelta();
	FlightAlert_SaveBoxBackground();
	g_resync.owns_alert = 1;
	strcpy(text, g_strDiskIoMessages[DISK_IO_STR_COM_FAILURE_SENDING]);
	name = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(player));
	if (name)
		strcat(text, name);
	FlightAlert_DrawBox(1, text, 0x30);
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_NOTICE;
	g_flightNetScratchPacket.payloadDwords[0] = player;
	XvtFlightNetwork_Broadcast((unsigned*)&g_flightNetScratchPacket, 8);
	size_t prefix;
	if (!XvtFlightCheckpoint_Validate(world, size, &prefix, &g_resync.completed_tick)) {
		Aeron_LogWarn("xvt.network", "world state for dpid %d is not a valid checkpoint (%d bytes)", player,
					  size);
		XvtResync_EndSend(0);
		return 0;
	}
	g_resync.pinned = malloc((size_t)size);
	if (!g_resync.pinned) {
		Aeron_LogWarn("xvt.network", "cannot allocate %d bytes for the world state transfer", size);
		XvtResync_EndSend(0);
		return 0;
	}
	memcpy(g_resync.pinned, world, size);
	g_resync.world = g_resync.pinned;
	g_resync.epoch = (unsigned)g_resync.completed_tick;
	if (!XvtSnapshot_ChecksumImage(world, size, g_resync.checksums, g_resync.lengths)) {
		Aeron_LogWarn("xvt.network", "cannot checksum the world state for dpid %d", player);
		XvtResync_EndSend(0);
		return 0;
	}
	Aeron_LogInfo("xvt.network", "sending world state to dpid %d '%s': %d bytes, tick %d", player,
				  name ? name : "?", size, g_resync.completed_tick);
	XvtResync_SendRequest();
	g_flightNetPendingAckCount = 1;
	g_flightNetRemoteResyncChecksumsReceivedFlag = 0;
	return -1;
}

static void XvtResync_NewChunk(void) {
	FlightNetWorldStateChunkPacket* packet = &g_flightNetWorldStateChunkPackets[g_resync.slot];
	packet->packetType = NET_PACKET_RESYNC_CHUNK;
	packet->baseChecksum = (int)g_resync.epoch;
	packet->chunkIndex = g_resync.slot;
	g_resync.free_bytes = XVT_FLIGHT_PACKET_BYTES - sizeof(XvtFlightChunkHeader) - 2 * sizeof(XvtWireU32);
	g_resync.payload_offset = 0;
}

static void XvtResync_SendChunk(void) {
	FlightNetWorldStateChunkPacket* packet = &g_flightNetWorldStateChunkPackets[g_resync.slot];
	XvtWire_Set32(packet->payload + g_resync.payload_offset, UINT32_MAX);
	size_t packet_bytes = sizeof(XvtFlightChunkHeader) + g_resync.payload_offset + sizeof(XvtWireU32);
	XvtFlightNetwork_SendPacket(g_resync.peer_dpid, (unsigned*)packet, (int)packet_bytes);
	++g_resync.slot;
}

static void XvtResync_Checksums(void) {
	int saved = g_inputTimestamp;
	if (XvtResync_Escape()) {
		g_resync.retries = 1;
		g_resync.elapsed = XVT_RESYNC_RETRY_TICKS;
	} else {
		FlightNet_ProcessIncomingPackets();
		if (g_resync.restart_requested)
			return;
		g_inputTimestamp += Time_GetFrameDelta();
		g_resync.elapsed += g_inputTimestamp - saved;
		g_inputTimestamp = saved;
	}
	if (g_flightNetRemoteResyncChecksumsReceivedFlag || g_resync.elapsed >= XVT_RESYNC_RETRY_TICKS) {
		g_resync.pulse += g_resync.elapsed;
		if (g_resync.pulse >= XVT_RESYNC_RETRY_TICKS) {
			g_resync.pulse = 0;
			g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;
			XvtFlightNetwork_SendPacket(g_resync.peer_dpid, (unsigned*)&g_flightNetScratchPacket, 4);
			XvtResync_Pulse(0);
		}
		if (g_flightNetRemoteResyncChecksumsReceivedFlag) {
			memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
			XvtResync_NewChunk();
			g_resync.phase = RESYNC_BUILD;
		} else if (!--g_resync.retries) {
			XvtResync_AbandonSend(g_resync.peer_dpid, "the peer never confirmed the checksum table");
			XvtResync_EndSend(0);
		} else {
			g_resync.elapsed = 0;
			g_flightNetRemoteResyncChecksumsReceivedFlag = 0;
			XvtResync_SendRequest();
		}
	}
}

static void XvtResync_BeginAcks(int final) {
	g_resync.final_batch = final;
	g_resync.phase = RESYNC_ACKS;
	g_resync.ack_count = g_resync.slot;
	g_resync.ack_previous = 0;
	g_resync.ack_retries = XVT_RESYNC_ACK_RETRIES;
	g_resync.ack_elapsed = 0;
	g_resync.pulse = 0;
	g_resync.blink = 0;
}

static void XvtResync_Build(void) {
	/* Send the pinned complete image in bounded batches. */
	while (g_resync.offset < g_resync.image_size) {
		FlightNetWorldStateChunkPacket* packet = &g_flightNetWorldStateChunkPackets[g_resync.slot];
		int bytes = g_resync.image_size - g_resync.offset + sizeof(FlightNetWorldStateChunkRecordHeader);
		if (bytes > g_resync.free_bytes)
			bytes = g_resync.free_bytes;
		FlightNetWorldStateChunkRecordHeader header = { g_resync.offset, bytes - sizeof(header) };
		memcpy(packet->payload + g_resync.payload_offset, &header, sizeof(header));
		memcpy(packet->payload + g_resync.payload_offset + sizeof(header), g_resync.world + g_resync.offset,
			   header.dataSize);
		g_resync.offset += header.dataSize;
		g_resync.free_bytes -= bytes;
		g_resync.payload_offset += bytes;
		if (g_resync.free_bytes < XVT_RESYNC_CHUNK_MIN_FREE) {
			XvtResync_SendChunk();
			if (g_resync.slot == XVT_RESYNC_CHUNKS_PER_BATCH) {
				XvtResync_BeginAcks(g_resync.offset == g_resync.image_size);
				return;
			}
			XvtResync_NewChunk();
		}
	}
	if (g_resync.payload_offset != 0) {
		XvtResync_SendChunk();
		XvtResync_BeginAcks(1);
	} else if (g_resync.slot != 0)
		XvtResync_BeginAcks(1);
	else
		XvtResync_EndSend(1);
}

int XvtResync_WaitAcks(int player, int count) {
	int ack = 0, saved = g_inputTimestamp;
	if (XvtResync_Escape()) {
		g_resync.ack_elapsed = XVT_RESYNC_RETRY_TICKS;
		ack = g_resync.ack_previous;
		g_resync.ack_retries = 1;
	} else {
		FlightNet_ProcessIncomingPackets();
		if (g_resync.restart_requested)
			return -1;
		g_inputTimestamp += Time_GetFrameDelta();
		g_resync.ack_elapsed += g_inputTimestamp - saved;
		g_inputTimestamp = saved;
		while (ack < count && g_flightNetWorldStateChunkAcked[ack])
			++ack;
		if (ack == count)
			return 1;
	}
	if (g_resync.ack_elapsed < XVT_RESYNC_RETRY_TICKS)
		return -1;
	g_resync.pulse += g_resync.ack_elapsed;
	if (g_resync.pulse >= XVT_RESYNC_RETRY_TICKS) {
		g_resync.pulse = 0;
		XvtResync_Pulse(1);
	}
	if (g_resync.ack_previous == ack)
		--g_resync.ack_retries;
	else
		g_resync.ack_retries = XVT_RESYNC_ACK_RETRIES;
	g_resync.ack_previous = ack;
	g_resync.ack_elapsed = 0;
	if (g_resync.ack_retries)
		return -1;
	char stage[64];
	snprintf(stage, sizeof stage, "chunk acknowledgements stalled at %d of %d", ack, count);
	XvtResync_AbandonSend(player, stage);
	return 0;
}

void XvtResync_BeginApply(int player, int size) {
	g_resync.phase = RESYNC_APPLY;
	g_resync.peer_dpid = player;
	g_resync.image_size = size;
	g_resync.retries = XVT_RESYNC_RETRIES;
	g_resync.pulse = 0;
	g_resync.elapsed = g_inputTimestamp;
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_APPLY;
	g_flightNetScratchPacket.payloadDwords[0] = (int)g_resync.epoch;
	g_flightNetScratchPacket.payloadDwords[1] = size;
	g_flightNetScratchPacket.payloadDwords[2] = g_inputTimestamp;
	XvtFlightNetwork_SendPacket(player, (unsigned*)&g_flightNetScratchPacket, 16);
	Time_GetFrameDelta();
	g_flightNetPendingAckCount = 1;
}

static void XvtResync_Apply(void) {
	if (XvtResync_Escape()) {
		g_resync.retries = 1;
		g_inputTimestamp += XVT_RESYNC_RETRY_TICKS;
	} else {
		FlightNet_ProcessIncomingPackets();
		if (g_resync.restart_requested)
			return;
		g_inputTimestamp += Time_GetFrameDelta();
		if (g_flightNetWorldStateAckReceivedFlag) {
			g_flightNetWorldStateAckReceivedFlag = 0;
			g_inputTimestamp = g_resync.elapsed;
		}
	}
	if (g_flightNetPendingAckCount &&
		(unsigned)(g_inputTimestamp - g_resync.elapsed) < XVT_RESYNC_RETRY_TICKS)
		return;
	g_resync.pulse += g_inputTimestamp - g_resync.elapsed;
	if (g_resync.pulse >= XVT_RESYNC_RETRY_TICKS) {
		g_resync.pulse = 0;
		g_flightNetScratchPacket.packetType = NET_PACKET_STILL_LOADING;
		XvtFlightNetwork_Broadcast((unsigned*)&g_flightNetScratchPacket, 4);
	}
	if (g_flightNetPendingAckCount && --g_resync.retries) {
		g_resync.elapsed = g_inputTimestamp;
		g_flightNetPendingAckCount = 1;
		XvtFlightResyncApplyWire apply;
		XvtWire_Set32(apply.opcode, NET_PACKET_RESYNC_APPLY);
		XvtWire_Set32(apply.epoch, g_resync.epoch);
		XvtWire_Set32(apply.image_bytes, g_resync.image_size);
		XvtWire_Set32(apply.input_tick, g_inputTimestamp);
		XvtFlightNetwork_SendWire(g_resync.peer_dpid, &apply, sizeof apply);
		return;
	}
	if (g_flightNetPendingAckCount == 1) {
		XvtResync_AbandonSend(g_resync.peer_dpid, "the peer never acknowledged applying the world state");
		g_flightNetPendingAckCount = 0;
	} else
		Aeron_LogInfo("xvt.network", "world state transfer to dpid %d complete", g_resync.peer_dpid);
	g_inputTimestamp += Time_GetFrameDelta();
	g_inputTimestamp = g_flightNetClockLeadAllowanceMs + g_serverTickTime;
	g_flightNetScratchPacket.packetType = NET_PACKET_RESYNC_NOTICE;
	g_flightNetScratchPacket.payloadDwords[0] = 0;
	XvtFlightNetwork_Broadcast((unsigned*)&g_flightNetScratchPacket, 8);
	XvtResync_CompleteChecksum();
}

static void XvtResync_RestartReceive(void) {
	XvtFlightNetwork_RequestRecovery("state recovery restarted");
	Aeron_LogWarn("xvt.network", "restarting state recovery (attempt %d)", g_receive.restarts + 1);
	if (++g_receive.restarts > XVT_RESYNC_REPLAY_RESTARTS) {
		XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_RESYNC_RESTARTS, g_receive.restarts);
		FlightNet_BroadcastPlayerAbort(g_localPlayer);
		g_flightMissionState.missionEndPending = 1;
		XvtResync_Reset();
		return;
	}
	g_resync.phase = RESYNC_IDLE;
	g_receive.request_sent = 0;
	XvtFlightMessages_Clear(XVT_QUEUE_PENDING);
	XvtFlightMessages_Clear(XVT_QUEUE_REPLAY);
	XvtFlightFrame_ResetReplay();
	XvtResync_RequestState();
}

static int XvtResync_ReadyChecksum(void) {
	for (unsigned i = 0; i < g_checksum_count; ++i) {
		unsigned index = (g_checksum_read + i) % XVT_DEFERRED_CHECKSUMS;
		const XvtFlightChecksumReportWire* packet = &g_checksums[index].packet;
		int ready = (XvtWire_Get32(packet->request_state) == XVT_CHECKSUM_REQUEST_STATE
						 ? g_serverTickTime >= g_flightNetLastSentWorldMessageTimestamp
						 : XvtWire_Get32(packet->state.epoch) <= g_flightNetWorldChecksumEpoch);
		if (ready)
			return (int)i;
	}
	return -1;
}

static void XvtResync_ServiceChecksums(void) {
	for (unsigned work = 0; work < XVT_NETWORK_PACKETS_PER_ITERATION && g_resync.phase == RESYNC_IDLE;
		 ++work) {
		int ready = XvtResync_ReadyChecksum();
		if (ready < 0)
			return;
		unsigned index = (g_checksum_read + (unsigned)ready) % XVT_DEFERRED_CHECKSUMS;
		int sender = g_checksums[index].sender;
		int packet[sizeof(XvtFlightChecksumReportWire) / sizeof(int)];
		memcpy(packet, &g_checksums[index].packet, sizeof packet);
		for (unsigned i = (unsigned)ready; i + 1 < g_checksum_count; ++i)
			g_checksums[(g_checksum_read + i) % XVT_DEFERRED_CHECKSUMS] =
				g_checksums[(g_checksum_read + i + 1) % XVT_DEFERRED_CHECKSUMS];
		--g_checksum_count;
		FlightSync_HandleWorldChecksumPacket(sender, packet);
	}
}

void XvtResync_Tick(void) {
	if (g_resync.phase != RESYNC_IDLE && NetSession_GetLocalPlayerId() && g_resync.restart_requested) {
		Aeron_LogInfo("xvt.network", "world state transfer to dpid %d restarted at the peer's request",
					  g_resync.peer_dpid);
		if (g_resync.owns_alert)
			FlightAlert_RestoreBoxBackground();
		free(g_resync.pinned);
		memset(&g_resync, 0, sizeof g_resync);
		g_flightNetPendingAckCount = 0;
	}
	if (g_receive.restart_pending) {
		g_receive.restart_pending = 0;
		XvtResync_RestartReceive();
	}
	if (g_resync.phase == RESYNC_IDLE)
		XvtResync_ServiceChecksums();
	if (XvtFlightTiming_IsNetwork125()) {
		XvtFlightNetwork_BeginIteration();
		XvtFlightNetwork_FlushWorld();
	}
	switch (g_resync.phase) {
		case RESYNC_FULL_RECEIVE:
			FlightNet_ProcessIncomingPackets();
			if (XvtFlightNetwork_NeedsRecovery()) {
				XvtResync_RestartReceive();
				break;
			}
			if (Aeron_NowUs() >= g_receive.deadline) {
				XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_RESYNC_RECEIVE_TIMEOUT, g_receive.size);
				g_flightMissionState.missionEndPending = 1;
				XvtResync_Reset();
			}
			break;
		case RESYNC_REPLAY: {
			FlightNet_ProcessIncomingPackets();
			if (XvtFlightNetwork_NeedsRecovery()) {
				XvtResync_RestartReceive();
				break;
			}
			XvtFlightReplayResult replay = XvtFlightFrame_ReplayBuffered();
			if (replay == XVT_REPLAY_IDLE || replay == XVT_REPLAY_TERMINAL) {
				unsigned ack = NET_PACKET_ACK;
				XvtFlightNetwork_SendPacket(NetSession_GetHostDplayId(), &ack, 4);
				g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
				if (g_resync.owns_alert)
					FlightAlert_RestoreBoxBackground();
				g_resync.owns_alert = 0;
				g_resync.phase = RESYNC_IDLE;
				Aeron_LogInfo("xvt.network", "state recovery complete at tick %d", g_serverTickTime);
				XvtFlightNetwork_Recovered();
				XvtFlightControls_Recover();
				if (replay != XVT_REPLAY_TERMINAL)
					XvtResync_WorldApplied();
				g_receive.request_sent = g_receive.restarts = 0;
			}
			break;
		}
		case RESYNC_IDLE:
			break;
		case RESYNC_CHECKSUMS:
			XvtResync_Checksums();
			break;
		case RESYNC_BUILD:
			XvtResync_Build();
			break;
		case RESYNC_ACKS: {
			int result = XvtResync_WaitAcks(g_resync.peer_dpid, g_resync.ack_count);
			if (result == -1)
				break;
			if (!result || g_resync.final_batch)
				XvtResync_EndSend(result);
			else {
				XvtResync_Pulse(0);
				g_resync.slot = 0;
				memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
				XvtResync_NewChunk();
				g_resync.phase = RESYNC_BUILD;
			}
			break;
		}
		case RESYNC_APPLY:
			XvtResync_Apply();
			break;
		default:
			break;
	}
	if (g_resync.phase != RESYNC_IDLE && g_flightMissionState.missionEndPending)
		XvtResync_Reset();
}

int XvtResync_HasStateRequest(void) {
	for (unsigned i = 0; i < g_checksum_count; ++i)
		if (XvtWire_Get32(g_checksums[(g_checksum_read + i) % XVT_DEFERRED_CHECKSUMS].packet.request_state) ==
			XVT_CHECKSUM_REQUEST_STATE)
			return 1;
	return 0;
}

int XvtResync_IsActive(void) { return g_resync.phase != RESYNC_IDLE; }

int XvtResync_HoldsInput(void) {
	return !NetSession_GetLocalPlayerId() &&
		   (g_receive.request_sent || g_resync.phase == RESYNC_FULL_RECEIVE ||
			g_resync.phase == RESYNC_REPLAY);
}

uint64_t XvtResync_NextWakeDelayUs(void) {
	if (g_receive.restart_pending || g_resync.restart_requested || g_resync.phase == RESYNC_BUILD ||
		g_resync.phase == RESYNC_REPLAY || (g_resync.phase == RESYNC_IDLE && XvtResync_ReadyChecksum() >= 0))
		return 0;
	if (g_resync.phase == RESYNC_FULL_RECEIVE || g_receive.request_sent) {
		uint64_t now = Aeron_NowUs();
		return g_receive.deadline > now ? g_receive.deadline - now : 0;
	}
	int elapsed = g_resync.phase == RESYNC_ACKS    ? g_resync.ack_elapsed
				  : g_resync.phase == RESYNC_APPLY ? g_inputTimestamp - g_resync.elapsed
												   : g_resync.elapsed;
	if (g_resync.phase == RESYNC_IDLE)
		return UINT64_MAX;
	unsigned remaining = elapsed < XVT_RESYNC_RETRY_TICKS ? XVT_RESYNC_RETRY_TICKS - elapsed : 0;
	return XvtFlightTime_DelayForTicks(remaining);
}

int XvtResync_ReceiveFloor(void) {
	return g_resync.phase == RESYNC_FULL_RECEIVE || g_resync.phase == RESYNC_REPLAY ? g_receive.tick
																					: g_serverTickTime;
}

void XvtResync_Reset(void) {
	if (g_resync.owns_alert)
		FlightAlert_RestoreBoxBackground();
	free(g_resync.pinned);
	memset(&g_resync, 0, sizeof(g_resync));
	g_checksum_read = g_checksum_count = 0;
	memset(&g_receive, 0, sizeof g_receive);
}

void XvtResync_WorldApplied(void) { XvtRenderCapture_WorldChanged(); }

void XvtResync_RequestState(void) {
	if (!XvtFlightTiming_IsNetwork125())
		return;
	if (g_receive.request_sent) {
		if (Aeron_NowUs() >= g_receive.deadline) {
			XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_STATE_REQUEST_TIMEOUT, (int)g_receive.table_valid);
			FlightNet_BroadcastPlayerAbort(g_localPlayer);
			g_flightMissionState.missionEndPending = 1;
		}
		return;
	}
	if (NetSession_GetLocalPlayerId()) {
		/* A host cannot obtain an authoritative image from a client. */
		XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_HOST_RECOVERY, g_serverTickTime);
		g_flightMissionState.missionEndPending = 1;
		FlightNet_BroadcastLocalPlayerLeft();
		return;
	}
	XvtFlightChecksumReportWire packet = { 0 };
	XvtWire_Set32(packet.state.opcode, NET_PACKET_WORLD_CHECKSUM);
	XvtWire_Set32(packet.state.epoch, g_flightNetWorldChecksumEpoch);
	for (unsigned region = 0; region < XVT_WORLD_CHECKSUM_REGIONS; ++region) {
		XvtWire_Set32(packet.state.checksums[region], g_worldChecksum[region]);
		XvtWire_Set32(packet.state.lengths[region], g_peerChecksumRegionLengths[region]);
	}
	XvtWire_Set32(packet.request_state, XVT_CHECKSUM_REQUEST_STATE);
	XvtFlightNetwork_SendWire(NetSession_GetHostDplayId(), &packet, sizeof packet);
	Aeron_LogInfo("xvt.network", "requesting world state from the host at confirmed tick %d",
				  g_serverTickTime);
	g_receive.request_sent = 1;
	g_receive.deadline = Aeron_NowUs() + (uint64_t)XVT_PEER_TIMEOUT_TICKS * XVT_FLIGHT_TICK_US;
}

static int XvtResync_FullRequest(const uint8_t* bytes, unsigned size) {
	XvtFlightResyncRequestWire request;
	if (size != sizeof request || !g_receive.table_valid)
		return 1;
	memcpy(&request, bytes, sizeof request);
	if (XvtWire_Get32(request.epoch) != g_receive.epoch)
		return 1;
	size_t total = XvtWire_Get32(request.image_bytes), sum = 0;
	unsigned tick = XvtWire_Get32(request.completed_tick);
	for (unsigned region = 0; region < XVT_WORLD_CHECKSUM_REGIONS; ++region)
		sum += g_receive.lengths[region];
	if (total != sum || total < sizeof(XvtStateFooter) || total > XvtSnapshot_CalculateSize() ||
		tick != g_receive.epoch || tick > INT32_MAX || tick % XVT_NETWORK_STEP_TICKS)
		return 1;
	if (g_resync.phase != RESYNC_FULL_RECEIVE) {
		if (!XvtFlightMessages_PrepareRecovery(tick)) {
			XvtFlightNetwork_RequestRecovery("replay storage exhausted preparing recovery to tick %u", tick);
			g_receive.restart_pending = 1;
			return 1;
		}
		XvtFlightFrame_ResetReplay();
		XvtFlightNetwork_BeginRecovery();
		g_receive.size = (int)total;
		g_receive.tick = (int)tick;
		memset(g_worldStateDupBuffer, 0, total);
		if (!g_resync.owns_alert) {
			FlightAlert_SaveBoxBackground();
			g_resync.owns_alert = 1;
		}
		FlightAlert_DrawBox(1, g_strDiskIoMessages[DISK_IO_STR_COM_FAILURE_RECEIVING], 0x30);
		Aeron_LogInfo("xvt.network", "receiving world state from the host: %zu bytes, tick %u", total, tick);
		g_resync.phase = RESYNC_FULL_RECEIVE;
	}
	g_receive.deadline = Aeron_NowUs() + (uint64_t)XVT_PEER_TIMEOUT_TICKS * XVT_FLIGHT_TICK_US;
	XvtFlightEpochWire ready;
	XvtWire_Set32(ready.opcode, NET_PACKET_RESYNC_CHECKSUMS);
	XvtWire_Set32(ready.epoch, g_receive.epoch);
	XvtFlightNetwork_SendWire(NetSession_GetHostDplayId(), &ready, sizeof ready);
	return 1;
}

static int XvtResync_FullChunk(const uint8_t* bytes, unsigned size) {
	XvtFlightChunkHeader header;
	if (g_resync.phase != RESYNC_FULL_RECEIVE || size < sizeof header + sizeof(XvtWireU32))
		return 1;
	memcpy(&header, bytes, sizeof header);
	unsigned chunk = XvtWire_Get32(header.index);
	if (XvtWire_Get32(header.epoch) != g_receive.epoch || chunk >= XVT_RESYNC_CHUNKS_PER_BATCH)
		return 1;
	size_t offset = sizeof header;
	/* Validate the complete datagram before modifying the candidate image. */
	while (offset + sizeof(XvtWireU32) <= size && XvtWire_Get32(bytes + offset) != UINT32_MAX) {
		XvtFlightChunkSpan span;
		if (size - offset < sizeof span)
			return 1;
		memcpy(&span, bytes + offset, sizeof span);
		size_t destination = XvtWire_Get32(span.offset), count = XvtWire_Get32(span.bytes);
		if (!count || destination > (unsigned)g_receive.size ||
			count > (unsigned)g_receive.size - destination || count > size - offset - sizeof span)
			return 1;
		offset += sizeof span + count;
	}
	if (offset + sizeof(XvtWireU32) != size || XvtWire_Get32(bytes + offset) != UINT32_MAX)
		return 1;
	for (size_t cursor = sizeof header; cursor < offset;) {
		XvtFlightChunkSpan span;
		memcpy(&span, bytes + cursor, sizeof span);
		size_t destination = XvtWire_Get32(span.offset), count = XvtWire_Get32(span.bytes);
		memcpy(g_worldStateDupBuffer + destination, bytes + cursor + sizeof span, count);
		cursor += sizeof span + count;
	}
	XvtFlightChunkAckWire ack;
	XvtWire_Set32(ack.opcode, NET_PACKET_RESYNC_CHUNK_ACK);
	XvtWire_Set32(ack.index, chunk);
	XvtFlightNetwork_SendWire(NetSession_GetHostDplayId(), &ack, sizeof ack);
	g_receive.deadline = Aeron_NowUs() + (uint64_t)XVT_PEER_TIMEOUT_TICKS * XVT_FLIGHT_TICK_US;
	return 1;
}

static int XvtResync_FullApply(const uint8_t* bytes, unsigned size) {
	XvtFlightResyncApplyWire apply;
	if (g_resync.phase != RESYNC_FULL_RECEIVE || size != sizeof apply)
		return 1;
	memcpy(&apply, bytes, sizeof apply);
	if (XvtWire_Get32(apply.epoch) != g_receive.epoch ||
		XvtWire_Get32(apply.image_bytes) != (unsigned)g_receive.size)
		return 1;
	if (XvtFlightNetwork_NeedsRecovery()) {
		Aeron_LogWarn("xvt.network", "world state received while recovery is still required");
		g_receive.restart_pending = 1;
		return 1;
	}
	unsigned checksums[XVT_WORLD_CHECKSUM_REGIONS], lengths[XVT_WORLD_CHECKSUM_REGIONS];
	if (!XvtSnapshot_ChecksumImage(g_worldStateDupBuffer, g_receive.size, checksums, lengths) ||
		memcmp(checksums, g_receive.checksums, sizeof checksums) ||
		memcmp(lengths, g_receive.lengths, sizeof lengths)) {
		Aeron_LogWarn("xvt.network", "received world state does not match the host's checksum table");
		g_receive.restart_pending = 1;
		return 1;
	}
	if (!XvtSnapshot_Decode(g_worldStateDupBuffer, g_receive.size)) {
		Aeron_LogWarn("xvt.network", "received world state cannot be decoded");
		g_receive.restart_pending = 1;
		return 1;
	}
	XvtFlightHistory_Recover();
	memcpy(g_worldStateBuffer, g_worldStateDupBuffer, g_receive.size);
	g_worldStateSize = worldStateSize = g_receive.size;
	g_serverTickTime = g_receive.tick;
	g_flightNetWorldChecksumEpoch = g_receive.epoch;
	memcpy(g_worldChecksum, checksums, sizeof checksums);
	memcpy(g_peerChecksumRegionLengths, lengths, sizeof lengths);
	FlightNet_SendWorldChecksumToLocalPlayer(
		(const int*)g_worldChecksum, (const int*)g_peerChecksumRegionLengths, XVT_WORLD_CHECKSUM_REGIONS);
	XvtFlightFrame_ResetReplay();
	XvtFlightNetwork_Recovered();
	g_flightNetBufferWorldMessagesUntilChecksum = 0;
	Aeron_LogInfo("xvt.network", "world state applied at tick %d", g_serverTickTime);
	g_resync.phase = RESYNC_REPLAY;
	return 1;
}

int XvtResync_ReceivePacket(int sender, const uint8_t* bytes, unsigned size) {
	if (size < sizeof(XvtWireU32))
		return 0;
	unsigned opcode = XvtWire_Get32(bytes);
	if (opcode == NET_PACKET_WORLD_CHECKSUM && size == sizeof(XvtFlightChecksumReportWire)) {
		XvtFlightChecksumReportWire report;
		memcpy(&report, bytes, sizeof report);
		if (NetSession_GetLocalPlayerId() && g_resync.phase && sender == g_resync.peer_dpid) {
			if (XvtWire_Get32(report.request_state) == XVT_CHECKSUM_REQUEST_STATE) {
				int aligned[sizeof report / sizeof(int)];
				memcpy(aligned, &report, sizeof report);
				XvtResync_DeferChecksum(sender, aligned);
				g_resync.restart_requested = 1;
				return 1;
			}
			if (!XvtWire_Get32(report.request_state) && XvtWire_Get32(report.state.epoch) == g_resync.epoch &&
				g_resync.epoch != g_flightNetWorldChecksumEpoch)
				return 1;
		}
	}
	if (opcode == NET_PACKET_RESYNC_CHECKSUMS) {
		XvtFlightEpochWire ready;
		if (g_resync.phase == RESYNC_CHECKSUMS && sender == g_resync.peer_dpid && size == sizeof ready) {
			memcpy(&ready, bytes, sizeof ready);
			if (XvtWire_Get32(ready.epoch) == g_resync.epoch)
				g_flightNetRemoteResyncChecksumsReceivedFlag = 1;
		}
		return 1;
	}
	if (sender != NetSession_GetHostDplayId())
		return opcode == NET_PACKET_RESYNC_REQUEST || opcode == NET_PACKET_RESYNC_CHUNK ||
			   opcode == NET_PACKET_RESYNC_APPLY;
	if (opcode == NET_PACKET_SERVER_CHECKSUM && size == sizeof(XvtFlightChecksumWire)) {
		if (!NetSession_GetLocalPlayerId() && g_resync.phase != RESYNC_FULL_RECEIVE &&
			g_resync.phase != RESYNC_REPLAY) {
			XvtFlightChecksumWire table;
			memcpy(&table, bytes, sizeof table);
			g_receive.epoch = XvtWire_Get32(table.epoch);
			for (unsigned region = 0; region < XVT_WORLD_CHECKSUM_REGIONS; ++region) {
				g_receive.checksums[region] = XvtWire_Get32(table.checksums[region]);
				g_receive.lengths[region] = XvtWire_Get32(table.lengths[region]);
			}
			g_receive.table_valid = 1;
		}
		return g_resync.phase == RESYNC_FULL_RECEIVE || g_resync.phase == RESYNC_REPLAY;
	}
	if (opcode == NET_PACKET_RESYNC_REQUEST)
		return XvtResync_FullRequest(bytes, size);
	if (opcode == NET_PACKET_RESYNC_CHUNK)
		return XvtResync_FullChunk(bytes, size);
	if (opcode == NET_PACKET_RESYNC_APPLY)
		return XvtResync_FullApply(bytes, size);
	return 0;
}
