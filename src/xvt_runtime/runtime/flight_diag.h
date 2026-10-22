#ifndef XVT_RUNTIME_FLIGHT_DIAG_H
#define XVT_RUNTIME_FLIGHT_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Why a flight left its frame loop. Sites record a reason just before they set
 * missionEndPending or otherwise end the flight; the first reason of a flight wins. */
typedef enum XvtFlightEndReason {
	XVT_FLIGHT_END_NONE,
	XVT_FLIGHT_END_SESSION_LOST,
	XVT_FLIGHT_END_HOST_ABORT,
	XVT_FLIGHT_END_KICKED,
	XVT_FLIGHT_END_HOST_TIMEOUT,
	XVT_FLIGHT_END_STATE_REQUEST_TIMEOUT,
	XVT_FLIGHT_END_RESYNC_RESTARTS,
	XVT_FLIGHT_END_RESYNC_RECEIVE_TIMEOUT,
	XVT_FLIGHT_END_HOST_RECOVERY,
	XVT_FLIGHT_END_CHECKSUM_QUEUE,
	XVT_FLIGHT_END_SNAPSHOT_RESTORE,
	XVT_FLIGHT_END_SNAPSHOT_CHECKSUM,
	XVT_FLIGHT_END_TICK_LIMIT,
	XVT_FLIGHT_END_TIME_LIMIT,
	XVT_FLIGHT_END_NO_FLYING_PLAYERS,
	XVT_FLIGHT_END_LAST_PLAYER_FINISHED,
	XVT_FLIGHT_END_LOCAL_DISCONNECTED,
	XVT_FLIGHT_END_LOCAL_DEPARTED,
	XVT_FLIGHT_END_LOCAL_LEFT,
	XVT_FLIGHT_END_PROVING_GROUNDS,
	XVT_FLIGHT_END_COUNT
} XvtFlightEndReason;

/* Incoming flight packets discarded without effect. */
typedef enum XvtFlightDrop {
	XVT_FLIGHT_DROP_INVALID_CONTROL,
	XVT_FLIGHT_DROP_UNKNOWN_SENDER,
	XVT_FLIGHT_DROP_INVALID_BATCH,
	XVT_FLIGHT_DROP_INACTIVE_BATCH,
	XVT_FLIGHT_DROP_RECOVERY_BATCH,
	XVT_FLIGHT_DROP_FOREIGN_WORLD,
	XVT_FLIGHT_DROP_COUNT
} XvtFlightDrop;

void XvtFlightDiag_Begin(void);
void XvtFlightDiag_NoteEnd(XvtFlightEndReason reason, int detail);
void XvtFlightDiag_CountDrop(XvtFlightDrop kind, int sender);
/* Gameplay participation: a player stopped flying (finished, out of craft) or was removed. */
void XvtFlightDiag_PlayerFinished(int slot, unsigned flying);
void XvtFlightDiag_PlayerRemoved(int slot);
void XvtFlightDiag_LogStart(void);
void XvtFlightDiag_LogEnd(int phase);

#ifdef __cplusplus
}
#endif
#endif
