#include "xvt/net/net_reliable.h"
#ifdef XVT_MODERN
#include "aeron/aeron.h"
#endif

#include "xvt/net/net_session.h"
#include "xvt/util/time.h"
#include <stddef.h>
#include <string.h>

// GLOBAL: XVT 0x527024
int g_netRecvQueueWriteIndex = 0;
// GLOBAL: XVT 0x527028
int g_netRecvQueueReadIndex;
// GLOBAL: XVT 0x52702C
unsigned int g_netRecvQueueCount;
// GLOBAL: XVT 0x5599A8
NetQueuedPacket g_netSessionRecvQueue[1024];
// GLOBAL: XVT 0x60F1BC
int g_netLastDeliveredRecvSequence;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x46F650
int NetReliable_GetLastDeliveredRecvSequence(void) { return g_netLastDeliveredRecvSequence; }

// FUNCTION: XVT 0x46FC00
int NetReliable_IsDuplicateRecvSequence(int directPlayId, int sequence, int channelA, int channelB) {
	uint32_t savedSlotCount;
	unsigned int slot;
	int recvSequence;
	int delta;

	savedSlotCount = g_netSession.reliablePeerSlotCount;
	slot = NetReliable_FindOrCreatePeerSlot(directPlayId);
	if (savedSlotCount != g_netSession.reliablePeerSlotCount || slot >= 40)
		return 0;

	if (channelA)
		recvSequence = g_netSession.reliablePeerSlots[slot].recvSeqChannelA;
	else if (channelB)
		recvSequence = g_netSession.reliablePeerSlots[slot].recvSeqChannelB;
	else
		recvSequence = g_netSession.reliablePeerSlots[slot].recvSeqDefault;

	delta = sequence - recvSequence;
	if (delta >= -64 && (delta <= 0 || delta >= 64))
		return 1;

	if (channelA)
		g_netSession.reliablePeerSlots[slot].recvSeqChannelA = sequence;
	else if (channelB)
		g_netSession.reliablePeerSlots[slot].recvSeqChannelB = sequence;
	else
		g_netSession.reliablePeerSlots[slot].recvSeqDefault = sequence;
	return 0;
}

/* Finds a queued reliable receive packet matching sequence, channel, and peer
 * filters. Returns the ring index or -1. */
// FUNCTION: XVT 0x46FCE0
int NetReliable_FindQueuedRecvPacket(int unused, int remoteSeq, int wantType0, int wantType2, int peerSlot) {
	unsigned int i;
	int index;
	int sequenceByte;
	int isClass0;
	int isClass2;
	unsigned int slot;
	DPID directPlayId;
	NetReliablePeerSlot* peer;

	(void)unused;

	index = g_netRecvQueueReadIndex;
	for (i = g_netRecvQueueCount; i != 0; --i) {
		if (g_netSessionRecvQueue[index].queuedFlag != 0) {
			directPlayId = g_netSessionRecvQueue[index].directPlayId;
			sequenceByte = g_netSessionRecvQueue[index].sequenceByte;
			isClass0 = g_netSessionRecvQueue[index].packetClass == 0;
			isClass2 = g_netSessionRecvQueue[index].packetClass == 2;
			slot = 0;
			if (g_netSession.reliablePeerSlotCount != 0) {
				peer = g_netSession.reliablePeerSlots;
				while (peer->directPlayId != directPlayId) {
					++peer;
					++slot;
					if (slot >= g_netSession.reliablePeerSlotCount)
						break;
				}
			}
			if (slot == (unsigned int)peerSlot) {
				if (wantType0 != 0) {
					if (isClass0 && sequenceByte == remoteSeq)
						return index;
				} else if (wantType2 != 0) {
					if (isClass2 && sequenceByte == remoteSeq)
						return index;
				} else if (!isClass0 && !isClass2 && sequenceByte == remoteSeq) {
					return index;
				}
			}
		}
		if ((unsigned int)++index >= 0x400)
			index = 0;
	}
	return -1;
}

// FUNCTION: XVT 0x46FDF0
int NetReliable_RemoveQueuedPacket(unsigned int queueIndex) {
	if (g_netRecvQueueReadIndex == (int)queueIndex) {
		++g_netRecvQueueReadIndex;
		--g_netRecvQueueCount;
		if (g_netRecvQueueReadIndex >= 1024) {
			g_netRecvQueueReadIndex = 0;
		}
		return 1;
	} else {
		unsigned int srcIndex;
		unsigned int endIndex;

		srcIndex = queueIndex + 1;
		if (srcIndex >= 1024u) {
			srcIndex = 0;
		}
		endIndex = g_netRecvQueueReadIndex + g_netRecvQueueCount;
		if (endIndex >= 1024u) {
			endIndex -= 1024u;
		}

		while (endIndex != srcIndex) {
			memcpy(&g_netSessionRecvQueue[queueIndex], &g_netSessionRecvQueue[srcIndex],
				   sizeof(g_netSessionRecvQueue[queueIndex]));
			++queueIndex;
			if (queueIndex >= 1024u) {
				queueIndex = 0;
			}
			++srcIndex;
			if (srcIndex >= 1024u) {
				srcIndex = 0;
			}
		}

		--g_netRecvQueueCount;
		if (g_netRecvQueueWriteIndex == 0) {
			g_netRecvQueueWriteIndex = 1023;
		} else {
			--g_netRecvQueueWriteIndex;
		}
		return 0;
	}
}

// FUNCTION: XVT 0x46FEE0
unsigned int NetReliable_FindOrCreatePeerSlot(int directPlayId) {
	unsigned int slot;
	NetReliablePeerSlot* peer;
	unsigned int initializeSlot;
	unsigned int* peerSlotCount;

	slot = 0;
	if (g_netSession.reliablePeerSlotCount != 0) {
		peer = g_netSession.reliablePeerSlots;
		do {
			if (peer->directPlayId == (DPID)directPlayId) {
				return slot;
			}
			peer++;
			slot++;
		} while (slot < g_netSession.reliablePeerSlotCount);
	}

	if (g_netSession.reliablePeerSlotCount == slot && slot < 40) {
		g_netSession.reliablePeerSlots[slot].directPlayId = directPlayId;
		initializeSlot = slot;
		g_netSession.reliablePeerSlots[initializeSlot].prevRecvSeqDefault = 127;
		g_netSession.reliablePeerSlots[initializeSlot].prevRecvSeqChannelA = 127;
		g_netSession.reliablePeerSlots[initializeSlot].prevRecvSeqChannelB = 127;
		g_netSession.reliablePeerSlots[initializeSlot].recvSeqDefault = 127;
		g_netSession.reliablePeerSlots[initializeSlot].recvSeqChannelA = 127;
		g_netSession.reliablePeerSlots[initializeSlot].recvSeqChannelB = 127;
		g_netSession.reliablePeerSlots[initializeSlot].sendSeq = 0;
		g_netSession.reliablePeerSlots[initializeSlot].lastPiggybackType = NET_PACKET_NOP;
		g_netSession.reliablePeerSlots[initializeSlot].piggybackLength = 1;
		g_netSession.reliablePeerSlots[slot].lastActivityMs = timeGetTime();
		g_netSession.reliablePeerSlots[slot].packetCount = 0;
		g_netSession.reliablePeerSlots[slot].packetDropCount = 0;
		peerSlotCount = &g_netSession.reliablePeerSlotCount;
		++*peerSlotCount;
	}

	return slot;
}

// FUNCTION: XVT 0x46FFC0
void NetReliable_ResetRecvQueueState(void) {
	unsigned int slot;

	g_netRecvQueueReadIndex = g_netRecvQueueWriteIndex;
	g_netRecvQueueCount = 0;

	for (slot = 0; slot < g_netSession.reliablePeerSlotCount; ++slot) {
		g_netSession.reliablePeerSlots[slot].prevRecvSeqDefault =
			g_netSession.reliablePeerSlots[slot].recvSeqDefault;
		g_netSession.reliablePeerSlots[slot].prevRecvSeqChannelA =
			g_netSession.reliablePeerSlots[slot].recvSeqChannelA;
		g_netSession.reliablePeerSlots[slot].prevRecvSeqChannelB =
			g_netSession.reliablePeerSlots[slot].recvSeqChannelB;
		g_netSession.reliablePeerSlots[slot].lastActivityMs = timeGetTime();
	}
}

// FUNCTION: XVT 0x470020
int NetReliable_GetPeerPacketDropCountByDpid_0(int directPlayId) {
	unsigned int slot;
	NetReliablePeerSlot* peer;

	slot = 0;
	if (g_netSession.reliablePeerSlotCount != 0) {
		peer = g_netSession.reliablePeerSlots;
		do {
			if ((DPID)directPlayId == peer->directPlayId) {
				return g_netSession.reliablePeerSlots[slot].packetDropCount;
			}
			peer++;
			slot++;
		} while (slot < g_netSession.reliablePeerSlotCount);
	}
#ifdef XVT_MODERN
	return -1;
#endif
}

#if defined(_MSC_VER) && _MSC_VER <= 1100
#pragma function(memcpy)
#endif
// FUNCTION: XVT 0x470060
int NetReliable_CompactLocalReceiveQueue(void) {
	unsigned int dstIndex;
	unsigned int srcIndex;
	unsigned int keptCount;
	unsigned int slot;
	DPID directPlayId;
	NetQueuedPacket* sourcePacket;
	NetReliablePeerSlot* peer;
#ifdef XVT_MODERN
	unsigned int queuedCount = g_netRecvQueueCount;
#endif

	dstIndex = (unsigned int)g_netRecvQueueReadIndex;
	srcIndex = (unsigned int)g_netRecvQueueReadIndex;
	keptCount = 0;
	while ((int)g_netRecvQueueCount > 0) {
		sourcePacket = &g_netSessionRecvQueue[srcIndex];
		directPlayId = sourcePacket->directPlayId;
		if (directPlayId == 0 || (DPID)g_netSession.hostDplayId == directPlayId) {
			memcpy(&g_netSessionRecvQueue[dstIndex], sourcePacket, sizeof(*sourcePacket));
			++dstIndex;
			if (dstIndex >= 1024)
				dstIndex = 0;
			++keptCount;
		}

		++srcIndex;
		if (srcIndex >= 1024)
			srcIndex = 0;
		--g_netRecvQueueCount;
	}

	g_netRecvQueueCount = keptCount;
#ifdef XVT_MODERN
	Aeron_LogWarn("xvt.net",
				  "receive queue compacted: kept %u host/system packets, discarded %u from other peers",
				  keptCount, queuedCount - keptCount);
#endif
	g_netRecvQueueWriteIndex = (int)dstIndex;
	slot = 0;
	if (g_netSession.reliablePeerSlotCount != 0) {
		peer = g_netSession.reliablePeerSlots;
		do {
			if (peer->directPlayId != (DPID)g_netSession.hostDplayId) {
				peer->prevRecvSeqDefault = peer->recvSeqDefault;
				peer->prevRecvSeqChannelA = peer->recvSeqChannelA;
				peer->prevRecvSeqChannelB = peer->recvSeqChannelB;
				peer->lastActivityMs = timeGetTime();
			}
			++peer;
			++slot;
		} while (g_netSession.reliablePeerSlotCount > slot);
	}

	return 1;
}
#if defined(_MSC_VER) && _MSC_VER <= 1100
#pragma intrinsic(memcpy)
#endif
