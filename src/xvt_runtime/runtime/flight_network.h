#ifndef XVT_RUNTIME_FLIGHT_NETWORK_H
#define XVT_RUNTIME_FLIGHT_NETWORK_H

#include "xvt/flight/flight_input.h"
#include "xvt_runtime/runtime/flight_messages.h"
#include "xvt_runtime/timing/flight_timing.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void XvtFlightNetwork_ProcessPackets(void);
void XvtFlightNetwork_BeginClockProbes(void);
int XvtFlightNetwork_ShouldSend(int inputTimestamp);
int XvtFlightNetwork_NeedsRecovery(void);
void XvtFlightNetwork_RequestRecovery(void);
int XvtFlightNetwork_PlayerAbort(unsigned player);
void XvtFlightNetwork_BeginRecovery(void);
void XvtFlightNetwork_Recovered(void);
void XvtFlightNetwork_BeginIteration(void);
int XvtFlightNetwork_TakePacketBudget(void);
void XvtFlightNetwork_BeginMission(void);
void XvtFlightNetwork_FlushInput(int now);
int XvtFlightNetwork_AdmitInput(int tick);
int XvtFlightNetwork_Outgoing(void);
uint64_t XvtFlightNetwork_NextWakeDelayUs(int now);
void XvtFlightNetwork_FlushWorld(void);
void XvtFlightNetwork_SendWorld(void);
int XvtFlightNetwork_InsertWorld(const XvtFlightMessage* message);
int XvtFlightNetwork_Receive(int sender, const uint8_t* bytes, size_t size);

enum { XVT_FLIGHT_NETWORK_PENDING = -1 };

int XvtFlightNetwork_SendWire(int dpid, const void* packet, size_t size);
int XvtFlightNetwork_BroadcastWire(const void* packet, size_t size);
int XvtFlightNetwork_SendPacket(int dpid, const unsigned* packet, int size);
int XvtFlightNetwork_Broadcast(const unsigned* packet, int size);
int XvtFlightNetwork_DecodeControl(const uint8_t* packet, int* size);
uint32_t XvtFlightNetwork_Cookie(void);
void XvtFlightNetwork_CloseSession(void);
int XvtFlightNetwork_BeginSession(int players, int in_progress);
int XvtFlightNetwork_Session(void);
int XvtFlightNetwork_Options(void);
int XvtFlightNetwork_Start(void);
void XvtFlightNetwork_Reset(void);

#ifdef __cplusplus
}
#endif
#endif
