#include "xvt/flight/ai/paiorder.h"
#include "xvt/assets/opt_model.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/ai/pai.h"
#include "xvt/flight/ai/pai_targetability.h"
#include "xvt/flight/ai/paifight.h"
#include "xvt/flight/ai/paiman.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/hud/msg.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/flight/object/collide.h"
#include "xvt/flight/object/laser.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/math/math2.h"
#include "xvt/math/trig2.h"
#include "xvt/util/game_rand.h"
#include <string.h>

// GLOBAL: XVT 0x5243A8
int g_aiStillAttackLastAttackerRangeBySkill[4] = { 0x8000, 0xC000, 0xE000, 0 };

// GLOBAL: XVT 0x5243D8
uint8_t g_aiThreatBearingClassByOctant[8] = { 0, 1, 1, 2, 2, 1, 1, 0 };
// GLOBAL: XVT 0x5243B8
int g_aiAttackerSearchRangeBySkill[4] = { 0x2000, 0x3000, 0x4000, 0 };
// GLOBAL: XVT 0x5243C8
int g_aiWarheadThreatRangeBySkill[4] = { 0x800, 0x1000, 0x1800, 0 };
// GLOBAL: XVT 0x5243E0
uint8_t g_aiUnderAttackFrontManeuverChoices[4] = { AI_MANEUVER_MODE_ZOOM, AI_MANEUVER_MODE_DIVE,
												   AI_MANEUVER_MODE_SPLITS_DIVE, AI_MANEUVER_MODE_IMMELMANN };
// GLOBAL: XVT 0x5243E8
uint8_t g_aiUnderAttackSideRearManeuverChoices[8] = {
	AI_MANEUVER_MODE_TURN_INSIDE, AI_MANEUVER_MODE_SPLITS_DIVE,    AI_MANEUVER_MODE_TURN_INSIDE,
	AI_MANEUVER_MODE_TURN_INSIDE, AI_MANEUVER_MODE_AVOID_ATTACKER, AI_MANEUVER_MODE_AVOID_ATTACKER,
	AI_MANEUVER_MODE_SCISSORS,    AI_MANEUVER_MODE_AVOID_ATTACKER,
};

// GLOBAL: XVT 0x5243F0
PaiOrderFunc g_orderTable[48] = {
	paiorder_nullhandler,
	paiorder_updatecourseorder,
	paiorder_underattackorder,
	paiorder_stillattackorder,
	paiorder_flyhomeorder,
	paifight_fightershootorder,
	paifight_gunnerselfdefenseorder,
	paifight_gunneroffenseorder,
	paifight_missiledefenseorder,
	paifight_scanfortargetorder,
	paiorder_waitrunorder,
	paiorder_breakofforder,
	paiorder_leaderdeadorder,
	paifight_coverleaderorder,
	paifight_followleadatkorder,
	paiorder_abortmissionorder,
	paiorder_ontailorder,
	paiorder_alwaysorder,
	paifight_checkescortorder,
	paiorder_leadergohomeorder,
	paiorder_hyperspaceorder,
	paiorder_enterhangarorder,
	paiorder_mothershiporder,
	paifight_escorttargetorder,
	paiorder_lookforcrafttoboardorder,
	paiorder_abortboardorder,
	paiorder_returnboardorder,
	paiorder_awaitboardorder,
	paiorder_makedisabledorder,
	paiorder_returnboardorder_2,
	paiorder_rocketsonboardorder,
	paiorder_avoidhitorder,
	paiorder_waitforallreturnorder,
	paiorder_waitforallcreateorder,
	paiorder_evasiveorder,
	paiorder_targetfromplayerorder,
	paiorder_avoidstarshiporder,
	paiorder_checkhyperorder,
	paiorder_stopgohomeorder,
	paiorder_completegohomeorder,
	paiorder_completegootherorder,
	paiorder_completefolloworder,
	paiorder_waitgootherorder,
	paiorder_orderswitchorder,
	paiorder_killselforder,
	paiorder_dropoffdestorder,
	paiorder_abortmotherwaitorder,
	paiorder_checkconditionalorder,
};

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x4661E0
int16_t paiorder_checkconditionalorder(void) { return 0; }

// FUNCTION: XVT 0x4661F0
int16_t paiorder_nullhandler(void) { return 0; }

// FUNCTION: XVT 0x466200
int16_t paiorder_updatecourseorder(void) {
	g_aiCourseOrderManeuverMode = g_aiCourseOrderManeuverTable[g_paiContext.controller->maneuverMode];
	return g_aiCourseOrderManeuverMode();
}

// FUNCTION: XVT 0x466220
int16_t paiorder_underattackorder(void) {
	unsigned int toRef;
	uint16_t objectIdx;
	int maxRangeScore;

	toRef = g_paiContext.objectIndex;

	if (g_objectTable[toRef].genusId != CRAFT_GENUS_STARFIGHTER &&
		g_objectTable[toRef].genusId != CRAFT_GENUS_TRANSPORT)
		return 0;
	if (g_paiContext.controller->maneuverMode == g_paiContext.initialManeuverId) {
		if (g_curCraft->lastAttackerObjIdx == UINT16_MAX) {
			maxRangeScore = g_aiWarheadThreatRangeBySkill[g_paiContext.skillTier];
			for (objectIdx = (uint16_t)g_projectileObjectSlotStart; objectIdx < g_projectileObjectSlotEnd;
				 ++objectIdx) {
				uint8_t objectType = g_objectTable[objectIdx].objectType;
				if (objectType != 0 && g_objectTable[objectIdx].mobj->pWarheadGuidance->homingTier != 0 &&
					g_objectTable[objectIdx].mobj->pWarheadGuidance->targetObjIdx ==
						g_paiContext.objectIndex) {
					int threatRange = maxRangeScore * 3;
					if (objectType != WARHEAD_OBJECT_TYPE_CONCUSSION_MISSILE)
						threatRange = maxRangeScore;
					if (pai_IsObjectWithinCurrentPointRange(objectIdx, (unsigned int)threatRange) == 1) {
						g_curCraft->lastAttackerObjIdx = objectIdx;
						pai_ObjectRefDirectionToObjectRef(g_paiContext.objectIndex, objectIdx);
						if (g_aiThreatBearingClassByOctant
								[(uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.objectIndex].yaw) >>
								 13] == 0)
							g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_TURN_AWAY;
						else
							g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_TURN_INSIDE;
						paiman_initmaneuver();
						if (g_curCraft->cmTypeId != COUNTERMEASURE_TYPE_NONE &&
							g_curCraft->cmAmmoCount != 0) {
							if (g_curCraft->cmTypeId == COUNTERMEASURE_TYPE_CHAFF &&
								(g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_COUNTERMEASURES) != 0) {
								g_curCraft->chaffActiveTimer += 10;
								if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.status1 !=
										21 &&
									g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.status2 !=
										21)
									--g_curCraft->cmAmmoCount;
							} else if (g_curCraft->cmFireCooldownTimer == 0) {
								laser_createcountermeasureprojectile(g_paiContext.objectIndex,
																	 COUNTERMEASURE_PROJECTILE_OBJECT_TYPE);
							}
						}
						return 0;
					}
				}
			}

			maxRangeScore = g_aiAttackerSearchRangeBySkill[g_paiContext.skillTier];
			if (g_curCraft->lastAttackerObjIdx == UINT16_MAX) {
				for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
					 objectIdx < g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
					if (g_objectTable[objectIdx].playerOwnerIdx == -1 &&
						g_objectTable[objectIdx].objectType != 0) {
						int objectTeam =
							g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.team;
						int sourceTeam = g_objectTable[g_paiContext.objectIndex].mobj->team;
						int isEnemy =
							objectTeam == sourceTeam ? 0 : g_missionTeams[sourceTeam].allies[objectTeam] == 0;
						if (isEnemy && g_curCraft->objectKind == CRAFT_OBJECT_KIND_ACTIVE &&
							g_objectTable[objectIdx].genusId == CRAFT_GENUS_STARFIGHTER &&
							pai_IsObjectWithinCurrentPointRange(objectIdx, (unsigned int)maxRangeScore) ==
								1) {
							uint16_t horizontalAngle;
							uint16_t verticalAngle;
							pai_ObjectRefDirectionToObjectRef(objectIdx, toRef);
							horizontalAngle = (uint16_t)(trig2_xyangle - g_objectTable[objectIdx].yaw);
							if (horizontalAngle >= 0x8000)
								horizontalAngle = (uint16_t)-horizontalAngle;
							verticalAngle = (uint16_t)(pitchQ16 - g_objectTable[objectIdx].pitch);
							if (verticalAngle >= 0x8000)
								verticalAngle = (uint16_t)-verticalAngle;
							if (horizontalAngle < 0x2000 && verticalAngle < 0x2000) {
								g_curCraft->lastAttackerObjIdx = objectIdx;
								break;
							}
						}
					}
				}
			}
		}

		if (g_curCraft->lastAttackerObjIdx != UINT16_MAX) {
			uint8_t threatBearing;
			uint16_t attackerMaxSpeed;
			MobileObject* attacker;
			uint16_t ownMaxSpeed;
			uint16_t randomValue;
			uint8_t maneuverMode;

			pai_ObjectRefDirectionToObjectRef(toRef, g_curCraft->lastAttackerObjIdx);
			threatBearing =
				g_aiThreatBearingClassByOctant[(uint16_t)(trig2_xyangle - g_objectTable[toRef].yaw) >> 13];
			ownMaxSpeed = g_modelDefs[g_curCraft->modelIndex].maxSpeed;
			attacker = g_objectTable[g_curCraft->lastAttackerObjIdx].mobj;
			if (attacker->state == 0)
				attackerMaxSpeed = g_modelDefs[attacker->pCraft->modelIndex].maxSpeed;
			else
				attackerMaxSpeed = 900;
			randomValue = (uint16_t)GameRand();
			if (threatBearing == 1) {
				if (trig2_polardistance >= 0x2000 || randomValue >= 0x4000)
					maneuverMode = g_aiUnderAttackFrontManeuverChoices[randomValue & 3];
				else
					maneuverMode = AI_MANEUVER_MODE_TURN_INSIDE;
			} else if (threatBearing == 0) {
				maneuverMode = randomValue <= 0x8000 ? g_aiUnderAttackFrontManeuverChoices[randomValue & 3]
													 : AI_MANEUVER_MODE_HEAD_ON_ATTACK;
			} else {
				if (ownMaxSpeed < attackerMaxSpeed || trig2_polardistance <= 0x8000)
					maneuverMode = g_aiUnderAttackSideRearManeuverChoices[randomValue & 7];
				else
					maneuverMode = AI_MANEUVER_MODE_SPEED_AWAY;
			}
			g_paiContext.controller->maneuverMode = maneuverMode;
			paiman_initmaneuver();
		}
	}
	return 0;
}

// FUNCTION: XVT 0x4666E0
int16_t paiorder_stillattackorder(void) {
	uint16_t lastAttackerObjIdx;
	unsigned int attackerIndex;
	uint16_t* lastAttackerObjIdxPtr;
	ObjectRecord* attacker;
	WarheadGuidanceState* guidance;

	if (g_paiContext.controller->maneuverMode != g_paiContext.initialManeuverId) {
		lastAttackerObjIdxPtr = &g_curCraft->lastAttackerObjIdx;
		lastAttackerObjIdx = *lastAttackerObjIdxPtr;
		if (lastAttackerObjIdx != UINT16_MAX) {
			attackerIndex = lastAttackerObjIdx;
			if (g_projectileObjectSlotStart <= (int)attackerIndex &&
				g_projectileObjectSlotEnd > (int)attackerIndex) {
				attacker = &g_objectTable[attackerIndex];
				guidance = attacker->mobj->pWarheadGuidance;
				if (attacker->objectType == 0 || guidance->targetObjIdx != g_paiContext.objectIndex) {
					*lastAttackerObjIdxPtr = UINT16_MAX;
					return 1;
				}
			} else if (lastAttackerObjIdx != UINT16_MAX &&
					   !pai_IsObjectWithinCurrentPointRange(
						   attackerIndex,
						   (unsigned int)g_aiStillAttackLastAttackerRangeBySkill[g_paiContext.skillTier])) {
				g_curCraft->lastAttackerObjIdx = UINT16_MAX;
				return 1;
			}
		}
	}
	return 0;
}

// FUNCTION: XVT 0x4667B0
int16_t paiorder_flyhomeorder(void) {
	uint16_t mothershipObject;
	uint8_t mothershipFlightGroup;
	int modelIndex;

	g_curCraft->aiFlight.separation = 1;
	mothershipObject = UINT16_MAX;
	if (g_curCraft->wasCaptured != 0) {
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartViaMothership != 0) {
			mothershipFlightGroup =
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartureMothership;
			mothershipObject = pai_FindMothershipObject(mothershipFlightGroup);
		}
	} else {
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMethod != 0)
			mothershipObject = pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMothership);
		if (mothershipObject == UINT16_MAX &&
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothershipUsed != 0)
			mothershipObject = pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothership);
	}
	if (mothershipObject != UINT16_MAX &&
		g_missionFlightGroups[g_objectTable[mothershipObject].flightGroupIdx].playerOwnerIdx != -1)
		mothershipObject = UINT16_MAX;
	if (mothershipObject != UINT16_MAX) {
		g_paiContext.controller->targetObjIdx = mothershipObject;
		g_paiContext.controller->targetSignature = g_objectTable[mothershipObject].objectSignature;
		g_paiContext.controller->hasLiveTarget = 0;
		modelIndex = g_objectTable[mothershipObject].mobj->pCraft->modelIndex;
		pai_RotateLocalVectorToWorldScratch(&g_objectTable[mothershipObject],
											g_modelDefs[modelIndex].hangarPoints.outside.side,
											g_modelDefs[modelIndex].hangarPoints.outside.up,
											g_modelDefs[modelIndex].hangarPoints.outside.forward);
		g_paiContext.controller->aimPointX = g_rotatedX + g_objectTable[mothershipObject].world_x;
		g_paiContext.controller->aimPointY = g_rotatedY + g_objectTable[mothershipObject].world_y;
		g_paiContext.controller->aimPointZ = g_rotatedZ + g_objectTable[mothershipObject].world_z;
		pai_CalcAnglesToAimPoint();
		if (trig2_polardistance < 0x10000)
			paiman_setspeed(g_paiContext.objectIndex, 0x96);
		if (trig2_polardistance < 0x8000)
			paiman_setspeed(g_paiContext.objectIndex, 0x64);
		if (trig2_polardistance < 0x4000)
			paiman_setspeed(g_paiContext.objectIndex, 0x4B);
		return trig2_polardistance < 2048;
	}

	if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.missionPointEnabled[13] != 0)
		g_paiContext.controller->targetObjIdx = 0x800D;
	else
		g_paiContext.controller->targetObjIdx = 0x8000;
	g_paiContext.controller->targetSignature = 0;
	g_paiContext.controller->hasLiveTarget = 0;
	pai_UpdateAimPointFromOrderTarget();
	return 0;
}

// FUNCTION: XVT 0x46A140
int16_t paiorder_dropoffdestorder(void) {
	MissionOrder* order =
		&g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[g_paiContext.orderSlot];
	uint16_t destinationFlightGroup = (uint16_t)(order->variable2 - 1);

	if (g_missionFgStats[destinationFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED] != 0)
		return 0;

	Mission_ResolveFormationSlotWorldLoc(destinationFlightGroup, 0, UINT16_MAX);
	g_paiContext.controller->aimPointX = worldlocx;
	g_paiContext.controller->aimPointY = worldlocy;
	g_paiContext.controller->aimPointZ = worldlocz + 932;
	pai_CalcAnglesToAimPoint();
	if (trig2_polardistance < 0x4000)
		paiman_setpower(g_paiContext.objectIndex, 0xC000);
	if (trig2_polardistance < 4096)
		paiman_setpower(g_paiContext.objectIndex, 0x6000);
	if (trig2_polardistance >= 2048)
		return 0;

	g_paiContext.controller->waypointIndex = 0;
	return 1;
}

// FUNCTION: XVT 0x466A70
int16_t paiorder_enterhangarorder(void) {
	uint16_t objectIndex;
	uint16_t mothershipObject;
	uint16_t outcomeId;
	unsigned int team;
	unsigned int groupIndex;
	uint16_t carriedObjectIndex;
	uint16_t carriedGroupIndex;
	CraftData* carriedCraft;
	int specialCargo;

	g_curCraft->aiFlight.separation = 1;
	g_paiContext.controller->thinkInterval = 29;
	if (g_curCraft->wasCaptured != 0) {
		mothershipObject = pai_FindMothershipObject(
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartureMothership);
		outcomeId = FLIGHT_GROUP_OUTCOME_CAPTURED_MOTHERSHIP_DEPENDENT;
	} else {
		mothershipObject = pai_FindMothershipObject(
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMothership);
		outcomeId = FLIGHT_GROUP_OUTCOME_PRIMARY_MOTHERSHIP_DEPENDENT;
		if (mothershipObject == UINT16_MAX &&
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothershipUsed != 0) {
			mothershipObject = pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothership);
			outcomeId = FLIGHT_GROUP_OUTCOME_ALTERNATE_MOTHERSHIP_DEPENDENT;
		}
	}
	if (mothershipObject != UINT16_MAX) {
		uint16_t modelIndex;

		g_paiContext.controller->targetObjIdx = mothershipObject;
		g_paiContext.controller->targetSignature = g_objectTable[mothershipObject].objectSignature;
		g_paiContext.controller->hasLiveTarget = 0;
		modelIndex = g_objectTable[mothershipObject].mobj->pCraft->modelIndex;
		pai_RotateLocalVectorToWorldScratch(&g_objectTable[mothershipObject],
											g_modelDefs[modelIndex].hangarPoints.inside.side,
											g_modelDefs[modelIndex].hangarPoints.inside.up,
											g_modelDefs[modelIndex].hangarPoints.inside.forward);
		g_paiContext.controller->aimPointX = g_rotatedX + g_objectTable[mothershipObject].world_x;
		g_paiContext.controller->aimPointY = g_rotatedY + g_objectTable[mothershipObject].world_y;
		g_paiContext.controller->aimPointZ = g_rotatedZ + g_objectTable[mothershipObject].world_z;
	}
	pai_CalcAnglesToAimPoint();
	if (mothershipObject != UINT16_MAX) {
		if (g_objectTable[mothershipObject].mobj->speed >= 25)
			paiman_setspeed(g_paiContext.objectIndex, g_objectTable[mothershipObject].mobj->speed + 25);
		else
			paiman_setspeed(g_paiContext.objectIndex, 40);

		if ((g_objectTable[g_paiContext.objectIndex].genusId == CRAFT_GENUS_STARSHIP ? 1024 : 512) >
			trig2_polardistance) {
			for (objectIndex = (uint16_t)g_activeRegionObjectSlotStart;
				 objectIndex < g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
				int tableIndex;
				ObjectRecord* object;
				CraftData* otherCraft;
				AiController* otherController;

				tableIndex = objectIndex;
				object = &g_objectTable[tableIndex];
				if (object->objectType == 0 || object->flightGroupIdx != g_paiContext.orderFlightGroupIndex)
					continue;
				otherCraft = object->mobj->pCraft;
				otherController = &otherCraft->aiController;
				if (otherCraft->leader_obj_idx == UINT8_MAX ||
					otherController->maneuverMode != AI_MANEUVER_MODE_FOLLOW_LEADER ||
					object->playerOwnerIdx != -1)
					continue;
				if (otherCraft->wasCaptured == 0) {
					if (otherController->orderStateFlag == 0 &&
						(otherCraft->aiFlight.goHomeFlag != 0 ||
						 (otherCraft->aiFlight.missionAbortedFlag == 0 &&
						  otherCraft->aiFlight.departTimerFlag == 0))) {
						specialCargo = 0;
						++g_missionFgStats[g_paiContext.orderFlightGroupIndex]
							  .outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED];
						if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
							otherCraft->waveNumber) {
							g_missionFgStats[g_paiContext.orderFlightGroupIndex]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED] = 1;
							specialCargo = 1;
						}
						Mission_ApplyTeamGoalScoreAllEnabledTeams(12, g_paiContext.orderFlightGroupIndex,
																  specialCargo);
					}
					if (otherCraft->wasCaptured == 0 && otherController->orderStateFlag == 1 &&
						otherCraft->aiFlight.missionAbortedFlag == 0 &&
						otherCraft->aiFlight.departTimerFlag == 0) {
						++g_missionFgStats[g_paiContext.orderFlightGroupIndex]
							  .outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE];
						if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
							otherCraft->waveNumber)
							g_missionFgStats[g_paiContext.orderFlightGroupIndex]
								.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE] = 1;
					}
				}
				msg_emitCraftMessage(objectIndex, otherCraft, 141);
				Mission_RecordCraftOutcome(objectIndex, g_paiContext.orderFlightGroupIndex, outcomeId);
				g_objectTable[tableIndex].objectType = 0;
				Craft_ClearEffectiveAiObjectLink(otherCraft);
			}

			if (g_curCraft->wasCaptured == 0 && g_paiContext.controller->orderStateFlag == 0 &&
				(g_curCraft->aiFlight.goHomeFlag != 0 || (g_curCraft->aiFlight.missionAbortedFlag == 0 &&
														  g_curCraft->aiFlight.departTimerFlag == 0))) {
				++g_missionFgStats[g_paiContext.orderFlightGroupIndex]
					  .outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED];
				specialCargo = 0;
				if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
					g_curCraft->waveNumber) {
					g_missionFgStats[g_paiContext.orderFlightGroupIndex]
						.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED] = 1;
					specialCargo = 1;
				}
				Mission_ApplyTeamGoalScoreAllEnabledTeams(12, g_paiContext.orderFlightGroupIndex,
														  specialCargo);
			}
			if (g_curCraft->wasCaptured == 0 && g_paiContext.controller->orderStateFlag == 1 &&
				g_curCraft->aiFlight.missionAbortedFlag == 0 && g_curCraft->aiFlight.departTimerFlag == 0) {
				++g_missionFgStats[g_paiContext.orderFlightGroupIndex]
					  .outcomeCount[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE];
				if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
					g_curCraft->waveNumber)
					g_missionFgStats[g_paiContext.orderFlightGroupIndex]
						.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DEPARTED_WITH_ORDER_INCOMPLETE] = 1;
			}
			if (g_curCraft->wasCaptured != 0) {
				team = g_objectTable[g_paiContext.objectIndex].mobj->team;
				++g_missionFgStats[g_paiContext.orderFlightGroupIndex].teamCondition44Count[team];
				specialCargo = 0;
				if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
					g_curCraft->waveNumber) {
					++g_missionFgStats[g_paiContext.orderFlightGroupIndex].teamCondition44SpecialCargo[team];
					specialCargo = 1;
				}
				Mission_ApplyTeamGoalScoreForTeam(44, g_paiContext.orderFlightGroupIndex, specialCargo,
												  (uint8_t)team);
				for (groupIndex = 0; groupIndex < 10; ++groupIndex) {
					if (groupIndex != team &&
						g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.team != groupIndex) {
						++g_missionFgStats[g_paiContext.orderFlightGroupIndex]
							  .teamCondition44OtherTeamCount[groupIndex];
						if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft ==
							g_curCraft->waveNumber)
							g_missionFgStats[g_paiContext.orderFlightGroupIndex]
								.teamCondition44OtherTeamSpecialCargo[groupIndex] = 1;
					}
				}
			}
			msg_emitCraftMessage(g_paiContext.objectIndex, g_curCraft, 141);
			Mission_RecordCraftOutcome(g_paiContext.objectIndex, g_paiContext.orderFlightGroupIndex,
									   outcomeId);
			g_objectTable[g_paiContext.objectIndex].objectType = 0;
			Craft_ClearEffectiveAiObjectLink(g_curCraft);

			carriedObjectIndex = g_curCraft->carriedObjectIndex;
			if (carriedObjectIndex != UINT16_MAX && g_objectTable[carriedObjectIndex].mobj != NULL) {
				carriedGroupIndex = g_objectTable[carriedObjectIndex].flightGroupIdx;
				Mission_RecordCraftOutcome(carriedObjectIndex, carriedGroupIndex, outcomeId);
				carriedCraft = g_objectTable[carriedObjectIndex].mobj->pCraft;
				if (carriedCraft->wasCaptured != 0) {
					team = g_objectTable[carriedObjectIndex].mobj->team;
					++g_missionFgStats[carriedGroupIndex].teamCondition44Count[team];
					specialCargo = 0;
					if (g_missionFlightGroups[carriedGroupIndex].fg.specialCargoCraft ==
						carriedCraft->waveNumber) {
						++g_missionFgStats[carriedGroupIndex].teamCondition44SpecialCargo[team];
						specialCargo = 1;
					}
					Mission_ApplyTeamGoalScoreForTeam(44, carriedGroupIndex, specialCargo, (uint8_t)team);
					for (groupIndex = 0; groupIndex < 10; ++groupIndex) {
						if (groupIndex != team &&
							g_missionFlightGroups[carriedGroupIndex].fg.team != groupIndex) {
							++g_missionFgStats[carriedGroupIndex].teamCondition44OtherTeamCount[groupIndex];
							if (g_missionFlightGroups[carriedGroupIndex].fg.specialCargoCraft ==
								carriedCraft->waveNumber)
								g_missionFgStats[carriedGroupIndex]
									.teamCondition44OtherTeamSpecialCargo[groupIndex] = 1;
						}
					}
					++g_missionFgStats[carriedGroupIndex]
						  .outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION];
					if (g_missionFlightGroups[carriedGroupIndex].fg.specialCargoCraft ==
						carriedCraft->waveNumber)
						g_missionFgStats[carriedGroupIndex]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_CAPTURED_BY_DESTINATION] = 1;
				} else {
					++g_missionFgStats[carriedGroupIndex]
						  .outcomeCount[FLIGHT_GROUP_OUTCOME_CAPTURED_BY_DESTINATION];
					specialCargo = 0;
					if (g_missionFlightGroups[carriedGroupIndex].fg.specialCargoCraft ==
						carriedCraft->waveNumber) {
						g_missionFgStats[carriedGroupIndex]
							.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_CAPTURED_BY_DESTINATION] = 1;
						specialCargo = 1;
					}
					Mission_ApplyTeamGoalScoreAllEnabledTeams(46, carriedGroupIndex, specialCargo);
				}
				g_objectTable[carriedObjectIndex].objectType = 0;
				Craft_ClearEffectiveAiObjectLink(carriedCraft);
			}
		}
		return 0;
	}
	paiman_setspeed(g_paiContext.objectIndex, 35);
	return 1;
}

// FUNCTION: XVT 0x467320
int16_t paiorder_waitrunorder(void) {
	uint16_t effectiveSkill;

	if (g_paiContext.controller->maneuverMode == g_paiContext.initialManeuverId) {
		effectiveSkill = pai_GetEffectiveSkillValue(g_curCraft);
		if (pai_IsObjectWithinCurrentPointRange(g_paiContext.controller->targetObjIdx,
												(unsigned int)effectiveSkill + 0x20000) == 1) {
			return 1;
		}
	}
	return 0;
}

// FUNCTION: XVT 0x467380
int16_t paiorder_breakofforder(void) {
	uint16_t targetObjIdx = g_paiContext.controller->targetObjIdx;
	int targetValid;
	uint16_t typeSpecificWord;

	if (g_curCraft->playerCommandAvoidTargetObjIdx == targetObjIdx) {
		g_paiContext.controller->targetObjIdx = UINT16_MAX;
		g_paiContext.controller->targetSignature = 0;
		g_paiContext.controller->hasLiveTarget = 0;
		g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
		return 1;
	}

	targetValid = pai_IsObjectTargetable(targetObjIdx);

	if (targetValid == 0) {
		g_paiContext.controller->targetObjIdx = UINT16_MAX;
		g_paiContext.controller->targetSignature = 0;
		g_paiContext.controller->hasLiveTarget = 0;
		g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
		return 1;
	}

	if (g_paiContext.controller->targetSignature != g_objectTable[targetObjIdx].objectSignature) {
		g_paiContext.controller->targetObjIdx = UINT16_MAX;
		g_paiContext.controller->targetSignature = 0;
		g_paiContext.controller->hasLiveTarget = 0;
		g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
		return 1;
	}

	if (strcmp(g_planTable[g_paiContext.controller->currentPlanId].name, "disableldr1pln") == 0) {
		if (g_activeRegionCraftObjectSlotEnd <= (int)targetObjIdx) {
		} else if (g_objectTable[targetObjIdx].mobj->pCraft->workingSubsystems == 0) {
			g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
			return 1;
		}
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		if (g_activeRegionCraftObjectSlotEnd <= (int)targetObjIdx) {
		} else if (Object_HasActiveDecoyBeam(targetObjIdx) == 1) {
			pai_ObjectRefUpdateApproxRangeScore(g_paiContext.objectIndex, targetObjIdx);
			if (g_targetRangeScore > 0x4000) {
				g_paiContext.controller->targetObjIdx = UINT16_MAX;
				g_paiContext.controller->targetSignature = 0;
				g_paiContext.controller->hasLiveTarget = 0;
				g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
				return 1;
			}
		}
	}

	if (g_missionFileVersion == 14) {
		typeSpecificWord = g_objectTable[targetObjIdx].typeSpecificWord;
		if (g_objectTable[targetObjIdx].mobj != NULL && g_objectTable[targetObjIdx].mobj->pCraft != NULL)
			typeSpecificWord = g_objectTable[targetObjIdx].mobj->pCraft->workingSubsystems;
		if (typeSpecificWord == 0 && g_paiContext.controller->candidateTargetIdx != targetObjIdx &&
			pai_CurrentOrderTargetsMatchObject(targetObjIdx) == 0) {
			g_paiContext.controller->targetObjIdx = UINT16_MAX;
			g_paiContext.controller->targetSignature = 0;
			g_paiContext.controller->hasLiveTarget = 0;
			g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
			return 1;
		}
	}
	return 0;
}

// FUNCTION: XVT 0x467710
int16_t paiorder_abortmissionorder(void) {
	int abortReasonMessage;
	int abortMission;
	uint16_t launcherIndex;
	uint8_t launcherCount;
	uint16_t weaponSlotIndex;
	uint16_t lastWeaponSlot;
	int16_t hasWarheads;

	if (g_curCraft->aiFlight.maxSpeedCache == 0) {
		return 0;
	}

	abortMission = 0;
	switch (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.abortTrigger) {
		case 1:
			if ((g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_SHIELDS) == 0) {
				abortMission = 1;
			}
			if ((g_objectTable[g_paiContext.objectIndex].genusId == 3 ||
				 g_objectTable[g_paiContext.objectIndex].genusId == 4) &&
				g_curCraft->shieldEnergy[0] + g_curCraft->shieldEnergy[1] == 0) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_390_SHIELDS_OUT;
			break;

		case 2:
			if ((g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_CANNONS) == 0) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_394_WARHEADS_OUT;
			break;

		case 3:
			if ((g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_WARHEAD_LAUNCHER) == 0) {
				abortMission = 1;
			}
			hasWarheads = 0;
			launcherIndex = 0;
			launcherCount = g_curCraft->warheadLauncherCount;
			while (launcherIndex < launcherCount) {
				if (g_curCraft->warheadSlotTypeIds[launcherIndex] != 0) {
					lastWeaponSlot =
						g_modelDefs[g_curCraft->modelIndex].warheadLauncherLastSlot[launcherIndex];
					weaponSlotIndex =
						g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[launcherIndex];
					while (!(lastWeaponSlot < weaponSlotIndex)) {
						if (g_curCraft->weaponSlots[weaponSlotIndex].count != 0) {
							hasWarheads = 1;
						}
						++weaponSlotIndex;
					}
				}
				++launcherIndex;
			}
			if (hasWarheads == 0) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_394_WARHEADS_OUT;
			break;

		case 4:
			if (MATH2_longfraction(g_curCraft->hullMax, 0x8000) <= g_curCraft->hullDamage) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_392_HULL_AT_50;
			break;

		case 5:
			for (launcherIndex = 0; launcherIndex < 10; ++launcherIndex) {
				if (g_curCraft->attackedByTeam[launcherIndex] != 0) {
					abortMission = 1;
				}
			}
			abortReasonMessage = IFMSG_395_UNDER_ATTACK;
			break;

		case 6:
			if ((unsigned int)(g_curCraft->shieldEnergy[0] + g_curCraft->shieldEnergy[1]) <=
				(uint16_t)MATH2_fraction((uint16_t)Craft_GetObjectMaxShield(g_paiContext.objectIndex),
										 0x8000)) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_388_SHIELDS_AT_50;
			break;

		case 7:
			if ((unsigned int)(g_curCraft->shieldEnergy[0] + g_curCraft->shieldEnergy[1]) <=
				(uint16_t)MATH2_fraction((uint16_t)Craft_GetObjectMaxShield(g_paiContext.objectIndex),
										 0x4000)) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_389_SHIELDS_AT_25;
			break;

		case 8:
			if (MATH2_longfraction(g_curCraft->hullMax, 0x4000) <= g_curCraft->hullDamage) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_391_HULL_AT_75;
			break;

		case 9:
			if (MATH2_longfraction(g_curCraft->hullMax, 0xC000) <= g_curCraft->hullDamage) {
				abortMission = 1;
			}
			abortReasonMessage = IFMSG_393_HULL_AT_25;
			break;
	}

	if (abortMission != 0) {
		if (g_curCraft->aiFlight.missionAbortedFlag == 0) {
			++g_missionFgStats[g_paiContext.orderFlightGroupIndex].outcomeCount[FLIGHT_GROUP_OUTCOME_ABORTED];
			if (g_curCraft->waveNumber ==
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft) {
				g_missionFgStats[g_paiContext.orderFlightGroupIndex]
					.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ABORTED] = 1;
			}
			if (g_curCraft->aiFlight.departTimerFlag != 0) {
				--g_missionFgStats[g_paiContext.orderFlightGroupIndex]
					  .outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
				if (g_curCraft->waveNumber ==
					g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.specialCargoCraft) {
					g_missionFgStats[g_paiContext.orderFlightGroupIndex]
						.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] = 0;
				}
				g_curCraft->aiFlight.departTimerFlag = 0;
			}

			fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_WITHDRAWING,
										   g_paiContext.objectIndex, UINT16_MAX);
			if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].playerOwnerIdx != -1) {
				g_msgSenderIff = (uint8_t)g_objectTable[g_paiContext.objectIndex].mobj->iff;
				msg_addMessagePtr(0, g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.name);
				g_msgArgTable[1] = (uint16_t)Hud_MissionFG_GetCraftNumberIfShown(
					g_paiContext.orderFlightGroupIndex, g_curCraft);
				g_msgArgTable[2] = (uint16_t)abortReasonMessage;
				msg_emitInFlightMessage(
					IFMSG_387_WINGMAN_ARG_ARG_ABORTING_MISSION_ARG,
					g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].playerOwnerIdx);
			}

			if (strcmp(g_planTable[g_builtinPlanIdByNameIndex
									   [g_orderLeaderBuiltinPlanNameIndex
											[g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
												 .fg.orders[g_paiContext.orderSlot]
												 .order]]]
						   .name,
					   "waitforboardpln") == 0 &&
				g_curCraft->subsystemDamage == 0) {
				g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			}
		}
		g_curCraft->aiFlight.missionAbortedFlag = 1;
	}

	return (int16_t)abortMission;
}

// FUNCTION: XVT 0x467C50
int16_t paiorder_leaderdeadorder(void) {
	uint8_t leaderObjectIndex;
	ObjectRecord* leaderObject;
	CraftData* leaderCraft;
	AiController* leaderController;
	uint8_t leaderInvalid;
	uint16_t objectIndex;
	ObjectRecord* object;
	CraftData* craft;
	AiController* controller;

	leaderObjectIndex = g_curCraft->leader_obj_idx;
	if (leaderObjectIndex == UINT8_MAX) {
		return 0;
	}
	if (g_activeRegionCraftObjectSlotEnd <= (int)leaderObjectIndex) {
		return 0;
	}
	leaderObject = &g_objectTable[leaderObjectIndex];
	leaderCraft = leaderObject->mobj->pCraft;
	leaderController = &leaderCraft->aiController;
	leaderInvalid = 0;
	if (leaderObject->objectType == 0) {
		leaderInvalid = 1;
	}
	if (g_objectTable[g_paiContext.objectIndex].flightGroupIdx != leaderObject->flightGroupIdx) {
		leaderInvalid = 1;
	}
	if (leaderCraft->objectKind == CRAFT_OBJECT_KIND_BREAKING_UP ||
		leaderCraft->objectKind == CRAFT_OBJECT_KIND_EXPLODING) {
		leaderInvalid = 1;
	}
	if (leaderCraft->aiFlight.missionAbortedFlag != 0) {
		leaderInvalid = 1;
	}
	if (leaderObject->playerOwnerIdx != -1) {
		leaderInvalid = 1;
	}
	if (leaderInvalid != 0) {
		for (objectIndex = (uint16_t)g_activeRegionObjectSlotStart;
			 objectIndex < (int)g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
			object = &g_objectTable[objectIndex];
			craft = object->mobj->pCraft;
			controller = &craft->aiController;
			if (object->objectType != 0 && object->flightGroupIdx == g_paiContext.orderFlightGroupIndex) {
				if (g_paiContext.objectIndex == objectIndex) {
					craft->leader_obj_idx = UINT8_MAX;
					craft->aiFlight.separation = leaderCraft->aiFlight.separation;
					controller->waypointIndex = leaderController->waypointIndex;
					controller->targetObjIdx = leaderController->targetObjIdx;
					controller->targetSignature = leaderController->targetSignature;
					controller->hasLiveTarget = leaderController->hasLiveTarget;
#ifdef XVT_MODERN
					/* A leader without a target has no aim point to resolve. */
					if (controller->targetObjIdx != UINT16_MAX)
#endif
						pai_UpdateAimPointFromOrderTarget();
				} else {
					craft->leader_obj_idx = (uint8_t)g_paiContext.objectIndex;
				}
			}
		}
	} else if (strcmp(g_planTable[g_paiContext.targetCraft->aiController.pendingPlanId].name,
					  "enterhangarpln") == 0) {
		g_paiContext.controller->thinkInterval = 59;
	}
	return leaderInvalid;
}

// FUNCTION: XVT 0x467E20
int16_t paiorder_ontailorder(void) {
	uint16_t objectIndex;
	int16_t randomManeuver;
	uint8_t maneuverMode;

	objectIndex = g_paiContext.objectIndex;
	if (g_paiContext.controller->maneuverMode == g_paiContext.initialManeuverId &&
		g_curCraft->lastAttackerObjIdx != UINT16_MAX) {
		pai_ObjectRefDirectionToObjectRef(objectIndex, g_curCraft->lastAttackerObjIdx);
		if (g_aiThreatBearingClassByOctant[(uint16_t)(trig2_xyangle - g_objectTable[objectIndex].yaw) >>
										   13] == 2) {
			randomManeuver = GameRand() & 3;
			if (randomManeuver == 0) {
				maneuverMode = AI_MANEUVER_MODE_TURN_INSIDE;
			} else if (randomManeuver == 1) {
				maneuverMode = AI_MANEUVER_MODE_ZOOM;
			} else if (randomManeuver == 2) {
				maneuverMode = AI_MANEUVER_MODE_SCISSORS;
			} else {
				maneuverMode = AI_MANEUVER_MODE_DIVE;
			}
			g_paiContext.controller->maneuverMode = maneuverMode;
			paiman_initmaneuver();
		}
	}
	return 0;
}

// FUNCTION: XVT 0x467EE0
int16_t paiorder_alwaysorder(void) { return 1; }

// FUNCTION: XVT 0x467EF0
int16_t paiorder_leadergohomeorder(void) {
	AiController* controller = &g_paiContext.targetCraft->aiController;

	return strcmp(g_planTable[controller->pendingPlanId].name, "flyhomepln") == 0 ||
		   strcmp(g_planTable[controller->pendingPlanId].name, "flyhomeevadepln") == 0;
}

// FUNCTION: XVT 0x467F60
int16_t paiorder_hyperspaceorder(void) {
	int16_t canEnterHyperspace;

	if (g_curCraft->leader_obj_idx != UINT8_MAX && g_curCraft->aiFlight.missionAbortedFlag == 0 &&
		strcmp(g_planTable[g_paiContext.controller->pendingPlanId].name, "flyhomeevadepln") != 0) {
		canEnterHyperspace = 0;
		if (g_modelDefs[g_curCraft->modelIndex].hasHyperdrive != 0) {
			if (g_curCraft->wasCaptured == 0) {
				if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMethod == 0) {
					canEnterHyperspace = 1;
				}
			} else if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
						   .fg.capturedDepartViaMothership == 0) {
				canEnterHyperspace = 1;
			}
		}
		if (canEnterHyperspace != 0 &&
			g_paiContext.targetCraft->objectKind == CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE) {
			g_paiContext.controller->pendingPlanId = (uint8_t)pai_findplanbyname("intohyperspacepln");
			g_curCraft->objectKind = CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE;
			g_curCraft->aiFlight.enterFlag = 0;
			g_curCraft->aiFlight.headingState = 0;
			g_curCraft->aiFlight.turnState = 0;
			g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_INTO_HYPERSPACE;
			g_curCraft->pushAccumX = 0;
			g_curCraft->pushAccumY = g_curCraft->pushAccumX;
			g_curCraft->pushAccumZ = g_curCraft->pushAccumY;
			g_paiContext.controller->maneuverPhase = 1;
			g_paiContext.controller->aiPlanState = 944;
			g_paiContext.controller->maneuverTimer = 2360;
			paiman_setpower(g_paiContext.objectIndex, UINT16_MAX);
		}
	} else if (g_modelDefs[g_curCraft->modelIndex].hasHyperdrive != 0) {
		if (g_curCraft->wasCaptured == 0) {
			if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMethod == 0) {
				return 1;
			}
		} else if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartViaMothership ==
				   0) {
			return 1;
		}
	}
	return 0;
}

// FUNCTION: XVT 0x468180
int16_t paiorder_mothershiporder(void) {
	if (g_objectTable[g_paiContext.objectIndex].genusId == CRAFT_GENUS_PLATFORM) {
		return 0;
	}
	if (g_curCraft->leader_obj_idx == UINT8_MAX) {
		return g_paiContext.controller->targetObjIdx == 0x800Du;
	}
	return g_paiContext.targetCraft->aiController.targetObjIdx == 0x800Du;
}

// FUNCTION: XVT 0x4681E0
int16_t paiorder_lookforcrafttoboardorder(void) {
	uint16_t candidateTargetIdx;

	candidateTargetIdx = g_paiContext.controller->candidateTargetIdx;
	if (candidateTargetIdx != UINT16_MAX && candidateTargetIdx != AI_TARGET_ABORT) {
		if (pai_IsObjectTargetable(candidateTargetIdx) != 0) {
			g_paiContext.controller->targetObjIdx = candidateTargetIdx;
			g_paiContext.controller->targetSignature = g_objectTable[candidateTargetIdx].objectSignature;
			g_paiContext.controller->hasLiveTarget = 1;
			return 1;
		}
		g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
	}
	candidateTargetIdx = (uint16_t)pai_FindBoardingTargetFromOrder(g_paiContext.orderSlot);
	if (candidateTargetIdx != UINT16_MAX) {
		g_paiContext.controller->targetObjIdx = candidateTargetIdx;
		g_paiContext.controller->targetSignature = g_objectTable[candidateTargetIdx].objectSignature;
		g_paiContext.controller->hasLiveTarget = 1;
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x4683F0
int16_t paiorder_abortboardorder(void) {
	int16_t shouldAbort;
	uint8_t maneuverPhase;
	uint16_t targetObjIdx;
	ObjectRecord* target;

	shouldAbort = 0;
	maneuverPhase = g_paiContext.controller->maneuverPhase;
	if (maneuverPhase < 3) {
		targetObjIdx = g_paiContext.controller->targetObjIdx;
		target = &g_objectTable[targetObjIdx];
		if (target->mobj != NULL) {
			if (target->objectType == 0) {
				shouldAbort = 1;
			}
			if (target->mobj->state == 5) {
				shouldAbort = 1;
			}
		} else if (target->objectType == 0) {
			shouldAbort = 1;
		}
		if (g_curCraft->workingSubsystems == 0) {
			shouldAbort = 1;
		}
		if (g_paiContext.controller->targetSignature != target->objectSignature) {
			shouldAbort = 1;
		}
	}

	if (maneuverPhase == 1 || maneuverPhase == 2) {
		target = &g_objectTable[targetObjIdx];
		if (target->playerOwnerIdx != -1 && target->mobj->speed != 0) {
			shouldAbort = 1;
		}
	}

	if (shouldAbort) {
		g_curCraft->pushAccumZ = 0;
		g_curCraft->pushAccumY = g_curCraft->pushAccumZ;
		g_curCraft->pushAccumX = g_curCraft->pushAccumY;
		g_paiContext.controller->targetObjIdx = 0x8000;
		g_paiContext.controller->targetSignature = 0;
		g_paiContext.controller->hasLiveTarget = 0;
		pai_UpdateAimPointFromOrderTarget();
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x468520
int16_t paiorder_returnboardorder(void) {
	pai_CalcAnglesToAimPoint();
	return trig2_polardistance < 0x4000;
}

// FUNCTION: XVT 0x468540
int16_t paiorder_awaitboardorder(void) {
	uint8_t boardingState;

	boardingState = g_curCraft->boardingState;
	if (boardingState == 2 || boardingState == 3) {
		++g_paiContext.controller->orderScratch.goalProgress[g_paiContext.orderSlot];
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
				.fg.orders[g_paiContext.orderSlot]
				.variable1 <= g_curCraft->aiFlight.orderActionCounter) {
			g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			g_curCraft->subsystemDamage = 0;
			if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
						.fg.orders[g_paiContext.orderSlot]
						.variable1 == g_curCraft->aiFlight.orderActionCounter &&
				strcmp(g_planTable[g_paiContext.controller->currentPlanId].name, "disabledpln") == 0) {
				msg_emitCraftMessage(g_paiContext.objectIndex, g_curCraft, IFMSG_138_HAS_BEEN_REPAIRED);
				return 0;
			}
		} else {
			g_curCraft->boardingState = 0;
		}
	}
	return 0;
}

// FUNCTION: XVT 0x468670
int16_t paiorder_makedisabledorder(void) {
	g_curCraft->workingSubsystems = 0;
	return 0;
}

// FUNCTION: XVT 0x468690
int16_t paiorder_returnboardorder_2(void) {
	pai_CalcAnglesToAimPoint();
	return trig2_polardistance < 0x4000;
}

// FUNCTION: XVT 0x4686B0
int16_t paiorder_rocketsonboardorder(void) {
	unsigned int targetObjIdx;
	int16_t targetGenus;
	uint16_t requiredWarheadClass;
	uint16_t launcherIndex;
	uint8_t projectileType;
	uint16_t matchesRequiredClass;
	uint16_t weaponSlot;
	uint16_t lastWeaponSlot;

	targetObjIdx = g_paiContext.controller->targetObjIdx;
	if (g_activeRegionCraftObjectSlotEnd > (int)targetObjIdx) {
		targetGenus = g_objectTable[targetObjIdx].genusId;
		if (targetGenus == 3 || targetGenus == 5 || targetGenus == 4 ||
			(targetGenus == 1 && g_missionFileVersion == 14)) {
			requiredWarheadClass = 2;
		} else {
			requiredWarheadClass = 1;
		}
	} else {
		requiredWarheadClass = 1;
	}

	for (launcherIndex = 0; launcherIndex < g_curCraft->warheadLauncherCount; ++launcherIndex) {
		projectileType = g_curCraft->warheadSlotTypeIds[launcherIndex];
		if (g_projectileDamageByObjectType.warheadClass[projectileType - PROJECTILE_OBJECT_TYPE_FIRST] !=
			requiredWarheadClass) {
			if (g_missionFileVersion == 14 &&
				(projectileType == WARHEAD_OBJECT_TYPE_MAGNETIC_PULSE || projectileType == 153)) {
				matchesRequiredClass = 1;
				if (requiredWarheadClass != 2) {
					matchesRequiredClass = 0;
				}
			} else {
				matchesRequiredClass = 0;
			}
			if (!matchesRequiredClass) {
				continue;
			}
		}
		lastWeaponSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherLastSlot[launcherIndex];
		weaponSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[launcherIndex];
		while (weaponSlot <= lastWeaponSlot) {
			if (g_curCraft->weaponSlots[weaponSlot].count != 0) {
				return 1;
			}
			++weaponSlot;
		}
	}

	return 0;
}

// FUNCTION: XVT 0x468820
int16_t paiorder_avoidhitorder(void) {
	uint16_t objectIdx;
	int maxRangeScore;
	uint8_t genusId = g_objectTable[g_paiContext.objectIndex].genusId;

	if (genusId == CRAFT_GENUS_FREIGHTER || genusId == CRAFT_GENUS_STARSHIP)
		return 0;

	if (g_paiContext.controller->maneuverMode == g_paiContext.initialManeuverId) {
		int objectTeam;
		int sourceTeam;
		int isHostile;

		if (g_curCraft->lastAttackerObjIdx == UINT16_MAX) {
			maxRangeScore = g_aiWarheadThreatRangeBySkill[g_paiContext.skillTier];
			for (objectIdx = (uint16_t)g_projectileObjectSlotStart; objectIdx < g_projectileObjectSlotEnd;
				 ++objectIdx) {
				if (g_objectTable[objectIdx].objectType != 0 &&
					g_objectTable[objectIdx].mobj->pWarheadGuidance->homingTier != 0 &&
					g_objectTable[objectIdx].mobj->pWarheadGuidance->targetObjIdx ==
						g_paiContext.objectIndex) {
					if (pai_IsObjectWithinCurrentPointRange(
							objectIdx, (unsigned int)((g_objectTable[objectIdx].objectType ==
														   WARHEAD_OBJECT_TYPE_CONCUSSION_MISSILE ||
													   g_objectTable[objectIdx].objectType == 149)
														  ? maxRangeScore * 3
														  : maxRangeScore)) == 1) {
						g_curCraft->lastAttackerObjIdx = objectIdx;
						pai_ObjectRefDirectionToObjectRef(g_paiContext.objectIndex, objectIdx);
						if (g_aiThreatBearingClassByOctant
								[(uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.objectIndex].yaw) >>
								 13] == 0)
							g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_TURN_INSIDE;
						else
							g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_AVOID_ATTACKER;
						paiman_initmaneuver();
						if (g_curCraft->cmAmmoCount != 0) {
							if (g_curCraft->cmTypeId == COUNTERMEASURE_TYPE_CHAFF &&
								(g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_COUNTERMEASURES) != 0) {
								if (g_curCraft->chaffActiveTimer == 0) {
									g_curCraft->chaffActiveTimer += 10;
									if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
												.fg.status1 != 21 &&
										g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
												.fg.status2 != 21)
										--g_curCraft->cmAmmoCount;
								}
							} else if (g_curCraft->cmTypeId == COUNTERMEASURE_TYPE_FLARE &&
									   g_curCraft->cmFireCooldownTimer == 0) {
								laser_createcountermeasureprojectile(g_paiContext.objectIndex,
																	 COUNTERMEASURE_PROJECTILE_OBJECT_TYPE);
							}
						}
						return 0;
					}
				}
			}
			maxRangeScore = g_aiAttackerSearchRangeBySkill[g_paiContext.skillTier];
			if (g_curCraft->lastAttackerObjIdx == UINT16_MAX) {
				for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
					 objectIdx < g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
					if (g_objectTable[objectIdx].objectType == 0)
						continue;
					objectTeam = g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.team;
					sourceTeam = g_objectTable[g_paiContext.objectIndex].mobj->team;
					isHostile =
						sourceTeam == objectTeam ? 0 : g_missionTeams[sourceTeam].allies[objectTeam] == 0;
					if (isHostile && g_curCraft->objectKind == CRAFT_OBJECT_KIND_ACTIVE &&
						g_objectTable[objectIdx].genusId == CRAFT_GENUS_STARFIGHTER &&
						pai_IsObjectWithinCurrentPointRange(objectIdx, (unsigned int)maxRangeScore) == 1) {
						uint16_t horizontalAngle;
						uint16_t verticalAngle;

						pai_ObjectRefDirectionToObjectRef(objectIdx, g_paiContext.objectIndex);
						horizontalAngle = (uint16_t)(trig2_xyangle - g_objectTable[objectIdx].yaw);
						if (horizontalAngle >= 0x8000)
							horizontalAngle = (uint16_t)-horizontalAngle;
						verticalAngle = (uint16_t)(pitchQ16 - g_objectTable[objectIdx].pitch);
						if (verticalAngle >= 0x8000)
							verticalAngle = (uint16_t)-verticalAngle;
						if (horizontalAngle < 0x2000 && verticalAngle < 0x2000) {
							g_curCraft->lastAttackerObjIdx = objectIdx;
							break;
						}
					}
				}
			}
		}
		if (g_curCraft->lastAttackerObjIdx != UINT16_MAX &&
			g_objectTable[g_paiContext.objectIndex].genusId != CRAFT_GENUS_UTILITY_VEHICLE &&
			(((g_curCraft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_SHIELDS) != 0 &&
			  g_curCraft->shieldEnergy[0] < 500) ||
			 g_curCraft->hullDamage >= g_curCraft->systemDamageHullThreshold)) {
			pai_ObjectRefDirectionToObjectRef(g_curCraft->lastAttackerObjIdx, g_paiContext.objectIndex);
			if (trig2_polardistance < 0x8000 && g_curCraft->cmTypeId == COUNTERMEASURE_TYPE_FLARE &&
				g_curCraft->cmAmmoCount != 0 && g_curCraft->cmFireCooldownTimer == 0)
				laser_createcountermeasureprojectile(g_paiContext.objectIndex,
													 COUNTERMEASURE_PROJECTILE_OBJECT_TYPE);
			if (g_objectTable[g_curCraft->lastAttackerObjIdx].playerOwnerIdx != -1) {
				g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_AVOID_ATTACKER;
				paiman_initmaneuver();
				paiman_setpower(g_paiContext.objectIndex, UINT16_MAX);
			}
		}
	}

	return 0;
}

// FUNCTION: XVT 0x468CF0
int16_t paiorder_waitforallreturnorder(void) {
	uint16_t flightGroupIndex;
	uint16_t objectIndex;
	ObjectRecord* object;

	flightGroupIndex = 0;
	if ((int16_t)g_missionHeader.numFlightGroups > 0) {
		do {
			if (!((g_missionFgStats[flightGroupIndex].arrivalEnabled == 0 &&
				   g_missionFlightGroups[flightGroupIndex].playerOwnerIdx == -1) ||
				  g_paiContext.orderFlightGroupIndex == flightGroupIndex ||
				  g_missionFlightGroups[flightGroupIndex].fg.departureMethod == 0 ||
				  g_missionFlightGroups[flightGroupIndex].fg.departureMothership !=
					  g_paiContext.orderFlightGroupIndex)) {
				if (g_missionFgStats[flightGroupIndex].hasArrived == 0 ||
					g_missionFgStats[flightGroupIndex].wavesRemaining != 0) {
					return 0;
				}
				for (objectIndex = g_activeRegionObjectSlotStart;
					 objectIndex < g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
					object = &g_objectTable[objectIndex];
					if (object->objectType != 0 && object->flightGroupIdx == flightGroupIndex) {
						return 0;
					}
				}
			}
			++flightGroupIndex;
		} while ((int16_t)g_missionHeader.numFlightGroups > (int)flightGroupIndex);
	}
	return 1;
}

// FUNCTION: XVT 0x468E40
int16_t paiorder_waitforallcreateorder(void) {
	uint16_t flightGroupIndex;

	flightGroupIndex = 0;
	if ((int16_t)g_missionHeader.numFlightGroups > 0) {
		do {
			if (!((g_missionFgStats[flightGroupIndex].arrivalEnabled == 0 &&
				   g_missionFlightGroups[flightGroupIndex].playerOwnerIdx == -1) ||
				  g_paiContext.orderFlightGroupIndex == flightGroupIndex ||
				  g_missionFlightGroups[flightGroupIndex].fg.arrivalMethod == 0 ||
				  g_missionFlightGroups[flightGroupIndex].fg.arrivalMothership !=
					  g_paiContext.orderFlightGroupIndex ||
				  (g_missionFgStats[flightGroupIndex].hasArrived != 0 &&
				   g_missionFgStats[flightGroupIndex].wavesRemaining == 0))) {
				return 0;
			}
			++flightGroupIndex;
		} while ((int16_t)g_missionHeader.numFlightGroups > (int)flightGroupIndex);
	}

	return 1;
}

// FUNCTION: XVT 0x468F20
int16_t paiorder_evasiveorder(void) {
	if (g_paiContext.controller->maneuverMode != g_paiContext.initialManeuverId ||
		g_paiContext.controller->candidateTargetIdx != AI_TARGET_ABORT) {
		return 0;
	}

	g_paiContext.controller->targetObjIdx = 0xffff;
	g_paiContext.controller->targetSignature = 0;
	g_paiContext.controller->hasLiveTarget = 0;
	g_paiContext.controller->candidateTargetIdx = 0xffff;

	return 1;
}

// FUNCTION: XVT 0x468F80
int16_t paiorder_targetfromplayerorder(void) {
	uint16_t candidateTargetIdx;
	unsigned int objectIndex;
	int validTarget;

	candidateTargetIdx = g_paiContext.controller->candidateTargetIdx;
	if (candidateTargetIdx == UINT16_MAX || candidateTargetIdx == AI_TARGET_ABORT) {
		return 0;
	}
	objectIndex = g_paiContext.controller->candidateTargetIdx;
	validTarget = pai_IsObjectTargetable(objectIndex);
	if (validTarget != 0) {
		candidateTargetIdx = g_paiContext.controller->candidateTargetIdx;
		if (g_paiContext.controller->targetObjIdx == candidateTargetIdx) {
			return 0;
		}
		g_paiContext.controller->targetObjIdx = candidateTargetIdx;
		g_paiContext.controller->targetSignature =
			g_objectTable[g_paiContext.controller->targetObjIdx].objectSignature;
		g_paiContext.controller->hasLiveTarget = 1;
		return 0;
	}
	g_paiContext.controller->candidateTargetIdx = UINT16_MAX;
	return 0;
}

// FUNCTION: XVT 0x469150
int16_t paiorder_avoidstarshiporder(void) {
	enum {
		COLLISION_LOOKAHEAD_STEPS = 6,
		QUARTER_TURN = 0x4000,
		MIN_AVOIDANCE_SECONDS = 3,
		AVOIDANCE_DURATION_MASK = 3,
	};

	int16_t maneuverMode;
	CraftData* savedCraft;
	uint16_t collisionObjectIndex;

	maneuverMode = g_paiContext.controller->maneuverMode;
	if (maneuverMode != AI_MANEUVER_MODE_AVOID_STARSHIP) {
		int16_t sourceObjectIndex = g_paiContext.objectIndex;

		savedCraft = g_curCraft;
		collisionObjectIndex = collide_craftstarshipcollision(sourceObjectIndex, COLLISION_LOOKAHEAD_STEPS);
		g_curCraft = savedCraft;
		if (collisionObjectIndex != UINT16_MAX) {
			if (g_paiContext.controller->targetObjIdx == collisionObjectIndex &&
				(maneuverMode == AI_MANEUVER_MODE_ATTACK || maneuverMode == AI_MANEUVER_MODE_ROCKET_ATTACK)) {
				uint8_t objectType = g_objectTable[collisionObjectIndex].objectType;
				if (objectType != CRAFT_SPECIES_CALAMARI_CRUISER &&
					objectType != CRAFT_SPECIES_IMPERIAL_STAR_DESTROYER)
					return 0;
			}
			if (collisionObjectIndex != g_paiContext.objectIndex &&
				savedCraft->carriedObjectIndex != collisionObjectIndex) {
				if ((savedCraft->waveNumber & 1) != 0)
					g_paiContext.controller->targetXYAngle =
						(uint16_t)(g_objectTable[g_paiContext.objectIndex].yaw + QUARTER_TURN);
				else
					g_paiContext.controller->targetXYAngle =
						(uint16_t)(g_objectTable[g_paiContext.objectIndex].yaw - QUARTER_TURN);

				if (g_paiContext.controller->targetZAngle > QUARTER_TURN)
					g_paiContext.controller->targetZAngle =
						(uint16_t)(g_objectTable[g_paiContext.objectIndex].pitch - QUARTER_TURN);
				else
					g_paiContext.controller->targetZAngle =
						(uint16_t)(g_objectTable[g_paiContext.objectIndex].pitch + QUARTER_TURN);

				g_paiContext.controller->maneuverMode = AI_MANEUVER_MODE_AVOID_STARSHIP;
				paiman_initmaneuver();
				if (collisionObjectIndex >= g_regionMainObjectSlotEnd) {
					g_paiContext.controller->aiPlanState =
						SIMULATION_TICKS_PER_SECOND *
						((GameRand() & AVOIDANCE_DURATION_MASK) + MIN_AVOIDANCE_SECONDS);
				}
			} else {
				return 0;
			}
		}
	} else if (g_paiContext.controller->aiPlanState == 0) {
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x469310
int16_t paiorder_checkhyperorder(void) {
	return g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMethod != 0;
}

// FUNCTION: XVT 0x469340
int16_t paiorder_stopgohomeorder(void) {
	enum {
		DEPART_TIMER_ACTIVE = 1,
		SECONDS_PER_MINUTE = 60,
		MINUTES_PER_HOUR = 60,
		MESSAGE_MODEL_SLOT = 0,
		MESSAGE_FLIGHT_GROUP_SLOT = 1,
	};

	int departNow;
	uint8_t departureClockSeconds;
	uint8_t departureClockMinutes;

	if (g_curCraft->aiFlight.maxSpeedCache == 0)
		return 0;

	departNow = 0;
	departureClockSeconds = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureClockSec;
	departureClockMinutes = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureClockMin;
	if (departureClockSeconds + departureClockMinutes != 0) {
		if (g_missionElapsedClock.minutes > departureClockMinutes) {
			departNow = 1;
		} else if (g_missionElapsedClock.minutes == departureClockMinutes &&
				   g_missionElapsedClock.seconds >= departureClockSeconds) {
			departNow = 1;
		}
	}

	if (g_curCraft->aiFlight.departTimerFlag == 0) {
		if ((g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
					 .fg.departureTrigger.triggers[0]
					 .condition != 0 ||
			 g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
					 .fg.departureTrigger.triggers[1]
					 .condition != 0) &&
			(Mission_EvaluateTriggerPair(
				 &g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureTrigger, 0) &
			 1) != 0) {
			departNow = 1;
		}
		if (departNow == 1) {
			uint8_t* departTimerFlag;
			uint16_t flightGroupIndex;

			g_curCraft->aiFlight.departClockHours = g_missionElapsedClock.hours;
			g_curCraft->aiFlight.departClockMinutes = g_missionElapsedClock.minutes;
			g_curCraft->aiFlight.departClockSeconds = g_missionElapsedClock.seconds;
			departTimerFlag = &g_curCraft->aiFlight.departTimerFlag;
			if (*departTimerFlag == 0) {
				flightGroupIndex = g_paiContext.orderFlightGroupIndex;
				++g_missionFgStats[flightGroupIndex].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
				if (g_missionFlightGroups[flightGroupIndex].fg.specialCargoCraft == g_curCraft->waveNumber) {
					g_missionFgStats[flightGroupIndex]
						.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] = 1;
				}
			}
			*departTimerFlag = DEPART_TIMER_ACTIVE;
		}
	}

	if (g_curCraft->aiFlight.departTimerFlag == DEPART_TIMER_ACTIVE) {
		unsigned int departureDelaySeconds;
		unsigned int elapsedSeconds;
		uint16_t order;
		uint8_t planId;

		departureDelaySeconds =
			SECONDS_PER_MINUTE *
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureDelayMinutes +
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureDelaySeconds;
		elapsedSeconds =
			SECONDS_PER_MINUTE *
				(g_missionElapsedClock.minutes +
				 MINUTES_PER_HOUR * (g_missionElapsedClock.hours - g_curCraft->aiFlight.departClockHours) -
				 g_curCraft->aiFlight.departClockMinutes) -
			g_curCraft->aiFlight.departClockSeconds + g_missionElapsedClock.seconds;
		if (departureDelaySeconds == 0 || elapsedSeconds >= departureDelaySeconds) {
			fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_WITHDRAWING,
										   g_paiContext.objectIndex, UINT16_MAX);
			g_msgSenderIff = (uint8_t)g_objectTable[g_paiContext.objectIndex].mobj->iff;
			msg_addMessagePtr(MESSAGE_MODEL_SLOT, &g_modelDefs[g_curCraft->modelIndex]);
			msg_addMessagePtr(MESSAGE_FLIGHT_GROUP_SLOT,
							  &g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]);
			if (Hud_MissionFG_GetCraftNumberIfShown(g_paiContext.orderFlightGroupIndex, g_curCraft) == 0) {
				msg_emitInFlightMessage(IFMSG_385_ARG_ARG_WITHDRAWING_FROM_COMBAT_AREA, g_localPlayer);
			} else {
				msg_emitInFlightMessage(IFMSG_386_FLIGHT_GROUP_ARG_ARG_WITHDRAWING_FROM_COMBAT_AREA,
										g_localPlayer);
			}

			order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
						.fg.orders[g_paiContext.orderSlot]
						.order;
			planId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
			if (strcmp(g_planTable[planId].name, "waitforboardpln") == 0 &&
				g_curCraft->subsystemDamage == 0) {
				g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			}
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XVT 0x469690
int16_t paiorder_completegohomeorder(void) {
	uint8_t completionState;
	uint16_t orderSlot;
	unsigned int activeOrderCount;
	uint16_t flightGroupIdx;
	int completedOrderCount;
	int completedBoardingOrderCount;
	int16_t allOrdersComplete;

	completionState = g_paiContext.controller->orderScratch.completionState[g_paiContext.orderSlot];
	if (completionState != 2 && completionState != 3) {
		if (pai_IsPlanCompleteForOrderSlot(g_paiContext.controller->currentPlanId, g_paiContext.orderSlot) !=
			0) {
			g_paiContext.controller->orderScratch.completionState[g_paiContext.orderSlot] = 2;
		} else if (pai_IsBoardingPlanCompleteForOrderSlot(g_paiContext.controller->currentPlanId,
														  g_paiContext.orderSlot) != 0) {
			g_paiContext.controller->orderScratch.completionState[g_paiContext.orderSlot] = 3;
		}
	}

	completionState = g_paiContext.controller->orderScratch.completionState[g_paiContext.orderSlot];
	if (completionState != 2 && completionState != 3)
		return 0;
	if (g_curCraft->aiFlight.maxSpeedCache == 0)
		return 0;
	if (g_paiContext.controller->orderStateFlag == 1)
		return 1;

	orderSlot = 0;
	activeOrderCount = 0;
	flightGroupIdx = g_paiContext.orderFlightGroupIndex;
	completedOrderCount = 0;
	completedBoardingOrderCount = 0;
	do {
		if (g_missionFlightGroups[flightGroupIdx].fg.orders[orderSlot].order != 0) {
			++activeOrderCount;
			completionState = g_paiContext.controller->orderScratch.completionState[orderSlot];
			if (completionState == 2)
				++completedOrderCount;
			if (completionState == 3)
				++completedBoardingOrderCount;
		}
		++orderSlot;
	} while (orderSlot < 3);
	if (activeOrderCount != 0 && (unsigned int)completedOrderCount == activeOrderCount &&
		g_curCraft->aiFlight.goHomeFlag == 0) {
		g_curCraft->aiFlight.goHomeFlag = 1;
	}
	allOrdersComplete = (unsigned int)(completedBoardingOrderCount + completedOrderCount) >= activeOrderCount;
	return allOrdersComplete;
}

// FUNCTION: XVT 0x4697F0
int16_t paiorder_completegootherorder(void) {
	enum {
		ORDER_NONE = 0,
		ORDER_STATE_COMPLETE = 1,
		ORDER_COMPLETION_COMPLETE = 2,
		THIRD_ORDER_SLOT = 2,
		FOURTH_ORDER_SLOT = 3,
	};

	int order;

	if (g_paiContext.controller->orderStateFlag == ORDER_STATE_COMPLETE) {
		return 0;
	}
	if (g_paiSkipToOrder4Checked == 0) {
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.skipToOrder4.triggers[0].condition !=
				MISSION_COND_ALWAYS_TRUE ||
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.skipToOrder4.triggers[1].condition !=
				MISSION_COND_ALWAYS_TRUE) {
			if ((Mission_EvaluateTriggerPair(
					 &g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.skipToOrder4, 0) &
				 1) != 0) {
				g_paiContext.controller->orderStateFlag = ORDER_STATE_COMPLETE;
				g_paiContext.orderSlot = FOURTH_ORDER_SLOT;
				g_paiContext.controller->currentOrderSlot = FOURTH_ORDER_SLOT;
				order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
							.fg.orders[g_paiContext.orderSlot]
							.order;
				g_paiContext.controller->currentPlanId =
					g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
				if (g_curCraft->leader_obj_idx == UINT8_MAX) {
					g_paiContext.nullPlanId =
						g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
				} else {
					g_paiContext.nullPlanId =
						g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
				}
				return 1;
			}
			g_paiSkipToOrder4Checked = 1;
		}
	}

	if (g_paiContext.controller->orderScratch.completionState[g_paiContext.orderSlot] !=
			ORDER_COMPLETION_COMPLETE ||
		g_paiContext.orderSlot == THIRD_ORDER_SLOT ||
		g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
				.fg.orders[g_paiContext.orderSlot + 1]
				.order == ORDER_NONE) {
		return 0;
	}

	++g_paiContext.orderSlot;
	g_paiContext.controller->currentOrderSlot = (uint8_t)g_paiContext.orderSlot;
	order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[g_paiContext.orderSlot].order;
	g_paiContext.controller->currentPlanId =
		g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
	if (g_curCraft->leader_obj_idx == UINT8_MAX) {
		g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
	} else {
		g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
	}
	return 1;
}

// FUNCTION: XVT 0x469A10
int16_t paiorder_waitgootherorder(void) {
	uint16_t order;
	uint16_t orderSlot;
	const char* planName;

	if (g_paiContext.controller->orderStateFlag == 1) {
		return 0;
	}
	orderSlot = g_paiContext.orderSlot + 1;
	while (orderSlot < 3) {
		order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[orderSlot].order;
		planName = g_planTable[order].name;
		if (strcmp(planName, "capldr1pln") == 0 || strcmp(planName, "capescortersldr1pln") == 0 ||
			strcmp(planName, "caprespondldr1pln") == 0 || strcmp(planName, "capldr2pln") == 0 ||
			strcmp(planName, "capldr3pln") == 0 || strcmp(planName, "capldr4pln") == 0 ||
			strcmp(planName, "capldr5pln") == 0 || strcmp(planName, "capflw1pln") == 0 ||
			strcmp(planName, "capflw2pln") == 0 || strcmp(planName, "capflw3pln") == 0 ||
			strcmp(planName, "capflw4pln") == 0) {
			g_paiContext.controller->currentOrderSlot = (uint8_t)orderSlot;
			g_paiContext.controller->currentPlanId =
				g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
			if (g_curCraft->leader_obj_idx == UINT8_MAX) {
				g_paiContext.nullPlanId =
					g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
			} else {
				g_paiContext.nullPlanId =
					g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
			}
			return 1;
		}
		++orderSlot;
	}
	return 0;
}

// FUNCTION: XVT 0x469BD0
int16_t paiorder_orderswitchorder(void) {
	enum {
		ORDER_STATE_COMPLETE = 1,
		ORDER_COMPLETION_COMPLETE = 2,
		FOURTH_ORDER_SLOT = 3,
	};

	int order;
	int16_t foundOrder;
	uint16_t orderSlot;

	if (g_paiContext.controller->orderStateFlag == ORDER_STATE_COMPLETE)
		return 0;

	if (g_paiSkipToOrder4Checked == 0) {
		int flightGroupIndex;

		flightGroupIndex = g_paiContext.orderFlightGroupIndex;
		if (g_missionFlightGroups[flightGroupIndex].fg.skipToOrder4.triggers[0].condition !=
				MISSION_COND_ALWAYS_TRUE ||
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.skipToOrder4.triggers[1].condition !=
				MISSION_COND_ALWAYS_TRUE) {
			if ((Mission_EvaluateTriggerPair(&g_missionFlightGroups[flightGroupIndex].fg.skipToOrder4, 0) &
				 1) != 0) {
				g_paiContext.controller->orderStateFlag = ORDER_STATE_COMPLETE;
				g_paiContext.orderSlot = FOURTH_ORDER_SLOT;
				g_paiContext.controller->currentOrderSlot = FOURTH_ORDER_SLOT;
				order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
							.fg.orders[g_paiContext.orderSlot]
							.order;
				g_paiContext.controller->currentPlanId =
					g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
				if (g_curCraft->leader_obj_idx == UINT8_MAX) {
					g_paiContext.nullPlanId =
						g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
				} else {
					g_paiContext.nullPlanId =
						g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
				}
				return 1;
			}
			g_paiSkipToOrder4Checked = 1;
		}
	}

	if (g_paiContext.orderSlot == 0)
		return 0;

	foundOrder = 0;
	orderSlot = 0;
	while (orderSlot < g_paiContext.orderSlot) {
		const char* planName;

		if (foundOrder != 0)
			break;
		if (g_paiContext.controller->orderScratch.completionState[orderSlot] != ORDER_COMPLETION_COMPLETE) {
			order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[orderSlot].order;
			planName = g_planTable[g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]]].name;
			if (strcmp(planName, "capfreeldr1pln") == 0 || strcmp(planName, "caprespondldr1pln") == 0 ||
				strcmp(planName, "capescortersldr1pln") == 0 || strcmp(planName, "disableldr1pln") == 0) {
				if (paifight_OrderSlotCanTarget(orderSlot) != 0)
					foundOrder = 1;
			} else if ((strcmp(planName, "boardtogivepln") == 0 || strcmp(planName, "boardtotakepln") == 0 ||
						strcmp(planName, "boardtoexchangepln") == 0 ||
						strcmp(planName, "boardtocapturepln") == 0 ||
						strcmp(planName, "boardtodestroypln") == 0 ||
						strcmp(planName, "boardtocontactpln") == 0 ||
						strcmp(planName, "boardtorepairpln") == 0) &&
					   pai_OrderSlotCanBoardTarget(orderSlot) != 0) {
				foundOrder = 1;
			}
		}
		++orderSlot;
	}

	if (foundOrder != 0) {
		--orderSlot;
		g_paiContext.controller->currentOrderSlot = (uint8_t)orderSlot;
		order = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[orderSlot].order;
		g_paiContext.controller->currentPlanId =
			g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
		if (g_curCraft->leader_obj_idx == UINT8_MAX) {
			g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
		} else {
			g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
		}
		return 1;
	}

	return 0;
}

// FUNCTION: XVT 0x469F40
int16_t paiorder_completefolloworder(void) {
	AiController* leaderController;
	uint16_t flightGroupIndex;
	int runtimeFlightGroupIndex;
	uint16_t currentOrderSlot;

	leaderController = &g_paiContext.targetCraft->aiController;
	if (g_paiContext.leaderObjectIndex != UINT8_MAX) {
		if (g_paiContext.targetCraft->aiFlight.goHomeFlag == 1 && g_curCraft->aiFlight.goHomeFlag == 0) {
			g_curCraft->aiFlight.goHomeFlag = 1;
		}
		if (g_paiContext.targetCraft->aiFlight.departTimerFlag == 1 &&
			g_curCraft->aiFlight.departTimerFlag == 0) {
			g_curCraft->aiFlight.departTimerFlag = 1;
			flightGroupIndex = g_paiContext.orderFlightGroupIndex;
			runtimeFlightGroupIndex = g_paiContext.orderFlightGroupIndex;
			++g_missionFgStats[runtimeFlightGroupIndex].outcomeCount[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED];
			if (g_missionFlightGroups[flightGroupIndex].fg.specialCargoCraft == g_curCraft->waveNumber) {
				g_missionFgStats[runtimeFlightGroupIndex]
					.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_NOT_DEPARTED] = 1;
			}
		}
	}

	currentOrderSlot = leaderController->currentOrderSlot;
	if (g_paiContext.orderSlot != currentOrderSlot) {
		int order;

		g_paiContext.orderSlot = currentOrderSlot;
		g_paiContext.controller->currentOrderSlot = (uint8_t)currentOrderSlot;
		order =
			g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.orders[g_paiContext.orderSlot].order;
		{
			AiController* currentController;
			int planNameIndex;

			currentController = g_paiContext.controller;
			planNameIndex = g_orderLeaderBuiltinPlanNameIndex[order];
			currentController->currentPlanId = g_builtinPlanIdByNameIndex[planNameIndex];
		}
		if (g_paiContext.leaderObjectIndex == UINT8_MAX) {
			g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
			return 1;
		}
		g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x46A0A0
int16_t paiorder_killselforder(void) {
	uint16_t delaySeconds;

	if (g_objectTable[g_paiContext.objectIndex].mobj->lifetimeTimer == 0) {
		delaySeconds = g_missionFlightGroups[g_paiContext.orderFlightGroupIndex]
						   .fg.orders[g_paiContext.orderSlot]
						   .variable1;
		if (delaySeconds == 0)
			delaySeconds = ((uint16_t)GameRand() & 3) + 2;

		g_objectTable[g_paiContext.objectIndex].mobj->lifetimeTimer = (uint16_t)(delaySeconds * 1180u);
	}
	return 0;
}

// FUNCTION: XVT 0x46A250
int16_t paiorder_abortmotherwaitorder(void) {
	int16_t result;

	if ((int)g_paiContext.controller->targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}
	if (g_modelDefs[g_curCraft->modelIndex].hasHyperdrive == 0) {
		return 0;
	}

	result = 0;
	if (g_curCraft->wasCaptured != 0) {
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartViaMothership != 0) {
			uint16_t mothershipFlightGroup;

			mothershipFlightGroup =
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.capturedDepartureMothership;
			if (g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] ==
				g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
				result = 1;
			}
		}
	} else {
		int16_t departureMothershipReady;
		int16_t alternateMothershipReady;

		departureMothershipReady = 1;
		alternateMothershipReady = 1;
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMethod != 0) {
			uint16_t mothershipFlightGroup;

			mothershipFlightGroup =
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.departureMothership;
			if (g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] !=
				g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
				departureMothershipReady = 0;
			}
		}
		if (g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothershipUsed != 0) {
			uint16_t mothershipFlightGroup;

			mothershipFlightGroup =
				g_missionFlightGroups[g_paiContext.orderFlightGroupIndex].fg.alternateMothership;
			if (g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_TOTAL] !=
				g_missionFgStats[mothershipFlightGroup].outcomeCount[FLIGHT_GROUP_OUTCOME_ARRIVED]) {
				alternateMothershipReady = 0;
			}
		}
		result = (int16_t)(departureMothershipReady & alternateMothershipReady);
	}
	return result;
}
