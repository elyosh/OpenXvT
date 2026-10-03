#ifndef XVT_RUNTIME_FLIGHT_PROTOCOL_H
#define XVT_RUNTIME_FLIGHT_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

enum {
	XVT_FLIGHT_PLAYERS = 8,
	XVT_INPUT_HISTORY_CAPACITY = 450,
	XVT_FLIGHT_PACKET_BYTES = 508,
	XVT_WORLD_RECORDS = XVT_FLIGHT_PLAYERS * XVT_INPUT_HISTORY_CAPACITY,
	XVT_WORLD_PART_RECORDS = 44,
	XVT_WORLD_PARTS = (XVT_WORLD_RECORDS + XVT_WORLD_PART_RECORDS - 1) / XVT_WORLD_PART_RECORDS,
	XVT_INPUT_BATCH_RECORDS = 16,
	XVT_INPUT_BATCHES_PER_ITERATION = 4,
	XVT_INPUT_STAGED_RECORDS = XVT_INPUT_BATCH_RECORDS * XVT_INPUT_BATCHES_PER_ITERATION,
	XVT_WORLD_PARTS_PER_ITERATION = 4,
	XVT_NETWORK_PACKETS_PER_ITERATION = 64,
	XVT_WORLD_LATE_INTERVALS = 5,
	XVT_WORLD_START_LEAD_SHIFT = 3,
	XVT_INPUT_AUTHORITATIVE = 0,
	XVT_INPUT_REAL = 1,
	XVT_INPUT_PREDICTED = 2,
	XVT_NETWORK_STEP_TICKS = 2,
	XVT_NATIVE_STEP_TICKS = 8,
	XVT_OFFLINE_STEP_TICKS = 1,
	XVT_COMPONENT_EVENT_TICKS = 32,
	XVT_COMPONENT_TIMER_TICKS = 29,
	XVT_REFERENCE_TICKS = 8,
	XVT_INPUT_BATCH_TICKS = 4,
	XVT_WORLD_MESSAGE_TICKS = 8,
	/* Clients lead confirmed time by their round trip plus this margin. */
	XVT_CLOCK_PROBE_BIAS_TICKS = 20,
	/* Clients' inputs reach the host about the probe margin ahead of confirmed time.
	 * Staying just inside it keeps the host from predicting client controls, while
	 * its own inputs reach clients as early as that allows. */
	XVT_HOST_CLOCK_LEAD_TICKS = XVT_CLOCK_PROBE_BIAS_TICKS - 4,
	XVT_PREDICTION_LEAD_TICKS = 256,
	XVT_SIM_STEPS_PER_ITERATION = 16,
	XVT_SIM_BUDGET_US = 2000,
	XVT_FLIGHT_TICK_US = 4000,
	XVT_WORLD_CHECKSUM_TICKS = 472,
	XVT_WORLD_CHECKSUM_REGIONS = 16,
	XVT_TIMING_CHECKSUM_REGION = XVT_WORLD_CHECKSUM_REGIONS - 1,
	XVT_RESYNC_CHUNKS_PER_BATCH = 16,
	XVT_RESYNC_CHUNK_MIN_FREE = 32,
	XVT_RESYNC_RETRIES = 10,
	XVT_RESYNC_ACK_RETRIES = 20,
	XVT_RESYNC_RETRY_TICKS = 236,
	XVT_RESYNC_REPLAY_RESTARTS = 1,
	XVT_PEER_TIMEOUT_TICKS = 7080,
	XVT_PENDING_MESSAGES = 128,
	XVT_PENDING_BYTES = 256 * 1024,
	XVT_REPLAY_MESSAGES = 512,
	XVT_REPLAY_BYTES = 1024 * 1024,
	XVT_DEFERRED_CHECKSUMS = 256,
	XVT_TIMING_SCHEMA = 5,
	XVT_WIRE_PROFILE_NETWORK_125 = 1,
	XVT_CHECKSUM_REPORT = 0,
	XVT_CHECKSUM_REQUEST_STATE = 1,
};

static const uint32_t XVT_WORLD_CHECKSUM_FLAG = UINT32_C(0x80000000);

/* Byte-array scalars keep wire layouts endian-independent and naturally byte aligned. */
typedef uint8_t XvtWireU16[2];
typedef uint8_t XvtWireU32[4];
typedef uint8_t XvtWireU64[8];

typedef struct XvtFlightInputWire {
	XvtWireU32 tick;
	uint8_t key, axes[3];
	XvtWireU16 throttle;
} XvtFlightInputWire;

typedef struct XvtFlightWorldInputWire {
	uint8_t player;
	XvtFlightInputWire input;
} XvtFlightWorldInputWire;

typedef struct XvtFlightBatchHeader {
	XvtWireU32 opcode, cookie;
	XvtWireU16 count, reserved;
} XvtFlightBatchHeader;

typedef struct XvtFlightWorldHeader {
	XvtWireU32 opcode, target_flags, cookie;
	uint8_t part_index, part_count, record_count, participant_mask;
} XvtFlightWorldHeader;

typedef struct XvtFlightAgreementWire {
	XvtWireU32 schema, profile, cookie;
} XvtFlightAgreementWire;

typedef struct XvtFlightOptionsWire {
	XvtWireU32 opcode, resolution, rating, schema;
} XvtFlightOptionsWire;

typedef struct XvtFlightRosterHeader {
	XvtWireU32 opcode, new_net;
} XvtFlightRosterHeader;

typedef struct XvtFlightRosterPlayerWire {
	XvtWireU32 resolution, rating;
} XvtFlightRosterPlayerWire;

typedef struct XvtFlightSlotWire {
	XvtWireU32 opcode, player;
} XvtFlightSlotWire;

typedef struct XvtFlightEpochWire {
	XvtWireU32 opcode, epoch;
} XvtFlightEpochWire;

typedef struct XvtFlightClockProbeWire {
	XvtWireU32 opcode, timestamp, lead;
} XvtFlightClockProbeWire;

typedef struct XvtFlightChecksumWire {
	XvtWireU32 opcode, epoch;
	XvtWireU32 checksums[XVT_WORLD_CHECKSUM_REGIONS], lengths[XVT_WORLD_CHECKSUM_REGIONS];
} XvtFlightChecksumWire;

typedef struct XvtFlightChecksumReportWire {
	XvtFlightChecksumWire state;
	XvtWireU32 request_state;
} XvtFlightChecksumReportWire;

typedef struct XvtFlightResyncRequestWire {
	XvtWireU32 opcode, epoch, image_bytes, completed_tick;
} XvtFlightResyncRequestWire;

typedef struct XvtFlightResyncApplyWire {
	XvtWireU32 opcode, epoch, image_bytes, input_tick;
} XvtFlightResyncApplyWire;

typedef struct XvtFlightChunkHeader {
	XvtWireU32 opcode, epoch, index;
} XvtFlightChunkHeader;

typedef struct XvtFlightChunkSpan {
	XvtWireU32 offset, bytes;
} XvtFlightChunkSpan;

typedef struct XvtFlightChunkAckWire {
	XvtWireU32 opcode, index;
} XvtFlightChunkAckWire;

typedef char XvtFlightInputWire_layout[(sizeof(XvtFlightInputWire) == 10) ? 1 : -1];
typedef char XvtFlightWorldInputWire_layout[(sizeof(XvtFlightWorldInputWire) == 11) ? 1 : -1];
typedef char XvtFlightWorldHeader_layout[(sizeof(XvtFlightWorldHeader) == 16) ? 1 : -1];
#endif
