#include "xvt/flight/mission/mission.h"
#include "xvt/assets/file.h"

#include "xvt/assets/model_bounds.h"
#include "xvt/assets/model_mesh.h"
#include "xvt/assets/opt_model.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/ai/pai.h"
#include "xvt/flight/ai/paiman.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/fediskio.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/flight_object.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/flight_view.h"
#include "xvt/flight/hud/flight_text.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/hud/msg.h"
#include "xvt/flight/object/collide.h"
#include "xvt/flight/object/laser.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/frontend/config.h"
#include "xvt/frontend/pilot_record.h"
#include "xvt/math/math2.h"
#include "xvt/math/trig2.h"
#include "xvt/net/flight_net.h"
#include "xvt/net/net_session.h"
#include "xvt/render/backdrop.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/renderer.h"
#include "xvt/util/game_rand.h"
#include "xvt/util/memory.h"
#include "xvt/util/time.h"
#include <stdio.h>
#include <string.h>

// GLOBAL: XVT 0xA00750
char g_missionDebugBuffer[256] = { 0 };

// GLOBAL: XVT 0x99F970
int g_preparedSpawnMissionX = 0;
// GLOBAL: XVT 0x99F974
int g_preparedSpawnMissionY = 0;
// GLOBAL: XVT 0x99F978
int g_preparedSpawnMissionZ = 0;
// GLOBAL: XVT 0x99F97C
uint16_t g_preparedSpawnYawByte = 0;
// GLOBAL: XVT 0x99F97E
uint16_t g_preparedSpawnPitchByte = 0;
// GLOBAL: XVT 0x99F980
uint16_t g_preparedSpawnRollByte = 0;
// GLOBAL: XVT 0x99F982
uint8_t g_spawnLeaderObjIdx = 0;
// GLOBAL: XVT 0x99F983
uint8_t g_spawnTeamId = 0;
// GLOBAL: XVT 0x99F984
uint8_t g_spawnIff = 0;
// GLOBAL: XVT 0x99F985
uint8_t g_spawnFormationSpacing = 0;
// GLOBAL: XVT 0x99F987
uint8_t g_spawnGroupAI = 0;
// GLOBAL: XVT 0x99F988
uint16_t g_spawnYaw = 0;
// GLOBAL: XVT 0x99F98A
uint8_t g_spawnOutOfHyperspaceFlag = 0;
// GLOBAL: XVT 0x99F98B
uint8_t g_spawnStatus1 = 0;
// GLOBAL: XVT 0x99F98C
uint8_t g_spawnFromMothershipFlag = 0;
// GLOBAL: XVT 0x99F98E
uint16_t g_spawnPitch = 0;
// GLOBAL: XVT 0x99F990
uint16_t g_spawnCraftOrdinal = 0;
// GLOBAL: XVT 0x99F992
uint8_t g_spawnObjectType = 0;
// GLOBAL: XVT 0x9A1080
CraftObjectKind g_spawnObjectKind = CRAFT_OBJECT_KIND_ACTIVE;
// GLOBAL: XVT 0x9A1081
uint8_t g_spawnStatus2 = 0;
// GLOBAL: XVT 0x9A1084
int g_spawnWorldX = 0;
// GLOBAL: XVT 0x9A1088
int g_spawnWorldY = 0;
// GLOBAL: XVT 0x9A108C
int g_spawnWorldZ = 0;
// GLOBAL: XVT 0x9A1830
uint8_t g_spawnGenusId = 0;
// GLOBAL: XVT 0x9A1831
uint8_t g_spawnedObjectIff = 0;
// GLOBAL: XVT 0x9A1832
uint8_t g_spawnFormation = 0;
// GLOBAL: XVT 0x556ECC
uint8_t g_initialSpawnBindPlayerCraftSlots = 0;
// GLOBAL: XVT 0x9A73F0
uint16_t g_asteroidFieldRandSeed = 0;
// GLOBAL: XVT 0x9A8D40
uint16_t g_nextObjectSignature = 0;
// GLOBAL: XVT 0x9A2000
MissionHeader g_missionHeader = { 0 };
// GLOBAL: XVT 0x9A20C0
MissionFgRuntimeStats g_missionFgStats[48];
// GLOBAL: XVT 0x9A8080
GlobalGoal g_missionGlobalGoals[10][7] = { { { 0 } } };
// GLOBAL: XVT 0x9D8C30
MissionFlightGroup g_missionFlightGroups[48];
// GLOBAL: XVT 0x9FD440
Team g_missionTeams[10] = { { 0 } };
// GLOBAL: XVT 0x9A1834
uint16_t g_currentFlightGroupIdx = 0;
// GLOBAL: XVT 0x9ED228
MissionClock g_missionElapsedClock;
// GLOBAL: XVT 0x9D8B70
MissionClock g_missionCountdownClock = { { 0, 0, 0 }, 0, 0, 0, 0 };
// GLOBAL: XVT 0x9FE800
MissionMessage g_missionMessages[MISSION_MESSAGE_COUNT] = { 0 };
// GLOBAL: XVT 0x9D8120
uint16_t g_missionFgOverrideStringHandles[48][8][3] = { 0 };
// GLOBAL: XVT 0x9E8F60
uint16_t g_globalGoalOverrideStringHandles[10][7][4][3] = { 0 };
// GLOBAL: XVT 0xA07CDC
int worldlocx = 0;
// GLOBAL: XVT 0xA07CD8
int worldlocy = 0;
// GLOBAL: XVT 0xA07CD4
int worldlocz = 0;
// GLOBAL: XVT 0xA08294
uint16_t g_missionFileVersion = 0;
// GLOBAL: XVT 0x556AA8
EMissionStruct g_tieMissionHeader = { 0 };
// GLOBAL: XVT 0x556C70
EFGStruct g_tieFlightGroup = { 0 };
// GLOBAL: XVT 0x556D98
EMissionGoal g_tieMissionGoal = { 0 };
// GLOBAL: XVT 0x556DB8
XvtV10FlightGroupText g_xvtV10FlightGroupText = { 0 };
// GLOBAL: XVT 0x556DE8
TieRadioMessage g_tieRadioMessage = { 0 };
// GLOBAL: XVT 0x556E48
XvtV10MissionHeader g_xvtV10MissionHeader = { 0 };
// GLOBAL: XVT 0x521158
const int g_defaultPilotRatingByAiLevel[6] = { 2, 4, 7, 9, 10, 11 };
// GLOBAL: XVT 0x521170
const int g_beamTypePointValue[6] = { 0, 150, 150, 250, 50, 0 };
// GLOBAL: XVT 0x521188
const int g_countermeasureTypePointValue[4] = { 0, 150, 100, 150 };
// GLOBAL: XVT 0x5241D0
const uint8_t g_aiOpponentCraftTypeByPlayerCraftType[24] = {
	0, 6, 16, 8, 16, 14, 1, 14, 3, 3, 3, 3, 3, 3, 5, 5, 2, 0, 0, 0, 0, 0, 0, 0,
};
// GLOBAL: XVT 0x5241E8
const uint8_t g_genusConvert[12] = { 0, 1, 3, 4, 2, 5, 8, 9, 2, 0, 0, 0 };
// GLOBAL: XVT 0x5241F4
const uint8_t g_familyConvert[4] = { 0, 1, 2, 0 };
// GLOBAL: XVT 0x521128
const uint8_t g_missionConditionUsesCountByTriggerType[48] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0,
};
// GLOBAL: XVT 0x556354
uint16_t g_missionConditionTotalCount = 0;
// GLOBAL: XVT 0x556358
uint16_t g_missionConditionCurrentCount = 0;
// GLOBAL: XVT 0x524280
const uint8_t g_fgArrivalDifficultyMasks[8] = { 7, 1, 2, 4, 6, 3, 0, 0 };
// GLOBAL: XVT 0x524288
const uint8_t g_missionDifficultyArrivalMasks[8] = { 1, 2, 4, 0, 0, 0, 0, 0 };
// GLOBAL: XVT 0x9D77BC
uint16_t g_flightRuntimeReservedState = 0;
// GLOBAL: XVT 0x523440
uint8_t g_flightRuntimeStateInitialized = 1;
// GLOBAL: XVT 0x9A8C04
uint16_t g_flightFrameStepMirror = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x415A40
uint16_t Mission_GetFlightGroupSpecialCargoOutcome8(unsigned int flightGroupIdx, uint16_t specialCargoCraft) {
	(void)specialCargoCraft;
	return g_missionFgStats[(uint16_t)flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_INSPECTED];
}

// FUNCTION: XVT 0x430A00
void Mission_UpdateLogic(void) {
	enum {
		PLAYER_COUNT = 8,
		TEAM_COUNT = 10,
		FG_GOAL_COUNT = 8,
		GLOBAL_GOAL_COUNT = 3,
		GLOBAL_TRIGGER_COUNT = 4,
		GOAL_PRIMARY = 0,
		GOAL_SECONDARY = 1,
		GOAL_BONUS = 2,
		GOAL_STATE_SUCCESS = 1,
		GOAL_STATE_FAILURE = 2,
		GOAL_STATE_PENDING = 4,
		GOAL_RESULT_AWARD_SCORE = 8,
		GOAL_AMOUNT_NO_SCORE_1 = 18,
		GOAL_AMOUNT_NO_SCORE_2 = 19,
		MISSION_MESSAGE_REFRESH_TICKS = 1180,
		GLOBAL_GOAL_SCORE_SCALE = 250,
		FLIGHT_GROUP_GOAL_SCORE_SCALE = 250,
		PRIMARY_GOAL_SCORE = 10000,
		BONUS_GOAL_SCORE = 2500,
		TEAM_MESSAGE_COUNT = 2,
		PRIMARY_TEAM_MESSAGE = 0,
		FAILED_TEAM_MESSAGE = 2,
		BONUS_TEAM_MESSAGE = 4,
		PRIMARY_TEAM_VOICE = 111,
		BONUS_TEAM_VOICE = 112,
		FAILED_TEAM_VOICE = 113,
		MISSION_MESSAGE_VOICE_BASE = 95,
		MISSION_MESSAGE_VOICE_COUNT = 16,
		MESSAGE_DIGIT_FIRST = '1',
		MESSAGE_DIGIT_LAST = '6',
		MESSAGE_ARGUMENT_SCORE_BASE = 294,
	};

	uint16_t messageIndex;

	if (g_flightMissionState.provingGroundsModeActive != 0)
		return;

	if (g_flightGlobalCountdownTimers.missionGoalEvaluationTimer == 0 ||
		g_flightMissionState.missionEndPending != 0) {
		unsigned int missionElapsedSeconds;
		uint16_t flightGroupIndex;
		uint16_t teamIndex;
		int playerIndex;

		g_flightMissionState.connectedPlayerCount = 0;
		for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
			if (g_players[playerIndex].connectedFlag != 0)
				++g_flightMissionState.connectedPlayerCount;
		}
		if ((unsigned int)g_flightMissionState.maxConnectedPlayerCountThisMission <
			(unsigned int)g_flightMissionState.connectedPlayerCount) {
			g_flightMissionState.maxConnectedPlayerCountThisMission =
				g_flightMissionState.connectedPlayerCount;
		}
		missionElapsedSeconds = Mission_GameTimeToSeconds(
			g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);

		for (flightGroupIndex = 0; flightGroupIndex < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIndex) {
			uint16_t goalIndex;

			for (goalIndex = 0; goalIndex < FG_GOAL_COUNT; ++goalIndex) {
				if (g_missionFgStats[flightGroupIndex].arrivalEnabled == 0 &&
					g_missionFlightGroups[flightGroupIndex].playerOwnerIdx == -1) {
					for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex)
						g_missionFgStats[flightGroupIndex].goalState[FG_GOAL_COUNT * teamIndex + goalIndex] =
							0;
				} else {
					for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
						if (g_missionFlightGroups[flightGroupIndex]
								.fg.goals[goalIndex]
								.enabledTeams[teamIndex] != 0) {
							int16_t result;

							if (g_missionFgStats[flightGroupIndex]
									.goalState[FG_GOAL_COUNT * teamIndex + goalIndex] != GOAL_STATE_PENDING) {
								continue;
							}
							if (g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].eventCondition ==
									MISSION_COND_ALWAYS_TRUE ||
								g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].eventCondition ==
									MISSION_COND_NEVER_FALSE) {
								g_missionFgStats[flightGroupIndex]
									.goalState[FG_GOAL_COUNT * teamIndex + goalIndex] = 0;
								continue;
							}
							result = Mission_EvaluateCondition(
								g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].eventCondition,
								GOAL_TARGET_FLIGHT_GROUP, flightGroupIndex,
								(uint8_t)g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].amount,
								0, teamIndex);
							if (result == GOAL_STATE_SUCCESS) {
								result = GOAL_RESULT_AWARD_SCORE;
								if (g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].timeLimit5s !=
									0) {
									result = 5u * g_missionFlightGroups[flightGroupIndex]
															 .fg.goals[goalIndex]
															 .timeLimit5s <
													 missionElapsedSeconds
												 ? GOAL_STATE_FAILURE
												 : GOAL_RESULT_AWARD_SCORE;
								}
							}
							if (result == GOAL_RESULT_AWARD_SCORE &&
								(g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].type !=
									 GOAL_BONUS ||
								 (g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].amount !=
									  GOAL_AMOUNT_NO_SCORE_1 &&
								  g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].amount !=
									  GOAL_AMOUNT_NO_SCORE_2))) {
								g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] +=
									FLIGHT_GROUP_GOAL_SCORE_SCALE *
									g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].points;
								result = GOAL_STATE_SUCCESS;
							}
							g_missionFgStats[flightGroupIndex]
								.goalState[FG_GOAL_COUNT * teamIndex + goalIndex] = (uint8_t)result;
						} else {
							g_missionFgStats[flightGroupIndex]
								.goalState[FG_GOAL_COUNT * teamIndex + goalIndex] = 0;
						}
					}
				}
			}
		}

		for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
			uint16_t goalKind;

			for (goalKind = 0; goalKind < GLOBAL_GOAL_COUNT; ++goalKind) {
				int16_t sawSuccess = 0;
				uint16_t aggregateState = goalKind != GOAL_SECONDARY;
				uint16_t globalState;
				uint8_t condition1 =
					g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[0].condition;
				uint8_t condition2 =
					g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[1].condition;
				uint8_t condition3 =
					g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[0].condition;
				uint8_t condition4 =
					g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[1].condition;
				uint16_t triggerIndex;

				for (triggerIndex = 0; triggerIndex < GLOBAL_TRIGGER_COUNT; ++triggerIndex) {
					g_flightMissionState.runtime
						.globalGoalTriggerCounts[0][teamIndex][goalKind][triggerIndex] = 0;
					g_flightMissionState.runtime
						.globalGoalTriggerCounts[1][teamIndex][goalKind][triggerIndex] = 0;
				}
				if ((condition1 == MISSION_COND_NEVER_FALSE || condition1 == MISSION_COND_ALWAYS_TRUE) &&
					(condition2 == MISSION_COND_NEVER_FALSE || condition2 == MISSION_COND_ALWAYS_TRUE) &&
					(condition3 == MISSION_COND_NEVER_FALSE || condition3 == MISSION_COND_ALWAYS_TRUE) &&
					(condition4 == MISSION_COND_NEVER_FALSE || condition4 == MISSION_COND_ALWAYS_TRUE)) {
					globalState = 0;
				} else {
					int pair1Team = TEAM_COUNT;
					int pair2Team;
					int16_t pair1Result;
					int16_t pair2Result;
					uint16_t trigger1Result;
					uint16_t trigger2Result;

					sawSuccess = 1;
					if (condition2 == MISSION_COND_NO_CONDITION &&
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[1].variableType ==
							GOAL_TARGET_TEAM) {
						pair1Team =
							g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[1].variable;
					}
					trigger1Result = (uint16_t)Mission_EvaluateCondition(
						condition1,
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[0].variableType,
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[0].variable,
						(uint8_t)g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[0].amount,
						0, (uint16_t)pair1Team);
					g_flightMissionState.runtime.globalGoalTriggerCounts[0][teamIndex][goalKind][0] =
						g_missionConditionCurrentCount;
					g_flightMissionState.runtime.globalGoalTriggerCounts[1][teamIndex][goalKind][0] =
						g_missionConditionTotalCount;
					if (condition2 != MISSION_COND_NO_CONDITION) {
						trigger2Result = (uint16_t)Mission_EvaluateCondition(
							condition2,
							g_missionGlobalGoals[teamIndex][goalKind]
								.triggerPairs[0]
								.triggers[1]
								.variableType,
							g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].triggers[1].variable,
							(uint8_t)g_missionGlobalGoals[teamIndex][goalKind]
								.triggerPairs[0]
								.triggers[1]
								.amount,
							0, (uint16_t)pair1Team);
						g_flightMissionState.runtime.globalGoalTriggerCounts[0][teamIndex][goalKind][1] =
							g_missionConditionCurrentCount;
						g_flightMissionState.runtime.globalGoalTriggerCounts[1][teamIndex][goalKind][1] =
							g_missionConditionTotalCount;
					} else {
						trigger2Result = 0;
					}
					if (condition2 == MISSION_COND_NO_CONDITION) {
						pair1Result =
							(trigger1Result & 1) != 0
								? GOAL_STATE_SUCCESS
								: ((trigger1Result & 2) != 0 ? GOAL_STATE_FAILURE : GOAL_STATE_PENDING);
					} else if (g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[0].trigger1OrTrigger2 ==
							   GOAL_OPERATOR_STR_OR) {
						pair1Result =
							((trigger1Result | trigger2Result) & 1) != 0
								? GOAL_STATE_SUCCESS
								: (((trigger1Result & trigger2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																				: GOAL_STATE_PENDING);
					} else {
						pair1Result =
							((trigger1Result & trigger2Result) & 1) != 0
								? GOAL_STATE_SUCCESS
								: (((trigger1Result | trigger2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																				: GOAL_STATE_PENDING);
					}

					pair2Team = TEAM_COUNT;
					if (condition4 == MISSION_COND_NO_CONDITION &&
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[1].variableType ==
							GOAL_TARGET_TEAM) {
						pair2Team =
							g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[1].variable;
					}
					trigger1Result = (uint16_t)Mission_EvaluateCondition(
						condition3,
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[0].variableType,
						g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[0].variable,
						(uint8_t)g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[0].amount,
						0, (uint16_t)pair2Team);
					g_flightMissionState.runtime.globalGoalTriggerCounts[0][teamIndex][goalKind][2] =
						g_missionConditionCurrentCount;
					g_flightMissionState.runtime.globalGoalTriggerCounts[1][teamIndex][goalKind][2] =
						g_missionConditionTotalCount;
					if (condition4 != MISSION_COND_NO_CONDITION) {
						trigger2Result = (uint16_t)Mission_EvaluateCondition(
							condition4,
							g_missionGlobalGoals[teamIndex][goalKind]
								.triggerPairs[1]
								.triggers[1]
								.variableType,
							g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].triggers[1].variable,
							(uint8_t)g_missionGlobalGoals[teamIndex][goalKind]
								.triggerPairs[1]
								.triggers[1]
								.amount,
							0, (uint16_t)pair2Team);
						g_flightMissionState.runtime.globalGoalTriggerCounts[0][teamIndex][goalKind][3] =
							g_missionConditionCurrentCount;
						g_flightMissionState.runtime.globalGoalTriggerCounts[1][teamIndex][goalKind][3] =
							g_missionConditionTotalCount;
					} else {
						trigger2Result = 0;
					}
					if (condition4 == MISSION_COND_NO_CONDITION) {
						pair1Result =
							(trigger1Result & 1) != 0
								? GOAL_STATE_SUCCESS
								: ((trigger1Result & 2) != 0 ? GOAL_STATE_FAILURE : GOAL_STATE_PENDING);
					} else if (g_missionGlobalGoals[teamIndex][goalKind].triggerPairs[1].trigger1OrTrigger2 ==
							   GOAL_OPERATOR_STR_OR) {
						pair2Result =
							((trigger1Result | trigger2Result) & 1) != 0
								? GOAL_STATE_SUCCESS
								: (((trigger1Result & trigger2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																				: GOAL_STATE_PENDING);
					} else {
						pair2Result =
							((trigger1Result & trigger2Result) & 1) != 0
								? GOAL_STATE_SUCCESS
								: (((trigger1Result | trigger2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																				: GOAL_STATE_PENDING);
					}

					if (g_missionGlobalGoals[teamIndex][goalKind].triggerPair1OrTriggerPair2 ==
						GOAL_OPERATOR_STR_OR) {
						globalState = ((pair1Result | pair2Result) & 1) != 0
										  ? GOAL_STATE_SUCCESS
										  : (((pair1Result & pair2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																					: GOAL_STATE_PENDING);
					} else {
						globalState = ((pair1Result & pair2Result) & 1) != 0
										  ? GOAL_STATE_SUCCESS
										  : (((pair1Result | pair2Result) & 2) != 0 ? GOAL_STATE_FAILURE
																					: GOAL_STATE_PENDING);
					}
				}

				if (goalKind != GOAL_SECONDARY) {
					if (globalState == GOAL_STATE_SUCCESS) {
						sawSuccess = 1;
						if (g_flightMissionState.runtime.teamGlobalGoalState[teamIndex][goalKind] !=
							globalState) {
							g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] +=
								GLOBAL_GOAL_SCORE_SCALE * g_missionGlobalGoals[teamIndex][goalKind].rawPoints;
						}
					} else if (globalState == GOAL_STATE_FAILURE) {
						aggregateState = GOAL_STATE_FAILURE;
					} else if (globalState == GOAL_STATE_PENDING) {
						aggregateState = 0;
					}
				} else if (globalState == GOAL_STATE_SUCCESS) {
					aggregateState = GOAL_STATE_SUCCESS;
					sawSuccess = 1;
					if (g_flightMissionState.runtime.teamGlobalGoalState[teamIndex][goalKind] !=
						globalState) {
						g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] +=
							GLOBAL_GOAL_SCORE_SCALE * g_missionGlobalGoals[teamIndex][goalKind].rawPoints;
					}
				}
				g_flightMissionState.runtime.teamGlobalGoalState[teamIndex][goalKind] = (uint8_t)globalState;

				for (flightGroupIndex = 0; flightGroupIndex < (int16_t)g_missionHeader.numFlightGroups;
					 ++flightGroupIndex) {
					uint16_t goalIndex;

					for (goalIndex = 0; goalIndex < FG_GOAL_COUNT; ++goalIndex) {
						if (g_missionFlightGroups[flightGroupIndex]
									.fg.goals[goalIndex]
									.enabledTeams[teamIndex] != 0 &&
							g_missionFlightGroups[flightGroupIndex].fg.goals[goalIndex].type == goalKind) {
							if (goalKind != GOAL_SECONDARY) {
								if (g_missionFgStats[flightGroupIndex]
										.goalState[FG_GOAL_COUNT * teamIndex + goalIndex] ==
									GOAL_STATE_SUCCESS) {
									sawSuccess = 1;
								} else if (g_missionFgStats[flightGroupIndex]
											   .goalState[FG_GOAL_COUNT * teamIndex + goalIndex] ==
										   GOAL_STATE_FAILURE) {
									aggregateState = GOAL_STATE_FAILURE;
								} else if (g_missionFgStats[flightGroupIndex]
												   .goalState[FG_GOAL_COUNT * teamIndex + goalIndex] ==
											   GOAL_STATE_PENDING &&
										   aggregateState != GOAL_STATE_FAILURE) {
									aggregateState = 0;
								}
							} else if (g_missionFgStats[flightGroupIndex]
										   .goalState[FG_GOAL_COUNT * teamIndex + goalIndex] ==
									   GOAL_STATE_SUCCESS) {
								aggregateState = GOAL_STATE_SUCCESS;
								sawSuccess = 1;
							}
						}
					}
				}

				if (aggregateState == GOAL_STATE_SUCCESS && sawSuccess == 0)
					aggregateState = 0;
				if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && goalKind == GOAL_SECONDARY &&
					aggregateState == GOAL_STATE_SUCCESS &&
					g_flightMissionState.runtime.teamGoalStatus[teamIndex][GOAL_PRIMARY] ==
						GOAL_STATE_SUCCESS) {
					aggregateState = 0;
				}
				if (goalKind == GOAL_PRIMARY && g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
					g_missionHeader.goalsUnimportant != 0) {
					aggregateState = 0;
				}

				{
					uint8_t oldState = g_flightMissionState.runtime.teamGoalStatus[teamIndex][goalKind];

					if (oldState != aggregateState && aggregateState != 0 && oldState != GOAL_STATE_SUCCESS &&
						g_missionHeader.goalsUnimportant == 0) {
						int completedTeamCount = 0;

						g_flightMissionState.runtime.teamGoalStatus[teamIndex][goalKind] =
							(uint8_t)aggregateState;
						if (aggregateState == GOAL_STATE_SUCCESS) {
							for (playerIndex = 0; playerIndex < TEAM_COUNT; ++playerIndex) {
								if (g_flightMissionState.runtime.teamGoalStatus[playerIndex][goalKind] ==
									GOAL_STATE_SUCCESS) {
									++completedTeamCount;
								}
							}
							if (goalKind == GOAL_PRIMARY) {
								int hostileCompletedTeams = 0;

								if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH) {
									int isHostileTeam;
									int otherTeam;

									for (otherTeam = 0; otherTeam < TEAM_COUNT; ++otherTeam) {
										isHostileTeam = teamIndex != otherTeam &&
														g_missionTeams[otherTeam].allies[teamIndex] == 0;
										if (isHostileTeam && g_flightMissionState.runtime
																	 .teamGoalStatus[otherTeam][goalKind] ==
																 GOAL_STATE_SUCCESS) {
											++hostileCompletedTeams;
										}
									}
								}
								if (hostileCompletedTeams == 0) {
									g_flightMissionState.runtime
										.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] += PRIMARY_GOAL_SCORE;
								}
								g_flightMissionState.runtime.teamMissionCompletionTimeSeconds[teamIndex] =
									(int)missionElapsedSeconds;
								g_flightMissionState.runtime.globalPrimaryGoalStatus = GOAL_STATE_SUCCESS;
							} else if (goalKind == GOAL_BONUS) {
								g_flightMissionState.runtime.globalBonusGoalStatus = GOAL_STATE_SUCCESS;
								g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] +=
									BONUS_GOAL_SCORE;
							}
							if (goalKind == GOAL_PRIMARY) {
								int teamPlayerSlot = -1;

								for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
									if (g_players[playerIndex].connectedFlag != 0 &&
										(uint16_t)g_players[playerIndex].playerIff == teamIndex) {
										teamPlayerSlot = playerIndex;
									}
								}
								for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
									if (g_players[playerIndex].connectedFlag == 0)
										continue;
									if ((uint16_t)g_players[playerIndex].playerIff == teamIndex) {
										uint8_t announceVictory;

										g_players[playerIndex].missionStats.field_14 = completedTeamCount;
										g_msgSenderIff = g_players[playerIndex].iff;
										announceVictory = 1;
										if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH &&
											g_flightMissionState.runtime
													.teamGoalStatus[teamIndex][GOAL_SECONDARY] ==
												GOAL_STATE_SUCCESS) {
											announceVictory = 0;
										}
										if (g_pilotData.missionDirectoryId ==
												MISSION_DIRECTORY_TRAINING_EXERCISES &&
											g_pilotData.missionSequenceActive == 1 &&
											g_flightMissionState.runtime
													.teamGoalStatus[teamIndex][GOAL_SECONDARY] ==
												GOAL_STATE_SUCCESS) {
											announceVictory = 0;
										}
										if (announceVictory != 0) {
											msg_emitInFlightMessage(
												IFMSG_193_EXCELLENT_JOB_PRIMARY_MISSION_OBJECTIVES_COMPLETED,
												playerIndex);
											if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
												g_flightPlayerCount > 1) {
												g_msgArgTable[0] =
													completedTeamCount + MESSAGE_ARGUMENT_SCORE_BASE;
												msg_emitInFlightMessage(
													IFMSG_305_YOU_ARE_THE_ARG_TO_COMPLETE_YOUR_MISSION_GOALS,
													playerIndex);
											}
											if (playerIndex == g_localPlayer) {
												uint16_t teamMessageIndex;
												for (teamMessageIndex = 0;
													 teamMessageIndex < TEAM_MESSAGE_COUNT;
													 ++teamMessageIndex) {
													if (g_missionTeams[teamIndex]
															.endOfMissionMessages[PRIMARY_TEAM_MESSAGE +
																				  teamMessageIndex][0] ==
														'\0') {
														continue;
													}
													msg_addMessagePtr(
														0, g_missionTeams[teamIndex]
															   .endOfMissionMessages[PRIMARY_TEAM_MESSAGE +
																					 teamMessageIndex]);
													g_pendingHudMessageVoiceSfxId = PRIMARY_TEAM_VOICE;
													if (teamMessageIndex != 0)
														g_pendingHudMessageVoiceSfxId = 0;
													if ((uint8_t)g_missionTeams[teamIndex]
																.endOfMissionMessages[PRIMARY_TEAM_MESSAGE +
																					  teamMessageIndex][0] >=
															MESSAGE_DIGIT_FIRST &&
														(uint8_t)g_missionTeams[teamIndex]
																.endOfMissionMessages[PRIMARY_TEAM_MESSAGE +
																					  teamMessageIndex][0] <=
															MESSAGE_DIGIT_LAST) {
														msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT,
																				playerIndex);
													} else {
														msg_emitInFlightMessage(IFMSG_196_CODE_02_ARGUMENT,
																				playerIndex);
													}
												}
												if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH)
													fsfx_QueueCommanderVoiceCategory(1, -1);
												else
													fsfx_QueueCommanderVoiceCategory(0, -1);
											}
										} else if (teamIndex == 0) {
											msg_emitInFlightMessage(
												IFMSG_208_GOOD_WORK_OUR_FORCES_HAVE_BOUNCED_BACK_TO_PULL_OUT_A_DRAW,
												playerIndex);
										} else {
											msg_emitInFlightMessage(
												IFMSG_209_GOOD_WORK_THE_ALLIANCE_HAS_RALLIED_TO_GAIN_A_DRAW_FROM_THIS_MISSION,
												playerIndex);
										}
									} else {
										g_msgArgTable[1] = completedTeamCount + MESSAGE_ARGUMENT_SCORE_BASE;
										if ((g_missionHeader.missionType == MISSION_TYPE_QUICK_START ||
											 g_missionHeader.missionType == MISSION_TYPE_SIMULATOR_1) &&
											teamPlayerSlot != -1) {
											msg_addMessagePtr(0, NetSession_GetPlayerName(teamPlayerSlot));
											msg_emitInFlightMessage(
												IFMSG_306_ARG_IS_THE_ARG_TO_COMPLETE_HIS_MISSION_GOALS,
												playerIndex);
										}
									}
								}
							} else if (goalKind == GOAL_SECONDARY) {
								for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
									if (g_players[playerIndex].connectedFlag != 0 &&
										(uint16_t)g_players[playerIndex].playerIff == teamIndex &&
										g_flightMissionState.runtime
												.teamGoalStatus[teamIndex][GOAL_PRIMARY] !=
											GOAL_STATE_FAILURE &&
										playerIndex == g_localPlayer) {
										uint8_t announceFailure = 1;
										uint16_t teamMessageIndex;

										if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH &&
											g_flightMissionState.runtime
													.teamGoalStatus[teamIndex][GOAL_PRIMARY] ==
												GOAL_STATE_SUCCESS) {
											announceFailure = 0;
										}
										if (announceFailure != 0) {
											g_msgSenderIff = g_players[playerIndex].iff;
											msg_emitInFlightMessage(
												IFMSG_206_MISSION_OBJECTIVES_CANNOT_BE_FINISHED_ABORT_MISSION,
												playerIndex);
											for (teamMessageIndex = 0; teamMessageIndex < TEAM_MESSAGE_COUNT;
												 ++teamMessageIndex) {
												if (g_missionTeams[teamIndex]
														.endOfMissionMessages[FAILED_TEAM_MESSAGE +
																			  teamMessageIndex][0] == '\0') {
													continue;
												}
												msg_addMessagePtr(
													0, g_missionTeams[teamIndex]
														   .endOfMissionMessages[FAILED_TEAM_MESSAGE +
																				 teamMessageIndex]);
												g_pendingHudMessageVoiceSfxId = FAILED_TEAM_VOICE;
												if (teamMessageIndex != 0)
													g_pendingHudMessageVoiceSfxId = 0;
												if ((uint8_t)g_missionTeams[teamIndex]
															.endOfMissionMessages[FAILED_TEAM_MESSAGE +
																				  teamMessageIndex][0] >=
														MESSAGE_DIGIT_FIRST &&
													(uint8_t)g_missionTeams[teamIndex]
															.endOfMissionMessages[FAILED_TEAM_MESSAGE +
																				  teamMessageIndex][0] <=
														MESSAGE_DIGIT_LAST) {
													msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT,
																			playerIndex);
												} else {
													msg_emitInFlightMessage(IFMSG_196_CODE_02_ARGUMENT,
																			playerIndex);
												}
											}
											if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH)
												fsfx_QueueCommanderVoiceCategory(7, -1);
											else
												fsfx_QueueCommanderVoiceCategory(6, -1);
										} else if (teamIndex == 0) {
											msg_emitInFlightMessage(
												IFMSG_210_WE_HAVE_LOST_OUR_VICTORY_THE_REBELS_HAVE_GAINED_A_DRAW,
												playerIndex);
										} else {
											msg_emitInFlightMessage(
												IFMSG_211_WE_HAVE_LOST_OUR_VICTORY_THE_EMPIRE_HAS_GAINED_A_DRAW,
												playerIndex);
										}
									}
								}
							} else {
								for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
									if (g_players[playerIndex].connectedFlag != 0 &&
										(uint16_t)g_players[playerIndex].playerIff == teamIndex &&
										playerIndex == g_localPlayer) {
										uint16_t teamMessageIndex;
										for (teamMessageIndex = 0; teamMessageIndex < TEAM_MESSAGE_COUNT;
											 ++teamMessageIndex) {
											if (g_missionTeams[teamIndex]
													.endOfMissionMessages[BONUS_TEAM_MESSAGE +
																		  teamMessageIndex][0] == '\0') {
												continue;
											}
											msg_addMessagePtr(0,
															  g_missionTeams[teamIndex]
																  .endOfMissionMessages[BONUS_TEAM_MESSAGE +
																						teamMessageIndex]);
											g_pendingHudMessageVoiceSfxId = BONUS_TEAM_VOICE;
											if (teamMessageIndex != 0)
												g_pendingHudMessageVoiceSfxId = 0;
											g_msgSenderIff = g_players[playerIndex].iff;
											if ((uint8_t)g_missionTeams[teamIndex]
														.endOfMissionMessages[BONUS_TEAM_MESSAGE +
																			  teamMessageIndex][0] >=
													MESSAGE_DIGIT_FIRST &&
												(uint8_t)g_missionTeams[teamIndex]
														.endOfMissionMessages[BONUS_TEAM_MESSAGE +
																			  teamMessageIndex][0] <=
													MESSAGE_DIGIT_LAST) {
												msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT,
																		playerIndex);
											} else {
												msg_emitInFlightMessage(IFMSG_196_CODE_02_ARGUMENT,
																		playerIndex);
											}
										}
									}
								}
							}
						} else if (aggregateState == GOAL_STATE_FAILURE && goalKind == GOAL_PRIMARY) {
							for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
								if (g_players[playerIndex].connectedFlag != 0 &&
									(uint16_t)g_players[playerIndex].playerIff == teamIndex &&
									g_flightMissionState.runtime.teamGoalStatus[teamIndex][GOAL_SECONDARY] !=
										GOAL_STATE_SUCCESS &&
									playerIndex == g_localPlayer) {
									uint16_t teamMessageIndex;

									g_msgSenderIff = g_players[playerIndex].iff;
									msg_emitInFlightMessage(
										IFMSG_206_MISSION_OBJECTIVES_CANNOT_BE_FINISHED_ABORT_MISSION,
										playerIndex);
									for (teamMessageIndex = 0; teamMessageIndex < TEAM_MESSAGE_COUNT;
										 ++teamMessageIndex) {
										if (g_missionTeams[teamIndex]
												.endOfMissionMessages[FAILED_TEAM_MESSAGE + teamMessageIndex]
																	 [0] == '\0') {
											continue;
										}
										msg_addMessagePtr(0, g_missionTeams[teamIndex]
																 .endOfMissionMessages[FAILED_TEAM_MESSAGE +
																					   teamMessageIndex]);
										g_pendingHudMessageVoiceSfxId = FAILED_TEAM_VOICE;
										if (teamMessageIndex != 0)
											g_pendingHudMessageVoiceSfxId = 0;
										if ((uint8_t)g_missionTeams[teamIndex]
													.endOfMissionMessages[FAILED_TEAM_MESSAGE +
																		  teamMessageIndex][0] >=
												MESSAGE_DIGIT_FIRST &&
											(uint8_t)g_missionTeams[teamIndex]
													.endOfMissionMessages[FAILED_TEAM_MESSAGE +
																		  teamMessageIndex][0] <=
												MESSAGE_DIGIT_LAST) {
											msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT, playerIndex);
										} else {
											msg_emitInFlightMessage(IFMSG_196_CODE_02_ARGUMENT, playerIndex);
										}
									}
									if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH)
										fsfx_QueueCommanderVoiceCategory(7, -1);
									else
										fsfx_QueueCommanderVoiceCategory(6, -1);
								}
							}
						}
					}
				}
			}
		}
		g_flightGlobalCountdownTimers.missionGoalEvaluationTimer = SIMULATION_TICKS_PER_SECOND;
	}

	if ((int16_t)g_flightGlobalCountdownTimers.missionMessageScanTimer > 0)
		return;

	for (messageIndex = 0; messageIndex < (int16_t)g_missionHeader.numMessages; ++messageIndex) {
		if (g_flightMissionState.messageTriggered[messageIndex] == 0) {
			int16_t triggerPair1 =
				(int16_t)Mission_EvaluateTriggerPair(&g_missionMessages[messageIndex].triggerPairs[0], 0);
			int16_t triggerPair2 =
				(int16_t)Mission_EvaluateTriggerPair(&g_missionMessages[messageIndex].triggerPairs[1], 0);
			int16_t triggered =
				g_missionMessages[messageIndex].triggerPair1OrTriggerPair2 == GOAL_OPERATOR_STR_OR
					? triggerPair1 | triggerPair2
					: triggerPair1 & triggerPair2;

			if ((triggered & 1) == 0)
				continue;
			g_flightMissionState.messageTriggered[messageIndex] = 1;
			g_flightMissionState.messageDelayCountdown[messageIndex] =
				g_missionMessages[messageIndex].rawDelay;
			if (g_missionMessages[messageIndex].rawDelay != 0 ||
				g_missionMessages[messageIndex].sentToTeam[(uint16_t)g_players[g_localPlayer].playerIff] ==
					0) {
				continue;
			}
			msg_addMessagePtr(0, &g_missionMessages[messageIndex]);
			if (messageIndex < MISSION_MESSAGE_VOICE_COUNT)
				g_pendingHudMessageVoiceSfxId = messageIndex + MISSION_MESSAGE_VOICE_BASE;
			else
				g_pendingHudMessageVoiceSfxId = 0;
			msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT, g_localPlayer);
		} else {
			if (g_flightMissionState.messageDelayCountdown[messageIndex] == 0)
				continue;
			--g_flightMissionState.messageDelayCountdown[messageIndex];
			if (g_flightMissionState.messageDelayCountdown[messageIndex] != 0 ||
				g_missionMessages[messageIndex].sentToTeam[(uint16_t)g_players[g_localPlayer].playerIff] ==
					0) {
				continue;
			}
			msg_addMessagePtr(0, &g_missionMessages[messageIndex]);
			if (messageIndex < MISSION_MESSAGE_VOICE_COUNT)
				g_pendingHudMessageVoiceSfxId = messageIndex + MISSION_MESSAGE_VOICE_BASE;
			else
				g_pendingHudMessageVoiceSfxId = 0;
			msg_emitInFlightMessage(IFMSG_207_CODE_01_ARGUMENT, g_localPlayer);
		}
	}
	g_flightGlobalCountdownTimers.missionMessageScanTimer = MISSION_MESSAGE_REFRESH_TICKS;
}

// FUNCTION: XVT 0x431B50
int Mission_EvaluateTriggerPair(const MissionTriggerPair* triggerPair, int16_t flightGroupIdx) {
	enum { MISSION_TEAM_COUNT = 10 };

	int teamOrVariable;
	int trigger1Result;
	int trigger2Result;

	teamOrVariable = MISSION_TEAM_COUNT;
	if (triggerPair->triggers[1].condition == MISSION_COND_NO_CONDITION &&
		triggerPair->triggers[1].variableType == GOAL_TARGET_TEAM) {
		teamOrVariable = triggerPair->triggers[1].variable;
	}

	trigger1Result = (uint16_t)Mission_EvaluateCondition(
		triggerPair->triggers[0].condition, triggerPair->triggers[0].variableType,
		triggerPair->triggers[0].variable, (uint8_t)triggerPair->triggers[0].amount, flightGroupIdx,
		(uint16_t)teamOrVariable);
	if (triggerPair->triggers[1].condition != MISSION_COND_NO_CONDITION) {
		trigger2Result = (uint16_t)Mission_EvaluateCondition(
			triggerPair->triggers[1].condition, triggerPair->triggers[1].variableType,
			triggerPair->triggers[1].variable, (uint8_t)triggerPair->triggers[1].amount, flightGroupIdx,
			MISSION_TEAM_COUNT);
	} else {
		trigger2Result = 0;
	}

	if (triggerPair->trigger1OrTrigger2 == GOAL_OPERATOR_STR_OR ||
		triggerPair->triggers[1].condition == MISSION_COND_NO_CONDITION) {
		return trigger1Result | trigger2Result;
	}
	return trigger1Result & trigger2Result;
}

// FUNCTION: XVT 0x431C10
int16_t Mission_EvaluateCondition(uint16_t conditionType, int16_t variableType, uint16_t variable,
								  int16_t amountType, int16_t includeDepartedAsDestroyed,
								  uint16_t teamOrVariable) {
	enum {
		MISSION_TEAM_COUNT = 10,
		MISSION_IFF_COUNT = 6,
		MISSION_PLAYER_COUNT = 8,
	};

	uint16_t met;
	uint16_t metSpecialCargo;
	uint16_t failed;
	uint16_t failedSpecialCargo;
	uint16_t total;
	uint16_t totalSpecialCargo;
	uint16_t subsetTotal;
	int16_t status;
	uint16_t flightGroupIdx;

	g_missionConditionCurrentCount = 0;
	g_missionConditionTotalCount = 0;
	if (g_missionConditionUsesCountByTriggerType[conditionType] != 0) {
		if (variableType == GOAL_TARGET_NONE) {
			return 2;
		}

		total = 0;
		totalSpecialCargo = 0;
		subsetTotal = 0;
		met = 0;
		metSpecialCargo = 0;
		failed = 0;
		failedSpecialCargo = 0;
		for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIdx) {
			uint16_t specialCargoOutcome1;

			if (g_missionFlightGroups[flightGroupIdx].fg.craftType == CRAFT_SPECIES_UNKNOWN ||
				Mission_FlightGroupMatchesTriggerVariable(flightGroupIdx, variableType, variable) == 0) {
				continue;
			}

			total += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL];
			subsetTotal += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
			totalSpecialCargo +=
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL];
			specialCargoOutcome1 =
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];
			switch ((MissionConditionType)conditionType) {
				case MISSION_COND_ARRIVED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
					metSpecialCargo += specialCargoOutcome1;
					break;

				case MISSION_COND_DESTROYED:
					met += g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED];
					if (includeDepartedAsDestroyed == 0) {
						failed +=
							g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
						failedSpecialCargo += g_missionFgStats[flightGroupIdx]
												  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
					} else {
						met +=
							g_missionFgStats[flightGroupIdx]
								.outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx]
								.outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx]
								.outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
						metSpecialCargo +=
							g_missionFgStats[flightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] +
							g_missionFgStats[flightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
					}
					break;

				case MISSION_COND_ATTACKED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ATTACKED];
					failed +=
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ATTACKED];
					failedSpecialCargo += g_missionFgStats[flightGroupIdx]
											  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED];
					break;

				case MISSION_COND_CAPTURED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED];
					failed +=
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED];
					failedSpecialCargo += g_missionFgStats[flightGroupIdx]
											  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED];
					break;

				case MISSION_COND_INSPECTED:
					if (teamOrVariable < MISSION_TEAM_COUNT) {
						met += g_missionFgStats[flightGroupIdx].teamInspected[teamOrVariable];
						metSpecialCargo +=
							g_missionFgStats[flightGroupIdx].teamSpecialCargoInspected[teamOrVariable];
						failed += g_missionFgStats[flightGroupIdx].teamUninspectedLost[teamOrVariable];
						failedSpecialCargo +=
							g_missionFgStats[flightGroupIdx].teamSpecialCargoUninspectedLost[teamOrVariable];
					} else {
						met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_INSPECTED];
						failed +=
							g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED];
						metSpecialCargo += g_missionFgStats[flightGroupIdx]
											   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_INSPECTED];
						failedSpecialCargo += g_missionFgStats[flightGroupIdx]
												  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED];
					}
					break;

				case MISSION_COND_BOARDED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_BOARDED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_BOARDED];
					failedSpecialCargo += g_missionFgStats[flightGroupIdx]
											  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED];
					break;

				case MISSION_COND_DOCKED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DOCKED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DOCKED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DOCKED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DOCKED];
					break;

				case MISSION_COND_DISABLED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DISABLED];
					failed +=
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DISABLED];
					failedSpecialCargo += g_missionFgStats[flightGroupIdx]
											  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED];
					break;

				case MISSION_COND_SURVIVED:
					met = g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] -
						  g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						  met -
						  g_missionFgStats[flightGroupIdx]
							  .outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP];
					metSpecialCargo =
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] -
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						metSpecialCargo -
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED];
					failed += g_missionFgStats[flightGroupIdx]
								  .outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
							  g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED];
					break;

				case MISSION_COND_DEPARTED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED];
					failed +=
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ABORTED] +
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ABORTED] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
					break;

				case MISSION_COND_SHIELDS_DEPLETED:
				case MISSION_COND_HULL_ABOVE_50:
				case MISSION_COND_NO_WARHEADS:
				case MISSION_COND_SYSTEM_DAMAGED:
				case MISSION_COND_SHIELDS_BELOW_50:
				case MISSION_COND_SHIELDS_BELOW_25:
				case MISSION_COND_HULL_ABOVE_25:
				case MISSION_COND_HULL_ABOVE_75: {
					int objectIdx;

					for (objectIdx = g_activeRegionObjectSlotStart;
						 objectIdx < g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
						ObjectRecord* object;
						CraftData* craft;
						int maxShield;
						int16_t matchesCondition;

						object = &g_objectTable[objectIdx];
						if (object->objectType == 0 || object->flightGroupIdx != flightGroupIdx) {
							continue;
						}
						craft = object->mobj->pCraft;
						matchesCondition = 0;
						maxShield = 2 * g_modelDefs[craft->modelIndex].shieldStrength;
						if (conditionType == MISSION_COND_SHIELDS_DEPLETED) {
							if (craft->shieldEnergy[0] + craft->shieldEnergy[1] <= 0) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_SHIELDS_BELOW_25) {
							if (craft->shieldEnergy[0] + craft->shieldEnergy[1] <= maxShield / 4) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_SHIELDS_BELOW_50) {
							if (craft->shieldEnergy[0] + craft->shieldEnergy[1] <= maxShield / 2) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_HULL_ABOVE_50) {
							if ((craft->hullMax >> 1) < craft->hullDamage) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_HULL_ABOVE_25) {
							if ((craft->hullMax >> 2) < craft->hullDamage) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_HULL_ABOVE_75) {
							if (3 * (craft->hullMax >> 2) < craft->hullDamage) {
								matchesCondition = 1;
							}
						} else if (conditionType == MISSION_COND_NO_WARHEADS) {
							const ModelDef* modelDef;
							uint16_t launcherIdx;
							uint16_t warheadCount;

							modelDef = &g_modelDefs[craft->modelIndex];
							warheadCount = 0;
							for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount; ++launcherIdx) {
								warheadCount +=
									craft->weaponSlots[modelDef->warheadLauncherFirstSlot[launcherIdx]].count;
								warheadCount +=
									craft->weaponSlots[modelDef->warheadLauncherLastSlot[launcherIdx]].count;
							}
							if (warheadCount == 0) {
								matchesCondition = 1;
							}
						} else if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_CANNONS) == 0) {
							matchesCondition = 1;
						}

						if (matchesCondition != 0) {
							++met;
							if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft ==
								craft->waveNumber) {
								++metSpecialCargo;
							}
						} else {
							++failed;
							if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft ==
								craft->waveNumber) {
								++failedSpecialCargo;
							}
						}
					}
					break;
				}

				case MISSION_COND_NOT_ARRIVED:
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
					failedSpecialCargo += specialCargoOutcome1;
					break;

				case MISSION_COND_NOT_ATTACKED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ATTACKED];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ATTACKED];
					break;

				case MISSION_COND_NOT_DISABLED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DISABLED];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DISABLED];
					break;

				case MISSION_COND_NOT_CAPTURED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED];
					break;

				case MISSION_COND_NOT_INSPECTED:
					if (teamOrVariable < MISSION_TEAM_COUNT) {
						met += g_missionFgStats[flightGroupIdx].teamUninspectedLost[teamOrVariable];
						metSpecialCargo +=
							g_missionFgStats[flightGroupIdx].teamSpecialCargoUninspectedLost[teamOrVariable];
						failed += g_missionFgStats[flightGroupIdx].teamInspected[teamOrVariable];
						failedSpecialCargo +=
							g_missionFgStats[flightGroupIdx].teamSpecialCargoInspected[teamOrVariable];
					} else {
						met +=
							g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED];
						failed +=
							g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_INSPECTED];
						metSpecialCargo += g_missionFgStats[flightGroupIdx]
											   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED];
						failedSpecialCargo += g_missionFgStats[flightGroupIdx]
												  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_INSPECTED];
					}
					break;

				case MISSION_COND_COMPLETED_MISSION:
					met +=
						g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_COMPLETED_MISSION];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_COMPLETED_MISSION];
					break;

				case MISSION_COND_NOT_BOARDED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_BOARDED];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_BOARDED];
					break;

				case MISSION_COND_FAILED_MISSION:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_FAILED_MISSION];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_FAILED_MISSION];
					break;

				case MISSION_COND_NOT_DOCKED:
					met += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DOCKED];
					failed += g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DOCKED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DOCKED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DOCKED];
					break;

				case MISSION_COND_DESTROYED_OR_DEPARTED:
					met += g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						   g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] +
						   g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] +
						   g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LEFT_REGION];
					break;

				case MISSION_COND_ORDER_COMPLETED:
					if (teamOrVariable < MISSION_TEAM_COUNT) {
						met += g_missionFgStats[flightGroupIdx].teamCondition44Count[teamOrVariable];
						metSpecialCargo +=
							g_missionFgStats[flightGroupIdx].teamCondition44SpecialCargo[teamOrVariable];
						failed +=
							g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount[teamOrVariable];
						failedSpecialCargo += g_missionFgStats[flightGroupIdx]
												  .teamCondition44OtherTeamSpecialCargo[teamOrVariable];
					} else {
						uint16_t teamIdx;

						for (teamIdx = 0; teamIdx < MISSION_TEAM_COUNT; ++teamIdx) {
							met += g_missionFgStats[flightGroupIdx].teamCondition44Count[teamIdx];
							metSpecialCargo +=
								g_missionFgStats[flightGroupIdx].teamCondition44SpecialCargo[teamIdx];
							if (g_flightMissionState.runtime.teamHasCountableCraft[teamIdx] != 0) {
								failed +=
									g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount[teamIdx];
								failedSpecialCargo += g_missionFgStats[flightGroupIdx]
														  .teamCondition44OtherTeamSpecialCargo[teamIdx];
							}
						}
					}
					break;

				case MISSION_COND_DESTROYED_OR_CAPTURED:
					met += g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ABORTED] +
						   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
					metSpecialCargo +=
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DESTROYED] +
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ABORTED] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
					failed += g_missionFgStats[flightGroupIdx]
								  .outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE] +
							  g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED] +
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE];
					break;

				case MISSION_COND_CAPTURED_BY_DESTINATION:
					met += g_missionFgStats[flightGroupIdx]
							   .outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_BY_DESTINATION];
					failed += g_missionFgStats[flightGroupIdx]
								  .outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION];
					metSpecialCargo += g_missionFgStats[flightGroupIdx]
										   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED_BY_DESTINATION];
					failedSpecialCargo +=
						g_missionFgStats[flightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION];
					break;

				default:
					break;
			}
		}

		status = 4;
		if (total != 0) {
			switch ((uint16_t)amountType) {
				case GOAL_AMT_100:
					if (met >= total) {
						status = 1;
					} else if (failed != 0) {
						status = 2;
					}
					break;
				case GOAL_AMT_75:
					if (MATH2_divide(met, total) >= 0xc000u) {
						status = 1;
					} else if (MATH2_divide(failed, total) > 0x4000u) {
						status = 2;
					}
					break;
				case GOAL_AMT_50:
					if (MATH2_divide(met, total) >= 0x8000u) {
						status = 1;
					} else if (MATH2_divide(failed, total) > 0x8000u) {
						status = 2;
					}
					break;
				case GOAL_AMT_25:
					if (MATH2_divide(met, total) >= 0x4000u) {
						status = 1;
					} else if (MATH2_divide(failed, total) > 0xc000u) {
						status = 2;
					}
					break;
				case GOAL_AMT_AT_LEAST_1:
				case GOAL_AMT_AT_LEAST_1_ALT:
					if (met != 0) {
						status = 1;
					} else if (failed == total) {
						status = 2;
					}
					break;
				case GOAL_AMT_ALL_BUT_1:
					if ((uint16_t)(total - 1) <= met) {
						status = 1;
					} else if (failed > 1) {
						status = 2;
					}
					break;
				case GOAL_AMT_ALL_SPECIAL_CARGO:
					if (totalSpecialCargo != 0) {
						if (metSpecialCargo == totalSpecialCargo) {
							status = 1;
						} else if (failedSpecialCargo != 0) {
							status = 2;
						}
					}
					break;
				case GOAL_AMT_ALL_NON_SPECIAL:
					if (total - met == totalSpecialCargo) {
						status = 1;
					} else if ((failed != 0 && metSpecialCargo == 0) ||
							   (failed > 1 && metSpecialCargo != 0)) {
						status = 2;
					}
					break;
				case GOAL_AMT_100_OF_SUBSET:
					if (met >= subsetTotal) {
						status = 1;
					}
					break;
				case GOAL_AMT_75_OF_SUBSET:
					if (MATH2_divide(met, subsetTotal) >= 0xc000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_50_OF_SUBSET:
					if (MATH2_divide(met, subsetTotal) >= 0x8000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_25_OF_SUBSET:
					if (MATH2_divide(met, subsetTotal) >= 0x4000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_ALL_BUT_1_OF_SUBSET:
					if (subsetTotal - 1 <= met) {
						status = 1;
					}
					break;
				case GOAL_AMT_66:
					if (MATH2_divide(met, total) >= 0xaaaau) {
						status = 1;
					} else if (MATH2_divide(failed, total) > 0x5555u) {
						status = 2;
					}
					break;
				case GOAL_AMT_33:
					if (MATH2_divide(met, total) >= 0x5555u) {
						status = 1;
					} else if (MATH2_divide(failed, total) > 0xaaaau) {
						status = 2;
					}
					break;
				default:
					break;
			}
		}
		g_missionConditionCurrentCount = met;
		g_missionConditionTotalCount = total;
		return status;
	}

	status = 4;
	switch ((MissionConditionType)conditionType) {
		case MISSION_COND_ALWAYS_TRUE:
			status = 1;
			break;
		case MISSION_COND_NEVER_FALSE:
			status = 0;
			break;
		case MISSION_COND_PRIMARY_GOAL_COMPLETE: {
			uint8_t goalStatus;

			if (variableType == GOAL_TARGET_TEAM) {
				goalStatus = g_flightMissionState.runtime.teamGoalStatus[variable][0];
			} else {
				goalStatus = g_flightMissionState.runtime.globalPrimaryGoalStatus;
			}
			if (goalStatus == 1) {
				status = 1;
			} else if (goalStatus == 2) {
				status = 2;
			}
			break;
		}
		case MISSION_COND_PRIMARY_GOAL_FAILED: {
			uint8_t goalStatus;

			if (variableType == GOAL_TARGET_TEAM) {
				goalStatus = g_flightMissionState.runtime.teamGoalStatus[variable][0];
			} else {
				goalStatus = g_flightMissionState.runtime.globalPrimaryGoalStatus;
			}
			if (goalStatus == 2) {
				status = 1;
			} else if (goalStatus == 1) {
				status = 2;
			}
			break;
		}
		case MISSION_COND_BONUS_GOAL_COMPLETE: {
			uint8_t goalStatus;

			if (variableType == GOAL_TARGET_TEAM) {
				goalStatus = g_flightMissionState.runtime.teamGoalStatus[variable][2];
				if (goalStatus == 2) {
					status = 1;
				} else if (goalStatus == 1) {
					status = 2;
				}
			} else {
				goalStatus = g_flightMissionState.runtime.globalBonusGoalStatus;
				if (goalStatus == 1) {
					status = 1;
				} else if (goalStatus == 2) {
					status = 2;
				}
			}
			break;
		}
		case MISSION_COND_BONUS_GOAL_FAILED:
			if (g_flightMissionState.runtime.globalBonusGoalStatus == 2) {
				status = 1;
			} else if (g_flightMissionState.runtime.globalBonusGoalStatus == 1) {
				status = 2;
			}
			break;
		case MISSION_COND_REINFORCEMENT_NOT_CALLED:
			status = (g_flightMissionState.runtime.teamActiveGoalSequence[variable] == 0) + 1;
			break;
		case MISSION_COND_ALWAYS_PENDING:
			status = 2;
			break;

		case MISSION_COND_PLAYER_CONNECTED:
		case MISSION_COND_PLAYER_DISCONNECTED: {
			uint16_t connectedByIff[MISSION_IFF_COUNT];
			uint16_t connectedByTeam[MISSION_TEAM_COUNT];
			uint16_t playerCraftByIff[MISSION_IFF_COUNT];
			uint16_t playerCraftByTeam[MISSION_TEAM_COUNT];
			uint16_t playerIdx;
			uint16_t totalPlayers;
			uint16_t currentPlayers;

			status = 2;

			for (playerIdx = 0; playerIdx < MISSION_TEAM_COUNT; ++playerIdx) {
				connectedByTeam[playerIdx] = 0;
				playerCraftByTeam[playerIdx] = 0;
			}
			for (playerIdx = 0; playerIdx < MISSION_IFF_COUNT; ++playerIdx) {
				connectedByIff[playerIdx] = 0;
				playerCraftByIff[playerIdx] = 0;
			}
			for (playerIdx = 0; playerIdx < MISSION_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag != 0) {
					++connectedByTeam[(uint16_t)g_players[playerIdx].playerIff];
					++connectedByIff[(uint16_t)g_players[playerIdx].iff];
				}
			}
			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
					++playerCraftByTeam[g_missionFlightGroups[flightGroupIdx].fg.team];
					++playerCraftByIff[g_missionFlightGroups[flightGroupIdx].fg.iff];
				}
			}

			if (variableType == GOAL_TARGET_PLAYER_NUMBER) {
				if (conditionType == MISSION_COND_PLAYER_CONNECTED) {
					for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
						 ++flightGroupIdx) {
						uint8_t playerNumber;

						playerNumber = g_missionFlightGroups[flightGroupIdx].fg.playerNumber;
						if (playerNumber != 0 && playerNumber - variable == 1 &&
							g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1) {
							status = 1;
						}
					}
				} else {
					for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
						 ++flightGroupIdx) {
						uint8_t playerNumber;

						playerNumber = g_missionFlightGroups[flightGroupIdx].fg.playerNumber;
						if (playerNumber != 0 && playerNumber - variable == 1 &&
							g_missionFlightGroups[flightGroupIdx].playerOwnerIdx == -1) {
							status = 1;
						}
					}
				}
				break;
			}

			totalPlayers = 0;
			currentPlayers = 0;
			if (variableType == GOAL_TARGET_TEAM) {
				totalPlayers = playerCraftByTeam[variable];
				if (conditionType == MISSION_COND_PLAYER_CONNECTED) {
					currentPlayers = connectedByTeam[variable];
				} else {
					currentPlayers = totalPlayers - connectedByTeam[variable];
				}
			} else if (variableType == GOAL_TARGET_IFF) {
				totalPlayers = playerCraftByIff[variable];
				if (conditionType == MISSION_COND_PLAYER_CONNECTED) {
					currentPlayers = connectedByIff[variable];
				} else {
					currentPlayers = totalPlayers - connectedByIff[variable];
				}
			}

			switch ((uint16_t)amountType) {
				case GOAL_AMT_100:
					if (currentPlayers == totalPlayers) {
						status = 1;
					}
					break;
				case GOAL_AMT_75:
					if (MATH2_divide(currentPlayers, totalPlayers) >= 0xc000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_50:
					if (MATH2_divide(currentPlayers, totalPlayers) >= 0x8000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_25:
					if (MATH2_divide(currentPlayers, totalPlayers) >= 0x4000u) {
						status = 1;
					}
					break;
				case GOAL_AMT_AT_LEAST_1:
					if (currentPlayers != 0) {
						status = 1;
					}
					break;
				case GOAL_AMT_ALL_BUT_1:
					if (totalPlayers - currentPlayers == 1) {
						status = 1;
					}
					break;
				case GOAL_AMT_66:
					if (MATH2_divide(currentPlayers, totalPlayers) >= 0xaaaau) {
						status = 1;
					}
					break;
				case GOAL_AMT_33:
					if (MATH2_divide(currentPlayers, totalPlayers) >= 0x5555u) {
						status = 1;
					}
					break;
				default:
					break;
			}
			break;
		}

		default:
			break;
	}
	return status;
}

// FUNCTION: XVT 0x432FA0
int16_t Mission_FlightGroupMatchesTriggerVariable(uint16_t flightGroupIdx, int16_t variableType,
												  uint16_t variable) {
	uint8_t objectType;
	uint16_t triggerVariableType;
	int16_t result;

	result = 0;
	objectType = g_craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];
	triggerVariableType = (uint16_t)variableType;
	switch (triggerVariableType) {
		case 0:
			return result;

		case 1:
			if (flightGroupIdx == variable) {
				result = 1;
				return result;
			}
			break;

		case 2:
			if (g_craftTypeToObjectType[variable + 1] == objectType) {
				result = 1;
				return result;
			}
			break;

		case 3:
			if (g_modelTypeTable[objectType].genusId == g_genusConvert[variable]) {
				result = 1;
				return result;
			}
			break;

		case 4:
			if ((uint8_t)g_modelTypeTable[objectType].familyId == g_familyConvert[variable]) {
				result = 1;
				return result;
			}
			break;

		case 5:
			if (g_missionFlightGroups[flightGroupIdx].fg.iff == variable) {
				result = 1;
				return result;
			}
			break;

		case 6:
			if (g_missionFlightGroups[flightGroupIdx].fg.orders[0].order == variable) {
				result = 1;
				return result;
			}
			break;

		case 7:
			result = 0;
			if (variable == 9) {
				if (g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1) {
					result = 1;
					return result;
				}
				break;
			}
			if (variable == 10) {
				if (g_missionFlightGroups[flightGroupIdx].playerOwnerIdx == -1) {
					result = 1;
					return result;
				}
				break;
			}
			result = 1;
			break;

		case 8:
			if (g_missionFlightGroups[flightGroupIdx].fg.globalGroup == variable) {
				result = 1;
				return result;
			}
			break;

		case 9:
			if (g_missionFlightGroups[flightGroupIdx].fg.groupAI == variable) {
				result = 1;
				return result;
			}
			break;

		case 10:
			if (g_missionFlightGroups[flightGroupIdx].fg.status1 == variable) {
				result = 1;
				return result;
			}
			break;

		case 11:
			result = 1;
			break;

		case 12:
			if (g_missionFlightGroups[flightGroupIdx].fg.team == variable) {
				result = 1;
				return result;
			}
			break;

		case 15:
			if (flightGroupIdx != variable) {
				result = 1;
				return result;
			}
			break;

		case 16:
			if (g_craftTypeToObjectType[variable + 1] != objectType) {
				result = 1;
				return result;
			}
			break;

		case 17:
			if (g_modelTypeTable[objectType].genusId != g_genusConvert[variable]) {
				result = 1;
				return result;
			}
			break;

		case 18:
			if ((uint8_t)g_modelTypeTable[objectType].familyId != g_familyConvert[variable]) {
				result = 1;
				return result;
			}
			break;

		case 19:
			if (g_missionFlightGroups[flightGroupIdx].fg.iff != variable) {
				result = 1;
				return result;
			}
			break;

		case 20:
			if (g_missionFlightGroups[flightGroupIdx].fg.globalGroup != variable) {
				result = 1;
				return result;
			}
			break;

		case 21:
			if (g_missionFlightGroups[flightGroupIdx].fg.team != variable) {
				result = 1;
				return result;
			}
			break;

		case 23:
			if (g_missionFlightGroups[flightGroupIdx].fg.globalUnit == variable) {
				result = 1;
				return result;
			}
			break;

		case 24:
			if (g_missionFlightGroups[flightGroupIdx].fg.globalUnit != variable) {
				result = 1;
				return result;
			}
			break;

		default:
			break;
	}

	return result;
}

// FUNCTION: XVT 0x433390
int16_t Mission_ObjectMatchesTriggerVariable(uint16_t objectIdx, uint16_t variableType, uint16_t variable) {
	ObjectRecord* object = &g_objectTable[objectIdx];
	MobileObject* mobile = object->mobj;
	CraftData* craft;
	uint16_t flightGroup;
	uint16_t team;
	uint16_t objectType;
	int16_t result;

	if (mobile != NULL) {
		craft = mobile->pCraft;
		flightGroup = object->flightGroupIdx;
		team = mobile->team;
	} else {
		flightGroup = object->flightGroupIdx;
		team = g_missionFlightGroups[flightGroup].fg.team;
	}
	result = 0;
	objectType = g_craftTypeToObjectType[g_missionFlightGroups[flightGroup].fg.craftType];

	switch (variableType) {
		case 0:
			result = 0;
			break;
		case 1:
			if (variable == flightGroup)
				result = 1;
			break;
		case 2:
			if (g_craftTypeToObjectType[variable + 1] == objectType)
				result = 1;
			break;
		case 3:
			if (g_modelTypeTable[objectType].genusId == g_genusConvert[variable])
				result = 1;
			break;
		case 4:
			if ((uint8_t)g_modelTypeTable[objectType].familyId == g_familyConvert[variable])
				result = 1;
			break;
		case 5:
			if (mobile == NULL) {
				if (g_missionFlightGroups[flightGroup].fg.iff == variable)
					result = 1;
			} else {
				if ((uint8_t)mobile->iff == variable)
					result = 1;
			}
			break;
		case 6:
			if (g_missionFlightGroups[flightGroup].fg.orders[0].order == variable)
				result = 1;
			break;
		case 7:
			if (mobile == NULL)
				break;
			switch (variable) {
				case 0:
					if (craft->wasCaptured != 0)
						result = 1;
					break;
				case 1: {
					uint16_t index;
					for (index = 0; index < 10; ++index)
						if (mobile->team != index && craft->iffVisibility[index] != 0)
							result = 1;
					break;
				}
				case 2:
					if (craft->aiFlight.orderActionCounter != 0)
						result = 1;
					break;
				case 3:
					if (craft->aiFlight.objSignatureCount != 0)
						result = 1;
					break;
				case 4:
					if (craft->workingSubsystems == 0)
						result = 1;
					break;
				case 5: {
					uint16_t index;
					for (index = 0; index < 10; ++index)
						if (mobile->team != index && craft->attackedByTeam[index] != 0)
							result = 1;
					break;
				}
				case 6:
					if (craft->hullDamage != 0)
						result = 1;
					break;
				case 7:
					if (g_missionFlightGroups[flightGroup].fg.specialCargoCraft == craft->waveNumber)
						result = 1;
					break;
				case 8:
					if (g_missionFlightGroups[flightGroup].fg.specialCargoCraft != craft->waveNumber)
						result = 1;
					break;
				case 9:
					if (object->playerOwnerIdx != -1)
						result = 1;
					break;
				case 10:
					if (object->playerOwnerIdx == -1)
						result = 1;
					break;
				case 11: {
					uint16_t index;
					for (index = 0; index < 10; ++index)
						if (craft->attackedByTeam[index] == 0)
							result = 1;
					break;
				}
				case 12:
					if (craft->workingSubsystems != 0)
						result = 1;
					break;
				case 13:
					if (craft->wasCaptured == 0)
						result = 1;
					break;
				case 14: {
					uint16_t index;
					for (index = 0; index < 10; ++index)
						if (craft->iffVisibility[index] == 0)
							result = 1;
					break;
				}
				case 15:
					result = 0;
					break;
				case 16:
					if (craft->aiFlight.orderActionCounter == 0)
						result = 1;
					break;
				case 17:
					result = 0;
					break;
				case 18:
					if (craft->aiFlight.objSignatureCount == 0)
						result = 1;
					break;
				case 22:
					if ((craft->hullMax >> 2) <= craft->hullDamage)
						result = 1;
					break;
				case 23:
					if ((craft->hullMax >> 1) <= craft->hullDamage)
						result = 1;
					break;
				case 24:
					if (MATH2_longfraction(craft->hullMax, 0xC000) <= craft->hullDamage)
						result = 1;
					break;
				case 25: {
					int16_t launcherCount = 0;
					uint16_t index;
					uint8_t warheadLauncherCount = craft->warheadLauncherCount;
					for (index = 0; index < warheadLauncherCount; ++index) {
						launcherCount +=
							craft->weaponSlots[g_modelDefs[craft->modelIndex].warheadLauncherFirstSlot[index]]
								.count;
						launcherCount +=
							craft->weaponSlots[g_modelDefs[craft->modelIndex].warheadLauncherLastSlot[index]]
								.count;
					}
					if (launcherCount == 0)
						result = 1;
					break;
				}
				default:
					break;
			}
			break;
		case 8:
			if (g_missionFlightGroups[flightGroup].fg.globalGroup == variable)
				result = 1;
			break;
		case 9:
			if (g_missionFlightGroups[flightGroup].fg.groupAI == variable)
				result = 1;
			break;
		case 10:
			if (g_missionFlightGroups[flightGroup].fg.status1 == variable)
				result = 1;
			break;
		case 11:
			result = 1;
			break;
		case 12:
			if (variable == team)
				result = 1;
			break;
		case 13:
#ifdef XVT_MODERN
			if (object->playerOwnerIdx >= 0 &&
				g_missionFlightGroups[g_players[object->playerOwnerIdx].boundFlightGroupIdx].fg.playerNumber -
						variable ==
					1)
#else
			if (g_missionFlightGroups[g_players[object->playerOwnerIdx].boundFlightGroupIdx].fg.playerNumber -
					variable ==
				1)
#endif
				result = 1;
			break;
		case 14: {
			uint16_t targetTime = 5 * variable;
			uint16_t targetMinutes = targetTime / 60;
			if (g_missionElapsedClock.minutes < targetMinutes)
				result = 1;
			else if (g_missionElapsedClock.minutes == targetMinutes)
				result = targetTime % 60 >= g_missionElapsedClock.seconds;
			break;
		}
		case 15:
			if (variable != flightGroup)
				result = 1;
			break;
		case 16:
			if (g_craftTypeToObjectType[variable + 1] != objectType)
				result = 1;
			break;
		case 17:
			if (g_modelTypeTable[objectType].genusId != g_genusConvert[variable])
				result = 1;
			break;
		case 18:
			if ((uint8_t)g_modelTypeTable[objectType].familyId != g_familyConvert[variable])
				result = 1;
			break;
		case 19:
			if (mobile == NULL) {
				if (g_missionFlightGroups[flightGroup].fg.iff != variable)
					result = 1;
			} else {
				if ((uint8_t)mobile->iff != variable)
					result = 1;
			}
			break;
		case 20:
			if (g_missionFlightGroups[flightGroup].fg.globalGroup != variable)
				result = 1;
			break;
		case 21:
			if (variable != team)
				result = 1;
			break;
		case 22:
			if (object->playerOwnerIdx != variable)
				result = 1;
			break;
		case 23:
			if (g_missionFlightGroups[flightGroup].fg.globalUnit == variable)
				result = 1;
			break;
		case 24:
			if (g_missionFlightGroups[flightGroup].fg.globalUnit != variable)
				result = 1;
			break;
		default:
			break;
	}
	return result;
}

// FUNCTION: XVT 0x433D30
void Mission_RecordCraftOutcome(uint16_t objIdx, uint16_t flightGroupIdx, uint16_t outcomeId) {
	CraftData* craft = g_objectTable[objIdx].mobj->pCraft;

	if (craft->missionAccountingDone == 1)
		return;
	craft->missionAccountingDone = 1;
	++g_missionFgStats[flightGroupIdx].outcomeCount[outcomeId];
	if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[outcomeId] = 1;

	if (outcomeId == FLIGHT_GROUP_OUTCOME_DESTROYED) {
		if (craft->wasCaptured != 0) {
			--g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED];
			if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED] = 0;
			craft->wasCaptured = 0;
		}
		if (craft->aiFlight.departTimerFlag != 0) {
			--g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
			if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] = 0;
		}
		if (craft->aiFlight.missionAbortedFlag != 0) {
			--g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ABORTED];
			if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ABORTED] = 0;
		}
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION];
		if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
			g_missionFgStats[flightGroupIdx]
				.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION] = 1;
	} else if (g_objectTable[objIdx].playerOwnerIdx != -1 && craft->aiFlight.departTimerFlag == 0 &&
			   craft->aiFlight.missionAbortedFlag == 0 &&
			   g_flightMissionState.runtime.teamGoalStatus[g_objectTable[objIdx].mobj->team][0] != 1) {
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
		if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] = 1;
	}

	{
		uint16_t index;
		for (index = 0; index < 10; ++index) {
			if (craft->iffVisibility[index] == 0) {
				++g_missionFgStats[flightGroupIdx].teamUninspectedLost[index];
				if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
					g_missionFgStats[flightGroupIdx].teamSpecialCargoUninspectedLost[index] = 1;
			}
		}
	}
	if (craft->notDisabledAccountingSuppress == 0) {
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED];
		if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] = 1;
	}
	if (craft->wasCaptured == 0) {
		uint16_t index;
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED];
		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber)
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] = 1;
		for (index = 0; index < 10; ++index) {
			if (g_missionFlightGroups[flightGroupIdx].fg.team != index) {
				++g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount[index];
				g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamSpecialCargo[index] = 1;
			}
		}
	}
	{
		int16_t hasAttackedTeam = 0;
		uint16_t index;
		for (index = 0; index < 10; ++index) {
			if (craft->attackedByTeam[index] == 1)
				hasAttackedTeam = 1;
		}
		if (hasAttackedTeam == 0) {
			++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED];
			if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] = 1;
		}
	}
	if (craft->aiFlight.orderActionCounter == 0) {
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED];
		if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft)
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] = 1;
	}
	if (craft->aiFlight.objSignatureCount == 0) {
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DOCKED];
		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber)
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DOCKED] = 1;
	}

	if (outcomeId == FLIGHT_GROUP_OUTCOME_DESTROYED) {
		uint16_t index;
		for (index = 0; g_missionHeader.numFlightGroups > index; ++index) {
			if (flightGroupIdx != index) {
				if (g_missionFlightGroups[index].fg.arrivalMethod != 0 &&
					g_missionFlightGroups[index].fg.arrivalMothership == flightGroupIdx) {
					Mission_CloseUnavailableFlightGroupAccounting(index);
				}
				if (g_missionFlightGroups[index].fg.departureMethod != 0 &&
					g_missionFlightGroups[index].fg.departureMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] =
						0;
				}
				if (g_missionFlightGroups[index].fg.alternateMothershipUsed != 0 &&
					g_missionFlightGroups[index].fg.alternateMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index]
						.outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] = 0;
				}
				if (g_missionFlightGroups[index].fg.capturedDepartViaMothership != 0 &&
					g_missionFlightGroups[index].fg.capturedDepartureMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] =
						0;
				}
			}
		}
		{
			MissionOrder* order = g_missionFlightGroups[flightGroupIdx].fg.orders;
			int ordersRemaining = 4;
			do {
				if (strcmp(g_planTable
							   [g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order->order]]]
								   .name,
						   "dropoffldr1pln") == 0) {
					Mission_CloseUnavailableFlightGroupAccounting((uint16_t)(order->variable2 - 1));
				}
				++order;
			} while (--ordersRemaining != 0);
		}
	}
	if (outcomeId == FLIGHT_GROUP_OUTCOME_LEFT_REGION) {
		uint16_t index;
		for (index = 0; g_missionHeader.numFlightGroups > index; ++index) {
			if (flightGroupIdx != index) {
				if (g_missionFlightGroups[index].fg.departureMethod != 0 &&
					g_missionFlightGroups[index].fg.departureMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] =
						0;
				}
				if (g_missionFlightGroups[index].fg.alternateMothershipUsed != 0 &&
					g_missionFlightGroups[index].fg.alternateMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index]
						.outcomeCount[FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT] = 0;
				}
				if (g_missionFlightGroups[index].fg.capturedDepartViaMothership != 0 &&
					g_missionFlightGroups[index].fg.capturedDepartureMothership == flightGroupIdx) {
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION] +=
						g_missionFgStats[index]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT];
					g_missionFgStats[index].outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT] =
						0;
				}
			}
		}
	}
	{
		int slotIndex;
		for (slotIndex = g_activeRegionObjectSlotStart; g_activeRegionCraftObjectSlotEnd > slotIndex;
			 ++slotIndex) {
			if (g_objectTable[slotIndex].objectType != 0) {
				CraftData* otherCraft = g_objectTable[slotIndex].mobj->pCraft;
				if (otherCraft->lastAttackerObjIdx == objIdx)
					otherCraft->lastAttackerObjIdx = UINT16_MAX;
			}
		}
	}
	{
		int slotIndex;
		for (slotIndex = 0; slotIndex < 8; ++slotIndex) {
			if (g_players[slotIndex].connectedFlag != 0) {
				int presetIndex;
				for (presetIndex = 0; presetIndex < 4; ++presetIndex) {
					if (g_players[slotIndex].targetPresetSlot[presetIndex] == (int16_t)objIdx)
						g_players[slotIndex].targetPresetSlot[presetIndex] = -1;
				}
			}
		}
	}
}

// FUNCTION: XVT 0x434440
int16_t Mission_CloseUnavailableFlightGroupAccounting(int flightGroupIdx) {
	int16_t unavailableCount;
	int16_t unavailableSpecialCargoCount;
	int16_t result;

	unavailableCount = g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] -
					   g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
	unavailableSpecialCargoCount =
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] -
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];
	result = g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +=
		unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_LOST_WITH_MOTHERSHIP] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] += unavailableCount;
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] +=
		unavailableSpecialCargoCount;
	g_missionFgStats[flightGroupIdx].hasArrived = 1;
	g_missionFgStats[flightGroupIdx].wavesRemaining = 0;
	return result;
}

// FUNCTION: XVT 0x434500
void Mission_CreditDestructionDamageContributors(uint16_t sourceObjIdx, uint16_t victimObjIdx) {
	enum { PLAYER_COUNT = 8, TEAM_COUNT = 10 };

	ObjectRecord* victim;
	CraftData* craft;
	int victimRating;
	int creditedOwner;
	int awardRating;
	int attackerRatingWeight;
	int victimFlightGroupIndex;
	int specialCargo;
	int playerContributionByTeam[TEAM_COUNT];
	int playerIndex;
	int teamIndex;

	specialCargo = 0;
	memset(playerContributionByTeam, 0, sizeof(playerContributionByTeam));
	creditedOwner = -1;
	craft = NULL;
	victim = &g_objectTable[victimObjIdx];
	if (victim->mobj != NULL)
		craft = victim->mobj->pCraft;
	if (craft == NULL || craft->damageStats.damageReceivedTotal == 0) {
		if (g_objectTable[sourceObjIdx].playerOwnerIdx != -1) {
			Mission_CreditPlayerKillContribution(victimObjIdx, 0, 3,
												 g_objectTable[sourceObjIdx].playerOwnerIdx, -1, 0);
			if (g_objectTable[victimObjIdx].genusId == 8) {
				PlayerData* ownerPlayer = &g_players[g_objectTable[sourceObjIdx].playerOwnerIdx];
				ownerPlayer->missionStats.worseRatingPromoPoints += 4;
			}
		}
		Mission_CreditTeamKillContribution(
			victimObjIdx, 0, 3, g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx].fg.team);
		return;
	}

	victimFlightGroupIndex = g_objectTable[victimObjIdx].flightGroupIdx;
	if (g_missionFlightGroups[victimFlightGroupIndex].fg.specialCargoCraft == craft->waveNumber)
		specialCargo = 1;
	if ((uint16_t)MATH2_percentage(craft->damageStats.damageReceivedByPlayerOwnedCraft,
								   craft->damageStats.damageReceivedTotal) >= 0x8000u)
		memcpy(&creditedOwner, &g_missionFlightGroups[victimFlightGroupIndex].playerOwnerIdx,
			   sizeof(creditedOwner));
	if (creditedOwner != -1)
		victimRating = g_players[creditedOwner].pilotRating;
	else
		victimRating = g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.groupAI;

	for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
		int contributionTier;
		uint16_t damagePercent;

		if (g_players[playerIndex].connectedFlag == 0)
			continue;
		damagePercent = (uint16_t)MATH2_percentage(craft->damageStats.damageFromPlayer[playerIndex],
												   craft->damageStats.damageReceivedTotal);
		if (damagePercent >= 0xAAAAu)
			contributionTier = 3;
		else if (damagePercent >= 0x5999u)
			contributionTier = 2;
		else if (damagePercent >= 0x0CCCu)
			contributionTier = 1;
		else
			contributionTier = 0;
		if (contributionTier != 0) {
			Mission_CreditPlayerKillContribution(victimObjIdx, specialCargo, contributionTier, playerIndex,
												 creditedOwner, victimRating);
			{
				ObjectRecord* damagedVictim = &g_objectTable[victimObjIdx];
				if (g_missionTeams[(uint16_t)g_players[playerIndex].playerIff]
						.allies[g_missionFlightGroups[damagedVictim->flightGroupIdx].fg.team] == 0) {
					int minimumRatingAward;
					int victimRatingWeight;
					int ratingPoints;
					uint8_t planId;
					const char* planName;

					planId = craft->aiController.currentPlanId;
					planName = g_planTable[planId].name;
					minimumRatingAward =
						(strcmp(planName, "nullpln") == 0 || strcmp(planName, "stationaryldrpln") == 0 ||
						 strcmp(planName, "formldr1pln") == 0 || strcmp(planName, "formflw1pln") == 0 ||
						 strcmp(planName, "formevadeldr1pln") == 0 ||
						 strcmp(planName, "formevadeflw1pln") == 0 ||
						 strcmp(planName, "exithangarpln") == 0 || strcmp(planName, "enterhangarpln") == 0 ||
						 strcmp(planName, "disabledpln") == 0 || strcmp(planName, "selfdestroypln") == 0) &&
						creditedOwner == -1;

					victimRatingWeight =
						g_modelDefs[GetModelIndexFromType(damagedVictim->objectType)].ratingWeight;
					if (victimRatingWeight == 0)
						minimumRatingAward = 1;
					attackerRatingWeight =
						g_modelDefs[GetModelIndexFromType(
										g_craftTypeToObjectType
											[g_missionFlightGroups[g_players[playerIndex].boundFlightGroupIdx]
												 .fg.craftType])]
							.ratingWeight;
					if (creditedOwner == -1)
						awardRating =
							g_defaultPilotRatingByAiLevel[g_missionFlightGroups[victimFlightGroupIndex]
															  .fg.groupAI];
					else
						awardRating = g_players[creditedOwner].pilotRating;

					{
						int ratingFactor = awardRating - g_players[playerIndex].pilotRating;
						if (minimumRatingAward == 0 && ratingFactor >= -4) {
							ratingFactor += 4;
							if (g_players[playerIndex].pilotRating < 15) {
								if (ratingFactor < 4)
									ratingFactor = 4;
							} else if (ratingFactor < 3) {
								ratingFactor = 3;
							}
							ratingPoints =
								ratingFactor * awardRating * victimRatingWeight / attackerRatingWeight;
							if (contributionTier == 2)
								ratingPoints /= 2;
							else if (contributionTier == 1)
								ratingPoints /= 10;
							g_players[playerIndex].missionStats.ratingPromoPoints += ratingPoints;
#ifdef XVT_MODERN
							sprintf(g_flightTextScratchBuffer,
									"Rating points awarded: %d to player: %d Better total: %d\n",
									ratingPoints, playerIndex,
									g_players[playerIndex].missionStats.ratingPromoPoints);
#else
							sprintf(g_missionDebugBuffer,
									"Rating points awarded: %d to player: %d Better total: %d\n",
									ratingPoints, playerIndex,
									g_players[playerIndex].missionStats.ratingPromoPoints);
#endif
						} else {
							if (minimumRatingAward != 0) {
								awardRating = 1;
								if (victimRatingWeight == 0)
									victimRatingWeight = 1;
							}
							if (awardRating == 0)
								awardRating = 1;
							ratingPoints = awardRating * victimRatingWeight / attackerRatingWeight;
							if (contributionTier == 2)
								ratingPoints /= 2;
							else if (contributionTier == 1)
								ratingPoints /= 10;
							if (ratingPoints == 0)
								ratingPoints = 1;
							g_players[playerIndex].missionStats.worseRatingPromoPoints += ratingPoints;
#ifdef XVT_MODERN
							sprintf(g_flightTextScratchBuffer,
									"Rating points awarded: %d to player: %d Worse total: %d\n", ratingPoints,
									playerIndex, g_players[playerIndex].missionStats.worseRatingPromoPoints);
#else
							sprintf(g_missionDebugBuffer,
									"Rating points awarded: %d to player: %d Worse total: %d\n", ratingPoints,
									playerIndex, g_players[playerIndex].missionStats.worseRatingPromoPoints);
#endif
						}
					}

					if (contributionTier == 2 || contributionTier == 3) {
						int attributionCredit;
						if (contributionTier == 2)
							attributionCredit = 1;
						else
							attributionCredit = 2;
						Mission_RecordPlayerCraftLossAttribution(g_players[playerIndex].boundFlightGroupIdx,
																 victimObjIdx, contributionTier);
						playerContributionByTeam[(uint16_t)g_players[playerIndex].playerIff] +=
							attributionCredit;
					}
				}
			}
		}
	}

	for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
		int flightGroupIndex;
		int* damageFromFlightGroup;
		MissionFlightGroup* flightGroup;
		unsigned int teamDamage = 0;
		unsigned int largestDamage = 0;
		int largestFlightGroup = 0;
		flightGroupIndex = 0;
		if ((int16_t)g_missionHeader.numFlightGroups > 0) {
			int flightGroupCount = (int16_t)g_missionHeader.numFlightGroups;
			damageFromFlightGroup = &craft->damageStats.damageFromFlightGroupAmount[0];
			flightGroup = &g_missionFlightGroups[0];
			do {
				if (flightGroup->fg.team == teamIndex) {
					unsigned int damage;
					damage = *damageFromFlightGroup;
					teamDamage += damage;
					if (largestDamage < damage) {
						largestDamage = damage;
						largestFlightGroup = flightGroupIndex;
					}
				}
				++flightGroupIndex;
				++flightGroup;
				++damageFromFlightGroup;
			} while (flightGroupIndex < flightGroupCount);
		}
		{
			int contributionTier;
			uint16_t damagePercent =
				(uint16_t)MATH2_percentage(teamDamage, craft->damageStats.damageReceivedTotal);
			if (damagePercent >= 0xAAAAu)
				contributionTier = 3;
			else if (damagePercent >= 0x5999u)
				contributionTier = 2;
			else if (damagePercent >= 0x0CCCu)
				contributionTier = 1;
			else
				contributionTier = 0;
			if (contributionTier != 0)
				Mission_CreditTeamKillContribution(victimObjIdx, specialCargo, contributionTier, teamIndex);
			if ((contributionTier == 2 || contributionTier == 3) && playerContributionByTeam[teamIndex] < 2) {
				int remainingCredit;
				contributionTier -= 2;
				remainingCredit = (contributionTier != 0 ? 2 : 1) - playerContributionByTeam[teamIndex];
				if (remainingCredit == 2) {
					Mission_RecordPlayerCraftLossAttribution(largestFlightGroup, victimObjIdx, 3);
					if (creditedOwner != -1)
						++g_players[creditedOwner]
							  .perMissionKills.killsFullFromFlightGroup[largestFlightGroup];
				} else if (remainingCredit == 1) {
					Mission_RecordPlayerCraftLossAttribution(largestFlightGroup, victimObjIdx, 2);
					if (creditedOwner != -1)
						++g_players[creditedOwner]
							  .perMissionKills.killsSharedFromFlightGroup[largestFlightGroup];
				}
			}
		}
	}
}

// FUNCTION: XVT 0x434BC0
void Mission_CreditPlayerKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										  int playerIdx, int creditedOwnerIdx, int victimRating) {
	uint16_t flightGroupIdx;
	uint16_t tacticalVoiceProbability;
	int scoreDivisor;
	int goalIndex;

	flightGroupIdx = g_objectTable[victimObjIdx].flightGroupIdx;
	tacticalVoiceProbability = 0;
	scoreDivisor = 1;
	for (goalIndex = 0; goalIndex < 8; goalIndex++) {
		if (g_missionFlightGroups[flightGroupIdx].fg.goals[goalIndex].type == 0 &&
			g_missionFlightGroups[flightGroupIdx]
					.fg.goals[goalIndex]
					.enabledTeams[(uint16_t)g_players[playerIdx].playerIff] == 1) {
			switch (g_missionFlightGroups[flightGroupIdx].fg.goals[goalIndex].eventCondition) {
				case 0:
				case 10:
					tacticalVoiceProbability = 24576;
					break;
				case 2:
					tacticalVoiceProbability = (uint16_t)-4096;
					break;
			}
		}
	}

	if (g_missionTeams[(uint16_t)g_players[playerIdx].playerIff]
			.allies[g_missionFlightGroups[flightGroupIdx].fg.team] == 0) {
		int score = Mission_ComputeKillScoreForObject(victimObjIdx);
		switch (contributionTier) {
			case 1:
				g_players[playerIdx].perMissionKills.killsAssistOnFlightGroup[flightGroupIdx]++;
				if (creditedOwnerIdx != -1) {
					g_players[playerIdx].perMissionKills.killsAssistOnPlayerRating[victimRating]++;
				} else {
					g_players[playerIdx].perMissionKills.killsAssistOnAiRating[victimRating]++;
				}
				scoreDivisor = 10;
				score /= 10;
				break;
			case 2:
				g_players[playerIdx].perMissionKills.killsSharedOnFlightGroup[flightGroupIdx]++;
				if (creditedOwnerIdx != -1) {
					g_players[playerIdx].perMissionKills.killsSharedOnPlayer[creditedOwnerIdx]++;
					g_players[playerIdx].perMissionKills.killsSharedOnPlayerRating[victimRating]++;
					g_players[creditedOwnerIdx].perMissionKills.killsSharedFromPlayer[playerIdx]++;
				} else {
					g_players[playerIdx].perMissionKills.killsSharedOnAiRating[victimRating]++;
				}
				scoreDivisor = 6;
				score /= 2;
				break;
			case 3:
				g_players[playerIdx].perMissionKills.killsFullOnFlightGroup[flightGroupIdx]++;
				if (creditedOwnerIdx != -1) {
					g_players[playerIdx].perMissionKills.killsFullOnPlayer[creditedOwnerIdx]++;
					g_players[playerIdx].perMissionKills.killsFullOnPlayerRating[victimRating]++;
					g_players[creditedOwnerIdx].perMissionKills.killsFullFromPlayer[playerIdx]++;
				} else {
					g_players[playerIdx].perMissionKills.killsFullOnAiRating[victimRating]++;
				}
				scoreDivisor = 1;
				break;
			default:
				break;
		}
		if (g_flightMissionState.difficulty == 2) {
			score *= 2;
		} else if (g_flightMissionState.difficulty == 0) {
			score /= 2;
		}
		if (Mission_ApplyFlightGroupGoalScore(2, flightGroupIdx, playerIdx, (uint16_t)scoreDivisor,
											  specialCargoFlag,
											  (uint16_t)g_players[playerIdx].playerIff) >= 0) {
			g_players[playerIdx].missionStats.missionScore += score;
		}
		if (contributionTier == 3 && g_localPlayer == playerIdx) {
			fsfx_speakorderack(g_localPlayer, -1, 21, -1, -1, tacticalVoiceProbability);
		}
	} else if (contributionTier == 2 || contributionTier == 3) {
		int score = Mission_ComputeKillScoreForObject(victimObjIdx);
		g_players[playerIdx].missionStats.missionScore -= score;
		g_players[playerIdx].perMissionKills.friendliesKilled++;
		g_players[playerIdx].missionStats.ratingPromoPoints -= 500;
		g_msgSenderIff = g_players[playerIdx].iff;
		msg_emitInFlightMessage(IFMSG_281_YOU_HAVE_DESTROYED_A_CRAFT_ON_YOUR_OWN_SIDE, playerIdx);
		if (score != 0) {
			g_msgArgTable[0] = (uint16_t)score;
			msg_emitInFlightMessage(IFMSG_200_PENALTY_POINTS_DEDUCTED_ARG, playerIdx);
		}
		if (g_localPlayer == playerIdx) {
			fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_ORDER, TACTICAL_MSG_FRIENDLY_CRAFT_DESTROYED,
										   victimObjIdx, UINT16_MAX);
		}
	}
}

// FUNCTION: XVT 0x434F20
void Mission_CreditTeamKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										int teamIdx) {
	int scoreDivisor;
	uint16_t flightGroupIdx;
	int score;

	scoreDivisor = 1;
	flightGroupIdx = g_objectTable[victimObjIdx].flightGroupIdx;
	if (g_missionTeams[teamIdx].allies[g_missionFlightGroups[flightGroupIdx].fg.team] == 0) {
		score = Mission_ComputeKillScoreForObject(victimObjIdx);
		switch (contributionTier) {
			case 1:
				++g_flightMissionState.runtime.teamKillStats[2][teamIdx];
				scoreDivisor = 10;
				score /= 10;
				break;
			case 2:
				++g_flightMissionState.runtime.teamKillStats[1][teamIdx];
				scoreDivisor = 6;
				score /= 2;
				break;
			case 3:
				++g_flightMissionState.runtime.teamKillStats[0][teamIdx];
				scoreDivisor = 1;
				break;
		}
		if (g_flightMissionState.difficulty == 2) {
			score *= 2;
		} else if (g_flightMissionState.difficulty == 0) {
			score /= 2;
		}
		if (Mission_ApplyFlightGroupGoalScore(2, flightGroupIdx, -1, scoreDivisor, specialCargoFlag,
											  teamIdx) >= 0) {
			g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx] += score;
		}
	} else if (contributionTier == 2 || contributionTier == 3) {
		g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx] -=
			Mission_ComputeKillScoreForObject(victimObjIdx);
	}
}

// FUNCTION: XVT 0x435070
void Mission_RecordProjectileHitStats(uint16_t projectileObjIdx) {
	int ownerObjIdx;
	CraftData* ownerCraft;
	int ownerPlayerIdx;
	uint16_t playerProjectileSlotEnd;

	ownerObjIdx = g_objectTable[projectileObjIdx].mobj->sourceObjIdx;
	if (g_activeRegionCraftObjectSlotEnd <= ownerObjIdx)
		return;

	if (g_objectTable[ownerObjIdx].objectType == 0)
		return;

	playerProjectileSlotEnd = (uint16_t)(g_projectileObjectSlotStart + 128);
	ownerPlayerIdx = g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].playerOwnerIdx;
	ownerCraft = g_objectTable[ownerObjIdx].mobj->pCraft;

	switch (g_objectTable[projectileObjIdx].objectType) {
		case PROJECTILE_OBJECT_TYPE_REBEL_LASER:
		case PROJECTILE_OBJECT_TYPE_REBEL_TURBO_LASER:
		case PROJECTILE_OBJECT_TYPE_IMPERIAL_LASER:
		case PROJECTILE_OBJECT_TYPE_IMPERIAL_TURBO_LASER:
			++ownerCraft->weaponStats.laserHitsScored;
			if (playerProjectileSlotEnd > projectileObjIdx && ownerPlayerIdx != -1)
				++g_players[ownerPlayerIdx].missionStats.laserHitsScored;
			break;

		case PROJECTILE_OBJECT_TYPE_ION_LASER:
		case PROJECTILE_OBJECT_TYPE_ION_TURBO_LASER:
			++ownerCraft->weaponStats.ionHitsScored;
			if (playerProjectileSlotEnd > projectileObjIdx && ownerPlayerIdx != -1)
				++g_players[ownerPlayerIdx].missionStats.ionHitsScored;
			break;

		case WARHEAD_OBJECT_TYPE_PROTON_TORPEDO:
		case WARHEAD_OBJECT_TYPE_CONCUSSION_MISSILE:
		case WARHEAD_OBJECT_TYPE_ADVANCED_PROTON_TORPEDO:
		case WARHEAD_OBJECT_TYPE_ADVANCED_CONCUSSION_MISSILE:
		case WARHEAD_OBJECT_TYPE_SPACE_BOMB:
		case WARHEAD_OBJECT_TYPE_HEAVY_ROCKET:
		case WARHEAD_OBJECT_TYPE_MAGNETIC_PULSE:
		case WARHEAD_OBJECT_TYPE_ION_PULSE:
		case WARHEAD_OBJECT_TYPE_LASER_3: {
			int flightGroupIdx;

			++ownerCraft->weaponStats.warheadHitsScored;
			flightGroupIdx = g_objectTable[ownerObjIdx].flightGroupIdx;
			if (playerProjectileSlotEnd > projectileObjIdx && ownerPlayerIdx != -1) {
				++g_players[ownerPlayerIdx].perMissionKills.warheadHits;
				g_players[ownerPlayerIdx].missionStats.missionScore +=
					g_projectileDamageByObjectType
						.warheadPointValue[g_objectTable[projectileObjIdx].objectType -
										   PROJECTILE_OBJECT_TYPE_FIRST];
			}
			g_flightMissionState.runtime
				.teamScores[TEAM_SCORE_MISSION][g_missionFlightGroups[flightGroupIdx].fg.team] +=
				g_projectileDamageByObjectType.warheadPointValue[g_objectTable[projectileObjIdx].objectType -
																 PROJECTILE_OBJECT_TYPE_FIRST];
			break;
		}

		default:
			break;
	}
}

// FUNCTION: XVT 0x435250
int Mission_RecordPlayerCraftLoss(unsigned int objIdx, int allowPendingDamageCredit) {
	enum { PLAYER_COUNT = 8, AI_SKILL_COUNT = 6, PLAYER_DAMAGE_THRESHOLD = 0x8000 };

	unsigned int playerIdx;
	int aiSkillIdx;
	int pointPenalty;
	ObjectRecord* object;
	int teamIdx;
	int ownerPlayerIdx;
	CraftData* craft;
	int creditedPlayerIdx;

	creditedPlayerIdx = -1;
	pointPenalty = Mission_ComputeCraftPointValue(objIdx) / 2;
	object = &g_objectTable[objIdx];
	teamIdx = g_missionFlightGroups[object->flightGroupIdx].fg.team;
	g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx] -= pointPenalty;
	++g_flightMissionState.runtime.teamKillStats[3][g_missionFlightGroups[object->flightGroupIdx].fg.team];
	ownerPlayerIdx = object->playerOwnerIdx;
	if (ownerPlayerIdx == -1) {
		ownerPlayerIdx = g_missionFlightGroups[object->flightGroupIdx].playerOwnerIdx;
		if (ownerPlayerIdx == -1 || g_players[ownerPlayerIdx].connectedFlag == 0)
			return pointPenalty;
	}

	craft = object->mobj->pCraft;
	g_players[ownerPlayerIdx].missionStats.missionScore -= pointPenalty;
	if (allowPendingDamageCredit != 0) {
		uint32_t remainingDurability;
		uint32_t pendingDamage;

		if ((uint32_t)craft->hullDamage < (uint32_t)craft->hullMax)
			remainingDurability = (uint32_t)(craft->hullMax - craft->hullDamage);
		else
			remainingDurability = 0;
		if (craft->shieldEnergy[0] > 0)
			remainingDurability += (uint16_t)craft->shieldEnergy[0];
		if (craft->shieldEnergy[1] > 0)
			remainingDurability += (uint16_t)craft->shieldEnergy[1];
		remainingDurability >>= 1;
		pendingDamage = 0;
		for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx)
			pendingDamage += (uint32_t)craft->damageStats.damageFromPlayer[playerIdx];
		for (aiSkillIdx = 0; aiSkillIdx < AI_SKILL_COUNT; ++aiSkillIdx)
			pendingDamage += (uint32_t)craft->damageStats.damageFromAiSkill[aiSkillIdx];
		if (pendingDamage > remainingDurability)
			Mission_CreditDestructionDamageContributors(objIdx, objIdx);
	}

	if ((uint16_t)MATH2_percentage((uint32_t)craft->damageStats.damageReceivedByPlayerOwnedCraft,
								   (uint32_t)craft->damageStats.damageReceivedTotal) >=
		PLAYER_DAMAGE_THRESHOLD) {
		uint32_t playerDamage;
		uint32_t aiDamage;
		uint32_t collisionDamage;
		uint32_t mineDamage;
		uint32_t starshipDamage;
		uint32_t totalClassifiedDamage;

		++g_players[ownerPlayerIdx].perMissionKills.totalCraftLosses;
		playerDamage = 0;
		for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx)
			playerDamage += (uint32_t)craft->damageStats.damageFromPlayer[playerIdx];
		aiDamage = 0;
		for (aiSkillIdx = 0; aiSkillIdx < AI_SKILL_COUNT; ++aiSkillIdx)
			aiDamage += (uint32_t)craft->damageStats.damageFromAiSkill[aiSkillIdx];
		collisionDamage = (uint32_t)craft->damageStats.damageFromCollision;
		starshipDamage = (uint32_t)craft->damageStats.damageFromStarship;
		mineDamage = (uint32_t)craft->damageStats.damageFromMine;
		totalClassifiedDamage = playerDamage | aiDamage | collisionDamage | starshipDamage | mineDamage;
		if (totalClassifiedDamage != 0) {
			if (collisionDamage >= mineDamage && collisionDamage >= starshipDamage &&
				collisionDamage >= playerDamage && collisionDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByCollisions;
			} else if (starshipDamage >= mineDamage && starshipDamage >= collisionDamage &&
					   starshipDamage >= playerDamage && starshipDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByStarships;
			} else if (mineDamage >= starshipDamage && mineDamage >= collisionDamage &&
					   mineDamage >= playerDamage && mineDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByMines;
			} else if (playerDamage >= starshipDamage && playerDamage >= collisionDamage &&
					   playerDamage >= mineDamage && playerDamage >= aiDamage) {
				uint32_t maxPlayerDamage;
				int topPlayerIdx;
				int topPlayerRating;

				maxPlayerDamage = 0;
				topPlayerRating = -1;
				for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
					if ((uint32_t)craft->damageStats.damageFromPlayer[playerIdx] > maxPlayerDamage) {
						maxPlayerDamage = (uint32_t)craft->damageStats.damageFromPlayer[playerIdx];
						topPlayerRating = g_players[playerIdx].pilotRating;
						topPlayerIdx = playerIdx;
					}
				}
				if (topPlayerRating != -1)
					creditedPlayerIdx = topPlayerIdx;
			} else {
				uint32_t maxAiDamage;

				maxAiDamage = 0;
				for (aiSkillIdx = 0; aiSkillIdx < AI_SKILL_COUNT; ++aiSkillIdx) {
					if ((uint32_t)craft->damageStats.damageFromAiSkill[aiSkillIdx] > maxAiDamage)
						maxAiDamage = (uint32_t)craft->damageStats.damageFromAiSkill[aiSkillIdx];
				}
			}
		}
	}
	return creditedPlayerIdx;
}

// FUNCTION: XVT 0x435530
void Mission_RecordPlayerCraftLossAttribution(int attackerFlightGroupIdx, int victimObjIdx,
											  int contributionTier) {
	ObjectRecord* victim;
	int victimPlayerIdx;
	int attackerPlayerIdx;
	unsigned int damagePercent;

	if (contributionTier != 2 && contributionTier != 3) {
		return;
	}

	victim = &g_objectTable[victimObjIdx];
	victimPlayerIdx = victim->playerOwnerIdx;
	if (victimPlayerIdx == -1) {
		victimPlayerIdx = g_missionFlightGroups[victim->flightGroupIdx].playerOwnerIdx;
		if (victimPlayerIdx == -1) {
			return;
		}
	}

	damagePercent =
		MATH2_percentage((unsigned int)victim->mobj->pCraft->damageStats.damageReceivedByPlayerOwnedCraft,
						 (unsigned int)victim->mobj->pCraft->damageStats.damageReceivedTotal);
	if ((uint16_t)damagePercent < 0x8000u) {
		return;
	}

	attackerPlayerIdx = g_missionFlightGroups[attackerFlightGroupIdx].playerOwnerIdx;
	if (attackerPlayerIdx != -1) {
		unsigned int pilotRating;

		pilotRating = g_players[attackerPlayerIdx].pilotRating;
		if (contributionTier == 3) {
			++g_players[victimPlayerIdx].perMissionKills.killedByPlayerRating[pilotRating];
		}
		return;
	}

	{
		unsigned int groupAI;

		groupAI = g_missionFlightGroups[attackerFlightGroupIdx].fg.groupAI;
		if (contributionTier == 3) {
			++g_players[victimPlayerIdx].perMissionKills.killedByAiRating[groupAI];
		}
	}
}

// FUNCTION: XVT 0x435640
int Mission_ApplyFlightGroupGoalScore(int16_t eventCondition, uint16_t flightGroupIdx, int playerIdx,
									  uint16_t scoreDivisor, int specialCargoFlag, int teamIdx) {
	unsigned int missionTimeSeconds;
	int goalIndex;
	int scoreTotal;
	unsigned int timeLimitSeconds;
	FlightGroupGoal* goal;
	int score;
	int scoreTenth;
	int scoreAdjustment;
	unsigned int flightGroupNumber;

	missionTimeSeconds = Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
												   g_missionElapsedClock.seconds);
	scoreTotal = 0;
	flightGroupNumber = flightGroupIdx;
	goal = g_missionFlightGroups[flightGroupIdx].fg.goals;
	goalIndex = 0;
	do {
		if ((goal->type == 2 && goal->eventCondition == (uint16_t)eventCondition && goal->amount == 18 &&
			 g_missionFlightGroups[flightGroupNumber].fg.goals[goalIndex].enabledTeams[teamIdx] != 0) ||
			(goal->type == 2 && goal->eventCondition == (uint16_t)eventCondition && goal->amount == 19 &&
			 g_missionFlightGroups[flightGroupNumber].fg.goals[goalIndex].enabledTeams[teamIdx] != 0 &&
			 specialCargoFlag == 1)) {
			timeLimitSeconds = 5 * goal->timeLimit5s;
			if (timeLimitSeconds == 0 || missionTimeSeconds <= timeLimitSeconds) {
				score = 250 * goal->points;
				scoreTenth = score / 10;
				if (scoreDivisor > 10) {
					scoreDivisor = 9;
				}
				if (scoreDivisor > 1) {
					if (score > 0) {
						scoreAdjustment = 1;
						scoreAdjustment -= scoreDivisor;
						scoreAdjustment *= scoreTenth;
						score += scoreAdjustment;
					} else {
						score += scoreTenth * (scoreDivisor - 1);
					}
				}
				if (playerIdx != -1) {
					g_players[playerIdx].missionStats.missionScore += score;
				} else {
					g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx] += score;
				}
				scoreTotal += score;
			}
		}
		++goal;
		++goalIndex;
	} while (goalIndex < 8);

	if (scoreTotal != 0 && playerIdx == g_localPlayer) {
		if (scoreTotal > 0) {
			g_msgArgTable[0] = (uint16_t)scoreTotal;
			msg_emitInFlightMessage(IFMSG_199_BONUS_POINTS_AWARDED_ARG, playerIdx);
		} else {
			g_msgArgTable[0] = (uint16_t)-scoreTotal;
			msg_emitInFlightMessage(IFMSG_200_PENALTY_POINTS_DEDUCTED_ARG, playerIdx);
		}
	}
	return scoreTotal;
}

// FUNCTION: XVT 0x435850
void Mission_ApplyTeamGoalScoreAllEnabledTeams(int16_t eventCondition, uint16_t flightGroupIdx,
											   int specialCargoFlag) {
	int score;
	int remainingGoals;
	FlightGroupGoal* goal;
	unsigned int elapsedSeconds;
	unsigned int timeLimitSeconds;
	int teamIndex;

	goal = g_missionFlightGroups[flightGroupIdx].fg.goals;
	remainingGoals = 8;
	do {
		if (goal->type == 2 && goal->eventCondition == (uint16_t)eventCondition &&
			(goal->amount == 18 || (goal->amount == 19 && specialCargoFlag == 1))) {
			elapsedSeconds = Mission_GameTimeToSeconds(
				g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
			timeLimitSeconds = 5 * goal->timeLimit5s;
			if (timeLimitSeconds == 0 || elapsedSeconds <= timeLimitSeconds) {
				score = goal->points;
			}
			score *= 250;
			teamIndex = 0;
			do {
				if (goal->enabledTeams[teamIndex] != 0) {
					g_flightMissionState.runtime.teamScores[0][teamIndex] += score;
				}
				++teamIndex;
			} while (teamIndex < 10);
		}
		goal++;
		--remainingGoals;
	} while (remainingGoals != 0);
}

// FUNCTION: XVT 0x435930
void Mission_ApplyTeamGoalScoreForTeam(int16_t eventCondition, uint16_t flightGroupIdx, int specialCargoFlag,
									   uint8_t teamIdx) {
	int remainingGoals;
	uint8_t* enabledTeam;
	int score;
	FlightGroupGoal* goal;
	unsigned int elapsedSeconds;
	unsigned int timeLimitSeconds;
	int teamIndex;

	goal = g_missionFlightGroups[flightGroupIdx].fg.goals;
	teamIndex = teamIdx;
	enabledTeam = &goal->enabledTeams[teamIndex];
	remainingGoals = 8;
	do {
		if (goal->type == 2 && goal->eventCondition == (uint16_t)eventCondition &&
			(goal->amount == 18 || (goal->amount == 19 && specialCargoFlag == 1))) {
			elapsedSeconds = Mission_GameTimeToSeconds(
				g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
			timeLimitSeconds = 5 * goal->timeLimit5s;
			if (timeLimitSeconds == 0 || elapsedSeconds <= timeLimitSeconds) {
				score = goal->points;
			}
			score *= 250;
			if (*enabledTeam != 0) {
				g_flightMissionState.runtime.teamScores[0][teamIndex] += score;
			}
		}
		enabledTeam += sizeof(*goal);
		goal++;
		--remainingGoals;
	} while (remainingGoals != 0);
}

// FUNCTION: XVT 0x435A10
int Mission_GameTimeToSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	return 60 * (minutes + 60 * hours) + seconds;
}

// FUNCTION: XVT 0x435A40
int Mission_ComputeKillScoreForObject(int victimObjIdx) {
	unsigned int objectType;
	int killScore;

	objectType = g_craftTypeToObjectType[g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx]
											 .fg.craftType];
	if (g_modelTypeTable[objectType].familyId == CRAFT_FAMILY_SPACE_CRAFT) {
		return Mission_ComputeCraftPointValue(victimObjIdx);
	}
	if ((objectType >= 0x46 && objectType <= 0x4A) || (objectType >= 0x50 && objectType <= 0x54)) {
		killScore = 500;
	} else {
		killScore = 40;
	}
	if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
		killScore *= 3;
	}
	return killScore;
}

// FUNCTION: XVT 0x435AD0
int Mission_ComputeCraftPointValue(int objIdx) {
	CraftData* craft;
	unsigned int modelIndex;
	unsigned int pointValue;
	unsigned int launcherIndex;
	unsigned int beamType;
	unsigned int countermeasureType;

	craft = g_objectTable[objIdx].mobj->pCraft;
	modelIndex = GetModelIndexFromType(g_objectTable[objIdx].objectType);
	pointValue = 40 * g_modelDefs[modelIndex].craftPointValue;
	if (g_objectTable[objIdx].objectType == CRAFT_SPECIES_SUPER_STAR_DESTROYER) {
		pointValue *= 2;
	}

	for (launcherIndex = 0; launcherIndex < craft->warheadLauncherCount; ++launcherIndex) {
		unsigned int firstSlot;
		unsigned int lastSlot;
		unsigned int warheadType;
		unsigned int weaponCount;

		firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIndex];
		weaponCount = craft->weaponSlots[firstSlot].count;
		lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIndex];
		weaponCount += craft->weaponSlots[lastSlot].count;
		warheadType = craft->warheadSlotTypeIds[launcherIndex];
#ifdef XVT_MODERN
		if (warheadType >= PROJECTILE_OBJECT_TYPE_FIRST &&
			warheadType < PROJECTILE_OBJECT_TYPE_FIRST + PROJECTILE_OBJECT_TYPE_COUNT) {
			pointValue +=
				g_projectileDamageByObjectType.warheadPointValue[warheadType - PROJECTILE_OBJECT_TYPE_FIRST] *
				weaponCount;
		}
#else
		pointValue +=
			g_projectileDamageByObjectType.warheadPointValue[warheadType - PROJECTILE_OBJECT_TYPE_FIRST] *
			weaponCount;
#endif
	}

	countermeasureType = (uint8_t)craft->cmTypeId;
	beamType = (uint8_t)craft->beamTypeId;
#ifdef XVT_MODERN
	if (countermeasureType >=
		sizeof(g_countermeasureTypePointValue) / sizeof(g_countermeasureTypePointValue[0])) {
		countermeasureType = 0;
	}
	if (beamType >= sizeof(g_beamTypePointValue) / sizeof(g_beamTypePointValue[0])) {
		beamType = 0;
	}
#endif
	pointValue += g_countermeasureTypePointValue[countermeasureType];
	pointValue += g_beamTypePointValue[beamType];
	return pointValue;
}

// FUNCTION: XVT 0x448D70
int Mission_GetElapsedClockSeconds(void) {
	return 60 * (g_missionElapsedClock.minutes + 60 * g_missionElapsedClock.hours) +
		   g_missionElapsedClock.seconds;
}

// FUNCTION: XVT 0x452CE0
uint16_t Mission_Init(char* fileName) {
	enum {
		PLAYER_COUNT = 8,
		TEAM_COUNT = 10,
		CRAFT_SLOT_COUNT = 32,
		PLAYER_PROJECTILE_SLOT_COUNT = 128,
		OTHER_PROJECTILE_SLOT_COUNT = 32,
		DEBRIS_SLOT_COUNT = 16,
		EXPLOSION_SLOT_COUNT = 16,
		CHAR_DATA_SLOT_COUNT = 256,
		STATIC_OBJECT_SLOT_COUNT = 64,
		LOCAL_DEBRIS_SLOT_COUNT = 8,
		MOBILE_OBJECT_SLOT_COUNT = 488,
		MISSION_MESSAGE_COUNT_USED = 64,
		MODEL_TYPE_COUNT = 201,
		OPTIONAL_CRAFT_COUNT = 10,
		OPTIONAL_WARHEAD_COUNT = 8,
		OPTIONAL_BEAM_COUNT = 6,
		OPTIONAL_COUNTERMEASURE_COUNT = 4,
		CRAFT_GENUS_RANGE_LIMIT = 6,
		RESERVED_GENUS_RANGE_FIRST = 8,
		RESERVED_GENUS_RANGE_LIMIT = 11,
		DEBRIS_GENUS_RANGE = 11,
		RESERVED_GENUS_RANGE_12 = 12,
		RESERVED_GENUS_RANGE_14 = 14,
		RESERVED_GENUS_RANGE_15 = 15,
		CHAR_DATA_GENUS_RANGE = 16,
		MODEL_ASSET_REQUIRED = 0x10,
		MODEL_FLAG_BACKDROP = 0x20,
		AI_FIGHTER_OBJECT_TYPE_FIRST = 1,
		AI_FIGHTER_OBJECT_TYPE_LIMIT = 4,
		AI_HEADHUNTER_OBJECT_TYPE = 14,
		MAX_GROUP_AI = 5,
		RANDOM_OPTIONAL_CRAFT_CATEGORY = 4,
		UNLIMITED_PLAYER_WAVES = 99,
		MISSION_POINT_REF_BASE = 0x8000,
		MISSION_POINT_VARIANT_COUNT = 3,
		MULTIPART_MODEL_TYPE = 100,
		MULTIPART_FIRST_DEPENDENT_MODEL_TYPE = 101,
		MULTIPART_LAST_DEPENDENT_MODEL_TYPE = 105,
		BACKDROP_DIRECTION_COUNT = 6,
		BACKDROP_DIRECTION_MAX = BACKDROP_DIRECTION_COUNT - 1,
		BACKDROP_RANDOM_SEED_BIAS = 16657,
		BACKDROP_SOURCE_MODEL_TYPE = 87,
		BACKDROP_FIRST_FREE_MODEL_TYPE = 114,
		BACKDROP_MODEL_TYPE_LIMIT = 116,
		PALETTED_BYTES_PER_PIXEL = 1,
		BACKDROP_BASE_STATUS_COUNT = 8,
		BACKDROP_STATUS_COUNT = 17,
		BACKDROP_STATUS_FRAME_OFFSET = 85,
	};

	int teamPlayerFgCounts[TEAM_COUNT];
	int teamOwnedFlightGroup[TEAM_COUNT];
	int teamPlayerOwnerCounts[TEAM_COUNT];
	uint16_t flightGroupIdx;
	uint16_t slot;
	int teamIdx;
	int playerOwnedTeamCount;

	FlightSurface_Unlock();
	g_craftDataPoolCapacity = CRAFT_SLOT_COUNT;
	g_debrisObjectSlotsTotal = DEBRIS_SLOT_COUNT;
	g_worldStateReservedDword = DEBRIS_SLOT_COUNT;
	g_activeRegionObjectSlotStart = 0;
	g_activeRegionCraftObjectSlotEnd = CRAFT_SLOT_COUNT;
	g_projectileObjectSlotStart = CRAFT_SLOT_COUNT;
	g_projectileObjectSlotEnd = CRAFT_SLOT_COUNT + PLAYER_PROJECTILE_SLOT_COUNT + OTHER_PROJECTILE_SLOT_COUNT;
	g_debrisObjectSlotStart = g_projectileObjectSlotEnd;
	g_mobileObjectCharDataCount = CHAR_DATA_SLOT_COUNT;
	g_projectileObjectSlotsTotal = PLAYER_PROJECTILE_SLOT_COUNT + OTHER_PROJECTILE_SLOT_COUNT;
	g_regionMainObjectSlotStart = LOCAL_DEBRIS_SLOT_COUNT;
	g_debrisObjectSlotEnd = g_debrisObjectSlotStart + DEBRIS_SLOT_COUNT;
	g_explosionObjectSlotStart = g_debrisObjectSlotEnd;
	g_explosionObjectSlotEnd = g_explosionObjectSlotStart + EXPLOSION_SLOT_COUNT;
	g_mobileObjectCharDataSlotStart = g_explosionObjectSlotEnd;
	g_regionStaticObjectSlotCount = STATIC_OBJECT_SLOT_COUNT;
	g_mobileObjectCharDataSlotEnd = g_mobileObjectCharDataSlotStart + CHAR_DATA_SLOT_COUNT;
	g_localTransientSlotStart = g_mobileObjectCharDataSlotEnd;
	g_localDebrisSlotEnd = g_localTransientSlotStart + LOCAL_DEBRIS_SLOT_COUNT;
	g_regionMainObjectSlotEnd = g_localDebrisSlotEnd;

	if (g_objectTableHandle != 0) {
		Memory_UnlockHandle(g_objectTableHandle);
		Memory_FreeHandle(g_objectTableHandle);
		g_objectTableHandle = 0;
	}
	if (g_mobileObjectPoolHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectPoolHandle);
		Memory_FreeHandle(g_mobileObjectPoolHandle);
		g_mobileObjectPoolHandle = 0;
	}
	if (g_mobileObjectCharDataHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectCharDataHandle);
		Memory_FreeHandle(g_mobileObjectCharDataHandle);
		g_mobileObjectCharDataHandle = 0;
	}
	if (g_craftDataPoolHandle != 0) {
		Memory_UnlockHandle(g_craftDataPoolHandle);
		Memory_FreeHandle(g_craftDataPoolHandle);
		g_craftDataPoolHandle = 0;
	}
	if (g_warheadGuidancePoolHandle != 0) {
		Memory_UnlockHandle(g_warheadGuidancePoolHandle);
		Memory_FreeHandle(g_warheadGuidancePoolHandle);
		g_warheadGuidancePoolHandle = 0;
	}

	FeDiskIo_UnlockGlobalBuffers();
	g_objectTableHandle = Memory_AllocHandleZeroed(
		(sizeof(ObjectRecord) * (size_t)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount)), 0);
	g_mobileObjectPoolHandle =
		Memory_AllocHandleZeroed(sizeof(MobileObject) * (size_t)g_regionMainObjectSlotEnd, 0);
	g_mobileObjectCharDataHandle =
		Memory_AllocHandleZeroed(sizeof(MobileObjectCharData) * (size_t)g_mobileObjectCharDataCount, 0);
	g_craftDataPoolHandle = Memory_AllocHandleZeroed(sizeof(CraftData) * (size_t)g_craftDataPoolCapacity, 0);
	g_warheadGuidancePoolHandle = Memory_AllocHandleZeroed(
		sizeof(WarheadGuidanceState) * (size_t)(g_projectileObjectSlotsTotal + 1), 0);
	FeDiskIo_LockGlobalBuffers();

	for (slot = 0; slot < MOBILE_OBJECT_SLOT_COUNT; ++slot) {
		g_mobileObjectLinkIndices[slot].warheadGuidanceIdx = -1;
		g_mobileObjectLinkIndices[slot].craftDataIdx = -1;
		g_mobileObjectLinkIndices[slot].charDataIdx = -1;
	}
	memset(g_spawnObjectTypeByObjectSlot, 0xFF, sizeof(g_spawnObjectTypeByObjectSlot));
	for (slot = 0; slot < CRAFT_GENUS_RANGE_LIMIT; ++slot) {
		g_objectSlotRangeByGenus[slot].start = (uint16_t)g_activeRegionObjectSlotStart;
		g_objectSlotRangeByGenus[slot].end = (uint16_t)g_activeRegionCraftObjectSlotEnd;
	}
	g_objectSlotRangeByGenus[CRAFT_GENUS_PLAYER_PROJECTILE].start = (uint16_t)g_projectileObjectSlotStart;
	g_objectSlotRangeByGenus[CRAFT_GENUS_PLAYER_PROJECTILE].end =
		(uint16_t)(g_projectileObjectSlotStart + PLAYER_PROJECTILE_SLOT_COUNT);
	g_objectSlotRangeByGenus[CRAFT_GENUS_OTHER_PROJECTILE].start =
		(uint16_t)(g_projectileObjectSlotStart + PLAYER_PROJECTILE_SLOT_COUNT);
	g_objectSlotRangeByGenus[CRAFT_GENUS_OTHER_PROJECTILE].end = (uint16_t)g_projectileObjectSlotEnd;
	for (slot = RESERVED_GENUS_RANGE_FIRST; slot < RESERVED_GENUS_RANGE_LIMIT; ++slot) {
		g_objectSlotRangeByGenus[slot].start = 0;
		g_objectSlotRangeByGenus[slot].end = 0;
	}
	g_objectSlotRangeByGenus[DEBRIS_GENUS_RANGE].start = (uint16_t)g_debrisObjectSlotStart;
	g_objectSlotRangeByGenus[DEBRIS_GENUS_RANGE].end = (uint16_t)g_debrisObjectSlotEnd;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_12].start = 0;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_12].end = 0;
	g_objectSlotRangeByGenus[CRAFT_GENUS_EXPLOSION].start = (uint16_t)g_explosionObjectSlotStart;
	g_objectSlotRangeByGenus[CRAFT_GENUS_EXPLOSION].end = (uint16_t)g_explosionObjectSlotEnd;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_14].start = 0;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_14].end = 0;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_15].start = 0;
	g_objectSlotRangeByGenus[RESERVED_GENUS_RANGE_15].end = 0;
	g_objectSlotRangeByGenus[CHAR_DATA_GENUS_RANGE].start = (uint16_t)g_mobileObjectCharDataSlotStart;
	g_objectSlotRangeByGenus[CHAR_DATA_GENUS_RANGE].end = (uint16_t)g_mobileObjectCharDataSlotEnd;
	g_simStepScale = 0;
	g_flightMissionState.provingGroundsModeActive = g_flightMissionState.provingGroundsCraftType;
	for (slot = 0; slot < g_mobileObjectCharDataCount; ++slot) {
		memset(&g_mobileObjectCharDataPool[slot], 0, sizeof(g_mobileObjectCharDataPool[slot]));
	}
	for (slot = 0; slot < g_regionMainObjectSlotEnd; ++slot) {
		g_mobileObjectPoolBase[slot].framesAlive = 0;
		g_mobileObjectPoolBase[slot].lifetimeTimer = 0;
		g_mobileObjectPoolBase[slot].simStateTimestamp = 0;
		g_mobileObjectPoolBase[slot].orientMatrixDirty = 0;
		g_mobileObjectPoolBase[slot].pCraft = NULL;
		g_mobileObjectPoolBase[slot].pWarheadGuidance = NULL;
		g_mobileObjectPoolBase[slot].pCharData = NULL;
	}
	for (slot = 0; slot < g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount; ++slot) {
		g_objectTable[slot].objectType = 0;
		g_objectTable[slot].playerOwnerIdx = -1;
		if (slot < g_regionMainObjectSlotEnd)
			g_objectTable[slot].mobj = &g_mobileObjectPoolBase[slot];
		else
			g_objectTable[slot].mobj = NULL;
	}
	for (slot = (uint16_t)g_activeRegionObjectSlotStart; slot < g_activeRegionCraftObjectSlotEnd; ++slot) {
		g_objectTable[slot].mobj->iff = -1;
	}
	for (slot = 0; slot < PLAYER_COUNT; ++slot) {
		g_players[slot].objectIndex = -1;
		g_players[slot].boundObjectSignature = 0;
	}
	for (slot = 0; slot < MODEL_TYPE_COUNT; ++slot) {
		if (g_modelTypeTable[slot].recordFlags != 0)
			g_modelTypeTable[slot].assetFlags &= (uint8_t)~MODEL_ASSET_REQUIRED;
	}
	for (slot = 0; slot < MISSION_MESSAGE_COUNT_USED; ++slot)
		g_missionMessages[slot].message[0] = 0;
	for (slot = 0; slot < sizeof(g_flightMissionState.globalUnitCraftCount) /
							  sizeof(g_flightMissionState.globalUnitCraftCount[0]);
		 ++slot) {
		g_flightMissionState.globalUnitCraftCount[slot] = 0;
	}
	memset(g_flightMissionState.runtime.teamHasCountableCraft, 0,
		   sizeof(g_flightMissionState.runtime.teamHasCountableCraft));
	g_flightMissionState.provingGroundsModeActive = 0;
	g_flightMissionState.provingGroundsCraftType = 0;
	g_flightMissionState.provingGroundsLevel = 0;
	g_flightMissionState.provingGroundsScore = 0;
	memset(g_flightMissionState.reserved08, 0, sizeof(g_flightMissionState.reserved08));
	g_flightMissionState.provingGroundsCheckpointsPassed = 0;
	memset(g_flightMissionState.reserved0C, 0, sizeof(g_flightMissionState.reserved0C));
	g_flightMissionState.provingGroundsCheckpointsRemaining = 0;
	g_flightMissionState.provingGroundsTargetsDestroyed = 0;
	g_flightMissionState.provingGroundsTimeBonus = 0;

	if (Mission_LoadFile(fileName) == 0)
		return 0;

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		g_missionFlightGroups[flightGroupIdx].playerOwnerIdx = -1;
	}

	if (g_flightConfNoPilot == 0) {
		int selectedCraftType = 0;
		int selectedWarhead = 0;

		for (slot = 0; slot < PLAYER_COUNT; ++slot) {
			if (g_pilotData.networkPlayers[slot].directPlayId != 0) {
				uint16_t sourceFlightGroup = (uint16_t)g_pilotData.networkPlayers[slot].flightGroupId;

				selectedCraftType = g_missionFlightGroups[sourceFlightGroup].fg.craftType;
				selectedWarhead = g_missionFlightGroups[sourceFlightGroup].fg.warhead;
				break;
			}
		}
		for (slot = 0; slot < PLAYER_COUNT; ++slot) {
			if (g_pilotData.networkPlayers[slot].directPlayId != 0) {
				uint16_t assignedFlightGroup = (uint16_t)g_pilotData.networkPlayers[slot].flightGroupId;
				unsigned int playerSlot =
					NetSession_FindPlayerSlotByDpid(g_pilotData.networkPlayers[slot].directPlayId);

				if (playerSlot < PLAYER_COUNT) {
					int craftType;

					g_missionFlightGroups[assignedFlightGroup].playerOwnerIdx = (int)playerSlot;
					g_players[playerSlot].iff = g_missionFlightGroups[assignedFlightGroup].fg.iff;
					g_players[playerSlot].playerIff = g_missionFlightGroups[assignedFlightGroup].fg.team;
					if (g_pilotData.networkPlayers[slot].craftId != 0) {
						g_missionFlightGroups[assignedFlightGroup].fg.craftType =
							(CraftSpecies)g_pilotData.networkPlayers[slot].craftId;
					} else if (g_pilotData.networkPlayers[slot].craftOption != -1 &&
							   g_pilotData.networkPlayers[slot].craftOption < OPTIONAL_CRAFT_COUNT) {
						g_missionFlightGroups[assignedFlightGroup].fg.craftType =
							g_missionFlightGroups[assignedFlightGroup]
								.fg.optionalCraft[g_pilotData.networkPlayers[slot].craftOption];
						g_missionFlightGroups[assignedFlightGroup].fg.numberOfCraft =
							g_missionFlightGroups[assignedFlightGroup]
								.fg.numberOfOptionalCraft[g_pilotData.networkPlayers[slot].craftOption];
						g_missionFlightGroups[assignedFlightGroup].fg.numberOfWaves =
							g_missionFlightGroups[assignedFlightGroup]
								.fg.numberOfOptionalCraftWaves[g_pilotData.networkPlayers[slot].craftOption];
					}
					if (g_pilotData.networkPlayers[slot].warheadOption != -1 &&
						g_pilotData.networkPlayers[slot].warheadOption < OPTIONAL_WARHEAD_COUNT) {
						g_missionFlightGroups[assignedFlightGroup].fg.warhead =
							g_missionFlightGroups[assignedFlightGroup]
								.fg.optionalWarheads[g_pilotData.networkPlayers[slot].warheadOption];
					}
					if (g_pilotData.networkPlayers[slot].beamOption != -1 &&
						g_pilotData.networkPlayers[slot].beamOption < OPTIONAL_BEAM_COUNT) {
						craftType = g_missionFlightGroups[assignedFlightGroup].fg.craftType;
						if (craftType >= CRAFT_SPECIES_X_WING &&
							(craftType <= CRAFT_SPECIES_TIE_FIGHTER ||
							 craftType == CRAFT_SPECIES_Z_95_HEADHUNTER)) {
							g_missionFlightGroups[assignedFlightGroup].fg.beam = 0;
						} else {
							g_missionFlightGroups[assignedFlightGroup].fg.beam =
								g_missionFlightGroups[assignedFlightGroup]
									.fg.optionalBeams[g_pilotData.networkPlayers[slot].beamOption];
						}
					}
					if (g_pilotData.networkPlayers[slot].countermeasureOption != -1 &&
						g_pilotData.networkPlayers[slot].countermeasureOption <
							OPTIONAL_COUNTERMEASURE_COUNT) {
						g_missionFlightGroups[assignedFlightGroup].fg.countermeasures =
							g_missionFlightGroups[assignedFlightGroup].fg.optionalCountermeasures
								[g_pilotData.networkPlayers[slot].countermeasureOption];
					}
				}
			}
		}

		for (slot = 0; slot < TEAM_COUNT; ++slot)
			teamPlayerOwnerCounts[slot] = 0;
		for (teamIdx = 0; teamIdx < (int16_t)g_missionHeader.numFlightGroups; ++teamIdx) {
			if (g_missionFlightGroups[teamIdx].playerOwnerIdx != -1) {
				++teamPlayerOwnerCounts[g_missionFlightGroups[teamIdx].fg.team];
			}
		}
		playerOwnedTeamCount = 0;
		for (slot = 0; slot < TEAM_COUNT; ++slot) {
			if (teamPlayerOwnerCounts[slot] != 0)
				++playerOwnedTeamCount;
		}
		if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START && playerOwnedTeamCount == 1)
			g_flightMissionState.aiOpponentsEnabled = 1;

		if (g_gameConfig.craftSelection == CRAFT_SELECTION_HOST_ONLY ||
			(g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
			 g_pilotData.numHumanPlayersLastMission == 1 &&
			 (g_pilotData.missionSequenceActive != 1 ||
			  g_pilotData.meleeTournamentSequenceState.humanPlayerCount == 1)) ||
			(g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
			 g_gameConfig.craftSelection == CRAFT_SELECTION_OFF && g_pilotData.missionSequenceActive == 1)) {
			uint16_t sourceFlightGroup;

			for (sourceFlightGroup = 0; sourceFlightGroup < PLAYER_COUNT; ++sourceFlightGroup) {
				if (g_pilotData.networkPlayers[sourceFlightGroup].directPlayId != 0) {
					sourceFlightGroup = (uint16_t)g_pilotData.networkPlayers[sourceFlightGroup].flightGroupId;
					break;
				}
			}
			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
					if (g_missionFlightGroups[flightGroupIdx].fg.craftType == selectedCraftType &&
						g_missionFlightGroups[flightGroupIdx].fg.warhead == selectedWarhead) {
						g_missionFlightGroups[flightGroupIdx].fg.warhead =
							g_missionFlightGroups[sourceFlightGroup].fg.warhead;
					}
					g_missionFlightGroups[flightGroupIdx].fg.craftType =
						g_missionFlightGroups[sourceFlightGroup].fg.craftType;
					g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft =
						g_missionFlightGroups[sourceFlightGroup].fg.numberOfCraft;
					g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves =
						g_missionFlightGroups[sourceFlightGroup].fg.numberOfWaves;
					g_missionFlightGroups[flightGroupIdx].fg.beam =
						g_missionFlightGroups[sourceFlightGroup].fg.beam;
					g_missionFlightGroups[flightGroupIdx].fg.countermeasures =
						g_missionFlightGroups[sourceFlightGroup].fg.countermeasures;
				}
			}
		} else if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
				   g_gameConfig.craftSelection == CRAFT_SELECTION_ON) {
			for (slot = 0; slot < TEAM_COUNT; ++slot) {
				teamPlayerFgCounts[slot] = 0;
				teamPlayerOwnerCounts[slot] = 0;
				teamOwnedFlightGroup[slot] = 0;
			}
			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
					int flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;

					++teamPlayerFgCounts[flightGroupTeam];
					if (g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1) {
						++teamPlayerOwnerCounts[flightGroupTeam];
						teamOwnedFlightGroup[flightGroupTeam] = flightGroupIdx;
					}
				}
			}
			playerOwnedTeamCount = 0;
			for (slot = 0; slot < TEAM_COUNT; ++slot) {
				if (teamPlayerOwnerCounts[slot] != 0)
					++playerOwnedTeamCount;
			}
			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0 &&
					g_missionFlightGroups[flightGroupIdx].playerOwnerIdx == -1 &&
					teamPlayerOwnerCounts[g_missionFlightGroups[flightGroupIdx].fg.team] != 0) {
					uint16_t sourceFg =
						(uint16_t)teamOwnedFlightGroup[g_missionFlightGroups[flightGroupIdx].fg.team];

					g_missionFlightGroups[flightGroupIdx].fg.craftType =
						g_missionFlightGroups[sourceFg].fg.craftType;
					g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft =
						g_missionFlightGroups[sourceFg].fg.numberOfCraft;
					g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves =
						g_missionFlightGroups[sourceFg].fg.numberOfWaves;
					g_missionFlightGroups[flightGroupIdx].fg.warhead =
						g_missionFlightGroups[sourceFg].fg.warhead;
					g_missionFlightGroups[flightGroupIdx].fg.beam = g_missionFlightGroups[sourceFg].fg.beam;
					g_missionFlightGroups[flightGroupIdx].fg.countermeasures =
						g_missionFlightGroups[sourceFg].fg.countermeasures;
				}
			}
			if (g_flightMissionState.aiOpponentsEnabled == 1) {
				for (slot = 0; slot < TEAM_COUNT; ++slot) {
					if (teamPlayerFgCounts[slot] != 0 && teamPlayerOwnerCounts[slot] == 0) {
						int sourceTeam;
						uint16_t sourceFg;

						if (g_pilotData.missionSequenceActive == 1 &&
							g_pilotData.meleeTournamentSequenceState.currentMissionIndex != 0) {
							int sourceTeamAndType =
								g_pilotData.meleeTournamentSequenceState.teamStandings[slot]
									.aiOpponentSourceTeamAndTypeFlag;

							sourceTeam = sourceTeamAndType & INT32_MAX;
							if (teamPlayerOwnerCounts[sourceTeam] == 0) {
								uint16_t targetFg = 0;
								int replaceCraftType = 0;

								while (targetFg < (int16_t)g_missionHeader.numFlightGroups) {
									if (g_missionFlightGroups[targetFg].fg.team == slot &&
										g_missionFlightGroups[targetFg].fg.playerNumber != 0) {
										break;
									}
									{
										int objectType =
											g_craftTypeToObjectType[g_missionFlightGroups[targetFg]
																		.fg.craftType];

										if (objectType >= AI_FIGHTER_OBJECT_TYPE_FIRST &&
											(objectType <= AI_FIGHTER_OBJECT_TYPE_LIMIT ||
											 objectType == AI_HEADHUNTER_OBJECT_TYPE)) {
											if ((sourceTeamAndType & INT32_MIN) == 0)
												replaceCraftType = 1;
										} else if ((sourceTeamAndType & INT32_MIN) != 0) {
											replaceCraftType = 1;
										}
									}
									++targetFg;
								}
								if (replaceCraftType != 0) {
									uint8_t replacement =
										g_aiOpponentCraftTypeByPlayerCraftType[g_missionFlightGroups[targetFg]
																				   .fg.craftType];

									for (flightGroupIdx = 0;
										 flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
										 ++flightGroupIdx) {
										if (g_missionFlightGroups[flightGroupIdx].fg.team == slot &&
											g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
											g_missionFlightGroups[flightGroupIdx].fg.craftType = replacement;
										}
									}
								}
								sourceFg = (uint16_t)g_missionHeader.numFlightGroups;
							} else {
								sourceFg = teamOwnedFlightGroup[sourceTeam];
							}
						} else {
							int selectedOrdinal = GameRandRange((uint16_t)playerOwnedTeamCount);
							int ordinal = 0;
							uint16_t candidateTeam;

							for (candidateTeam = 0; candidateTeam < TEAM_COUNT; ++candidateTeam) {
								if (teamPlayerFgCounts[candidateTeam] != 0 &&
									teamPlayerOwnerCounts[candidateTeam] != 0) {
									if (ordinal == selectedOrdinal) {
										sourceTeam = candidateTeam;
										break;
									}
									++ordinal;
								}
							}
#ifdef XVT_MODERN
							if (candidateTeam == TEAM_COUNT)
								continue;
#endif
							if (g_pilotData.missionSequenceActive == 1 &&
								g_pilotData.meleeTournamentSequenceState.teamStandings[slot]
										.aiOpponentSourceTeamAndTypeFlag != -1) {
								int sourceCraftType =
									g_missionFlightGroups[teamOwnedFlightGroup[sourceTeam]].fg.craftType;
								int sourceObjectType = g_craftTypeToObjectType[sourceCraftType];

								if (sourceObjectType >= AI_FIGHTER_OBJECT_TYPE_FIRST &&
									(sourceObjectType <= AI_FIGHTER_OBJECT_TYPE_LIMIT ||
									 sourceObjectType == AI_HEADHUNTER_OBJECT_TYPE)) {
									g_pilotData.meleeTournamentSequenceState.teamStandings[slot]
										.aiOpponentSourceTeamAndTypeFlag = sourceTeam | INT32_MIN;
								} else {
									g_pilotData.meleeTournamentSequenceState.teamStandings[slot]
										.aiOpponentSourceTeamAndTypeFlag = sourceTeam;
								}
							}
							sourceFg = (uint16_t)teamOwnedFlightGroup[sourceTeam];
						}

						if (sourceFg != g_missionHeader.numFlightGroups) {
							for (flightGroupIdx = 0;
								 flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
								 ++flightGroupIdx) {
								if (g_missionFlightGroups[flightGroupIdx].fg.team == slot &&
									g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
									g_missionFlightGroups[flightGroupIdx].fg.craftType =
										g_missionFlightGroups[sourceFg].fg.craftType;
									g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft =
										g_missionFlightGroups[sourceFg].fg.numberOfCraft;
									g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves =
										g_missionFlightGroups[sourceFg].fg.numberOfWaves;
									g_missionFlightGroups[flightGroupIdx].fg.warhead =
										g_missionFlightGroups[sourceFg].fg.warhead;
									g_missionFlightGroups[flightGroupIdx].fg.beam =
										g_missionFlightGroups[sourceFg].fg.beam;
									g_missionFlightGroups[flightGroupIdx].fg.countermeasures =
										g_missionFlightGroups[sourceFg].fg.countermeasures;
								}
							}
						}
					}
				}
			}
		}
	} else {
		for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIdx) {
			int playerNumber = g_missionFlightGroups[flightGroupIdx].fg.playerNumber;

			if (playerNumber <= g_activeFlightPlayerCount && playerNumber != 0) {
				int playerIdx = playerNumber - 1;

				g_missionFlightGroups[flightGroupIdx].playerOwnerIdx = playerIdx;
				g_players[playerIdx].iff = g_missionFlightGroups[flightGroupIdx].fg.iff;
				g_players[playerIdx].playerIff = g_missionFlightGroups[flightGroupIdx].fg.team;
			} else {
				g_missionFlightGroups[flightGroupIdx].playerOwnerIdx = -1;
			}
		}
	}

	if (g_flightMissionState.randomVariationEnabled != 0) {
		for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIdx) {
			if (g_missionFlightGroups[flightGroupIdx].playerOwnerIdx == -1 &&
				g_missionFlightGroups[flightGroupIdx].fg.optionalCraftCategory ==
					RANDOM_OPTIONAL_CRAFT_CATEGORY) {
				unsigned int optionCount = 1;
				unsigned int selectedOption;
				unsigned int optionIdx;

				for (optionIdx = 0; optionIdx < OPTIONAL_CRAFT_COUNT; ++optionIdx) {
					if (g_missionFlightGroups[flightGroupIdx].fg.optionalCraft[optionIdx] !=
						CRAFT_SPECIES_UNKNOWN) {
						++optionCount;
					}
				}
				selectedOption = GameRandRange((uint16_t)optionCount);
				if (optionCount > 1 && selectedOption != 0) {
					unsigned int ordinal = 1;

					for (optionIdx = 0; optionIdx < OPTIONAL_CRAFT_COUNT; ++optionIdx) {
						if (g_missionFlightGroups[flightGroupIdx].fg.optionalCraft[optionIdx] !=
							CRAFT_SPECIES_UNKNOWN) {
							if (ordinal == selectedOption)
								break;
							++ordinal;
						}
					}
					if (g_missionFlightGroups[flightGroupIdx].fg.optionalCraft[optionIdx] !=
						CRAFT_SPECIES_UNKNOWN) {
						g_missionFlightGroups[flightGroupIdx].fg.craftType =
							g_missionFlightGroups[flightGroupIdx].fg.optionalCraft[optionIdx];
						g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft =
							g_missionFlightGroups[flightGroupIdx].fg.numberOfOptionalCraft[optionIdx];
						g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves =
							g_missionFlightGroups[flightGroupIdx].fg.numberOfOptionalCraftWaves[optionIdx];
					}
				}
			}
		}
	}

	if (g_flightMissionState.difficulty != GAME_DIFFICULTY_MEDIUM &&
		(g_missionHeader.missionType != MISSION_TYPE_SKIRMISH ||
		 g_pilotData.numHumanPlayersLastMission < 2)) {
		int playerIff = (uint16_t)g_players[0].playerIff;

		for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIdx) {
			if (g_flightMissionState.difficulty == GAME_DIFFICULTY_HARD) {
				int flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
				int hostile =
					playerIff == flightGroupTeam ? 0 : g_missionTeams[flightGroupTeam].allies[playerIff] == 0;

				if (hostile != 0) {
					g_missionFlightGroups[flightGroupIdx].fg.groupAI += 2;
					if (g_missionFlightGroups[flightGroupIdx].fg.groupAI > MAX_GROUP_AI)
						g_missionFlightGroups[flightGroupIdx].fg.groupAI = MAX_GROUP_AI;
				} else if (g_missionHeader.missionType != MISSION_TYPE_QUICK_START &&
						   (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TRAINING_EXERCISES ||
							g_pilotData.missionSequenceActive != 1 || g_flightPlayerCount != 1) &&
						   g_missionFlightGroups[flightGroupIdx].fg.groupAI != 0) {
					--g_missionFlightGroups[flightGroupIdx].fg.groupAI;
				}
			} else {
				int flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
				int hostile =
					playerIff == flightGroupTeam ? 0 : g_missionTeams[flightGroupTeam].allies[playerIff] == 0;

				if (hostile == 0) {
					g_missionFlightGroups[flightGroupIdx].fg.groupAI += 2;
					if (g_missionFlightGroups[flightGroupIdx].fg.groupAI > MAX_GROUP_AI)
						g_missionFlightGroups[flightGroupIdx].fg.groupAI = MAX_GROUP_AI;
				} else if (g_missionFlightGroups[flightGroupIdx].fg.groupAI != 0) {
					--g_missionFlightGroups[flightGroupIdx].fg.groupAI;
				}
			}
		}
	}

	if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
		int quickStartAiBoostTeam;
		int boostCount;

		if (g_pilotData.missionSequenceActive == 1 &&
			g_pilotData.meleeTournamentSequenceState.currentMissionIndex != 0) {
			quickStartAiBoostTeam = g_pilotData.meleeTournamentSequenceState.quickStartAiBoostTeam;
		} else {
			int teamsWithoutOwners = 0;
			int selectedOrdinal;
			int ordinal;
			int flightGroup;

			memset(teamOwnedFlightGroup, 0, sizeof(teamOwnedFlightGroup));
			memset(teamPlayerFgCounts, 0, sizeof(teamPlayerFgCounts));
			for (flightGroup = 0; flightGroup < g_missionHeader.numFlightGroups; ++flightGroup) {
				if (g_missionFlightGroups[flightGroup].fg.playerNumber != 0)
					++teamPlayerFgCounts[g_missionFlightGroups[flightGroup].fg.team];
				if (g_missionFlightGroups[flightGroup].playerOwnerIdx != -1)
					++teamOwnedFlightGroup[g_missionFlightGroups[flightGroup].fg.team];
			}
			for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
				if (teamPlayerFgCounts[teamIdx] != 0 && teamOwnedFlightGroup[teamIdx] == 0)
					++teamsWithoutOwners;
			}
			selectedOrdinal = GameRandRange((uint16_t)teamsWithoutOwners);
			ordinal = -1;
			for (quickStartAiBoostTeam = 0; quickStartAiBoostTeam < TEAM_COUNT; ++quickStartAiBoostTeam) {
				if (teamPlayerFgCounts[quickStartAiBoostTeam] != 0 &&
					teamOwnedFlightGroup[quickStartAiBoostTeam] == 0 && ++ordinal == selectedOrdinal) {
					break;
				}
			}
			if (quickStartAiBoostTeam == TEAM_COUNT)
				quickStartAiBoostTeam = teamPlayerFgCounts[0];
			if (g_pilotData.missionSequenceActive == 1) {
				g_pilotData.meleeTournamentSequenceState.quickStartAiBoostTeam = quickStartAiBoostTeam;
			}
		}
		if (g_flightMissionState.difficulty == GAME_DIFFICULTY_EASY)
			boostCount = 3;
		else if (g_flightMissionState.difficulty == GAME_DIFFICULTY_MEDIUM)
			boostCount = 2;
		else
			boostCount = 1;
		while (boostCount-- != 0) {
			int flightGroup;

			for (flightGroup = 0; flightGroup < g_missionHeader.numFlightGroups; ++flightGroup) {
				if (g_missionFlightGroups[flightGroup].fg.playerNumber != 0 &&
					g_missionFlightGroups[flightGroup].playerOwnerIdx == -1 &&
					g_missionFlightGroups[flightGroup].fg.team == quickStartAiBoostTeam &&
					g_missionFlightGroups[flightGroup].fg.groupAI < MAX_GROUP_AI) {
					++g_missionFlightGroups[flightGroup].fg.groupAI;
					break;
				}
			}
			++quickStartAiBoostTeam;
		}
	}

	if (g_missionHeader.missionType == MISSION_TYPE_SKIRMISH && g_pilotData.numHumanPlayersLastMission >= 2) {
		if (g_gameConfig.combatBalance == COMBAT_BALANCE_AUTOBALANCE) {
			if (playerOwnedTeamCount == 1) {
				int favoredTeam = (uint16_t)g_players[0].playerIff;

				for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
					 ++flightGroupIdx) {
					uint8_t flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
					int isOpponent = favoredTeam == flightGroupTeam
										 ? 0
										 : g_missionTeams[flightGroupTeam].allies[favoredTeam] == 0;

					if (isOpponent) {
						if (g_pilotData.numHumanPlayersLastMission == 2)
							++g_missionFlightGroups[flightGroupIdx].fg.groupAI;
						else
							g_missionFlightGroups[flightGroupIdx].fg.groupAI += 2;
						if (g_missionFlightGroups[flightGroupIdx].fg.groupAI > MAX_GROUP_AI)
							g_missionFlightGroups[flightGroupIdx].fg.groupAI = MAX_GROUP_AI;
					}
				}
			}
		} else if (g_gameConfig.combatBalance == COMBAT_BALANCE_FAVOR_IMPERIAL) {
			int favoredTeam = 1;

			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				uint8_t flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
				int isOpponent = favoredTeam == flightGroupTeam
									 ? 0
									 : g_missionTeams[flightGroupTeam].allies[favoredTeam] == 0;

				if (isOpponent) {
					if (g_pilotData.numHumanPlayersLastMission == 2)
						++g_missionFlightGroups[flightGroupIdx].fg.groupAI;
					else
						g_missionFlightGroups[flightGroupIdx].fg.groupAI += 2;
					if (g_missionFlightGroups[flightGroupIdx].fg.groupAI > MAX_GROUP_AI)
						g_missionFlightGroups[flightGroupIdx].fg.groupAI = MAX_GROUP_AI;
				}
			}
		} else if (g_gameConfig.combatBalance == COMBAT_BALANCE_FAVOR_REBEL) {
			int favoredTeam = 0;

			for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
				 ++flightGroupIdx) {
				uint8_t flightGroupTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
				int isOpponent = favoredTeam == flightGroupTeam
									 ? 0
									 : g_missionTeams[flightGroupTeam].allies[favoredTeam] == 0;

				if (isOpponent) {
					if (g_pilotData.numHumanPlayersLastMission == 2)
						++g_missionFlightGroups[flightGroupIdx].fg.groupAI;
					else
						g_missionFlightGroups[flightGroupIdx].fg.groupAI += 2;
					if (g_missionFlightGroups[flightGroupIdx].fg.groupAI > MAX_GROUP_AI)
						g_missionFlightGroups[flightGroupIdx].fg.groupAI = MAX_GROUP_AI;
				}
			}
		}
	}

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		uint8_t objectType;
		uint16_t missionPointRef;

		if (g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1) {
			if (g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft > 1u) {
				if (g_missionFlightGroups[flightGroupIdx].fg.playerCraft == 0)
					g_missionFlightGroups[flightGroupIdx].fg.playerCraft = 1;
			}
			if (g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft == 1u)
				g_missionFlightGroups[flightGroupIdx].fg.playerCraft = 0;
		}
		if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0 &&
			(g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1 ||
			 g_missionHeader.missionType == MISSION_TYPE_QUICK_START)) {
			if (g_flightMissionState.playerFlightGroupWaveMode == 0)
				g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves = 0;
			else if (g_flightMissionState.playerFlightGroupWaveMode == 2)
				g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves = UNLIMITED_PLAYER_WAVES;
		}
		objectType = g_craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];
		g_modelTypeTable[objectType].assetFlags |= MODEL_ASSET_REQUIRED;
		if (objectType == MULTIPART_MODEL_TYPE) {
			for (objectType = MULTIPART_FIRST_DEPENDENT_MODEL_TYPE;
				 objectType <= MULTIPART_LAST_DEPENDENT_MODEL_TYPE; ++objectType) {
				g_modelTypeTable[objectType].assetFlags |= MODEL_ASSET_REQUIRED;
			}
		}
		if (g_missionFlightGroups[flightGroupIdx].fg.randomSpecialCargoCraft != 0) {
			g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft =
				(uint8_t)GameRandRange(g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft);
		}
		missionPointRef = MISSION_POINT_REF_BASE;
		if (g_flightMissionState.randomVariationEnabled != 0) {
			unsigned int optionCount = 1;
			int selectedOption;

			for (slot = MISSION_POINT_REF_BASE + 1;
				 slot <= MISSION_POINT_REF_BASE + MISSION_POINT_VARIANT_COUNT; ++slot) {
				if (g_missionFlightGroups[flightGroupIdx]
						.fg.missionPointEnabled[slot - MISSION_POINT_REF_BASE] != 0) {
					++optionCount;
				}
			}
			selectedOption = GameRandRange((uint16_t)optionCount);
			if (optionCount > 1 && selectedOption != 0) {
				int ordinal = 1;

				for (slot = MISSION_POINT_REF_BASE + 1;
					 slot <= MISSION_POINT_REF_BASE + MISSION_POINT_VARIANT_COUNT; ++slot) {
					if (g_missionFlightGroups[flightGroupIdx]
							.fg.missionPointEnabled[slot - MISSION_POINT_REF_BASE] != 0) {
						if (ordinal == selectedOption) {
							missionPointRef = slot;
							break;
						}
						++ordinal;
					}
				}
			}
		}
		g_missionFgStats[flightGroupIdx].currentMissionPointRef = missionPointRef;
		if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
			if (g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0) {
				uint16_t markingModelType =
					g_craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];

				if (markingModelType == MODEL_005_TIE_INTERCEPTOR ||
					markingModelType == MODEL_006_TIE_BOMBER || markingModelType == MODEL_007_TIE_ADVANCED ||
					markingModelType == MODEL_008_TIE_DEFENDER || markingModelType == MODEL_009_EMPTY_NAME) {
					if (++g_missionFlightGroups[flightGroupIdx].fg.markings > 3u)
						g_missionFlightGroups[flightGroupIdx].fg.markings = 0;
				}
			}
			if ((g_pilotData.missionSequenceActive != 1 ||
				 g_pilotData.meleeTournamentSequenceState
						 .teamStandings[g_missionFlightGroups[flightGroupIdx].fg.team]
						 .aiOpponentSourceTeamAndTypeFlag == -1) &&
				g_missionFlightGroups[flightGroupIdx].fg.playerNumber != 0 &&
				g_missionFlightGroups[flightGroupIdx].playerOwnerIdx == -1 &&
				g_flightMissionState.aiOpponentsEnabled == 0) {
				for (slot = 0; slot < (int16_t)g_missionHeader.numFlightGroups; ++slot) {
					if (g_missionFlightGroups[slot].fg.playerNumber != 0 &&
						g_missionFlightGroups[slot].playerOwnerIdx != -1 &&
						g_missionFlightGroups[slot].fg.team ==
							g_missionFlightGroups[flightGroupIdx].fg.team) {
						break;
					}
				}
				if (slot == (int16_t)g_missionHeader.numFlightGroups)
					g_missionFlightGroups[flightGroupIdx].fg.arriveOnlyIfHuman = 1;
			}
		}
	}

	{
		uint16_t savedRandomState = (uint16_t)g_gameRandStateB;
		uint16_t backdropDirectionStarts[BACKDROP_DIRECTION_COUNT];

		g_gameRandStateB = (int16_t)(g_missionHeader.backdrop - BACKDROP_RANDOM_SEED_BIAS);
		Backdrop_GenerateDefaultRecords();
		g_asteroidFieldRandSeed = (uint16_t)g_gameRandStateB;
		g_gameRandStateB = (int16_t)savedRandomState;

		backdropDirectionStarts[0] = 0;
		backdropDirectionStarts[1] = g_backdropPositiveYCount;
		backdropDirectionStarts[2] = g_backdropNegativeYCount + g_backdropPositiveYCount;
		backdropDirectionStarts[3] = backdropDirectionStarts[2] + g_backdropPositiveXCount;
		backdropDirectionStarts[4] = g_backdropNegativeXCount + backdropDirectionStarts[3];
		backdropDirectionStarts[5] = g_backdropPositiveZCount + backdropDirectionStarts[4];

		for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++g_currentFlightGroupIdx) {
			int currentFlightGroup = g_currentFlightGroupIdx;
			uint8_t craftType = g_missionFlightGroups[currentFlightGroup].fg.craftType;

			if (craftType != CRAFT_SPECIES_UNKNOWN) {
				uint16_t backdropType = g_craftTypeToObjectType[craftType];

				if ((g_modelTypeTable[backdropType].flags & MODEL_FLAG_BACKDROP) != 0) {
					uint16_t sourceBackdropType = backdropType;
					uint8_t packedY;
					uint16_t side;
					uint16_t recordIndex;

					if (backdropType == BACKDROP_SOURCE_MODEL_TYPE) {
						backdropType = BACKDROP_FIRST_FREE_MODEL_TYPE;
						while (backdropType < BACKDROP_MODEL_TYPE_LIMIT &&
							   (g_modelTypeTable[backdropType].assetFlags & MODEL_ASSET_REQUIRED) != 0) {
							++backdropType;
						}
						if (g_flight16bppBytesPerPixel == PALETTED_BYTES_PER_PIXEL &&
							g_missionFlightGroups[currentFlightGroup].fg.status1 >=
								(unsigned int)BACKDROP_BASE_STATUS_COUNT) {
							g_missionFlightGroups[currentFlightGroup].fg.status1 =
								g_missionFlightGroups[currentFlightGroup].fg.status1 & 7;
						}
						if (g_missionFlightGroups[currentFlightGroup].fg.status1 <
							(unsigned int)BACKDROP_BASE_STATUS_COUNT) {
							g_modelTypeTable[backdropType].frameCount =
								g_modelTypeTable[sourceBackdropType].frameCount +
								g_missionFlightGroups[currentFlightGroup].fg.status1;
							g_modelTypeTable[backdropType].palette = g_backdropPaletteRemapByFlightGroupStatus
								[g_missionFlightGroups[currentFlightGroup].fg.status1];
						} else if (g_missionFlightGroups[currentFlightGroup].fg.status1 <
								   (unsigned int)BACKDROP_STATUS_COUNT) {
							g_modelTypeTable[backdropType].frameCount =
								g_missionFlightGroups[currentFlightGroup].fg.status1 +
								BACKDROP_STATUS_FRAME_OFFSET;
							g_modelTypeTable[backdropType].palette = g_backdropPaletteRemapByFlightGroupStatus
								[g_missionFlightGroups[currentFlightGroup].fg.status1];
						}
					}
					g_modelTypeTable[backdropType].assetFlags |= MODEL_ASSET_REQUIRED;
					side = g_missionFlightGroups[currentFlightGroup].fg.missionPointZ[0];
					if (side > (unsigned int)BACKDROP_DIRECTION_MAX)
						side = BACKDROP_DIRECTION_MAX;
					recordIndex = backdropDirectionStarts[side]++;
					g_backdropModelTypes[recordIndex] = (uint8_t)backdropType;
					packedY =
						(uint8_t)(-(g_missionFlightGroups[currentFlightGroup].fg.missionPointY[0] << 4));
					g_backdropPackedDirections[recordIndex] =
						(uint8_t)(packedY ^
								  ((packedY ^ g_missionFlightGroups[currentFlightGroup].fg.missionPointX[0]) &
								   0xF));
				}
			}
		}
	}

	g_missionElapsedClock.subsecondTicks = 0;
	g_flightMissionState.teamVictoryTimeLimitStarted = 0;
	g_missionElapsedClock.hours = 0;
	g_missionElapsedClock.minutes = 0;
	g_missionCountdownClock.subsecondTicks = 0;
	g_missionElapsedClock.seconds = 0;
	g_missionCountdownClock.hours = 0;
	g_missionCountdownClock.minutes = 0;
	if (g_flightMissionState.provingGroundsModeActive == 0) {
		uint8_t timeLimitMinutes = g_flightMissionState.missionTimeLimitMinutes;

		if (timeLimitMinutes != 0) {
			if (timeLimitMinutes != UINT8_MAX) {
				g_missionCountdownClock.minutes = timeLimitMinutes;
			} else if (g_missionHeader.timeLimitMinutes != 0) {
				g_missionCountdownClock.minutes = g_missionHeader.timeLimitMinutes;
				g_flightMissionState.missionTimeLimitMinutes = g_missionHeader.timeLimitMinutes;
			} else {
				g_flightMissionState.missionTimeLimitMinutes = 0;
			}
		} else {
			g_flightMissionState.missionTimeLimitMinutes = 0;
		}
		g_missionCountdownClock.seconds = 0;
	} else {
		g_missionFlightGroups[0].fg.craftType = (CraftSpecies)g_flightMissionState.provingGroundsCraftType;
		g_modelTypeTable[g_craftTypeToObjectType[g_flightMissionState.provingGroundsCraftType]].assetFlags |=
			MODEL_ASSET_REQUIRED;
		g_missionCountdownClock.seconds = 59;
		g_missionCountdownClock.minutes = (uint8_t)(10 - g_flightMissionState.provingGroundsLevel);
	}
	FlightSurface_Lock();
	return 1;
}

// FUNCTION: XVT 0x454A20
void Mission_InitFlightRuntimeState(void) {
	enum {
		PLAYER_COUNT = 8,
		TEAM_COUNT = 10,
		MISSION_FLIGHT_GROUP_COUNT = 48,
		FLIGHT_GROUP_GOAL_STATE_COUNT = 80,
		TEAM_GOAL_COUNT = 3,
		GLOBAL_GOAL_TRIGGER_COUNT = 4,
		ARRIVAL_STATE_COMPLETE = 1,
		ARRIVAL_STATE_FAILED = 2,
		ARRIVAL_STATE_PENDING = 4,
		PLAYER_CONDITION_TEAM = 10,
		COUNTABLE_MODEL_FLAG = 0x20,
		DEFAULT_CAMERA_DISTANCE = 1024,
		DEFAULT_SIM_STEP = 15,
		GOAL_STATE_BATCH_SIZE = 8,
	};

	int flightGroupIndex;
	int teamIndex;
	int playerIndex;

	FlightSurface_Unlock();
	g_initialSpawnBindPlayerCraftSlots = 1;
	g_nextObjectSignature = 1;
	for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < g_missionHeader.numFlightGroups;
		 ++g_currentFlightGroupIdx) {
		int16_t firstPairState;
		int16_t secondPairState;
		uint8_t arrivalState;
		uint8_t condition1;
		uint8_t condition2;

		g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled =
			g_missionDifficultyArrivalMasks[g_flightMissionState.difficulty] &
			g_fgArrivalDifficultyMasks[g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalDifficulty];
		if (g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled == 0)
			continue;

		firstPairState = ARRIVAL_STATE_PENDING;
		secondPairState = ARRIVAL_STATE_PENDING;
		condition1 =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[0].condition;
		condition2 =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[1].condition;
		if ((condition1 == MISSION_COND_PLAYER_CONNECTED || condition2 == MISSION_COND_PLAYER_CONNECTED ||
			 condition1 == MISSION_COND_PLAYER_DISCONNECTED ||
			 condition2 == MISSION_COND_PLAYER_DISCONNECTED) &&
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].trigger1OrTrigger2 == 0) {
			uint16_t result1;
			uint16_t result2;

			result1 = (uint16_t)Mission_EvaluateCondition(
				condition1,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[0].variableType,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[0].variable,
				(uint8_t)g_missionFlightGroups[g_currentFlightGroupIdx]
					.fg.arrivalTriggers[0]
					.triggers[0]
					.amount,
				0, PLAYER_CONDITION_TEAM);
			result2 = (uint16_t)Mission_EvaluateCondition(
				condition2,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[1].variableType,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[1].variable,
				(uint8_t)g_missionFlightGroups[g_currentFlightGroupIdx]
					.fg.arrivalTriggers[0]
					.triggers[1]
					.amount,
				0, PLAYER_CONDITION_TEAM);
			if ((result1 & result2 & ARRIVAL_STATE_COMPLETE) != 0)
				firstPairState = ARRIVAL_STATE_COMPLETE;
			else
				firstPairState = ((result1 | result2) & ARRIVAL_STATE_FAILED) == 0 ? ARRIVAL_STATE_PENDING
																				   : ARRIVAL_STATE_FAILED;
		}

		condition1 =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[0].condition;
		condition2 =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[1].condition;
		if ((condition1 == MISSION_COND_PLAYER_CONNECTED || condition2 == MISSION_COND_PLAYER_CONNECTED ||
			 condition1 == MISSION_COND_PLAYER_DISCONNECTED ||
			 condition2 == MISSION_COND_PLAYER_DISCONNECTED) &&
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].trigger1OrTrigger2 == 0) {
			uint16_t result1;
			uint16_t result2;

			result1 = (uint16_t)Mission_EvaluateCondition(
				condition1,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[0].variableType,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[0].variable,
				(uint8_t)g_missionFlightGroups[g_currentFlightGroupIdx]
					.fg.arrivalTriggers[1]
					.triggers[0]
					.amount,
				0, PLAYER_CONDITION_TEAM);
			result2 = (uint16_t)Mission_EvaluateCondition(
				condition2,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[1].variableType,
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1].triggers[1].variable,
				(uint8_t)g_missionFlightGroups[g_currentFlightGroupIdx]
					.fg.arrivalTriggers[1]
					.triggers[1]
					.amount,
				0, PLAYER_CONDITION_TEAM);
			if ((result1 & result2 & ARRIVAL_STATE_COMPLETE) != 0)
				secondPairState = ARRIVAL_STATE_COMPLETE;
			else
				secondPairState = ((result1 | result2) & ARRIVAL_STATE_FAILED) == 0 ? ARRIVAL_STATE_PENDING
																					: ARRIVAL_STATE_FAILED;
		}

		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivals12OrArrivals34 == 1) {
			if ((((uint8_t)firstPairState | (uint8_t)secondPairState) & ARRIVAL_STATE_COMPLETE) != 0)
				arrivalState = ARRIVAL_STATE_COMPLETE;
			else
				arrivalState = ((firstPairState & secondPairState) & ARRIVAL_STATE_FAILED) == 0
								   ? ARRIVAL_STATE_PENDING
								   : ARRIVAL_STATE_FAILED;
		} else {
			if ((((uint8_t)firstPairState & (uint8_t)secondPairState) & ARRIVAL_STATE_COMPLETE) != 0)
				arrivalState = ARRIVAL_STATE_COMPLETE;
			else
				arrivalState = ((firstPairState | secondPairState) & ARRIVAL_STATE_FAILED) == 0
								   ? ARRIVAL_STATE_PENDING
								   : ARRIVAL_STATE_FAILED;
		}
		if ((arrivalState & ARRIVAL_STATE_FAILED) != 0)
			g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled = 0;
	}

	for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < g_missionHeader.numFlightGroups;
		 ++g_currentFlightGroupIdx) {
		int16_t numberOfCraft;
		int outcomeIndex;
		int goalStateIndex;

		g_missionFgStats[g_currentFlightGroupIdx].hasArrived = 0;
		g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending = 0;
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;
		g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount = 0;
		g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer = 0;
		numberOfCraft = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
		if (g_modelTypeTable
				[g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
					.genusId == CRAFT_GENUS_MINE)
			numberOfCraft *= numberOfCraft;
		if (g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled != 0 ||
			g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) {
			g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] =
				(uint16_t)(numberOfCraft *
						   (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves + 1));
			if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.randomSpecialCargoCraft != 0 ||
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft <
					g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft)
				g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] =
					g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves + 1;
			else
				g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] = 0;
		} else {
			g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] = 0;
		}
		for (outcomeIndex = FLIGHT_GROUP_OUTCOME_ARRIVED; outcomeIndex < FLIGHT_GROUP_OUTCOME_COUNT;
			 ++outcomeIndex) {
			g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[outcomeIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[outcomeIndex] = 0;
		}
		for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
			g_missionFgStats[g_currentFlightGroupIdx].teamInspected[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamSpecialCargoInspected[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamUninspectedLost[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamSpecialCargoUninspectedLost[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamCondition44Count[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamCondition44SpecialCargo[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamCondition44OtherTeamCount[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamCondition44OtherTeamSpecialCargo[teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamEventExtra[0][teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamEventExtra[1][teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamEventExtra[2][teamIndex] = 0;
			g_missionFgStats[g_currentFlightGroupIdx].teamEventExtra[3][teamIndex] = 0;
		}
		for (goalStateIndex = 0; goalStateIndex < FLIGHT_GROUP_GOAL_STATE_COUNT;
			 goalStateIndex += GOAL_STATE_BATCH_SIZE) {
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 1] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 2] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 3] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 4] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 5] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 6] = ARRIVAL_STATE_PENDING;
			g_missionFgStats[g_currentFlightGroupIdx].goalState[goalStateIndex + 7] = ARRIVAL_STATE_PENDING;
		}

		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType != CRAFT_SPECIES_UNKNOWN &&
			(g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled != 0 ||
			 g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) &&
			g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] != 0) {
			if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1 ||
				(g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0].triggers[0].condition ==
					 MISSION_COND_ALWAYS_TRUE &&
				 g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalDelayMinutes == 0 &&
				 g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalDelaySeconds == 0 &&
				 (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber == 0 ||
				  g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1 ||
				  g_missionFlightGroups[g_currentFlightGroupIdx].fg.arriveOnlyIfHuman == 0)))
				Mission_StartFlightGroupArrival(UINT16_MAX);
			if ((g_modelTypeTable
					 [g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
						 .flags &
				 COUNTABLE_MODEL_FLAG) == 0) {
				teamIndex = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
				g_flightMissionState.runtime.teamHasCountableCraft[teamIndex] = 1;
			}
		}
	}

	for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
		int goalIndex;
		int teamFlightGroupIndex;

		g_flightMissionState.runtime.teamActiveGoalSequence[teamIndex] = 0;
		g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIndex] = 0;
		g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIndex] = 0;
		g_flightMissionState.runtime.teamMissionCompletionTimeSeconds[teamIndex] = 0;
		for (goalIndex = 0; goalIndex < TEAM_GOAL_COUNT; ++goalIndex) {
			int triggerIndex;

			g_flightMissionState.runtime.teamGlobalGoalState[teamIndex][goalIndex] = ARRIVAL_STATE_PENDING;
			g_flightMissionState.runtime.teamGoalStatus[teamIndex][goalIndex] = 0;
			for (triggerIndex = 0; triggerIndex < GLOBAL_GOAL_TRIGGER_COUNT; ++triggerIndex) {
				g_flightMissionState.runtime.globalGoalTriggerCounts[0][teamIndex][goalIndex][triggerIndex] =
					0;
				g_flightMissionState.runtime.globalGoalTriggerCounts[1][teamIndex][goalIndex][triggerIndex] =
					0;
			}
		}
		g_flightMissionState.runtime.teamKillStats[0][teamIndex] = 0;
		g_flightMissionState.runtime.teamKillStats[1][teamIndex] = 0;
		g_flightMissionState.runtime.teamKillStats[2][teamIndex] = 0;
		g_flightMissionState.runtime.teamKillStats[3][teamIndex] = 0;
		for (teamFlightGroupIndex = 0; teamFlightGroupIndex < MISSION_FLIGHT_GROUP_COUNT;
			 ++teamFlightGroupIndex) {
			g_flightMissionState.runtime.teamFgCounters[0][teamIndex][teamFlightGroupIndex] = 0;
			g_flightMissionState.runtime.teamFgCounters[1][teamIndex][teamFlightGroupIndex] = 0;
			g_flightMissionState.runtime.teamFgDesignationCode[teamIndex][teamFlightGroupIndex] = 0;
		}
	}

	g_currentFlightGroupIdx = 0;
	for (; g_currentFlightGroupIdx < g_missionHeader.numFlightGroups; ++g_currentFlightGroupIdx) {
		int roleOffset;
		uint8_t flightGroupTeam;

		flightGroupTeam = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
		for (roleOffset = 0;
			 roleOffset < (int)sizeof(g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftRole);
			 roleOffset += 4) {
			uint8_t teamSelector;
			int designationCode;
			uint8_t selector;
			char role1;
			char role2;
			char role3;

			selector = g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftRole[roleOffset];
			if (selector == '\0')
				break;
			teamSelector = UINT8_MAX;
			if (selector == 'A')
				teamSelector = 10;
			else if (selector == 'O')
				teamSelector = 11;
			else if (selector == 'F')
				teamSelector = 12;
			else if (selector == 'H')
				teamSelector = 13;
			else if (selector >= '1' && selector <= '9')
				teamSelector = (uint8_t)(selector - '1');
			if (teamSelector == UINT8_MAX)
				continue;

			role1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftRole[roleOffset + 1];
			role2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftRole[roleOffset + 2];
			role3 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftRole[roleOffset + 3];
			designationCode = 0;
			if (role1 == 'C' && role2 == 'O' && role3 == 'M')
				designationCode = 1;
			else if (role1 == 'B' && role2 == 'A' && role3 == 'S')
				designationCode = 2;
			else if (role1 == 'S' && role2 == 'T' && role3 == 'A')
				designationCode = 3;
			else if (role1 == 'M' && role2 == 'I' && role3 == 'S')
				designationCode = 4;
			else if (role1 == 'C' && role2 == 'O' && role3 == 'N')
				designationCode = 5;
			else if (role1 == 'S' && role2 == 'T' && role3 == 'R')
				designationCode = 6;
			else if (role1 == 'R' && role2 == 'E' && role3 == 'L')
				designationCode = 7;
			else if (role1 == 'P' && role2 == 'R' && role3 == 'I')
				designationCode = 8;
			else if (role1 == 'S' && role2 == 'E' && role3 == 'C')
				designationCode = 9;
			else if (role1 == 'T' && role2 == 'E' && role3 == 'R')
				designationCode = 10;
			else if (role1 == 'R' && role2 == 'E' && role3 == 'S')
				designationCode = 11;
			else if (role1 == 'M' && role2 == 'A' && role3 == 'N')
				designationCode = 12;
			if (designationCode == 0)
				continue;

			if (teamSelector < TEAM_COUNT) {
				g_flightMissionState.runtime.teamFgDesignationCode[teamSelector][g_currentFlightGroupIdx] =
					designationCode;
			} else {
				for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
					if (teamSelector == 10 || (teamSelector == 11 && flightGroupTeam == teamIndex) ||
						(teamSelector == 12 && g_missionTeams[flightGroupTeam].allies[teamIndex] != 0) ||
						(teamSelector == 13 && g_missionTeams[flightGroupTeam].allies[teamIndex] == 0))
						g_flightMissionState.runtime
							.teamFgDesignationCode[teamIndex][g_currentFlightGroupIdx] = designationCode;
				}
			}
		}
	}

	for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
		int presetIndex;

		g_players[playerIndex].regionSessionId = 0;
		g_players[playerIndex].targetBoxEnabled = 1;
		g_players[playerIndex].currentTargetObjectIdx = -1;
		g_players[playerIndex].targetCycleStart = -1;
		g_players[playerIndex].selectedTargetComponent = -1;
		g_players[playerIndex].targetingState = -1;
		g_players[playerIndex].engineWashSourceObjIdx = -1;
		for (presetIndex = 0; presetIndex < (int)(sizeof(g_players[playerIndex].targetPresetSlot) /
												  sizeof(g_players[playerIndex].targetPresetSlot[0]));
			 ++presetIndex)
			g_players[playerIndex].targetPresetSlot[presetIndex] = -1;
		g_players[playerIndex].engineWashStrength = 0;
		g_players[playerIndex].yawRollSwap = 0;
		g_players[playerIndex].smoothedInputYaw = 0;
		g_players[playerIndex].smoothedInputPitch = 0;
		g_players[playerIndex].savedKeyMods = 0;
		g_players[playerIndex].keyModsHoldTimer = 0;
		g_players[playerIndex].hyperspacePhase = 0;
		g_players[playerIndex].hyperspaceRuntime.phaseElapsedTicks = 0;
		memset(g_players[playerIndex].msgText, 0, sizeof(g_players[playerIndex].msgText));
		g_players[playerIndex].msgLength = 0;
		g_players[playerIndex].msgTypeId = FLIGHT_CHAT_RECIPIENT_INACTIVE;
	}

	g_localDebrisRecycleSlotCursor = (uint16_t)g_localTransientSlotStart;
	if (g_hudCockpitResourcesLoaded == 0)
		Hud_LoadCockpitResources();
	g_flightRuntimeReservedState = 0;
	memset(&g_flightGlobalCountdownTimers, 0, sizeof(g_flightGlobalCountdownTimers));
	g_renderObjectRefFlags = 0;
	g_flightRuntimeStateInitialized = 1;
	g_renderObjectRef = UINT16_MAX;
	for (teamIndex = 0; teamIndex < (int)(sizeof(PlayerFlightTransientTimers) / sizeof(uint16_t));
		 ++teamIndex) {
		for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex)
			((uint16_t*)&g_playerFlightTransientTimers[playerIndex])[teamIndex] = 0;
	}
	for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
		g_players[playerIndex].pendingActionTimer = 0;
		g_players[playerIndex].beamFireCooldownTimer = 0;
	}
	for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
		g_players[playerIndex].viewState.hudAimXSnapState = 0;
		g_players[playerIndex].viewState.hudAimX = 0;
		g_players[playerIndex].viewState.hudAimY = 0;
		g_players[playerIndex].viewState.externalCameraActive = 0;
		g_players[playerIndex].viewState.cameraDistance = DEFAULT_CAMERA_DISTANCE;
		g_players[playerIndex].viewState.playerInputBlocked = 0;
		g_players[playerIndex].viewState.transitionTimer = 0;
		g_players[playerIndex].viewState.cameraFocusObjIdx = (uint16_t)g_players[playerIndex].objectIndex;
	}
	g_hudLoadedPanelSetId = UINT8_MAX;
	for (playerIndex = 0; playerIndex < PLAYER_COUNT; ++playerIndex) {
		g_players[playerIndex].savedHudViewState = HUD_VIEW_FORWARD;
		Hud_ForcePlayerViewState(HUD_VIEW_FORWARD, playerIndex);
	}
	g_actionKey = 0;
	g_flightInitialTextureCacheFlushPending = 1;
	g_flightDisplayRebuildPending = 1;
	g_simStepScale = DEFAULT_SIM_STEP;
	g_elapsedTicks = DEFAULT_SIM_STEP;
	g_flightFrameStepMirror = DEFAULT_SIM_STEP;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	g_readyMessageQueueCount = 0;
	g_inputTimestamp = 0;
	for (flightGroupIndex = 0; flightGroupIndex < MISSION_MESSAGE_COUNT; ++flightGroupIndex) {
		g_flightMissionState.messageTriggered[flightGroupIndex] = 0;
		g_flightMissionState.messageDelayCountdown[flightGroupIndex] = 0;
	}
	g_flightMissionState.runtime.globalGoalStatusUnused = 0;
	g_flightMissionState.missionEndPending = 0;
	g_flightMissionState.maxConnectedPlayerCountThisMission = 0;
	g_flightMissionState.runtime.globalPrimaryGoalStatus = 0;
	g_flightMissionState.runtime.globalBonusGoalStatus = 0;
	g_initialSpawnBindPlayerCraftSlots = 0;
	FlightSurface_Lock();
}

// FUNCTION: XVT 0x455600
int16_t Mission_StartFlightGroupArrival(uint16_t craftOrdinal) {
	enum { STATIC_MODEL_FLAG = 0x80 };

	uint16_t flightGroupIndex = g_currentFlightGroupIdx;
	uint8_t objectType;

	g_missionFgStats[flightGroupIndex].hasArrived = 1;
	objectType = g_craftTypeToObjectType[g_missionFlightGroups[flightGroupIndex].fg.craftType];
	if ((g_modelTypeTable[objectType].flags & STATIC_MODEL_FLAG) == 0) {
		g_missionFgStats[flightGroupIndex].wavesRemaining =
			g_missionFlightGroups[flightGroupIndex].fg.numberOfWaves;
		Mission_SpawnFlightGroupWaveCraft(craftOrdinal);
		return 1;
	}

	if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
		g_missionFgStats[flightGroupIndex].wavesRemaining =
			g_missionFlightGroups[flightGroupIndex].fg.numberOfWaves;
	} else {
		g_missionFgStats[flightGroupIndex].wavesRemaining = 0;
	}
	Mission_SpawnFlightGroupStaticObjects(craftOrdinal);
	return 1;
}

// FUNCTION: XVT 0x4556B0
void Mission_UpdateFlightGroupArrivals(void) {
	enum {
		STATIC_MODEL_FLAG = 0x80,
		PLAYER_WAVE_MODE_PRESERVE = 2,
		GOAL_STATUS_COMPLETE = 1,
		GOAL_STATUS_FAILED = 2,
		STOP_ARRIVING_OUTCOME = 1,
		STOP_ARRIVING_PRIMARY_COMPLETE = 2,
		STOP_ARRIVING_MISSION_RESOLVED = 3,
	};

	if (g_flightGlobalCountdownTimers.missionArrivalTriggerScanTimer == 0) {
		g_flightGlobalCountdownTimers.missionArrivalTriggerScanTimer = SIMULATION_TICKS_PER_SECOND;

		for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < g_missionHeader.numFlightGroups;
			 ++g_currentFlightGroupIdx) {
			if (g_missionFgStats[g_currentFlightGroupIdx].hasArrived == 0 &&
				g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending == 0) {
				if ((g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled != 0 ||
					 g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) &&
					g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] != 0) {
					char firstTriggerResult = (char)Mission_EvaluateTriggerPair(
						&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[0], 1);
					char secondTriggerResult = (char)Mission_EvaluateTriggerPair(
						&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalTriggers[1], 1);
					char arrivalTriggered;
					unsigned int flightGroupIdx = g_currentFlightGroupIdx;

					if (g_missionFlightGroups[flightGroupIdx].fg.arrivals12OrArrivals34 == 1) {
						arrivalTriggered = firstTriggerResult | secondTriggerResult;
					} else {
						arrivalTriggered = firstTriggerResult & secondTriggerResult;
					}

					if ((arrivalTriggered & 1) != 0 &&
						(g_missionFlightGroups[flightGroupIdx].playerOwnerIdx != -1 ||
						 g_missionFlightGroups[flightGroupIdx].fg.arriveOnlyIfHuman == 0)) {
						int16_t* arrivalDelayTimer = &g_missionFgStats[flightGroupIdx].arrivalDelayTimer;
						int16_t fixedDelaySeconds =
							60 * g_missionFlightGroups[flightGroupIdx].fg.arrivalDelayMinutes +
							g_missionFlightGroups[flightGroupIdx].fg.arrivalDelaySeconds;
						uint16_t randomDelaySeconds;
						uint16_t randomDelay;

						randomDelaySeconds =
							60 * g_missionFlightGroups[flightGroupIdx].fg.arrivalRandDelayMinutes;
						*arrivalDelayTimer = fixedDelaySeconds;
						randomDelaySeconds +=
							g_missionFlightGroups[flightGroupIdx].fg.arrivalRandDelaySeconds;
						if (g_flightMissionState.randomVariationEnabled != 0) {
							randomDelay = GameRandRange(randomDelaySeconds);
						} else {
							randomDelay = randomDelaySeconds >> 1;
						}

						*arrivalDelayTimer = fixedDelaySeconds + randomDelay;
						g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending = 1;
					}
				}
				continue;
			}

			if (g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled == 0 ||
				g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining == 0 ||
				g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] == 0 ||
				g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) {
				continue;
			}

			if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber != 0) {
				uint8_t team = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
				uint8_t primaryGoalStatus = g_flightMissionState.runtime.teamGoalStatus[team][0];

				if (primaryGoalStatus == GOAL_STATUS_COMPLETE &&
					g_flightMissionState.runtime.teamGoalStatus[team][1] == GOAL_STATUS_COMPLETE) {
					continue;
				}
				if (primaryGoalStatus == GOAL_STATUS_COMPLETE &&
					g_flightMissionState.runtime.teamGoalStatus[team][1] == GOAL_STATUS_FAILED) {
					continue;
				}
				if (primaryGoalStatus == GOAL_STATUS_FAILED &&
					g_flightMissionState.runtime.teamGoalStatus[team][1] == GOAL_STATUS_COMPLETE) {
					continue;
				}
			}

			{
				unsigned int objectIndex;
				uint8_t objectType =
					g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
				char waveObjectsAbsent = 1;

				if ((g_modelTypeTable[objectType].flags & STATIC_MODEL_FLAG) == 0) {
					for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
						 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
						ObjectRecord* object = &g_objectTable[objectIndex];

						if (object->objectType != 0 && object->mobj->state == 0 &&
							object->flightGroupIdx == g_currentFlightGroupIdx) {
							g_curCraft = object->mobj->pCraft;
							if (g_curCraft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
								g_curCraft->objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
								waveObjectsAbsent = 0;
								break;
							}
						}
					}
				} else {
					unsigned int staticObjectEnd;

					objectIndex = (unsigned int)g_regionMainObjectSlotEnd;
					staticObjectEnd = objectIndex + (unsigned int)g_regionStaticObjectSlotCount;
					for (; objectIndex < staticObjectEnd; ++objectIndex) {
						if (g_objectTable[objectIndex].objectType != 0 &&
							g_objectTable[objectIndex].flightGroupIdx == g_currentFlightGroupIdx) {
							waveObjectsAbsent = 0;
							break;
						}
					}
				}

				if (waveObjectsAbsent != 0) {
					char stopArriving = 0;

					if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx == -1 ||
						g_flightMissionState.playerFlightGroupWaveMode != PLAYER_WAVE_MODE_PRESERVE) {
						uint8_t stopCondition;

						if ((g_missionFlightGroups[g_currentFlightGroupIdx]
									 .fg.departureTrigger.triggers[0]
									 .condition != 0 ||
							 g_missionFlightGroups[g_currentFlightGroupIdx]
									 .fg.departureTrigger.triggers[1]
									 .condition != 0) &&
							(Mission_EvaluateTriggerPair(
								 &g_missionFlightGroups[g_currentFlightGroupIdx].fg.departureTrigger, 0) &
							 1) != 0) {
							stopArriving = 1;
						}

						stopCondition = g_missionFlightGroups[g_currentFlightGroupIdx].fg.stopArrivingWhen;
						if (stopCondition == STOP_ARRIVING_OUTCOME &&
							g_missionFgStats[g_currentFlightGroupIdx]
									.outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED] != 0) {
							stopArriving = 1;
						}
						if (stopCondition == STOP_ARRIVING_PRIMARY_COMPLETE &&
							g_flightMissionState.runtime
									.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
												   [0] == GOAL_STATUS_COMPLETE) {
							stopArriving = 1;
						}
						if (stopCondition == STOP_ARRIVING_MISSION_RESOLVED &&
							(g_flightMissionState.runtime
									 .teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
													[0] == GOAL_STATUS_FAILED ||
							 g_flightMissionState.runtime
									 .teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
													[1] == GOAL_STATUS_COMPLETE)) {
							stopArriving = 1;
						}
					}

					if (stopArriving != 0) {
						uint16_t unavailableCraftCount = g_missionFgStats[g_currentFlightGroupIdx]
															 .outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] -
														 g_missionFgStats[g_currentFlightGroupIdx]
															 .outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
						uint8_t unavailableSpecialCargoCount =
							g_missionFgStats[g_currentFlightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] -
							g_missionFgStats[g_currentFlightGroupIdx]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];

						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] += unavailableCraftCount;
						g_missionFgStats[g_currentFlightGroupIdx]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] +=
							unavailableSpecialCargoCount;
						g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;

						if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.departureMethod != 0 ||
							g_missionFlightGroups[g_currentFlightGroupIdx].fg.alternateMothershipUsed != 0) {
							g_missionFgStats[g_currentFlightGroupIdx]
								.outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] +=
								unavailableCraftCount;
						} else {
							g_missionFgStats[g_currentFlightGroupIdx]
								.outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION] += unavailableCraftCount;
						}
					} else if (Mission_HasCapacityForCurrentFlightGroupWave() != 0) {
						Mission_SpawnCurrentFlightGroupWave();
					}
				}
			}
		}
	}

	if (g_flightGlobalCountdownTimers.missionArrivalDelayScanTimer == 0) {
		g_flightGlobalCountdownTimers.missionArrivalDelayScanTimer = SIMULATION_TICKS_PER_SECOND;

		for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < g_missionHeader.numFlightGroups;
			 ++g_currentFlightGroupIdx) {
			if (g_missionFgStats[g_currentFlightGroupIdx].hasArrived == 0 &&
				g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending == 1) {
				if (g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer == 0) {
					if (Mission_HasCapacityForCurrentFlightGroupWave() != 0) {
						Mission_StartFlightGroupArrival(UINT16_MAX);
					}
				} else {
					--g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer;
				}
			}
		}
	}
}

// FUNCTION: XVT 0x455C80
void Mission_ProcessFlightGroupWaveCompletion(uint16_t flightGroupIdx) {
	enum {
		PLAYER_WAVE_MODE_PRESERVE = 2,
		GOAL_STATUS_COMPLETE = 1,
		GOAL_STATUS_FAILED = 2,
		STOP_ARRIVING_OUTCOME = 1,
		STOP_ARRIVING_PRIMARY_COMPLETE = 2,
		STOP_ARRIVING_MISSION_RESOLVED = 3,
		GENUS_BACKDROP = 13,
	};

	int stopArriving;
	unsigned int objectIndex;
	char waveComplete;

	if (g_missionFgStats[flightGroupIdx].wavesRemaining == 0 ||
		g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] == 0)
		return;

	waveComplete = 1;
	for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
		 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
		ObjectRecord* object = &g_objectTable[objectIndex];

		if (object->objectType != 0 && object->mobj->state == 0 && object->flightGroupIdx == flightGroupIdx &&
			object->mobj->pCraft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
			object->mobj->pCraft->objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
			waveComplete = 0;
		}
	}
	if (waveComplete == 0)
		return;

	if (g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][0] ==
			GOAL_STATUS_COMPLETE &&
		g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][1] ==
			GOAL_STATUS_COMPLETE) {
		return;
	}
	if (g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][0] ==
			GOAL_STATUS_COMPLETE &&
		g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][1] ==
			GOAL_STATUS_FAILED) {
		return;
	}
	if (g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][0] ==
			GOAL_STATUS_FAILED &&
		g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][1] ==
			GOAL_STATUS_COMPLETE) {
		return;
	}

	{
		stopArriving = 0;

		if (g_flightMissionState.playerFlightGroupWaveMode != PLAYER_WAVE_MODE_PRESERVE) {
			uint8_t stopCondition;

			if ((g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[0].condition != 0 ||
				 g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[1].condition != 0) &&
				(Mission_EvaluateTriggerPair(&g_missionFlightGroups[flightGroupIdx].fg.departureTrigger, 0) &
				 1) != 0) {
				stopArriving = 1;
			}
			stopCondition = g_missionFlightGroups[flightGroupIdx].fg.stopArrivingWhen;
			if (stopCondition == STOP_ARRIVING_OUTCOME &&
				g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED] != 0) {
				stopArriving = 1;
			}
			if (stopCondition == STOP_ARRIVING_PRIMARY_COMPLETE &&
				g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team]
														   [0] == GOAL_STATUS_COMPLETE) {
				stopArriving = 1;
			}
			if (stopCondition == STOP_ARRIVING_MISSION_RESOLVED &&
				(g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team]
															[0] == GOAL_STATUS_FAILED ||
				 g_flightMissionState.runtime.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team]
															[1] == GOAL_STATUS_COMPLETE)) {
				stopArriving = 1;
			}
		}

		if (stopArriving != 0) {
			uint16_t unavailableCraftCount =
				g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] -
				g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
			uint8_t unavailableSpecialCargoCount =
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_TOTAL] -
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];

			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_INSPECTED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DISABLED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_ATTACKED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] +=
				unavailableCraftCount;
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_BOARDED] +=
				unavailableSpecialCargoCount;
			g_missionFgStats[flightGroupIdx].wavesRemaining = 0;
			if (g_missionFlightGroups[flightGroupIdx].fg.departureMethod != 0 ||
				g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed != 0) {
				g_missionFgStats[flightGroupIdx]
					.outcomeCount[FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT] += unavailableCraftCount;
			} else {
				g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_LEFT_REGION] +=
					unavailableCraftCount;
			}
			return;
		}
	}

	g_currentFlightGroupIdx = flightGroupIdx;
	{
		unsigned int freeObjectSlotCount = 0;
		unsigned int numberOfCraft;

		for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
			 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
			if (g_objectTable[objectIndex].objectType == 0)
				++freeObjectSlotCount;
		}

		numberOfCraft = g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft;
		if (freeObjectSlotCount < numberOfCraft) {
			unsigned int slotsToFree = numberOfCraft - freeObjectSlotCount;

			for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
				 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
				ObjectRecord* object = &g_objectTable[objectIndex];

				if (object->objectType != 0 && object->genusId == GENUS_BACKDROP &&
					object->playerOwnerIdx == -1) {
					object->objectType = 0;
					Mission_RecordCraftOutcome(objectIndex, g_objectTable[objectIndex].flightGroupIdx,
											   FLIGHT_GROUP_OUTCOME_DESTROYED);
					if (--slotsToFree == 0)
						break;
				}
			}

			if (slotsToFree != 0) {
				for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
					 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
					ObjectRecord* object = &g_objectTable[objectIndex];

					if (object->objectType != 0 && object->mobj->state == 0 && object->playerOwnerIdx == -1 &&
						(object->mobj->pCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
						 object->mobj->pCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING)) {
						CraftData* craft = object->mobj->pCraft;

						object->objectType = 0;
						Craft_ClearEffectiveAiObjectLink(craft);
						Mission_RecordCraftOutcome(objectIndex, g_objectTable[objectIndex].flightGroupIdx,
												   FLIGHT_GROUP_OUTCOME_DESTROYED);
						if (--slotsToFree == 0)
							break;
					}
				}
			}
		}
	}

	Mission_SpawnCurrentFlightGroupWave();
}

// FUNCTION: XVT 0x456040
void Mission_SpawnCurrentFlightGroupWave(void) {
	enum {
		STATIC_MODEL_FLAG = 0x80,
		PRESERVE_PLAYER_WAVES_MODE = 2,
	};

	uint8_t objectType;
	uint8_t* wavesRemaining;
	uint8_t remainingWaveCount;

	objectType = g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	if ((g_modelTypeTable[objectType].flags & STATIC_MODEL_FLAG) == 0)
		Mission_SpawnFlightGroupWaveCraft(UINT16_MAX);
	else
		Mission_SpawnFlightGroupStaticObjects(UINT16_MAX);

	wavesRemaining = &g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining;
	remainingWaveCount = *wavesRemaining;
	if (remainingWaveCount != 0) {
		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber != 0) {
			if (g_flightMissionState.playerFlightGroupWaveMode != PRESERVE_PLAYER_WAVES_MODE)
				*wavesRemaining = remainingWaveCount - 1;
		} else {
			*wavesRemaining = remainingWaveCount - 1;
		}
	}
}

// FUNCTION: XVT 0x4560F0
int Mission_HasCapacityForCurrentFlightGroupWave(void) {
	unsigned int freeSlotCount;
	unsigned int objectIndex;
	uint16_t objectType;

	objectType = g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	if ((g_modelTypeTable[objectType].flags & 0x80) == 0) {
		freeSlotCount = 0;
		for (objectIndex = (unsigned int)g_activeRegionObjectSlotStart;
			 objectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
			if (g_objectTable[objectIndex].objectType == 0)
				++freeSlotCount;
		}

		return freeSlotCount >= g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
	}

	return 1;
}

// FUNCTION: XVT 0x456190
int16_t Mission_SpawnFlightGroupWaveCraft(uint16_t craftOrdinal) {
	enum {
		TEAM_COUNT = 10,
		PLAYER_NUMBER_COUNT = 8,
		MISSION_POINT_1 = 0x8000,
		MISSION_POINT_2 = 0x8001,
		MISSION_POINT_5 = 0x8004,
		DEFAULT_SPAWN_PITCH = 0x4000,
		HYPERSPACE_REVERSE_ANGLE = 0x8000,
		HYPERSPACE_DISTANCE = UINT16_MAX,
		HYPERSPACE_OFFSET_SCALE = 8,
		RANDOM_PLAYER_OFFSET_MASK = 0x7FFF,
		QUICK_START_FORMATION_CENTER = 3,
		LARGE_MOTHERSHIP_FORMATION = 6,
		LARGE_MOTHERSHIP_FORMATION_THRESHOLD = 3,
		MISSION_VERSION_14 = 14,
	};

	uint8_t missionStarted;
	uint8_t playerFlightGroupsByTeam[TEAM_COUNT];
	int spawnedCraftCount;
	int16_t spawnPitch;

	missionStarted = g_missionElapsedClock.minutes;
	missionStarted |= g_missionElapsedClock.seconds;
	missionStarted |= g_missionElapsedClock.hours;
	g_spawnOutOfHyperspaceFlag = 0;
	g_spawnFromMothershipFlag = 0;

	if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMethod != 0 && missionStarted != 0 &&
		g_flightMissionState.provingGroundsModeActive == 0 && craftOrdinal == UINT16_MAX) {
		uint16_t objectIndex;
		uint16_t mothershipObjectIndex = 0;
		int16_t foundMothership = 0;
		int16_t arrivalMothership = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMothership;

		for (objectIndex = (uint16_t)g_activeRegionObjectSlotStart;
			 (int)objectIndex < g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
			if (g_objectTable[objectIndex].objectType == 0)
				continue;
			g_curCraft = g_objectTable[objectIndex].mobj->pCraft;
			if (g_objectTable[objectIndex].flightGroupIdx == arrivalMothership &&
				g_curCraft->leader_obj_idx == UINT8_MAX) {
				mothershipObjectIndex = objectIndex;
				foundMothership = 1;
				break;
			}
		}
		if (foundMothership == 0)
			return 0;

		{
			uint16_t modelIndex;

			g_curCraft = g_objectTable[mothershipObjectIndex].mobj->pCraft;
			modelIndex = g_curCraft->modelIndex;
			pai_RotateLocalVectorToWorldScratch(&g_objectTable[mothershipObjectIndex],
												g_modelDefs[modelIndex].hangarPoints.inside.side,
												g_modelDefs[modelIndex].hangarPoints.inside.up,
												g_modelDefs[modelIndex].hangarPoints.inside.forward);
			g_spawnWorldX = g_rotatedX + g_objectTable[mothershipObjectIndex].world_x;
			g_spawnWorldY = g_rotatedY + g_objectTable[mothershipObjectIndex].world_y;
			g_spawnWorldZ = g_rotatedZ + g_objectTable[mothershipObjectIndex].world_z;
			pai_RotateLocalVectorToWorldScratch(&g_objectTable[mothershipObjectIndex],
												g_modelDefs[modelIndex].hangarPoints.outside.side,
												g_modelDefs[modelIndex].hangarPoints.outside.up,
												g_modelDefs[modelIndex].hangarPoints.outside.forward);
			worldlocx = g_rotatedX + g_objectTable[mothershipObjectIndex].world_x;
			worldlocy = g_rotatedY + g_objectTable[mothershipObjectIndex].world_y;
			worldlocz = g_rotatedZ + g_objectTable[mothershipObjectIndex].world_z;
		}
		trig2_ctop(worldlocx - g_spawnWorldX, worldlocy - g_spawnWorldY, worldlocz - g_spawnWorldZ);
		g_spawnFromMothershipFlag = 1;
		g_spawnYaw = (uint16_t)trig2_xyangle;
		g_spawnPitch = (uint16_t)pitchQ16;
		g_spawnFormation = 0;
		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft >
			LARGE_MOTHERSHIP_FORMATION_THRESHOLD) {
			g_spawnFormation = LARGE_MOTHERSHIP_FORMATION;
		}
		g_spawnFormationSpacing = 0;
	} else {
		Mission_ResolveObjectOrMissionPointWorldLoc(MISSION_POINT_1, g_currentFlightGroupIdx);
		g_spawnWorldX = worldlocx;
		g_spawnWorldY = worldlocy;
		g_spawnWorldZ = worldlocz;

		if (missionStarted != 0) {
			if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber != 0) {
					uint8_t flightGroupIndex;

					memset(playerFlightGroupsByTeam, 0, sizeof(playerFlightGroupsByTeam));
					for (flightGroupIndex = 0; flightGroupIndex < (int16_t)g_missionHeader.numFlightGroups;
						 ++flightGroupIndex) {
						if (g_missionFlightGroups[flightGroupIndex].fg.playerNumber != 0) {
							++playerFlightGroupsByTeam[g_missionFlightGroups[flightGroupIndex].fg.team];
						}
					}
					if (playerFlightGroupsByTeam[0] == 1) {
						uint8_t waveIndex =
							(uint8_t)g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount /
							g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
						int8_t playerNumberOffset;
						uint8_t targetPlayerNumber;

						playerNumberOffset =
							(g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber & 1) != 0
								? waveIndex - QUICK_START_FORMATION_CENTER
								: -QUICK_START_FORMATION_CENTER - waveIndex;
						targetPlayerNumber =
							(uint8_t)((QUICK_START_FORMATION_CENTER * playerNumberOffset +
									   g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber) &
									  (PLAYER_NUMBER_COUNT - 1)) +
							1;
						for (flightGroupIndex = 0;
							 flightGroupIndex < (int16_t)g_missionHeader.numFlightGroups;
							 ++flightGroupIndex) {
							if (g_missionFlightGroups[flightGroupIndex].fg.playerNumber ==
								targetPlayerNumber) {
								Mission_ResolveObjectOrMissionPointWorldLoc(MISSION_POINT_1,
																			flightGroupIndex);
								g_spawnWorldX = worldlocx;
								g_spawnWorldY = worldlocy;
								g_spawnWorldZ = worldlocz;
								break;
							}
						}
					}
				}
				if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) {
					int randomOffset = GameRand() & RANDOM_PLAYER_OFFSET_MASK;

					if ((GameRand() & 1) != 0)
						g_spawnWorldX += randomOffset;
					else
						g_spawnWorldX -= randomOffset;
					randomOffset = GameRand() & RANDOM_PLAYER_OFFSET_MASK;
					if ((GameRand() & 1) != 0)
						g_spawnWorldY += randomOffset;
					else
						g_spawnWorldY -= randomOffset;
				}
			} else if (g_missionFileVersion == MISSION_VERSION_14 &&
					   g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber != 0 &&
					   g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointEnabled[1] != 0) {
				Mission_ResolveObjectOrMissionPointWorldLoc(MISSION_POINT_2, g_currentFlightGroupIdx);
				g_spawnWorldX = worldlocx;
				g_spawnWorldY = worldlocy;
				g_spawnWorldZ = worldlocz;
			}
		}

		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointEnabled[4] != 0) {
			Mission_ResolveObjectOrMissionPointWorldLoc(MISSION_POINT_5, g_currentFlightGroupIdx);
			trig2_ctop(worldlocx - g_spawnWorldX, worldlocy - g_spawnWorldY, worldlocz - g_spawnWorldZ);
			g_spawnYaw = (uint16_t)trig2_xyangle;
			spawnPitch = pitchQ16;
		} else {
			trig2_xyangle = 0;
			g_spawnYaw = 0;
			spawnPitch = DEFAULT_SPAWN_PITCH;
			pitchQ16 = DEFAULT_SPAWN_PITCH;
		}
		g_spawnPitch = (uint16_t)spawnPitch;

		if (g_flightMissionState.provingGroundsModeActive == 0 &&
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMethod == 0 && missionStarted != 0 &&
			craftOrdinal == UINT16_MAX &&
			g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx == -1) {
			trig2_xyangle += HYPERSPACE_REVERSE_ANGLE;
			pitchQ16 = HYPERSPACE_REVERSE_ANGLE - pitchQ16;
			trig2_movexyz(HYPERSPACE_DISTANCE, trig2_xyangle, (uint16_t)pitchQ16);
			g_spawnWorldX += HYPERSPACE_OFFSET_SCALE * trig2_xmovedist;
			g_spawnWorldY += HYPERSPACE_OFFSET_SCALE * trig2_ymovedist;
			g_spawnWorldZ += HYPERSPACE_OFFSET_SCALE * trig2_zmovedist;
			trig2_xmovedist *= HYPERSPACE_OFFSET_SCALE;
			trig2_ymovedist *= HYPERSPACE_OFFSET_SCALE;
			trig2_zmovedist *= HYPERSPACE_OFFSET_SCALE;
			g_spawnOutOfHyperspaceFlag = 1;
		}
		g_spawnFormation = g_missionFlightGroups[g_currentFlightGroupIdx].fg.formation;
		g_spawnFormationSpacing = g_missionFlightGroups[g_currentFlightGroupIdx].fg.formationSpacing;
	}

	{
		uint8_t team = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
		uint8_t status1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status1;
		uint8_t status2;
		uint8_t groupAI;

		g_spawnIff = g_missionFlightGroups[g_currentFlightGroupIdx].fg.iff;
		status2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status2;
		g_spawnTeamId = team;
		g_spawnStatus1 = status1;
		spawnedCraftCount = 0;
		g_spawnStatus2 = status2;
		g_spawnObjectKind = CRAFT_OBJECT_KIND_ACTIVE;
		groupAI = g_missionFlightGroups[g_currentFlightGroupIdx].fg.groupAI;
		g_spawnGenusId =
			g_modelTypeTable
				[g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
					.genusId;
		g_spawnGroupAI = groupAI;
	}

	if (craftOrdinal == UINT16_MAX) {
		g_spawnCraftOrdinal = 0;
		g_spawnLeaderObjIdx = UINT8_MAX;
		while (g_spawnCraftOrdinal < g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft) {
			if (g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] >
				g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
				if (Mission_InitFlightGroupObjectSlot() == UINT16_MAX)
					return 0;
				++spawnedCraftCount;
				++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft ==
					g_spawnCraftOrdinal) {
					++g_missionFgStats[g_currentFlightGroupIdx]
						  .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];
				}
			}
			++g_spawnCraftOrdinal;
		}
	} else if (g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] >
			   g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
		g_spawnCraftOrdinal = craftOrdinal;
		if (Mission_InitFlightGroupObjectSlot() == UINT16_MAX)
			return 0;
		spawnedCraftCount = 1;
		++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft == g_spawnCraftOrdinal) {
			++g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ARRIVED];
		}
	}
	if (missionStarted != 0 && craftOrdinal == UINT16_MAX && spawnedCraftCount != 0) {
		uint16_t modelIndex = GetModelIndexFromType(g_spawnObjectType);

		msg_reportfgcreation(g_currentFlightGroupIdx, modelIndex);
	}
	return 1;
}

// FUNCTION: XVT 0x456AC0
uint16_t Mission_InitFlightGroupObjectSlot(void) {
	enum {
		PLAYER_COUNT = 8,
		TEAM_COUNT = 10,
		AI_RATING_COUNT = 6,
		PLAYER_RATING_COUNT = 25,
		FLIGHT_GROUP_COUNT = 48,
		LASER_GROUP_COUNT = 2,
		WARHEAD_LAUNCHER_COUNT = 2,
		COMPONENT_COUNT = 50,
		ORDER_GOAL_COUNT = 4,
		BEAM_EFFECT_COUNT = 5,
		SPECIAL_CARGO_NAME_LENGTH = 16,
		HUD_FEATURES_ALL = 0x1FFF,
		HUD_FEATURE_SHIELDS = 0x0800,
		HUD_FEATURE_BEAM = 0x1000,
		HUD_FEATURE_BEAM_LEVEL = 0x0010,
		LASER_CHARGE_FULL = 127,
		BEAM_CHARGE_FULL = 9999,
		SHIELD_OVERFLOW_CLAMP = 32700,
		DEFAULT_TARGET_Z_ANGLE = 0x4000,
		DEFAULT_THROTTLE = 0x8000,
		PLAYER_THROTTLE_PRESET = 21845,
		ADVANCED_MISSILE_OBJECT_TYPE = 149,
		SPECIAL_WARHEAD_INDEX = 5,
		SPECIAL_WARHEAD_MODEL_OBJECT_TYPE = 12,
		SPECIAL_BOUNDS_OBJECT_TYPE = 58,
		SPECIAL_SHIELD_GENERATOR_OBJECT_TYPE = 54,
		PLATFORM_OBJECT_TYPE_FIRST = 60,
		PLATFORM_OBJECT_TYPE_END = 65,
		DYNAMIC_MODEL_OBJECT_TYPE_FIRST = 73,
		OUT_OF_HYPERSPACE_PLAN_NAME_INDEX = 52,
		FROM_MOTHERSHIP_PLAN_NAME_INDEX = 50,
		MISSION_VERSION_14 = 14,
		LEGACY_GENUS_OBJECT_TYPE_FIRST = 37,
		LEGACY_GENUS_OBJECT_TYPE_END = 38,
		PLATFORM_BEAM_DISABLED_STRIDE = 12,
	};

	uint16_t objectIndex;
	uint16_t objectSlotEnd;
	uint16_t objectSlotStart;
	int boundPlayerIdx;
	ObjectTypeId objectType;
	uint16_t modelIndex;
	uint16_t formationType;
	uint16_t waveNumber;
	int16_t formationSpacing;
	int16_t spacingScale;
	int16_t boundSizeX;
	int16_t boundSizeZ;
	int16_t boundSizeY;
	int16_t side;
	int16_t up;
	int16_t forward;
	int16_t formationDivisor;
	uint8_t hasPlayerOwner;
	uint8_t playerCraftBound;
	uint8_t engineGlowCount;
	int16_t throttleOff;
	int playerOwnerIdx;
	int maxBoundsExtent;
	int aiSkillLevel;
	uint16_t index;
	int slot;

	boundPlayerIdx = 0;
	objectType = g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	g_spawnObjectType = (uint8_t)objectType;
	objectSlotStart = g_objectSlotRangeByGenus[g_spawnGenusId].start;
	objectSlotEnd = g_objectSlotRangeByGenus[g_spawnGenusId].end;
	for (objectIndex = objectSlotStart; objectIndex < objectSlotEnd; ++objectIndex) {
		if (g_objectTable[objectIndex].objectType == 0)
			break;
	}
	if (objectIndex >= objectSlotEnd)
		return UINT16_MAX;

	collide_ResetObjectProximityForSlot(objectIndex);
	playerOwnerIdx = g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx;
	if (playerOwnerIdx != -1 &&
		g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerCraft == g_spawnCraftOrdinal &&
		g_initialSpawnBindPlayerCraftSlots != 0) {
		boundPlayerIdx = g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx;
		g_players[playerOwnerIdx].objectIndex = objectIndex;
		playerCraftBound = 1;
		g_objectTable[objectIndex].playerOwnerIdx = playerOwnerIdx;
	} else {
		playerCraftBound = 0;
		g_objectTable[objectIndex].playerOwnerIdx = -1;
	}
	hasPlayerOwner = g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1;
	g_objectTable[objectIndex].mobj->pCraft = &g_craftDataPoolBase[objectIndex - objectSlotStart];
	g_curCraft = &g_craftDataPoolBase[objectIndex - objectSlotStart];
	g_spawnObjectTypeByObjectSlot[objectIndex] = objectType;
	g_curCraft->effectiveAiObjectLink = NULL;
	g_curCraft->effectiveAiObjectSignature = 0;
	g_objectTable[objectIndex].objectType = (uint8_t)objectType;
	g_objectTable[objectIndex].objectSignature = g_nextObjectSignature++;
	maxBoundsExtent = g_modelTypeTable[objectType].maxBoundsExtent;
	if (objectType == SPECIAL_BOUNDS_OBJECT_TYPE)
		maxBoundsExtent *= 8;
	if (maxBoundsExtent < 0x2000)
		maxBoundsExtent *= 4;
	g_objectTable[objectIndex].mobj->damageAmount = maxBoundsExtent;
	g_curCraft->modelIndex = GetModelIndexFromType(objectType);
	modelIndex = g_curCraft->modelIndex;
	g_objectTable[objectIndex].mobj->iff = g_spawnIff;
	g_spawnedObjectIff = g_objectTable[objectIndex].mobj->iff;
	g_objectTable[objectIndex].mobj->team = g_spawnTeamId;
	g_objectTable[objectIndex].genusId = g_spawnGenusId;
	g_objectTable[objectIndex].mobj->state = g_modelTypeTable[objectType].familyId;
	g_objectTable[objectIndex].mobj->framesAlive = 0;
	g_objectTable[objectIndex].mobj->lifetimeTimer = 0;
	g_objectTable[objectIndex].mobj->simStateTimestamp = 0;
	g_objectTable[objectIndex].mobj->nodeSwitchIndex =
		g_missionFlightGroups[g_currentFlightGroupIdx].fg.markings;
	g_objectTable[objectIndex].mobj->sourceObjIdx = objectIndex;
	g_objectTable[objectIndex].mobj->sourceObjectType = (uint8_t)objectType;
	g_objectTable[objectIndex].flightGroupIdx = (uint8_t)g_currentFlightGroupIdx;
	g_curCraft->leader_obj_idx = g_spawnLeaderObjIdx;
	if (g_spawnLeaderObjIdx == UINT8_MAX)
		g_spawnLeaderObjIdx = (uint8_t)objectIndex;
	g_curCraft->aiFlight.formationType = g_spawnFormation;
	formationType = g_curCraft->aiFlight.formationType;
	g_curCraft->waveNumber = (uint8_t)g_spawnCraftOrdinal;
	waveNumber = g_curCraft->waveNumber;
	formationSpacing = g_spawnFormationSpacing;
	if (g_spawnFromMothershipFlag != 0)
		formationSpacing = 0;
	g_curCraft->aiFlight.separation = (uint8_t)formationSpacing;
	g_curCraft->pushAccumZ = 0;
	g_curCraft->pushAccumY = g_curCraft->pushAccumZ;
	g_curCraft->pushAccumX = g_curCraft->pushAccumY;
	if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalUnit == 0 ||
		g_missionFlightGroups[g_currentFlightGroupIdx].fg.disableWaveNumbering != 0) {
		g_curCraft->craftIndexInGroup = g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount;
		++g_curCraft->craftIndexInGroup;
	} else {
		++g_flightMissionState
			  .globalUnitCraftCount[g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalUnit];
		g_curCraft->craftIndexInGroup =
			g_flightMissionState
				.globalUnitCraftCount[g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalUnit];
	}

	boundSizeX = g_modelDefs[modelIndex].boundSizeX;
	boundSizeZ = g_modelDefs[modelIndex].boundSizeZ;
	boundSizeY = g_modelDefs[modelIndex].boundSizeY;
	spacingScale = formationSpacing + 1;
	forward = boundSizeY;
	forward *= spacingScale;
	forward *= g_formPosY[formationType][waveNumber];
	up = boundSizeZ;
	up *= spacingScale;
	up *= g_formPosZ[formationType][waveNumber];
	side = boundSizeX;
	side *= spacingScale;
	side *= g_formPosX[formationType][waveNumber];
	if (spacingScale == 1) {
		side += g_formPosX[formationType][waveNumber] * (boundSizeX / 2);
		up += g_formPosZ[formationType][waveNumber] * (boundSizeZ / 2);
		forward += g_formPosY[formationType][waveNumber] * (boundSizeY / 4);
	}
	formationDivisor = g_formationDivisor[formationType];
	if (formationDivisor != 1) {
		side /= formationDivisor;
		forward /= formationDivisor;
		up /= formationDivisor;
	}
	if (g_spawnCraftOrdinal == 0) {
		g_objectTable[objectIndex].world_x = g_spawnWorldX;
		g_objectTable[objectIndex].world_y = g_spawnWorldY;
		g_objectTable[objectIndex].world_z = g_spawnWorldZ;
		g_objectTable[objectIndex].yaw = g_spawnYaw;
		g_objectTable[objectIndex].pitch = g_spawnPitch;
		g_objectTable[objectIndex].roll = 0;
		g_objectTable[objectIndex].mobj->orientMatrixDirty = 1;
		g_objectTable[objectIndex].mobj->moveVectorDirty = g_objectTable[objectIndex].mobj->orientMatrixDirty;
		pai_calcrotatedpoint(&g_objectTable[objectIndex], side, up, forward);
	} else {
		pai_calcrotatedpoint(&g_objectTable[g_curCraft->leader_obj_idx], side, up, forward);
	}
	if (g_modelDefs[modelIndex].boundSizeShift != 0) {
		g_rotatedX *= 1 << g_modelDefs[modelIndex].boundSizeShift;
		g_rotatedY *= 1 << g_modelDefs[modelIndex].boundSizeShift;
		g_rotatedZ *= 1 << g_modelDefs[modelIndex].boundSizeShift;
	}
	g_objectTable[objectIndex].world_x = g_rotatedX + g_spawnWorldX;
	g_objectTable[objectIndex].mobj->prevWorldX = g_objectTable[objectIndex].world_x;
	g_objectTable[objectIndex].world_y = g_rotatedY + g_spawnWorldY;
	g_objectTable[objectIndex].mobj->prevWorldY = g_objectTable[objectIndex].world_y;
	g_objectTable[objectIndex].world_z = g_rotatedZ + g_spawnWorldZ;
	g_objectTable[objectIndex].mobj->prevWorldZ = g_objectTable[objectIndex].world_z;

	{
		const char* cargo = g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargo;
		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft != g_curCraft->waveNumber)
			cargo = g_missionFlightGroups[g_currentFlightGroupIdx].fg.cargo;
		for (index = 0; index < SPECIAL_CARGO_NAME_LENGTH; ++index)
			g_curCraft->specialCargoName[index] = cargo[index];
	}
	g_curCraft->boardingState = 0;
	g_curCraft->systemFlags = CRAFT_SUBSYSTEM_FLAGS_ALL;
	g_curCraft->damageStats.installedHudFeatureMask = HUD_FEATURES_ALL;
	g_objectTable[objectIndex].yaw = g_spawnYaw;
	g_curCraft->yaw = g_objectTable[objectIndex].yaw;
	g_objectTable[objectIndex].pitch = g_spawnPitch;
	g_curCraft->pitch = g_objectTable[objectIndex].pitch;
	g_objectTable[objectIndex].roll = 0;
	g_objectTable[objectIndex].mobj->rollImpulseRate = 0;
	g_objectTable[objectIndex].mobj->orientMatrixDirty = 1;
	g_objectTable[objectIndex].mobj->moveVectorDirty = g_objectTable[objectIndex].mobj->orientMatrixDirty;
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->aiFlight.climbState = 0;
	g_curCraft->aiFlight.diveState = g_curCraft->aiFlight.climbState;
	g_curCraft->aiFlight.headingForce = g_curCraft->aiFlight.diveState;
	g_curCraft->aiFlight.motionScale = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_curCraft->aiFlight.pitchAccel = -1;
	g_curCraft->aiFlight.turnAccel = -1;
	g_curCraft->aiFlight.rollRate = g_modelDefs[modelIndex].rollRate;
	g_curCraft->aiFlight.pitchRate = g_modelDefs[modelIndex].pitchRate;
	g_curCraft->aiFlight.turnRate = g_modelDefs[modelIndex].yawRate;
	g_curCraft->aiFlight.maxSpeedCache = g_modelDefs[modelIndex].maxSpeed;
	aiSkillLevel = g_spawnGroupAI;
	g_curCraft->aiSkill = g_aiSkillValueQ16ByLevel[aiSkillLevel];
	g_curCraft->cannonClassCount = 0;
	{
		uint16_t laserSlotCount = 0;
		for (index = 0; index < LASER_GROUP_COUNT; ++index) {
			uint16_t weaponSlot;
			uint16_t firstSlot;
			uint16_t lastSlot;
			g_curCraft->laserState.projectileTypeId[index] =
				g_modelDefs[modelIndex].laserGroupWeaponType[index];
			if (playerCraftBound != 0)
				g_curCraft->laserState.linkMode[index] = 1;
			else
				g_curCraft->laserState.linkMode[index] = 0;
			g_curCraft->laserState.burstRemaining[index] = 0;
			g_curCraft->laserState.nextSlot[index] = 0;
			g_curCraft->laserState.fireCooldownTicks[index] = 0;
			g_curCraft->laserState.lastFireTimestamp[index] = 0;
			if (g_curCraft->laserState.projectileTypeId[index] == 0)
				continue;
			laserSlotCount += g_modelDefs[modelIndex].laserGroupSlotCount[index];
			firstSlot = g_modelDefs[modelIndex].laserGroupFirstSlot[index];
			lastSlot = g_modelDefs[modelIndex].laserGroupLastSlot[index];
			if (g_modelDefs[modelIndex].laserGroupMountType[index] != 2) {
				++g_curCraft->cannonClassCount;
				g_curCraft->laserState.nextSlot[index] = (uint8_t)firstSlot;
			}
			for (weaponSlot = firstSlot; weaponSlot <= lastSlot; ++weaponSlot) {
				if (g_modelDefs[modelIndex].laserGroupMountType[index] != 2)
					g_curCraft->weaponSlots[weaponSlot].projectileTypeId =
						g_curCraft->laserState.projectileTypeId[index];
				else
					g_curCraft->weaponSlots[weaponSlot].projectileTypeId = 2;
				g_curCraft->weaponSlots[weaponSlot].laserCharge = LASER_CHARGE_FULL;
				g_curCraft->weaponSlots[weaponSlot].count = 0;
				g_curCraft->turretTargetStates[weaponSlot].targetObjIdx = UINT16_MAX;
			}
		}
		g_curCraft->laserRedirect = POWER_RECHARGE_MAINTENANCE;
		g_curCraft->laserSlotCount = (uint8_t)laserSlotCount;
	}
	if (g_curCraft->cannonClassCount == 0)
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_CANNONS;

	g_curCraft->warheadLauncherCount = 0;
	for (index = 0; index < WARHEAD_LAUNCHER_COUNT; ++index) {
		uint16_t weaponSlot;
		uint16_t lastSlot;
		if (index == 0 || GetModelIndexFromType(SPECIAL_WARHEAD_MODEL_OBJECT_TYPE) == modelIndex)
			g_curCraft->warheadSlotTypeIds[index] =
				g_warheadTypeIds[g_missionFlightGroups[g_currentFlightGroupIdx].fg.warhead];
		else
			g_curCraft->warheadSlotTypeIds[index] = 0;
		if (index == 1 && GetModelIndexFromType(SPECIAL_WARHEAD_MODEL_OBJECT_TYPE) == modelIndex &&
			g_flightMissionState.provingGroundsModeActive == 0) {
			g_curCraft->warheadSlotTypeIds[index] = ADVANCED_MISSILE_OBJECT_TYPE;
		}
		g_curCraft->warheadLauncherFlags[index] = 1;
		g_curCraft->warheadLauncherCooldownTicks[index] = 0;
		if (g_curCraft->warheadSlotTypeIds[index] == 0)
			continue;
		++g_curCraft->warheadLauncherCount;
		lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[index];
		for (weaponSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[index]; weaponSlot <= lastSlot;
			 ++weaponSlot) {
			uint16_t warhead = g_missionFlightGroups[g_currentFlightGroupIdx].fg.warhead;
			uint8_t ammoCount;
			g_curCraft->weaponSlots[weaponSlot].projectileTypeId = g_curCraft->warheadSlotTypeIds[index];
			g_curCraft->turretTargetStates[weaponSlot].targetObjIdx = UINT16_MAX;
			g_curCraft->weaponSlots[weaponSlot].laserCharge = LASER_CHARGE_FULL;
			if (index == 1 && GetModelIndexFromType(SPECIAL_WARHEAD_MODEL_OBJECT_TYPE) == modelIndex)
				warhead = SPECIAL_WARHEAD_INDEX;
			ammoCount = (uint8_t)MATH2_fraction(g_modelDefs[modelIndex].warheadLauncherValue[index],
												g_warheadAmmoCounts[warhead]);
			if (ammoCount == 0)
				ammoCount = 1;
			if (g_spawnStatus1 == 1 || g_spawnStatus2 == 1)
				ammoCount *= 2;
			else if (g_spawnStatus1 == 2 || g_spawnStatus2 == 2)
				ammoCount >>= 1;
			if (ammoCount == 0)
				ammoCount = 1;
			if (hasPlayerOwner != 0 && ammoCount > 9 &&
				g_objectTable[objectIndex].objectType != SPECIAL_WARHEAD_MODEL_OBJECT_TYPE)
				ammoCount = 9;
			g_curCraft->weaponSlots[weaponSlot].count = ammoCount;
		}
	}
	g_curCraft->warheadLockTicks = 0;
	if (g_curCraft->warheadLauncherCount == 0)
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_WARHEAD_LAUNCHER;

	g_curCraft->weaponStats.laserHitsScored = 0;
	g_curCraft->weaponStats.laserShotsFired = g_curCraft->weaponStats.laserHitsScored;
	g_curCraft->weaponStats.ionHitsScored = 0;
	g_curCraft->weaponStats.ionShotsFired = g_curCraft->weaponStats.ionHitsScored;
	g_curCraft->weaponStats.warheadHitsScored = 0;
	g_curCraft->weaponStats.warheadsFired = g_curCraft->weaponStats.warheadHitsScored;
	g_curCraft->field_29F = 0;
	if (playerCraftBound != 0) {
		g_players[boundPlayerIdx].boundObjectSignature = g_objectTable[objectIndex].objectSignature;
		g_players[boundPlayerIdx].boundFlightGroupIdx = g_currentFlightGroupIdx;
		g_players[boundPlayerIdx].missionStats.laserHitsScored = 0;
		g_players[boundPlayerIdx].missionStats.laserShotsFired = 0;
		g_players[boundPlayerIdx].missionStats.ionHitsScored = 0;
		g_players[boundPlayerIdx].missionStats.ionShotsFired = 0;
		g_players[boundPlayerIdx].perMissionKills.warheadHits = 0;
		g_players[boundPlayerIdx].warheadsFired = 0;
		g_players[boundPlayerIdx].missionStats.missionScore = 0;
		g_players[boundPlayerIdx].missionStats.ratingPromoPoints = 0;
		g_players[boundPlayerIdx].missionStats.worseRatingPromoPoints = 0;
		g_players[boundPlayerIdx].missionStats.field_0C = 0;
		g_players[boundPlayerIdx].missionStats.field_10 = 0;
		g_players[boundPlayerIdx].missionStats.field_14 = 0;
		g_players[boundPlayerIdx].perMissionKills.friendliesKilled = 0;
		g_players[boundPlayerIdx].perMissionKills.numCraftInspected = 0;
		g_players[boundPlayerIdx].perMissionKills.numSpecialInspected = 0;
		for (index = 0; index < FLIGHT_GROUP_COUNT; ++index) {
			g_players[boundPlayerIdx].perMissionKills.killsFullOnFlightGroup[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedOnFlightGroup[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsAssistOnFlightGroup[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsFullFromFlightGroup[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedFromFlightGroup[index] = 0;
		}
		for (index = 0; index < PLAYER_RATING_COUNT; ++index) {
			g_players[boundPlayerIdx].perMissionKills.killsFullOnPlayerRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedOnPlayerRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsAssistOnPlayerRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killedByPlayerRating[index] = 0;
		}
		for (index = 0; index < AI_RATING_COUNT; ++index) {
			g_players[boundPlayerIdx].perMissionKills.killsFullOnAiRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedOnAiRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsAssistOnAiRating[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killedByAiRating[index] = 0;
		}
		for (index = 0; index < PLAYER_COUNT; ++index) {
			g_players[boundPlayerIdx].perMissionKills.killsFullOnPlayer[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedOnPlayer[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsFullFromPlayer[index] = 0;
			g_players[boundPlayerIdx].perMissionKills.killsSharedFromPlayer[index] = 0;
		}
		g_players[boundPlayerIdx].perMissionKills.totalCraftLosses = 0;
		g_players[boundPlayerIdx].perMissionKills.lossesByCollisions = 0;
		g_players[boundPlayerIdx].perMissionKills.lossesByStarships = 0;
		g_players[boundPlayerIdx].perMissionKills.lossesByMines = 0;
	}

	g_curCraft->hullMax = g_modelDefs[modelIndex].hullStrength;
	g_curCraft->systemDamageHullThreshold = g_modelDefs[modelIndex].systemDamageHullThreshold;
	g_curCraft->hullDamage = 0;
	g_curCraft->damageStats.lastSystemHitTime = 0;
	g_curCraft->subsystemDamage = 0;
	g_curCraft->damageStats.damageReceivedTotal = 0;
	g_curCraft->damageStats.damageReceivedByPlayerOwnedCraft = 0;
	g_curCraft->damageStats.damageFromCollision = 0;
	g_curCraft->damageStats.damageFromStarship = 0;
	g_curCraft->damageStats.damageFromMine = 0;
	for (slot = 0; slot < PLAYER_COUNT; ++slot)
		g_curCraft->damageStats.damageFromPlayer[slot] = 0;
	for (slot = 0; slot < TEAM_COUNT; ++slot) {
		g_curCraft->damageStats.damageFromFlightGroupAmount[slot] = 0;
		g_curCraft->attackedByTeam[slot] = 0;
	}
	for (slot = 0; slot < AI_RATING_COUNT; ++slot)
		g_curCraft->damageStats.damageFromAiSkill[slot] = 0;
	g_curCraft->weaponFireInhibitTimer = 0;
	g_curCraft->missionAccountingDone = 0;
	g_curCraft->unusedMissionFlag = 0;
	g_curCraft->notDisabledAccountingSuppress = 0;
	g_curCraft->wasCaptured = 0;
	g_curCraft->sFoilState = 0;
	for (index = 0; index < BEAM_EFFECT_COUNT; ++index)
		g_curCraft->beamEffectAccum[index] = 0;
	g_curCraft->beamActive = 0;
	g_curCraft->beamTimer = 0;
	g_curCraft->beamTargetObjIdx = -1;
	for (slot = 0; slot < TEAM_COUNT; ++slot) {
		if (g_objectTable[objectIndex].mobj->team == slot)
			g_curCraft->iffVisibility[slot] = 1;
		else
			g_curCraft->iffVisibility[slot] = 0;
	}

	if (g_modelDefs[modelIndex].hasHyperdrive == 0 && g_spawnStatus1 != 9 && g_spawnStatus2 != 9 &&
		g_spawnStatus1 != 16 && g_spawnStatus2 != 16) {
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_HYPERDRIVE;
	}
	if (g_spawnStatus1 == 6 || g_spawnStatus2 == 6)
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_HYPERDRIVE;
	g_curCraft->shieldEnergy[0] = g_modelDefs[modelIndex].shieldStrength;
	if (playerCraftBound != 0) {
		g_curCraft->shieldEnergy[1] = g_modelDefs[modelIndex].shieldStrength;
		g_curCraft->shieldDistribMode = SHIELD_DISTRIBUTION_EVEN;
	} else {
		int combinedShieldEnergy = g_curCraft->shieldEnergy[0] + g_modelDefs[modelIndex].shieldStrength;

		g_curCraft->shieldEnergy[0] = combinedShieldEnergy;
		g_curCraft->shieldEnergy[1] = 0;
		g_curCraft->shieldDistribMode = SHIELD_DISTRIBUTION_FULLY_FORWARD;
	}
	if (g_spawnStatus1 == 3 || g_spawnStatus2 == 3) {
		g_curCraft->shieldEnergy[0] = 0;
		g_curCraft->shieldEnergy[1] = 0;
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_SHIELDS;
	} else if (g_spawnStatus1 == 4 || g_spawnStatus2 == 4) {
		g_curCraft->shieldEnergy[0] >>= 1;
		g_curCraft->shieldEnergy[1] >>= 1;
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_SHIELDS;
	} else if (g_spawnStatus1 == 7 || g_spawnStatus2 == 7) {
		g_curCraft->shieldEnergy[0] = 0;
		g_curCraft->shieldEnergy[1] = 0;
	} else if (g_spawnStatus1 == 19 || g_spawnStatus2 == 19 || g_spawnStatus1 == 13 || g_spawnStatus2 == 13) {
		g_curCraft->shieldEnergy[0] >>= 1;
		g_curCraft->shieldEnergy[1] >>= 1;
	} else if (g_spawnStatus1 == 18 || g_spawnStatus2 == 18 || g_spawnStatus1 == 12 || g_spawnStatus2 == 12) {
		g_curCraft->shieldEnergy[0] *= 2;
		g_curCraft->shieldEnergy[1] *= 2;
		if (g_curCraft->shieldEnergy[0] < 0)
			g_curCraft->shieldEnergy[0] = SHIELD_OVERFLOW_CLAMP;
		if (g_curCraft->shieldEnergy[1] < 0)
			g_curCraft->shieldEnergy[1] = SHIELD_OVERFLOW_CLAMP;
	}
	g_curCraft->shieldRedirect = POWER_RECHARGE_MAINTENANCE;
	if (g_modelDefs[modelIndex].hasShields == 0 && g_spawnStatus1 != 8 && g_spawnStatus2 != 8 &&
		g_spawnStatus1 != 16 && g_spawnStatus2 != 16) {
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_SHIELDS;
		g_curCraft->shieldEnergy[0] = 0;
		g_curCraft->shieldEnergy[1] = 0;
		g_curCraft->damageStats.installedHudFeatureMask ^= HUD_FEATURE_SHIELDS;
	}
	g_curCraft->beamTypeId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.beam;
	if (g_spawnObjectType == 1 || g_spawnObjectType == 2 || g_spawnObjectType == 4 ||
		g_spawnObjectType == 3 || g_spawnObjectType == 5) {
		g_curCraft->beamTypeId = BEAM_TYPE_NONE;
	}
	g_curCraft->beamLevel = POWER_RECHARGE_MAINTENANCE;
	g_curCraft->beamPresent = BEAM_CHARGE_FULL;
	if (g_curCraft->beamTypeId == BEAM_TYPE_NONE) {
		g_curCraft->beamPresent = 0;
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_BEAM_SYSTEM;
		g_curCraft->damageStats.installedHudFeatureMask ^= HUD_FEATURE_BEAM;
		g_curCraft->damageStats.installedHudFeatureMask ^= HUD_FEATURE_BEAM_LEVEL;
	}
	g_curCraft->cmTypeId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.countermeasures;
	if (g_curCraft->cmTypeId != COUNTERMEASURE_TYPE_NONE) {
		g_curCraft->cmAmmoCount = g_modelDefs[modelIndex].countermeasureCount;
		if (g_curCraft->cmTypeId == COUNTERMEASURE_TYPE_FLARE)
			g_curCraft->cmAmmoCount = (uint8_t)MATH2_fraction(g_curCraft->cmAmmoCount, 0xAAACu);
	} else {
		g_curCraft->cmAmmoCount = 0;
		g_curCraft->systemFlags ^= CRAFT_SUBSYSTEM_FLAG_COUNTERMEASURES;
	}
	g_curCraft->chaffActiveTimer = 0;
	g_curCraft->cmFireCooldownTimer = 0;
	g_curCraft->workingSubsystems = g_curCraft->systemFlags;
	g_curCraft->damageStats.activeHudFeatureMask = g_curCraft->damageStats.installedHudFeatureMask;
	for (index = 0; index < sizeof(g_objectTable[objectIndex].typeSpecificByte); ++index)
		g_objectTable[objectIndex].typeSpecificByte[index] = 0;
	for (index = 0; index < COMPONENT_COUNT; ++index) {
		g_curCraft->componentState[index] = 0;
		g_curCraft->meshRotation[index] = 0;
		g_curCraft->componentHp[index] = UINT8_MAX;
	}
	{
		int meshCount;

		if (objectType < DYNAMIC_MODEL_OBJECT_TYPE_FIRST)
			meshCount = g_objectTypeMeshCache[objectType].meshCount;
		else
			meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
		for (index = 0; index < meshCount; ++index) {
			MeshComponentType meshType;
			if (objectType < DYNAMIC_MODEL_OBJECT_TYPE_FIRST) {
				unsigned int cachedMeshIndex = index;
				if (cachedMeshIndex >= (unsigned int)g_objectTypeMeshCache[objectType].meshCount)
					cachedMeshIndex = g_objectTypeMeshCache[objectType].meshCount - 1;
				meshType = g_objectTypeMeshCache[objectType].meshTypes[cachedMeshIndex];
			} else {
				meshType = ModelMesh_GetObjectTypeMeshType(objectType, index);
			}
			if (ModelMesh_IsObjectTypeMeshDamageable(objectType, index) != 0) {
				uint8_t componentHp;

				if (objectType == SPECIAL_SHIELD_GENERATOR_OBJECT_TYPE &&
					meshType == MESH_COMPONENT_08_SHLD_GEN) {
					componentHp = g_meshTypeComponentMaxHp[meshType];
					componentHp += componentHp;
					--componentHp;
				} else {
					componentHp = g_meshTypeComponentMaxHp[meshType];
				}
				g_curCraft->componentHp[index] = componentHp;
			}
			if ((g_spawnStatus1 == 5 || g_spawnStatus2 == 5 || g_spawnStatus1 == 14 ||
				 g_spawnStatus2 == 14) &&
				(meshType == MESH_COMPONENT_04_LASR_TUR || meshType == MESH_COMPONENT_21_LASR_TUR ||
				 meshType == MESH_COMPONENT_05_LASR_GUN)) {
				g_curCraft->componentHp[index] = 0;
				g_curCraft->componentState[index] = 4;
			}
		}
	}
	if (g_spawnGenusId == CRAFT_GENUS_PLATFORM && g_spawnObjectType >= PLATFORM_OBJECT_TYPE_FIRST &&
		g_spawnObjectType < PLATFORM_OBJECT_TYPE_END &&
		g_missionFlightGroups[g_currentFlightGroupIdx].fg.beam != 0) {
		uint16_t disabledCount =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.beam == BEAM_TYPE_TRACTOR ? 6 : 12;
		uint16_t disabledSlot =
			(g_spawnObjectType - PLATFORM_OBJECT_TYPE_FIRST) * PLATFORM_BEAM_DISABLED_STRIDE;
		int disabledSlotEnd = disabledSlot + disabledCount;

		for (; disabledSlot < disabledSlotEnd; ++disabledSlot) {
			uint8_t componentIndex = g_platformBeamDisabledComponentIds[disabledSlot];
			if (componentIndex != UINT8_MAX) {
				g_curCraft->componentHp[componentIndex] = 0;
				g_curCraft->componentState[componentIndex] = 4;
			}
		}
	}

	{
		int order = g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[0].order;
		uint16_t leaderPlan = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
		uint8_t followerPlan = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
		PaiPlanRecord* plan;
		uint16_t throttleSpeed;
		g_curCraft->aiController.pendingPlanId = (uint8_t)leaderPlan;
		g_curCraft->aiController.currentPlanId = g_curCraft->aiController.pendingPlanId;
		if (g_spawnOutOfHyperspaceFlag != 0)
			followerPlan = g_builtinPlanIdByNameIndex[OUT_OF_HYPERSPACE_PLAN_NAME_INDEX];
		else if (g_spawnFromMothershipFlag != 0)
			followerPlan = g_builtinPlanIdByNameIndex[FROM_MOTHERSHIP_PLAN_NAME_INDEX];
		else if (g_curCraft->leader_obj_idx == UINT8_MAX)
			followerPlan = (uint8_t)leaderPlan;
		g_curCraft->aiController.pendingPlanId = followerPlan;
		plan = &g_planTable[leaderPlan];
		if ((strcmp(plan->name, "nullpln") == 0 || strcmp(plan->name, "stationaryldrpln") == 0 ||
			 strcmp(plan->name, "stationaryflwpln") == 0 || strcmp(plan->name, "disabledpln") == 0) &&
			playerCraftBound == 0) {
			throttleSpeed = 0;
		} else {
			throttleSpeed = DEFAULT_THROTTLE;
			if (strcmp(plan->name, "escortldr1pln") != 0)
				throttleSpeed = g_orderThrottleToCraftThrottleSpeed
					[g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[0].throttle];
		}
		engineGlowCount = g_modelDefs[modelIndex].engineGlowCount;
		throttleOff = -1;
		g_curCraft->objectKind = g_spawnObjectKind;
		g_curCraft->throttleSpeed = throttleSpeed;
		g_curCraft->engineOutputScale = (uint16_t)throttleOff;
		g_objectTable[objectIndex].mobj->speed =
			MATH2_fraction(g_modelDefs[modelIndex].maxSpeed, throttleSpeed);
		g_objectTable[objectIndex].mobj->speedRemainder = 0;
	}
	if (playerCraftBound != 0) {
		g_players[boundPlayerIdx].selectedWarhead = 0;
		g_players[boundPlayerIdx].selectedWeaponMode = 0;
		g_players[boundPlayerIdx].boundCraftEngineGlowCount = engineGlowCount;
		g_players[boundPlayerIdx].throttlePreset[0] = PLAYER_THROTTLE_PRESET;
		g_players[boundPlayerIdx].laserPreset[0] = POWER_RECHARGE_MAINTENANCE;
		g_players[boundPlayerIdx].shieldPreset[0] = POWER_RECHARGE_MAINTENANCE;
		g_players[boundPlayerIdx].beamPreset[0] = POWER_RECHARGE_MAINTENANCE;
		g_players[boundPlayerIdx].throttlePreset[1] = throttleOff;
		g_players[boundPlayerIdx].laserPreset[1] = POWER_RECHARGE_INCREASED;
		g_players[boundPlayerIdx].shieldPreset[1] = POWER_RECHARGE_MAINTENANCE;
		g_players[boundPlayerIdx].beamPreset[1] = POWER_RECHARGE_MAINTENANCE;
	}
	for (index = 0; index < ORDER_GOAL_COUNT; ++index) {
		g_curCraft->aiController.orderScratch.completionState[index] = 0;
		g_curCraft->aiController.orderScratch.goalProgress[index] = 0;
	}
	g_curCraft->aiController.currentOrderSlot = 0;
	g_curCraft->aiController.orderStateFlag = 0;
	g_curCraft->aiController.targetObjIdx = UINT16_MAX;
	g_curCraft->aiController.candidateTargetIdx = g_curCraft->aiController.targetObjIdx;
	g_curCraft->aiController.targetSignature = 0;
	g_curCraft->aiController.hasLiveTarget = 0;
	g_curCraft->aiController.targetComponent = UINT16_MAX;
	g_curCraft->carriedObjectIndex = g_curCraft->aiController.targetComponent;
	g_curCraft->carrierObjIdx = g_curCraft->carriedObjectIndex;
	g_curCraft->aiFlight.impactObjIdx = UINT16_MAX;
	g_curCraft->aiController.escortTargetFG = UINT8_MAX;
	g_curCraft->aiFlight.goHomeFlag = 0;
	g_curCraft->aiFlight.missionAbortedFlag = 0;
	g_curCraft->aiFlight.departTimerFlag = 0;
	g_curCraft->aiFlight.departClockHours = 0;
	g_curCraft->aiFlight.departClockMinutes = 0;
	g_curCraft->aiFlight.departClockSeconds = 0;
	g_curCraft->aiFlight.reactionTimer = 0;
	g_curCraft->commandedSpeed = 0;
	g_curCraft->aiFlight.boardedAccountingDone = 0;
	g_curCraft->aiFlight.orderActionCounter = 0;
	g_curCraft->aiFlight.orderActionFlag = 0;
	g_curCraft->aiFlight.objSignatureCount = 0;
	g_curCraft->aiController.targetComponent = UINT16_MAX;
	g_curCraft->aiController.escortTargetFG = UINT8_MAX;
	g_curCraft->aiController.aimPointX = 0;
	g_curCraft->aiController.aimPointY = 0;
	g_curCraft->aiController.aimPointZ = 0;
	g_curCraft->aiController.targetZAngle = DEFAULT_TARGET_Z_ANGLE;
	g_curCraft->aiController.targetRoll = 0;
	g_curCraft->aiController.targetXYAngle = 0;
	g_curCraft->aiController.waypointIndex = ORDER_GOAL_COUNT;
	g_curCraft->aiController.savedPlanId = 0;
	g_curCraft->aiController.thinkInterval = g_aiThinkIntervalBySkill[aiSkillLevel];
	g_curCraft->aiController.savedRandSeed = GameRand() ^ 0xBEEF;
	g_curCraft->aiController.maneuverMode = AI_MANEUVER_MODE_NULL;
	g_curCraft->aiController.maneuverPhase = 0;
	g_curCraft->aiController.maneuverTimer = 0;
	for (index = 0; index < CRAFT_SUBSYSTEM_COUNT; ++index) {
		g_curCraft->systemDisplaySlotBySystem[index] = (uint8_t)index;
		g_curCraft->systemHealth[index] = 100;
		g_curCraft->systemTimer[index] = 0;
	}
	pai_setupcraftcontext(objectIndex);
	pai_ApplyPendingPlanTargetAndManeuver(objectIndex);
	Craft_ClearTurretObjectLinks(g_curCraft);
	g_curCraft->playerCommandAvoidTargetObjIdx = UINT16_MAX;
	++g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount;
	if (g_missionFileVersion == MISSION_VERSION_14 &&
		(g_objectTable[objectIndex].objectType == LEGACY_GENUS_OBJECT_TYPE_FIRST ||
		 g_objectTable[objectIndex].objectType == LEGACY_GENUS_OBJECT_TYPE_END)) {
		g_objectTable[objectIndex].genusId = CRAFT_GENUS_TRANSPORT;
	}
	return objectIndex;
}

// FUNCTION: XVT 0x4587C0
void Mission_SpawnFlightGroupStaticObjects(uint16_t craftOrdinal) {
	int genusId;
	int xStep;
	int yStep;
	int zRowStep;
	int zColumnStep;
	uint8_t numberOfCraft;
	int gridExtent;
	uint32_t spawnIndex;
	uint8_t objectType;
	int column;
	int ordinal;
	int baseX;
	int baseY;
	int baseZ;
	int x;
	int z;
	uint16_t savedRandomState;
	int spawnX;
	int spawnY;
	int spawnZ;
	int objectIndex;
	int staticObjectEnd;
	ObjectRecord* object;

	if (g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] == 0)
		return;

	objectType = g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	if ((g_modelTypeTable[objectType].flags & 0x80u) == 0)
		return;

	genusId =
		g_modelTypeTable[g_craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
			.genusId;
	switch (genusId) {
		case CRAFT_GENUS_MINE:
			xStep = 0;
			yStep = 0;
			zRowStep = 0;
			zColumnStep = 0;
			switch (g_missionFlightGroups[g_currentFlightGroupIdx].fg.status1 & 3u) {
				case 0:
					xStep = 64;
					yStep = 64;
					break;
				case 1:
					yStep = 64;
					zColumnStep = 64;
					break;
				case 2:
					xStep = 64;
					zRowStep = 64;
					break;
				default:
					break;
			}

			numberOfCraft = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
			gridExtent = numberOfCraft - 1;
			baseX = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointX[0] -
					(int)((uint32_t)(gridExtent * xStep) >> 1);
			baseY = -(g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointY[0] +
					  (int)((uint32_t)(gridExtent * yStep) >> 1));
			baseZ = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointZ[0] -
					(int)((uint32_t)(gridExtent * zRowStep) >> 1) -
					(int)((uint32_t)(gridExtent * zColumnStep) >> 1);
			g_preparedSpawnYawByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.yaw;
			g_preparedSpawnPitchByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.pitch;
			g_preparedSpawnRollByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.roll;

			ordinal = 0;
			spawnIndex = 0;
			if (numberOfCraft != 0) {
				do {
					column = 0;
					if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft != 0) {
						x = baseX;
						z = baseZ + (int)spawnIndex * zRowStep;
						do {
							if ((craftOrdinal == UINT16_MAX || craftOrdinal == ordinal) &&
								g_missionFgStats[g_currentFlightGroupIdx]
										.outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] >
									g_missionFgStats[g_currentFlightGroupIdx]
										.outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
								g_preparedSpawnMissionX = x;
								g_preparedSpawnMissionY = baseY + (int)spawnIndex * yStep;
								g_preparedSpawnMissionZ = z;
								Mission_SpawnPreparedObject(g_currentFlightGroupIdx, CRAFT_GENUS_MINE,
															objectType);
							}
							++ordinal;
							x += xStep;
							z += zColumnStep;
							++column;
						} while (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft > column);
					}
					++spawnIndex;
				} while (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft > spawnIndex);
			}
			break;

		case CRAFT_GENUS_SATELLITE:
			g_preparedSpawnMissionX = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointX[0];
			g_preparedSpawnMissionY = -g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointY[0];
			g_preparedSpawnMissionZ = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointZ[0];
			g_preparedSpawnYawByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.yaw;
			g_preparedSpawnPitchByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.pitch;
			g_preparedSpawnRollByte = g_missionFlightGroups[g_currentFlightGroupIdx].fg.roll;
			Mission_SpawnPreparedObject(g_currentFlightGroupIdx, CRAFT_GENUS_SATELLITE, objectType);
			break;

		case CRAFT_GENUS_NORMAL_DEBRIS:
			baseX = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointX[0];
			savedRandomState = (uint16_t)g_gameRandStateB;
			spawnIndex = 0;
			baseY = -g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointY[0];
			baseZ = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointZ[0];
			g_gameRandStateB = (int16_t)g_asteroidFieldRandSeed;
			while (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft > spawnIndex) {
				do {
					spawnX = (GameRand() & 0x1FF) + baseX - 256;
					spawnY = (GameRand() & 0x1FF) + baseY - 256;
					spawnZ = (GameRand() & 0x1FF) + baseZ - 256;
					staticObjectEnd = g_regionStaticObjectSlotCount;
					objectIndex = g_regionMainObjectSlotEnd;
					staticObjectEnd += objectIndex;
					if (objectIndex < staticObjectEnd) {
						object = &g_objectTable[objectIndex];
						do {
							if (object->objectType != 0 && object->world_x == spawnX &&
								object->world_y == spawnY && object->world_z == spawnZ)
								break;
							++object;
							++objectIndex;
						} while (staticObjectEnd > objectIndex);
					}
				} while (g_regionStaticObjectSlotCount > objectIndex);

				g_preparedSpawnMissionX = spawnX;
				g_preparedSpawnMissionY = spawnY;
				g_preparedSpawnMissionZ = spawnZ;
				g_preparedSpawnYawByte = 0;
				g_preparedSpawnPitchByte = 0;
				g_preparedSpawnRollByte = 0;
				Mission_SpawnPreparedObject(g_currentFlightGroupIdx, CRAFT_GENUS_NORMAL_DEBRIS,
											(uint8_t)((uint16_t)GameRand() % 6u + 100u));
				++spawnIndex;
			}
			g_asteroidFieldRandSeed = (uint16_t)g_gameRandStateB;
			g_gameRandStateB = (int16_t)savedRandomState;
			break;

		default:
			break;
	}
}

// FUNCTION: XVT 0x458C80
uint16_t Mission_SpawnPreparedObject(uint16_t flightGroupIdx, int16_t genusId, uint8_t objectType) {
	uint16_t objectIndex;

	objectIndex = Object_FindFreeMissionSlot();
	if (objectIndex != UINT16_MAX) {
		g_objectTable[objectIndex].world_x = g_preparedSpawnMissionX;
		g_objectTable[objectIndex].world_y = g_preparedSpawnMissionY;
		g_objectTable[objectIndex].world_z = g_preparedSpawnMissionZ;
		g_objectTable[objectIndex].yaw = g_preparedSpawnYawByte;
		g_objectTable[objectIndex].pitch = g_preparedSpawnPitchByte;
		g_objectTable[objectIndex].roll = g_preparedSpawnRollByte;
		g_objectTable[objectIndex].world_x *= 256;
		g_objectTable[objectIndex].world_y *= 256;
		g_objectTable[objectIndex].world_z *= 256;
		g_objectTable[objectIndex].yaw <<= 8;
		g_objectTable[objectIndex].pitch <<= 8;
		g_objectTable[objectIndex].roll <<= 8;
		g_objectTable[objectIndex].objectSignature = g_nextObjectSignature++;
		g_objectTable[objectIndex].flightGroupIdx = (uint8_t)flightGroupIdx;
		g_objectTable[objectIndex].genusId = (uint8_t)genusId;
		g_objectTable[objectIndex].objectType = objectType;
		g_objectTable[objectIndex].typeSpecificWord = 1023;
		g_objectTable[objectIndex].typeSpecificByte[0] = 0;
		if (genusId == 8) {
			g_objectTable[objectIndex].typeSpecificByte[1] =
				(uint8_t)(29 *
						  (g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED] & 7));
		} else {
			g_objectTable[objectIndex].typeSpecificByte[1] = 0;
		}
		++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED];
	}
	return objectIndex;
}

// FUNCTION: XVT 0x458E20
void Mission_ResolveObjectOrMissionPointWorldLoc(unsigned int objOrMissionPointRef, int flightGroupIdx) {
	unsigned int currentMissionPointRef;
	int missionPointIdx;

	currentMissionPointRef = objOrMissionPointRef;
	if (objOrMissionPointRef < 0x8000u) {
		worldlocx = g_objectTable[objOrMissionPointRef].world_x;
		worldlocy = g_objectTable[objOrMissionPointRef].world_y;
		worldlocz = g_objectTable[objOrMissionPointRef].world_z;
		return;
	}

	if (objOrMissionPointRef == 0x8000u)
		currentMissionPointRef = g_missionFgStats[flightGroupIdx].currentMissionPointRef;

	missionPointIdx = (int)(currentMissionPointRef - 0x8000u);
	worldlocx = g_missionFlightGroups[flightGroupIdx].fg.missionPointX[missionPointIdx] * 256;
	worldlocy = -(g_missionFlightGroups[flightGroupIdx].fg.missionPointY[missionPointIdx] * 256);
	worldlocz = g_missionFlightGroups[flightGroupIdx].fg.missionPointZ[missionPointIdx] * 256;
}

// FUNCTION: XVT 0x459B40
void Mission_ResolveFormationSlotWorldLoc(uint16_t flightGroupIdx, uint16_t formationSlotIdx,
										  uint16_t basisObjIdx) {
	uint16_t objectType;
	uint16_t modelIndex;
	int16_t maxZ;

	objectType = g_missionFlightGroups[flightGroupIdx].fg.craftType;
	objectType = g_craftTypeToObjectType[objectType];
	modelIndex = g_modelTypeTable[objectType].modelIndex;
	maxZ = (int16_t)ModelBounds_GetMaxZ(objectType);

	Mission_ResolveObjectOrMissionPointWorldLoc(0x8000, flightGroupIdx);
	if ((g_modelTypeTable[objectType].flags & 0x80) != 0) {
		if (g_modelTypeTable[objectType].genusId == 9) {
			worldlocx = g_missionFlightGroups[flightGroupIdx].fg.missionPointX[0] * 256;
			worldlocy = -(g_missionFlightGroups[flightGroupIdx].fg.missionPointY[0] * 256);
			worldlocz = g_missionFlightGroups[flightGroupIdx].fg.missionPointZ[0] * 256;
		} else if (g_modelTypeTable[objectType].genusId == 8) {
			int16_t yStep;
			int16_t zRowStep;
			int16_t xStep;
			int16_t zColumnStep;
			int16_t craftCountMinusOne;
			int16_t baseX;
			int16_t baseY;
			int16_t baseZ;
			int16_t column;
			int16_t row;
			uint16_t slot;

			xStep = 0;
			yStep = 0;
			zRowStep = 0;
			zColumnStep = 0;
			switch (g_missionFlightGroups[flightGroupIdx].fg.status1 & 3) {
				case 0:
					xStep = 64;
					yStep = 64;
					break;
				case 1:
					yStep = 64;
					zColumnStep = 64;
					break;
				case 2:
					xStep = 64;
					zRowStep = 64;
					break;
			}

			craftCountMinusOne = (int16_t)(g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft - 1);
			baseX = (int16_t)(g_missionFlightGroups[flightGroupIdx].fg.missionPointX[0] -
							  xStep * craftCountMinusOne / 2);
			baseY = (int16_t)-(g_missionFlightGroups[flightGroupIdx].fg.missionPointY[0] +
							   yStep * craftCountMinusOne / 2);
			baseZ = (int16_t)(g_missionFlightGroups[flightGroupIdx].fg.missionPointZ[0] -
							  zRowStep * craftCountMinusOne / 2 - zColumnStep * craftCountMinusOne / 2);

			row = 0;
			slot = 0;
			if (g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft != 0) {
				do {
					column = 0;
					if (g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft != 0) {
						do {
							if (slot == formationSlotIdx) {
								worldlocx = (baseX + xStep * column) * 256;
								worldlocy = (baseY + yStep * row) * 256;
								worldlocz = (baseZ + zColumnStep * column + zRowStep * row) * 256 + maxZ;
								return;
							}
							++slot;
							++column;
						} while (column < (int)g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft);
					}
					++row;
				} while (row < (int)g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft);
			}
		}
	} else {
		int16_t boundX;
		int16_t boundZ;
		int16_t boundY;
		int16_t spacingScale;
		uint16_t formationIndex;
		int16_t up;
		int16_t forward;
		int16_t side;
		int rotatedX;
		int rotatedY;
		int rotatedZ;

		boundX = g_modelDefs[modelIndex].boundSizeX;
		boundZ = g_modelDefs[modelIndex].boundSizeZ;
		boundY = g_modelDefs[modelIndex].boundSizeY;
		spacingScale = g_missionFlightGroups[flightGroupIdx].fg.formationSpacing + 1;
		formationIndex = formationSlotIdx + 6 * g_missionFlightGroups[flightGroupIdx].fg.formation;
		forward = spacingScale;
		forward *= ((const int16_t*)g_formPosY)[formationIndex];
		forward *= boundY;
		up = spacingScale;
		up *= ((const int16_t*)g_formPosZ)[formationIndex];
		up *= boundZ;
		side = spacingScale;
		side *= ((const int16_t*)g_formPosX)[formationIndex];
		side *= boundX;

		if (spacingScale == 1) {
			side += ((const int16_t*)g_formPosX)[formationIndex] * (boundX / 2);
			up += ((const int16_t*)g_formPosZ)[formationIndex] * (boundZ / 2);
			forward += ((const int16_t*)g_formPosY)[formationIndex] * (boundY / 4);
		}
		if (basisObjIdx == UINT16_MAX) {
			rotatedY = 0;
			rotatedZ = 0;
			Mission_ResolveObjectOrMissionPointWorldLoc(0x8000, flightGroupIdx);
			rotatedX = 0;
		} else {
			pai_calcrotatedpoint(&g_objectTable[basisObjIdx], side, up, forward);
			rotatedX = g_rotatedX;
			rotatedY = g_rotatedY;
			rotatedZ = g_rotatedZ;
		}
		if (g_modelDefs[modelIndex].boundSizeShift != 0) {
			rotatedX *= 1 << g_modelDefs[modelIndex].boundSizeShift;
			rotatedY *= 1 << g_modelDefs[modelIndex].boundSizeShift;
			rotatedZ *= 1 << g_modelDefs[modelIndex].boundSizeShift;
		}
		g_rotatedX = rotatedX;
		g_rotatedY = rotatedY;
		g_rotatedZ = rotatedZ;
		worldlocx += rotatedX;
		worldlocy += rotatedY;
		worldlocz += rotatedZ;
	}
	worldlocz += maxZ;
}

// FUNCTION: XVT 0x45AD80
int Mission_LoadFile(char* fileName) {
	enum {
		MISSION_VERSION_LEGACY_TIE = UINT16_MAX,
		MISSION_VERSION_XVT_10 = 10,
		MISSION_VERSION_XVT_12 = 12,
		MISSION_VERSION_XVT_13 = 13,
		MISSION_VERSION_XVT_14 = 14,
		TEAM_COUNT = 10,
		LEGACY_IFF_NAME_FIRST = 2,
		LEGACY_WAYPOINT_COUNT = 15,
		PLAYER_COUNT = 8,
		PILOT_RECORD_SKIP_SIZE = 810,
		PILOT_FLAG_COUNT = 10,
		PILOT_STRING_COUNT = 32,
		OVERRIDE_STRING_SIZE = 64,
		FG_OVERRIDE_GROUP_COUNT = 8,
		OVERRIDE_STRING_COUNT = 3,
		GLOBAL_GOAL_COUNT = 7,
		GLOBAL_GOAL_TEXT_COUNT = 4,
		ACTIVE_GLOBAL_GOAL_COUNT = 3,
		CRAFT_ROLE_SIZE = 16,
	};

	XvtFile* stream;
	int16_t recordCount;
	int16_t flightGroupCount;
	int16_t messageCount;
	int16_t goalCount;
	int16_t index;
	int16_t stringLength;
	int16_t flightGroupIdx;
	char stringBuffer[160];
	MissionHeader missionHeader;
	int16_t messageIdx;
	int16_t outerIdx;
	int16_t slotIdx;
	int16_t textIdx;

	if (!File_OpenGlobalStream(fileName, "rb", 1, 0))
		return 0;

	stream = g_stream;
	FeDiskIo_ReadWithRetryPrompt(&g_missionFileVersion, sizeof(g_missionFileVersion), 1, g_stream);
	if (g_missionFileVersion != MISSION_VERSION_XVT_14) {
		if (g_missionFileVersion != MISSION_VERSION_LEGACY_TIE &&
			g_missionFileVersion != MISSION_VERSION_XVT_12 &&
			g_missionFileVersion != MISSION_VERSION_XVT_13) {
			return 0;
		}
	}

	if (g_missionFileVersion == MISSION_VERSION_XVT_14 || g_missionFileVersion == MISSION_VERSION_XVT_12 ||
		g_missionFileVersion == MISSION_VERSION_XVT_13) {
		if (g_missionFileVersion == MISSION_VERSION_XVT_10) {
			FeDiskIo_ReadWithRetryPrompt(&g_xvtV10MissionHeader, sizeof(g_xvtV10MissionHeader), 1, stream);
			g_missionHeader.numFlightGroups = g_xvtV10MissionHeader.numFlightGroups;
			g_missionHeader.numMessages = g_xvtV10MissionHeader.numMessages;
			g_missionHeader.timeLimitMin = g_xvtV10MissionHeader.timeLimitMin;
			g_missionHeader.timeLimitSec = g_xvtV10MissionHeader.timeLimitSec;
			g_missionHeader.winType = g_xvtV10MissionHeader.winType;
			g_missionHeader.backdrop = g_xvtV10MissionHeader.backdrop;
			g_missionHeader.rescue = g_xvtV10MissionHeader.rescue;
			g_missionHeader.allWaypointsShown = g_xvtV10MissionHeader.allWaypointsShown;
			memcpy(g_missionHeader.variables, g_xvtV10MissionHeader.variables,
				   sizeof(g_missionHeader.variables));
			memcpy(g_missionHeader.iffNames[0], g_xvtV10MissionHeader.iffNames[0],
				   sizeof(g_xvtV10MissionHeader.iffNames[0]));
			memcpy(g_missionHeader.iffNames[1], g_xvtV10MissionHeader.iffNames[1],
				   sizeof(g_xvtV10MissionHeader.iffNames[1]));
			memcpy(g_missionHeader.iffNames[2], g_xvtV10MissionHeader.iffNames[2],
				   sizeof(g_xvtV10MissionHeader.iffNames[2]));
			memcpy(g_missionHeader.iffNames[3], g_xvtV10MissionHeader.iffNames[3],
				   sizeof(g_xvtV10MissionHeader.iffNames[3]));
			g_missionHeader.missionType = g_xvtV10MissionHeader.missionType;
		} else {
			FeDiskIo_ReadWithRetryPrompt(&missionHeader, sizeof(missionHeader), 1, stream);
			memset(&g_missionHeader, 0, sizeof(g_missionHeader));
			g_missionHeader = missionHeader;
		}

		for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++flightGroupIdx) {
			if (g_missionFileVersion == MISSION_VERSION_XVT_10) {
				FeDiskIo_ReadWithRetryPrompt(&g_xvtV10FlightGroupText, sizeof(g_xvtV10FlightGroupText), 1,
											 stream);
				memcpy(g_missionFlightGroups[flightGroupIdx].fg.name, g_xvtV10FlightGroupText.name,
					   sizeof(g_xvtV10FlightGroupText.name));
				memcpy(g_missionFlightGroups[flightGroupIdx].fg.craftRole, g_xvtV10FlightGroupText.craftRole,
					   sizeof(g_xvtV10FlightGroupText.craftRole));
				memcpy(g_missionFlightGroups[flightGroupIdx].fg.cargo, g_xvtV10FlightGroupText.cargo,
					   sizeof(g_xvtV10FlightGroupText.cargo));
				memcpy(g_missionFlightGroups[flightGroupIdx].fg.specialCargo,
					   g_xvtV10FlightGroupText.specialCargo, sizeof(g_xvtV10FlightGroupText.specialCargo));
				FeDiskIo_ReadWithRetryPrompt(&g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft,
											 sizeof(g_missionFlightGroups[flightGroupIdx].fg) -
												 offsetof(XvtFlightGroup, specialCargoCraft),
											 1, stream);
			} else {
				FeDiskIo_ReadWithRetryPrompt(&g_missionFlightGroups[flightGroupIdx].fg,
											 sizeof(g_missionFlightGroups[flightGroupIdx].fg), 1, stream);
			}
			g_missionFlightGroups[flightGroupIdx].playerOwnerIdx = -1;
		}

		for (messageIdx = 0; messageIdx < (int16_t)g_missionHeader.numMessages; ++messageIdx) {
			if (g_missionFileVersion == MISSION_VERSION_XVT_10)
				index = (int16_t)messageIdx;
			else
				FeDiskIo_ReadWithRetryPrompt(&index, sizeof(index), 1, stream);
			FeDiskIo_ReadWithRetryPrompt(&g_missionMessages[index], sizeof(g_missionMessages[0]), 1, stream);
		}

		for (outerIdx = 0; outerIdx < TEAM_COUNT; ++outerIdx) {
			FeDiskIo_ReadWithRetryPrompt(&recordCount, sizeof(recordCount), 1, stream);
			FeDiskIo_ReadWithRetryPrompt(g_missionGlobalGoals[outerIdx], sizeof(g_missionGlobalGoals[0][0]),
										 recordCount, stream);
		}
		for (outerIdx = 0; outerIdx < TEAM_COUNT; ++outerIdx) {
			FeDiskIo_ReadWithRetryPrompt(&recordCount, sizeof(recordCount), 1, stream);
			if (recordCount != 0)
				FeDiskIo_ReadWithRetryPrompt(&g_missionTeams[outerIdx], sizeof(g_missionTeams[outerIdx]), 1,
											 stream);
		}

		for (index = 0; index < PLAYER_COUNT; ++index) {
			File_RawSeek(stream, PILOT_RECORD_SKIP_SIZE, SEEK_CUR);
			for (slotIdx = PILOT_FLAG_COUNT; slotIdx != 0; --slotIdx)
				File_RawSeek(stream, 1, SEEK_CUR);
			for (slotIdx = PILOT_STRING_COUNT; slotIdx != 0; --slotIdx) {
				FeDiskIo_ReadWithRetryPrompt(&stringLength, sizeof(stringLength), 1, stream);
				if (stringLength != 0)
					File_RawSeek(stream, stringLength, SEEK_CUR);
			}
			for (slotIdx = PILOT_STRING_COUNT; slotIdx != 0; --slotIdx) {
				FeDiskIo_ReadWithRetryPrompt(&stringLength, sizeof(stringLength), 1, stream);
				if (stringLength != 0)
					File_RawSeek(stream, stringLength, SEEK_CUR);
			}
		}

		for (outerIdx = 0; outerIdx < (int16_t)g_missionHeader.numFlightGroups; ++outerIdx) {
			for (index = 0; index < FG_OVERRIDE_GROUP_COUNT; ++index) {
				for (slotIdx = 0; slotIdx < OVERRIDE_STRING_COUNT; ++slotIdx) {
					uint16_t handle;
					FeDiskIo_ReadWithRetryPrompt(stringBuffer, OVERRIDE_STRING_SIZE, 1, stream);
					stringLength = (int16_t)strlen(stringBuffer);
					if (stringLength > 0) {
						handle = Memory_AllocHandleZeroed(stringLength + 1, 0);
						if (handle != 0) {
							char* string = Memory_LockHandle(handle);
							memcpy(string, stringBuffer, stringLength);
							string[stringLength] = '\0';
						}
						Memory_UnlockHandle(handle);
					} else {
						handle = 0;
					}
					g_missionFgOverrideStringHandles[outerIdx][index][slotIdx] = handle;
				}
			}
		}
		for (outerIdx = 0; outerIdx < TEAM_COUNT; ++outerIdx) {
			for (index = 0; index < GLOBAL_GOAL_COUNT; ++index) {
				for (textIdx = 0; textIdx < GLOBAL_GOAL_TEXT_COUNT; ++textIdx) {
					for (slotIdx = 0; slotIdx < OVERRIDE_STRING_COUNT; ++slotIdx) {
						uint16_t handle;
						FeDiskIo_ReadWithRetryPrompt(stringBuffer, OVERRIDE_STRING_SIZE, 1, stream);
						stringLength = (int16_t)strlen(stringBuffer);
						if (stringLength > 0) {
							handle = Memory_AllocHandleZeroed(stringLength + 1, 0);
							if (handle != 0) {
								char* string = Memory_LockHandle(handle);
								memcpy(string, stringBuffer, stringLength);
								string[stringLength] = '\0';
							}
							Memory_UnlockHandle(handle);
						} else {
							handle = 0;
						}
						g_globalGoalOverrideStringHandles[outerIdx][index][textIdx][slotIdx] = handle;
					}
				}
			}
		}
	} else {
		FeDiskIo_ReadWithRetryPrompt(&flightGroupCount, sizeof(flightGroupCount), 1, stream);
		FeDiskIo_ReadWithRetryPrompt(&messageCount, sizeof(messageCount), 1, stream);
		FeDiskIo_ReadWithRetryPrompt(&goalCount, sizeof(goalCount), 1, stream);
		g_missionHeader.numFlightGroups = flightGroupCount;
		g_missionHeader.numMessages = messageCount;
		FeDiskIo_ReadWithRetryPrompt(&g_tieMissionHeader, sizeof(g_tieMissionHeader), 1, stream);
		g_missionHeader.timeLimitMin = g_tieMissionHeader.backdrop;
		g_missionHeader.rescue = g_tieMissionHeader.rescue;
		g_missionHeader.allWaypointsShown = g_tieMissionHeader.all_way_shown;
		g_missionHeader.variables[0] = g_tieMissionHeader.mis_var[0];
		g_missionHeader.variables[1] = g_tieMissionHeader.mis_var[1];
		g_missionHeader.variables[2] = g_tieMissionHeader.mis_var[2];
		g_missionHeader.variables[3] = g_tieMissionHeader.mis_var[3];
		g_missionHeader.variables[4] = g_tieMissionHeader.mis_var[4];
		g_missionHeader.variables[5] = g_tieMissionHeader.mis_var[5];
		g_missionHeader.variables[6] = g_tieMissionHeader.mis_var[6];
		g_missionHeader.variables[7] = g_tieMissionHeader.mis_var[7];
		strcpy(g_missionTeams[0].endOfMissionMessages[0], (char*)g_tieMissionHeader.win_msg1[0]);
		strcpy(g_missionTeams[0].endOfMissionMessages[1], (char*)g_tieMissionHeader.win_msg1[1]);
		strcpy(g_missionTeams[0].endOfMissionMessages[2], (char*)g_tieMissionHeader.loss_msg[0]);
		strcpy(g_missionTeams[0].endOfMissionMessages[3], (char*)g_tieMissionHeader.loss_msg[1]);
		strcpy(g_missionTeams[0].endOfMissionMessages[4], (char*)g_tieMissionHeader.win_msg2[0]);
		strcpy(g_missionTeams[0].endOfMissionMessages[5], (char*)g_tieMissionHeader.win_msg2[1]);
		g_missionTeams[0].eomRawDelay[1] = g_tieMissionHeader.loss_msg_delay;
		strcpy(g_missionHeader.iffNames[0], g_tieMissionHeader.neutral_name[0]);
		strcpy(g_missionHeader.iffNames[1], g_tieMissionHeader.neutral_name[0]);
		strcpy(g_missionHeader.iffNames[2], g_tieMissionHeader.neutral_name[0]);
		strcpy(g_missionHeader.iffNames[3], g_tieMissionHeader.neutral_name[0]);

		for (flightGroupIdx = 0; flightGroupIdx < flightGroupCount; ++flightGroupIdx) {
			FeDiskIo_ReadWithRetryPrompt(&g_tieFlightGroup, sizeof(g_tieFlightGroup), 1, stream);
			strcpy(g_missionFlightGroups[flightGroupIdx].fg.name, g_tieFlightGroup.name);
			strcpy(g_missionFlightGroups[flightGroupIdx].fg.craftRole, g_tieFlightGroup.cmdr);
			strcpy(g_missionFlightGroups[flightGroupIdx].fg.cargo, g_tieFlightGroup.contents[0]);
			strcpy(g_missionFlightGroups[flightGroupIdx].fg.specialCargo, g_tieFlightGroup.contents[1]);
			g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft = g_tieFlightGroup.special_craft;
			g_missionFlightGroups[flightGroupIdx].fg.randomSpecialCargoCraft = g_tieFlightGroup.special_flag;
			g_missionFlightGroups[flightGroupIdx].fg.craftType = g_tieFlightGroup.species;
			g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft = g_tieFlightGroup.count;
			g_missionFlightGroups[flightGroupIdx].fg.status1 = g_tieFlightGroup.version;
			g_missionFlightGroups[flightGroupIdx].fg.warhead = g_tieFlightGroup.warhead;
			g_missionFlightGroups[flightGroupIdx].fg.beam = g_tieFlightGroup.beam;
			g_missionFlightGroups[flightGroupIdx].fg.iff = g_tieFlightGroup.side;
			g_missionFlightGroups[flightGroupIdx].fg.team = 0;
			g_missionFlightGroups[flightGroupIdx].fg.groupAI = g_tieFlightGroup.skill;
			g_missionFlightGroups[flightGroupIdx].fg.markings = g_tieFlightGroup.camoflage;
			g_missionFlightGroups[flightGroupIdx].fg.radio = g_tieFlightGroup.camo_flag;
			g_missionFlightGroups[flightGroupIdx].fg.reserved5C = g_tieFlightGroup.camo_unused;
			g_missionFlightGroups[flightGroupIdx].fg.formation = g_tieFlightGroup.formation;
			g_missionFlightGroups[flightGroupIdx].fg.formationSpacing = g_tieFlightGroup.form_spacing;
			g_missionFlightGroups[flightGroupIdx].fg.globalGroup = g_tieFlightGroup.set;
			g_missionFlightGroups[flightGroupIdx].fg.reserved60 = g_tieFlightGroup.set_unused;
			g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves = g_tieFlightGroup.waves;
			g_missionFlightGroups[flightGroupIdx].fg.wavesDelay = g_tieFlightGroup.wave_delay;
			g_missionFlightGroups[flightGroupIdx].fg.stopArrivingWhen = 0;
			if (g_tieFlightGroup.player_flag != 0) {
				g_missionFlightGroups[flightGroupIdx].fg.playerNumber = 1;
				g_missionFlightGroups[flightGroupIdx].fg.playerCraft = g_tieFlightGroup.player_flag - 1;
			} else {
				g_missionFlightGroups[flightGroupIdx].fg.playerNumber = 0;
			}
			g_missionFlightGroups[flightGroupIdx].fg.arriveOnlyIfHuman = 0;
			g_missionFlightGroups[flightGroupIdx].fg.yaw = g_tieFlightGroup.heading;
			g_missionFlightGroups[flightGroupIdx].fg.pitch = g_tieFlightGroup.pitch;
			g_missionFlightGroups[flightGroupIdx].fg.roll = g_tieFlightGroup.rotation;
			g_missionFlightGroups[flightGroupIdx].fg.legacyPermaDeathEnabled = g_tieFlightGroup.link_flag;
			g_missionFlightGroups[flightGroupIdx].fg.legacyPermaDeathId = g_tieFlightGroup.link_code;
			g_missionFlightGroups[flightGroupIdx].fg.reservedPermaDeath = g_tieFlightGroup.link_unused;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalDifficulty = g_tieFlightGroup.difficulty;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[0].condition =
				g_tieFlightGroup.start_cond[0].cond;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[1].condition =
				g_tieFlightGroup.start_cond[1].cond;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[0].variableType =
				g_tieFlightGroup.start_cond[0].type;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[1].variableType =
				g_tieFlightGroup.start_cond[1].type;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[0].amount =
				g_tieFlightGroup.start_cond[0].pct;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[1].amount =
				g_tieFlightGroup.start_cond[1].pct;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[0].variable =
				g_tieFlightGroup.start_cond[0].id;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].triggers[1].variable =
				g_tieFlightGroup.start_cond[1].id;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalTriggers[0].trigger1OrTrigger2 =
				g_tieFlightGroup.start_op;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalRandDelayMinutes = 0;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalRandDelaySeconds = 0;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalDelayMinutes = g_tieFlightGroup.start_delay_min;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalDelaySeconds = g_tieFlightGroup.start_delay_sec;
			g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[0].condition =
				g_tieFlightGroup.stop_cond.cond;
			g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[0].variableType =
				g_tieFlightGroup.stop_cond.type;
			g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[0].amount =
				g_tieFlightGroup.stop_cond.pct;
			g_missionFlightGroups[flightGroupIdx].fg.departureTrigger.triggers[0].variable =
				g_tieFlightGroup.stop_cond.id;
			g_missionFlightGroups[flightGroupIdx].fg.departureDelayMinutes = g_tieFlightGroup.stop_min;
			g_missionFlightGroups[flightGroupIdx].fg.departureDelaySeconds = g_tieFlightGroup.stop_sec;
			g_missionFlightGroups[flightGroupIdx].fg.abortTrigger = g_tieFlightGroup.stop_abort;
			memcpy((uint8_t*)&g_missionFlightGroups[flightGroupIdx].fg.editorMothership + 1,
				   &g_tieFlightGroup.cur_start_fg, sizeof(g_tieFlightGroup.cur_start_fg));
			g_missionFlightGroups[flightGroupIdx].fg.arrivalMothership = g_tieFlightGroup.start_fg;
			g_missionFlightGroups[flightGroupIdx].fg.arrivalMethod = g_tieFlightGroup.start_fg_used;
			g_missionFlightGroups[flightGroupIdx].fg.departureMothership = g_tieFlightGroup.pri_stop_fg;
			g_missionFlightGroups[flightGroupIdx].fg.departureMethod = g_tieFlightGroup.pri_stop_fg_used;
			g_missionFlightGroups[flightGroupIdx].fg.alternateMothership = g_tieFlightGroup.sec_stop_fg;
			g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed =
				g_tieFlightGroup.sec_stop_fg_used;
			g_missionFlightGroups[flightGroupIdx].fg.capturedDepartureMothership =
				g_tieFlightGroup.capture_fg;
			g_missionFlightGroups[flightGroupIdx].fg.capturedDepartViaMothership =
				g_tieFlightGroup.capture_fg_used;

			memcpy(&g_missionFlightGroups[flightGroupIdx].fg.orders[0], &g_tieFlightGroup.ai[0],
				   sizeof(g_tieFlightGroup.ai[0]));
			memcpy(&g_missionFlightGroups[flightGroupIdx].fg.orders[1], &g_tieFlightGroup.ai[1],
				   sizeof(g_tieFlightGroup.ai[1]));
			memcpy(&g_missionFlightGroups[flightGroupIdx].fg.orders[2], &g_tieFlightGroup.ai[2],
				   sizeof(g_tieFlightGroup.ai[2]));
			g_missionFlightGroups[flightGroupIdx].fg.orders[0].target1Type = g_tieFlightGroup.ai[0].pri_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[0].target2Type = g_tieFlightGroup.ai[0].sec_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[1].target1Type = g_tieFlightGroup.ai[1].pri_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[1].target2Type = g_tieFlightGroup.ai[1].sec_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[2].target1Type = g_tieFlightGroup.ai[2].pri_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[2].target2Type = g_tieFlightGroup.ai[2].sec_type;
			g_missionFlightGroups[flightGroupIdx].fg.orders[0].order = g_tieFlightGroup.ai[0].order;
			g_missionFlightGroups[flightGroupIdx].fg.orders[1].order = g_tieFlightGroup.ai[1].order;
			g_missionFlightGroups[flightGroupIdx].fg.orders[2].order = g_tieFlightGroup.ai[1].order;
			g_missionFlightGroups[flightGroupIdx].fg.orders[0].target1 = g_tieFlightGroup.ai[0].pri_id;
			g_missionFlightGroups[flightGroupIdx].fg.orders[0].target2 = g_tieFlightGroup.ai[0].sec_id;
			g_missionFlightGroups[flightGroupIdx].fg.orders[1].target1 = g_tieFlightGroup.ai[1].pri_id;
			g_missionFlightGroups[flightGroupIdx].fg.orders[1].target2 = g_tieFlightGroup.ai[1].sec_id;
			g_missionFlightGroups[flightGroupIdx].fg.orders[2].target1 = g_tieFlightGroup.ai[2].pri_id;
			g_missionFlightGroups[flightGroupIdx].fg.orders[2].target2 = g_tieFlightGroup.ai[2].sec_id;

			g_missionFlightGroups[flightGroupIdx].fg.goals[0].type = 0;
			g_missionFlightGroups[flightGroupIdx].fg.goals[0].eventCondition = g_tieFlightGroup.pri_win_cond;
			g_missionFlightGroups[flightGroupIdx].fg.goals[0].amount = g_tieFlightGroup.pri_win_pct;
			g_missionFlightGroups[flightGroupIdx].fg.goals[0].enabledTeams[0] = 1;
			g_missionFlightGroups[flightGroupIdx].fg.goals[1].type = 2;
			g_missionFlightGroups[flightGroupIdx].fg.goals[1].eventCondition = g_tieFlightGroup.sec_win_cond;
			g_missionFlightGroups[flightGroupIdx].fg.goals[1].amount = g_tieFlightGroup.sec_win_pct;
			g_missionFlightGroups[flightGroupIdx].fg.goals[1].enabledTeams[0] = 1;
			g_missionFlightGroups[flightGroupIdx].fg.goals[2].type = 1;
			g_missionFlightGroups[flightGroupIdx].fg.goals[2].eventCondition = g_tieFlightGroup.loss_cond;
			g_missionFlightGroups[flightGroupIdx].fg.goals[2].amount = g_tieFlightGroup.loss_pct;
			g_missionFlightGroups[flightGroupIdx].fg.goals[2].enabledTeams[0] = 1;
			g_missionFlightGroups[flightGroupIdx].fg.goals[3].type = 2;
			g_missionFlightGroups[flightGroupIdx].fg.goals[3].eventCondition = g_tieFlightGroup.bonus_cond;
			g_missionFlightGroups[flightGroupIdx].fg.goals[3].amount = g_tieFlightGroup.bonus_pct;
			g_missionFlightGroups[flightGroupIdx].fg.goals[3].points = g_tieFlightGroup.bonus_points;
			g_missionFlightGroups[flightGroupIdx].fg.goals[3].enabledTeams[0] = 1;

			for (index = 0; index < LEGACY_WAYPOINT_COUNT; ++index) {
				g_missionFlightGroups[flightGroupIdx].fg.missionPointX[index] = g_tieFlightGroup.way_x[index];
				g_missionFlightGroups[flightGroupIdx].fg.missionPointY[index] = g_tieFlightGroup.way_y[index];
				g_missionFlightGroups[flightGroupIdx].fg.missionPointZ[index] = g_tieFlightGroup.way_z[index];
				g_missionFlightGroups[flightGroupIdx].fg.missionPointEnabled[index] =
					g_tieFlightGroup.way_used[index];
			}
			g_missionFlightGroups[flightGroupIdx].fg.reservedOptionsPrefix[0] = g_tieFlightGroup.way_shown;
			g_missionFlightGroups[flightGroupIdx].fg.reservedOptionsPrefix[1] = g_tieFlightGroup.way_unused;
			g_missionFlightGroups[flightGroupIdx].fg.reservedOptionsPrefix[2] =
				g_tieFlightGroup.way_brief_link;
			g_missionFlightGroups[flightGroupIdx].fg.reservedOptions[0] = g_tieFlightGroup.way_brief_shown;

			if (g_missionFlightGroups[flightGroupIdx].fg.iff == 0 ||
				g_missionFlightGroups[flightGroupIdx].fg.iff == 4) {
				g_missionFlightGroups[flightGroupIdx].fg.team = 1;
			} else if (g_missionFlightGroups[flightGroupIdx].fg.iff == 2 ||
					   g_missionFlightGroups[flightGroupIdx].fg.iff == 3 ||
					   g_missionFlightGroups[flightGroupIdx].fg.iff == 5) {
				if (g_tieMissionHeader.neutral_name[g_missionFlightGroups[flightGroupIdx].fg.iff -
													LEGACY_IFF_NAME_FIRST][0] == '1') {
					g_missionFlightGroups[flightGroupIdx].fg.team = 1;
				} else {
					g_missionFlightGroups[flightGroupIdx].fg.team = 0;
				}
			} else {
				g_missionFlightGroups[flightGroupIdx].fg.team = 0;
			}
			g_missionFlightGroups[flightGroupIdx].playerOwnerIdx = -1;
		}

		for (messageIdx = 0; messageIdx < messageCount; ++messageIdx) {
			FeDiskIo_ReadWithRetryPrompt(&g_tieRadioMessage, sizeof(g_tieRadioMessage), 1, stream);
			strncpy(g_missionMessages[messageIdx].message, g_tieRadioMessage.message,
					sizeof(g_missionMessages[messageIdx].message));
			g_missionMessages[messageIdx].triggerPairs[0].triggers[0].condition =
				g_tieRadioMessage.conditions[0].cond;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[1].condition =
				g_tieRadioMessage.conditions[1].cond;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[0].variableType =
				g_tieRadioMessage.conditions[0].type;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[1].variableType =
				g_tieRadioMessage.conditions[1].type;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[0].amount =
				g_tieRadioMessage.conditions[0].pct;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[1].amount =
				g_tieRadioMessage.conditions[1].pct;
			g_missionMessages[messageIdx].triggerPairs[0].trigger1OrTrigger2 =
				g_tieRadioMessage.condition1OrCondition2;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[0].variable =
				g_tieRadioMessage.conditions[0].id;
			g_missionMessages[messageIdx].triggerPairs[0].triggers[1].variable =
				g_tieRadioMessage.conditions[1].id;
			strncpy(g_missionMessages[messageIdx].voice, g_tieRadioMessage.voice,
					sizeof(g_missionMessages[messageIdx].voice));
			g_missionMessages[messageIdx].rawDelay = g_tieRadioMessage.rawDelay;
			g_missionMessages[messageIdx].sentToTeam[0] = 1;
		}

		{
			int16_t legacyGoalIdx;
			for (legacyGoalIdx = 0; legacyGoalIdx < goalCount; ++legacyGoalIdx) {
				FeDiskIo_ReadWithRetryPrompt(&g_tieMissionGoal, sizeof(g_tieMissionGoal), 1, stream);
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[0].condition =
					g_tieMissionGoal.subcond[0].cond;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[1].condition =
					g_tieMissionGoal.subcond[1].cond;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[0].variableType =
					g_tieMissionGoal.subcond[0].type;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[1].variableType =
					g_tieMissionGoal.subcond[1].type;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[0].amount =
					g_tieMissionGoal.subcond[0].pct;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[1].amount =
					g_tieMissionGoal.subcond[1].pct;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].trigger1OrTrigger2 =
					g_tieMissionGoal.or_joined;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[0].variable =
					g_tieMissionGoal.subcond[0].id;
				g_missionGlobalGoals[0][legacyGoalIdx].triggerPairs[0].triggers[1].variable =
					g_tieMissionGoal.subcond[1].id;
				strncpy(g_missionGlobalGoals[0][legacyGoalIdx].name, (char*)g_tieMissionGoal.editor_name,
						sizeof(g_missionGlobalGoals[0][legacyGoalIdx].name));
				g_missionGlobalGoals[0][legacyGoalIdx].version =
					g_tieMissionGoal.editor_name[sizeof(g_missionGlobalGoals[0][legacyGoalIdx].name)];
				g_missionGlobalGoals[0][legacyGoalIdx].rawDelay = g_tieMissionGoal._pad[0];
				g_missionGlobalGoals[0][legacyGoalIdx].rawPoints = 0;
			}
		}

		File_RawSeek(stream, PILOT_RECORD_SKIP_SIZE, SEEK_CUR);
		{
			int16_t firstStringCount;
			int16_t secondStringCount;
			for (firstStringCount = PILOT_STRING_COUNT; firstStringCount != 0; --firstStringCount) {
				FeDiskIo_ReadWithRetryPrompt(&stringLength, sizeof(stringLength), 1, stream);
				if (stringLength != 0)
					File_RawSeek(stream, stringLength, SEEK_CUR);
			}
			for (secondStringCount = PILOT_STRING_COUNT; secondStringCount != 0; --secondStringCount) {
				FeDiskIo_ReadWithRetryPrompt(&stringLength, sizeof(stringLength), 1, stream);
				if (stringLength != 0)
					File_RawSeek(stream, stringLength, SEEK_CUR);
			}
		}
	}

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		index = 0;
		if (g_missionFlightGroups[flightGroupIdx].fg.craftRole[0] != '\0') {
			do {
				char roleCharacter;
				if (index >= CRAFT_ROLE_SIZE)
					break;
				roleCharacter = g_missionFlightGroups[flightGroupIdx].fg.craftRole[index];
				if (roleCharacter >= 'a' && roleCharacter <= 'z') {
					g_missionFlightGroups[flightGroupIdx].fg.craftRole[index] = roleCharacter - ('a' - 'A');
				}
				++index;
			} while (g_missionFlightGroups[flightGroupIdx].fg.craftRole[index] != '\0');
		}
	}
	for (outerIdx = 0; outerIdx < TEAM_COUNT; ++outerIdx)
		g_missionTeams[outerIdx].allies[outerIdx] = 1;

	for (outerIdx = 0; outerIdx < TEAM_COUNT; ++outerIdx) {
		for (index = 0; index < ACTIVE_GLOBAL_GOAL_COUNT; ++index) {
			GlobalGoal* goal = &g_missionGlobalGoals[outerIdx][index];
			uint8_t condition1 = goal->triggerPairs[0].triggers[0].condition;
			uint8_t condition2 = goal->triggerPairs[0].triggers[1].condition;
			uint8_t condition3 = goal->triggerPairs[1].triggers[0].condition;
			uint8_t condition4 = goal->triggerPairs[1].triggers[1].condition;

			if (condition1 == MISSION_COND_NEVER_FALSE) {
				if (condition2 == MISSION_COND_NEVER_FALSE && condition3 == MISSION_COND_NEVER_FALSE &&
					condition4 == MISSION_COND_NEVER_FALSE) {
					continue;
				}
			} else {
				if (condition2 == MISSION_COND_NEVER_FALSE && condition3 == MISSION_COND_NEVER_FALSE &&
					condition4 == MISSION_COND_NEVER_FALSE) {
					goal->triggerPairs[0].triggers[1].condition = MISSION_COND_ALWAYS_TRUE;
					goal->triggerPairs[0].trigger1OrTrigger2 = 0;
					goal->triggerPairs[1].triggers[0].condition = MISSION_COND_ALWAYS_TRUE;
					goal->triggerPairs[1].triggers[1].condition = MISSION_COND_ALWAYS_TRUE;
					goal->triggerPairs[1].trigger1OrTrigger2 = 0;
					goal->triggerPair1OrTriggerPair2 = 0;
					continue;
				}
				if (condition2 != MISSION_COND_NEVER_FALSE && condition3 == MISSION_COND_NEVER_FALSE &&
					condition4 == MISSION_COND_NEVER_FALSE) {
					goal->triggerPairs[1].triggers[0].condition = MISSION_COND_ALWAYS_TRUE;
					goal->triggerPairs[1].triggers[1].condition = MISSION_COND_ALWAYS_TRUE;
					goal->triggerPairs[1].trigger1OrTrigger2 = 0;
					goal->triggerPair1OrTriggerPair2 = 0;
					continue;
				}
			}
			if (condition1 != MISSION_COND_NEVER_FALSE && condition2 != MISSION_COND_NEVER_FALSE &&
				condition3 != MISSION_COND_NEVER_FALSE && condition4 == MISSION_COND_NEVER_FALSE) {
				goal->triggerPairs[1].triggers[1].condition = MISSION_COND_ALWAYS_TRUE;
				goal->triggerPairs[1].trigger1OrTrigger2 = 0;
			}
		}
	}

	g_stream = stream;
	return FeDiskIo_CloseGlobalStream(0) == 0;
}

// FUNCTION: XVT 0x45C250
int Mission_SyncPilotNetworkPlayersToSessionSlots(void) {
	int sessionPlayerCount;
	int sessionPlayerIndex;
	int pilotPlayerIndex;
	SessionPlayerInfo* sessionPlayers;

	sessionPlayers = NetSession_GetPlayerRoster(&sessionPlayerCount);
	for (sessionPlayerIndex = 0; sessionPlayerIndex < sessionPlayerCount; ++sessionPlayerIndex) {
		for (pilotPlayerIndex = 0; pilotPlayerIndex < 8; ++pilotPlayerIndex) {
			if (g_pilotData.networkPlayers[pilotPlayerIndex].directPlayId != 0 &&
				g_pilotData.networkPlayers[pilotPlayerIndex].directPlayId ==
					sessionPlayers[sessionPlayerIndex].directPlayId) {
				break;
			}
		}

		if (pilotPlayerIndex < 8) {
			g_players[NetSession_FindPlayerSlotByDpid(sessionPlayers[sessionPlayerIndex].directPlayId)]
				.network.directPlayId = sessionPlayers[sessionPlayerIndex].directPlayId;
		}
	}

	return 1;
}

// FUNCTION: XVT 0x45C2D0
void Mission_FreeOverrideStringHandles(void) {
	int flightGroupIdx;
	int groupIdx;
	int slotIdx;
	int teamIdx;
	int goalIdx;
	int conditionIdx;
	int textIdx;
	unsigned int handle;
	uint16_t goalHandle;

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		for (groupIdx = 0; groupIdx < 8; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 3; ++slotIdx) {
				handle = g_missionFgOverrideStringHandles[flightGroupIdx][groupIdx][slotIdx];
				if (handle != 0)
					Memory_FreeHandle(handle);
			}
		}
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
			for (conditionIdx = 0; conditionIdx < 4; ++conditionIdx) {
				for (textIdx = 0; textIdx < 3; ++textIdx) {
					goalHandle = g_globalGoalOverrideStringHandles[teamIdx][goalIdx][conditionIdx][textIdx];
					if (goalHandle != 0) {
						handle = goalHandle;
						Memory_FreeHandle((uint16_t)handle);
					}
				}
			}
		}
	}
}
