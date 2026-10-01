#ifndef XVT_FLIGHT_MISSION_MISSION_H
#define XVT_FLIGHT_MISSION_MISSION_H

#include "xvt/assets/object_type.h"
#include "xvt/flight/mission/goals.h"
#include "xvt/xvt_typedefs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

struct MissionOrder {
	uint8_t order;
	uint8_t throttle;
	uint8_t variable1;
	uint8_t variable2;
	uint8_t variable3;
	uint8_t variable4;
	uint8_t secondaryTargetTypes[2];
	uint8_t secondaryTargets[2];
	uint8_t target3OrTarget4;
	uint8_t unused0;
	uint8_t target1Type;
	uint8_t target1;
	uint8_t target2Type;
	uint8_t target2;
	uint8_t target1OrTarget2;
	uint8_t unused1;
	uint8_t speed;
	char designation[16];
	uint8_t reserved[47];
};

#pragma pack(pop)
typedef char xvt_size_MissionOrder[(sizeof(MissionOrder) == 82) ? 1 : -1];

extern uint16_t g_flightRuntimeReservedState;
extern uint8_t g_flightRuntimeStateInitialized;

#pragma pack(push, 1)

struct XvtFlightGroup {
	char name[20];
	char craftRole[16];
	uint8_t reservedCraftRole[4];
	char cargo[20];
	char specialCargo[20];
	uint8_t specialCargoCraft;
	uint8_t randomSpecialCargoCraft;
	CraftSpecies craftType; ///< Primary CraftSpecies for this mission flight group.
	uint8_t numberOfCraft;
	uint8_t status1;
	uint8_t warhead;
	uint8_t beam;
	uint8_t iff;
	uint8_t team;
	uint8_t groupAI;
	uint8_t markings;
	uint8_t radio;
	uint8_t reserved5C;
	uint8_t formation;
	uint8_t formationSpacing;
	uint8_t globalGroup;
	uint8_t reserved60;
	uint8_t numberOfWaves;
	uint8_t wavesDelay;
	uint8_t stopArrivingWhen;
	uint8_t playerNumber;
	uint8_t arriveOnlyIfHuman;
	uint8_t playerCraft;
	uint8_t yaw;
	uint8_t pitch;
	uint8_t roll;
	uint8_t legacyPermaDeathEnabled;
	uint8_t legacyPermaDeathId;
	uint8_t reservedPermaDeath;
	uint8_t arrivalDifficulty;
	MissionTriggerPair arrivalTriggers[2];
	uint8_t arrivals12OrArrivals34;
	uint8_t arrivalRandDelayMinutes;
	uint8_t arrivalDelayMinutes;
	uint8_t arrivalDelaySeconds;
	MissionTriggerPair departureTrigger;
	uint8_t departureDelayMinutes;
	uint8_t departureDelaySeconds;
	uint8_t abortTrigger;
	uint8_t arrivalRandDelaySeconds;
	int16_t editorMothership;
	uint8_t reservedMothership;
	uint8_t arrivalMothership;
	uint8_t arrivalMethod;
	uint8_t departureMothership;
	uint8_t departureMethod;
	uint8_t alternateMothership;
	uint8_t alternateMothershipUsed;
	uint8_t capturedDepartureMothership;
	uint8_t capturedDepartViaMothership;
	MissionOrder orders[4];
	MissionTriggerPair skipToOrder4;
	FlightGroupGoal goals[8];
	uint8_t reservedGoalsTail;
	int16_t missionPointX[22];
	int16_t missionPointY[22];
	int16_t missionPointZ[22];
	uint16_t missionPointEnabled[22];
	uint8_t reservedOptionsPrefix[10];
	uint8_t disableWaveNumbering;
	uint8_t departureClockMin;
	uint8_t departureClockSec;
	uint8_t countermeasures;
	uint8_t craftExplosionTime;
	uint8_t status2;
	uint8_t globalUnit;
	uint8_t reservedOptions[8];
	uint8_t handicap;
	uint8_t optionalWarheads[8];
	uint8_t optionalBeams[6];
	uint8_t optionalCountermeasures[4];
	uint8_t optionalCraftCategory;
	CraftSpecies optionalCraft[10]; ///< Alternative CraftSpecies values exposed by mission loadout selection.
	uint8_t numberOfOptionalCraft[10];
	uint8_t numberOfOptionalCraftWaves[10];
	uint8_t reservedTail;
};

#pragma pack(pop)
typedef char xvt_size_XvtFlightGroup[(sizeof(XvtFlightGroup) == 1378) ? 1 : -1];

#pragma pack(push, 1)

struct MissionFlightGroup {
	XvtFlightGroup fg;
	int playerOwnerIdx;
};

#pragma pack(pop)
typedef char xvt_size_MissionFlightGroup[(sizeof(MissionFlightGroup) == 1382) ? 1 : -1];

extern MissionFlightGroup g_missionFlightGroups[48];
extern GlobalGoal g_missionGlobalGoals[10][7];
extern uint16_t g_currentFlightGroupIdx;
extern uint16_t g_missionFileVersion;
extern uint8_t g_initialSpawnBindPlayerCraftSlots;
extern uint8_t g_spawnLeaderObjIdx;
extern char g_missionDebugBuffer[256];
extern uint16_t g_missionFgOverrideStringHandles[48][8][3];
extern uint16_t g_globalGoalOverrideStringHandles[10][7][4][3];
extern int worldlocx;
extern int worldlocy;
extern int worldlocz;
#pragma pack(push, 1)

struct Team {
	char name[16];
	uint8_t reserved10[8]; ///< Mission-file reserved bytes; loaded with the 0x1E5-byte team record and
						   ///< otherwise unreferenced.
	uint8_t allies[10];
	char endOfMissionMessages[6][64];
	uint8_t eomRawDelay[3];
	uint8_t eomSourceFG[3];
	char voiceIDs[3][20];
	uint8_t reservedTail; ///< Reserved tail byte of the 0x1E5-byte mission team record.
};

#pragma pack(pop)
typedef char xvt_size_Team[(sizeof(Team) == 485) ? 1 : -1];

extern Team g_missionTeams[10];

/* Stored as int8_t in the binary (IDB enum MissionType). */
typedef int8_t MissionType;

enum {
	MISSION_TYPE_JUNKYARD = 0x0,
	MISSION_TYPE_SIMULATOR_1 = 0x1,
	MISSION_TYPE_QUICK_START = 0x2,
	MISSION_TYPE_SIMULATOR_2 = 0x3,
	MISSION_TYPE_SKIRMISH = 0x4,
};

#pragma pack(push, 1)

struct MissionHeader {
	int16_t numFlightGroups;
	uint16_t numMessages;
	uint8_t timeLimitMin;
	uint8_t timeLimitSec;
	uint8_t winType;
	uint8_t backdrop;
	uint8_t rescue;
	uint8_t allWaypointsShown;
	uint8_t variables[8];
	char iffNames[4][20];
	MissionType
		missionType; ///< One-byte mission mode; XVT uses the shared legacy values through SKIRMISH (0..4).
	uint8_t goalsUnimportant; ///< Nonzero suppresses normal mission-goal importance/failure handling.
	uint8_t timeLimitMinutes; ///< Mission countdown duration in whole minutes; zero disables the
							  ///< header-supplied limit.
	uint8_t reserved[61];
};

#pragma pack(pop)
typedef char xvt_size_MissionHeader[(sizeof(MissionHeader) == 162) ? 1 : -1];

extern MissionHeader g_missionHeader;
extern uint16_t g_asteroidFieldRandSeed;

struct MissionClock {
	uint8_t reserved[3];
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	int16_t subsecondTicks;
};

extern MissionClock g_missionElapsedClock;
extern MissionClock g_missionCountdownClock;

#pragma pack(push, 1)

struct MissionMessage {
	char message[64]; ///< Message text passed directly to the in-flight message queue.
	uint8_t
		sentToTeam[10]; ///< Nonzero entry allows the corresponding player IFF/team to receive the message.
	MissionTriggerPair triggerPairs[2]; ///< Two trigger pairs evaluated before the message becomes active.
	char voice[16]; ///< Voice resource name stored by the mission file format; not consumed by XVT's runtime
					///< message path.
	uint8_t rawDelay;                   ///< Raw per-message delay copied to the runtime countdown.
	uint8_t triggerPair1OrTriggerPair2; ///< Value 1 combines the trigger-pair results with OR; other values
										///< use AND.
};

#pragma pack(pop)
typedef char xvt_size_MissionMessage[(sizeof(MissionMessage) == 114) ? 1 : -1];

enum { MISSION_MESSAGE_COUNT = 64 };

extern MissionMessage g_missionMessages[MISSION_MESSAGE_COUNT];

enum {
	TEAM_SCORE_BONUS_TENTHS = 0,
	TEAM_SCORE_MISSION = 1,
};

typedef enum FlightGroupOutcomeIndex {
	FLIGHT_GROUP_OUTCOME_TOTAL = 0,
	FLIGHT_GROUP_OUTCOME_ARRIVED = 1,
	FLIGHT_GROUP_OUTCOME_DESTROYED = 2,
	FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP = 3,
	FLIGHT_GROUP_OUTCOME_ATTACKED = 4,
	FLIGHT_GROUP_OUTCOME_NOT_ATTACKED = 5,
	FLIGHT_GROUP_OUTCOME_CAPTURED = 6,
	FLIGHT_GROUP_OUTCOME_NOT_CAPTURED = 7,
	FLIGHT_GROUP_OUTCOME_INSPECTED = 8,
	FLIGHT_GROUP_OUTCOME_NOT_INSPECTED = 9,
	FLIGHT_GROUP_OUTCOME_BOARDED = 10,
	FLIGHT_GROUP_OUTCOME_NOT_BOARDED = 11,
	FLIGHT_GROUP_OUTCOME_DOCKED = 12,
	FLIGHT_GROUP_OUTCOME_NOT_DOCKED = 13,
	FLIGHT_GROUP_OUTCOME_DISABLED = 14,
	FLIGHT_GROUP_OUTCOME_NOT_DISABLED = 15,
	FLIGHT_GROUP_OUTCOME_DEPARTED = 16,
	FLIGHT_GROUP_OUTCOME_LEFT_REGION = 17,
	FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT = 18,
	FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT = 19,
	FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT = 20,
	FLIGHT_GROUP_OUTCOME_ABORTED = 21,
	FLIGHT_GROUP_OUTCOME_NOT_DEPARTED = 22,
	FLIGHT_GROUP_OUTCOME_CAPTURED_BY_DESTINATION = 23,
	FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION = 24,
	FLIGHT_GROUP_OUTCOME_COMPLETED_MISSION = 25,
	FLIGHT_GROUP_OUTCOME_FAILED_MISSION = 26,
	FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE = 27,
	FLIGHT_GROUP_OUTCOME_COUNT = 28,
} FlightGroupOutcomeIndex;

struct MissionFlightRuntimeState {
	int teamScores[2][10];
	uint16_t teamKillStats[4][10];
	uint16_t teamFgCounters[2][10][48];
	uint8_t teamFgDesignationCode[10][48];
	uint8_t globalPrimaryGoalStatus;
	uint16_t globalGoalStatusUnused;
	uint8_t globalBonusGoalStatus;
	uint8_t teamGlobalGoalState[10][3];
	uint8_t teamGoalStatus[10][3];
	uint16_t globalGoalTriggerCounts[2][10][3][4];
	unsigned int teamMissionCompletionTimeSeconds[10];
	uint8_t teamHasCountableCraft[10];
	uint8_t teamActiveGoalSequence[10];
};

struct MissionFgRuntimeStats {
	uint8_t hasArrived;
	uint8_t wavesRemaining;
	uint8_t arrivalDelayPending;
	uint8_t arrivalEnabled;
	int16_t arrivalDelayTimer;
	uint16_t currentMissionPointRef;
	uint16_t spawnedCraftCount;
	uint16_t outcomeCount[FLIGHT_GROUP_OUTCOME_COUNT];
	uint8_t specialCargoOutcome[FLIGHT_GROUP_OUTCOME_COUNT];
	uint8_t teamInspected[10];
	uint8_t teamSpecialCargoInspected[10];
	uint8_t teamUninspectedLost[10];
	uint8_t teamSpecialCargoUninspectedLost[10];
	uint8_t teamCondition44Count[10];
	uint8_t teamCondition44SpecialCargo[10];
	uint8_t teamCondition44OtherTeamCount[10];
	uint8_t teamCondition44OtherTeamSpecialCargo[10];
	uint8_t teamEventExtra[4][10];
	uint8_t goalState[80];
};

extern MissionFgRuntimeStats g_missionFgStats[48];
extern const uint8_t g_missionConditionUsesCountByTriggerType[48];
extern uint16_t g_missionConditionTotalCount;
extern uint16_t g_missionConditionCurrentCount;
extern int g_preparedSpawnMissionX;
extern int g_preparedSpawnMissionY;
extern int g_preparedSpawnMissionZ;
extern uint16_t g_preparedSpawnYawByte;
extern uint16_t g_preparedSpawnPitchByte;
extern uint16_t g_preparedSpawnRollByte;
extern uint16_t g_nextObjectSignature;
extern const uint8_t g_genusConvert[12];
extern const uint8_t g_familyConvert[4];

#pragma pack(push, 1)

struct EMissionStruct {
	uint8_t time_min;
	uint8_t time_sec;
	uint8_t win_type;
	uint8_t backdrop;
	uint8_t rescue;
	uint8_t all_way_shown;
	uint8_t mis_var[8];
	int8_t win_bonus[2];
	uint8_t win_msg1[2][64];
	uint8_t win_msg2[2][64];
	uint8_t loss_msg[2][64];
	uint8_t loss_msg_delay;
	uint8_t loss_unused;
	char neutral_name[4][12];
};

#pragma pack(pop)
typedef char xvt_size_EMissionStruct[(sizeof(EMissionStruct) == 450) ? 1 : -1];

#pragma pack(push, 1)

struct ECondStruct {
	uint8_t cond;
	uint8_t type;
	uint8_t id;
	uint8_t pct;
};

#pragma pack(pop)
typedef char xvt_size_ECondStruct[(sizeof(ECondStruct) == 4) ? 1 : -1];

#pragma pack(push, 1)

struct EAIStruct {
	uint8_t order;
	uint8_t speed;
	uint8_t var[4];
	uint8_t target_type[2];
	uint8_t target_id[2];
	uint8_t target_op;
	uint8_t target_unused;
	uint8_t pri_type;
	uint8_t pri_id;
	uint8_t sec_type;
	uint8_t sec_id;
	uint8_t pri_sec_op;
	uint8_t pri_sec_unused;
};

#pragma pack(pop)
typedef char xvt_size_EAIStruct[(sizeof(EAIStruct) == 18) ? 1 : -1];

#pragma pack(push, 1)

struct EFGStruct {
	char name[12];
	char cmdr[12];
	char contents[2][12];
	uint8_t special_craft;
	uint8_t special_flag;
	CraftSpecies species; ///< CraftSpecies stored in the legacy EFG mission record.
	uint8_t count;
	uint8_t version;
	uint8_t warhead;
	uint8_t beam;
	uint8_t side;
	uint8_t skill;
	uint8_t camoflage;
	uint8_t camo_flag;
	uint8_t camo_unused;
	uint8_t formation;
	uint8_t form_spacing;
	uint8_t set;
	uint8_t set_unused;
	uint8_t waves;
	uint8_t wave_delay;
	uint8_t player_flag;
	uint8_t heading;
	uint8_t pitch;
	uint8_t rotation;
	uint8_t link_flag;
	uint8_t link_code;
	uint8_t link_unused;
	uint8_t difficulty;
	ECondStruct start_cond[2];
	uint8_t start_op;
	uint8_t start_unused;
	uint8_t start_delay_min;
	uint8_t start_delay_sec;
	ECondStruct stop_cond;
	uint8_t stop_min;
	uint8_t stop_sec;
	uint8_t stop_abort;
	uint8_t stop_unused;
	int16_t cur_start_fg;
	uint8_t start_fg;
	uint8_t start_fg_used;
	uint8_t pri_stop_fg;
	uint8_t pri_stop_fg_used;
	uint8_t sec_stop_fg;
	uint8_t sec_stop_fg_used;
	uint8_t capture_fg;
	uint8_t capture_fg_used;
	EAIStruct ai[3];
	uint8_t pri_win_cond;
	uint8_t pri_win_pct;
	uint8_t sec_win_cond;
	uint8_t sec_win_pct;
	uint8_t loss_cond;
	uint8_t loss_pct;
	uint8_t bonus_cond;
	uint8_t bonus_pct;
	int8_t bonus_points;
	uint8_t bonus_unused;
	int16_t way_x[15];
	int16_t way_y[15];
	int16_t way_z[15];
	int16_t way_used[15];
	uint8_t way_shown;
	uint8_t way_unused;
	uint8_t way_brief_link;
	uint8_t way_brief_shown;
};

#pragma pack(pop)
typedef char xvt_size_EFGStruct[(sizeof(EFGStruct) == 292) ? 1 : -1];

#pragma pack(push, 1)

struct EMissionGoal {
	ECondStruct subcond[2];
	uint8_t editor_name[17];
	uint8_t or_joined;
	uint8_t _pad[2];
};

#pragma pack(pop)
typedef char xvt_size_EMissionGoal[(sizeof(EMissionGoal) == 28) ? 1 : -1];

#pragma pack(push, 1)

struct XvtV10FlightGroupText {
	char name[12];
	char craftRole[12];
	char cargo[12];
	char specialCargo[12];
};

#pragma pack(pop)
typedef char xvt_size_XvtV10FlightGroupText[(sizeof(XvtV10FlightGroupText) == 48) ? 1 : -1];

#pragma pack(push, 1)

struct TieRadioMessage {
	char message[64];
	ECondStruct conditions[2];
	char voice[16];
	uint8_t rawDelay;
	uint8_t condition1OrCondition2;
};

#pragma pack(pop)
typedef char xvt_size_TieRadioMessage[(sizeof(TieRadioMessage) == 90) ? 1 : -1];

#pragma pack(push, 1)

struct XvtV10MissionHeader {
	uint16_t numFlightGroups;
	uint16_t numMessages;
	uint8_t timeLimitMin;
	uint8_t timeLimitSec;
	uint8_t winType;
	uint8_t backdrop;
	uint8_t rescue;
	uint8_t allWaypointsShown;
	uint8_t variables[8];
	char iffNames[4][12];
	MissionType missionType;
	uint8_t goalsUnimportant;
	uint8_t missionTimeLimit;
	uint8_t reserved[61];
};

#pragma pack(pop)
typedef char xvt_size_XvtV10MissionHeader[(sizeof(XvtV10MissionHeader) == 130) ? 1 : -1];

uint16_t Mission_GetFlightGroupSpecialCargoOutcome8(unsigned int flightGroupIdx, uint16_t specialCargoCraft);
void Mission_UpdateLogic(void);
int Mission_EvaluateTriggerPair(const MissionTriggerPair* triggerPair, int16_t flightGroupIdx);
int16_t Mission_EvaluateCondition(uint16_t conditionType, int16_t variableType, uint16_t variable,
								  int16_t amountType, int16_t includeDepartedAsDestroyed,
								  uint16_t teamOrVariable);
int16_t Mission_FlightGroupMatchesTriggerVariable(uint16_t flightGroupIdx, int16_t variableType,
												  uint16_t variable);
int16_t Mission_ObjectMatchesTriggerVariable(uint16_t objectIdx, uint16_t variableType, uint16_t variable);
void Mission_RecordCraftOutcome(uint16_t objIdx, uint16_t flightGroupIdx, uint16_t outcomeId);
int16_t Mission_CloseUnavailableFlightGroupAccounting(int flightGroupIdx);
void Mission_CreditDestructionDamageContributors(uint16_t sourceObjIdx, uint16_t victimObjIdx);
void Mission_CreditPlayerKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										  int playerIdx, int creditedOwnerIdx, int victimRating);
void Mission_CreditTeamKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										int teamIdx);
void Mission_RecordProjectileHitStats(uint16_t projectileObjIdx);
int Mission_RecordPlayerCraftLoss(unsigned int objIdx, int allowPendingDamageCredit);
void Mission_RecordPlayerCraftLossAttribution(int attackerFlightGroupIdx, int victimObjIdx,
											  int contributionTier);
int Mission_ApplyFlightGroupGoalScore(int16_t eventCondition, uint16_t flightGroupIdx, int playerIdx,
									  uint16_t scoreDivisor, int specialCargoFlag, int teamIdx);
void Mission_ApplyTeamGoalScoreAllEnabledTeams(int16_t eventCondition, uint16_t flightGroupIdx,
											   int specialCargoFlag);
void Mission_ApplyTeamGoalScoreForTeam(int16_t eventCondition, uint16_t flightGroupIdx, int specialCargoFlag,
									   uint8_t teamIdx);
int Mission_GameTimeToSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds);
int Mission_ComputeKillScoreForObject(int victimObjIdx);
int Mission_ComputeCraftPointValue(int objIdx);
int Mission_GetElapsedClockSeconds(void);
uint16_t Mission_Init(char* fileName);
void Mission_InitFlightRuntimeState(void);
int16_t Mission_StartFlightGroupArrival(uint16_t craftOrdinal);
void Mission_UpdateFlightGroupArrivals(void);
void Mission_ProcessFlightGroupWaveCompletion(uint16_t flightGroupIdx);
void Mission_SpawnCurrentFlightGroupWave(void);
int Mission_HasCapacityForCurrentFlightGroupWave(void);
int16_t Mission_SpawnFlightGroupWaveCraft(uint16_t craftOrdinal);
uint16_t Mission_InitFlightGroupObjectSlot(void);
void Mission_SpawnFlightGroupStaticObjects(uint16_t craftOrdinal);
uint16_t Mission_SpawnPreparedObject(uint16_t flightGroupIdx, int16_t genusId, uint8_t objectType);
void Mission_ResolveObjectOrMissionPointWorldLoc(unsigned int objOrMissionPointRef, int flightGroupIdx);
void Mission_ResolveFormationSlotWorldLoc(uint16_t flightGroupIdx, uint16_t formationSlotIdx,
										  uint16_t basisObjIdx);
int Mission_LoadFile(char* fileName);
int Mission_SyncPilotNetworkPlayersToSessionSlots(void);
void Mission_FreeOverrideStringHandles(void);

#ifdef __cplusplus
}
#endif

#endif
