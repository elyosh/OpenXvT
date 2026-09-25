#include "xvt/net/flight_sync.h"
#ifdef XVT_MODERN
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/runtime/flight_network.h"
#include "xvt_runtime/runtime/flight_sim.h"
#include "xvt_runtime/runtime/resync_task.h"
#include "xvt_runtime/timing/flight_timing.h"
#endif

#include "xvt/audio/sound.h"
#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_view.h"
#include "xvt/flight/fview.h"
#include "xvt/flight/object/collide.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/math/math.h"
#include "xvt/net/flight_net.h"
#include "xvt/net/net_reliable.h"
#include "xvt/net/net_session.h"
#include "xvt/util/memory.h"
#include <string.h>

enum { INPUT_FRAME_PREDICTED = 2 };

// GLOBAL: XVT 0x9A8DB0
int g_inputFrameCount[8] = { 0 };
// GLOBAL: XVT 0x9ED670
InputFrame g_inputHistory[8][450] = { { { 0 } } };
// GLOBAL: XVT 0x51BF40
int g_flightNetDirtyAllObjectTransformsAfterRestore = 0;
#ifndef XVT_MODERN
// GLOBAL: XVT 0x51BF48
static int g_worldMessageBufferCapacity;
// GLOBAL: XVT 0x51BF4C
static int g_worldMessageBufferBytesFree;
// GLOBAL: XVT 0x51BF50
static int g_worldMessageBufferedCount;
// GLOBAL: XVT 0x51BF54
static uint16_t g_worldMessageBufferHandle = 0;
#endif
// GLOBAL: XVT 0x523430
int g_remotePlayerRenderSmoothingEnabled = 1;
// GLOBAL: XVT 0x550888
RemotePlayerRenderSample g_remotePlayerRenderSamples[8];
// GLOBAL: XVT 0x550A08
RemotePlayerSavedRenderPose g_remotePlayerSavedRenderPoses[8];
#ifndef XVT_MODERN
// GLOBAL: XVT 0x550B90
static uint8_t* g_worldMessageBuffer = NULL;
#endif

#ifndef XVT_MODERN
// FLAGS: /O2 /G5
// FUNCTION: XVT 0x418500
void FlightSync_QueuePredictedRemoteInputFrames(int predictedFrameDelta) {
	int playerIdx;
	FlightInputFrameRecord input;

	if (g_asyncFlag != 0) {
		return;
	}
	memset(&input, 0, sizeof(input));
	for (playerIdx = 0; playerIdx < 8; ++playerIdx) {
		int count;
		InputFrame* lastFrame;
		InputFrame* predictedFrame;

		if (g_players[playerIdx].connectedFlag == 0 || playerIdx == g_localPlayer) {
			continue;
		}
		count = g_inputFrameCount[playerIdx];
		if (count == 0) {
			continue;
		}
		lastFrame = &g_inputHistory[playerIdx][count - 1];
		input.axisX = lastFrame->input.axisX;
		input.axisY = lastFrame->input.axisY;
		predictedFrame =
			FlightSync_InsertInputFrame(playerIdx, lastFrame->timestamp + predictedFrameDelta, &input);
		if (predictedFrame != NULL) {
			predictedFrame->applied = 0;
			predictedFrame->valid = INPUT_FRAME_PREDICTED;
		}
	}
}
#endif

// FUNCTION: XVT 0x4185B0
void FlightSync_DiscardAllPredictedInputFrames(void) {
	int playerIndex;

#ifndef XVT_MODERN
	if (g_asyncFlag != 0)
		return;
#endif

	for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0 && playerIndex != g_localPlayer) {
			int frameIndex;
			InputFrame* frame;

			frame = g_inputHistory[playerIndex];
			frameIndex = 0;
			while (g_inputFrameCount[playerIndex] > frameIndex) {
				if (frame->applied == 0 && frame->valid == INPUT_FRAME_PREDICTED) {
					FlightSync_RemoveInputHistoryFrame(playerIndex, frame);
				} else {
					++frame;
					++frameIndex;
				}
			}
		}
	}
}

// FUNCTION: XVT 0x418650
void FlightSync_DiscardPredictedInputFrames(int playerIdx) {
	int frameIndex;
	InputFrame* frame;

	if (
#ifndef XVT_MODERN
		g_asyncFlag != 0 ||
#endif
		g_players[playerIdx].connectedFlag == 0 || playerIdx == g_localPlayer)
		return;

	frameIndex = 0;
	frame = g_inputHistory[playerIdx];
	while (frameIndex < g_inputFrameCount[playerIdx]) {
		if (frame->applied == 0 && frame->valid == INPUT_FRAME_PREDICTED) {
			FlightSync_RemoveInputHistoryFrame(playerIdx, frame);
		} else {
			++frame;
			++frameIndex;
		}
	}
}

// FUNCTION: XVT 0x4186E0
void FlightSync_RemoveInputHistoryFrame(int playerIdx, InputFrame* frame) {
	int frameCount;
	int copyIndex;
	InputFrame* current;

	frameCount = g_inputFrameCount[playerIdx];
	if (frameCount != 0) {
		current = g_inputHistory[playerIdx];
		if (current <= frame) {
			--frameCount;
			g_inputFrameCount[playerIdx] = frameCount;
			copyIndex = 0;
			while (copyIndex < g_inputFrameCount[playerIdx]) {
				if (current >= frame) {
					*current = current[1];
				}
				++copyIndex;
				++current;
			}
		}
	}
}

// FUNCTION: XVT 0x418760
InputFrame* FlightSync_InsertInputFrame(int playerIdx, int timestamp, const FlightInputFrameRecord* input) {
#ifdef XVT_MODERN
	InputFrame* inserted;
	XvtInputInsertStatus status = XvtFlightHistory_Insert((unsigned)playerIdx, timestamp, input, &inserted);
	if (status == XVT_INPUT_FULL && XvtFlightTiming_IsNetwork125())
		XvtFlightNetwork_RequestRecovery();
	return inserted;
#else

	InputFrame* arrayEnd;
	int existingTimestamp;
	int frameCount;
	int frameIndex;
	InputFrame* frame;

	frameIndex = 0;
	frameCount = g_inputFrameCount[playerIdx];
	frame = g_inputHistory[playerIdx];
	arrayEnd = &frame[frameCount];

	while (frameIndex < frameCount && frame->timestamp < timestamp) {
		++frameIndex;
		++frame;
	}
	existingTimestamp = frame->timestamp;
	if (existingTimestamp > timestamp || frameIndex == frameCount) {
		if (frameCount == 450) {
			return NULL;
		}
		g_inputFrameCount[playerIdx] = frameCount + 1;
		if (arrayEnd > frame) {
			frameIndex = frameCount - frameIndex;
			do {
				--frameIndex;
				frame[frameIndex + 1] = frame[frameIndex];
			} while (frameIndex != 0);
		}
	} else if (existingTimestamp == timestamp) {
		if (frame->valid == 0) {
			return NULL;
		}
		if (frame->applied == 1) {
			return NULL;
		}
	}
	frame->timestamp = timestamp;
	frame->valid = 1;
	frame->applied = 0;
	frame->input = *input;
	return frame;

#endif
}

// FUNCTION: XVT 0x418890
InputFrame* FlightSync_FindLastNonzeroInputFrame(int playerIdx) {
	InputFrame* frame;
	int frameCount;
	InputFrame* result;

	frame = g_inputHistory[playerIdx];
	frameCount = g_inputFrameCount[playerIdx];
	result = 0;
	while (frameCount > 0) {
		if (frame->applied != 0)
			result = frame;
		++frame;
		--frameCount;
	}
	return result;
}

// FUNCTION: XVT 0x418950
void FlightSync_ResetRemotePlayerRenderSmoothing(void) {
	int playerIndex;

	for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
		g_remotePlayerRenderSamples[playerIndex].valid = 0;
		g_remotePlayerSavedRenderPoses[playerIndex].valid = 0;
	}
}

// FUNCTION: XVT 0x418970
void FlightSync_CaptureRemotePlayerRenderSamples(void) {
	int playerIndex;

	if (g_remotePlayerRenderSmoothingEnabled == 0)
		return;

	playerIndex = 0;
	do {
		PlayerData* player = &g_players[playerIndex];
		int sampleWasValid = g_remotePlayerRenderSamples[playerIndex].valid;
		g_remotePlayerRenderSamples[playerIndex].valid = 0;
		if (g_players[playerIndex].connectedFlag != 0 && g_localPlayer != playerIndex &&
			player->objectIndex != -1) {
			ObjectRecord* object = &g_objectTable[player->objectIndex];
			if (object->objectType != 0 && object->mobj != NULL) {
				if (sampleWasValid == 0) {
					g_remotePlayerRenderSamples[playerIndex].roll = object->roll;
					g_remotePlayerRenderSamples[playerIndex].pitch = object->pitch;
					g_remotePlayerRenderSamples[playerIndex].yaw = object->yaw;
				}

				g_remotePlayerRenderSamples[playerIndex].valid = 1;
				g_remotePlayerRenderSamples[playerIndex].objectSignature = object->objectSignature;
				g_remotePlayerRenderSamples[playerIndex].worldX = object->world_x;
				g_remotePlayerRenderSamples[playerIndex].worldY = object->world_y;
				g_remotePlayerRenderSamples[playerIndex].worldZ = object->world_z;
				g_remotePlayerRenderSamples[playerIndex].rollDelta =
					object->roll - (uint16_t)g_remotePlayerRenderSamples[playerIndex].roll;
				g_remotePlayerRenderSamples[playerIndex].pitchDelta =
					object->pitch - (uint16_t)g_remotePlayerRenderSamples[playerIndex].pitch;
				g_remotePlayerRenderSamples[playerIndex].yawDelta =
					object->yaw - (uint16_t)g_remotePlayerRenderSamples[playerIndex].yaw;
				g_remotePlayerRenderSamples[playerIndex].roll = object->roll;
				g_remotePlayerRenderSamples[playerIndex].pitch = object->pitch;
				g_remotePlayerRenderSamples[playerIndex].yaw = object->yaw;

				if (object->mobj->moveVectorDirty != 0)
					FVIEW_calcrotatemove(object->pitch, object->yaw, object);
				g_remotePlayerRenderSamples[playerIndex].moveX = object->mobj->moveX;
				g_remotePlayerRenderSamples[playerIndex].moveY = object->mobj->moveY;
				g_remotePlayerRenderSamples[playerIndex].moveZ = object->mobj->moveZ;
				g_remotePlayerRenderSamples[playerIndex].speedMagnitude = object->mobj->speed;
				g_remotePlayerRenderSamples[playerIndex].simStateTimestamp = object->mobj->simStateTimestamp;

				if (g_remotePlayerSavedRenderPoses[playerIndex].valid != 0) {
					object->roll = g_remotePlayerSavedRenderPoses[playerIndex].roll;
					object->pitch = g_remotePlayerSavedRenderPoses[playerIndex].pitch;
					object->yaw = g_remotePlayerSavedRenderPoses[playerIndex].yaw;
					object->world_x = g_remotePlayerSavedRenderPoses[playerIndex].worldX;
					object->world_y = g_remotePlayerSavedRenderPoses[playerIndex].worldY;
					object->world_z = g_remotePlayerSavedRenderPoses[playerIndex].worldZ;
				}
			}
		}
		++playerIndex;
	} while (playerIndex < 8);
}

// FUNCTION: XVT 0x418B70
void FlightSync_ApplyRemotePlayerRenderSmoothing(void) {
	int playerIndex;
	ObjectRecord* object;
	int predictedWorldX;
	int predictedWorldY;
	int predictedWorldZ;
	int positionDeltaX;
	int positionDeltaY;
	int positionDeltaZ;
	int elapsedTime;
	int predictionDistance;
	int roughDistance;
	int positionBlend;
	int maxAngleChange;
	int angleDifference;
	int signedAngleDifference;
	int candidateAngle;
	int candidateDifference;
	int blendedX;
	int blendedY;
	int blendedZ;

	if (g_remotePlayerRenderSmoothingEnabled == 0)
		return;

	for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
		g_remotePlayerSavedRenderPoses[playerIndex].valid = 0;
		if (g_players[playerIndex].connectedFlag == 0 || g_players[playerIndex].objectIndex == -1)
			continue;

		object = &g_objectTable[g_players[playerIndex].objectIndex];
		if (object->objectType == 0 || object->mobj == NULL ||
			g_remotePlayerRenderSamples[playerIndex].valid == 0 || playerIndex == g_localPlayer ||
			g_remotePlayerRenderSamples[playerIndex].objectSignature !=
				g_players[playerIndex].boundObjectSignature) {
			continue;
		}

		g_remotePlayerSavedRenderPoses[playerIndex].roll = object->roll;
		g_remotePlayerSavedRenderPoses[playerIndex].pitch = object->pitch;
		g_remotePlayerSavedRenderPoses[playerIndex].yaw = object->yaw;
		g_remotePlayerSavedRenderPoses[playerIndex].worldX = object->world_x;
		g_remotePlayerSavedRenderPoses[playerIndex].worldY = object->world_y;
		g_remotePlayerSavedRenderPoses[playerIndex].worldZ = object->world_z;
		g_remotePlayerSavedRenderPoses[playerIndex].valid = 1;

		predictedWorldX = g_remotePlayerRenderSamples[playerIndex].worldX;
		predictedWorldY = g_remotePlayerRenderSamples[playerIndex].worldY;
		predictedWorldZ = g_remotePlayerRenderSamples[playerIndex].worldZ;

		elapsedTime =
			object->mobj->simStateTimestamp - g_remotePlayerRenderSamples[playerIndex].simStateTimestamp;
		if (elapsedTime < 0)
			continue;

		predictionDistance = 0;
		if (elapsedTime > 0 && g_remotePlayerRenderSamples[playerIndex].speedMagnitude != 0) {
			predictionDistance =
				elapsedTime * ((4660 * g_remotePlayerRenderSamples[playerIndex].speedMagnitude + 128) >> 8) /
				SIMULATION_TICKS_PER_SECOND;
			predictedWorldX +=
				Math_MulQ15(g_remotePlayerRenderSamples[playerIndex].moveX, predictionDistance);
			predictedWorldY +=
				Math_MulQ15(g_remotePlayerRenderSamples[playerIndex].moveY, predictionDistance);
			predictedWorldZ +=
				Math_MulQ15(g_remotePlayerRenderSamples[playerIndex].moveZ, predictionDistance);
		}

		positionDeltaX = object->world_x - predictedWorldX;
		positionDeltaY = object->world_y - predictedWorldY;
		positionDeltaZ = object->world_z - predictedWorldZ;
		roughDistance = collide_roughdistance3d(positionDeltaX, positionDeltaY, positionDeltaZ);
		predictionDistance *= 32;
		if (predictionDistance >= roughDistance && roughDistance != 0)
			positionBlend = (roughDistance << 14) / predictionDistance;
		else
			positionBlend = 0x4000;
		blendedX = Math_MulQ15(positionBlend, positionDeltaX);
		blendedY = Math_MulQ15(positionBlend, positionDeltaY);
		blendedZ = Math_MulQ15(positionBlend, positionDeltaZ);
		predictedWorldX += blendedX;
		predictedWorldY += blendedY;
		predictedWorldZ += blendedZ;
		object->world_x = predictedWorldX;
		object->world_y = predictedWorldY;
		object->world_z = predictedWorldZ;

		angleDifference = (int16_t)(object->roll - g_remotePlayerRenderSamples[playerIndex].roll);
		signedAngleDifference = angleDifference;
		if (g_remotePlayerRenderSamples[playerIndex].rollDelta > 0) {
			if (angleDifference < 0) {
				object->roll = g_remotePlayerRenderSamples[playerIndex].roll;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		} else if (g_remotePlayerRenderSamples[playerIndex].rollDelta < 0) {
			if (signedAngleDifference > 0) {
				object->roll = g_remotePlayerRenderSamples[playerIndex].roll;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		}
		if (angleDifference < 0)
			angleDifference = -angleDifference;
		maxAngleChange = 6144 * elapsedTime / SIMULATION_TICKS_PER_SECOND;
		if (angleDifference > maxAngleChange) {
			candidateAngle = g_remotePlayerRenderSamples[playerIndex].roll + signedAngleDifference;
			candidateDifference = object->roll - candidateAngle;
			if (candidateDifference < 0)
				candidateDifference = -candidateDifference;
			if (8 * maxAngleChange > candidateDifference)
				object->roll = candidateAngle;
		}

		angleDifference = (int16_t)(object->pitch - g_remotePlayerRenderSamples[playerIndex].pitch);
		signedAngleDifference = angleDifference;
		if (g_remotePlayerRenderSamples[playerIndex].pitchDelta > 0) {
			if (angleDifference < 0) {
				object->pitch = g_remotePlayerRenderSamples[playerIndex].pitch;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		} else if (g_remotePlayerRenderSamples[playerIndex].pitchDelta < 0) {
			if (signedAngleDifference > 0) {
				object->pitch = g_remotePlayerRenderSamples[playerIndex].pitch;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		}
		if (angleDifference < 0)
			angleDifference = -angleDifference;
		if (angleDifference > maxAngleChange) {
			candidateAngle = g_remotePlayerRenderSamples[playerIndex].pitch + signedAngleDifference;
			candidateDifference = object->pitch - candidateAngle;
			if (candidateDifference < 0)
				candidateDifference = -candidateDifference;
			if (8 * maxAngleChange > candidateDifference)
				object->pitch = candidateAngle;
		}

		angleDifference = (int16_t)(object->yaw - g_remotePlayerRenderSamples[playerIndex].yaw);
		signedAngleDifference = angleDifference;
		if (g_remotePlayerRenderSamples[playerIndex].yawDelta > 0) {
			if (angleDifference < 0) {
				object->yaw = g_remotePlayerRenderSamples[playerIndex].yaw;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		} else if (g_remotePlayerRenderSamples[playerIndex].yawDelta < 0) {
			if (signedAngleDifference > 0) {
				object->yaw = g_remotePlayerRenderSamples[playerIndex].yaw;
				angleDifference = 0;
				signedAngleDifference = 0;
			}
		}
		if (angleDifference < 0)
			angleDifference = -angleDifference;
		if (angleDifference > maxAngleChange) {
			candidateAngle = g_remotePlayerRenderSamples[playerIndex].yaw + signedAngleDifference;
			candidateDifference = object->yaw - candidateAngle;
			if (candidateDifference < 0)
				candidateDifference = -candidateDifference;
			if (8 * maxAngleChange > candidateDifference)
				object->yaw = candidateAngle;
		}
	}
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x418F80
void FlightSync_ApplyWorldMessagePacket(uint8_t* packet) {
	enum {
		PLAYER_SLOT_COUNT = 8,
		FULL_TIMESTAMP_CODE = 127,
		SHORT_DELTA_CODE = 126,
		BYTE_DELTA_CODE = 125,
		KEY_PRESENT_FLAG = 0x80,
		DELTA_CODE_MASK = 0x7F,
		WORLD_CHECKSUM_FLAG = INT32_MIN,
		WORLD_TIMESTAMP_MASK = 0x7FFFFFFFu
	};

	uint32_t rawPacketTick;
	int objectIndex;
	int playerIndex;
	int packetTick;
	int checksumRequested;
	FlightInputFrameRecord input;

	if (NetSession_GetLocalPlayerId() == 0 && g_flightNetBufferWorldMessagesUntilChecksum == 1) {
		FlightSync_BufferWorldMessagePacket(packet);
	}

	rawPacketTick = ((const uint32_t*)packet)[1];
	packet += 2 * sizeof(int);
	packetTick = (int)(rawPacketTick & WORLD_TIMESTAMP_MASK);
	checksumRequested = (int)(rawPacketTick & WORLD_CHECKSUM_FLAG);
	if (packetTick <= g_serverTickTime) {
		return;
	}

	if (packetTick - dtMs != g_serverTickTime) {
		NetReliable_ResetRecvQueueState();
	}
	FlightSync_DiscardAllPredictedInputFrames();
	Flight_RestoreWorldState();
	g_gameTime = g_serverTickTime;

	if (g_flightNetDirtyAllObjectTransformsAfterRestore != 0) {
		for (objectIndex = 0; objectIndex < g_regionMainObjectSlotEnd; ++objectIndex) {
			if (g_objectTable[objectIndex].objectType != 0 && g_objectTable[objectIndex].mobj != NULL) {
				g_objectTable[objectIndex].mobj->moveVectorDirty = 1;
				g_objectTable[objectIndex].mobj->orientMatrixDirty = 1;
			}
		}
		g_flightNetDirtyAllObjectTransformsAfterRestore = 0;
	}

	{
		uint8_t* cursor;
		int remainingPlayerBlocks;

		cursor = packet;
		remainingPlayerBlocks = *cursor++;
		for (playerIndex = 0; playerIndex < PLAYER_SLOT_COUNT; ++playerIndex) {
			int frameCount;

			if (g_players[playerIndex].connectedFlag == 0) {
				continue;
			}
			if (remainingPlayerBlocks == 0) {
				break;
			}

			--remainingPlayerBlocks;
			frameCount = *cursor++;
			while (frameCount > 0) {
				InputFrame* inserted;
				int timestampCode;
				int deltaCode;
				int timestamp;

				timestampCode = *cursor++;
				deltaCode = timestampCode & DELTA_CODE_MASK;
				if (deltaCode == FULL_TIMESTAMP_CODE) {
					timestamp = *(const int*)cursor;
					cursor += sizeof(timestamp);
				} else if (deltaCode == SHORT_DELTA_CODE) {
					timestamp = packetTick - *(const uint16_t*)cursor;
					cursor += sizeof(uint16_t);
				} else {
					timestamp = packetTick;
					if (deltaCode == BYTE_DELTA_CODE) {
						timestamp -= *cursor++;
					}
					timestamp -= deltaCode;
				}

				if ((timestampCode & KEY_PRESENT_FLAG) != 0) {
					input.key = *cursor++;
				} else {
					input.key = 0;
				}
				input.axisX = (int8_t)(cursor[0] & (uint8_t)~1u);
				input.axisY = (int8_t)(cursor[1] & (uint8_t)~1u);
				input.keyMods = cursor[1] & 1u;
				input.keyMods = (uint8_t)(input.keyMods << 1);
				input.keyMods |= cursor[0] & 1u;
				cursor += 2;

				inserted = FlightSync_InsertInputFrame(playerIndex, timestamp, &input);
				if (inserted != NULL) {
					inserted->valid = 0;
					inserted->applied = 0;
				}
				--frameCount;
			}
		}
	}

	g_flightSimSideEffectsSuppressed = 0;
	Flight_StepSimToTime(packetTick);
	for (playerIndex = 0; playerIndex < PLAYER_SLOT_COUNT; ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0 && playerIndex != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIndex);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);
	g_gameTime = packetTick;
	g_serverTickTime = packetTick;
	Sound_FlushQueuedEffects();
	Flight_SaveWorldState();

	if (checksumRequested != 0) {
		int checksumDwordCount = (int)(sizeof(g_worldChecksum) / sizeof(g_worldChecksum[0]));

		Flight_ChecksumWorldState(0, 0);
		g_flightNetWorldChecksumEpoch = (unsigned int)g_serverTickTime;
		if (NetSession_GetLocalPlayerId() != 0) {
			FlightNet_BroadcastWorldChecksum((const int*)g_worldChecksum,
											 (const int*)g_peerChecksumRegionLengths, checksumDwordCount);
		}
		FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum,
												 (const int*)g_peerChecksumRegionLengths, checksumDwordCount);
		g_flightNetBufferWorldMessagesUntilChecksum = 1;
		FlightSync_SnapshotWorldStateForReplay();
		FlightSync_ResetWorldMessageBufferCursor();
	}
}
#endif

// FUNCTION: XVT 0x4193C0
void FlightSync_HandleWorldChecksumPacket(int senderDpid, const int* packet) {
	enum {
		PACKET_EPOCH_INDEX = 1,
		PACKET_CHECKSUM_INDEX = 2,
		CHECKSUM_REGION_COUNT = 16,
		PACKET_REGION_LENGTH_INDEX = PACKET_CHECKSUM_INDEX + CHECKSUM_REGION_COUNT,
		PEER_STATUS_PENDING = 2,
		PEER_STATUS_MATCHED = 1,
		ALL_PEER_STATUS_BITS = 3
	};

	int allPeerStatus;
	int checksumMismatch;
	int localWorldStateSize;
	int playerIndex;
	int remoteWorldStateSize;
	int senderPlayerIndex;

	if ((unsigned int)packet[PACKET_EPOCH_INDEX] != g_flightNetWorldChecksumEpoch
#ifdef XVT_MODERN
		&& packet[34] != XVT_CHECKSUM_REQUEST_STATE
#endif
	) {
		return;
	}

	senderPlayerIndex = NetSession_FindPlayerSlotByDpid(senderDpid);
	checksumMismatch = 0;
#ifdef XVT_MODERN
	if ((unsigned)senderPlayerIndex >= 8)
		return;
	if ((unsigned)packet[34] > 1)
		return;
	if (packet[34] == 1 && NetSession_GetLocalPlayerId() && senderPlayerIndex != g_localPlayer) {
		XvtResync_BeginSend(senderDpid, g_worldStateBuffer, g_worldStateSize);
		return;
	}
#endif
	if (g_playerAbortFlags[senderPlayerIndex] != 0 || g_players[senderPlayerIndex].connectedFlag == 0) {
		return;
	}

	if (g_localPlayer != senderPlayerIndex) {
		const int* remoteChecksums = &packet[PACKET_CHECKSUM_INDEX];
		const int* remoteRegionLengths = &packet[PACKET_REGION_LENGTH_INDEX];

		localWorldStateSize = 0;
		remoteWorldStateSize = 0;
		for (playerIndex = 0; playerIndex < CHECKSUM_REGION_COUNT; ++playerIndex) {
			remoteWorldStateSize += remoteRegionLengths[playerIndex];
			localWorldStateSize += (int)g_peerChecksumRegionLengths[playerIndex];
			if (g_worldChecksum[playerIndex] != (unsigned int)remoteChecksums[playerIndex]) {
				checksumMismatch = 1;
			}
		}
		(void)localWorldStateSize;
		(void)remoteWorldStateSize;

		if (checksumMismatch != 0) {
#ifdef XVT_MODERN
			XvtResync_BeginSend(senderDpid, g_worldStateDupBuffer, worldStateSize);
			return;
#else
			if (FlightNet_SendWorldStateResyncToPlayer(senderDpid, g_worldStateDupBuffer, worldStateSize) !=
				0) {
				FlightNet_SendWorldStateResyncApplyRequest(senderDpid, worldStateSize);
			}
			checksumMismatch = 1;
#endif
		}
	}

	if (NetSession_GetLocalPlayerId() == 0) {
		return;
	}

	g_flightNetWorldChecksumPeerStatus[senderPlayerIndex] = PEER_STATUS_PENDING;
	if (checksumMismatch != 1) {
		g_flightNetWorldChecksumPeerStatus[senderPlayerIndex] = PEER_STATUS_MATCHED;
	}

	allPeerStatus = ALL_PEER_STATUS_BITS;
	for (playerIndex = 0; playerIndex < (int)(sizeof(g_players) / sizeof(g_players[0])); ++playerIndex) {
		if (g_players[playerIndex].connectedFlag != 0) {
			allPeerStatus &= g_flightNetWorldChecksumPeerStatus[playerIndex];
		}
	}
	if ((allPeerStatus & PEER_STATUS_MATCHED) != 0) {
		g_flightNetBufferWorldMessagesUntilChecksum = 0;
	}
}

// FUNCTION: XVT 0x419510
void FlightSync_HandleServerChecksumPacket(uint8_t* packet) {
	uint32_t* packetChecksum;
	unsigned int* localChecksum;
	int checksumMismatch;

	if (NetSession_GetLocalPlayerId() != 0 || ((uint32_t*)packet)[1] != g_flightNetWorldChecksumEpoch) {
		return;
	}

	packetChecksum = (uint32_t*)packet + 2;
	checksumMismatch = 0;
	localChecksum = g_worldChecksum;
	do {
		if (*localChecksum != *packetChecksum) {
			checksumMismatch = 1;
		}
		++localChecksum;
		++packetChecksum;
	} while (localChecksum < g_worldChecksum + 16);

	if (checksumMismatch == 0) {
		g_flightNetBufferWorldMessagesUntilChecksum = 0;
		FlightSync_ResetWorldMessageBufferCursor();
	}
}

// FUNCTION: XVT 0x419570
void FlightSync_CopyWorldStateResyncChunk(const void* src, int offset, unsigned int size) {
	memcpy(&g_worldStateDupBuffer[offset], src, size);
}

#ifndef XVT_MODERN
#pragma intrinsic(memcpy)
#endif

#ifndef XVT_MODERN
// FUNCTION: XVT 0x4195A0
void FlightSync_ReplayResyncMessages(unsigned int worldStateBytes, int serverTickTime) {
	int checksumDwordCount;

	worldStateSize = (int)worldStateBytes;
	g_flightNetDirtyAllObjectTransformsAfterRestore = 1;
	memcpy(g_worldStateBuffer, g_worldStateDupBuffer, worldStateBytes);
	g_worldStateSize = (unsigned int)worldStateSize;
	g_serverTickTime = serverTickTime;
	Flight_ChecksumWorldState(0, 0);
	checksumDwordCount = (int)(sizeof(g_worldChecksum) / sizeof(g_worldChecksum[0]));
	FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum,
											 (const int*)g_peerChecksumRegionLengths, checksumDwordCount);
	FlightSync_SnapshotWorldStateForReplay();
	g_flightNetBufferWorldMessagesUntilChecksum = 0;
	FlightSync_ReplayBufferedWorldMessages();
}
#endif

// FUNCTION: XVT 0x419620
void FlightSync_SnapshotWorldStateForReplay(void) {
	unsigned int snapshotBytes;

	snapshotBytes = g_worldStateSize;
	memcpy(g_worldStateDupBuffer, g_worldStateBuffer, snapshotBytes);
	worldStateSize = (int)snapshotBytes;
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x419650
void FlightSync_BufferWorldMessagePacket(uint8_t* packet) {
	uint16_t oldHandle;
	int packetSize;
	uint8_t* packetStart;
	int groupCount;

	packetSize = 9;
	packetStart = packet;
	groupCount = packet[8];
	packet += 8;
	++packet;
	if (groupCount > 0) {
		do {
			int entryCount;

			entryCount = *packet++;
			++packetSize;
			if (entryCount > 0) {
				do {
					int flags;

					flags = *packet++;
					++packetSize;
					if ((flags & 0x7F) == 0x7F) {
						packet += 4;
						packetSize += 4;
					}
					if ((flags & 0x80) != 0) {
						++packet;
						++packetSize;
					}
					packet += 2;
					packetSize += 2;
					--entryCount;
				} while (entryCount != 0);
			}
			--groupCount;
		} while (groupCount != 0);
	}

	if (g_worldMessageBufferBytesFree < packetSize) {
		unsigned int oldCapacity;
		int growth;
		uint8_t* oldBuffer;

		oldHandle = g_worldMessageBufferHandle;
		oldCapacity = g_worldMessageBufferCapacity;
		growth = 100 * packetSize;
		g_worldMessageBufferBytesFree += growth;
		g_worldMessageBufferCapacity += growth;
		g_worldMessageBufferHandle = Memory_AllocHandle(g_worldMessageBufferCapacity, 0);
		if (g_worldMessageBufferHandle == 0) {
			FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
		}
		g_worldMessageBuffer = Memory_LockHandle(g_worldMessageBufferHandle);
		if (oldHandle != 0) {
			oldBuffer = Memory_LockHandle(oldHandle);
			memcpy(g_worldMessageBuffer, oldBuffer, oldCapacity);
			Memory_UnlockHandle(oldHandle);
			Memory_FreeHandle(oldHandle);
		}
	}

	memcpy(&g_worldMessageBuffer[g_worldMessageBufferCapacity - g_worldMessageBufferBytesFree], packetStart,
		   packetSize);
	g_worldMessageBufferBytesFree -= packetSize;
	++g_worldMessageBufferedCount;
}
#endif

// FUNCTION: XVT 0x4197B0
void FlightSync_ResetWorldMessageBufferCursor(void) {
#ifdef XVT_MODERN
	XvtFlightMessages_Clear(XVT_QUEUE_REPLAY);
#else
	g_worldMessageBufferedCount = 0;
	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
#endif
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x4197D0
void FlightSync_ReplayBufferedWorldMessages(void) {
	enum {
		PACKET_PLAYER_COUNT_OFFSET = 2 * sizeof(int),
		FULL_TIMESTAMP_CODE = 0x7F,
		KEY_PRESENT_FLAG = 0x80,
		DELTA_CODE_MASK = 0x7F,
		INPUT_AXIS_BYTES = 2
	};

	int packetOffset;

	packetOffset = 0;
	while (g_worldMessageBufferedCount != 0) {
		uint8_t* cursor;
		uint8_t* packet;
		int playerSectionsRemaining;

		--g_worldMessageBufferedCount;
		packet = &g_worldMessageBuffer[packetOffset];
		packetOffset += PACKET_PLAYER_COUNT_OFFSET;
		cursor = packet + PACKET_PLAYER_COUNT_OFFSET;
		++packetOffset;
		playerSectionsRemaining = *cursor++;
		while (playerSectionsRemaining > 0) {
			int frameHeader;
			int framesRemaining;

			framesRemaining = *cursor++;
			++packetOffset;
			while (framesRemaining > 0) {
				frameHeader = *cursor++;
				++packetOffset;
				if ((frameHeader & DELTA_CODE_MASK) == FULL_TIMESTAMP_CODE) {
					cursor += sizeof(uint32_t);
					packetOffset += sizeof(uint32_t);
				}
				if ((frameHeader & KEY_PRESENT_FLAG) != 0) {
					++cursor;
					++packetOffset;
				}
				cursor += INPUT_AXIS_BYTES;
				packetOffset += INPUT_AXIS_BYTES;
				--framesRemaining;
			}
			--playerSectionsRemaining;
		}

		((uint32_t*)packet)[1] &= INT32_MAX;
		FlightSync_ApplyWorldMessagePacket(packet);
	}
	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
}
#endif

// FUNCTION: XVT 0x419870
int FlightSync_UnusedFourArgForwarder(int arg1, int arg2, int arg3, int arg4) {
	return Sound_UnusedFourArgStub(arg1, arg2, arg3, arg4);
}
