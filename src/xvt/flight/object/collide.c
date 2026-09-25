#include "xvt/flight/object/collide.h"
#ifdef XVT_MODERN
#include "xvt_runtime/timing/flight_timing.h"
#include "xvt_runtime/timing/player_timing.h"
#endif
#ifdef XVT_MODERN
#include "xvt_runtime/assets/opt_native.h"
#endif

#include "xvt/assets/model_mesh.h"
#include "xvt/assets/opt_model.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/ai/pai.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/fview.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/hud/msg.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/flight/object/laser.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/object/static.h"
#include "xvt/flight/player/player.h"
#include "xvt/flight/proving_grounds.h"
#include "xvt/frontend/config.h"
#include "xvt/math/math.h"
#include "xvt/math/math2.h"
#include "xvt/math/math3d.h"
#include "xvt/math/trig2.h"
#include "xvt/util/game_rand.h"
#include "xvt/util/memory.h"

#include <string.h>

typedef struct CollideOptRotationScale {
	OptVector origin;
	OptVector axis;
} CollideOptRotationScale;

typedef struct CollisionTargetRangeScratch {
	int segmentStartWorldX;
	int segmentStartWorldY;
	int segmentStartWorldZ;
	int probeWorldX;
	int probeWorldY;
	int probeWorldZ;
	int sweepStartX;
	int sweepStartY;
	int sweepStartZ;
	int sweepEndX;
	int sweepEndY;
	int sweepEndZ;
	int hitOffsetX;
	int hitOffsetY;
	int hitOffsetZ;
} CollisionTargetRangeScratch;

// GLOBAL: XVT 0x5181FC
const float g_collideZeroFloat = 0.0f;
// GLOBAL: XVT 0x518208
const float g_collideOptAxisQ15ToFloatScale = 0.000030517578125f;
// GLOBAL: XVT 0x527E84
int g_collideSweepRejectNearStartHits = 0;
// GLOBAL: XVT 0x527E88
OptNode* g_collideCurrentMeshVertsNode = NULL;
// GLOBAL: XVT 0x622C50
OptVector g_collideSweepWalkerStart = { 0 };
// GLOBAL: XVT 0x622C60
OptVector g_collideSweepWalkerEnd = { 0 };
// GLOBAL: XVT 0x622C70
OptVector g_collideSweepWalkerStartSaved = { 0 };
// GLOBAL: XVT 0x622C80
OptVector g_collideSweepWalkerEndSaved = { 0 };
// GLOBAL: XVT 0x622C6C
int g_collideSweepHitMeshOrdinal = 0;
// GLOBAL: XVT 0x622C8C
int g_collideSweepCurrentMeshOrdinal = 0;
// GLOBAL: XVT 0x622C94
int g_warheadLaunchHullMeshOrdinal = 0;
// GLOBAL: XVT 0x622C90
float g_collideCurrentMeshRotationAngle = 0.0f;
// GLOBAL: XVT 0x622C98
float g_collideSweepHitFraction = 0.0f;
// GLOBAL: XVT 0x9A1FD4
int g_collisionProbeWorldX = 0;
// GLOBAL: XVT 0x9A1FD8
int g_collisionProbeWorldY = 0;
// GLOBAL: XVT 0x9A1FD0
int g_collisionProbeWorldZ = 0;
// GLOBAL: XVT 0x9EC604
int g_collisionSegmentStartWorldX = 0;
// GLOBAL: XVT 0xA081E8
int g_collisionSegmentStartWorldY = 0;
// GLOBAL: XVT 0xA08298
int g_collisionSegmentStartWorldZ = 0;
// GLOBAL: XVT 0x9A7398
int g_approxDist = 0;
// GLOBAL: XVT 0x51BF58
int g_collisionStagedModelProbe = 0;
// GLOBAL: XVT 0x9D80C0
int g_collisionSweepStartX = 0;
// GLOBAL: XVT 0x9D8C18
int g_collisionSweepStartY = 0;
// GLOBAL: XVT 0x9D1150
int g_collisionSweepStartZ = 0;
// GLOBAL: XVT 0xA07BDC
int g_collisionSweepEndX = 0;
// GLOBAL: XVT 0xA07C54
int g_collisionSweepEndY = 0;
// GLOBAL: XVT 0xA07C58
int g_collisionSweepEndZ = 0;
// GLOBAL: XVT 0x9D77FC
int g_collisionHitOffsetX = 0;
// GLOBAL: XVT 0x9D6828
int g_collisionHitOffsetY = 0;
// GLOBAL: XVT 0x9CD260
int g_collisionHitOffsetZ = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x419890
void collide_PopulateMobileObjectProximityCandidates(MobileObjectProximityList* list, uint16_t ownerObjIdx) {
	ObjectRecord* ownerObject;
	MobileObject* ownerMobileObject;
	uint16_t candidateObjIdx;
	uint16_t sourceObjIdx;

	ownerObject = &g_objectTable[ownerObjIdx];
	if (ownerObject->playerOwnerIdx != -1) {
		if (ownerObject->mobj->pCraft->workingSubsystems != 0) {
			for (candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				 candidateObjIdx < g_activeRegionCraftObjectSlotEnd; ++candidateObjIdx) {
				ObjectRecord* candidateObject = &g_objectTable[candidateObjIdx];

				if (candidateObject->objectType != 0 && candidateObjIdx != ownerObjIdx &&
					candidateObject->genusId != CRAFT_GENUS_EXPLOSION &&
					candidateObject->flightGroupIdx != ownerObject->flightGroupIdx) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
			}
		}

		if (g_flightMissionState.provingGroundsModeActive == 0) {
			for (candidateObjIdx = (uint16_t)g_regionMainObjectSlotEnd;
				 candidateObjIdx < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd;
				 ++candidateObjIdx) {
				if (g_objectTable[candidateObjIdx].objectType != 0) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
			}
		}
		return;
	}

	ownerMobileObject = ownerObject->mobj;
	sourceObjIdx = ownerMobileObject->sourceObjIdx;
	switch (ownerObject->genusId) {
		case CRAFT_GENUS_STARFIGHTER:
		case CRAFT_GENUS_TRANSPORT:
		case CRAFT_GENUS_UTILITY_VEHICLE:
			if (g_flightMissionState.provingGroundsModeActive != 0) {
				return;
			}

			for (candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				 candidateObjIdx < g_activeRegionCraftObjectSlotEnd; ++candidateObjIdx) {
				ObjectRecord* candidateObject = &g_objectTable[candidateObjIdx];

				if (candidateObject->flightGroupIdx == ownerObject->flightGroupIdx ||
					candidateObject->objectType == 0) {
					continue;
				}

				if (candidateObject->playerOwnerIdx != -1 && candidateObject->mobj != NULL) {
					collide_InsertMobileObjectProximityCandidate(&candidateObject->mobj->proximityList,
																 candidateObjIdx, ownerObjIdx);
				}

				if (candidateObject->genusId != CRAFT_GENUS_STARSHIP &&
					candidateObject->genusId != CRAFT_GENUS_FREIGHTER &&
					candidateObject->genusId != CRAFT_GENUS_PLATFORM) {
					continue;
				}
				if (candidateObject->mobj == NULL) {
					continue;
				}

				if (candidateObject->mobj->pCraft != NULL &&
					candidateObject->mobj->pCraft->aiController.maneuverMode != AI_MANEUVER_MODE_DROPOFF &&
					candidateObject->mobj->pCraft->carrierObjIdx == UINT16_MAX) {
					collide_InsertMobileObjectProximityCandidate(&candidateObject->mobj->proximityList,
																 candidateObjIdx, ownerObjIdx);
				}
			}

			for (candidateObjIdx = (uint16_t)g_regionMainObjectSlotEnd;
				 candidateObjIdx < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd;
				 ++candidateObjIdx) {
				uint8_t objectType = g_objectTable[candidateObjIdx].objectType;

				/* Static proximity hazards: mines, probes, and navigation buoys. */
				if (objectType >= 100 && objectType <= 105) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
			}
			return;

		case CRAFT_GENUS_FREIGHTER:
		case CRAFT_GENUS_STARSHIP:
		case CRAFT_GENUS_PLATFORM:
			if (ownerMobileObject->pCraft->aiController.maneuverMode == AI_MANEUVER_MODE_DROPOFF ||
				ownerMobileObject->pCraft->carrierObjIdx != UINT16_MAX) {
				return;
			}

			for (candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				 candidateObjIdx < g_activeRegionCraftObjectSlotEnd; ++candidateObjIdx) {
				ObjectRecord* candidateObject;

				if (candidateObjIdx == ownerObjIdx) {
					continue;
				}
				candidateObject = &g_objectTable[candidateObjIdx];
				if (candidateObject->objectType == 0) {
					continue;
				}

				if (candidateObject->playerOwnerIdx != -1) {
					if (candidateObject->mobj != NULL) {
						collide_InsertMobileObjectProximityCandidate(&candidateObject->mobj->proximityList,
																	 candidateObjIdx, ownerObjIdx);
					}
					continue;
				}
				if (candidateObject->genusId == CRAFT_GENUS_EXPLOSION) {
					continue;
				}

				collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				if (candidateObject->genusId != CRAFT_GENUS_STARSHIP &&
					candidateObject->genusId != CRAFT_GENUS_FREIGHTER &&
					candidateObject->genusId != CRAFT_GENUS_PLATFORM) {
					continue;
				}
				if (candidateObject->mobj == NULL) {
					continue;
				}

				if (candidateObject->mobj->pCraft != NULL &&
					candidateObject->mobj->pCraft->aiController.maneuverMode != AI_MANEUVER_MODE_DROPOFF &&
					candidateObject->mobj->pCraft->carrierObjIdx == UINT16_MAX) {
					collide_InsertMobileObjectProximityCandidate(&candidateObject->mobj->proximityList,
																 candidateObjIdx, ownerObjIdx);
				}
			}
			return;

		case CRAFT_GENUS_PLAYER_PROJECTILE:
		case CRAFT_GENUS_OTHER_PROJECTILE: {
			uint16_t targetObjIdx =
				g_projectileGuidanceStates[ownerObjIdx - g_projectileObjectSlotStart].targetObjIdx;

			for (candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				 candidateObjIdx < g_projectileObjectSlotEnd; ++candidateObjIdx) {
				ObjectRecord* candidateObject = &g_objectTable[candidateObjIdx];
				int targetIsPlayerOwned;

				if (candidateObject->objectType == 0) {
					continue;
				}
#ifdef XVT_MODERN
				/* Impact effects remain in projectile slots after a hit. */
				if (candidateObjIdx >= g_activeRegionCraftObjectSlotEnd &&
					(candidateObject->objectType < PROJECTILE_OBJECT_TYPE_FIRST ||
					 candidateObject->objectType >=
						 PROJECTILE_OBJECT_TYPE_FIRST + PROJECTILE_OBJECT_TYPE_COUNT)) {
					continue;
				}
#endif
				if (candidateObjIdx >= g_activeRegionCraftObjectSlotEnd &&
					(g_projectileDamageByObjectType
							 .warheadClass[candidateObject->objectType - PROJECTILE_OBJECT_TYPE_FIRST] == 0 ||
					 candidateObjIdx == ownerObjIdx || candidateObject->mobj->sourceObjIdx == sourceObjIdx)) {
					continue;
				}
				if (candidateObjIdx == sourceObjIdx || candidateObject->genusId == CRAFT_GENUS_EXPLOSION) {
					continue;
				}

				targetIsPlayerOwned = g_activeRegionCraftObjectSlotEnd > targetObjIdx &&
									  g_activeRegionObjectSlotStart <= targetObjIdx &&
									  g_objectTable[targetObjIdx].playerOwnerIdx != -1;
				if (targetIsPlayerOwned || g_objectTable[sourceObjIdx].playerOwnerIdx != -1 ||
					targetObjIdx == candidateObjIdx) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
			}

			if (g_flightMissionState.provingGroundsModeActive == 0) {
				for (candidateObjIdx = (uint16_t)g_regionMainObjectSlotEnd;
					 candidateObjIdx < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd;
					 ++candidateObjIdx) {
					if (g_objectTable[candidateObjIdx].objectType != 0) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
					}
				}
			}
			return;
		}

		default:
			return;
	}
}

// FUNCTION: XVT 0x419DF0
void collide_collisions(void) {
	enum {
		ENGINE_WASH_UPDATE_TICKS = 29,
		NEW_OBJECT_COLLISION_GRACE_FRAMES = 3,
		INSPECTION_LARGE_MODEL_EXTENT = 3000,
		INSPECTION_RANGE_SCALE = 4,
		IFF_COUNT = 10,
		MISSION_GOAL_COUNT = 8,
		TARGET_DESCRIPTION_REFRESH_TICKS = 944,
		ACTION_PROMPT_TICKS = 1888,
		MIN_DOCK_PROMPT_FRAMES = 45,
		HANGAR_RANGE = 0x2000,
		LARGE_STARSHIP_HANGAR_RANGE = 0x4000,
		AI_MANEUVER_PHASE_APPROACH = 2,
		PROXIMITY_LIST_CAPACITY = 16,
		SMALL_DISTANCE_SQUARED = 50,
		BOUNCE_DIRECTION_SCALE = 100,
		BOUNCE_IMPULSE_SCALE = 1000,
		ANGLE_HALF_TURN = 0x8000,
		ANGLE_QUARTER_TURN = 0x4000,
		MAX_ROLL_IMPULSE = 0x7FFF,
		INVALID_MESH_INDEX = -1,
		FLIGHT_GROUP_STATUS_PROTECTED = 20,
		PENDING_ACTION_ENTER_HANGAR = 2,
		CRAFT_MESSAGE_INSPECTED = 142,
		INSPECTION_PLAYER_ARG_BASE = 293,
		EXPLOSION_OBJECT_TYPE_PROJECTILE = 129,
		EXPLOSION_OBJECT_TYPE_LASER = 131,
		EXPLOSION_OBJECT_TYPE_ION = 132,
	};

	uint16_t ownerObjIdx;

	for (ownerObjIdx = (uint16_t)g_activeRegionObjectSlotStart; ownerObjIdx < g_projectileObjectSlotEnd;
		 ++ownerObjIdx) {
		MobileObjectProximityList* list;
		int playerIdx;
		uint16_t candidateSlot;

		if (g_objectTable[ownerObjIdx].objectType == 0 ||
			g_objectTable[ownerObjIdx].genusId == CRAFT_GENUS_EXPLOSION) {
			continue;
		}

		playerIdx = g_objectTable[ownerObjIdx].playerOwnerIdx;
		if (playerIdx != -1) {
			uint16_t targetObjIdx;

			if (g_players[playerIdx].hyperspacePhase == 2) {
				continue;
			}
			if (g_players[playerIdx].impactDamageCooldownTime < g_gameTime
#ifdef XVT_MODERN
				&& XvtFlightTiming_ReferenceDue()
#endif
			) {
				uint16_t sourceObjIdx;

				g_players[playerIdx].engineWashSourceObjIdx = -1;
				g_players[playerIdx].engineWashStrength = 0;
				g_players[playerIdx].impactDamageCooldownTime = g_gameTime + ENGINE_WASH_UPDATE_TICKS;
				for (sourceObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
					 sourceObjIdx < g_activeRegionCraftObjectSlotEnd; ++sourceObjIdx) {
					ObjectRecord* sourceObject = &g_objectTable[sourceObjIdx];
					CraftData* sourceCraft;

					if (sourceObject->objectType == 0) {
						continue;
					}
					sourceCraft = sourceObject->mobj->pCraft;
					if (sourceCraft != NULL && sourceCraft->workingSubsystems != 0 &&
						sourceObject->genusId >= CRAFT_GENUS_FREIGHTER &&
						sourceObject->genusId <= CRAFT_GENUS_STARSHIP) {
						collide_ApplyEngineWashDamage(ownerObjIdx, sourceObjIdx);
					}
				}
			}

			g_collisionProbeWorldX = g_objectTable[ownerObjIdx].world_x;
			g_collisionProbeWorldY = g_objectTable[ownerObjIdx].world_y;
			g_collisionProbeWorldZ = g_objectTable[ownerObjIdx].world_z;
			g_collisionSegmentStartWorldX = g_objectTable[ownerObjIdx].mobj->prevWorldX;
			g_collisionSegmentStartWorldY = g_objectTable[ownerObjIdx].mobj->prevWorldY;
			g_collisionSegmentStartWorldZ = g_objectTable[ownerObjIdx].mobj->prevWorldZ;

			targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			if (targetObjIdx >= g_activeRegionObjectSlotStart &&
				targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
				ObjectRecord* targetObject = &g_objectTable[targetObjIdx];

				if (targetObject->objectType != 0 && targetObject->genusId != CRAFT_GENUS_EXPLOSION) {
					CraftData* targetCraft;
					int maxBoundsExtent;

					g_collisionSweepEndX = targetObject->world_x;
					g_collisionSweepEndY = targetObject->world_y;
					g_collisionSweepEndZ = targetObject->world_z;
					g_collisionSweepStartX = targetObject->mobj->prevWorldX;
					g_collisionSweepStartY = targetObject->mobj->prevWorldY;
					g_collisionSweepStartZ = targetObject->mobj->prevWorldZ;
					g_approxDist = collide_roughdistance3d(g_collisionProbeWorldX - g_collisionSweepEndX,
														   g_collisionProbeWorldY - g_collisionSweepEndY,
														   g_collisionProbeWorldZ - g_collisionSweepEndZ);
					targetCraft = targetObject->mobj->pCraft;
					if (targetCraft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						targetCraft->objectKind != CRAFT_OBJECT_KIND_EXPLODING &&
						targetCraft->iffVisibility[g_objectTable[ownerObjIdx].mobj->team] == 0) {
						maxBoundsExtent = g_modelTypeTable[targetObject->objectType].maxBoundsExtent;
						if (maxBoundsExtent > INSPECTION_LARGE_MODEL_EXTENT) {
							maxBoundsExtent >>= 1;
						}
						if (INSPECTION_RANGE_SCALE * maxBoundsExtent > g_approxDist) {
							uint16_t maxVisibility = 0;
							uint16_t playerIff = (uint16_t)g_players[playerIdx].playerIff;
							uint8_t flightGroupIdx = targetObject->flightGroupIdx;
							int specialCargoFlag = 0;
							int goalMessageRequired = 0;
							int playerScored;
							uint16_t iffIndex;
							int goalIndex;

							for (iffIndex = 0; iffIndex < IFF_COUNT; ++iffIndex) {
								if (maxVisibility < targetCraft->iffVisibility[iffIndex]) {
									maxVisibility = targetCraft->iffVisibility[iffIndex];
								}
							}
							++maxVisibility;
							targetCraft->iffVisibility[playerIff] = (uint8_t)maxVisibility;
							++g_flightMissionState.runtime.teamFgCounters[0][playerIff][flightGroupIdx];
							++g_players[playerIdx].perMissionKills.numCraftInspected;
							++g_missionFgStats[flightGroupIdx].outcomeCount[FLIGHT_GROUP_OUTCOME_INSPECTED];
							++g_missionFgStats[flightGroupIdx].teamInspected[playerIff];
							if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft ==
								targetCraft->waveNumber) {
								g_missionFgStats[flightGroupIdx]
									.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_INSPECTED] = 1;
								++g_players[playerIdx].perMissionKills.numSpecialInspected;
								specialCargoFlag = 1;
								++g_missionFgStats[flightGroupIdx].teamSpecialCargoInspected[playerIff];
							}
							playerScored = Mission_ApplyFlightGroupGoalScore(
								MISSION_COND_INSPECTED, flightGroupIdx, playerIdx, maxVisibility - 1,
								specialCargoFlag, playerIff);
							Mission_ApplyFlightGroupGoalScore(MISSION_COND_INSPECTED, flightGroupIdx, -1,
															  maxVisibility - 1, specialCargoFlag, playerIff);
							for (goalIndex = 0; goalIndex < MISSION_GOAL_COUNT; ++goalIndex) {
								FlightGroupGoal* goal =
									&g_missionFlightGroups[flightGroupIdx].fg.goals[goalIndex];

								if (goal->enabledTeams[playerIff] != 0 &&
									(goal->eventCondition == MISSION_COND_INSPECTED ||
									 (goal->amount == GOAL_AMT_ALL_SPECIAL_CARGO &&
									  (specialCargoFlag != 0 ||
									   g_missionFgStats[flightGroupIdx]
											   .specialCargoOutcome[FLIGHT_GROUP_OUTCOME_INSPECTED] == 0))) &&
									(goal->type == 0 || goal->type == 2)) {
									goalMessageRequired = 1;
								}
							}
							if (goalMessageRequired != 0 && playerIdx == g_localPlayer) {
								if (specialCargoFlag != 0) {
									msg_emitInFlightMessage(
										IFMSG_358_INSPECTION_COMPLETED_SPECIAL_CARGO_FOUND, g_localPlayer);
								} else {
									msg_emitInFlightMessage(IFMSG_357_INSPECTION_ASSIGNMENT_COMPLETED,
															g_localPlayer);
								}
								g_playerFlightTransientTimers[g_localPlayer].targetDescriptionRefreshTimer =
									TARGET_DESCRIPTION_REFRESH_TICKS;
							}
							g_hudCachedTargetObjectIdx = -3;
							if ((goalMessageRequired != 0 ||
								 targetObject->genusId != CRAFT_GENUS_STARFIGHTER ||
								 (g_flightMissionState.locatePlayersEnabled == 0 &&
								  targetObject->playerOwnerIdx != -1)) &&
								playerIdx == g_localPlayer) {
								msg_emitCraftMessage(targetObjIdx, targetCraft, CRAFT_MESSAGE_INSPECTED);
								if (playerScored != 0 && g_flightPlayerCount > 1 &&
									g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
									g_msgArgTable[0] = (uint16_t)(maxVisibility + INSPECTION_PLAYER_ARG_BASE);
									msg_emitInFlightMessage(IFMSG_308_YOU_ARE_THE_ARG_TO_INSPECT_THIS_CRAFT,
															g_localPlayer);
								}
							}
						}
					}
				}
			}

			if (g_objectTable[ownerObjIdx].mobj->framesAlive >= MIN_DOCK_PROMPT_FRAMES) {
				uint16_t mothershipPass;

				for (mothershipPass = 0; mothershipPass < 2; ++mothershipPass) {
					uint8_t mothershipFlightGroup = 0;
					int hasMothership = 0;
					XvtFlightGroup* ownerFlightGroup =
						&g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].fg;

					if (mothershipPass == 0) {
						if (ownerFlightGroup->departureMethod != 0) {
							mothershipFlightGroup = ownerFlightGroup->departureMothership;
							hasMothership = 1;
						}
					} else if (ownerFlightGroup->alternateMothershipUsed != 0) {
						mothershipFlightGroup = ownerFlightGroup->alternateMothership;
						hasMothership = 1;
					}
					if (hasMothership != 0) {
						uint16_t mothershipObjIdx;

						for (mothershipObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
							 mothershipObjIdx < g_activeRegionCraftObjectSlotEnd; ++mothershipObjIdx) {
							ObjectRecord* mothership = &g_objectTable[mothershipObjIdx];
							CraftData* mothershipCraft;
							unsigned int promptRange;

							if (mothership->objectType == 0 || mothership->genusId == CRAFT_GENUS_EXPLOSION ||
								mothership->flightGroupIdx != mothershipFlightGroup) {
								continue;
							}
							g_collisionSweepEndX = mothership->world_x;
							g_collisionSweepEndY = mothership->world_y;
							g_collisionSweepEndZ = mothership->world_z;
							g_collisionSweepStartX = mothership->mobj->prevWorldX;
							g_collisionSweepStartY = mothership->mobj->prevWorldY;
							g_collisionSweepStartZ = mothership->mobj->prevWorldZ;
							mothershipCraft = mothership->mobj->pCraft;
							if (mothershipCraft->objectKind != CRAFT_OBJECT_KIND_ACTIVE) {
								continue;
							}
							pai_RotateLocalVectorToWorldScratch(
								mothership, g_modelDefs[mothershipCraft->modelIndex].hangarPoints.inside.side,
								g_modelDefs[mothershipCraft->modelIndex].hangarPoints.inside.up,
								g_modelDefs[mothershipCraft->modelIndex].hangarPoints.inside.forward);
							g_collisionSweepEndX += g_rotatedX;
							g_collisionSweepEndY += g_rotatedY;
							g_collisionSweepEndZ += g_rotatedZ;
							promptRange = HANGAR_RANGE;
							if (mothership->genusId == CRAFT_GENUS_STARSHIP &&
								g_flightMissionState.runtime
										.teamGoalStatus[(uint16_t)g_players[playerIdx].playerIff][0] == 1) {
								promptRange = LARGE_STARSHIP_HANGAR_RANGE;
							}
							if ((unsigned int)collide_roughdistance3d(
									g_collisionSweepEndX - g_collisionProbeWorldX,
									g_collisionSweepEndY - g_collisionProbeWorldY,
									g_collisionSweepEndZ - g_collisionProbeWorldZ) < promptRange &&
								g_players[playerIdx].pendingActionId == 0) {
								if (playerIdx == g_localPlayer) {
									msg_emitInFlightMessage(
										IFMSG_214_HIT_SPACE_TO_ACTIVATE_TRACTOR_BEAM_AND_ENTER_HANGAR,
										g_localPlayer);
								}
								g_players[playerIdx].pendingActionId = PENDING_ACTION_ENTER_HANGAR;
								g_players[playerIdx].pendingActionParam = (int16_t)mothershipObjIdx;
								g_players[playerIdx].pendingActionTimer = ACTION_PROMPT_TICKS;
							}
						}
					}
				}
			}
		}

		if (g_objectTable[ownerObjIdx].mobj == NULL) {
			continue;
		}
		list = &g_objectTable[ownerObjIdx].mobj->proximityList;
		list->overflowScore -= g_elapsedTicks;
		if (list->overflowScore <= 0) {
			list->overflowScore = 0x7FFF;
			collide_PopulateMobileObjectProximityCandidates(list, ownerObjIdx);
		}

		for (candidateSlot = 0; candidateSlot < list->count; ++candidateSlot) {
			uint16_t candidateObjIdx = list->objIdx[candidateSlot];

			if (g_objectTable[candidateObjIdx].objectType == 0 ||
				g_objectTable[candidateObjIdx].genusId == CRAFT_GENUS_EXPLOSION) {
				collide_RemoveMobileObjectProximityCandidate(list, candidateObjIdx);
				--candidateSlot;
				continue;
			}
			if (g_objectTable[candidateObjIdx].mobj != NULL &&
				g_objectTable[candidateObjIdx].mobj->framesAlive < NEW_OBJECT_COLLISION_GRACE_FRAMES &&
				g_objectTable[candidateObjIdx].genusId != CRAFT_GENUS_PLAYER_PROJECTILE &&
				g_objectTable[candidateObjIdx].genusId != CRAFT_GENUS_OTHER_PROJECTILE &&
				g_objectTable[ownerObjIdx].genusId != CRAFT_GENUS_PLAYER_PROJECTILE &&
				g_objectTable[ownerObjIdx].genusId != CRAFT_GENUS_OTHER_PROJECTILE) {
				continue;
			}
			list->score[candidateSlot] -= g_elapsedTicks;
			if (list->score[candidateSlot] > 0) {
				continue;
			}
			if (g_objectTable[ownerObjIdx].mobj->framesAlive < NEW_OBJECT_COLLISION_GRACE_FRAMES &&
				g_objectTable[ownerObjIdx].genusId != CRAFT_GENUS_PLAYER_PROJECTILE &&
				g_objectTable[ownerObjIdx].genusId != CRAFT_GENUS_OTHER_PROJECTILE &&
				g_objectTable[candidateObjIdx].genusId != CRAFT_GENUS_PLAYER_PROJECTILE &&
				g_objectTable[candidateObjIdx].genusId != CRAFT_GENUS_OTHER_PROJECTILE) {
				collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				continue;
			}

			if (g_objectTable[ownerObjIdx].playerOwnerIdx != -1) {
				CraftData* ownerCraft = g_objectTable[ownerObjIdx].mobj->pCraft;

				if (ownerCraft->aiFlight.impactObjIdx == candidateObjIdx) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
					continue;
				}
				g_collisionProbeWorldX = g_objectTable[ownerObjIdx].world_x;
				g_collisionProbeWorldY = g_objectTable[ownerObjIdx].world_y;
				g_collisionProbeWorldZ = g_objectTable[ownerObjIdx].world_z;
				g_collisionSegmentStartWorldX = g_objectTable[ownerObjIdx].mobj->prevWorldX;
				g_collisionSegmentStartWorldY = g_objectTable[ownerObjIdx].mobj->prevWorldY;
				g_collisionSegmentStartWorldZ = g_objectTable[ownerObjIdx].mobj->prevWorldZ;
				if (g_objectTable[candidateObjIdx].mobj != NULL) {
					CraftData* candidateCraft = g_objectTable[candidateObjIdx].mobj->pCraft;
					int16_t hitMeshIndex;

					if (strcmp(g_planTable[candidateCraft->aiController.currentPlanId].name,
							   "boardtogivepln") == 0 &&
						candidateCraft->aiController.targetObjIdx == ownerObjIdx) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
						continue;
					}
					g_collisionSweepEndX = g_objectTable[candidateObjIdx].world_x;
					g_collisionSweepEndY = g_objectTable[candidateObjIdx].world_y;
					g_collisionSweepEndZ = g_objectTable[candidateObjIdx].world_z;
					g_collisionSweepStartX = g_objectTable[candidateObjIdx].mobj->prevWorldX;
					g_collisionSweepStartY = g_objectTable[candidateObjIdx].mobj->prevWorldY;
					g_collisionSweepStartZ = g_objectTable[candidateObjIdx].mobj->prevWorldZ;
					hitMeshIndex = collide_lasercraftcollide(ownerObjIdx, candidateObjIdx);
					if (hitMeshIndex != 0) {
						if (g_flightMissionState.provingGroundsModeActive != 0) {
							g_objectTable[ownerObjIdx].mobj->speed = 0;
							ownerCraft->throttleSpeed = 0;
							g_objectTable[ownerObjIdx].world_x = g_provingGroundsLocalPlayerWorldXHistory[3];
							g_objectTable[ownerObjIdx].mobj->prevWorldX = g_objectTable[ownerObjIdx].world_x;
							g_objectTable[ownerObjIdx].world_y = g_provingGroundsLocalPlayerWorldYHistory[3];
							g_objectTable[ownerObjIdx].mobj->prevWorldY = g_objectTable[ownerObjIdx].world_y;
							g_objectTable[ownerObjIdx].world_z = g_provingGroundsLocalPlayerWorldZHistory[3];
							g_objectTable[ownerObjIdx].mobj->prevWorldZ = g_objectTable[ownerObjIdx].world_z;
							g_objectTable[ownerObjIdx].roll = g_provingGroundsLocalPlayerRollHistory[3];
							g_objectTable[ownerObjIdx].pitch = g_provingGroundsLocalPlayerPitchHistory[3];
							ownerCraft->pitch = g_objectTable[ownerObjIdx].pitch;
							g_objectTable[ownerObjIdx].yaw = g_provingGroundsLocalPlayerYawHistory[3];
#ifdef XVT_MODERN
							if (XvtFlightTiming_IsUnlocked())
								XvtPlayerTiming_Recover(g_localPlayer);
#endif
							g_objectTable[ownerObjIdx].mobj->moveVectorDirty = 1;
							g_objectTable[ownerObjIdx].mobj->orientMatrixDirty =
								g_objectTable[ownerObjIdx].mobj->moveVectorDirty;
							fsfx_PlaySound((GameRand() & ANGLE_HALF_TURN) == 0 ? FLIGHT_SOUND_HULL_HIT_2
																			   : FLIGHT_SOUND_HULL_HIT_1,
										   ownerObjIdx, g_objectTable[ownerObjIdx].playerOwnerIdx);
						} else {
							if (g_objectTable[candidateObjIdx].genusId <= CRAFT_GENUS_UTILITY_VEHICLE) {
								int16_t angleDifference;
								uint16_t candidateSpeed;
								int16_t relativeSpeed;
								int deltaX;
								int deltaY;
								int deltaZ;
								int distanceSquared;
								int impulseX;
								int impulseY;
								int16_t adjustedMoveX;
								int16_t adjustedMoveY;
								int16_t rollImpulse;
								uint16_t savedYaw;

								ownerCraft->aiFlight.impactObjIdx = candidateObjIdx;
								angleDifference =
									g_objectTable[candidateObjIdx].yaw - g_objectTable[ownerObjIdx].yaw;
								if ((uint16_t)angleDifference >= ANGLE_HALF_TURN) {
									angleDifference =
										g_objectTable[ownerObjIdx].yaw - g_objectTable[candidateObjIdx].yaw;
								}
								if ((uint16_t)angleDifference > ANGLE_QUARTER_TURN) {
									angleDifference = ANGLE_HALF_TURN - angleDifference;
								}
								candidateSpeed = g_objectTable[candidateObjIdx].mobj->speed;
								if ((uint16_t)angleDifference < ANGLE_QUARTER_TURN) {
									relativeSpeed = g_objectTable[ownerObjIdx].mobj->speed -
													trig2_cosinewordmult(candidateSpeed, angleDifference);
								} else {
									relativeSpeed = g_objectTable[ownerObjIdx].mobj->speed +
													trig2_cosinewordmult(candidateSpeed, angleDifference);
								}
								if (relativeSpeed < 0) {
									relativeSpeed = -relativeSpeed;
								}
								deltaX = g_objectTable[ownerObjIdx].world_x -
										 g_objectTable[candidateObjIdx].world_x;
								deltaY = g_objectTable[ownerObjIdx].world_y -
										 g_objectTable[candidateObjIdx].world_y;
								deltaZ = g_objectTable[ownerObjIdx].world_z -
										 g_objectTable[candidateObjIdx].world_z;
								distanceSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
								if (distanceSquared <= SMALL_DISTANCE_SQUARED) {
									impulseX = 0;
									impulseY = BOUNCE_DIRECTION_SCALE;
								} else {
									deltaX *= BOUNCE_IMPULSE_SCALE * relativeSpeed;
									impulseX = deltaX / distanceSquared;
									deltaY *= BOUNCE_IMPULSE_SCALE * relativeSpeed;
									impulseY = deltaY / distanceSquared;
								}
								if (g_objectTable[ownerObjIdx].mobj->moveVectorDirty != 0) {
									FVIEW_calcrotatemove(g_objectTable[ownerObjIdx].pitch,
														 g_objectTable[ownerObjIdx].yaw,
														 &g_objectTable[ownerObjIdx]);
								}
								adjustedMoveX = g_objectTable[ownerObjIdx].mobj->moveX + (int16_t)impulseX;
								if (((uint16_t)(g_objectTable[ownerObjIdx].mobj->moveX ^ adjustedMoveX) &
									 ANGLE_HALF_TURN) != 0) {
									adjustedMoveX = g_objectTable[ownerObjIdx].mobj->moveX;
								}
								adjustedMoveY = g_objectTable[ownerObjIdx].mobj->moveY + (int16_t)impulseY;
								if (((uint16_t)(g_objectTable[ownerObjIdx].mobj->moveY ^ adjustedMoveY) &
									 ANGLE_HALF_TURN) != 0) {
									adjustedMoveY = g_objectTable[ownerObjIdx].mobj->moveY;
								}
								savedYaw = g_objectTable[ownerObjIdx].yaw;
								g_objectTable[ownerObjIdx].yaw = trig2_arctan(adjustedMoveX, adjustedMoveY);
								g_objectTable[candidateObjIdx].pitch = trig2_w_arccos(adjustedMoveX);
								candidateCraft->pitch = g_objectTable[candidateObjIdx].pitch;
								rollImpulse = BOUNCE_DIRECTION_SCALE * relativeSpeed;
								if ((uint16_t)rollImpulse >= ANGLE_HALF_TURN) {
									rollImpulse = MAX_ROLL_IMPULSE;
								}
								if (g_objectTable[candidateObjIdx].yaw > savedYaw) {
									rollImpulse = -rollImpulse;
								}
								g_objectTable[candidateObjIdx].mobj->rollImpulseRate = rollImpulse;
								FVIEW_calcrotatemove(g_objectTable[candidateObjIdx].pitch,
													 g_objectTable[candidateObjIdx].yaw,
													 &g_objectTable[candidateObjIdx]);
								FVIEW_calcrotateorient(g_objectTable[candidateObjIdx].roll, 0,
													   &g_objectTable[candidateObjIdx]);
								candidateCraft->aiFlight.impactObjIdx = ownerObjIdx;

								if (distanceSquared <= SMALL_DISTANCE_SQUARED) {
									impulseX = 0;
									impulseY = BOUNCE_DIRECTION_SCALE;
								} else {
									impulseX =
										BOUNCE_IMPULSE_SCALE * deltaX * relativeSpeed / distanceSquared;
									impulseY =
										BOUNCE_IMPULSE_SCALE * deltaY * relativeSpeed / distanceSquared;
								}
								if (g_objectTable[candidateObjIdx].mobj->moveVectorDirty != 0) {
									FVIEW_calcrotatemove(g_objectTable[candidateObjIdx].pitch,
														 g_objectTable[candidateObjIdx].yaw,
														 &g_objectTable[candidateObjIdx]);
								}
								adjustedMoveX =
									g_objectTable[candidateObjIdx].mobj->moveX + (int16_t)impulseX;
								if (((uint16_t)(g_objectTable[candidateObjIdx].mobj->moveX ^ adjustedMoveX) &
									 ANGLE_HALF_TURN) != 0) {
									adjustedMoveX = g_objectTable[candidateObjIdx].mobj->moveX;
								}
								adjustedMoveY =
									g_objectTable[candidateObjIdx].mobj->moveY + (int16_t)impulseY;
								if (((uint16_t)(g_objectTable[candidateObjIdx].mobj->moveY ^ adjustedMoveY) &
									 ANGLE_HALF_TURN) != 0) {
									adjustedMoveY = g_objectTable[candidateObjIdx].mobj->moveY;
								}
								savedYaw = g_objectTable[candidateObjIdx].yaw;
								g_objectTable[candidateObjIdx].yaw =
									trig2_arctan(adjustedMoveX, adjustedMoveY);
								g_objectTable[ownerObjIdx].pitch = trig2_w_arccos(adjustedMoveX);
								ownerCraft->pitch = g_objectTable[ownerObjIdx].pitch;
								g_objectTable[candidateObjIdx].mobj->orientMatrixDirty = 1;
								g_objectTable[candidateObjIdx].mobj->moveVectorDirty = 1;
								rollImpulse = BOUNCE_DIRECTION_SCALE * relativeSpeed;
								if ((uint16_t)rollImpulse >= ANGLE_HALF_TURN) {
									rollImpulse = MAX_ROLL_IMPULSE;
								}
								if (g_objectTable[ownerObjIdx].yaw > savedYaw) {
									rollImpulse = -rollImpulse;
								}
								g_objectTable[ownerObjIdx].mobj->rollImpulseRate = rollImpulse;
								FVIEW_calcrotatemove(g_objectTable[ownerObjIdx].pitch,
													 g_objectTable[ownerObjIdx].yaw,
													 &g_objectTable[ownerObjIdx]);
								FVIEW_calcrotateorient(g_objectTable[ownerObjIdx].roll, 0,
													   &g_objectTable[ownerObjIdx]);
								msg_emitInFlightMessage(IFMSG_220_COLLISION_WITH_ANOTHER_CRAFT_HAS_OCCURRED,
														g_objectTable[ownerObjIdx].playerOwnerIdx);
								fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_1, ownerObjIdx,
											   g_objectTable[ownerObjIdx].playerOwnerIdx);
								fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_2, candidateObjIdx,
											   g_objectTable[ownerObjIdx].playerOwnerIdx);
							}
							if (g_flightMissionState.collisionsEnabled != 0 ||
								g_objectTable[candidateObjIdx].genusId == CRAFT_GENUS_STARSHIP ||
								g_objectTable[candidateObjIdx].genusId == CRAFT_GENUS_FREIGHTER ||
								g_objectTable[candidateObjIdx].genusId == CRAFT_GENUS_PLATFORM) {
								int dotProduct;

								collide_damagecraft(candidateObjIdx, hitMeshIndex, ownerObjIdx, 0);
								if (g_objectTable[ownerObjIdx].mobj->orientMatrixDirty != 0) {
									FVIEW_calcrotatemove(g_objectTable[ownerObjIdx].pitch,
														 g_objectTable[ownerObjIdx].yaw,
														 &g_objectTable[ownerObjIdx]);
									FVIEW_calcrotateorient(g_objectTable[ownerObjIdx].roll, 0,
														   &g_objectTable[ownerObjIdx]);
								}
								dotProduct = Math_Dot3Q15Wrapped(
									(int16_t)(g_collisionSweepEndX - g_collisionSweepStartX),
									(int16_t)(g_collisionSweepEndY - g_collisionSweepStartY),
									(int16_t)(g_collisionSweepEndZ - g_collisionSweepStartZ),
									g_objectTable[ownerObjIdx].mobj->cachedFwdX,
									g_objectTable[ownerObjIdx].mobj->cachedFwdY,
									g_objectTable[ownerObjIdx].mobj->cachedFwdZ);
								collide_damagecraft(ownerObjIdx, INVALID_MESH_INDEX, candidateObjIdx,
													(dotProduct & ANGLE_HALF_TURN) != 0);
							}
						}
					}
				} else if (g_flightMissionState.provingGroundsModeActive == 0 &&
						   static_laserstaticcollide(ownerObjIdx, candidateObjIdx) != 0) {
					collide_applyCraftImpactBounce(ownerObjIdx, candidateObjIdx);
					if (g_flightMissionState.collisionsEnabled != 0) {
						XvtFlightGroup* candidateGroup =
							&g_missionFlightGroups[g_objectTable[candidateObjIdx].flightGroupIdx].fg;
						XvtFlightGroup* ownerGroup =
							&g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].fg;

						if (candidateGroup->status1 != FLIGHT_GROUP_STATUS_PROTECTED &&
							candidateGroup->status2 != FLIGHT_GROUP_STATUS_PROTECTED &&
							ownerGroup->status1 != FLIGHT_GROUP_STATUS_PROTECTED &&
							ownerGroup->status2 != FLIGHT_GROUP_STATUS_PROTECTED) {
							static_laserhitstatic(ownerObjIdx, candidateObjIdx);
						}
						collide_damagecraft(ownerObjIdx, INVALID_MESH_INDEX, candidateObjIdx, 0);
					}
				}
			} else {
				switch (g_objectTable[ownerObjIdx].genusId) {
					case CRAFT_GENUS_STARFIGHTER:
					case CRAFT_GENUS_TRANSPORT:
					case CRAFT_GENUS_UTILITY_VEHICLE:
						g_collisionProbeWorldX = g_objectTable[ownerObjIdx].world_x;
						g_collisionProbeWorldY = g_objectTable[ownerObjIdx].world_y;
						g_collisionProbeWorldZ = g_objectTable[ownerObjIdx].world_z;
						g_collisionSegmentStartWorldX = g_objectTable[ownerObjIdx].mobj->prevWorldX;
						g_collisionSegmentStartWorldY = g_objectTable[ownerObjIdx].mobj->prevWorldY;
						g_collisionSegmentStartWorldZ = g_objectTable[ownerObjIdx].mobj->prevWorldZ;
						if (candidateObjIdx >= g_regionMainObjectSlotEnd &&
							static_laserstaticcollide(ownerObjIdx, candidateObjIdx) != 0) {
							if (g_flightMissionState.collisionsEnabled != 0) {
								collide_damagecraft(ownerObjIdx, INVALID_MESH_INDEX, candidateObjIdx, 0);
							} else {
								collide_applyCraftImpactBounce(ownerObjIdx, candidateObjIdx);
							}
						}
						break;

					case CRAFT_GENUS_FREIGHTER:
					case CRAFT_GENUS_STARSHIP:
					case CRAFT_GENUS_PLATFORM: {
						CraftData* ownerCraft = g_objectTable[ownerObjIdx].mobj->pCraft;
						CraftData* candidateCraft;
						uint8_t maneuverMode = ownerCraft->aiController.maneuverMode;

						if (maneuverMode == AI_MANEUVER_MODE_DROPOFF ||
							ownerCraft->carrierObjIdx != UINT16_MAX) {
							candidateSlot = PROXIMITY_LIST_CAPACITY;
							continue;
						}
						if (ownerCraft->carriedObjectIndex == candidateObjIdx ||
							(maneuverMode == AI_MANEUVER_MODE_BOARD &&
							 ownerCraft->aiController.targetObjIdx == candidateObjIdx)) {
							collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
							continue;
						}
						candidateCraft = g_objectTable[candidateObjIdx].mobj->pCraft;
						if (strcmp(g_planTable[candidateCraft->aiController.pendingPlanId].name,
								   "exithangarpln") == 0 ||
							strcmp(g_planTable[candidateCraft->aiController.pendingPlanId].name,
								   "outofhyperspacepln") == 0 ||
							strcmp(g_planTable[candidateCraft->aiController.pendingPlanId].name,
								   "enterhangarpln") == 0) {
							collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
							continue;
						}
						if (strcmp(g_planTable[candidateCraft->aiController.pendingPlanId].name,
								   "followhomeevadepln") == 0) {
							CraftData* leaderCraft =
								g_objectTable[candidateCraft->leader_obj_idx].mobj->pCraft;
							if (strcmp(g_planTable[leaderCraft->aiController.pendingPlanId].name,
									   "exithangarpln") == 0 ||
								strcmp(g_planTable[leaderCraft->aiController.pendingPlanId].name,
									   "enterhangarpln") == 0) {
								collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx,
																			 candidateObjIdx);
								continue;
							}
						}
						maneuverMode = candidateCraft->aiController.maneuverMode;
						if (candidateCraft->carrierObjIdx != UINT16_MAX ||
							maneuverMode == AI_MANEUVER_MODE_BOARD ||
							maneuverMode == AI_MANEUVER_MODE_INTO_HYPERSPACE ||
							maneuverMode == AI_MANEUVER_MODE_DROPOFF) {
							collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
							continue;
						}
						g_collisionSweepEndX = g_objectTable[ownerObjIdx].world_x;
						g_collisionSweepEndY = g_objectTable[ownerObjIdx].world_y;
						g_collisionSweepEndZ = g_objectTable[ownerObjIdx].world_z;
						g_collisionSweepStartX = g_objectTable[ownerObjIdx].mobj->prevWorldX;
						g_collisionSweepStartY = g_objectTable[ownerObjIdx].mobj->prevWorldY;
						g_collisionSweepStartZ = g_objectTable[ownerObjIdx].mobj->prevWorldZ;
						g_collisionProbeWorldX = g_objectTable[candidateObjIdx].world_x;
						g_collisionProbeWorldY = g_objectTable[candidateObjIdx].world_y;
						g_collisionProbeWorldZ = g_objectTable[candidateObjIdx].world_z;
						g_collisionSegmentStartWorldX = g_objectTable[candidateObjIdx].mobj->prevWorldX;
						g_collisionSegmentStartWorldY = g_objectTable[candidateObjIdx].mobj->prevWorldY;
						g_collisionSegmentStartWorldZ = g_objectTable[candidateObjIdx].mobj->prevWorldZ;
						{
							int16_t hitMeshIndex = collide_lasercraftcollide(candidateObjIdx, ownerObjIdx);
							if (hitMeshIndex != 0) {
								collide_damagecraft(ownerObjIdx, hitMeshIndex, candidateObjIdx, 0);
								collide_damagecraft(candidateObjIdx, INVALID_MESH_INDEX, ownerObjIdx, 0);
							}
						}
						break;
					}

					case CRAFT_GENUS_PLAYER_PROJECTILE:
					case CRAFT_GENUS_OTHER_PROJECTILE: {
						if (g_objectTable[g_objectTable[ownerObjIdx].mobj->sourceObjIdx].playerOwnerIdx !=
								-1 &&
							candidateObjIdx < g_activeRegionCraftObjectSlotEnd) {
							CraftData* candidateCraft = g_objectTable[candidateObjIdx].mobj->pCraft;
							if (strcmp(g_planTable[candidateCraft->aiController.currentPlanId].name,
									   "boardtogivepln") == 0 &&
								candidateCraft->aiController.maneuverMode == AI_MANEUVER_MODE_BOARD &&
								candidateCraft->aiController.maneuverPhase == AI_MANEUVER_PHASE_APPROACH &&
								candidateCraft->aiController.targetObjIdx == ownerObjIdx) {
								collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx,
																			 candidateObjIdx);
								continue;
							}
						}
						g_collisionProbeWorldX = g_objectTable[ownerObjIdx].world_x;
						g_collisionProbeWorldY = g_objectTable[ownerObjIdx].world_y;
						g_collisionProbeWorldZ = g_objectTable[ownerObjIdx].world_z;
						g_collisionSegmentStartWorldX = g_objectTable[ownerObjIdx].mobj->prevWorldX;
						g_collisionSegmentStartWorldY = g_objectTable[ownerObjIdx].mobj->prevWorldY;
						g_collisionSegmentStartWorldZ = g_objectTable[ownerObjIdx].mobj->prevWorldZ;
						if (g_objectTable[candidateObjIdx].mobj != NULL) {
							int16_t hitMeshIndex;

							g_collisionSweepEndX = g_objectTable[candidateObjIdx].world_x;
							g_collisionSweepEndY = g_objectTable[candidateObjIdx].world_y;
							g_collisionSweepEndZ = g_objectTable[candidateObjIdx].world_z;
							g_collisionSweepStartX = g_objectTable[candidateObjIdx].mobj->prevWorldX;
							g_collisionSweepStartY = g_objectTable[candidateObjIdx].mobj->prevWorldY;
							g_collisionSweepStartZ = g_objectTable[candidateObjIdx].mobj->prevWorldZ;
							hitMeshIndex = collide_lasercraftcollide(ownerObjIdx, candidateObjIdx);
							if (hitMeshIndex != 0) {
								if (candidateObjIdx >= g_activeRegionCraftObjectSlotEnd) {
									if (g_projectileDamageByObjectType
											.warheadClass[g_objectTable[ownerObjIdx].objectType -
														  PROJECTILE_OBJECT_TYPE_FIRST] != 0) {
										if (g_projectileDamageByObjectType
												.warheadClass[g_objectTable[candidateObjIdx].objectType -
															  PROJECTILE_OBJECT_TYPE_FIRST] == 0) {
											Mission_RecordProjectileHitStats(candidateObjIdx);
										} else if (g_objectTable[ownerObjIdx]
													   .mobj->pWarheadGuidance->targetObjIdx ==
												   candidateObjIdx) {
											Mission_RecordProjectileHitStats(ownerObjIdx);
										} else if (g_objectTable[candidateObjIdx]
													   .mobj->pWarheadGuidance->targetObjIdx == ownerObjIdx) {
											Mission_RecordProjectileHitStats(candidateObjIdx);
										}
										collide_ConvertObjectToExplosion(ownerObjIdx,
																		 EXPLOSION_OBJECT_TYPE_PROJECTILE);
									} else {
										Mission_RecordProjectileHitStats(ownerObjIdx);
										if (g_objectTable[ownerObjIdx].objectType ==
												PROJECTILE_OBJECT_TYPE_ION_LASER ||
											g_objectTable[ownerObjIdx].objectType ==
												PROJECTILE_OBJECT_TYPE_ION_TURBO_LASER) {
											collide_ConvertObjectToExplosion(ownerObjIdx,
																			 EXPLOSION_OBJECT_TYPE_ION);
										} else {
											collide_ConvertObjectToExplosion(ownerObjIdx,
																			 EXPLOSION_OBJECT_TYPE_LASER);
										}
									}
									collide_ConvertObjectToExplosion(candidateObjIdx,
																	 EXPLOSION_OBJECT_TYPE_PROJECTILE);
								} else {
									Mission_RecordProjectileHitStats(ownerObjIdx);
									collide_laserhitcraft(ownerObjIdx, candidateObjIdx, hitMeshIndex);
								}
							}
						} else if (g_flightMissionState.provingGroundsModeActive == 0 &&
								   static_laserstaticcollide(ownerObjIdx, candidateObjIdx) != 0) {
							static_laserhitstatic(ownerObjIdx, candidateObjIdx);
							Mission_RecordProjectileHitStats(ownerObjIdx);
						}
						break;
					}

					default:
						break;
				}
			}

			if (g_objectTable[ownerObjIdx].objectType == 0 ||
				g_objectTable[ownerObjIdx].genusId == CRAFT_GENUS_EXPLOSION) {
				list->overflowScore = 0;
				list->count = 0;
				break;
			}
			if (g_objectTable[candidateObjIdx].objectType == 0 ||
				g_objectTable[candidateObjIdx].genusId == CRAFT_GENUS_EXPLOSION) {
				collide_RemoveMobileObjectProximityCandidate(list, candidateObjIdx);
				--candidateSlot;
			}
		}
	}
}

// FUNCTION: XVT 0x41B830
void collide_InsertMobileObjectProximityCandidate(MobileObjectProximityList* list, uint16_t ownerObjIdx,
												  uint16_t candidateObjIdx) {
	int clearance;
	int candidateSpeed;
	int combinedSpeed;
	int proximityScore;
	int index;
	uint8_t count;
	int moveIndex;
	int displacedScore;
	int* score;
	uint16_t* objectIndex;

	clearance =
		collide_roughdistance3d(g_objectTable[ownerObjIdx].world_x - g_objectTable[candidateObjIdx].world_x,
								g_objectTable[ownerObjIdx].world_y - g_objectTable[candidateObjIdx].world_y,
								g_objectTable[ownerObjIdx].world_z - g_objectTable[candidateObjIdx].world_z);
	clearance -= g_modelTypeTable[g_objectTable[ownerObjIdx].objectType].maxBoundsExtent;
	clearance -= g_modelTypeTable[g_objectTable[candidateObjIdx].objectType].maxBoundsExtent;
	if (clearance < 0) {
		proximityScore = 0;
	} else {
		candidateSpeed = collide_GetMobileObjectProximitySpeedQ12(candidateObjIdx);
		clearance >>= 8;
		combinedSpeed = (candidateSpeed + collide_GetMobileObjectProximitySpeedQ12(ownerObjIdx)) >> 8;
		if (combinedSpeed == 0) {
			return;
		}
		proximityScore = 13275 * clearance / combinedSpeed;
	}

	index = 0;
	count = list->count;
	if (count != 0) {
		score = list->score;
		objectIndex = list->objIdx;
		for (;;) {
			if (*objectIndex == candidateObjIdx) {
				list->score[index] = proximityScore;
				moveIndex = index + 1;
				while (moveIndex < list->count && proximityScore > list->score[moveIndex]) {
					list->score[moveIndex - 1] = list->score[moveIndex];
					list->objIdx[moveIndex - 1] = list->objIdx[moveIndex];
					list->score[moveIndex] = proximityScore;
					list->objIdx[moveIndex] = candidateObjIdx;
					++moveIndex;
				}
				index = moveIndex - 1;
				if (index > 0) {
					moveIndex = index - 1;
					do {
						if (proximityScore >= list->score[moveIndex]) {
							break;
						}
						list->score[moveIndex + 1] = list->score[moveIndex];
						list->objIdx[moveIndex + 1] = list->objIdx[moveIndex];
						--moveIndex;
						list->score[moveIndex + 1] = proximityScore;
						list->objIdx[moveIndex + 1] = candidateObjIdx;
					} while (moveIndex >= 0);
				}
				return;
			}
			if (*score > proximityScore) {
				break;
			}
			++score;
			++objectIndex;
			++index;
			if (index >= count) {
				break;
			}
		}
	}

	if (index == count) {
		if (count == 16) {
			if (list->overflowScore > proximityScore) {
				list->overflowScore = proximityScore;
			}
			return;
		}
		list->score[index] = proximityScore;
		list->objIdx[index] = candidateObjIdx;
		++list->count;
		return;
	}

	moveIndex = count;
	if (count == 16) {
		displacedScore = list->score[count - 1];
		if (list->overflowScore > displacedScore) {
			list->overflowScore = displacedScore;
		}
		--count;
		moveIndex = count;
		list->count = count;
	}
	while (moveIndex > index) {
		list->score[moveIndex] = list->score[moveIndex - 1];
		list->objIdx[moveIndex] = list->objIdx[moveIndex - 1];
		--moveIndex;
	}
	list->score[index] = proximityScore;
	list->objIdx[index] = candidateObjIdx;
	++list->count;
}

// FUNCTION: XVT 0x41BA60
int collide_GetMobileObjectProximitySpeedQ12(uint16_t objIdx) {
	MobileObject* mobileObject;
	int speed;
	uint16_t currentSpeed;

	mobileObject = g_objectTable[objIdx].mobj;
	if (mobileObject == NULL)
		return 0;

	switch (mobileObject->state) {
		case 0:
			speed = g_modelDefs[GetModelIndexFromType(g_objectTable[objIdx].objectType)].maxSpeed;
			mobileObject = g_objectTable[objIdx].mobj;
			currentSpeed = mobileObject->speed;
			if (speed < (uint16_t)currentSpeed)
				speed = currentSpeed;
			break;

		case 1:
			if (mobileObject->pWarheadGuidance != NULL)
				return mobileObject->pWarheadGuidance->minSpeed << 12;
			speed = mobileObject->speed;
			break;

		default:
			return 0;
	}

	return speed << 12;
}

// FUNCTION: XVT 0x41BB00
void collide_ResetObjectProximityForSlot(uint16_t objIdx) {
	MobileObject* mobileObject;
	int ownerObjIdx;
	int objectIndex;
	ObjectRecord* object;

	mobileObject = g_objectTable[objIdx].mobj;
	if (mobileObject != NULL) {
		mobileObject->proximityList.overflowScore = 0;
		g_objectTable[objIdx].mobj->proximityList.count = 0;
		return;
	}

	ownerObjIdx = g_activeRegionObjectSlotStart;
	if (g_activeRegionCraftObjectSlotEnd <= ownerObjIdx) {
		return;
	}
	objectIndex = g_activeRegionObjectSlotStart;
	do {
		object = &g_objectTable[objectIndex];
		if (object->objectType != 0 && object->playerOwnerIdx != -1) {
			mobileObject = object->mobj;
			if (mobileObject != NULL) {
				collide_InsertMobileObjectProximityCandidate(&mobileObject->proximityList,
															 (uint16_t)ownerObjIdx, objIdx);
			}
		}
		++objectIndex;
		++ownerObjIdx;
	} while (ownerObjIdx < g_activeRegionCraftObjectSlotEnd);
}

// FUNCTION: XVT 0x41BBA0
void collide_ResetNeighborProximityLists(uint16_t objectIndex) {
	MobileObject* mobileObject;
	int count;
	int proximityIndex;

	mobileObject = g_objectTable[objectIndex].mobj;
	if (mobileObject == NULL) {
		return;
	}

	count = mobileObject->proximityList.count;
	if (count <= 0) {
		return;
	}

	proximityIndex = 0;
	do {
		collide_ResetObjectProximityForSlot(
			g_objectTable[objectIndex].mobj->proximityList.objIdx[proximityIndex]);
		++proximityIndex;
		--count;
	} while (count != 0);
}

// FUNCTION: XVT 0x41BBF0
void collide_RemoveMobileObjectProximityCandidate(MobileObjectProximityList* list, uint16_t candidateObjIdx) {
	int index;

	index = 0;
	if (list->count != 0) {
		do {
			if (list->objIdx[index] == candidateObjIdx) {
				break;
			}
			++index;
		} while (index < list->count);
	}
	if (index == list->count) {
		return;
	}
	++index;
	if (index < list->count) {
		do {
			list->objIdx[index - 1] = list->objIdx[index];
			list->score[index - 1] = list->score[index];
			++index;
		} while (list->count > index);
	}
	--list->count;
}

// FUNCTION: XVT 0x41BC50
void collide_applyCraftImpactBounce(uint16_t craftObjIdx, uint16_t otherObjIdx) {
	CraftData* craft;
	int16_t speed;
	int16_t angle;
	int impulseX;
	int impulseY;
	int deltaX;
	int deltaY;
	int deltaZ;
	int distanceSquared;
	uint16_t savedYaw;
	int16_t forceX;
	int16_t forceY;
	int16_t forceZ;
	int16_t moveX;
	int16_t moveY;

	if (g_objectTable[craftObjIdx].genusId != CRAFT_GENUS_STARFIGHTER &&
		g_objectTable[craftObjIdx].genusId != CRAFT_GENUS_TRANSPORT &&
		g_objectTable[craftObjIdx].genusId != CRAFT_GENUS_UTILITY_VEHICLE)
		return;

	craft = g_objectTable[craftObjIdx].mobj->pCraft;
	craft->aiFlight.impactObjIdx = otherObjIdx;
	speed = (int16_t)g_objectTable[craftObjIdx].mobj->speed;
	angle = (int16_t)(g_objectTable[otherObjIdx].yaw - g_objectTable[craftObjIdx].yaw);
	if ((uint16_t)angle >= 0x8000)
		angle = (int16_t)-angle;
	if ((uint16_t)angle > 0x4000)
		angle = (int16_t)(0x8000 - angle);
	if (g_activeRegionCraftObjectSlotEnd > otherObjIdx) {
		if ((uint16_t)angle < 0x4000)
			speed = (int16_t)(speed - trig2_cosinewordmult(g_objectTable[otherObjIdx].mobj->speed, angle));
		else
			speed = (int16_t)(speed + trig2_cosinewordmult(g_objectTable[otherObjIdx].mobj->speed, angle));
	}
	if (speed < 0)
		speed = (int16_t)-speed;

	if (g_objectTable[craftObjIdx].mobj != NULL && g_objectTable[otherObjIdx].mobj != NULL) {
		deltaX = g_objectTable[craftObjIdx].mobj->prevWorldX - g_objectTable[otherObjIdx].mobj->prevWorldX;
		deltaY = g_objectTable[craftObjIdx].mobj->prevWorldY - g_objectTable[otherObjIdx].mobj->prevWorldY;
		impulseX = deltaX;
		impulseY = deltaY;
		deltaZ = g_objectTable[craftObjIdx].mobj->prevWorldZ - g_objectTable[otherObjIdx].mobj->prevWorldZ;
	} else {
		deltaX = g_objectTable[craftObjIdx].world_x - g_objectTable[otherObjIdx].world_x;
		deltaY = g_objectTable[craftObjIdx].world_y - g_objectTable[otherObjIdx].world_y;
		impulseX = deltaX;
		impulseY = deltaY;
		deltaZ = g_objectTable[craftObjIdx].world_z - g_objectTable[otherObjIdx].world_z;
	}
	distanceSquared = deltaZ * deltaZ + impulseY * impulseY + deltaX * deltaX;
	if (distanceSquared > 50) {
		impulseX = 1000 * speed * impulseX;
		forceX = (int16_t)(impulseX / distanceSquared);
		impulseY = 1000 * speed * impulseY;
		forceY = (int16_t)(impulseY / distanceSquared);
		forceZ = (int16_t)(1000 * speed * deltaZ / distanceSquared);
	} else {
		forceX = 0;
		forceY = 100;
	}

	if (g_objectTable[craftObjIdx].mobj->moveVectorDirty != 0)
		FVIEW_calcrotatemove(g_objectTable[craftObjIdx].pitch, g_objectTable[craftObjIdx].yaw,
							 &g_objectTable[craftObjIdx]);
	moveX = g_objectTable[craftObjIdx].mobj->moveX;
	forceX = (int16_t)(forceX + moveX);
	if (((forceX ^ moveX) & 0x8000) != 0)
		forceX = moveX;
	moveY = g_objectTable[craftObjIdx].mobj->moveY;
	forceY = (int16_t)(forceY + moveY);
	if (((forceY ^ moveY) & 0x8000) != 0)
		forceY = moveY;
	savedYaw = g_objectTable[craftObjIdx].yaw;
	g_objectTable[craftObjIdx].yaw = trig2_arctan(forceX, forceY);

	if (otherObjIdx >= g_projectileObjectSlotStart && otherObjIdx < g_projectileObjectSlotEnd) {
		int16_t yawStep = (int16_t)(8 * g_objectTable[otherObjIdx].mobj->speed);
		if (savedYaw > g_objectTable[craftObjIdx].yaw)
			yawStep = (int16_t)-yawStep;
		g_objectTable[craftObjIdx].yaw = (int16_t)(g_objectTable[craftObjIdx].yaw + yawStep);
	}

	if (g_activeRegionCraftObjectSlotEnd > otherObjIdx) {
		CraftData* otherCraft = g_objectTable[otherObjIdx].mobj->pCraft;
		int16_t rollImpulse;

		g_objectTable[otherObjIdx].pitch = trig2_w_arccos(forceX);
		otherCraft->pitch = g_objectTable[otherObjIdx].pitch;
		rollImpulse = (int16_t)(speed * 100);
		if ((uint16_t)rollImpulse >= 0x8000)
			rollImpulse = 0x7FFF;
		if (g_objectTable[otherObjIdx].yaw > savedYaw)
			rollImpulse = (int16_t)-rollImpulse;
		g_objectTable[otherObjIdx].mobj->rollImpulseRate = rollImpulse;
		FVIEW_calcrotatemove(g_objectTable[otherObjIdx].pitch, g_objectTable[otherObjIdx].yaw,
							 &g_objectTable[otherObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[otherObjIdx].roll, 0, &g_objectTable[otherObjIdx]);
	}

	if (g_activeRegionCraftObjectSlotEnd > otherObjIdx) {
		int16_t bounceX;
		int16_t bounceY;

		g_objectTable[otherObjIdx].mobj->pCraft->aiFlight.impactObjIdx = craftObjIdx;
		distanceSquared = deltaZ * deltaZ + deltaY * deltaY + deltaX * deltaX;
		if (distanceSquared > 50) {
			bounceX = (int16_t)(1000 * speed * impulseX / distanceSquared);
			bounceY = (int16_t)(1000 * speed * impulseY / distanceSquared);
		} else {
			bounceX = 0;
			bounceY = 100;
		}
		if (g_objectTable[otherObjIdx].mobj->moveVectorDirty != 0)
			FVIEW_calcrotatemove(g_objectTable[otherObjIdx].pitch, g_objectTable[otherObjIdx].yaw,
								 &g_objectTable[otherObjIdx]);
		bounceX = (int16_t)(bounceX + g_objectTable[otherObjIdx].mobj->moveX);
		bounceY = (int16_t)(bounceY + g_objectTable[otherObjIdx].mobj->moveY);
		savedYaw = g_objectTable[otherObjIdx].yaw;
		g_objectTable[otherObjIdx].yaw = trig2_arctan(bounceX, bounceY);
		g_objectTable[craftObjIdx].pitch = trig2_w_arccos(bounceX);
		craft->pitch = g_objectTable[craftObjIdx].pitch;
	}

	speed = (int16_t)(speed * 100);
	if ((uint16_t)speed >= 0x8000)
		speed = 0x7FFF;
	if (g_objectTable[craftObjIdx].yaw > savedYaw)
		speed = (int16_t)-speed;
	g_objectTable[craftObjIdx].mobj->rollImpulseRate = speed;
	FVIEW_calcrotatemove(g_objectTable[craftObjIdx].pitch, g_objectTable[craftObjIdx].yaw,
						 &g_objectTable[craftObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[craftObjIdx].roll, 0, &g_objectTable[craftObjIdx]);

	if (otherObjIdx < g_projectileObjectSlotEnd) {
		FVIEW_calcrotatemove(g_objectTable[otherObjIdx].pitch, g_objectTable[otherObjIdx].yaw,
							 &g_objectTable[otherObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[otherObjIdx].roll, 0, &g_objectTable[otherObjIdx]);
	}

	fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_1, craftObjIdx, g_objectTable[craftObjIdx].playerOwnerIdx);
	if (g_activeRegionCraftObjectSlotEnd > otherObjIdx)
		fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_2, otherObjIdx, g_objectTable[craftObjIdx].playerOwnerIdx);
}

// FUNCTION: XVT 0x41C1D0
int16_t collide_lasercraftcollide(uint16_t sourceObjIdx, uint16_t targetObjIdx) {
	enum { COLLISION_MARGIN = 0x20000, LARGE_MODEL_EXTENT = 1095 };

	int maxDistance;
	int dx;
	int dy;
	int dz;
	unsigned int targetDistance;
	int sourceDistance;
	int useDetailedCollision = 1;
	int sweepDistance;
	ObjectRecord* target;
	ObjectRecord* source;
	unsigned int targetObjectType;
	int sourceObjectType;
	uint8_t sourceGenus;
	int maxExtent;

	maxDistance = g_modelTypeTable[g_objectTable[targetObjIdx].objectType].maxBoundsExtent + COLLISION_MARGIN;
	maxDistance += g_modelTypeTable[g_objectTable[sourceObjIdx].objectType].maxBoundsExtent;
	g_approxDist = maxDistance;
	dx = g_collisionProbeWorldX - g_collisionSweepEndX;

	if (dx < 0)
		dx = -dx;
	if (dx > maxDistance)
		return 0;
	dy = g_collisionProbeWorldY - g_collisionSweepEndY;
	if (dy < 0)
		dy = -dy;
	if (dy > maxDistance)
		return 0;
	dz = g_collisionProbeWorldZ - g_collisionSweepEndZ;
	if (dz < 0)
		dz = -dz;
	if (dz > maxDistance)
		return 0;
	targetDistance = collide_roughdistance3du((unsigned int)dx, (unsigned int)dy, (unsigned int)dz);
	g_approxDist = targetDistance;
	if ((int)targetDistance > maxDistance)
		return 0;

	dx = g_collisionProbeWorldX - g_collisionSegmentStartWorldX;
	if (dx < 0)
		dx = -dx;
	dy = g_collisionProbeWorldY - g_collisionSegmentStartWorldY;
	if (dy < 0)
		dy = -dy;
	dz = g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ;
	if (dz < 0)
		dz = -dz;
	sourceDistance = dx + dy + dz;

	sweepDistance = g_collisionSweepEndX - g_collisionSweepStartX;
	if (sweepDistance < 0)
		sweepDistance = -sweepDistance;
	dx += sweepDistance;
	sweepDistance = g_collisionSweepEndY - g_collisionSweepStartY;
	if (sweepDistance < 0)
		sweepDistance = -sweepDistance;
	dy += sweepDistance;
	sweepDistance = g_collisionSweepEndZ - g_collisionSweepStartZ;
	if (sweepDistance < 0)
		sweepDistance = -sweepDistance;
	dz += sweepDistance;

	target = &g_objectTable[targetObjIdx];
	targetObjectType = target->objectType;
	maxExtent = g_modelTypeTable[targetObjectType].maxBoundsExtent;
	if (target->genusId == CRAFT_GENUS_OTHER_PROJECTILE || target->genusId == CRAFT_GENUS_PLAYER_PROJECTILE) {
		if (target->objectType != COUNTERMEASURE_PROJECTILE_OBJECT_TYPE)
			maxExtent >>= 1;
	} else if (target->genusId == CRAFT_GENUS_STARFIGHTER) {
		source = &g_objectTable[sourceObjIdx];
		sourceGenus = source->genusId;
		if (sourceGenus == CRAFT_GENUS_OTHER_PROJECTILE || sourceGenus == CRAFT_GENUS_PLAYER_PROJECTILE) {
			if (target->mobj->pCraft->shieldEnergy[0] + target->mobj->pCraft->shieldEnergy[1] != 0) {
				maxExtent += maxExtent >> 1;
				if (maxExtent > LARGE_MODEL_EXTENT)
					maxExtent = LARGE_MODEL_EXTENT - 1;
			}
			if (g_asyncFlag != 0 && target->playerOwnerIdx != -1 &&
				sourceGenus == CRAFT_GENUS_PLAYER_PROJECTILE) {
				sourceObjectType = source->objectType;
				if (g_projectileDamageByObjectType
						.warheadClass[sourceObjectType - PROJECTILE_OBJECT_TYPE_FIRST] == 0) {
					switch (targetObjectType) {
						case MODEL_002_A_WING:
						case MODEL_004_TIE_FIGHTER:
							break;
						case MODEL_003_B_WING:
						case MODEL_006_TIE_BOMBER:
						case MODEL_008_TIE_DEFENDER:
							maxExtent *= 2;
							break;
						case MODEL_005_TIE_INTERCEPTOR:
							maxExtent += maxExtent / 4 + maxExtent / 2;
							break;
						default:
							maxExtent += maxExtent / 2;
							break;
					}
					useDetailedCollision = 0;
				}
			}
		}
	}
	if (collide_roughdistance3du((unsigned int)(maxExtent + dx), (unsigned int)(maxExtent + dy),
								 (unsigned int)(maxExtent + dz)) < targetDistance)
		return 0;
	if ((useDetailedCollision != 0 && maxExtent >= LARGE_MODEL_EXTENT) ||
		target->objectType == MODEL_058_CONTAINER_I) {
		if (g_collisionStagedModelProbe != 0) {
			g_collisionStagedModelProbe = 0;
			if (target->mobj->speed < 40)
				return (int16_t)collide_CheckSweptModelCollision(sourceObjIdx, targetObjIdx);
		} else {
			if (sourceDistance == 0)
				return 0;
			return (int16_t)collide_CheckSweptModelCollision(sourceObjIdx, targetObjIdx);
		}
	}
	maxExtent >>= 2;
	return (int16_t)collide_checkboxcollision(maxExtent + (maxExtent >> 1));
}

// FUNCTION: XVT 0x41C570
int16_t collide_checkboxcollision(int radius) {
	int slope;
	uint16_t scaleQ15;
	int tEnter;
	int tExit;
	int tCandidate;
	int endRel;
	int sweepDeltaX;
	int sweepDeltaY;
	int sweepDeltaZ;
	int probeDeltaX;
	int probeDeltaY;
	int probeDeltaZ;
	int startRelX;
	int startRelY;
	int startRelZ;
	int deltaX;
	int deltaY;
	int deltaZ;
	int xFarNumerator;
	int yFarNumerator;
	int yNearNumerator;
	int zFarNumerator;
	int zNearNumerator;

	probeDeltaX = g_collisionProbeWorldX - g_collisionSegmentStartWorldX;
	probeDeltaY = g_collisionProbeWorldY - g_collisionSegmentStartWorldY;
	probeDeltaZ = g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ;
	sweepDeltaX = g_collisionSweepEndX - g_collisionSweepStartX;
	sweepDeltaY = g_collisionSweepEndY - g_collisionSweepStartY;
	sweepDeltaZ = g_collisionSweepEndZ - g_collisionSweepStartZ;

	startRelX = g_collisionSweepStartX - g_collisionSegmentStartWorldX;
	if (startRelX > radius) {
		xFarNumerator = radius - startRelX;
		deltaX = sweepDeltaX - probeDeltaX;
		if (deltaX >= 0)
			return 0;
		if (xFarNumerator < deltaX)
			return 0;
		tExit = -(startRelX + radius);
	} else if (startRelX < -radius) {
		tExit = -(startRelX + radius);
		deltaX = sweepDeltaX - probeDeltaX;
		if (deltaX < 0)
			return 0;
		if (tExit >= deltaX)
			return 0;
		xFarNumerator = radius - startRelX;
	} else {
		xFarNumerator = radius - startRelX;
		tExit = -(startRelX + radius);
		deltaX = 0;
	}

	startRelY = g_collisionSweepStartY - g_collisionSegmentStartWorldY;
	if (startRelY > radius) {
		yFarNumerator = radius - startRelY;
		deltaY = sweepDeltaY - probeDeltaY;
		if (deltaY >= 0)
			return 0;
		if (yFarNumerator < deltaY)
			return 0;
		yNearNumerator = -(startRelY + radius);
	} else if (startRelY < -radius) {
		yNearNumerator = -(startRelY + radius);
		deltaY = sweepDeltaY - probeDeltaY;
		if (deltaY < 0)
			return 0;
		if (yNearNumerator >= deltaY)
			return 0;
		yFarNumerator = radius - startRelY;
	} else {
		deltaY = 0;
		yFarNumerator = radius - startRelY;
		yNearNumerator = -(startRelY + radius);
	}

	startRelZ = g_collisionSweepStartZ - g_collisionSegmentStartWorldZ;
	if (startRelZ > radius) {
		zFarNumerator = radius - startRelZ;
		deltaZ = sweepDeltaZ - probeDeltaZ;
		if (deltaZ >= 0)
			return 0;
		if (zFarNumerator < deltaZ)
			return 0;
		zNearNumerator = -(startRelZ + radius);
	} else if (startRelZ < -radius) {
		zNearNumerator = -(startRelZ + radius);
		deltaZ = sweepDeltaZ - probeDeltaZ;
		if (deltaZ < 0)
			return 0;
		if (zNearNumerator >= deltaZ)
			return 0;
		zFarNumerator = radius - startRelZ;
	} else {
		deltaZ = 0;
		zFarNumerator = radius - startRelZ;
		zNearNumerator = -(startRelZ + radius);
	}

	xFarNumerator = (int32_t)((uint32_t)xFarNumerator << 8);
	tExit = (int32_t)((uint32_t)tExit << 8);
	yFarNumerator = (int32_t)((uint32_t)yFarNumerator << 8);
	yNearNumerator = (int32_t)((uint32_t)yNearNumerator << 8);
	zFarNumerator = (int32_t)((uint32_t)zFarNumerator << 8);
	zNearNumerator = (int32_t)((uint32_t)zNearNumerator << 8);

	if (deltaX == 0) {
		slope = sweepDeltaX - probeDeltaX;
		endRel = g_collisionSweepEndX - g_collisionProbeWorldX;
		tEnter = 0;
		if (endRel > radius)
			tExit = xFarNumerator / slope;
		else if (endRel < -radius)
			tExit = tExit / slope;
		else
			tExit = 255;
	} else {
		tEnter = xFarNumerator / deltaX;
		tExit = tExit / deltaX;
		if (deltaX >= 0) {
			tCandidate = tEnter;
			tEnter = tExit;
			tExit = tCandidate;
		}
	}

	if (deltaY == 0) {
		slope = sweepDeltaY - probeDeltaY;
		endRel = g_collisionSweepEndY - g_collisionProbeWorldY;
		if (endRel > radius)
			tCandidate = yFarNumerator / slope;
		else if (endRel < -radius)
			tCandidate = yNearNumerator / slope;
		else
			tCandidate = 255;
		if (deltaY > tExit)
			return 0;
		if (deltaY > tEnter)
			tEnter = deltaY;
		if (tCandidate < tEnter)
			return 0;
		if (tCandidate < tExit)
			tExit = tCandidate;
	} else {
		tCandidate = yFarNumerator / deltaY;
		if (deltaY >= 0) {
			if (tCandidate < tEnter)
				return 0;
			if (tCandidate < tExit)
				tExit = tCandidate;
			tCandidate = yNearNumerator / deltaY;
			if (tCandidate > tExit)
				return 0;
			if (tCandidate > tEnter)
				tEnter = tCandidate;
		} else {
			if (tCandidate > tExit)
				return 0;
			if (tCandidate > tEnter)
				tEnter = tCandidate;
			tCandidate = yNearNumerator / deltaY;
			if (tCandidate < tEnter)
				return 0;
			if (tCandidate < tExit)
				tExit = tCandidate;
		}
	}

	if (deltaZ == 0) {
		slope = sweepDeltaZ - probeDeltaZ;
		endRel = g_collisionSweepEndZ - g_collisionProbeWorldZ;
		if (endRel > radius)
			tCandidate = zFarNumerator / slope;
		else if (endRel < -radius)
			tCandidate = zNearNumerator / slope;
		else
			tCandidate = 255;
		if (deltaZ > tExit)
			return 0;
		if (deltaZ > tEnter)
			tEnter = deltaZ;
		if (tCandidate < tEnter)
			return 0;
	} else {
		tCandidate = zFarNumerator / deltaZ;
		if (deltaZ >= 0) {
			if (tCandidate < tEnter)
				return 0;
			if (tCandidate < tExit)
				tExit = tCandidate;
			tCandidate = zNearNumerator / deltaZ;
			if (tCandidate > tExit)
				return 0;
			if (tCandidate > tEnter)
				tEnter = tCandidate;
		} else {
			if (tCandidate > tExit)
				return 0;
			if (tCandidate > tEnter)
				tEnter = tCandidate;
			tCandidate = zNearNumerator / deltaZ;
			if (tCandidate < tEnter)
				return 0;
		}
	}

	if (tEnter > 255)
		return 0;

	scaleQ15 = (uint16_t)((uint16_t)tEnter << 7);
	g_collisionHitOffsetX = Math_MulQ15((int)scaleQ15, probeDeltaX);
	g_collisionHitOffsetY = Math_MulQ15((int)scaleQ15, probeDeltaY);
	g_collisionHitOffsetZ = Math_MulQ15((int)scaleQ15, probeDeltaZ);
	return -1;
}

// FUNCTION: XVT 0x41CA60
int collide_targetinrange(uint16_t sourceObjIdx, uint16_t targetObjIdx, uint16_t hardpointIndex) {
	enum {
		CHARGED_PROJECTILE_THRESHOLD = 64,
		PROJECTILE_SPEED_SCALE = 4660,
		PROJECTILE_SPEED_ROUNDING = 128,
		PROJECTILE_SPEED_SHIFT = 8,
		MOVE_VECTOR_SHIFT = 15,
	};

	CollisionTargetRangeScratch savedCollision;
	ObjectRecord* sourceObject;
	CraftData* sourceCraft;
	MobileObject* sourceMobileObject;
	ModelIndex sourceModelIndex;
	uint16_t projectileType;
	uint16_t projectileSpeed;
	int lifetimeTicks;
	int projectileDistance;
	ObjectRecord* targetObject;
	int result;
	int sourceMoveX;
	int sourceMoveY;
	int sourceMoveZ;
	int targetMoveX;
	int targetMoveY;
	int targetMoveZ;

	savedCollision.segmentStartWorldX = g_collisionSegmentStartWorldX;
	savedCollision.segmentStartWorldY = g_collisionSegmentStartWorldY;
	savedCollision.segmentStartWorldZ = g_collisionSegmentStartWorldZ;
	savedCollision.probeWorldX = g_collisionProbeWorldX;
	savedCollision.probeWorldY = g_collisionProbeWorldY;
	savedCollision.probeWorldZ = g_collisionProbeWorldZ;
	savedCollision.sweepStartX = g_collisionSweepStartX;
	savedCollision.sweepStartY = g_collisionSweepStartY;
	savedCollision.sweepStartZ = g_collisionSweepStartZ;
	savedCollision.sweepEndX = g_collisionSweepEndX;
	savedCollision.sweepEndY = g_collisionSweepEndY;
	savedCollision.sweepEndZ = g_collisionSweepEndZ;
	savedCollision.hitOffsetX = g_collisionHitOffsetX;
	savedCollision.hitOffsetY = g_collisionHitOffsetY;
	savedCollision.hitOffsetZ = g_collisionHitOffsetZ;

	sourceObject = &g_objectTable[sourceObjIdx];
	sourceCraft = sourceObject->mobj->pCraft;
	sourceModelIndex = sourceCraft->modelIndex;
	projectileType =
		g_modelDefs[sourceModelIndex].laserGroupWeaponType[g_players[g_localPlayer].selectedWarhead];
	if (sourceCraft->weaponSlots[hardpointIndex].laserCharge >= CHARGED_PROJECTILE_THRESHOLD)
		++projectileType;
	projectileSpeed = g_projectileDamageByObjectType.speed[projectileType - PROJECTILE_OBJECT_TYPE_FIRST];
	lifetimeTicks =
		SIMULATION_TICKS_PER_SECOND *
		g_projectileDamageByObjectType.lifetimeSeconds[projectileType - PROJECTILE_OBJECT_TYPE_FIRST];
	lifetimeTicks += (uint16_t)MATH2_fraction(
		SIMULATION_TICKS_PER_SECOND,
		g_projectileDamageByObjectType.lifetimeFracQ16[projectileType - PROJECTILE_OBJECT_TYPE_FIRST]);

	g_collisionSegmentStartWorldX = sourceObject->world_x;
	g_collisionSegmentStartWorldY = sourceObject->world_y;
	g_collisionSegmentStartWorldZ = sourceObject->world_z;
	pai_calcrotatedpoint(sourceObject, g_modelDefs[sourceModelIndex].weaponHardpoints[hardpointIndex].x,
						 g_modelDefs[sourceModelIndex].weaponHardpoints[hardpointIndex].z,
						 g_modelDefs[sourceModelIndex].weaponHardpoints[hardpointIndex].y);
	if (sourceObject->objectType == CRAFT_SPECIES_IMPERIAL_STAR_DESTROYER) {
		g_rotatedX *= 2;
		g_rotatedY *= 2;
		g_rotatedZ *= 2;
	}
	g_collisionSegmentStartWorldX += g_rotatedX;
	g_collisionSegmentStartWorldY += g_rotatedY;
	g_collisionSegmentStartWorldZ += g_rotatedZ;

	sourceMobileObject = sourceObject->mobj;
	projectileDistance = lifetimeTicks *
						 ((PROJECTILE_SPEED_SCALE * (projectileSpeed + sourceMobileObject->speed) +
						   PROJECTILE_SPEED_ROUNDING) >>
						  PROJECTILE_SPEED_SHIFT) /
						 SIMULATION_TICKS_PER_SECOND;
	if (sourceMobileObject->moveVectorDirty != 0)
		FVIEW_calcrotatemove(sourceObject->pitch, sourceObject->yaw, sourceObject);
	sourceMoveX = sourceObject->mobj->moveX;
	sourceMoveX = Math_MulQ15(sourceMoveX, projectileDistance);
	g_collisionProbeWorldX = g_collisionSegmentStartWorldX + sourceMoveX;
	sourceMoveY = sourceObject->mobj->moveY;
	sourceMoveY = Math_MulQ15(sourceMoveY, projectileDistance);
	g_collisionProbeWorldY = g_collisionSegmentStartWorldY + sourceMoveY;
	sourceMoveZ = sourceObject->mobj->moveZ;
	sourceMoveZ = Math_MulQ15(sourceMoveZ, projectileDistance);
	g_collisionProbeWorldZ = g_collisionSegmentStartWorldZ + sourceMoveZ;

	targetObject = &g_objectTable[targetObjIdx];
	if (targetObject->mobj != NULL) {
		MobileObject** targetMobileObjectLink = &targetObject->mobj;
		int targetDistance;

		g_collisionSweepStartX = targetObject->world_x;
		g_collisionSweepStartY = targetObject->world_y;
		g_collisionSweepStartZ = targetObject->world_z;
		if (g_players[g_localPlayer].selectedTargetComponent != 0) {
			uint16_t componentIndex = (uint16_t)g_players[g_localPlayer].selectedTargetComponent;
			int modelType = targetObject->objectType;

			pai_RotateLocalVectorToWorldScratch(targetObject, ModelMesh_GetCenterX(modelType, componentIndex),
												ModelMesh_GetCenterZ(modelType, componentIndex),
												-ModelMesh_GetCenterY(modelType, componentIndex));
			g_collisionSweepStartX += g_rotatedX;
			g_collisionSweepStartY += g_rotatedY;
			g_collisionSweepStartZ += g_rotatedZ;
		}

		targetDistance =
			lifetimeTicks *
			((PROJECTILE_SPEED_SCALE * (*targetMobileObjectLink)->speed + PROJECTILE_SPEED_ROUNDING) >>
			 PROJECTILE_SPEED_SHIFT) /
			SIMULATION_TICKS_PER_SECOND;
		if ((*targetMobileObjectLink)->moveVectorDirty != 0)
			FVIEW_calcrotatemove(targetObject->pitch, targetObject->yaw, targetObject);
		targetMoveX = (*targetMobileObjectLink)->moveX;
		targetMoveX = Math_MulQ15(targetMoveX, targetDistance);
		g_collisionSweepEndX = g_collisionSweepStartX + targetMoveX;
		targetMoveY = (*targetMobileObjectLink)->moveY;
		targetMoveY = Math_MulQ15(targetMoveY, targetDistance);
		g_collisionSweepEndY = g_collisionSweepStartY + targetMoveY;
		targetMoveZ = (*targetMobileObjectLink)->moveZ;
		targetMoveZ = Math_MulQ15(targetMoveZ, targetDistance);
		g_collisionSweepEndZ = g_collisionSweepStartZ + targetMoveZ;
		g_collisionStagedModelProbe = 1;
		result = (uint16_t)collide_lasercraftcollide(sourceObjIdx, targetObjIdx);
	} else {
		g_collisionStagedModelProbe = 1;
		result = (int16_t)static_laserstaticcollide(sourceObjIdx, targetObjIdx);
	}

	g_collisionSegmentStartWorldX = savedCollision.segmentStartWorldX;
	g_collisionSegmentStartWorldY = savedCollision.segmentStartWorldY;
	g_collisionSegmentStartWorldZ = savedCollision.segmentStartWorldZ;
	g_collisionProbeWorldX = savedCollision.probeWorldX;
	g_collisionProbeWorldY = savedCollision.probeWorldY;
	g_collisionProbeWorldZ = savedCollision.probeWorldZ;
	g_collisionSweepStartX = savedCollision.sweepStartX;
	g_collisionSweepStartY = savedCollision.sweepStartY;
	g_collisionSweepStartZ = savedCollision.sweepStartZ;
	g_collisionSweepEndX = savedCollision.sweepEndX;
	g_collisionSweepEndY = savedCollision.sweepEndY;
	g_collisionSweepEndZ = savedCollision.sweepEndZ;
	g_collisionHitOffsetX = savedCollision.hitOffsetX;
	g_collisionHitOffsetY = savedCollision.hitOffsetY;
	g_collisionHitOffsetZ = savedCollision.hitOffsetZ;
	g_collisionStagedModelProbe = 0;
	return result;
}

// FUNCTION: XVT 0x41CFD0
uint16_t collide_craftstarshipcollision(uint16_t sourceObjIdx, int16_t lookaheadSteps) {
	ObjectRecord* source = &g_objectTable[sourceObjIdx];
	int16_t lookahead = (int16_t)(g_simStepScale * lookaheadSteps);
	uint16_t movementStep;
	uint16_t objectIndex;
	MobileObjectProximityList* proximityList;

	g_collisionSegmentStartWorldX = source->world_x;
	g_collisionSegmentStartWorldY = source->world_y;
	g_collisionSegmentStartWorldZ = source->world_z;
	movementStep =
		(uint16_t)(g_elapsedTicks * ((4660 * source->mobj->speed + 128) >> 8) / SIMULATION_TICKS_PER_SECOND);
	if (source->mobj->moveVectorDirty != 0)
		FVIEW_calcrotatemove(source->pitch, source->yaw, source);
	g_collisionProbeWorldX =
		g_collisionSegmentStartWorldX + Math_MulQ15(source->mobj->moveX, (int)movementStep) * lookahead;
	g_collisionProbeWorldY =
		g_collisionSegmentStartWorldY + Math_MulQ15(source->mobj->moveY, (int)movementStep) * lookahead;
	g_collisionProbeWorldZ =
		g_collisionSegmentStartWorldZ + Math_MulQ15(source->mobj->moveZ, (int)movementStep) * lookahead;
	for (objectIndex = (uint16_t)g_activeRegionObjectSlotStart;
		 objectIndex < g_activeRegionCraftObjectSlotEnd; ++objectIndex) {
		ObjectRecord* candidate = &g_objectTable[objectIndex];
		if (candidate->objectType != 0 && objectIndex != sourceObjIdx &&
			(candidate->genusId == CRAFT_GENUS_STARSHIP || candidate->genusId == CRAFT_GENUS_PLATFORM ||
			 candidate->genusId == CRAFT_GENUS_FREIGHTER)) {
			uint16_t candidateStep;

			g_collisionSweepStartX = candidate->world_x;
			g_collisionSweepStartY = candidate->world_y;
			g_collisionSweepStartZ = candidate->world_z;
			candidateStep = (uint16_t)MATH2_mphconvert(candidate->mobj->speed, g_simStepScale);
			if (candidate->mobj->moveVectorDirty != 0)
				FVIEW_calcrotatemove(candidate->pitch, candidate->yaw, candidate);
			g_collisionSweepEndX =
				g_collisionSweepStartX + Math_MulQ15(candidate->mobj->moveX, (int)candidateStep) * lookahead;
			g_collisionSweepEndY =
				g_collisionSweepStartY + Math_MulQ15(candidate->mobj->moveY, (int)candidateStep) * lookahead;
			g_collisionSweepEndZ =
				g_collisionSweepStartZ + Math_MulQ15(candidate->mobj->moveZ, (int)candidateStep) * lookahead;
			if (collide_lasercraftcollide(sourceObjIdx, objectIndex) != 0)
				return objectIndex;
		}
	}
	proximityList = &g_objectTable[sourceObjIdx].mobj->proximityList;
	for (objectIndex = 0; objectIndex < proximityList->count; ++objectIndex) {
		uint16_t candidateIndex = proximityList->objIdx[objectIndex];
		if (candidateIndex >= g_regionMainObjectSlotEnd &&
			candidateIndex < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd &&
			static_laserstaticcollide(sourceObjIdx, candidateIndex) != 0)
			return candidateIndex;
	}
	return UINT16_MAX;
}

// FUNCTION: XVT 0x41D320
void collide_laserhitcraft(uint16_t otherObjIdx, uint16_t craftObjIdx, int16_t hitMeshIndex) {
	enum {
		ATTACK_COUNT_MASK = 0x70,
		ATTACK_COUNT_SHIFT = 4,
		ATTACK_COUNT_MAX = 7,
		ATTACK_COUNT_LASER_LIMIT = 5,
		ATTACK_COUNT_WARHEAD_INCREMENT = 4,
		ATTACKED_GOAL_SCORED_MASK = 0x80,
		ATTACKED_PRESERVE_MASK = 0x8F,
		GOAL_EVENT_ATTACKED = 3,
		GOAL_SCORE_DIVISOR = 1,
		MAGNETIC_PULSE_SHORT_INHIBIT_TICKS = 3540,
		MAGNETIC_PULSE_LONG_INHIBIT_TICKS = 7080,
		LASER_SYSTEM_MESSAGE_ARG = 92,
		SYSTEM_FAILED_MESSAGE_ARG = 87,
		EXPLOSION_GENUS = 13,
		EXPLOSION_STATE = 5,
		IMPACT_EFFECT_SUBTYPE = 2,
		WARHEAD_IMPACT_EFFECT_TYPE = 129,
		LASER_IMPACT_EFFECT_TYPE = 131,
		ION_IMPACT_EFFECT_TYPE = 132,
	};

	uint16_t sourceObjIdx = g_objectTable[otherObjIdx].mobj->sourceObjIdx;
	uint16_t attackerTeam = g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx].fg.team;
	CraftData* craft;
	int8_t* attackedByTeam;
	int16_t forwardDot;
	uint16_t forwardPositive;
	uint16_t hitSide;
	int8_t hitRegistered;
	uint8_t projectileObjectType;

	if (sourceObjIdx == craftObjIdx)
		return;

	craft = g_objectTable[craftObjIdx].mobj->pCraft;
	attackedByTeam = &craft->attackedByTeam[attackerTeam];
	if (*attackedByTeam == 0) {
		*attackedByTeam |= 1;
		++g_missionFgStats[g_objectTable[craftObjIdx].flightGroupIdx]
			  .outcomeCount[FLIGHT_GROUP_OUTCOME_ATTACKED];
		if (g_missionFlightGroups[g_objectTable[craftObjIdx].flightGroupIdx].fg.specialCargoCraft ==
			craft->waveNumber)
			g_missionFgStats[g_objectTable[craftObjIdx].flightGroupIdx]
				.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_ATTACKED] = 1;

		if (g_gameConfig.voiceTacticalOfficerEnabled == 2 &&
			g_missionFlightGroups[g_objectTable[craftObjIdx].flightGroupIdx].fg.team ==
				(uint16_t)g_players[g_localPlayer].playerIff &&
			g_objectTable[craftObjIdx].genusId != CRAFT_GENUS_STARFIGHTER) {
			if (sourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
				int projectileType = g_objectTable[otherObjIdx].objectType;

				if (projectileType == WARHEAD_OBJECT_TYPE_CONCUSSION_MISSILE ||
					projectileType == WARHEAD_OBJECT_TYPE_ADVANCED_CONCUSSION_MISSILE) {
					fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_MISSILE_ATTACKER,
												   craftObjIdx, UINT16_MAX);
				} else if (projectileType == WARHEAD_OBJECT_TYPE_PROTON_TORPEDO ||
						   projectileType == WARHEAD_OBJECT_TYPE_ADVANCED_PROTON_TORPEDO) {
					fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_TORPEDO_ATTACKER,
												   craftObjIdx, UINT16_MAX);
				} else if (projectileType == WARHEAD_OBJECT_TYPE_HEAVY_ROCKET) {
					fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_ROCKET_ATTACKER,
												   craftObjIdx, UINT16_MAX);
				} else if (projectileType == WARHEAD_OBJECT_TYPE_SPACE_BOMB) {
					fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_SPACE_BOMB_ATTACKER,
												   craftObjIdx, UINT16_MAX);
				} else {
					if (g_objectTable[sourceObjIdx].genusId == CRAFT_GENUS_STARFIGHTER)
						fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS,
													   TACTICAL_MSG_STARFIGHTER_ATTACKER, craftObjIdx,
													   UINT16_MAX);
					else if (g_objectTable[sourceObjIdx].genusId == CRAFT_GENUS_STARSHIP)
						fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_STARSHIP_ATTACKER,
													   craftObjIdx, UINT16_MAX);
					else
						fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_UNKNOWN_ATTACKER,
													   craftObjIdx, UINT16_MAX);
				}
			} else {
				fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_UNKNOWN_ATTACKER,
											   craftObjIdx, UINT16_MAX);
			}
		}
	}

	{
		int sourcePlayerIdx = g_objectTable[sourceObjIdx].playerOwnerIdx;

		if (sourcePlayerIdx != -1) {
			if (((uint8_t)*attackedByTeam & ATTACKED_GOAL_SCORED_MASK) == 0) {
				uint16_t specialCargoFlag =
					g_missionFlightGroups[g_objectTable[craftObjIdx].flightGroupIdx].fg.specialCargoCraft ==
					craft->waveNumber;
				uint16_t sourceTeam = (uint16_t)g_players[sourcePlayerIdx].playerIff;

				Mission_ApplyFlightGroupGoalScore(GOAL_EVENT_ATTACKED,
												  g_objectTable[craftObjIdx].flightGroupIdx, sourcePlayerIdx,
												  GOAL_SCORE_DIVISOR, specialCargoFlag, sourceTeam);
				Mission_ApplyFlightGroupGoalScore(
					GOAL_EVENT_ATTACKED, g_objectTable[craftObjIdx].flightGroupIdx, -1, GOAL_SCORE_DIVISOR,
					specialCargoFlag,
					(uint16_t)g_players[g_objectTable[sourceObjIdx].playerOwnerIdx].playerIff);
			}
			*attackedByTeam |= ATTACKED_GOAL_SCORED_MASK;
		}
	}

	if (craft->lastAttackerObjIdx == UINT16_MAX) {
		if (sourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
			ObjectRecord* sourceObject = &g_objectTable[sourceObjIdx];
			uint8_t sourceGenus = sourceObject->genusId;

			if (sourceGenus != CRAFT_GENUS_STARSHIP && sourceGenus != CRAFT_GENUS_PLATFORM &&
				sourceGenus != CRAFT_GENUS_FREIGHTER) {
				uint8_t recordAttacker = 1;

				if (sourceObject->playerOwnerIdx != -1) {
					int teamsHostile = sourceObject->mobj->team == g_objectTable[craftObjIdx].mobj->team
										   ? 0
										   : g_missionTeams[sourceObject->mobj->team]
													 .allies[g_objectTable[craftObjIdx].mobj->team] == 0;

					if (!teamsHostile && g_objectTable[craftObjIdx].genusId != CRAFT_GENUS_STARFIGHTER) {
						uint8_t attackCount =
							((uint8_t)*attackedByTeam & ATTACK_COUNT_MASK) >> ATTACK_COUNT_SHIFT;

						if (attackCount < ATTACK_COUNT_LASER_LIMIT) {
							attackCount += g_projectileDamageByObjectType
													   .warheadClass[g_objectTable[otherObjIdx].objectType -
																	 PROJECTILE_OBJECT_TYPE_FIRST] != 0
											   ? ATTACK_COUNT_WARHEAD_INCREMENT
											   : 0;
							if (attackCount > ATTACK_COUNT_MAX)
								attackCount = ATTACK_COUNT_MAX;
							*attackedByTeam = (int8_t)(((uint8_t)*attackedByTeam & ATTACKED_PRESERVE_MASK) |
													   (attackCount << ATTACK_COUNT_SHIFT));
						}
						if (attackCount < ATTACK_COUNT_MAX)
							recordAttacker = 0;
					}
				}
				if (recordAttacker == 1) {
					craft->lastAttackerObjIdx = sourceObjIdx;
					craft->lastHitTimestamp = (uint16_t)Mission_GameTimeToSeconds(
						g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
						g_missionElapsedClock.seconds);
				}
			}
		}
	} else if (g_objectTable[craftObjIdx].playerOwnerIdx != -1) {
		craft->lastAttackerObjIdx = sourceObjIdx;
		craft->lastHitTimestamp = (uint16_t)Mission_GameTimeToSeconds(
			g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
	}

	craft->aiFlight.threatObjIdx = sourceObjIdx;
	++craft->aiFlight.reactionTimer;
	if (g_objectTable[craftObjIdx].mobj->orientMatrixDirty != 0) {
		FVIEW_calcrotatemove(g_objectTable[craftObjIdx].pitch, g_objectTable[craftObjIdx].yaw,
							 &g_objectTable[craftObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[craftObjIdx].roll, 0, &g_objectTable[craftObjIdx]);
	}
	forwardDot = (int16_t)Math_Dot3Q15Wrapped(
		g_objectTable[craftObjIdx].mobj->cachedFwdX, g_objectTable[craftObjIdx].mobj->cachedFwdY,
		g_objectTable[craftObjIdx].mobj->cachedFwdZ,
		(int16_t)(g_collisionProbeWorldX - g_collisionSegmentStartWorldX),
		(int16_t)(g_collisionProbeWorldY - g_collisionSegmentStartWorldY),
		(int16_t)(g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ));
	forwardPositive = forwardDot >= 0;
	hitSide = 0;
	if (g_objectTable[craftObjIdx].playerOwnerIdx != -1)
		hitSide = forwardDot >= 0;

	if (g_flightMissionState.craftImpactBounceEnabled != 0 && craft->objectKind == CRAFT_OBJECT_KIND_ACTIVE &&
		g_projectileDamageByObjectType
				.warheadClass[g_objectTable[otherObjIdx].objectType - PROJECTILE_OBJECT_TYPE_FIRST] != 0)
		collide_applyCraftImpactBounce(craftObjIdx, otherObjIdx);

	projectileObjectType = g_objectTable[otherObjIdx].objectType;
	if (projectileObjectType != WARHEAD_OBJECT_TYPE_MAGNETIC_PULSE) {
		int chaffIntercepted = 0;

		if (craft->cmTypeId == COUNTERMEASURE_TYPE_CHAFF && craft->chaffActiveTimer != 0 &&
			g_projectileDamageByObjectType
					.warheadClass[projectileObjectType - PROJECTILE_OBJECT_TYPE_FIRST] != 0 &&
			forwardPositive) {
			chaffIntercepted = 1;
			msg_emitInFlightMessage(IFMSG_369_WARHEAD_SCATTERED_BY_CHAFF_NO_DAMAGE,
									g_objectTable[craftObjIdx].playerOwnerIdx);
		}
		if (chaffIntercepted == 0)
			hitRegistered = (uint8_t)collide_damagecraft(craftObjIdx, hitMeshIndex, otherObjIdx, hitSide);
		else
			hitRegistered = 0;
	} else {
		uint16_t previousInhibitTimer = (uint16_t)craft->weaponFireInhibitTimer;

		if (g_objectTable[craftObjIdx].playerOwnerIdx != -1) {
			uint16_t weaponSlotIndex;

			for (weaponSlotIndex = 0; weaponSlotIndex < craft->laserSlotCount; ++weaponSlotIndex)
				craft->weaponSlots[weaponSlotIndex].laserCharge = 0;
			if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_CANNONS) != 0) {
				craft->workingSubsystems &= CRAFT_SUBSYSTEM_FLAG_CANNONS ^ CRAFT_SUBSYSTEM_FLAGS_ALL;
				craft->systemHealth[DAMAGE_SYSTEM_03_CANNON_SYSTEM] = 0;
				craft->systemTimer[DAMAGE_SYSTEM_03_CANNON_SYSTEM] =
					g_subsystemRepairDuration[DAMAGE_SYSTEM_03_CANNON_SYSTEM];
				if (g_objectTable[craftObjIdx].playerOwnerIdx == g_localPlayer &&
					g_players[g_localPlayer].regionSessionId == 0) {
					g_msgArgTable[0] = LASER_SYSTEM_MESSAGE_ARG;
					g_msgArgTable[1] = SYSTEM_FAILED_MESSAGE_ARG;
					msg_emitInFlightMessage(IFMSG_086_ARG_SYSTEM_IS_ARG, g_localPlayer);
				}
			} else {
				msg_emitInFlightMessage(IFMSG_283_WARHEAD_IMPACT_DRAINED_ALL_CANNON_ENERGY,
										g_objectTable[craftObjIdx].playerOwnerIdx);
			}
		} else if (g_objectTable[craftObjIdx].genusId == CRAFT_GENUS_STARFIGHTER ||
				   g_objectTable[craftObjIdx].genusId == CRAFT_GENUS_TRANSPORT ||
				   g_objectTable[craftObjIdx].genusId == CRAFT_GENUS_UTILITY_VEHICLE) {
			craft->weaponFireInhibitTimer =
				(int16_t)(previousInhibitTimer + MAGNETIC_PULSE_SHORT_INHIBIT_TICKS);
		} else {
			craft->weaponFireInhibitTimer =
				(int16_t)(previousInhibitTimer + MAGNETIC_PULSE_LONG_INHIBIT_TICKS);
		}
		if ((uint16_t)craft->weaponFireInhibitTimer < previousInhibitTimer)
			craft->weaponFireInhibitTimer = -1;
		hitRegistered = 1;
	}

	g_objectTable[otherObjIdx].world_x = g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
	g_objectTable[otherObjIdx].world_y = g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
	g_objectTable[otherObjIdx].world_z = g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
	projectileObjectType = g_objectTable[otherObjIdx].objectType;
	if (g_projectileDamageByObjectType.warheadClass[projectileObjectType - PROJECTILE_OBJECT_TYPE_FIRST] != 0)
		g_objectTable[otherObjIdx].objectType = WARHEAD_IMPACT_EFFECT_TYPE;
	else if (projectileObjectType == PROJECTILE_OBJECT_TYPE_ION_LASER ||
			 projectileObjectType == PROJECTILE_OBJECT_TYPE_ION_TURBO_LASER)
		g_objectTable[otherObjIdx].objectType = ION_IMPACT_EFFECT_TYPE;
	else
		g_objectTable[otherObjIdx].objectType = LASER_IMPACT_EFFECT_TYPE;
	g_objectTable[otherObjIdx].genusId = EXPLOSION_GENUS;
	g_objectTable[otherObjIdx].mobj->state = EXPLOSION_STATE;
	g_objectTable[otherObjIdx].typeSpecificByte[0] = IMPACT_EFFECT_SUBTYPE;
	g_objectTable[otherObjIdx].mobj->framesAlive = 0;
	g_objectTable[otherObjIdx].mobj->lifetimeTimer = 0;
	g_objectTable[otherObjIdx].mobj->lightIntensityScale = 0;
	g_objectTable[otherObjIdx].mobj->speed = g_objectTable[craftObjIdx].mobj->speed;
	g_objectTable[otherObjIdx].pitch = g_objectTable[craftObjIdx].pitch;
	g_objectTable[otherObjIdx].yaw = g_objectTable[craftObjIdx].yaw;
	g_objectTable[otherObjIdx].roll = 0;
	g_objectTable[otherObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[otherObjIdx].mobj->moveVectorDirty = g_objectTable[otherObjIdx].mobj->orientMatrixDirty;

	if (hitRegistered != 0) {
		if (g_objectTable[craftObjIdx].playerOwnerIdx == g_localPlayer) {
			fsfx_PlaySound(FLIGHT_SOUND_SHIELD_HIT, otherObjIdx, g_localPlayer);
		} else if (g_objectTable[otherObjIdx].objectType == LASER_IMPACT_EFFECT_TYPE ||
				   g_objectTable[otherObjIdx].objectType == ION_IMPACT_EFFECT_TYPE) {
			fsfx_PlaySound(FLIGHT_SOUND_LASER_IMPACT, otherObjIdx, g_localPlayer);
		} else {
			fsfx_PlaySound((GameRand2() & 3) + FLIGHT_SOUND_SMALL_EXPLOSION_FIRST, otherObjIdx,
						   g_localPlayer);
		}
	}
}

// FUNCTION: XVT 0x41DC30
int16_t collide_damagecraft(uint16_t victimObjIdx, int16_t hitMeshIndex, uint16_t sourceObjIdx,
							uint16_t hitSideOrDamageAmount) {
	enum {
		PLAYER_COUNT = 8,
		FIRST_OPT_OBJECT_TYPE = 73,
		MISSION_STATUS_INVULNERABLE = 20,
		SYNTHETIC_STARSHIP_SOURCE = UINT16_MAX - 1,
		DEFAULT_COLLISION_OBJECT_TYPE = 53,
		SPECIAL_COLLISION_OBJECT_TYPE = 58,
		MISSION_V14_DAMAGE_REDUCTION_TYPE_1 = 37,
		MISSION_V14_DAMAGE_REDUCTION_TYPE_2 = 38,
		ION_OBJECT_TYPE_1 = 141,
		ION_OBJECT_TYPE_2 = 142,
		ION_OBJECT_TYPE_3 = 147,
		MAX_MODEL_BOUNDS_EXTENT = 0x8000,
		DEFAULT_COLLISION_DAMAGE = 0x20000,
		STARSHIP_DAMAGE_SCALE = 16,
		FREIGHTER_DAMAGE_SCALE = 4,
		SYSTEM_DAMAGE_LIMIT = 1000,
		SYSTEM_DISABLE_THRESHOLD = 10,
		SYSTEM_DISABLE_DAMAGE_STEP = 200,
		SHIELD_HIT_FLASH_TICKS = 59,
		HULL_HIT_FLASH_TICKS = 59,
		PLAYER_DEATH_LIFETIME_TICKS = 708,
		CRAFT_EXPLOSION_SECOND_TICKS = 1180,
		CRAFT_EXPLOSION_TIME_BIAS = 1179,
		DETACH_COMPONENT_STATE = 4,
		BREAKUP_ANIMATION_STATE = 2,
		GENERIC_IMPACT_ORDER_PROBABILITY = 0x2000,
		HULL_IMPACT_ORDER_PROBABILITY = 0x4000,
		SYSTEM_FAILURE_ORDER_PROBABILITY = 0x6000,
		FRIENDLY_LOSS_ORDER_PROBABILITY = 0xC000,
		WINGMAN_KILL_ORDER_PROBABILITY = 0xA000,
	};

	int syntheticStarshipDamage;
	uint8_t result;
	uint16_t savedRandState;
	uint16_t damageObjectType;
	uint8_t cockpitStatusDirty;
	int* shieldEnergy;
	uint8_t sourceState;
	AiController* aiController;
	CraftData* craft;
	unsigned int damageAmount;
	int damage;
	uint16_t attackerSourceObjIdx;

	cockpitStatusDirty = 0;
	syntheticStarshipDamage = 0;
	result = 1;
	if (g_flightSimSideEffectsSuppressed != 0)
		return 1;

	if (g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.status1 ==
			MISSION_STATUS_INVULNERABLE ||
		g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.status2 ==
			MISSION_STATUS_INVULNERABLE)
		return 1;
	if (g_activeRegionObjectSlotStart <= sourceObjIdx && sourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
		int sourceFlightGroupIdx = g_objectTable[sourceObjIdx].flightGroupIdx;

		if (g_missionFlightGroups[sourceFlightGroupIdx].fg.status1 == MISSION_STATUS_INVULNERABLE ||
			g_missionFlightGroups[sourceFlightGroupIdx].fg.status2 == MISSION_STATUS_INVULNERABLE)
			return 1;
	}

	craft = g_objectTable[victimObjIdx].mobj->pCraft;
	savedRandState = (uint16_t)g_gameRandStateB;
	aiController = &craft->aiController;
	g_gameRandStateB = aiController->savedRandSeed;

	if (sourceObjIdx == SYNTHETIC_STARSHIP_SOURCE) {
		syntheticStarshipDamage = 1;
		sourceObjIdx = UINT16_MAX;
		damageObjectType = DEFAULT_COLLISION_OBJECT_TYPE;
		damageAmount = hitSideOrDamageAmount;
		hitSideOrDamageAmount = 0;
	} else if (sourceObjIdx == UINT16_MAX) {
		damageObjectType = DEFAULT_COLLISION_OBJECT_TYPE;
		damageAmount = DEFAULT_COLLISION_DAMAGE;
	} else {
		if (g_objectTable[sourceObjIdx].mobj != NULL) {
			damageObjectType = g_objectTable[sourceObjIdx].objectType;
			if (g_objectTable[sourceObjIdx].mobj->pCraft != NULL)
				damageAmount = collide_ComputeCraftDamageAmount(victimObjIdx, sourceObjIdx);
			else
				damageAmount = g_objectTable[sourceObjIdx].mobj->damageAmount;
		} else {
			unsigned int maxBoundsExtent;

			damageObjectType = g_objectTable[sourceObjIdx].objectType;
			maxBoundsExtent = (unsigned int)g_modelTypeTable[damageObjectType].maxBoundsExtent;
			if (maxBoundsExtent < MAX_MODEL_BOUNDS_EXTENT)
				damageAmount = 4 * maxBoundsExtent;
			else
				damageAmount = DEFAULT_COLLISION_DAMAGE;
		}
	}

	if (g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_STARSHIP ||
		g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_PLATFORM)
		damageAmount /= STARSHIP_DAMAGE_SCALE;
	if (g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_FREIGHTER)
		damageAmount /= FREIGHTER_DAMAGE_SCALE;
	if (g_missionFileVersion == 14 &&
		(g_objectTable[victimObjIdx].objectType == MISSION_V14_DAMAGE_REDUCTION_TYPE_1 ||
		 g_objectTable[victimObjIdx].objectType == MISSION_V14_DAMAGE_REDUCTION_TYPE_2))
		damageAmount /= FREIGHTER_DAMAGE_SCALE;

	damage = (int)damageAmount;
	attackerSourceObjIdx = UINT16_MAX;
	craft->damageStats.damageReceivedTotal += damageAmount;
	if (sourceObjIdx != attackerSourceObjIdx && g_objectTable[sourceObjIdx].mobj != NULL) {
		int sourcePlayerIdx;

		sourceState = g_objectTable[sourceObjIdx].mobj->state;
		if (sourceState == 0) {
			attackerSourceObjIdx = sourceObjIdx;
			sourcePlayerIdx = g_objectTable[sourceObjIdx].playerOwnerIdx;
		} else {
			attackerSourceObjIdx = g_objectTable[sourceObjIdx].mobj->sourceObjIdx;
			sourcePlayerIdx = g_objectTable[attackerSourceObjIdx].playerOwnerIdx;
			if (sourceState == 1 && g_objectTable[sourceObjIdx].mobj->pWarheadGuidance != NULL)
				sourcePlayerIdx = g_objectTable[sourceObjIdx].mobj->pWarheadGuidance->sourcePlayerIdx;
		}

		if (sourcePlayerIdx != -1) {
			craft->damageStats.damageFromPlayer[sourcePlayerIdx] += damageAmount;
			if (g_objectTable[sourceObjIdx].mobj->state == 0)
				craft->damageStats.damageFromCollision += damageAmount;
		} else {
			if (sourceState == 0) {
				craft->damageStats.damageFromCollision += damageAmount;
			} else {
				if (g_objectTable[attackerSourceObjIdx].genusId == CRAFT_GENUS_STARFIGHTER) {
					int aiSkill =
						g_missionFlightGroups[g_objectTable[attackerSourceObjIdx].flightGroupIdx].fg.groupAI;

					craft->damageStats.damageFromAiSkill[aiSkill] += damageAmount;
				} else {
					if (g_regionMainObjectSlotEnd > attackerSourceObjIdx ||
						g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount <= attackerSourceObjIdx)
						craft->damageStats.damageFromStarship += damageAmount;
					else
						craft->damageStats.damageFromMine += damageAmount;
				}
			}
		}
		craft->damageStats.damageFromFlightGroupAmount[g_objectTable[attackerSourceObjIdx].flightGroupIdx] +=
			damageAmount;
	} else if (sourceObjIdx != UINT16_MAX && g_objectTable[sourceObjIdx].mobj == NULL) {
		if (g_objectTable[sourceObjIdx].genusId == CRAFT_GENUS_NORMAL_DEBRIS)
			craft->damageStats.damageFromCollision += damageAmount;
	} else if (syntheticStarshipDamage != 0) {
		craft->damageStats.damageFromStarship += damageAmount;
	}
	if (g_objectTable[victimObjIdx].playerOwnerIdx != -1)
		craft->damageStats.damageReceivedByPlayerOwnedCraft += damageAmount;

	if (hitMeshIndex != -1) {
		if (craft->shieldEnergy[0] + craft->shieldEnergy[1] == 0 && g_flightMissionState.difficulty != 0) {
			if (ModelMesh_HasExplosionType1(g_objectTable[victimObjIdx].objectType,
											(uint16_t)hitMeshIndex - 1) != 0)
				damage = Craft_DamageComponent(victimObjIdx, hitMeshIndex, damage, attackerSourceObjIdx);
		} else if (g_flightMissionState.difficulty == 0 &&
				   ModelMesh_HasExplosionType1(g_objectTable[victimObjIdx].objectType,
											   (uint16_t)hitMeshIndex - 1) != 0) {
			damage = Craft_DamageComponent(victimObjIdx, hitMeshIndex, damage, attackerSourceObjIdx);
		}
	}
	if (g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_OBSTACLE)
		damage = 0;

	shieldEnergy = &craft->shieldEnergy[hitSideOrDamageAmount];
	if (damage < *shieldEnergy) {
		*shieldEnergy = *shieldEnergy - (int)damage;
		if (g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
			int otherShieldEnergy;

			g_playerFlightTransientTimers[g_objectTable[victimObjIdx].playerOwnerIdx].shieldHitFlashTimer =
				SHIELD_HIT_FLASH_TICKS;
			g_lastShieldDamageSide = (uint8_t)hitSideOrDamageAmount;
			otherShieldEnergy = craft->shieldEnergy[(uint16_t)(hitSideOrDamageAmount ^ 1)];
			if (*shieldEnergy < otherShieldEnergy) {
				int redistributedEnergy = (otherShieldEnergy - *shieldEnergy) / 2;

				craft->shieldEnergy[(uint16_t)(hitSideOrDamageAmount ^ 1)] =
					otherShieldEnergy - redistributedEnergy;
				*shieldEnergy += redistributedEnergy;
				craft->shieldDistribMode = SHIELD_DISTRIBUTION_EVEN;
			}
		}
		if (g_players[g_localPlayer].objectIndex != victimObjIdx)
			fsfx_speakorderack(g_localPlayer, victimObjIdx, 3, -1, victimObjIdx,
							   GENERIC_IMPACT_ORDER_PROBABILITY);
	} else {
		int overflowDamage;

		if (g_gameConfig.voiceTacticalOfficerEnabled == 2 &&
			g_objectTable[victimObjIdx].genusId != CRAFT_GENUS_STARFIGHTER && *shieldEnergy != 0 &&
			craft->hullDamage == 0)
			fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_SHIELDS_OUT, victimObjIdx,
										   UINT16_MAX);
		overflowDamage = (int)damage - *shieldEnergy;
		*shieldEnergy = 0;
		if (g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
			int otherShieldEnergy;

			if (hitSideOrDamageAmount == 0 && syntheticStarshipDamage == 0 && (uint16_t)GameRand() < 0x1000u)
				fsfx_PlaySound(FLIGHT_SOUND_R2_HIT, -1, g_objectTable[victimObjIdx].playerOwnerIdx);
			otherShieldEnergy = craft->shieldEnergy[(uint16_t)(hitSideOrDamageAmount ^ 1)];
			if (otherShieldEnergy != 0) {
				craft->shieldEnergy[(uint16_t)(hitSideOrDamageAmount ^ 1)] -= otherShieldEnergy / 2;
				*shieldEnergy += otherShieldEnergy / 2;
				craft->shieldDistribMode = SHIELD_DISTRIBUTION_EVEN;
			}
		}

		if (overflowDamage != 0) {
			if (damageObjectType == ION_OBJECT_TYPE_1 || damageObjectType == ION_OBJECT_TYPE_2 ||
				damageObjectType == ION_OBJECT_TYPE_3) {
				int16_t systemStrengthRemaining;

				if ((uint16_t)craft->subsystemDamage < SYSTEM_DAMAGE_LIMIT) {
					if (damageObjectType == ION_OBJECT_TYPE_1)
						craft->subsystemDamage += 1;
					if (damageObjectType == ION_OBJECT_TYPE_2)
						craft->subsystemDamage += 2;
					if (damageObjectType == ION_OBJECT_TYPE_3)
						craft->subsystemDamage += 4;
				}
				systemStrengthRemaining =
					(int16_t)(g_modelDefs[craft->modelIndex].systemStrength - craft->subsystemDamage);
				if (craft->workingSubsystems != 0 && systemStrengthRemaining <= SYSTEM_DISABLE_THRESHOLD) {
					if (overflowDamage > 0) {
						unsigned int disableCount =
							(unsigned int)(overflowDamage + SYSTEM_DISABLE_DAMAGE_STEP - 1) /
							SYSTEM_DISABLE_DAMAGE_STEP;

						do {
							uint16_t subsystemIndex;
							uint16_t workingSubsystems = craft->workingSubsystems;

							for (subsystemIndex = 0; subsystemIndex < CRAFT_SUBSYSTEM_COUNT;
								 ++subsystemIndex) {
								uint16_t subsystemFlag = g_subsystemIdToFlag[subsystemIndex];

								if ((workingSubsystems & subsystemFlag) != 0) {
									cockpitStatusDirty = 1;
									craft->workingSubsystems =
										workingSubsystems & (subsystemFlag ^ CRAFT_SUBSYSTEM_FLAGS_ALL);
									break;
								}
							}
							if (subsystemIndex < CRAFT_SUBSYSTEM_COUNT &&
								g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
								craft->systemHealth[subsystemIndex] = 0;
								craft->systemTimer[subsystemIndex] =
									g_subsystemRepairDuration[subsystemIndex];
							}
						} while (--disableCount != 0);
					}
					if (systemStrengthRemaining <= 0 && g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
						uint16_t subsystemIndex;

						for (subsystemIndex = 0; subsystemIndex < CRAFT_SUBSYSTEM_COUNT; ++subsystemIndex) {
							uint16_t subsystemFlag = g_subsystemIdToFlag[subsystemIndex];

							if ((craft->workingSubsystems & subsystemFlag) != 0) {
								craft->workingSubsystems &= subsystemFlag ^ CRAFT_SUBSYSTEM_FLAGS_ALL;
								craft->systemHealth[subsystemIndex] = 0;
								craft->systemTimer[subsystemIndex] =
									g_subsystemRepairDuration[subsystemIndex];
							}
						}
						craft->workingSubsystems = 0;
					}
					{
						int enemyCraft = 0;

						if (craft->workingSubsystems == enemyCraft) {
							craft->subsystemDamage = g_modelDefs[craft->modelIndex].systemStrength;
							craft->shieldEnergy[0] = enemyCraft;
							craft->shieldEnergy[1] = enemyCraft;
							msg_emitCraftMessage(victimObjIdx, craft, 137);
							if (fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_DISABLED,
															   victimObjIdx, UINT16_MAX) != 0) {
								int victimTeam =
									g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.team;
								int playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;

								if (victimTeam != playerTeam)
									enemyCraft = g_missionTeams[playerTeam].allies[victimTeam] == 0;
								if (enemyCraft == 0) {
									int localObjectIdx = g_players[g_localPlayer].objectIndex;

									if (localObjectIdx != -1) {
										uint8_t localObjectType = g_objectTable[localObjectIdx].objectType;

										fsfx_PlaySound((localObjectType == 1 || localObjectType == 2 ||
														localObjectType == 3 || localObjectType == 14 ||
														localObjectType == 4)
														   ? FLIGHT_SOUND_R2_WARNING
														   : FLIGHT_SOUND_GENERAL_WARNING,
													   -1, g_localPlayer);
									}
								}
							}
							if (g_objectTable[victimObjIdx].playerOwnerIdx == -1) {
								unsigned int slotIndex;

								for (slotIndex = 0; slotIndex < craft->laserSlotCount; ++slotIndex) {
									if (craft->weaponSlots[slotIndex].projectileTypeId == 2)
										craft->turretTargetStates[slotIndex].targetObjIdx = UINT16_MAX;
								}
								for (slotIndex = 0; slotIndex < craft->cannonClassCount; ++slotIndex)
									craft->laserState.linkMode[slotIndex] = 0;
							}
							{
								int victimFlightGroupIdx = g_objectTable[victimObjIdx].flightGroupIdx;

								++g_missionFgStats[victimFlightGroupIdx]
									  .outcomeCount[FLIGHT_GROUP_OUTCOME_DISABLED];
								if (g_missionFlightGroups[victimFlightGroupIdx].fg.specialCargoCraft ==
									craft->waveNumber)
									g_missionFgStats[victimFlightGroupIdx]
										.specialCargoOutcome[FLIGHT_GROUP_OUTCOME_DISABLED] = 1;
							}
						}
					}
				}
				if (g_objectTable[victimObjIdx].playerOwnerIdx == g_localPlayer)
					fsfx_PlaySound(FLIGHT_SOUND_SYSTEM_HIT, victimObjIdx, g_localPlayer);
			} else {
				unsigned int hullDamageBefore;

				if (hitMeshIndex != -1 &&
					ModelMesh_IsObjectTypeMeshDamageable(g_objectTable[victimObjIdx].objectType,
														 (uint16_t)hitMeshIndex - 1) != 0)
					overflowDamage = Craft_DamageComponent(
						victimObjIdx, hitMeshIndex, (unsigned int)overflowDamage, attackerSourceObjIdx);
				hullDamageBefore = craft->hullDamage;
				craft->hullDamage = hullDamageBefore + (unsigned int)overflowDamage;
				if (g_gameConfig.voiceTacticalOfficerEnabled == 2 &&
					g_objectTable[victimObjIdx].genusId != CRAFT_GENUS_STARFIGHTER) {
					unsigned int threshold = MATH2_longfraction(craft->hullMax, 0xF333);

					if (hullDamageBefore >= threshold || craft->hullDamage < threshold) {
						threshold = MATH2_longfraction(craft->hullMax, 0xC000);
						if (hullDamageBefore >= threshold || craft->hullDamage < threshold) {
							threshold = MATH2_longfraction(craft->hullMax, 0x4000);
							if (hullDamageBefore < threshold && craft->hullDamage >= threshold)
								fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS,
															   TACTICAL_MSG_HULL_AT_75_PERCENT, victimObjIdx,
															   UINT16_MAX);
						} else
							fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS,
														   TACTICAL_MSG_HULL_AT_25_PERCENT, victimObjIdx,
														   UINT16_MAX);
					} else {
						fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_HULL_CRITICAL,
													   victimObjIdx, UINT16_MAX);
					}
				}
				if (g_objectTable[victimObjIdx].playerOwnerIdx != -1 && syntheticStarshipDamage == 0) {
					g_playerFlightTransientTimers[g_objectTable[victimObjIdx].playerOwnerIdx]
						.hullHitFlashTimer += HULL_HIT_FLASH_TICKS;
					if ((uint16_t)GameRand() < 0x4000u) {
						uint16_t subsystemIndex = GameRand() & 7;
						uint16_t subsystemFlag;

						subsystemIndex += GameRand() & 1;
						subsystemIndex += GameRand() & 1;
						subsystemFlag = g_subsystemIdToFlag[subsystemIndex];

						if ((craft->workingSubsystems & subsystemFlag) != 0) {
							craft->workingSubsystems &= subsystemFlag ^ CRAFT_SUBSYSTEM_FLAGS_ALL;
							g_msgArgTable[0] = g_subsystemMessageArgById[subsystemIndex];
							g_msgArgTable[1] = 87;
							if (g_objectTable[victimObjIdx].playerOwnerIdx == g_localPlayer &&
								g_players[g_localPlayer].regionSessionId == 0)
								msg_emitInFlightMessage(IFMSG_086_ARG_SYSTEM_IS_ARG, g_localPlayer);
							craft->systemHealth[subsystemIndex] = 0;
							craft->systemTimer[subsystemIndex] = g_subsystemRepairDuration[subsystemIndex];
						}
					}
				}
			}

			if (craft->hullDamage < craft->systemDamageHullThreshold || syntheticStarshipDamage != 0) {
				if (g_objectTable[victimObjIdx].playerOwnerIdx == g_localPlayer &&
					syntheticStarshipDamage == 0) {
					if (((GameRand2() >> 8) & 0x80u) != 0)
						fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_1, victimObjIdx, g_localPlayer);
					else
						fsfx_PlaySound(FLIGHT_SOUND_HULL_HIT_2, victimObjIdx, g_localPlayer);
					result = 0;
				}
				if (g_players[g_localPlayer].objectIndex != victimObjIdx)
					fsfx_speakorderack(g_localPlayer, victimObjIdx, 3, -1, victimObjIdx,
									   HULL_IMPACT_ORDER_PROBABILITY);
			} else {
				uint16_t featureMask = g_subsystemFailureHudMaskByRandomSlot[GameRand() & 0xF];

				if (!((g_flightMissionState.provingGroundsModeActive != 0 && featureMask == 1) ||
					  (featureMask & craft->damageStats.installedHudFeatureMask) == 0)) {
					if (g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
						uint8_t victimObjectType = g_objectTable[victimObjIdx].objectType;

						fsfx_PlaySound(FLIGHT_SOUND_INTERNAL_HIT, victimObjIdx,
									   g_objectTable[victimObjIdx].playerOwnerIdx);
						result = 0;
						if (victimObjectType == 1 || victimObjectType == 2 || victimObjectType == 3 ||
							victimObjectType == 14 || victimObjectType == 4) {
							switch (featureMask) {
								case 0x2:
								case 0x4:
								case 0x8:
									featureMask = 0xE;
									break;
								case 0x80:
								case 0x100:
									featureMask = 0x180;
									break;
								case 0x200:
								case 0x400:
								case 0x800:
								case 0x1000:
									featureMask = 0xE00;
									break;
							}
						}
					}
					cockpitStatusDirty = 1;
					craft->damageStats.activeHudFeatureMask &= (uint16_t)~featureMask;
				}
				if (craft->objectKind == CRAFT_OBJECT_KIND_ACTIVE &&
					g_players[g_localPlayer].objectIndex != victimObjIdx)
					fsfx_speakorderack(g_localPlayer, victimObjIdx, 5, -1, victimObjIdx,
									   SYSTEM_FAILURE_ORDER_PROBABILITY);
			}
		}
	}

	if (cockpitStatusDirty != 0 && g_objectTable[victimObjIdx].playerOwnerIdx == g_localPlayer &&
		g_replayViewMode == 0 && g_flightSimSideEffectsSuppressed == 0) {
		FlightSurface_Lock();
		Hud_UpdateCraftSystemStatusIndicators();
		FlightSurface_Unlock();
	}

	if (g_flightSimSideEffectsSuppressed == 0 &&
		(craft->objectKind == CRAFT_OBJECT_KIND_ACTIVE ||
		 craft->objectKind == CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE) &&
		craft->hullDamage >= craft->hullMax) {
		int destructionSourcePlayerIdx;
		uint16_t objectIndex;

		if (sourceObjIdx != UINT16_MAX && g_objectTable[sourceObjIdx].mobj != NULL) {
			uint16_t destructionSourceObjIdx;

			if (g_objectTable[sourceObjIdx].mobj->state == 0)
				destructionSourceObjIdx = sourceObjIdx;
			else
				destructionSourceObjIdx = g_objectTable[sourceObjIdx].mobj->sourceObjIdx;
			Mission_CreditDestructionDamageContributors(destructionSourceObjIdx, victimObjIdx);
		}
		destructionSourcePlayerIdx = Mission_RecordPlayerCraftLoss(victimObjIdx, 0);
		if (g_objectTable[victimObjIdx].playerOwnerIdx != -1)
			Player_SaveCraftSettings(g_objectTable[victimObjIdx].playerOwnerIdx);
		if (g_flightMissionState.provingGroundsModeActive != 0) {
			if (g_objectTable[victimObjIdx].playerOwnerIdx == g_localPlayer)
				g_flightMissionState.missionEndPending = 1;
		} else if (g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
			Player_StartPostDestructionState(g_objectTable[victimObjIdx].playerOwnerIdx, attackerSourceObjIdx,
											 destructionSourcePlayerIdx);
		}
		if (g_objectTable[victimObjIdx].playerOwnerIdx != -1)
			g_flightGlobalCountdownTimers.missionArrivalTriggerScanTimer = 0;

		Mission_RecordCraftOutcome(victimObjIdx, (uint16_t)g_objectTable[victimObjIdx].flightGroupIdx,
								   FLIGHT_GROUP_OUTCOME_DESTROYED);
		for (objectIndex = g_activeRegionObjectSlotStart; objectIndex < g_activeRegionCraftObjectSlotEnd;
			 ++objectIndex) {
			if (g_objectTable[objectIndex].objectType != 0 &&
				g_objectTable[objectIndex].mobj->pCraft->carriedObjectIndex == victimObjIdx)
				g_objectTable[objectIndex].mobj->pCraft->carriedObjectIndex = UINT16_MAX;
		}
		for (objectIndex = g_activeRegionObjectSlotStart; objectIndex < g_activeRegionCraftObjectSlotEnd;
			 ++objectIndex) {
			if (g_objectTable[objectIndex].objectType != 0 &&
				g_objectTable[objectIndex].mobj->pCraft->carrierObjIdx == victimObjIdx)
				g_objectTable[objectIndex].mobj->pCraft->carrierObjIdx = UINT16_MAX;
		}
		msg_emitCraftMessage(victimObjIdx, craft, 136);

		if (g_objectTable[victimObjIdx].flightGroupIdx == g_players[g_localPlayer].boundFlightGroupIdx) {
			if (fsfx_speakorderack(g_localPlayer, victimObjIdx, 6, -1, victimObjIdx,
								   FRIENDLY_LOSS_ORDER_PROBABILITY) == 0)
				fsfx_speakorderack(g_localPlayer, -1, 16, -1, victimObjIdx, FRIENDLY_LOSS_ORDER_PROBABILITY);
		} else if (attackerSourceObjIdx < g_activeRegionCraftObjectSlotEnd &&
				   g_players[g_localPlayer].objectIndex != attackerSourceObjIdx &&
				   g_objectTable[attackerSourceObjIdx].flightGroupIdx ==
					   g_players[g_localPlayer].boundFlightGroupIdx) {
			if (g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_STARFIGHTER ||
				g_objectTable[victimObjIdx].genusId == CRAFT_GENUS_TRANSPORT)
				fsfx_speakorderack(g_localPlayer, attackerSourceObjIdx, 9, -1, victimObjIdx, UINT16_MAX);
			else
				fsfx_speakorderack(g_localPlayer, attackerSourceObjIdx, 10, -1, victimObjIdx,
								   WINGMAN_KILL_ORDER_PROBABILITY);
		}

		if (fsfx_SpeakTacticalOfficerEvent(TACTICAL_VOICE_STATUS, TACTICAL_MSG_DESTROYED, victimObjIdx,
										   UINT16_MAX) != 0) {
			int victimTeam = g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.team;
			int playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
			int enemyCraft = 0;

			if (victimTeam != playerTeam)
				enemyCraft = g_missionTeams[playerTeam].allies[victimTeam] == 0;
			if (enemyCraft == 0) {
				int localObjectIdx = g_players[g_localPlayer].objectIndex;

				if (localObjectIdx != -1) {
					uint8_t localObjectType = g_objectTable[localObjectIdx].objectType;

					fsfx_PlaySound((localObjectType == 1 || localObjectType == 2 || localObjectType == 3 ||
									localObjectType == 14 || localObjectType == 4)
									   ? FLIGHT_SOUND_R2_WARNING
									   : FLIGHT_SOUND_GENERAL_WARNING,
								   -1, g_localPlayer);
				}
			}
		}

		{
			uint16_t victimObjectType = g_objectTable[victimObjIdx].objectType;

			if (ModelMesh_HasFuselage(victimObjectType) == 0) {
				int modelIndex = craft->modelIndex;
				uint16_t breakupRollRate = (uint16_t)((GameRand() & 0x3FFF) + 0x2000);
				uint16_t maxTumbleAngle = g_modelDefs[modelIndex].maxTumbleAngle;

				while (breakupRollRate > maxTumbleAngle)
					breakupRollRate >>= 1;
				if ((uint16_t)GameRand() < 0x8000u)
					breakupRollRate = (uint16_t)-(int16_t)breakupRollRate;
				g_objectTable[victimObjIdx].mobj->rollImpulseRate = (int16_t)breakupRollRate;
				if (g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.craftExplosionTime !=
					0)
					g_objectTable[victimObjIdx].mobj->lifetimeTimer =
						CRAFT_EXPLOSION_SECOND_TICKS *
							g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx]
								.fg.craftExplosionTime -
						CRAFT_EXPLOSION_TIME_BIAS;
				else
					g_objectTable[victimObjIdx].mobj->lifetimeTimer =
						SIMULATION_TICKS_PER_SECOND * ((GameRand() & 7) + 8);
				craft->objectKind = CRAFT_OBJECT_KIND_BREAKING_UP;
			} else if ((g_modelTypeTable[damageObjectType].maxBoundsExtent <= 1095 ||
						g_modelTypeTable[damageObjectType].familyId != CRAFT_FAMILY_SPACE_CRAFT) &&
					   damageObjectType != SPECIAL_COLLISION_OBJECT_TYPE &&
					   g_objectTable[victimObjIdx].mobj->speed != 0) {
				if ((uint16_t)GameRand() < 0x4000u && g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
					g_objectTable[victimObjIdx].mobj->lifetimeTimer = 1;
					fsfx_PlaySound(FLIGHT_SOUND_LARGE_EXPLOSION, victimObjIdx, g_localPlayer);
					result = 0;
					craft->objectKind = CRAFT_OBJECT_KIND_EXPLODING;
				} else {
					uint16_t detachedYawOffset;
					uint16_t meshCount;
					uint16_t meshIndex;
					int16_t side;
					int16_t detachedRollRate;

					if (victimObjectType < FIRST_OPT_OBJECT_TYPE)
						meshCount = (uint16_t)g_objectTypeMeshCache[victimObjectType].meshCount;
					else
						meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(victimObjectType);
					detachedYawOffset = 0;
					detachedRollRate = 0;
					side = 0;
					if (meshCount > 1) {
						side = (int16_t)(GameRand() & 1);
						for (meshIndex = 0; (uint16_t)meshIndex < meshCount; ++meshIndex) {
							int componentIndex = meshIndex;
							MeshComponentType meshType;

							if (victimObjectType < FIRST_OPT_OBJECT_TYPE)
								meshType =
									ModelMesh_GetCachedObjectTypeMeshType(victimObjectType, componentIndex);
							else
								meshType = ModelMesh_GetObjectTypeMeshType(victimObjectType, componentIndex);

							if (craft->componentState[componentIndex] == 0 &&
								(meshType == MESH_COMPONENT_02_WING || meshType == MESH_COMPONENT_20_WING)) {
								if (side != 0) {
									if (ModelMesh_GetCenterX(victimObjectType, componentIndex) < 0)
										break;
								} else if (ModelMesh_GetCenterX(victimObjectType, componentIndex) > 0) {
									break;
								}
							}
						}
						if ((uint16_t)meshIndex < meshCount) {
							uint16_t detachedComponentObjIdx =
								Object_SpawnDetachedComponent(victimObjIdx, (int16_t)meshIndex);
							if (detachedComponentObjIdx != UINT16_MAX) {
								detachedRollRate = (int16_t)((GameRand() & 0x3FFF) + 0x4000);
								detachedYawOffset = (int16_t)((GameRand() & 0x7FF) + 2048);
								if (side != 0) {
									detachedYawOffset = (uint16_t)-detachedYawOffset;
									detachedRollRate = -detachedRollRate;
								}
								g_objectTable[detachedComponentObjIdx].mobj->rollImpulseRate =
									detachedRollRate;
								g_objectTable[detachedComponentObjIdx].yaw += detachedYawOffset;
								g_objectTable[detachedComponentObjIdx].mobj->orientMatrixDirty = 1;
								g_objectTable[detachedComponentObjIdx].mobj->moveVectorDirty =
									g_objectTable[detachedComponentObjIdx].mobj->orientMatrixDirty;
								g_objectTable[detachedComponentObjIdx].typeSpecificByte[1] =
									BREAKUP_ANIMATION_STATE;
								craft->componentState[meshIndex] = DETACH_COMPONENT_STATE;
								if (((GameRand2() >> 8) & 0x80u) != 0)
									fsfx_PlaySound(FLIGHT_SOUND_BREAKUP_1, victimObjIdx, g_localPlayer);
								else
									fsfx_PlaySound(FLIGHT_SOUND_BREAKUP_2, victimObjIdx, g_localPlayer);
								result = 0;
							}
						}
					}

					{
						int modelIndex = craft->modelIndex;
						uint16_t breakupRollRate = (uint16_t)((GameRand() & 0x3FFF) + 0x2000);
						uint16_t maxTumbleAngle = g_modelDefs[modelIndex].maxTumbleAngle;

						while (breakupRollRate > maxTumbleAngle)
							breakupRollRate >>= 1;
						if ((uint16_t)detachedRollRate < 0x8000u)
							breakupRollRate = (uint16_t)-(int16_t)breakupRollRate;
						g_objectTable[victimObjIdx].mobj->rollImpulseRate = (int16_t)breakupRollRate;
					}
					/* Recoil uses the signed, wrapped angle from the detached component. */
					if (detachedYawOffset != 0) {
						detachedYawOffset = (int16_t)((uint16_t)detachedYawOffset >> 1);
						if (side == 0)
							detachedYawOffset = -detachedYawOffset;
						g_objectTable[victimObjIdx].yaw += detachedYawOffset;
						g_objectTable[victimObjIdx].mobj->orientMatrixDirty = 1;
						g_objectTable[victimObjIdx].mobj->moveVectorDirty =
							g_objectTable[victimObjIdx].mobj->orientMatrixDirty;
					}
					craft->objectKind = CRAFT_OBJECT_KIND_BREAKING_UP;
					if (g_objectTable[victimObjIdx].playerOwnerIdx != -1) {
						g_objectTable[victimObjIdx].mobj->lifetimeTimer = PLAYER_DEATH_LIFETIME_TICKS;
					} else {
						uint16_t randomBias = GameRand() & 3;

						g_objectTable[victimObjIdx].mobj->lifetimeTimer =
							SIMULATION_TICKS_PER_SECOND * (randomBias + (GameRand() & 7) + 1);
					}
					craft->componentState[meshCount] = BREAKUP_ANIMATION_STATE;
				}
			} else {
				g_objectTable[victimObjIdx].mobj->lifetimeTimer = 1;
				fsfx_PlaySound(FLIGHT_SOUND_LARGE_EXPLOSION, victimObjIdx, g_localPlayer);
				result = 0;
				craft->objectKind = CRAFT_OBJECT_KIND_EXPLODING;
			}
		}
	} else if ((craft->objectKind == CRAFT_OBJECT_KIND_ACTIVE ||
				craft->objectKind == CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE) &&
			   craft->hullDamage >= craft->hullMax) {
		craft->hullDamage = craft->hullMax - 1;
	}

	aiController->savedRandSeed = g_gameRandStateB;
	g_gameRandStateB = (int16_t)savedRandState;
	return (int16_t)result;
}

// FUNCTION: XVT 0x41F300
int collide_ConvertObjectToExplosion(unsigned int objOrMissionPointRef, uint8_t objectType) {
	g_objectTable[objOrMissionPointRef].objectType = objectType;
	g_objectTable[objOrMissionPointRef].genusId = 13;
	g_objectTable[objOrMissionPointRef].mobj->state = 5;
	g_objectTable[objOrMissionPointRef].typeSpecificByte[0] = 2;
	g_objectTable[objOrMissionPointRef].mobj->lightIntensityScale = 0;
	g_objectTable[objOrMissionPointRef].mobj->speed = 0;
	g_objectTable[objOrMissionPointRef].mobj->framesAlive = 0;
	g_objectTable[objOrMissionPointRef].mobj->lifetimeTimer = 0;
	g_objectTable[objOrMissionPointRef].roll = 0;
	g_objectTable[objOrMissionPointRef].mobj->rollImpulseRate = 0;
	g_objectTable[objOrMissionPointRef].mobj->orientMatrixDirty = 1;
	return fsfx_PlaySound((GameRand2() & 3) + FLIGHT_SOUND_SMALL_EXPLOSION_FIRST, objOrMissionPointRef,
						  g_localPlayer);
}

// FUNCTION: XVT 0x41F3D0
unsigned int collide_roughdistance3du(unsigned int abs_dx, unsigned int abs_dy, unsigned int abs_dz) {
	if (abs_dx > abs_dy && abs_dx > abs_dz) {
		return abs_dx + (abs_dy >> 2) + (abs_dz >> 2);
	}
	if (abs_dy > abs_dx && abs_dy > abs_dz) {
		return abs_dy + (abs_dx >> 2) + (abs_dz >> 2);
	}
	return abs_dz + (abs_dx >> 2) + (abs_dy >> 2);
}

// FUNCTION: XVT 0x41F410
int collide_roughdistance3d(int dx, int dy, int dz) {
	int absDx;
	int absDy;
	int absDz;

	absDx = dx;
	if (absDx < 0) {
		absDx = (int)(0u - (unsigned int)absDx);
	}
	absDy = dy;
	if (absDy < 0) {
		absDy = (int)(0u - (unsigned int)absDy);
	}
	absDz = dz;
	if (absDz < 0) {
		absDz = (int)(0u - (unsigned int)absDz);
	}

	if (absDx > absDy && absDx > absDz) {
		return (int)((unsigned int)absDx + (unsigned int)(absDy >> 2) + (unsigned int)(absDz >> 2));
	}
	if (absDy > absDx && absDy > absDz) {
		return (int)((unsigned int)absDy + (unsigned int)(absDx >> 2) + (unsigned int)(absDz >> 2));
	}
	return (int)((unsigned int)absDz + (unsigned int)(absDx >> 2) + (unsigned int)(absDy >> 2));
}

// FUNCTION: XVT 0x41F470
unsigned int collide_TestSegmentAgainstLegacyPackedOptNode(const uint8_t* nodeData, int startX, int startY,
														   int startZ, int endX, int endY, int endZ,
														   int stopOnFirstHit) {
	const int16_t* nodeBounds;
	int bound;
	const uint8_t* packedVertexData;
	const int16_t* faceRecord;
	const uint8_t* vertex;
	const uint8_t* component;
	unsigned int faceRecordCount;
	unsigned int faceIndex;
	unsigned int nearestHitFractionQ15;
	unsigned int faceVertexCount;
	int planeSigns;
	unsigned int vertexRecordCount;
	int vertexIndex;
	int normalX;
	int normalY;
	int normalZ;
	int pointX;
	int pointY;
	int pointZ;
	int deltaStartX;
	int deltaStartY;
	int deltaStartZ;
	int deltaEndX;
	int deltaEndY;
	int deltaEndZ;
	int startPlaneDistance;
	int endPlaneDistance;
	int hitFractionQ15;
	int projectedPointU;
	int projectedPointV;
	int projectionAxisU;
	int projectionAxisV;
	int pointInsideFace;

	vertexRecordCount = nodeData[2];
	faceRecordCount = nodeData[4];
	nodeBounds = (const int16_t*)(nodeData + faceRecordCount + 5);
	bound = nodeBounds[0];
	if (startX < nodeBounds[0] && bound > endX) {
		return 0;
	}
	bound = nodeBounds[1];
	if (bound > startY && bound > endY) {
		return 0;
	}
	bound = nodeBounds[2];
	if (bound > startZ && bound > endZ) {
		return 0;
	}
	bound = nodeBounds[3];
	if (startX > nodeBounds[3] && bound < endX) {
		return 0;
	}
	bound = nodeBounds[4];
	if (bound < startY && bound < endY) {
		return 0;
	}
	bound = nodeBounds[5];
	if (bound < startZ && bound < endZ) {
		return 0;
	}

	packedVertexData = (const uint8_t*)(nodeBounds + 6);
	faceRecord = (const int16_t*)(packedVertexData + 12 * vertexRecordCount);
	nearestHitFractionQ15 = 0x7FFFFFFF;
	for (faceIndex = 0; faceRecordCount > faceIndex; faceIndex++) {
		const uint8_t* faceIndices;

		normalX = faceRecord[0];
		normalY = faceRecord[1];
		normalZ = faceRecord[2];
		faceIndices = (const uint8_t*)faceRecord + faceRecord[3];
		faceRecord += 4;
		faceVertexCount = faceIndices[0] & 0x3F;
		if (faceVertexCount == 2) {
			continue;
		}
		++faceIndices;

		vertexIndex = faceIndices[0];
		vertex = packedVertexData + 6 * vertexIndex;
		component = vertex;
		while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
			component -= 3 * (*(const uint16_t*)component & 0xFE);
		}
		pointX = *(const int16_t*)component;
		deltaStartX = startX - pointX;
		deltaEndX = endX - pointX;
		component = vertex + 2;
		while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
			component -= 3 * (*(const uint16_t*)component & 0xFE);
		}
		pointY = *(const int16_t*)component;
		deltaStartY = startY - pointY;
		deltaEndY = endY - pointY;
		component = vertex + 4;
		while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
			component -= 3 * (*(const uint16_t*)component & 0xFE);
		}
		pointZ = *(const int16_t*)component;
		deltaStartZ = startZ - pointZ;
		deltaEndZ = endZ - pointZ;

		startPlaneDistance = Math_Dot3Q15(normalX, normalY, normalZ, deltaStartX, deltaStartY, deltaStartZ);
		if (startPlaneDistance > -10 && startPlaneDistance < 10) {
			startPlaneDistance = 0;
		}
		endPlaneDistance = Math_Dot3Q15(normalX, normalY, normalZ, deltaEndX, deltaEndY, deltaEndZ);
		if (endPlaneDistance > -10 && endPlaneDistance < 10) {
			endPlaneDistance = 0;
		}
		planeSigns = startPlaneDistance ^ endPlaneDistance;
		if (startPlaneDistance == 0 || endPlaneDistance == 0) {
			planeSigns = -1;
		}
		if (planeSigns >= 0) {
			continue;
		}

		// Restore node-space endpoints after the plane-distance calculation.
		deltaStartX += pointX;
		deltaEndX += pointX;
		deltaStartY += pointY;
		deltaEndY += pointY;
		deltaStartZ += pointZ;
		deltaEndZ += pointZ;

		if (startPlaneDistance == 0) {
			pointX = deltaStartX;
			projectedPointU = deltaStartY;
			projectedPointV = deltaStartZ;
			hitFractionQ15 = 0x7FFF;
		} else if (endPlaneDistance == 0) {
			pointX = deltaEndX;
			projectedPointU = deltaEndY;
			projectedPointV = deltaEndZ;
			hitFractionQ15 = 0;
		} else {
			hitFractionQ15 =
				(int)((unsigned int)endPlaneDistance << 15) / (endPlaneDistance - startPlaneDistance);
			pointX = Math_MulQ15(deltaStartX - deltaEndX, hitFractionQ15);
			pointX += deltaEndX;
			projectedPointU = Math_MulQ15(deltaStartY - deltaEndY, hitFractionQ15);
			projectedPointU += deltaEndY;
			projectedPointV = Math_MulQ15(deltaStartZ - deltaEndZ, hitFractionQ15);
			projectedPointV += deltaEndZ;
		}

		if (normalX < 0) {
			normalX = -normalX;
		}
		if (normalY < 0) {
			normalY = -normalY;
		}
		if (normalZ < 0) {
			normalZ = -normalZ;
		}
		if (normalZ >= normalY && normalZ >= normalX) {
			projectionAxisU = 0;
			projectionAxisV = 1;
			projectedPointV = projectedPointU;
			projectedPointU = pointX;
		} else if (normalY >= normalX && normalZ <= normalY) {
			projectionAxisU = 0;
			projectionAxisV = 2;
			projectedPointU = pointX;
		} else {
			projectionAxisU = 1;
			projectionAxisV = 2;
		}

		{
			int previousU;
			int previousV;
			int currentU;
			int currentV;
			int firstEdgeIsNonpositive;

			vertexIndex = faceIndices[0];
			component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisU;
			while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
				component -= 3 * (*(const uint16_t*)component & 0xFE);
			}
			previousU = *(const int16_t*)component;
			component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisV;
			while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
				component -= 3 * (*(const uint16_t*)component & 0xFE);
			}
			previousV = *(const int16_t*)component;
			vertexIndex = faceIndices[2];
			component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisU;
			while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
				component -= 3 * (*(const uint16_t*)component & 0xFE);
			}
			currentU = *(const int16_t*)component;
			component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisV;
			while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
				component -= 3 * (*(const uint16_t*)component & 0xFE);
			}
			currentV = *(const int16_t*)component;

			firstEdgeIsNonpositive = collide_IsLegacyProjectedEdgeCrossNonpositive(
				projectedPointU - previousU, currentV - previousV, projectedPointV - previousV,
				currentU - previousU);
			pointInsideFace = 1;
			do {
				previousU = currentU;
				previousV = currentV;
				vertexIndex = faceIndices[4];
				component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisU;
				while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
					component -= 3 * (*(const uint16_t*)component & 0xFE);
				}
				currentU = *(const int16_t*)component;
				component = packedVertexData + 6 * vertexIndex + 2 * projectionAxisV;
				while ((*(const uint16_t*)component & 0xFF00) == 0x7F00) {
					component -= 3 * (*(const uint16_t*)component & 0xFE);
				}
				currentV = *(const int16_t*)component;
				if (collide_IsLegacyProjectedEdgeCrossNonpositive(
						projectedPointU - previousU, currentV - previousV, projectedPointV - previousV,
						currentU - previousU) != firstEdgeIsNonpositive) {
					pointInsideFace = 0;
					break;
				}
				faceIndices += 2;
			} while (--faceVertexCount != 0);
		}

		if (g_flightMissionState.provingGroundsModeActive != 0 && stopOnFirstHit != 0) {
			return (unsigned int)(hitFractionQ15 | 1);
		}
		if (pointInsideFace != 0) {
			hitFractionQ15 |= 1;
			if (nearestHitFractionQ15 > (unsigned int)hitFractionQ15) {
				nearestHitFractionQ15 = (unsigned int)hitFractionQ15;
			}
		}
	}
	return nearestHitFractionQ15 == 0x7FFFFFFF ? 0 : nearestHitFractionQ15;
}

// FUNCTION: XVT 0x41FAC0
unsigned int collide_ComputeCraftDamageAmount(uint16_t victimObjIdx, uint16_t sourceObjIdx) {
	MobileObject* mobileObject;
	int projectileType;
	unsigned int loadedWarheadCount;
	unsigned int launcherDamage;
	int weaponSlotIndex;
	unsigned int launcherIndex;
	int modelIndex;
	unsigned int damageAmount;
	CraftData* craft;

	mobileObject = g_objectTable[sourceObjIdx].mobj;
	damageAmount = mobileObject->damageAmount;
	if (g_missionFileVersion == 14) {
		if (g_objectTable[victimObjIdx].genusId == 4 || g_objectTable[victimObjIdx].genusId == 5) {
			craft = mobileObject->pCraft;
			modelIndex = GetModelIndexFromType(g_objectTable[sourceObjIdx].objectType);
			for (launcherIndex = 0; launcherIndex < craft->warheadLauncherCount; ++launcherIndex) {
				weaponSlotIndex = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIndex];
				loadedWarheadCount = craft->weaponSlots[weaponSlotIndex].count;
				weaponSlotIndex = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIndex];
				loadedWarheadCount += craft->weaponSlots[weaponSlotIndex].count;
				projectileType = craft->warheadSlotTypeIds[launcherIndex];
				if (g_objectTable[sourceObjIdx].genusId == 4 &&
					g_objectTable[sourceObjIdx].objectType == 48 &&
					g_objectTable[victimObjIdx].objectType == 54 &&
					craft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
					(uint8_t)projectileType == WARHEAD_OBJECT_TYPE_SPACE_BOMB) {
					launcherDamage = 4800000;
				} else {
					launcherDamage =
						(loadedWarheadCount * g_projectileDamageByObjectType
												  .damage[projectileType - PROJECTILE_OBJECT_TYPE_FIRST]) >>
						3;
				}
				damageAmount += launcherDamage;
			}
		}
	}
	if (g_objectTable[sourceObjIdx].objectType == 54) {
		damageAmount *= 16;
	}

	return damageAmount;
}

// FUNCTION: XVT 0x426060
int collide_IsLegacyProjectedEdgeCrossNonpositive(int pointDeltaU, int edgeDeltaV, int pointDeltaV,
												  int edgeDeltaU) {
	return pointDeltaV * edgeDeltaU - edgeDeltaV * pointDeltaU >= 0;
}

// FUNCTION: XVT 0x4A5490
int collide_CheckSweptModelCollision(uint16_t sourceObjIdx, uint16_t targetObjIdx) {
	enum { FIRST_OPT_OBJECT_TYPE = 73, SPECIAL_OBJECT_TYPE = 54 };

	ObjectRecord* target = &g_objectTable[targetObjIdx];
	int worldX;
	int worldY;
	int worldZ;
	int endX;
	int startX;
	int endY;
	int startY;
	int startYCopy;
	int endZ;
	int startZ;
	int sideStart;
	int fwdStart;
	int upStart;
	int sideEnd;
	int fwdEnd;
	int upEnd;
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	unsigned int rootIndex;
	OptNode* root;
	MeshDescriptor* descriptor;
	int descriptorIndex;
	unsigned int descriptorType;

	if (target->mobj != NULL)
		g_curCraft = target->mobj->pCraft;
	worldX = target->world_x;
	endX = g_collisionProbeWorldX - worldX;
	startX = g_collisionSegmentStartWorldX - worldX;
	worldY = target->world_y;
	endY = g_collisionProbeWorldY - worldY;
	startY = g_collisionSegmentStartWorldY - worldY;
	startYCopy = startY;
	worldZ = target->world_z;
	endZ = g_collisionProbeWorldZ - worldZ;
	startZ = g_collisionSegmentStartWorldZ - worldZ;
	{
		if (target->mobj != NULL) {
			if (target->mobj->orientMatrixDirty != 0) {
				FVIEW_calcrotatemove(target->pitch, target->yaw, target);
				FVIEW_calcrotateorient(target->roll, 0, target);
			}
			sideEnd = Math_Dot3Q15(target->mobj->cachedSideX, target->mobj->cachedSideY,
								   target->mobj->cachedSideZ, endX, endY, endZ);
			fwdEnd = -Math_Dot3Q15(target->mobj->cachedFwdX, target->mobj->cachedFwdY,
								   target->mobj->cachedFwdZ, endX, endY, endZ);
			upEnd = Math_Dot3Q15(target->mobj->cachedUpX, target->mobj->cachedUpY, target->mobj->cachedUpZ,
								 endX, endY, endZ);
			sideStart = Math_Dot3Q15(target->mobj->cachedSideX, target->mobj->cachedSideY,
									 target->mobj->cachedSideZ, startX, startYCopy, startZ);
			fwdStart = -Math_Dot3Q15(target->mobj->cachedFwdX, target->mobj->cachedFwdY,
									 target->mobj->cachedFwdZ, startX, startYCopy, startZ);
			upStart = Math_Dot3Q15(target->mobj->cachedUpX, target->mobj->cachedUpY, target->mobj->cachedUpZ,
								   startX, startYCopy, startZ);
		} else {
			FVIEW_calcrotatemove(target->pitch, target->yaw, NULL);
			FVIEW_calcrotateorient(target->roll, 0, NULL);
			sideEnd = Math_Dot3Q15(g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, endX, endY, endZ);
			fwdEnd = -Math_Dot3Q15(g_fviewForwardX_Q15, g_fviewForwardY_Q15, g_fviewForwardZ_Q15, endX, endY,
								   endZ);
			upEnd = Math_Dot3Q15(g_fviewUpX_Q15, g_fviewUpY_Q15, g_fviewUpZ_Q15, endX, endY, endZ);
			sideStart = Math_Dot3Q15(g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, startX, startYCopy,
									 startZ);
			fwdStart = -Math_Dot3Q15(g_fviewForwardX_Q15, g_fviewForwardY_Q15, g_fviewForwardZ_Q15, startX,
									 startYCopy, startZ);
			upStart =
				Math_Dot3Q15(g_fviewUpX_Q15, g_fviewUpY_Q15, g_fviewUpZ_Q15, startX, startYCopy, startZ);
		}
		g_collideSweepWalkerStart.x = (float)sideStart;
		g_collideSweepWalkerStart.y = (float)fwdStart;
		g_collideSweepWalkerStart.z = (float)upStart;
		g_collideSweepWalkerEnd.x = (float)sideEnd;
		g_collideSweepWalkerEnd.y = (float)fwdEnd;
		g_collideSweepWalkerEnd.z = (float)upEnd;
		g_collideSweepWalkerStartSaved = g_collideSweepWalkerStart;
		g_collideSweepWalkerEndSaved = g_collideSweepWalkerEnd;
	}
	g_collideSweepHitMeshOrdinal = 0;
	g_collideSweepHitFraction = 2.0f;
#ifdef XVT_MODERN
	/* Gunner obstruction checks can include ACT explosions left in craft slots.
	 * Relocating their data as a native OPT corrupts the sprite frame table. */
	if ((g_modelTypeTable[target->objectType].assetFlags & 1) == 0) {
		g_collideSweepCurrentMeshOrdinal = 0;
		g_collideCurrentMeshVertsNode = NULL;
		return 0;
	}
#endif
	modelHandle = g_loadedModels[target->objectType];
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	if (model == NULL)
		return 0;
	if (model->selfMarker != model)
		OptModel_AdjustOptimizedPolyObjectPointers(model);
	g_collideSweepCurrentMeshOrdinal = 0;
	g_collideCurrentMeshVertsNode = NULL;
	for (rootIndex = 0; rootIndex < (unsigned int)model->rootNodeCount; ++rootIndex) {
		g_collideCurrentMeshRotationAngle = 0.0f;
		root = model->rootNodes[rootIndex];
		if (root->nodeType == OPT_TEXTURE)
			continue;
		++g_collideSweepCurrentMeshOrdinal;
		if (sourceObjIdx == targetObjIdx) {
			int targetType = g_objectTable[targetObjIdx].objectType;
			int componentIndex = g_collideSweepCurrentMeshOrdinal - 1;
			int componentType;
			if (targetType < FIRST_OPT_OBJECT_TYPE) {
				if (componentIndex < 0)
					componentType = MESH_COMPONENT_00_HULL;
				else {
					int count = g_objectTypeMeshCache[targetType].meshCount;
					if (componentIndex >= count)
						componentIndex = count - 1;
					componentType = g_objectTypeMeshCache[targetType].meshTypes[componentIndex];
				}
			} else {
				componentType = ModelMesh_GetObjectTypeMeshType(targetType, componentIndex);
			}
			if (componentType == MESH_COMPONENT_04_LASR_TUR || componentType == MESH_COMPONENT_05_LASR_GUN ||
				componentType == MESH_COMPONENT_21_LASR_TUR ||
				(g_objectTable[targetObjIdx].objectType == SPECIAL_OBJECT_TYPE &&
				 g_collideSweepCurrentMeshOrdinal - g_warheadLaunchHullMeshOrdinal == 1))
				continue;
		}
		if (target->mobj != NULL && target->mobj->pCraft != NULL) {
			if (target->mobj->pCraft->componentHp[g_collideSweepCurrentMeshOrdinal - 1] == 0)
				continue;
			if (target->mobj->pCraft->meshRotation[g_collideSweepCurrentMeshOrdinal - 1] == 0 ||
				g_flightMissionState.provingGroundsModeActive != 0 ||
				g_objectTable[sourceObjIdx].playerOwnerIdx == -1) {
				g_collideCurrentMeshRotationAngle =
					(float)target->mobj->pCraft->meshRotation[g_collideSweepCurrentMeshOrdinal - 1] *
					0.024543673f;
			}
		}
		descriptorIndex = g_collideSweepCurrentMeshOrdinal - 1;
		descriptorType = g_objectTable[targetObjIdx].objectType;
		if (descriptorType < FIRST_OPT_OBJECT_TYPE) {
			if (descriptorIndex < 0)
				descriptor = NULL;
			else {
				int count = g_objectTypeMeshCache[descriptorType].meshCount;
				if (descriptorIndex >= count)
					descriptorIndex = count - 1;
				descriptor = g_objectTypeMeshCache[descriptorType].meshDescriptors[descriptorIndex];
			}
		} else {
			descriptor = ModelMesh_GetDescriptor(descriptorType, g_collideSweepCurrentMeshOrdinal - 1);
		}
		if (descriptor != NULL &&
			((sideEnd >= (int)descriptor->boxMin.x || sideStart >= (int)descriptor->boxMin.x) &&
			 (fwdEnd >= (int)descriptor->boxMin.y || fwdStart >= (int)descriptor->boxMin.y) &&
			 (upEnd >= (int)descriptor->boxMin.z || upStart >= (int)descriptor->boxMin.z) &&
			 (sideEnd <= (int)descriptor->boxMax.x || sideStart <= (int)descriptor->boxMax.x) &&
			 (fwdEnd <= (int)descriptor->boxMax.y || fwdStart <= (int)descriptor->boxMax.y) &&
			 (upEnd <= (int)descriptor->boxMax.z || upStart <= (int)descriptor->boxMax.z))) {
			collide_TestSweepAgainstOptNode(model, root);
			g_collideSweepWalkerStart = g_collideSweepWalkerStartSaved;
			g_collideSweepWalkerEnd = g_collideSweepWalkerEndSaved;
		}
	}
	Memory_UnlockHandle(modelHandle);
	if (g_collideSweepHitMeshOrdinal != 0) {
		g_collideSweepHitFraction -= 0.1f;
		if (g_collideSweepHitFraction < 0.0f)
			g_collideSweepHitFraction = 0.0f;
		g_collisionHitOffsetX =
			(int)((g_collisionProbeWorldX - g_collisionSegmentStartWorldX) * g_collideSweepHitFraction);
		g_collisionHitOffsetY =
			(int)((g_collisionProbeWorldY - g_collisionSegmentStartWorldY) * g_collideSweepHitFraction);
		g_collisionHitOffsetZ =
			(int)((g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ) * g_collideSweepHitFraction);
	}
	return g_collideSweepHitMeshOrdinal;
}

// FUNCTION: XVT 0x4A6080
int collide_TestSweepAgainstOptNode(OptimizedPolyObject* object, OptNode* node) {
	int childSelection;
	OptPackedFaceData* faceData;
	const OptPackedFaceRecord* face;
	const OptVector* faceNormal;
	const OptVector* meshVertices;
	const OptVector* bounds;
	int faceIndex;
	float hitFraction;
	float projectedPoint[3];
	const CollideOptRotationScale* rotationScale;
	uint32_t rotationAngleBits;
	float axisAngle[4];
	float rotationMatrix[16];
	int childIndex;

	for (;;) {
		if (node == NULL)
			return 0;

		childSelection = 0;
		while (node->nodeType == OPT_NODEREF) {
			if (g_cacheResolvedOptNodeRefs != 0) {
#ifdef XVT_MODERN
				node = XvtOpt_ResolveCached(object, node);
#else

				if (*(char*)node->param2 != '\0') {
					node->pName = (char*)OptModel_ResolveNodeRef(object, (const char*)node->param2);
					*(char*)node->param2 = '\0';
				}
				node = (OptNode*)node->pName;
#endif
			} else {
				node = OptModel_ResolveNodeRef(object, (const char*)node->param2);
			}
			if (node == NULL)
				return 0;
		}

		switch (node->nodeType) {
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17:
				faceData = (OptPackedFaceData*)node->param2;
				face = faceData->records;
				faceNormal = (const OptVector*)&faceData->records[node->param1];
				meshVertices = (const OptVector*)g_collideCurrentMeshVertsNode->param2;
				faceIndex = 0;
				if (node->param1 > 0) {
					do {
						if (collide_IntersectSegmentWithFacePlane(
								&faceNormal->x, &meshVertices[face->vertexIndices[0]].x,
								&g_collideSweepWalkerStart.x, &g_collideSweepWalkerEnd.x,
								&hitFraction) != 0 &&
							(g_collideSweepRejectNearStartHits == 0 || hitFraction >= 0.1f) &&
							g_collideSweepHitFraction > hitFraction) {
							projectedPoint[0] =
								(g_collideSweepWalkerEnd.x - g_collideSweepWalkerStart.x) * hitFraction +
								g_collideSweepWalkerStart.x;
							projectedPoint[1] =
								(g_collideSweepWalkerEnd.y - g_collideSweepWalkerStart.y) * hitFraction +
								g_collideSweepWalkerStart.y;
							projectedPoint[2] =
								(g_collideSweepWalkerEnd.z - g_collideSweepWalkerStart.z) * hitFraction +
								g_collideSweepWalkerStart.z;
							if (collide_PointInFacePolygon(&faceNormal->x, &meshVertices->x,
														   face->vertexIndices, projectedPoint) != 0) {
								g_collideSweepHitFraction = hitFraction;
								g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
							}
						}
						++faceNormal;
						++face;
						++faceIndex;
					} while (node->param1 > faceIndex);
				}
				break;

			case OPT_MESHVERTS:
				g_collideCurrentMeshVertsNode = node;
				meshVertices = (const OptVector*)node->param2;
				bounds = &meshVertices[node->param1 - 2];
				if (bounds[0].x > g_collideSweepWalkerStart.x && bounds[0].x > g_collideSweepWalkerEnd.x)
					return 1;
				if (bounds[0].y > g_collideSweepWalkerStart.y && bounds[0].y > g_collideSweepWalkerEnd.y)
					return 1;
				if (bounds[0].z > g_collideSweepWalkerStart.z && bounds[0].z > g_collideSweepWalkerEnd.z)
					return 1;
				++bounds;
				if (bounds->x < g_collideSweepWalkerStart.x && bounds->x < g_collideSweepWalkerEnd.x)
					return 1;
				if (bounds->y < g_collideSweepWalkerStart.y && bounds->y < g_collideSweepWalkerEnd.y)
					return 1;
				if (bounds->z < g_collideSweepWalkerStart.z && bounds->z < g_collideSweepWalkerEnd.z)
					return 1;
				break;

			case OPT_FACEGROUP:
				childSelection = 1;
				break;

			case OPT_ROTSCALE:
				memcpy(&rotationAngleBits, &g_collideCurrentMeshRotationAngle, sizeof(rotationAngleBits));
				if ((rotationAngleBits & 0x7FFFFFFFu) != 0) {
					rotationScale = (const CollideOptRotationScale*)node->param2;
					g_collideSweepWalkerStart.x -= rotationScale->origin.x;
					g_collideSweepWalkerStart.y -= rotationScale->origin.y;
					g_collideSweepWalkerStart.z -= rotationScale->origin.z;
					g_collideSweepWalkerEnd.x -= rotationScale->origin.x;
					g_collideSweepWalkerEnd.y -= rotationScale->origin.y;
					g_collideSweepWalkerEnd.z -= rotationScale->origin.z;
					axisAngle[0] = rotationScale->axis.x * g_collideOptAxisQ15ToFloatScale;
					axisAngle[1] = rotationScale->axis.y * g_collideOptAxisQ15ToFloatScale;
					axisAngle[2] = rotationScale->axis.z * g_collideOptAxisQ15ToFloatScale;
					axisAngle[3] = g_collideCurrentMeshRotationAngle;
					Math3D_BuildAxisAngleMatrix(rotationMatrix, axisAngle);
					Math3D_RotateVec3(&g_collideSweepWalkerStart.x, rotationMatrix);
					Math3D_RotateVec3(&g_collideSweepWalkerEnd.x, rotationMatrix);
					g_collideSweepWalkerStart.x += rotationScale->origin.x;
					g_collideSweepWalkerStart.y += rotationScale->origin.y;
					g_collideSweepWalkerStart.z += rotationScale->origin.z;
					g_collideSweepWalkerEnd.x += rotationScale->origin.x;
					g_collideSweepWalkerEnd.y += rotationScale->origin.y;
					g_collideSweepWalkerEnd.z += rotationScale->origin.z;
				}
				break;

			default:
				break;
		}

		if (node->childCount == 0)
			return 0;
		if (childSelection != 0) {
			if (childSelection == -1)
				return 0;
			node = node->pChildren[childSelection - 1];
			continue;
		}
		if (node->childCount <= 0)
			return 0;
		for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
			if (collide_TestSweepAgainstOptNode(object, node->pChildren[childIndex]) != 0)
				return 1;
		}
		return 0;
	}
}

// FUNCTION: XVT 0x4A6560
int collide_IntersectSegmentWithFacePlane(const float* faceNormal, const float* faceVertex,
										  const float* segmentStart, const float* segmentEnd, float* outT) {
	float startDistance;
	float endDistance;

	startDistance = (segmentStart[2] - faceVertex[2]) * faceNormal[2] +
					((segmentStart[1] - faceVertex[1]) * faceNormal[1] +
					 (segmentStart[0] - faceVertex[0]) * faceNormal[0]);
	endDistance =
		(segmentEnd[2] - faceVertex[2]) * faceNormal[2] +
		((segmentEnd[1] - faceVertex[1]) * faceNormal[1] + (segmentEnd[0] - faceVertex[0]) * faceNormal[0]);

	if (startDistance < 10.0f && startDistance > -10.0f) {
		startDistance = 0.0f;
	}
	if (endDistance < 10.0f && endDistance > -10.0f) {
		endDistance = 0.0f;
	}
	if (startDistance == 0.0f) {
		*outT = 0.0f;
		return 1;
	}
	if (endDistance == 0.0f) {
		*outT = 1.0f;
		return 1;
	}
	if (startDistance < 0.0f && endDistance > 0.0f) {
		*outT = startDistance;
		*outT = startDistance / (endDistance - startDistance);
		if (*outT < g_collideZeroFloat) {
			*outT = -*outT;
		}
		return 1;
	}
	if (endDistance < 0.0f && startDistance > 0.0f) {
		*outT = startDistance;
		*outT = startDistance / (startDistance - endDistance);
		if (*outT < g_collideZeroFloat) {
			*outT = -*outT;
		}
		return 1;
	}
	return 0;
}

// FUNCTION: XVT 0x4A66D0
int collide_PointInFacePolygon(const float* faceNormal, const float* vertexCoords,
							   const int32_t* faceVertexIndices, float* projectedPoint) {
	float absX;
	float absY;
	float absZ;
#ifdef XVT_MODERN
	uint32_t signBits;
#endif
	int axisU;
	int axisV;
	int vertexBase;
	const float* vertex0U;
	const float* vertex0V;
	float vertexU;
	float vertexV;
	float previousU;
	float previousV;
	float edgeCross;
	int firstEdgeNegative;
	int vertexIndex;

	absX = faceNormal[0];
	absY = faceNormal[1];
	absZ = faceNormal[2];
#ifdef XVT_MODERN
	memcpy(&signBits, &absX, sizeof(signBits));
	if (signBits > 0x80000000u)
#else
	if (*(const uint32_t*)&absX > 0x80000000u)
#endif
		absX = -absX;
#ifdef XVT_MODERN
	memcpy(&signBits, &absY, sizeof(signBits));
	if (signBits > 0x80000000u)
#else
	if (*(const uint32_t*)&absY > 0x80000000u)
#endif
		absY = -absY;
#ifdef XVT_MODERN
	memcpy(&signBits, &absZ, sizeof(signBits));
	if (signBits > 0x80000000u)
#else
	if (*(const uint32_t*)&absZ > 0x80000000u)
#endif
		absZ = -absZ;

	if (absZ >= absY && absZ >= absX) {
		axisU = 0;
		axisV = 1;
		projectedPoint[2] = projectedPoint[1];
		projectedPoint[1] = projectedPoint[0];
	} else if (absY >= absX && absZ <= absY) {
		axisU = 0;
		axisV = 2;
		projectedPoint[1] = projectedPoint[0];
	} else {
		axisU = 1;
		axisV = 2;
	}

	vertex0U = &vertexCoords[axisU + 3 * faceVertexIndices[0]];
	vertex0V = &vertexCoords[axisV + 3 * faceVertexIndices[0]];
	previousU = *vertex0U;
	previousV = *vertex0V;
	vertexBase = 3 * faceVertexIndices[1];
	vertexU = vertexCoords[axisU + vertexBase];
	vertexV = vertexCoords[axisV + vertexBase];
	if ((projectedPoint[1] - previousU) * (vertexV - previousV) -
			(projectedPoint[2] - previousV) * (vertexU - previousU) <
		g_collideZeroFloat) {
		firstEdgeNegative = 1;
	} else {
		firstEdgeNegative = 0;
	}

	previousU = vertexU;
	previousV = vertexV;
	vertexBase = 3 * faceVertexIndices[2];
	vertexU = vertexCoords[axisU + vertexBase];
	vertexV = vertexCoords[axisV + vertexBase];
	edgeCross = (projectedPoint[1] - previousU) * (vertexV - previousV) -
				(projectedPoint[2] - previousV) * (vertexU - previousU);
	if (edgeCross < g_collideZeroFloat && !firstEdgeNegative)
		return 0;
#ifdef XVT_MODERN
	memcpy(&signBits, &edgeCross, sizeof(signBits));
	if (signBits <= 0x80000000u && firstEdgeNegative)
#else
	if (*(const uint32_t*)&edgeCross <= 0x80000000u && firstEdgeNegative)
#endif
		return 0;

	vertexIndex = faceVertexIndices[3];
	if (vertexIndex != -1) {
		previousU = vertexU;
		previousV = vertexV;
		vertexBase = 3 * vertexIndex;
		vertexU = vertexCoords[axisU + vertexBase];
		vertexV = vertexCoords[axisV + vertexBase];
		edgeCross = (projectedPoint[1] - previousU) * (vertexV - previousV) -
					(projectedPoint[2] - previousV) * (vertexU - previousU);
		if (edgeCross < g_collideZeroFloat && !firstEdgeNegative)
			return 0;
#ifdef XVT_MODERN
		memcpy(&signBits, &edgeCross, sizeof(signBits));
		if (signBits <= 0x80000000u && firstEdgeNegative)
#else
		if (*(const uint32_t*)&edgeCross <= 0x80000000u && firstEdgeNegative)
#endif
			return 0;
	}

	edgeCross = (*vertex0V - vertexV) * (projectedPoint[1] - vertexU) -
				(projectedPoint[2] - vertexV) * (*vertex0U - vertexU);
	if (edgeCross < g_collideZeroFloat && !firstEdgeNegative)
		return 0;
#ifdef XVT_MODERN
	memcpy(&signBits, &edgeCross, sizeof(signBits));
	if (signBits <= 0x80000000u && firstEdgeNegative)
#else
	if (*(const uint32_t*)&edgeCross <= 0x80000000u && firstEdgeNegative)
#endif
		return 0;
	return 1;
}

// FUNCTION: XVT 0x4A8740
void collide_ApplyEngineWashDamage(int victimObjIdx, int sourceObjIdx) {
	enum {
		FIRST_OPT_OBJECT_TYPE = 73,
		SPECIAL_ENGINE_WASH_OBJECT_TYPE = 54,
		ASYMMETRIC_ENGINE_WASH_OBJECT_TYPE = 49,
		ENGINE_WASH_RANGE_SCALE = 3,
		ENGINE_WASH_LENGTH_SCALE = 8,
		ENGINE_WASH_MAX_DAMAGE = 64,
		ENGINE_WASH_PERCENT_SCALE = 100,
	};

	ObjectRecord* source = &g_objectTable[sourceObjIdx];
	int sideExtent;
	int upExtent;
	int meshIndex;
	int depthIntoWash;
	uint8_t sourceObjectType;
	int localUp;
	int meshCount;
	int localSide;
	int localForward;
	int sourceBoundsExtent = g_modelTypeTable[source->objectType].maxBoundsExtent;
	int victimObjectIndex = victimObjIdx;
	int deltaX = g_objectTable[victimObjectIndex].world_x - source->world_x;
	int deltaY = g_objectTable[victimObjectIndex].world_y - source->world_y;
	int deltaZ = g_objectTable[victimObjectIndex].world_z - source->world_z;
	unsigned int objectType;

	if (ENGINE_WASH_RANGE_SCALE * sourceBoundsExtent < collide_roughdistance3d(deltaX, deltaY, deltaZ))
		return;

	if (source->mobj == NULL)
		return;
	if (source->mobj->orientMatrixDirty != 0) {
		FVIEW_calcrotatemove(source->pitch, source->yaw, source);
		FVIEW_calcrotateorient(source->roll, 0, source);
	}

	localSide = Math_Dot3Q15(source->mobj->cachedSideX, source->mobj->cachedSideY, source->mobj->cachedSideZ,
							 deltaX, deltaY, deltaZ);
	localForward = -Math_Dot3Q15(source->mobj->cachedFwdX, source->mobj->cachedFwdY, source->mobj->cachedFwdZ,
								 deltaX, deltaY, deltaZ);
	if (localForward < -sourceBoundsExtent)
		return;
	localUp = Math_Dot3Q15(source->mobj->cachedUpX, source->mobj->cachedUpY, source->mobj->cachedUpZ, deltaX,
						   deltaY, deltaZ);

	objectType = source->objectType;
	if (objectType < FIRST_OPT_OBJECT_TYPE)
		meshCount = g_objectTypeMeshCache[objectType].meshCount;
	else
		meshCount = ModelMesh_GetObjectTypeMeshCount((int)objectType);

	for (meshIndex = 0; meshCount > meshIndex; ++meshIndex) {
		MeshDescriptor* descriptor;
		unsigned int descriptorObjectType = source->objectType;
		int engineMeshExtent;
		int washLength;
		int sideOffset;
		int upOffset;
		int washDamage;
		ObjectRecord* victim;
		int playerOwnerIdx;

		if (descriptorObjectType < FIRST_OPT_OBJECT_TYPE) {
			if (meshIndex < 0) {
				descriptor = NULL;
			} else {
				int descriptorIndex = meshIndex;

				if (descriptorIndex >= g_objectTypeMeshCache[descriptorObjectType].meshCount)
					descriptorIndex = g_objectTypeMeshCache[descriptorObjectType].meshCount - 1;
				descriptor = g_objectTypeMeshCache[descriptorObjectType].meshDescriptors[descriptorIndex];
			}
		} else {
			descriptor = ModelMesh_GetDescriptor((int)descriptorObjectType, meshIndex);
		}
		if (descriptor == NULL || descriptor->meshType != MESH_COMPONENT_06_ENGINE)
			continue;

		engineMeshExtent = (int)(descriptor->boxMax.y - descriptor->boxMin.y);
		sourceObjectType = source->objectType;
		if (sourceObjectType == SPECIAL_ENGINE_WASH_OBJECT_TYPE)
			engineMeshExtent >>= 6;
		sideExtent = (int)(descriptor->boxMax.x - descriptor->boxMin.x);
		upExtent = (int)(descriptor->boxMax.z - descriptor->boxMin.z);
		if (sourceObjectType == SPECIAL_ENGINE_WASH_OBJECT_TYPE && localUp > descriptor->boxMax.z)
			continue;
		if (sourceObjectType == ASYMMETRIC_ENGINE_WASH_OBJECT_TYPE &&
			descriptor->center.z < g_collideZeroFloat && localUp > descriptor->boxMax.z)
			continue;

		if (engineMeshExtent < sideExtent)
			engineMeshExtent = sideExtent;
		if (engineMeshExtent < upExtent)
			engineMeshExtent = upExtent;
		washLength = ENGINE_WASH_LENGTH_SCALE * engineMeshExtent;
		if (washLength > 2 * sourceBoundsExtent)
			washLength = 2 * sourceBoundsExtent;
		if (sourceObjectType == SPECIAL_ENGINE_WASH_OBJECT_TYPE)
			depthIntoWash = (washLength >> 8) - (int)descriptor->boxMax.y + localForward;
		else
			depthIntoWash = localForward - (int)descriptor->boxMin.y;
		if (depthIntoWash <= 0 || washLength < depthIntoWash)
			continue;

		sideOffset = localSide - (int)descriptor->center.x;
		upOffset = localUp - (int)descriptor->center.z;
		sideExtent += sideExtent >> 1;
		sideExtent += depthIntoWash * sideExtent / washLength;
		upExtent += upExtent >> 1;
		upExtent += depthIntoWash * upExtent / washLength;
		if (sideOffset < 0)
			sideOffset = -sideOffset;
		if (upOffset < 0)
			upOffset = -upOffset;
		if (sideExtent < sideOffset || upExtent < upOffset)
			continue;

		washDamage = (washLength >> 8) *
					 (ENGINE_WASH_PERCENT_SCALE * (washLength - depthIntoWash) / washLength *
					  (ENGINE_WASH_PERCENT_SCALE * (sideExtent + upExtent - upOffset - sideOffset) /
					   (sideExtent + upExtent)) /
					  ENGINE_WASH_PERCENT_SCALE) /
					 ENGINE_WASH_PERCENT_SCALE;
		if (sourceObjectType == SPECIAL_ENGINE_WASH_OBJECT_TYPE) {
			washDamage >>= 4;
			washDamage += washDamage >> 1;
		}
		if (washDamage > ENGINE_WASH_MAX_DAMAGE)
			washDamage = ENGINE_WASH_MAX_DAMAGE;

		victim = &g_objectTable[victimObjectIndex];
		playerOwnerIdx = victim->playerOwnerIdx;
		if (playerOwnerIdx != -1 && g_players[playerOwnerIdx].engineWashStrength < washDamage) {
			g_players[playerOwnerIdx].engineWashSourceObjIdx = (uint16_t)sourceObjIdx;
			g_players[playerOwnerIdx].engineWashStrength = (uint16_t)washDamage;
		}
		if (victim->mobj != NULL && victim->mobj->pCraft != NULL &&
			victim->mobj->pCraft->shieldEnergy[0] + victim->mobj->pCraft->shieldEnergy[1] == 0)
			washDamage >>= 3;
		if (washDamage < 1)
			washDamage = 1;
		collide_damagecraft((uint16_t)victimObjIdx, -1, UINT16_MAX - 1, (uint16_t)washDamage);
	}
}

// FUNCTION: XVT 0x4A8CC0
void collide_ApplyHostileProximityWeaponDisruption(int ownerObjIdx, int hostileObjIdx) {
	ObjectRecord* hostile = &g_objectTable[hostileObjIdx];
	int hostileBoundsExtent = g_modelTypeTable[hostile->objectType].maxBoundsExtent;
	int deltaX = g_objectTable[ownerObjIdx].world_x - hostile->world_x;
	int deltaY = g_objectTable[ownerObjIdx].world_y - hostile->world_y;
	int deltaZ = g_objectTable[ownerObjIdx].world_z - hostile->world_z;
	int roughDistance = collide_roughdistance3d(deltaX, deltaY, deltaZ);
	MobileObject* ownerMobj;

	if (2 * hostileBoundsExtent < roughDistance)
		return;
	if (hostile->mobj == NULL || hostile->mobj->pCraft->workingSubsystems == 0)
		return;

	if (hostile->objectType == CRAFT_SPECIES_X7_FACTORY) {
		if (roughDistance > 4096)
			return;
	} else {
		int localSide;
		int localFwd;
		int localUp;
		CraftData* hostileCraft;
		uint8_t modelIndex;
		int insideSide;
		int insideUp;
		int insideFwd;

		if (hostile->mobj->orientMatrixDirty != 0) {
			FVIEW_calcrotatemove(hostile->pitch, hostile->yaw, hostile);
			FVIEW_calcrotateorient(hostile->roll, 0, hostile);
		}

		localSide = Math_Dot3Q15(hostile->mobj->cachedSideX, hostile->mobj->cachedSideY,
								 hostile->mobj->cachedSideZ, deltaX, deltaY, deltaZ);
		localFwd = Math_Dot3Q15(hostile->mobj->cachedFwdX, hostile->mobj->cachedFwdY,
								hostile->mobj->cachedFwdZ, deltaX, deltaY, deltaZ);
		localUp = Math_Dot3Q15(hostile->mobj->cachedUpX, hostile->mobj->cachedUpY, hostile->mobj->cachedUpZ,
							   deltaX, deltaY, deltaZ);

		hostileCraft = hostile->mobj->pCraft;
		if (hostileCraft == NULL)
			return;

		modelIndex = hostileCraft->modelIndex;
		insideSide = g_modelDefs[modelIndex].hangarPoints.inside.side;
		insideUp = g_modelDefs[modelIndex].hangarPoints.inside.up;
		insideFwd = g_modelDefs[modelIndex].hangarPoints.inside.forward;

		if ((hostile->objectType == CRAFT_SPECIES_IMPERIAL_STAR_DESTROYER ||
			 hostile->objectType == CRAFT_SPECIES_INTERDICTOR ||
			 hostile->objectType == CRAFT_SPECIES_VICTORY_STAR_DESTROYER) &&
			localUp > 0)
			return;

		if (hostile->objectType == CRAFT_SPECIES_SUPER_STAR_DESTROYER) {
			int upDifference = localUp - insideUp;
			if (upDifference < 0)
				upDifference = -upDifference;
			if (insideUp < 0)
				insideUp = -insideUp;
			if (insideUp - (insideUp >> 2) < upDifference ||
				hostileBoundsExtent >> 3 <
					collide_roughdistance3d(localSide - insideSide, localFwd - insideFwd, upDifference))
				return;
		} else if (hostile->objectType == CRAFT_SPECIES_REPAIR_YARD) {
			int meshExtent = hostileBoundsExtent / 6;
			if (meshExtent <
				collide_roughdistance3d(localSide - insideSide, localFwd - insideFwd, localUp - insideUp)) {
				int objectType = hostile->objectType;
				int meshCount;
				int meshIndex;

				if (objectType < CRAFT_SPECIES_SAT_4)
					meshCount = g_objectTypeMeshCache[objectType].meshCount;
				else
					meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);

				meshIndex = 0;
				if (meshCount <= meshIndex)
					return;
				while (1) {
					MeshComponentType meshType;
					if (objectType < CRAFT_SPECIES_SAT_4) {
						if (meshIndex < 0)
							meshType = MESH_COMPONENT_00_HULL;
						else {
							int meshTypeIndex = meshIndex;
							if (meshIndex >= g_objectTypeMeshCache[objectType].meshCount)
								meshTypeIndex = g_objectTypeMeshCache[objectType].meshCount - 1;
							meshType = g_objectTypeMeshCache[objectType].meshTypes[meshTypeIndex];
						}
					} else {
						meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIndex);
					}
					if (meshType == MESH_COMPONENT_16_HANGAR) {
						int centerX = ModelMesh_GetCenterX(objectType, meshIndex);
						int centerZ = ModelMesh_GetCenterZ(objectType, meshIndex);
						int centerY = ModelMesh_GetCenterY(objectType, meshIndex);
						pai_RotateLocalVectorToWorldScratch(hostile, centerX, centerZ, -centerY);
						if (meshExtent > collide_roughdistance3d(localSide - g_rotatedX,
																 localFwd - g_rotatedY, localUp - g_rotatedZ))
							break;
					}
					if (meshCount <= ++meshIndex)
						return;
				}
			}
		} else {
			if (hostileBoundsExtent / 6 <
				collide_roughdistance3d(localSide - insideSide, localFwd - insideFwd, localUp - insideUp))
				return;
		}
	}

	ownerMobj = g_objectTable[ownerObjIdx].mobj;
	if (ownerMobj != NULL) {
		CraftData* ownerCraft = ownerMobj->pCraft;
		if (ownerCraft != NULL) {
			ownerCraft->beamEffectAccum[2] = 163840;
			ownerCraft->chaffActiveTimer = 0;
		}
	}
}
