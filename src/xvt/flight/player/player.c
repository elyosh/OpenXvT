#include "xvt/flight/player/player.h"
#ifdef XVT_MODERN
#include "xvt_runtime/hooks/orientation_hook.h"
#include "xvt_runtime/timing/flight_timing.h"
#include "xvt_runtime/timing/player_timing.h"
#endif
#ifdef XVT_MODERN
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/runtime/flight_diag.h"
#endif
#include "xvt/assets/model_mesh.h"
#include "xvt/assets/opt_model.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/ai/pai.h"
#include "xvt/flight/ai/paiman.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/flight_view.h"
#include "xvt/flight/fview.h"
#include "xvt/flight/hud/flight_text.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/hud/msg.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/flight/object/collide.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/targeting.h"
#include "xvt/flight/transfm2.h"
#include "xvt/math/math.h"
#include "xvt/math/math2.h"
#include "xvt/math/trig2.h"
#include "xvt/net/flight_net.h"
#include "xvt/net/net_session.h"
#include "xvt/render/renderer.h"

#include <limits.h>
#include <string.h>

// GLOBAL: XVT 0x9D8B80
PlayerFlightTransientTimers g_playerFlightTransientTimers[8];
// GLOBAL: XVT 0x9ECC34
int g_localPlayer;
// GLOBAL: XVT 0x9E9670
PlayerData g_players[8];
// GLOBAL: XVT 0x9D7800
char g_playerTauntText[8][4][70] = { { { 0 } } };

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x45A1E0
int Player_BindToAvailableCraft(int playerIdx, uint32_t previousObjectIdx, int preferredObjectSignature,
								int resetTargetingState) {
	enum {
		OBJECT_TYPE_NONE = 0,
		WEAPON_BANK_COUNT = 2,
		LASER_LINK_DEFAULT = 1,
		WARHEAD_LINK_PRESERVE_MASK = 0x81,
		WARHEAD_SAVED_STATE_PRESERVE_MASK = 0x80,
		WARHEAD_LINK_DEFAULT = 1,
		GUNNER_LASER_MOUNT_TYPE = 2,
		TARGET_OBJECT_INDEX_LIMIT = 0x8000,
		DEFAULT_CAMERA_DISTANCE = 1024,
	};

	CraftData* craft;
	int selectedObjectIdx;
	int objectsRemaining;
	int foundCraft;
	int matchedPreferredSignature;

	foundCraft = 0;
	matchedPreferredSignature = 0;
	if (preferredObjectSignature != 0) {
		for (selectedObjectIdx = g_activeRegionObjectSlotStart;
			 selectedObjectIdx < g_activeRegionCraftObjectSlotEnd; ++selectedObjectIdx) {
			if (g_objectTable[selectedObjectIdx].objectType != OBJECT_TYPE_NONE) {
				uint8_t objectKind;

				objectKind = g_objectTable[selectedObjectIdx].mobj->pCraft->objectKind;
				if (g_missionFlightGroups[g_objectTable[selectedObjectIdx].flightGroupIdx].playerOwnerIdx ==
					playerIdx) {
					if (objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						objectKind != CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE &&
						objectKind != CRAFT_OBJECT_KIND_EXPLODING &&
						g_objectTable[selectedObjectIdx].objectSignature == preferredObjectSignature) {
						foundCraft = 1;
						matchedPreferredSignature = 1;
						break;
					}
				}
			}
		}
	}

	if (foundCraft == 0) {
		selectedObjectIdx = previousObjectIdx;
		objectsRemaining = g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart;
		while (objectsRemaining != 0) {
			uint8_t objectKind;

			++selectedObjectIdx;
			if (selectedObjectIdx >= g_activeRegionCraftObjectSlotEnd)
				selectedObjectIdx = g_activeRegionObjectSlotStart;
			if (g_objectTable[selectedObjectIdx].objectType != OBJECT_TYPE_NONE) {
				objectKind = g_objectTable[selectedObjectIdx].mobj->pCraft->objectKind;
				if (g_missionFlightGroups[g_objectTable[selectedObjectIdx].flightGroupIdx].playerOwnerIdx ==
					playerIdx) {
					if (objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						objectKind != CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE &&
						objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
						break;
					}
				}
			}
			--objectsRemaining;
		}
		if (objectsRemaining == 0)
			return 1;
	}

	g_objectTable[selectedObjectIdx].playerOwnerIdx = playerIdx;
	g_objectTable[selectedObjectIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[selectedObjectIdx].mobj->moveVectorDirty = 1;
	collide_ResetObjectProximityForSlot((uint16_t)selectedObjectIdx);
	craft = g_objectTable[selectedObjectIdx].mobj->pCraft;
	{
		int weaponBank;

		for (weaponBank = 0; weaponBank < WEAPON_BANK_COUNT; ++weaponBank) {
			ModelIndex modelIndex;

			craft->laserState.linkMode[weaponBank] = LASER_LINK_DEFAULT;
			craft->laserState.burstRemaining[weaponBank] = 0;
			craft->laserState.nextSlot[weaponBank] = 0;
			craft->laserState.fireCooldownTicks[weaponBank] = 0;
			craft->laserState.lastFireTimestamp[weaponBank] = 0;
			modelIndex = GetModelIndexFromType(g_objectTable[selectedObjectIdx].objectType);
			if (craft->laserState.projectileTypeId[weaponBank] != 0 &&
				g_modelDefs[modelIndex].laserGroupMountType[weaponBank] != GUNNER_LASER_MOUNT_TYPE) {
				craft->laserState.nextSlot[weaponBank] =
					g_modelDefs[modelIndex].laserGroupFirstSlot[weaponBank];
			}
		}
	}
	craft->laserState.linkMode[0] = g_players[playerIdx].savedCraftSettings.laserLinkMode[0];
	craft->laserState.linkMode[1] = g_players[playerIdx].savedCraftSettings.laserLinkMode[1];
	{
		int launcherIndex;

		for (launcherIndex = 0; launcherIndex < WEAPON_BANK_COUNT; ++launcherIndex) {
			craft->warheadLauncherFlags[launcherIndex] =
				(int8_t)((craft->warheadLauncherFlags[launcherIndex] & WARHEAD_LINK_PRESERVE_MASK) |
						 WARHEAD_LINK_DEFAULT);
			craft->warheadLauncherCooldownTicks[launcherIndex] = 0;
		}
	}
	craft->warheadLauncherFlags[0] =
		(int8_t)((craft->warheadLauncherFlags[0] & WARHEAD_SAVED_STATE_PRESERVE_MASK) |
				 g_players[playerIdx].savedCraftSettings.warheadLauncherFlags[0]);
	craft->warheadLauncherFlags[1] =
		(int8_t)((craft->warheadLauncherFlags[1] & WARHEAD_SAVED_STATE_PRESERVE_MASK) |
				 g_players[playerIdx].savedCraftSettings.warheadLauncherFlags[1]);
	craft->warheadLockTicks = 0;
	{
		enum { FRONT_SHIELD = 0, REAR_SHIELD = 1 };

		int maxShieldPerFace;
		uint8_t shieldDistributionMode;

		maxShieldPerFace = 2 * g_modelDefs[craft->modelIndex].shieldStrength;
		shieldDistributionMode = g_players[playerIdx].savedCraftSettings.shieldDistribMode;
		craft->shieldDistribMode = shieldDistributionMode;
		switch (shieldDistributionMode) {
			case SHIELD_DISTRIBUTION_FULLY_FORWARD:
				if (craft->shieldEnergy[FRONT_SHIELD] > maxShieldPerFace) {
					craft->shieldEnergy[REAR_SHIELD] = craft->shieldEnergy[FRONT_SHIELD] - maxShieldPerFace;
					craft->shieldEnergy[FRONT_SHIELD] = maxShieldPerFace;
				}
				break;
			case SHIELD_DISTRIBUTION_EVEN:
				craft->shieldEnergy[FRONT_SHIELD] >>= 1;
				craft->shieldEnergy[REAR_SHIELD] = craft->shieldEnergy[FRONT_SHIELD];
				break;
			case SHIELD_DISTRIBUTION_FULLY_AFT:
				craft->shieldEnergy[REAR_SHIELD] = craft->shieldEnergy[FRONT_SHIELD];
				craft->shieldEnergy[FRONT_SHIELD] = 0;
				if (craft->shieldEnergy[REAR_SHIELD] > maxShieldPerFace) {
					craft->shieldEnergy[FRONT_SHIELD] = craft->shieldEnergy[REAR_SHIELD] - maxShieldPerFace;
					craft->shieldEnergy[REAR_SHIELD] = maxShieldPerFace;
				}
				break;
		}
	}
	craft->shieldRedirect = g_players[playerIdx].savedCraftSettings.shieldRedirect;
	craft->laserRedirect = g_players[playerIdx].savedCraftSettings.laserRedirect;
	craft->beamLevel = g_players[playerIdx].savedCraftSettings.beamLevel;

	g_players[playerIdx].objectIndex = selectedObjectIdx;
	g_players[playerIdx].boundObjectSignature = g_objectTable[selectedObjectIdx].objectSignature;
	g_players[playerIdx].regionSessionId = 0;
	g_players[playerIdx].hyperspacePhase = 0;
	if (matchedPreferredSignature != 0 || previousObjectIdx != UINT32_MAX)
		craft->throttleSpeed = g_players[playerIdx].savedCraftSettings.throttleSpeed;
	if (matchedPreferredSignature == 0) {
		g_players[playerIdx].selectedWarhead = 0;
		g_players[playerIdx].selectedWeaponMode = 0;
	}
	g_players[playerIdx].targetCycleStart = -1;
	g_players[playerIdx].targetingState = -1;
	g_players[playerIdx].selectedTargetComponent = 0;
	if (resetTargetingState == 1) {
		int16_t* targetPresetSlots;

		g_players[playerIdx].targetBoxEnabled = 1;
		g_players[playerIdx].currentTargetObjectIdx = -1;
		targetPresetSlots = g_players[playerIdx].targetPresetSlot;
		memset(targetPresetSlots, 0xFF, sizeof(g_players[playerIdx].targetPresetSlot));
	}
	if (previousObjectIdx != UINT32_MAX && g_objectTable[previousObjectIdx].mobj != NULL) {
		uint16_t targetObjectIdx;

		g_players[playerIdx].currentTargetObjectIdx = -1;
		targetObjectIdx = craft->aiController.targetObjIdx;
		if (targetObjectIdx < TARGET_OBJECT_INDEX_LIMIT) {
			g_players[playerIdx].currentTargetObjectIdx = (int16_t)targetObjectIdx;
			g_players[playerIdx].selectedTargetComponent = 0;
		}
	}
	if ((uint16_t)g_players[playerIdx].currentTargetObjectIdx == selectedObjectIdx)
		g_players[playerIdx].currentTargetObjectIdx = -1;
	Player_ValidateCurrentTargets(playerIdx);
	g_players[playerIdx].missileLockState = 0;
	craft->aiController.pendingPlanId = (uint8_t)pai_findplanbyname("nullpln");
	craft->aiController.targetObjIdx = UINT16_MAX;
	g_players[playerIdx].pendingActionTimer = 0;
	g_players[playerIdx].beamFireCooldownTimer = 0;
	g_players[playerIdx].yawRollSwap = 0;
	g_players[playerIdx].smoothedInputYaw = 0;
	g_players[playerIdx].smoothedInputPitch = 0;
	g_players[playerIdx].savedKeyMods = 0;
	g_players[playerIdx].keyModsHoldTimer = 0;
	g_players[playerIdx].engineWashSourceObjIdx = -1;
	memset(&g_playerFlightTransientTimers[playerIdx], 0, sizeof(g_playerFlightTransientTimers[playerIdx]));
	{
		ModelIndex modelIndex;

		modelIndex = GetModelIndexFromType(g_objectTable[g_players[playerIdx].objectIndex].objectType);
		pai_calcrotatedpoint(&g_objectTable[g_players[playerIdx].objectIndex], 0,
							 g_modelDefs[modelIndex].primaryHardpointZ,
							 g_modelDefs[modelIndex].primaryHardpointY);
	}
	g_players[playerIdx].hardpointWorldX = g_rotatedX;
	g_players[playerIdx].hardpointWorldY = g_rotatedY;
	g_players[playerIdx].hardpointWorldZ = g_rotatedZ;
	g_players[playerIdx].hardpointLocalX = g_players[playerIdx].hardpointWorldX;
	g_players[playerIdx].hardpointLocalY = g_players[playerIdx].hardpointWorldY;
	g_players[playerIdx].hardpointLocalZ = g_players[playerIdx].hardpointWorldZ;
	g_players[playerIdx].viewState.hudAimXSnapState = 0;
	g_players[playerIdx].viewState.hudAimX = 0;
	g_players[playerIdx].viewState.hudAimY = 0;
	g_players[playerIdx].viewState.externalCameraActive = 0;
	g_players[playerIdx].viewState.cameraDistance = DEFAULT_CAMERA_DISTANCE;
	g_players[playerIdx].viewState.playerInputBlocked = 0;
	g_players[playerIdx].viewState.transitionTimer = 0;
	g_players[playerIdx].viewState.cameraFocusObjIdx = (uint16_t)g_players[playerIdx].objectIndex;
	if (g_players[playerIdx].savedHudViewState == HUD_VIEW_HUD_ONLY)
		Hud_ForcePlayerViewState(HUD_VIEW_HUD_ONLY, playerIdx);
	else
		Hud_ForcePlayerViewState(HUD_VIEW_FORWARD, playerIdx);
	if (playerIdx == g_localPlayer) {
		Hud_DrawCraftNameFpsAndNetworkStatus();
		fsfx_PlaySound(FLIGHT_SOUND_BOMB_1, -1, playerIdx);
	}
	Flight_ComputeLiveWorldStateChecksum();
	return 0;
}

// FUNCTION: XVT 0x45A850
int Player_UnbindFromCurrentCraft(int playerIndex, int requireMultipleCraft, int assignAiPlan) {
	enum {
		MAX_OWNED_CRAFT_WITHOUT_REPLACEMENT = 1,
		WEAPON_BANK_COUNT = 2,
		ORDER_SLOT_COUNT = 3,
		WARHEAD_LINK_PRESERVE_MASK = 0x80,
		WARHEAD_LINK_DEFAULT = 1,
		ESCORT_THROTTLE_SPEED = 0x8000,
	};

	int objectIdx;
	int laserBank;
	CraftData* craft;
	int launcherIndex;

	if (requireMultipleCraft == 1) {
		uint16_t ownedCraftCount;
		int objectSlot;

		ownedCraftCount = 0;
		for (objectSlot = g_activeRegionObjectSlotStart; objectSlot < g_activeRegionCraftObjectSlotEnd;
			 ++objectSlot) {
			if (g_objectTable[objectSlot].objectType != 0 &&
				g_missionFlightGroups[g_objectTable[objectSlot].flightGroupIdx].playerOwnerIdx ==
					playerIndex &&
				g_objectTable[objectSlot].mobj->pCraft->objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
				g_objectTable[objectSlot].mobj->pCraft->objectKind != CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE &&
				g_objectTable[objectSlot].mobj->pCraft->objectKind != CRAFT_OBJECT_KIND_EXPLODING)
				++ownedCraftCount;
		}
		if (ownedCraftCount <= MAX_OWNED_CRAFT_WITHOUT_REPLACEMENT)
			return 0;
	}

	objectIdx = g_players[playerIndex].objectIndex;
	if (objectIdx == -1)
		return 0;

	g_objectTable[objectIdx].playerOwnerIdx = -1;
	g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
	collide_ResetNeighborProximityLists((uint16_t)objectIdx);
	collide_ResetObjectProximityForSlot((uint16_t)objectIdx);
	Player_SaveCraftSettings(playerIndex);

	craft = g_objectTable[objectIdx].mobj->pCraft;
	for (laserBank = 0; laserBank < WEAPON_BANK_COUNT; ++laserBank) {
		craft->laserState.linkMode[laserBank] = 0;
		craft->laserState.burstRemaining[laserBank] = 0;
		craft->laserState.nextSlot[laserBank] = 0;
		craft->laserState.fireCooldownTicks[laserBank] = 0;
		craft->laserState.lastFireTimestamp[laserBank] = 0;
	}
	for (launcherIndex = 0; launcherIndex < WEAPON_BANK_COUNT; ++launcherIndex) {
		craft->warheadLauncherFlags[launcherIndex] =
			(int8_t)((craft->warheadLauncherFlags[launcherIndex] & WARHEAD_LINK_PRESERVE_MASK) |
					 WARHEAD_LINK_DEFAULT);
		craft->warheadLauncherCooldownTicks[launcherIndex] = 0;
	}
	craft->warheadLockTicks = 0;
	craft->shieldEnergy[0] += craft->shieldEnergy[1];
	craft->shieldEnergy[1] = 0;
	craft->shieldDistribMode = SHIELD_DISTRIBUTION_FULLY_FORWARD;

	g_players[playerIndex].objectIndex = -1;
	g_players[playerIndex].regionSessionId = 0;
	g_players[playerIndex].hyperspacePhase = 0;
	g_players[playerIndex].missileLockState = 0;
	g_players[playerIndex].yawRollSwap = 0;
	g_players[playerIndex].smoothedInputYaw = 0;
	g_players[playerIndex].smoothedInputPitch = 0;
	g_players[playerIndex].savedKeyMods = 0;
	g_players[playerIndex].keyModsHoldTimer = 0;
	g_players[playerIndex].engineWashSourceObjIdx = -1;
	g_curCraft->aiController.currentOrderSlot = 0;

	if (assignAiPlan != 0) {
		uint16_t planId = g_builtinPlanIdByNameIndex
			[g_orderLeaderBuiltinPlanNameIndex
				 [g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.orders[0].order]];
		const char* planName;
		uint16_t throttleSpeed;

		g_curCraft->aiController.currentPlanId = planId;
		g_curCraft->aiController.pendingPlanId = planId;
		planName = g_planTable[planId].name;
		if (strcmp(planName, "nullpln") == 0 || strcmp(planName, "stationaryldrpln") == 0 ||
			strcmp(planName, "stationaryflwpln") == 0 || strcmp(planName, "disabledpln") == 0)
			throttleSpeed = 0;
		else if (strcmp(planName, "escortldr1pln") == 0)
			throttleSpeed = ESCORT_THROTTLE_SPEED;
		else
			throttleSpeed = g_orderThrottleToCraftThrottleSpeed
				[g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.orders[0].throttle];
		craft->throttleSpeed = throttleSpeed;
		g_objectTable[objectIdx].mobj->speed =
			(uint16_t)MATH2_fraction(g_modelDefs[craft->modelIndex].maxSpeed, throttleSpeed);
		g_objectTable[objectIdx].mobj->speedRemainder = 0;
		g_curCraft = craft;
		pai_setupcraftcontext((uint16_t)objectIdx);
		pai_ApplyPendingPlanTargetAndManeuver((unsigned int)objectIdx);

		if (g_players[playerIndex].currentTargetObjectIdx != -1) {
			int targetObjIdx = (uint16_t)g_players[playerIndex].currentTargetObjectIdx;
			if (g_activeRegionCraftObjectSlotEnd > targetObjIdx ||
				(g_regionMainObjectSlotEnd <= targetObjIdx &&
				 g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount > targetObjIdx)) {
				unsigned int orderSlot;
				int targetMatchesOrder;

				targetMatchesOrder = 0;
				for (orderSlot = 0; orderSlot < ORDER_SLOT_COUNT; ++orderSlot) {
					g_paiContext.orderSlot = (uint16_t)orderSlot;
					if (pai_CurrentOrderTargetsMatchObject(
							(uint16_t)g_players[playerIndex].currentTargetObjectIdx) != 0)
						targetMatchesOrder = 1;
				}
				if (targetMatchesOrder != 0)
					craft->aiController.candidateTargetIdx =
						(uint16_t)g_players[playerIndex].currentTargetObjectIdx;
			}
		}
	} else {
		g_curCraft->aiController.pendingPlanId = (uint8_t)pai_findplanbyname("nullpln");
		g_curCraft->aiController.currentPlanId = g_curCraft->aiController.pendingPlanId;
		g_curCraft = craft;
		pai_setupcraftcontext((uint16_t)objectIdx);
		pai_ApplyPendingPlanTargetAndManeuver((unsigned int)objectIdx);
		craft->aiFlight.enterFlag = 0;
		craft->aiFlight.headingState = 0;
		craft->aiFlight.turnState = 0;
		craft->aiFlight.climbState = 0;
		craft->aiFlight.diveState = 0;
	}
	return 1;
}

// FUNCTION: XVT 0x45ACD0
void Player_SaveCraftSettings(int playerIndex) {
	CraftData* craft;

	craft = g_objectTable[g_players[playerIndex].objectIndex].mobj->pCraft;
	g_players[playerIndex].savedCraftSettings.throttleSpeed = craft->throttleSpeed;
	g_players[playerIndex].savedCraftSettings.laserRedirect = craft->laserRedirect;
	g_players[playerIndex].savedCraftSettings.shieldRedirect = craft->shieldRedirect;
	g_players[playerIndex].savedCraftSettings.beamLevel = craft->beamLevel;
	g_players[playerIndex].savedCraftSettings.shieldDistribMode = craft->shieldDistribMode;
	g_players[playerIndex].savedCraftSettings.laserLinkMode[0] = craft->laserState.linkMode[0];
	g_players[playerIndex].savedCraftSettings.laserLinkMode[1] = craft->laserState.linkMode[1];
	g_players[playerIndex].savedCraftSettings.warheadLauncherFlags[0] =
		(uint8_t)(craft->warheadLauncherFlags[0] & 3);
	g_players[playerIndex].savedCraftSettings.warheadLauncherFlags[1] =
		(uint8_t)(craft->warheadLauncherFlags[1] & 3);
}

// FUNCTION: XVT 0x480570
void Player_UpdateFlightControlsAndCamera(int playerIdx) {
	enum {
		BASE_THROTTLE_SCALE = 0x5555,
		ROLL_RATE_SEGMENT = 0x3800,
		PITCH_RATE_SEGMENT = 0x1400,
		POWER_REDIRECT_NEUTRAL_TOTAL = 4,
		POWER_REDIRECT_SCALE = 0xC00,
		CONTROL_SMOOTHING_THRESHOLD = 8,
		CONTROL_SMOOTHING_BASE_STEP = 4,
		KEY_MODIFIER_MASK = 0xE,
		ROLL_CONTROL_MODIFIER = 2,
		MAP_CAMERA_DIRECTION_BIT = 0x80,
		MAP_CAMERA_STATE_MASK = 0x7F,
		MAP_CAMERA_MAX_TRANSITION = 0x7F,
		MAP_CAMERA_YAW_DEADZONE = 128,
		MAP_CAMERA_PITCH_DEADZONE = 48,
		CAMERA_DISTANCE_DEFAULT_STEP = 32,
		CAMERA_DISTANCE_DECAY_SHIFT = 3,
		CAMERA_DISTANCE_COORDINATE_SHIFT = 14,
		CAMERA_DISTANCE_LARGE_LIMIT = 28672,
		CAMERA_DISTANCE_LARGE_THRESHOLD = 57344,
		CAMERA_DISTANCE_MAP_MIN_STEP = 2048,
		CAMERA_DISTANCE_FOCUS_MIN_STEP = 256,
		CAMERA_DISTANCE_FREE_MIN_STEP = 1024,
		CAMERA_DISTANCE_FREE_MAX_STEP = 0x4000,
		CAMERA_DISTANCE_NORMAL_MAX = 5120,
		CAMERA_MINIMUM = 48,
		CAMERA_CLEARANCE = 512,
		CAMERA_WORLD_LIMIT = 0x1000000,
	};

	CraftData* craft;
	MobileObject* mobileObject;
	ObjectRecord* targetObject;
	int16_t desiredYaw;
#ifdef XVT_MODERN
	int16_t independentRollStep;
	int16_t modernDistanceStep;
	int16_t previousDistanceStep;
#endif
	int16_t desiredPitch;
	int16_t yawStep;
	int16_t pitchStep;
	int16_t smoothedInput;
	int16_t inputDifference;
	int16_t smoothingStep;
	int16_t absoluteYaw;
	int16_t absolutePitch;
	int16_t* cameraDistanceStep;
	int16_t powerBalance;
	int16_t redirectTotal;
	int16_t laserRedirect;
	uint16_t throttleScale;
	uint16_t rollRate;
	uint16_t pitchRate;
	uint16_t segmentCount;
	uint16_t segmentFraction;
	uint16_t inputMagnitude;
	uint16_t powerScale;
	int16_t transitionMagnitude;
	uint16_t keyMode;
	uint16_t cameraKeyMode;
	uint8_t cameraState;
	int targetClearance;
	int distance;
	int movement;
	int cameraScaleX, cameraScaleY, cameraScaleZ;
	int clearanceScaleX, clearanceScaleY, clearanceScaleZ;
	int reverseScaleX, reverseScaleY, reverseScaleZ;
	int cameraMovementX, cameraMovementY, cameraMovementZ;
	int clearanceMovementX, clearanceMovementY, clearanceMovementZ;
	int reverseMovementX, reverseMovementY, reverseMovementZ;

#ifdef XVT_MODERN
	XvtPlayerTiming_BeginControls(playerIdx);
#endif
	FlightInput_ApplyDeadzone();
	g_scaledInputPitch *= 2;
	if (g_players[playerIdx].viewState.playerInputBlocked == 0 && g_players[playerIdx].mapCameraState == 0) {
		if (g_players[playerIdx].hyperspacePhase == 0) {
			mobileObject = g_objectTable[g_players[playerIdx].objectIndex].mobj;
			craft = mobileObject->pCraft;
			throttleScale = craft->throttleSpeed;
			if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_ENGINES) == 0)
				throttleScale = 0;
			if (throttleScale < BASE_THROTTLE_SCALE)
				throttleScale = (uint16_t)(2 * throttleScale + BASE_THROTTLE_SCALE);
			else
				throttleScale = (uint16_t)((BASE_THROTTLE_SCALE - throttleScale) / 2 - 1);

			laserRedirect = (uint8_t)craft->laserRedirect;
			if ((craft->systemFlags & CRAFT_SUBSYSTEM_FLAG_SHIELDS) != 0)
				redirectTotal = (int16_t)((uint8_t)craft->shieldRedirect + laserRedirect);
			else
				redirectTotal = (int16_t)(2 * laserRedirect);
			powerBalance = (int16_t)(POWER_REDIRECT_NEUTRAL_TOTAL - redirectTotal);
			if (powerBalance < 0)
				powerScale = (uint16_t)(-POWER_REDIRECT_SCALE * powerBalance);
			else
				powerScale = (uint16_t)(POWER_REDIRECT_SCALE * powerBalance);
			rollRate = (uint16_t)MATH2_fraction(craft->aiFlight.rollRate, throttleScale);
			if (powerBalance > 0)
				rollRate = (uint16_t)(rollRate + MATH2_fraction(rollRate, powerScale));
			else
				rollRate = (uint16_t)(rollRate - MATH2_fraction(rollRate, powerScale));
			segmentCount = rollRate / ROLL_RATE_SEGMENT;
			segmentFraction = (uint16_t)MATH2_divide(rollRate % ROLL_RATE_SEGMENT, ROLL_RATE_SEGMENT);
			inputMagnitude = (uint16_t)g_scaledInputYaw;
			if (inputMagnitude >= 0x8000u)
				inputMagnitude = (uint16_t)-g_scaledInputYaw;
			desiredYaw =
				(int16_t)(MATH2_fraction(inputMagnitude, segmentFraction) + inputMagnitude * segmentCount);
			if ((uint16_t)g_scaledInputYaw >= 0x8000u)
				desiredYaw = (int16_t)-desiredYaw;
			g_absScaledInputYaw = g_scaledInputYaw;
			if ((uint16_t)g_scaledInputYaw >= 0x8000u)
				g_absScaledInputYaw = (int16_t)-g_scaledInputYaw;

			pitchRate = (uint16_t)MATH2_fraction(craft->aiFlight.pitchRate, throttleScale);
			if (powerBalance > 0)
				pitchRate = (uint16_t)(pitchRate + MATH2_fraction(pitchRate, powerScale));
			else
				pitchRate = (uint16_t)(pitchRate - MATH2_fraction(pitchRate, powerScale));
			segmentCount = pitchRate / PITCH_RATE_SEGMENT;
			segmentFraction = (uint16_t)MATH2_divide(pitchRate % PITCH_RATE_SEGMENT, PITCH_RATE_SEGMENT);
			inputMagnitude = (uint16_t)g_scaledInputPitch;
			if (inputMagnitude >= 0x8000u)
				inputMagnitude = (uint16_t)-g_scaledInputPitch;
			desiredPitch =
				(int16_t)(MATH2_fraction(inputMagnitude, segmentFraction) + inputMagnitude * segmentCount);
			if ((uint16_t)g_scaledInputPitch >= 0x8000u)
				desiredPitch = (int16_t)-desiredPitch;
			if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_FLIGHT_CONTROLS) == 0 ||
				(craft->beamEffectAccum[1] != 0 && craft->chaffActiveTimer == 0)) {
				desiredYaw = 0;
				desiredPitch = 0;
			}

			keyMode = 0;
			if ((g_flightKeyMods & KEY_MODIFIER_MASK) == ROLL_CONTROL_MODIFIER)
				keyMode = 1;
			if (g_players[playerIdx].yawRollSwap == keyMode) {
				smoothedInput = g_players[playerIdx].smoothedInputYaw;
				inputDifference = (int16_t)((uint16_t)desiredYaw - (uint16_t)smoothedInput);
#ifdef XVT_MODERN
				if (XvtFlightTiming_IsUnlocked()) {
					g_players[playerIdx].smoothedInputYaw =
						(int16_t)(smoothedInput +
								  XvtPlayerTiming_Slew(playerIdx, XVT_PLAYER_SLEW_YAW, inputDifference));
				} else
#endif
					if (inputDifference != 0) {
					smoothingStep = inputDifference;
					if (inputDifference < 0)
						smoothingStep = (int16_t)-inputDifference;
					if (smoothingStep < CONTROL_SMOOTHING_THRESHOLD) {
						g_players[playerIdx].smoothedInputYaw = (int16_t)(smoothedInput + inputDifference);
					} else {
						if (g_simStepScale > CONTROL_SMOOTHING_BASE_STEP) {
							smoothingStep = (int16_t)(smoothingStep / (int)g_simStepScale);
							if (smoothingStep == 0)
								smoothingStep = 1;
							smoothingStep *= CONTROL_SMOOTHING_BASE_STEP;
						}
						if (inputDifference < 0)
							g_players[playerIdx].smoothedInputYaw -= smoothingStep;
						else
							g_players[playerIdx].smoothedInputYaw += smoothingStep;
					}
				}
				smoothedInput = g_players[playerIdx].smoothedInputPitch;
				inputDifference = (int16_t)((uint16_t)desiredPitch - (uint16_t)smoothedInput);
#ifdef XVT_MODERN
				if (XvtFlightTiming_IsUnlocked()) {
					g_players[playerIdx].smoothedInputPitch =
						(int16_t)(smoothedInput +
								  XvtPlayerTiming_Slew(playerIdx, XVT_PLAYER_SLEW_PITCH, inputDifference));
				} else
#endif
					if (inputDifference != 0) {
					smoothingStep = inputDifference;
					if (inputDifference < 0)
						smoothingStep = (int16_t)-inputDifference;
					if (smoothingStep < CONTROL_SMOOTHING_THRESHOLD) {
						g_players[playerIdx].smoothedInputPitch = (int16_t)(smoothedInput + inputDifference);
					} else {
						if (g_simStepScale > CONTROL_SMOOTHING_BASE_STEP) {
							smoothingStep = (int16_t)(smoothingStep / (int)g_simStepScale);
							if (smoothingStep == 0)
								smoothingStep = 1;
							smoothingStep *= CONTROL_SMOOTHING_BASE_STEP;
						}
						if (inputDifference < 0)
							g_players[playerIdx].smoothedInputPitch -= smoothingStep;
						else
							g_players[playerIdx].smoothedInputPitch += smoothingStep;
					}
				}
			} else {
				g_players[playerIdx].smoothedInputYaw = 0;
				g_players[playerIdx].smoothedInputPitch = 0;
#ifdef XVT_MODERN
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_SLEW_YAW);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_SLEW_PITCH);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_YAW);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_PITCH);
#endif
			}
			g_players[playerIdx].yawRollSwap = (int16_t)keyMode;
			yawStep = (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked()
					 ? XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_YAW, g_players[playerIdx].smoothedInputYaw,
											 g_elapsedTicks, 236)
					 : Player_ScaleControlStepByElapsedTicks(g_players[playerIdx].smoothedInputYaw))
#else
				Player_ScaleControlStepByElapsedTicks(g_players[playerIdx].smoothedInputYaw)
#endif
				;
			pitchStep = (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked()
					 ? XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_PITCH,
											 g_players[playerIdx].smoothedInputPitch, g_elapsedTicks, 236)
					 : Player_ScaleControlStepByElapsedTicks(g_players[playerIdx].smoothedInputPitch))
#else
				Player_ScaleControlStepByElapsedTicks(g_players[playerIdx].smoothedInputPitch)
#endif
				;
#ifdef XVT_MODERN
			independentRollStep = XvtFlightControls_RollStep(playerIdx, rollRate, keyMode ? yawStep : 0);
#endif
			if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_FLIGHT_CONTROLS) == 0 ||
				(craft->beamEffectAccum[1] != 0 && craft->chaffActiveTimer == 0)) {
				yawStep = 0;
				pitchStep = 0;
#ifdef XVT_MODERN
				independentRollStep = 0;
#endif
			}
			if (keyMode != 0) {
#ifdef XVT_MODERN
				yawStep = independentRollStep;
#endif
				if (pitchStep != 0) {
					USER_calcdeltapitch(pitchStep, 0, (uint16_t)g_players[playerIdx].objectIndex, craft);
					g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty = 1;
					g_objectTable[g_players[playerIdx].objectIndex].mobj->moveVectorDirty =
						g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty;
				}
				if (yawStep != 0) {
					g_objectTable[g_players[playerIdx].objectIndex].roll -= 2 * yawStep;
					g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty = 1;
					g_objectTable[g_players[playerIdx].objectIndex].mobj->moveVectorDirty =
						g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty;
				}
			} else if (pitchStep != 0 || yawStep != 0) {
				USER_calcdeltapitch(pitchStep, (int16_t)-yawStep, (uint16_t)g_players[playerIdx].objectIndex,
									craft);
				g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty = 1;
				g_objectTable[g_players[playerIdx].objectIndex].mobj->moveVectorDirty =
					g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty;
				if (yawStep != 0)
					g_objectTable[g_players[playerIdx].objectIndex].roll -= yawStep;
			}
#ifdef XVT_MODERN
			if (!keyMode && independentRollStep) {
				g_objectTable[g_players[playerIdx].objectIndex].roll -= 2 * independentRollStep;
				mobileObject->orientMatrixDirty = 1;
				mobileObject->moveVectorDirty = 1;
			}
#endif
		}
		return;
	}

	cameraState = g_players[playerIdx].mapCameraState;
	if (cameraState != 0) {
		if ((cameraState & MAP_CAMERA_DIRECTION_BIT) != 0) {
			transitionMagnitude = (int16_t)(cameraState & MAP_CAMERA_STATE_MASK);
			if (transitionMagnitude < MAP_CAMERA_MAX_TRANSITION) {
				if (g_flightSimSideEffectsSuppressed == 0) {
					if (transitionMagnitude + (uint16_t)g_elapsedTicks > MAP_CAMERA_MAX_TRANSITION)
						transitionMagnitude = (int16_t)(MAP_CAMERA_MAX_TRANSITION - g_elapsedTicks);
					transitionMagnitude = (int16_t)(transitionMagnitude + g_elapsedTicks);
					transitionMagnitude |= MAP_CAMERA_DIRECTION_BIT;
					g_players[playerIdx].mapCameraState = (uint8_t)transitionMagnitude;
				}
			} else {
				if (g_players[playerIdx].viewState.cameraFocusObjIdx != UINT16_MAX) {
					g_players[playerIdx].viewState.savedTargetX =
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_x;
					g_players[playerIdx].viewState.savedTargetY =
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_y;
					g_players[playerIdx].viewState.savedTargetZ =
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_z;
					g_players[playerIdx].viewState.savedTargetZ +=
						g_players[playerIdx].viewState.cameraDistance;
				}
				g_players[playerIdx].viewState.cameraFocusObjIdx = UINT16_MAX;
			}
		} else {
			transitionMagnitude = (int16_t)(cameraState & MAP_CAMERA_STATE_MASK);
			if (transitionMagnitude > 1 && g_flightSimSideEffectsSuppressed == 0) {
				if (g_elapsedTicks >= transitionMagnitude)
					transitionMagnitude = (int16_t)(g_elapsedTicks + 1);
				transitionMagnitude = (int16_t)(transitionMagnitude - g_elapsedTicks);
				g_players[playerIdx].mapCameraState = (uint8_t)transitionMagnitude;
			}
		}
		if (g_players[playerIdx].mapCameraState != 0) {
			absoluteYaw = g_scaledInputYaw;
			absolutePitch = g_scaledInputPitch;
			if (absoluteYaw < 0)
				absoluteYaw = (int16_t)-absoluteYaw;
			if (absolutePitch < 0)
				absolutePitch = (int16_t)-absolutePitch;
			if (absoluteYaw < MAP_CAMERA_YAW_DEADZONE)
				g_scaledInputYaw = 0;
			if (absolutePitch < MAP_CAMERA_PITCH_DEADZONE)
				g_scaledInputPitch = 0;
		}
	}

	yawStep = (int16_t)
#ifdef XVT_MODERN
		(XvtFlightTiming_IsUnlocked()
			 ? XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_CAMERA_YAW, g_scaledInputYaw, g_elapsedTicks, 236)
			 : Player_ScaleControlStepByElapsedTicks(g_scaledInputYaw))
#else
		Player_ScaleControlStepByElapsedTicks(g_scaledInputYaw)
#endif
		;
	pitchStep = (int16_t)
#ifdef XVT_MODERN
		(XvtFlightTiming_IsUnlocked() ? XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_CAMERA_PITCH,
															  g_scaledInputPitch, g_elapsedTicks, 236)
									  : Player_ScaleControlStepByElapsedTicks(g_scaledInputPitch))
#else
		Player_ScaleControlStepByElapsedTicks(g_scaledInputPitch)
#endif
		;
	if ((g_players[playerIdx].mapCameraState & MAP_CAMERA_DIRECTION_BIT) != 0) {
		g_players[playerIdx].viewState.savedTargetX +=
			yawStep * ((g_players[playerIdx].viewState.savedTargetZ >> CAMERA_DISTANCE_COORDINATE_SHIFT) + 1);
		g_players[playerIdx].viewState.savedTargetY +=
			pitchStep *
			((g_players[playerIdx].viewState.savedTargetZ >> CAMERA_DISTANCE_COORDINATE_SHIFT) + 1);
	} else {
		if (g_players[playerIdx].viewState.cameraFocusObjIdx != UINT16_MAX) {
			g_players[playerIdx].viewState.hudAimY += yawStep;
			g_players[playerIdx].viewState.hudAimX += pitchStep;
		} else {
			if (pitchStep != 0 || yawStep != 0)
				FlightView_RotateViewByInput(pitchStep, -yawStep, playerIdx);
		}
	}

	cameraKeyMode = g_flightKeyMods & 0xF;
#ifdef XVT_MODERN
	previousDistanceStep = g_players[playerIdx].viewState.cameraDistanceStep;
#endif
	if (cameraKeyMode != 1 && cameraKeyMode != 2) {
		if (g_players[playerIdx].mapCameraState != 0) {
			uint16_t currentDistanceStep = g_players[playerIdx].viewState.cameraDistanceStep;
			if (currentDistanceStep > 0x100u) {
				uint16_t decayedDistanceStep = currentDistanceStep;
				currentDistanceStep >>= CAMERA_DISTANCE_DECAY_SHIFT;
				decayedDistanceStep -= currentDistanceStep;
				decayedDistanceStep -= CAMERA_DISTANCE_DEFAULT_STEP;

#ifdef XVT_MODERN
				g_players[playerIdx].viewState.cameraDistanceStep =
					XvtFlightTiming_IsUnlocked()
						? (int16_t)(previousDistanceStep +
									XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_ZOOM,
														  (int)decayedDistanceStep -
															  (uint16_t)previousDistanceStep,
														  g_elapsedTicks, 8))
						: (int16_t)decayedDistanceStep;
#else
				g_players[playerIdx].viewState.cameraDistanceStep = (int16_t)decayedDistanceStep;
#endif

			} else
				g_players[playerIdx].viewState.cameraDistanceStep = CAMERA_DISTANCE_DEFAULT_STEP;
		} else {
			g_players[playerIdx].viewState.cameraDistanceStep = CAMERA_DISTANCE_DEFAULT_STEP;
		}
		return;
	}

	if (g_players[playerIdx].mapCameraState > 1) {
		distance = g_players[playerIdx].viewState.savedTargetZ;
		cameraDistanceStep = &g_players[playerIdx].viewState.cameraDistanceStep;
		if ((distance & ~1) > CAMERA_DISTANCE_LARGE_THRESHOLD)
			*cameraDistanceStep = CAMERA_DISTANCE_LARGE_LIMIT;
		else
			*cameraDistanceStep = (int16_t)(distance >> 1);
		if ((uint16_t)*cameraDistanceStep < CAMERA_DISTANCE_MAP_MIN_STEP)
			*cameraDistanceStep = CAMERA_DISTANCE_MAP_MIN_STEP;
	} else if (g_players[playerIdx].mapCameraState == 1) {
		if (g_players[playerIdx].viewState.cameraFocusObjIdx != UINT16_MAX) {
			distance = g_players[playerIdx].viewState.cameraDistance;
			cameraDistanceStep = &g_players[playerIdx].viewState.cameraDistanceStep;
			if ((distance & ~1) > CAMERA_DISTANCE_LARGE_THRESHOLD)
				*cameraDistanceStep = CAMERA_DISTANCE_LARGE_LIMIT;
			else
				*cameraDistanceStep = (int16_t)(distance >> 1);
			if ((uint16_t)*cameraDistanceStep < CAMERA_DISTANCE_FOCUS_MIN_STEP)
				*cameraDistanceStep = CAMERA_DISTANCE_FOCUS_MIN_STEP;
		} else if (g_players[playerIdx].viewState.aimTargetIdx != UINT16_MAX) {
			targetObject = &g_objectTable[g_players[playerIdx].viewState.aimTargetIdx];
			cameraDistanceStep = &g_players[playerIdx].viewState.cameraDistanceStep;
			distance =
				collide_roughdistance3d(targetObject->world_x - g_players[playerIdx].viewState.savedTargetX,
										targetObject->world_y - g_players[playerIdx].viewState.savedTargetY,
										targetObject->world_z - g_players[playerIdx].viewState.savedTargetZ);
			if (distance <= CAMERA_DISTANCE_LARGE_LIMIT)
				*cameraDistanceStep = (int16_t)distance;
			else
				*cameraDistanceStep = CAMERA_DISTANCE_LARGE_LIMIT;
			if ((uint16_t)*cameraDistanceStep < CAMERA_DISTANCE_FREE_MIN_STEP)
#ifdef XVT_MODERN
			{
				*cameraDistanceStep = CAMERA_DISTANCE_FREE_MIN_STEP;
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_DISTANCE);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_ZOOM);
			}
#else
				*cameraDistanceStep = CAMERA_DISTANCE_FREE_MIN_STEP;
#endif
		} else {
			uint16_t currentDistanceStep;
			cameraDistanceStep = &g_players[playerIdx].viewState.cameraDistanceStep;
			currentDistanceStep =
				(uint16_t)(g_players[playerIdx].viewState.cameraDistanceStep + CAMERA_DISTANCE_DEFAULT_STEP);
			*cameraDistanceStep = (int16_t)currentDistanceStep;

#ifdef XVT_MODERN
			*cameraDistanceStep =
				XvtFlightTiming_IsUnlocked()
					? (int16_t)(previousDistanceStep +
								XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_ZOOM,
													  (currentDistanceStep + (currentDistanceStep >> 3)) -
														  previousDistanceStep,
													  g_elapsedTicks, 8))
					: (int16_t)(currentDistanceStep + (currentDistanceStep >> 3));
#else
			*cameraDistanceStep = (int16_t)(currentDistanceStep + (currentDistanceStep >> 3));
#endif

			if ((uint16_t)*cameraDistanceStep > CAMERA_DISTANCE_FREE_MAX_STEP)
#ifdef XVT_MODERN
			{
				*cameraDistanceStep = CAMERA_DISTANCE_FREE_MAX_STEP;
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_DISTANCE);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_ZOOM);
			}
#else
				*cameraDistanceStep = CAMERA_DISTANCE_FREE_MAX_STEP;
#endif
		}
	} else {
		cameraDistanceStep = &g_players[playerIdx].viewState.cameraDistanceStep;

#ifdef XVT_MODERN
		*cameraDistanceStep =
			XvtFlightTiming_IsUnlocked()
				? (int16_t)(previousDistanceStep +
							XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_ZOOM,
												  (previousDistanceStep + CAMERA_DISTANCE_DEFAULT_STEP) -
													  previousDistanceStep,
												  g_elapsedTicks, 8))
				: (int16_t)(previousDistanceStep + CAMERA_DISTANCE_DEFAULT_STEP);
#else
		*cameraDistanceStep =
			(int16_t)(g_players[playerIdx].viewState.cameraDistanceStep + CAMERA_DISTANCE_DEFAULT_STEP);
#endif

		if ((uint16_t)*cameraDistanceStep > 0x400u)
#ifdef XVT_MODERN
		{
			*cameraDistanceStep = CAMERA_DISTANCE_FREE_MIN_STEP;
			XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_DISTANCE);
			XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_ZOOM);
		}
#else
			*cameraDistanceStep = CAMERA_DISTANCE_FREE_MIN_STEP;
#endif
	}

#ifdef XVT_MODERN
	modernDistanceStep = XvtFlightTiming_IsUnlocked()
							 ? (int16_t)XvtPlayerTiming_Scale(playerIdx, XVT_PLAYER_DISTANCE,
															  *cameraDistanceStep, g_elapsedTicks, 236)
							 : 0;
#endif
	if (cameraKeyMode == 1) {
		if (g_players[playerIdx].mapCameraState == 0) {
			g_players[playerIdx].viewState.cameraDistance -= (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
											  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
				Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
				;
			if (g_players[playerIdx].viewState.cameraDistance < CAMERA_MINIMUM)
#ifdef XVT_MODERN
			{
				g_players[playerIdx].viewState.cameraDistance = CAMERA_MINIMUM;
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_DISTANCE);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_ZOOM);
			}
#else
				g_players[playerIdx].viewState.cameraDistance = CAMERA_MINIMUM;
#endif
			return;
		}
		if (g_players[playerIdx].viewState.cameraFocusObjIdx != UINT16_MAX) {
			g_players[playerIdx].viewState.cameraDistance -= (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
											  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
				Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
				;
			targetClearance =
				g_modelTypeTable[g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].objectType]
					.maxBoundsExtent +
				CAMERA_CLEARANCE;
			if (g_players[playerIdx].viewState.cameraDistance < targetClearance)
#ifdef XVT_MODERN
			{
				g_players[playerIdx].viewState.cameraDistance = targetClearance;
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_DISTANCE);
				XvtPlayerTiming_Clear(playerIdx, XVT_PLAYER_ZOOM);
			}
#else
				g_players[playerIdx].viewState.cameraDistance = targetClearance;
#endif
			return;
		}
		if (g_players[playerIdx].viewState.aimTargetIdx != UINT16_MAX)
			FVIEW_BuildCameraOrient(0, g_players[playerIdx].viewState.viewPitch,
									g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL);
		else
			FVIEW_BuildCameraOrient(
				0, g_players[playerIdx].viewState.viewPitch, g_players[playerIdx].viewState.viewYaw, 0,
				g_players[playerIdx].viewState.hudAimX, g_players[playerIdx].viewState.hudAimY, NULL);
		cameraScaleX = g_camMatR2_X;
		cameraMovementX = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		cameraMovementX = Math_MulQ15(cameraMovementX, cameraScaleX);
		g_players[playerIdx].viewState.savedTargetX += cameraMovementX;
		cameraScaleY = g_camMatR2_Y;
		cameraMovementY = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		cameraMovementY = Math_MulQ15(cameraMovementY, cameraScaleY);
		g_players[playerIdx].viewState.savedTargetY += cameraMovementY;
		cameraScaleZ = g_camMatR2_Z;
		cameraMovementZ = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		cameraMovementZ = Math_MulQ15(cameraMovementZ, cameraScaleZ);
		g_players[playerIdx].viewState.savedTargetZ += cameraMovementZ;
		if (g_players[playerIdx].viewState.aimTargetIdx != UINT16_MAX) {
			targetObject = &g_objectTable[g_players[playerIdx].viewState.aimTargetIdx];
			distance =
				collide_roughdistance3d(targetObject->world_x - g_players[playerIdx].viewState.savedTargetX,
										targetObject->world_y - g_players[playerIdx].viewState.savedTargetY,
										targetObject->world_z - g_players[playerIdx].viewState.savedTargetZ);
			targetClearance =
				g_modelTypeTable[g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].objectType]
					.maxBoundsExtent +
				CAMERA_CLEARANCE;
			if (targetClearance > distance) {
				targetClearance =
					g_modelTypeTable[g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]
										 .objectType]
						.maxBoundsExtent +
					CAMERA_CLEARANCE;
				movement = distance - targetClearance;
				clearanceScaleX = g_camMatR2_X;
				clearanceMovementX = Math_MulQ15(movement, clearanceScaleX);
				g_players[playerIdx].viewState.savedTargetX += clearanceMovementX;
				clearanceScaleY = g_camMatR2_Y;
				clearanceMovementY = Math_MulQ15(movement, clearanceScaleY);
				g_players[playerIdx].viewState.savedTargetY += clearanceMovementY;
				clearanceScaleZ = g_camMatR2_Z;
				clearanceMovementZ = Math_MulQ15(movement, clearanceScaleZ);
				g_players[playerIdx].viewState.savedTargetZ += clearanceMovementZ;
			}
		}
		if (g_players[playerIdx].mapCameraState > 1 &&
			g_players[playerIdx].viewState.savedTargetZ < CAMERA_MINIMUM)
			g_players[playerIdx].viewState.savedTargetZ = CAMERA_MINIMUM;
		if (g_players[playerIdx].viewState.savedTargetX < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetX = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetX > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetX = CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetY < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetY = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetY > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetY = CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetZ < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetZ = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetZ > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetZ = CAMERA_WORLD_LIMIT;
	} else {
		if (g_players[playerIdx].mapCameraState == 0) {
			g_players[playerIdx].viewState.cameraDistance += (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
											  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
				Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
				;
			if (g_players[playerIdx].viewState.cameraDistance > CAMERA_DISTANCE_NORMAL_MAX)
				g_players[playerIdx].viewState.cameraDistance = CAMERA_DISTANCE_NORMAL_MAX;
			return;
		}
		if (g_players[playerIdx].viewState.cameraFocusObjIdx != UINT16_MAX) {
			g_players[playerIdx].viewState.cameraDistance += (int16_t)
#ifdef XVT_MODERN
				(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
											  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
				Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
				;
			return;
		}
		if (g_players[playerIdx].viewState.aimTargetIdx != UINT16_MAX)
			FVIEW_BuildCameraOrient(0, g_players[playerIdx].viewState.viewPitch,
									g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL);
		else
			FVIEW_BuildCameraOrient(
				0, g_players[playerIdx].viewState.viewPitch, g_players[playerIdx].viewState.viewYaw, 0,
				g_players[playerIdx].viewState.hudAimX, g_players[playerIdx].viewState.hudAimY, NULL);
		reverseScaleX = g_camMatR2_X;
		reverseMovementX = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		reverseMovementX = Math_MulQ15(reverseMovementX, reverseScaleX);
		g_players[playerIdx].viewState.savedTargetX -= reverseMovementX;
		reverseScaleY = g_camMatR2_Y;
		reverseMovementY = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		reverseMovementY = Math_MulQ15(reverseMovementY, reverseScaleY);
		g_players[playerIdx].viewState.savedTargetY -= reverseMovementY;
		reverseScaleZ = g_camMatR2_Z;
		reverseMovementZ = (int16_t)
#ifdef XVT_MODERN
			(XvtFlightTiming_IsUnlocked() ? modernDistanceStep
										  : Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep))
#else
			Player_ScaleControlStepByElapsedTicks(*cameraDistanceStep)
#endif
			;
		reverseMovementZ = Math_MulQ15(reverseMovementZ, reverseScaleZ);
		g_players[playerIdx].viewState.savedTargetZ -= reverseMovementZ;
		if (g_players[playerIdx].viewState.savedTargetX < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetX = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetX > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetX = CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetY < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetY = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetY > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetY = CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetZ < -CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetZ = -CAMERA_WORLD_LIMIT;
		if (g_players[playerIdx].viewState.savedTargetZ > CAMERA_WORLD_LIMIT)
			g_players[playerIdx].viewState.savedTargetZ = CAMERA_WORLD_LIMIT;
	}
}

// FUNCTION: XVT 0x481420
void FlightChat_HandleInput(int playerIdx) {
	PlayerData* player;
	uint8_t messageLength;
	int recipientIndex;
	PlayerData* recipient;
	int shouldSend;
	FlightChatRecipientMode recipientMode;
	const char* tauntText;

	switch (g_currentActionKey) {
		case 8:
			player = &g_players[playerIdx];
			messageLength = player->msgLength;
			if (messageLength != 0) {
				--messageLength;
				player->msgLength = messageLength;
				player->msgText[messageLength] = '_';
				player->msgText[messageLength + 1] = '\0';
			}
			msg_addMessagePtr(0, player->msgText);
			msg_emitInFlightMessage((InFlightMessageId)((uint8_t)player->msgTypeId + IFMSG_374_FROM_ARG_ARG),
									playerIdx);
			return;

		case 9:
			player = &g_players[playerIdx];
			++player->msgTypeId;
			if ((uint8_t)player->msgTypeId > FLIGHT_CHAT_RECIPIENT_ALL) {
				player->msgTypeId = FLIGHT_CHAT_RECIPIENT_TEAM;
			}
			msg_addMessagePtr(0, player->msgText);
			msg_emitInFlightMessage((InFlightMessageId)((uint8_t)player->msgTypeId + IFMSG_374_FROM_ARG_ARG),
									playerIdx);
			return;

		case 13:
			player = &g_players[playerIdx];
			player->msgText[player->msgLength] = '\0';
			msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
			msg_addMessagePtr(1, player->msgText);
			for (recipientIndex = 0; recipientIndex < 8; ++recipientIndex) {
				recipient = &g_players[recipientIndex];
				if (recipient->connectedFlag != 0) {
					g_msgSenderIff = 3;
					recipientMode = player->msgTypeId;
					shouldSend = 0;
					if (recipientMode == FLIGHT_CHAT_RECIPIENT_TEAM) {
						if (player->playerIff == recipient->playerIff) {
							shouldSend = 1;
						}
					} else if (recipientMode == FLIGHT_CHAT_RECIPIENT_ENEMY) {
						if (recipient->playerIff != player->playerIff &&
							g_missionTeams[(uint16_t)recipient->playerIff]
									.allies[(uint16_t)player->playerIff] == 0) {
							shouldSend = 1;
						}
					} else {
						shouldSend = 1;
					}
					if (shouldSend != 0) {
						msg_emitInFlightMessage(IFMSG_374_FROM_ARG_ARG, recipientIndex);
					}
				}
			}
			player->msgTypeId = FLIGHT_CHAT_RECIPIENT_INACTIVE;
			msg_emitInFlightMessage(IFMSG_378_MESSAGE_SENT, playerIdx);
			return;

		case 27:
			player = &g_players[playerIdx];
			player->msgTypeId = FLIGHT_CHAT_RECIPIENT_INACTIVE;
			msg_emitInFlightMessage(IFMSG_379_MESSAGE_ABORTED, playerIdx);
			return;

		case 155:
		case 156:
		case 157:
		case 158:
			player = &g_players[playerIdx];
			player->msgText[player->msgLength] = '\0';
			msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
			tauntText = g_playerTauntText[playerIdx][g_currentActionKey - 155];
			msg_addMessagePtr(1, tauntText);
			for (recipientIndex = 0; recipientIndex < 8; ++recipientIndex) {
				recipient = &g_players[recipientIndex];
				if (recipient->connectedFlag != 0) {
					g_msgSenderIff = 3;
					recipientMode = player->msgTypeId;
					shouldSend = 0;
					if (recipientMode == FLIGHT_CHAT_RECIPIENT_TEAM) {
						if (player->playerIff == recipient->playerIff) {
							shouldSend = 1;
						}
					} else if (recipientMode == FLIGHT_CHAT_RECIPIENT_ENEMY) {
						if (recipient->playerIff != player->playerIff &&
							g_missionTeams[(uint16_t)recipient->playerIff]
									.allies[(uint16_t)player->playerIff] == 0) {
							shouldSend = 1;
						}
					} else {
						shouldSend = 1;
					}
					if (shouldSend != 0) {
						msg_emitInFlightMessage(IFMSG_374_FROM_ARG_ARG, recipientIndex);
					}
				}
			}
			player->msgTypeId = FLIGHT_CHAT_RECIPIENT_INACTIVE;
			msg_emitInFlightMessage(IFMSG_378_MESSAGE_SENT, playerIdx);
			return;

		default:
			player = &g_players[playerIdx];
			if (g_currentActionKey != 0) {
				messageLength = player->msgLength;
				if (messageLength < 48) {
					player->msgText[messageLength] = (char)g_currentActionKey;
					player->msgText[messageLength + 1] = '_';
					player->msgText[messageLength + 2] = '\0';
					++player->msgLength;
				}
				msg_addMessagePtr(0, player->msgText);
				msg_emitInFlightMessage(
					(InFlightMessageId)((uint8_t)player->msgTypeId + IFMSG_374_FROM_ARG_ARG), playerIdx);
			} else if (playerIdx == g_localPlayer &&
					   (int16_t)g_playerFlightTransientTimers[playerIdx].flightGroupMessagePaneTimer <
						   SIMULATION_TICKS_PER_SECOND) {
				msg_addMessagePtr(0, player->msgText);
				msg_emitInFlightMessage(
					(InFlightMessageId)((uint8_t)player->msgTypeId + IFMSG_374_FROM_ARG_ARG), playerIdx);
			}
			return;
	}
}

// FUNCTION: XVT 0x481940
int16_t Player_FindNearestObjective(int goalType, int playerIdx) {
	enum {
		FLIGHT_GROUP_GOAL_COUNT = 8,
		GOAL_STATE_COMPLETE = 4,
		TRIGGER_CONDITION_NONE = 0,
		TRIGGER_CONDITION_ALWAYS = 10,
	};

	unsigned int objectIdx;
	uint16_t bestActionableObject;
	uint16_t bestObjectiveObject;
	unsigned int bestActionableRange;
	unsigned int bestObjectiveRange;
	unsigned int playerIff;
	int16_t selectedObject;

	playerIff = (uint16_t)g_players[playerIdx].playerIff;
	bestActionableObject = UINT16_MAX;
	bestObjectiveObject = UINT16_MAX;
	bestActionableRange = UINT_MAX;
	bestObjectiveRange = UINT_MAX;

	for (objectIdx = g_activeRegionObjectSlotStart;
		 objectIdx < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
		int flightGroupIdx;
		int objective;
		int triggerCondition;
		unsigned int goalIndex;
		FlightGroupGoal* goals;
		uint8_t* enabledTeamGoals;

		if (g_objectTable[objectIdx].objectType == 0 ||
			objectIdx == (unsigned int)g_players[playerIdx].objectIndex ||
			g_objectTable[objectIdx].genusId == CRAFT_GENUS_EXPLOSION ||
			Object_HasActiveDecoyBeam(objectIdx) != 0)
			continue;
		flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
		if (g_missionFlightGroups[flightGroupIdx].fg.team == playerIff)
			continue;
		goalIndex = 0;
		goals = g_missionFlightGroups[flightGroupIdx].fg.goals;
		objective = 0;
		enabledTeamGoals = &goals[0].enabledTeams[playerIff];
		for (; goalIndex < FLIGHT_GROUP_GOAL_COUNT; ++goalIndex) {
			FlightGroupGoal* goal = &goals[goalIndex];
			if (enabledTeamGoals[goalIndex * sizeof(*goal)] != 0 && goal->type == (uint8_t)goalType &&
				goal->points >= 0 &&
				g_missionFgStats[g_objectTable[objectIdx].flightGroupIdx]
						.goalState[FLIGHT_GROUP_GOAL_COUNT * playerIff + goalIndex] == GOAL_STATE_COMPLETE) {
				objective = 1;
			}
		}
		triggerCondition = g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[0].condition;
		if (triggerCondition != TRIGGER_CONDITION_ALWAYS && triggerCondition != TRIGGER_CONDITION_NONE &&
			Mission_ObjectMatchesTriggerVariable(
				objectIdx, g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[0].variableType,
				g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[0].variable) != 0)
			objective = 1;
		triggerCondition = g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[1].condition;
		if (triggerCondition != TRIGGER_CONDITION_ALWAYS && triggerCondition != TRIGGER_CONDITION_NONE &&
			Mission_ObjectMatchesTriggerVariable(
				objectIdx, g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[1].variableType,
				g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[1].variable) != 0)
			objective = 1;
		/* The original reads the second pair's condition but the first pair's variable. */
		triggerCondition = g_missionGlobalGoals[playerIff][goalType].triggerPairs[1].triggers[0].condition;
		if (triggerCondition != TRIGGER_CONDITION_ALWAYS && triggerCondition != TRIGGER_CONDITION_NONE &&
			Mission_ObjectMatchesTriggerVariable(
				objectIdx, g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[0].variableType,
				g_missionGlobalGoals[playerIff][goalType].triggerPairs[0].triggers[0].variable) != 0)
			objective = 1;
		triggerCondition = g_missionGlobalGoals[playerIff][goalType].triggerPairs[1].triggers[1].condition;
		if (triggerCondition != TRIGGER_CONDITION_ALWAYS && triggerCondition != TRIGGER_CONDITION_NONE &&
			Mission_ObjectMatchesTriggerVariable(
				objectIdx, g_missionGlobalGoals[playerIff][goalType].triggerPairs[1].triggers[1].variableType,
				g_missionGlobalGoals[playerIff][goalType].triggerPairs[1].triggers[1].variable) != 0)
			objective = 1;
		if (objective != 0 &&
			(g_objectTable[objectIdx].mobj->pCraft->objectKind == CRAFT_OBJECT_KIND_ACTIVE ||
			 g_objectTable[objectIdx].mobj->pCraft->objectKind ==
				 CRAFT_OBJECT_KIND_ARRIVING_FROM_HYPERSPACE)) {
			int actionable;
			Player_ComputePolarToObjectRef(playerIdx, objectIdx);
			actionable = msg_BuildTargetDescription(objectIdx, playerIdx, 0, 1);
			if (actionable != 0) {
				if (bestActionableRange > (unsigned int)trig2_polardistance) {
					bestActionableRange = (unsigned int)trig2_polardistance;
					bestActionableObject = objectIdx;
				}
			} else if (bestObjectiveRange > (unsigned int)trig2_polardistance) {
				bestObjectiveRange = (unsigned int)trig2_polardistance;
				bestObjectiveObject = objectIdx;
			}
		}
	}

	for (objectIdx = g_regionMainObjectSlotEnd;
		 objectIdx < (unsigned int)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount); ++objectIdx) {
		int flightGroupIdx;
		unsigned int goalIndex;
		FlightGroupGoal* goals;
		uint8_t* enabledTeamGoals;

		if (g_objectTable[objectIdx].objectType == 0)
			continue;
		flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
		if (g_missionFlightGroups[flightGroupIdx].fg.team == g_players[playerIdx].playerIff)
			continue;
		goalIndex = 0;
		goals = g_missionFlightGroups[flightGroupIdx].fg.goals;
		enabledTeamGoals = &goals[0].enabledTeams[playerIff];
		for (; goalIndex < FLIGHT_GROUP_GOAL_COUNT; ++goalIndex) {
			FlightGroupGoal* goal = &goals[goalIndex];
			if (enabledTeamGoals[goalIndex * sizeof(*goal)] != 0 && goal->type == (uint8_t)goalType &&
				goal->points >= 0 &&
				g_missionFgStats[flightGroupIdx].goalState[FLIGHT_GROUP_GOAL_COUNT * playerIff + goalIndex] ==
					GOAL_STATE_COMPLETE) {
				Player_ComputePolarToObjectRef(playerIdx, objectIdx);
				if (bestActionableRange > (unsigned int)trig2_polardistance) {
					bestActionableRange = (unsigned int)trig2_polardistance;
					bestActionableObject = objectIdx;
				}
			}
		}
	}
	selectedObject = (int16_t)bestActionableObject;
	if (bestActionableObject == UINT16_MAX)
		selectedObject = (int16_t)bestObjectiveObject;
	return selectedObject;
}

// FUNCTION: XVT 0x481D70
int Player_ScaleControlStepByElapsedTicks(int16_t step) {
	return MATH2_ABoverC32(step, g_elapsedTicks, SIMULATION_TICKS_PER_SECOND);
}

// FUNCTION: XVT 0x481EA0
void Player_TransferShieldBankEnergy(uint16_t dstBank, uint16_t srcBank, int playerIdx) {
	PlayerData* player;
	CraftData* craft;
	int* dstShieldEnergy;
	int16_t objectMaxShield;
	int dstEnergy;
	int srcEnergy;
	int16_t transferCapacity;

	player = &g_players[playerIdx];
	if (g_objectTable[player->objectIndex].mobj->pCraft->shieldEnergy[srcBank] > 0) {
		objectMaxShield = (int16_t)Craft_GetObjectMaxShield(player->objectIndex);
		craft = g_objectTable[player->objectIndex].mobj->pCraft;
		dstEnergy = craft->shieldEnergy[dstBank];
		dstShieldEnergy = &craft->shieldEnergy[dstBank];
		transferCapacity = objectMaxShield - dstEnergy;
		if (transferCapacity > 0) {
			srcEnergy = craft->shieldEnergy[srcBank];
			if (srcEnergy > transferCapacity) {
				*dstShieldEnergy = dstEnergy + transferCapacity;
				g_objectTable[player->objectIndex].mobj->pCraft->shieldEnergy[srcBank] -= transferCapacity;
			} else {
				*dstShieldEnergy = dstEnergy + srcEnergy;
				g_objectTable[player->objectIndex].mobj->pCraft->shieldEnergy[srcBank] = 0;
			}
		}
	}
}

// FUNCTION: XVT 0x481FB0
void Player_UpdateHudViewForCameraFocus(int playerIdx) {
	enum {
		CAMERA_HISTORY_SAMPLE_COUNT = 60,
	};

	uint16_t sampleIndex;

	if (g_players[playerIdx].viewState.externalCameraActive != 0) {
		if (g_players[playerIdx].viewState.transitionTimer != 0) {
			Hud_SetHudViewState(HUD_VIEW_TARGET_CAMERA, playerIdx);
		} else {
			Hud_SetHudViewState(HUD_VIEW_FULL_SCREEN, playerIdx);
		}
		for (sampleIndex = 0; sampleIndex < CAMERA_HISTORY_SAMPLE_COUNT; ++sampleIndex) {
			g_players[playerIdx].viewState.cameraRollHistory[sampleIndex] =
				g_players[playerIdx].viewState.viewRoll;
			g_players[playerIdx].viewState.cameraPitchHistory[sampleIndex] =
				g_players[playerIdx].viewState.viewPitch;
			g_players[playerIdx].viewState.cameraYawHistory[sampleIndex] =
				g_players[playerIdx].viewState.viewYaw;
		}
	} else {
		g_players[playerIdx].viewState.playerInputBlocked = 0;
		if (g_players[playerIdx].viewState.cameraFocusObjIdx == g_players[playerIdx].objectIndex) {
			g_players[playerIdx].viewState.hudAimX = g_players[playerIdx].viewState.savedHudAimX;
			g_players[playerIdx].viewState.hudAimY = g_players[playerIdx].viewState.savedHudAimY;
			Hud_SetHudViewState(g_players[playerIdx].viewState.savedHudStateByte, playerIdx);
		} else {
			g_players[playerIdx].viewState.hudAimX = 0;
			g_players[playerIdx].viewState.hudAimY = 0;
			Hud_SetHudViewState(HUD_VIEW_FULL_SCREEN, playerIdx);
			g_players[playerIdx].viewState.externalCameraActive = 1;
		}
	}
}

// FUNCTION: XVT 0x4820B0
uint16_t Player_PickTargetInSight(int playerIdx) {
	uint16_t bestAngularTarget = UINT16_MAX;
	uint16_t bestTarget = UINT16_MAX;
	uint16_t bestAngle = UINT16_MAX;
	unsigned int bestRange = UINT_MAX;
	uint16_t objectIdx;

	for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart; objectIdx < g_regionMainObjectSlotEnd;
		 ++objectIdx) {
		if (g_objectTable[objectIdx].objectType != 0 && g_players[playerIdx].objectIndex != objectIdx &&
			(g_modelTypeTable[g_objectTable[objectIdx].objectType].flags & 1) != 0) {
			if (Targeting_ScoreCandidate(objectIdx, 1, playerIdx)) {
				if (bestRange > (unsigned int)g_targetRangeScore) {
					bestTarget = objectIdx;
					bestRange = g_targetRangeScore;
				}
			} else {
				if (bestAngle <= g_targetAngleScore)
					continue;
				bestAngle = g_targetAngleScore;
				bestAngularTarget = objectIdx;
			}
		}
	}
	for (objectIdx = (uint16_t)g_regionMainObjectSlotEnd;
		 objectIdx < g_regionStaticObjectSlotCount + g_regionMainObjectSlotEnd; ++objectIdx) {
		if (g_objectTable[objectIdx].objectType != 0 &&
			(g_modelTypeTable[g_objectTable[objectIdx].objectType].flags & 1) != 0) {
			if (Targeting_ScoreCandidate(objectIdx, 1, playerIdx)) {
				if (bestRange > (unsigned int)g_targetRangeScore) {
					bestTarget = objectIdx;
					bestRange = g_targetRangeScore;
				}
			} else {
				if (bestAngle <= g_targetAngleScore)
					continue;
				bestAngle = g_targetAngleScore;
				bestAngularTarget = objectIdx;
			}
		}
	}
	if (bestTarget == UINT16_MAX) {
		if (g_players[playerIdx].mapCameraState != 0) {
			if (bestAngle < 0xFA)
				bestTarget = bestAngularTarget;
		} else if (bestAngle < 0x32) {
			bestTarget = bestAngularTarget;
		}
	}
	if (g_players[playerIdx].mapCameraState == 0 && Object_HasActiveDecoyBeam(bestTarget) == 1) {
		bestTarget = UINT16_MAX;
		msg_emitInFlightMessage(IFMSG_257_TARGET_ACQUISITION_BLOCKED_BY_DECOY_BEAM, playerIdx);
	}
	return bestTarget;
}

// FUNCTION: XVT 0x4822C0
uint16_t Player_CycleTargetAnyIFF(uint16_t currentObjIdx, int16_t direction, int playerIdx) {
	int16_t remainingObjects;
	int objectCount;
	ObjectRecord* object;
	MobileObject* mobileObject;
	uint8_t objectKind;

	objectCount = g_regionStaticObjectSlotCount;
	remainingObjects = (int16_t)objectCount;
	remainingObjects += (int16_t)g_regionMainObjectSlotEnd;
	for (;;) {
		if (remainingObjects-- == 0) {
			return UINT16_MAX;
		}
		currentObjIdx += direction;
		objectCount = g_regionStaticObjectSlotCount;
		if (currentObjIdx >= 0x8000u) {
			currentObjIdx = (uint16_t)(objectCount + (int16_t)g_regionMainObjectSlotEnd - 1);
		} else if (objectCount + g_regionMainObjectSlotEnd == currentObjIdx) {
			currentObjIdx = 0;
		}

		if (g_players[playerIdx].objectIndex != currentObjIdx) {
			object = &g_objectTable[currentObjIdx];
			if (object->objectType != 0 && (g_modelTypeTable[object->objectType].flags & 1) != 0) {
				if (object->mobj == NULL) {
					break;
				}
				if (object->genusId != CRAFT_GENUS_EXPLOSION &&
					Object_HasActiveDecoyBeam(currentObjIdx) == 0) {
					mobileObject = g_objectTable[currentObjIdx].mobj;
					if (mobileObject->state != 0) {
						break;
					}
					g_curCraft = mobileObject->pCraft;
					objectKind = g_curCraft->objectKind;
					if (objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
						break;
					}
				}
			}
		}
	}
	return currentObjIdx;
}

// FUNCTION: XVT 0x4823E0
uint16_t Player_CycleTarget(uint16_t currentObjIdx, int16_t direction, int playerIdx, int iffFilter,
							int targetFlags) {
	ObjectRecord* object;
	MobileObject* mobileObject;
	uint16_t objectIndex;
	int16_t remainingObjects;
	int objectCount;
	uint8_t objectKind;
	ObjectTypeId objectType;
	int objectIff;
	uint16_t playerIff;

	objectIndex = currentObjIdx;
	objectCount = g_regionStaticObjectSlotCount;
	remainingObjects = (int16_t)objectCount;
	remainingObjects += (int16_t)g_regionMainObjectSlotEnd;
	for (;;) {
		if (remainingObjects-- == 0)
			return UINT16_MAX;

		objectIndex += direction;
		objectCount = g_regionStaticObjectSlotCount;
		if (objectIndex >= 0x8000u) {
			objectIndex = (uint16_t)(objectCount + (int16_t)g_regionMainObjectSlotEnd - 1);
		} else if (objectCount + g_regionMainObjectSlotEnd == objectIndex) {
			objectIndex = 0;
		}

		if (g_players[playerIdx].objectIndex != objectIndex &&
			((targetFlags & 4) == 0 || g_projectileObjectSlotStart > objectIndex ||
			 g_projectileObjectSlotEnd <= objectIndex) &&
			((targetFlags & 2) == 0 || g_projectileObjectSlotEnd > objectIndex)) {
			object = &g_objectTable[objectIndex];
			objectType = object->objectType;
			if (objectType != 0 && (g_modelTypeTable[objectType].flags & 1) != 0 &&
				((targetFlags & 1) == 0 || object->genusId != CRAFT_GENUS_MINE)) {
				objectIff = object->mobj == NULL ? g_missionFlightGroups[object->flightGroupIdx].fg.team
												 : object->mobj->team;

				switch (iffFilter) {
					case 1:
						if ((uint16_t)g_players[playerIdx].playerIff != objectIff)
							continue;
						break;

					case 2:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (playerIff == objectIff)
							objectIff = 0;
						else
							objectIff = g_missionTeams[playerIff].allies[objectIff] == 0;
						if (objectIff == 1)
							continue;
						break;

					case 3:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (playerIff == objectIff)
							objectIff = 0;
						else
							objectIff = g_missionTeams[playerIff].allies[objectIff] == 0;
						if (objectIff == 0)
							continue;
						break;

					case 4:
						if (object->playerOwnerIdx == -1)
							continue;
						break;

					default:
						break;
				}

				if (object->mobj == NULL)
					break;
				if (object->genusId != CRAFT_GENUS_EXPLOSION && Object_HasActiveDecoyBeam(objectIndex) == 0) {
					mobileObject = g_objectTable[objectIndex].mobj;
					if (mobileObject->state != 0)
						break;
					g_curCraft = mobileObject->pCraft;
					objectKind = g_curCraft->objectKind;
					if (objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						objectKind != CRAFT_OBJECT_KIND_EXPLODING)
						break;
				}
			}
		}
	}

	return objectIndex;
}

// FUNCTION: XVT 0x4833D0
void Player_SetTarget(int newTargetObjIdx, int playerIdx) {
	enum { FIRST_OPT_OBJECT_TYPE = 73 };

	int targetObjIdx;
	int playerObjectIdx;
	int canTarget;

	if ((uint16_t)newTargetObjIdx == UINT16_MAX)
		return;
	targetObjIdx = (uint16_t)newTargetObjIdx;
	if (g_objectTable[targetObjIdx].objectType == 0 || g_objectTable[targetObjIdx].genusId == 13)
		return;
	playerObjectIdx = g_players[playerIdx].objectIndex;
	if (playerObjectIdx == targetObjIdx)
		return;
	canTarget = 1;
	if (playerObjectIdx != -1) {
		if ((g_objectTable[playerObjectIdx].mobj->pCraft->workingSubsystems &
			 CRAFT_SUBSYSTEM_FLAG_TARGETING_COMPUTER) == 0)
			canTarget = 0;
	}
	if (canTarget != 0) {
		if ((uint16_t)newTargetObjIdx != UINT16_MAX &&
			(uint16_t)g_players[playerIdx].currentTargetObjectIdx != (uint16_t)newTargetObjIdx) {
			fsfx_PlaySound(FLIGHT_SOUND_TARGET_SELECTED, -1, playerIdx);
			g_players[playerIdx].currentTargetObjectIdx = (uint16_t)newTargetObjIdx;
			g_players[playerIdx].selectedTargetComponent = 0;
			if (g_activeRegionCraftObjectSlotEnd > targetObjIdx) {
				if (g_players[playerIdx].mapCameraState != 0) {
					int objectType;
					int meshCount;
					uint16_t meshIndex;

					objectType =
						g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].objectType;
					if (objectType < FIRST_OPT_OBJECT_TYPE)
						meshCount = g_objectTypeMeshCache[objectType].meshCount;
					else
						meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
					for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
						int cachedMeshIndex;
						int meshType;

						cachedMeshIndex = meshIndex;
						objectType =
							g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].objectType;
						if (objectType < FIRST_OPT_OBJECT_TYPE)
							meshType = ModelMesh_GetCachedObjectTypeMeshType(objectType, cachedMeshIndex);
						else
							meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIndex);
						if (meshType == MESH_COMPONENT_01_HULL || meshType == MESH_COMPONENT_03_FUSELAGE) {
							g_players[playerIdx].selectedTargetComponent = meshIndex;
							break;
						}
					}
				} else {
					g_players[playerIdx].selectedTargetComponent =
						Player_SelectTargetComponentMesh((uint16_t)newTargetObjIdx, playerIdx);
				}
			}
			if (g_players[playerIdx].viewState.transitionTimer != 0)
				g_players[playerIdx].viewState.cameraFocusObjIdx =
					g_players[playerIdx].currentTargetObjectIdx;
			g_players[playerIdx].missileLockState = 0;
			playerObjectIdx = g_players[playerIdx].objectIndex;
			if (playerObjectIdx != -1)
				g_objectTable[playerObjectIdx].mobj->pCraft->warheadLockTicks = 0;
			if (g_players[playerIdx].mapCameraState != 0) {
				msg_BuildTargetDescription(newTargetObjIdx, playerIdx, 1, 0);
				return;
			}
			playerObjectIdx = g_players[playerIdx].objectIndex;
			if ((g_objectTable[playerObjectIdx].mobj->pCraft->damageStats.activeHudFeatureMask & 1) == 0)
				return;
			if (g_players[playerIdx].viewState.hudStateLive == HUD_VIEW_HUD_ONLY ||
				g_players[playerIdx].viewState.hudStateLive == HUD_VIEW_FORWARD ||
				g_players[g_localPlayer].viewState.hudStateLive == HUD_VIEW_TARGET_CAMERA)
				msg_BuildTargetDescription(newTargetObjIdx, playerIdx, 1, 0);
		}
	} else {
		g_msgArgTable[0] = IFMSG_096_TARGETING_COMPUTER;
		g_msgArgTable[1] = IFMSG_087_DAMAGED_AND_INOPERATIVE;
		msg_emitInFlightMessage(IFMSG_086_ARG_SYSTEM_IS_ARG, playerIdx);
	}
}

// FUNCTION: XVT 0x483800
uint16_t Player_SelectTargetComponentMesh(uint16_t targetObjIdx, unsigned int playerIdx) {
	/* Prefer the nearest hull or fuselage component for capital ships. */
	uint16_t genusId;
	int objectType;
	int meshTypeIndex;
	uint16_t meshIndex;
	int meshCount;
	MeshComponentType meshType;
	uint16_t selectedMeshIdx;
	unsigned int nearestDistance;

	nearestDistance = 0x1000000;
	genusId = g_objectTable[targetObjIdx].genusId;

	if (genusId == CRAFT_GENUS_STARSHIP || genusId == CRAFT_GENUS_PLATFORM) {
		selectedMeshIdx = 0;
		objectType = g_objectTable[targetObjIdx].objectType;
		if (objectType < (int)(sizeof(g_objectTypeMeshCache) / sizeof(g_objectTypeMeshCache[0]))) {
			meshCount = g_objectTypeMeshCache[objectType].meshCount;
		} else {
			meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
		}

		for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
			meshTypeIndex = meshIndex;
			objectType = g_objectTable[targetObjIdx].objectType;
			if (objectType < (int)(sizeof(g_objectTypeMeshCache) / sizeof(g_objectTypeMeshCache[0]))) {
				meshType = ModelMesh_GetCachedObjectTypeMeshType(objectType, meshTypeIndex);
			} else {
				meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIndex);
			}
			if (meshType == MESH_COMPONENT_01_HULL || meshType == MESH_COMPONENT_03_FUSELAGE) {
				unsigned int distance = Object_DirectionAndDistanceToMeshCenter(
					g_players[playerIdx].objectIndex, targetObjIdx, meshIndex);
				if (distance < nearestDistance) {
					selectedMeshIdx = meshIndex;
					nearestDistance = distance;
				}
			}
		}
		return selectedMeshIdx;
	}

	objectType = g_objectTable[targetObjIdx].objectType;
	if (objectType < (int)(sizeof(g_objectTypeMeshCache) / sizeof(g_objectTypeMeshCache[0]))) {
		meshCount = g_objectTypeMeshCache[objectType].meshCount;
	} else {
		meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	}

	for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		meshTypeIndex = meshIndex;
		objectType = g_objectTable[targetObjIdx].objectType;
		if (objectType < (int)(sizeof(g_objectTypeMeshCache) / sizeof(g_objectTypeMeshCache[0]))) {
			meshType = ModelMesh_GetCachedObjectTypeMeshType(objectType, meshTypeIndex);
		} else {
			meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshTypeIndex);
		}
		if (meshType == MESH_COMPONENT_01_HULL || meshType == MESH_COMPONENT_03_FUSELAGE) {
			return meshIndex;
		}
	}
	return meshIndex;
}

// FUNCTION: XVT 0x483A00
int16_t USER_calcdeltapitch(int16_t angleQ16, int16_t yawAngleQ16, uint16_t objectIndex, CraftData* craft) {
#ifdef XVT_MODERN
	ObjectRecord* object = &g_objectTable[objectIndex];
	XvtOrientationAngles current = { object->yaw, object->pitch, object->roll };
	XvtOrientationAngles updated =
		XvtOrientation_ApplyPitchYaw(current, angleQ16, (g_flightKeyMods & 0xE) == 2 ? 0 : yawAngleQ16);
	/* BoP also retains the commanded pitch in CraftData. Publish a coherent
	 * orientation immediately, as XWA does, before the per-object step gate. */
	craft->pitch = object->pitch = updated.pitch;
	object->yaw = updated.yaw;
	object->roll = updated.roll;
	return (int16_t)updated.roll;
#else
	int16_t pitch;
	int16_t yaw;
	int16_t pitchCos;
	int16_t pitchSin;
	int16_t yawCos;
	int16_t yawSin;
	int16_t pitchCosYawCos;
	int16_t pitchCosYawSin;
	int16_t pitchSinYawCos;
	int16_t pitchSinYawSin;
	int negativeYawSin;
	int negativePitchSin;
	int16_t rotatedX;
	int16_t rotatedY;
	int16_t rotatedZ;
	int16_t result;

	if (g_objectTable[objectIndex].mobj->orientMatrixDirty != 0) {
		FVIEW_calcrotatemove(g_objectTable[objectIndex].pitch, g_objectTable[objectIndex].yaw,
							 &g_objectTable[objectIndex]);
		FVIEW_calcrotateorient(g_objectTable[objectIndex].roll, 0, &g_objectTable[objectIndex]);
	}
	g_curMatR2_X = -g_objectTable[objectIndex].mobj->cachedFwdX;
	g_curMatR2_Y = -g_objectTable[objectIndex].mobj->cachedFwdY;
	g_curMatR2_Z = -g_objectTable[objectIndex].mobj->cachedFwdZ;
	g_curMatR1_X = g_objectTable[objectIndex].mobj->cachedUpX;
	g_curMatR1_Y = g_objectTable[objectIndex].mobj->cachedUpY;
	g_curMatR1_Z = g_objectTable[objectIndex].mobj->cachedUpZ;
	g_curMatR0_X = g_objectTable[objectIndex].mobj->cachedSideX;
	g_curMatR0_Y = g_objectTable[objectIndex].mobj->cachedSideY;
	g_curMatR0_Z = g_objectTable[objectIndex].mobj->cachedSideZ;
	FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, angleQ16);
	if ((g_flightKeyMods & 0xE) != 2)
		FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, yawAngleQ16);

	pitch = trig2_w_arccos((int16_t)-g_curMatR2_Z);
	craft->pitch = pitch;
	yaw = (int16_t)-trig2_arctan(g_curMatR2_X, -g_curMatR2_Y);
	yawCos = trig2_getsignedcos(yaw);
	yawSin = trig2_getsignedsin(yaw);
	pitchCos = trig2_getsignedcos(pitch);
	pitchSin = trig2_getsignedsin(pitch);
	pitchCosYawSin = (int16_t)Math_MulQ15(yawSin, pitchCos);
	pitchSinYawSin = (int16_t)Math_MulQ15(yawSin, pitchSin);
	pitchCosYawCos = (int16_t)Math_MulQ15(yawCos, pitchCos);
	pitchSinYawCos = (int16_t)Math_MulQ15(yawCos, pitchSin);
	negativeYawSin = (int16_t)-yawSin;
	negativePitchSin = (int16_t)-pitchSin;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR0_X = rotatedX;
	g_curMatR0_Y = rotatedY;
	g_curMatR0_Z = rotatedZ;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR1_X = rotatedX;
	g_curMatR1_Y = rotatedY;
	g_curMatR1_Z = rotatedZ;

	rotatedX =
		(int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, yawCos, negativeYawSin, 0);
	rotatedY = (int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, pitchCosYawSin,
											pitchCosYawCos, negativePitchSin);
	rotatedZ = (int16_t)Math_Dot3Q15Wrapped(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, pitchSinYawSin,
											pitchSinYawCos, pitchCos);
	g_curMatR2_X = rotatedX;
	g_curMatR2_Y = rotatedY;
	g_curMatR2_Z = rotatedZ;

	result = (int16_t)-trig2_arctan(g_curMatR0_Y, g_curMatR0_X);
	g_objectTable[objectIndex].yaw = yaw;
	g_objectTable[objectIndex].roll = result;
	return result;
#endif
}

// FUNCTION: XVT 0x4841B0
int16_t Player_CanRadioCommandCraft(int objectIndex) {
	int currentTargetObjectIdx;
	ObjectRecord* targetObject;
	int flightGroupIdx;
	int boundFlightGroupIdx;
	uint8_t radio;
	int playerIff;
	uint8_t globalUnit;

	if (g_players[objectIndex].currentTargetObjectIdx == -1) {
		return 0;
	}
	currentTargetObjectIdx = (uint16_t)g_players[objectIndex].currentTargetObjectIdx;
	if (g_activeRegionCraftObjectSlotEnd <= currentTargetObjectIdx) {
		return 0;
	}
	targetObject = &g_objectTable[currentTargetObjectIdx];
	if (targetObject->playerOwnerIdx != -1) {
		return 0;
	}
	g_curCraft = targetObject->mobj->pCraft;
	if (g_curCraft->objectKind != CRAFT_OBJECT_KIND_ACTIVE) {
		return 0;
	}
	if (g_curCraft->workingSubsystems == 0) {
		return 0;
	}
	flightGroupIdx = targetObject->flightGroupIdx;
	boundFlightGroupIdx = (uint16_t)g_players[objectIndex].boundFlightGroupIdx;
	if (flightGroupIdx == boundFlightGroupIdx) {
		return 1;
	}
	radio = g_missionFlightGroups[flightGroupIdx].fg.radio;
	if (radio == 0) {
		return 0;
	}
	playerIff = (uint16_t)g_players[objectIndex].playerIff;
	if (g_missionFlightGroups[flightGroupIdx].fg.team == playerIff) {
		return 1;
	}
	globalUnit = g_missionFlightGroups[flightGroupIdx].fg.globalUnit;
	if (globalUnit != 0 && g_missionFlightGroups[boundFlightGroupIdx].fg.globalUnit == globalUnit) {
		return 1;
	}
	if (radio - g_missionFlightGroups[boundFlightGroupIdx].fg.playerNumber == 8) {
		return 1;
	}
	return radio - playerIff == 1;
}

// FUNCTION: XVT 0x484320
void Player_IssueAiWingmanTargetOrder(uint16_t targetObjIdx, uint16_t commandId, uint16_t responseIndex,
									  int playerIdx) {
	uint16_t matchingWingmen;
	uint16_t objectIndex;
	uint16_t lastWingman;

	if (targetObjIdx != UINT16_MAX) {
		int targetTeam = g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.team;
		int targetIsHostile;
		if (targetTeam == (uint16_t)g_players[playerIdx].playerIff) {
			targetIsHostile = 0;
		} else {
			targetIsHostile = !g_missionTeams[(uint16_t)g_players[playerIdx].playerIff].allies[targetTeam];
		}
		if (!targetIsHostile) {
			return;
		}
	}

	matchingWingmen = 0;
	lastWingman = UINT16_MAX;
	for (objectIndex = (uint16_t)g_activeRegionObjectSlotStart;
		 objectIndex < g_activeRegionCraftObjectSlotEnd; objectIndex++) {
		ObjectRecord* object;
		AiController* ai;
		CraftData* craft;
		int flightGroupIdx;
		int playerSlot;
		uint16_t boundFlightGroupIdx;

		if (g_players[playerIdx].objectIndex == objectIndex) {
			continue;
		}
		object = &g_objectTable[objectIndex];
		if (object->playerOwnerIdx != -1 || object->objectType == 0) {
			continue;
		}
		flightGroupIdx = object->flightGroupIdx;
		if (g_missionFlightGroups[flightGroupIdx].fg.team != (uint16_t)g_players[playerIdx].playerIff) {
			continue;
		}
		craft = object->mobj->pCraft;
		if (craft->objectKind != CRAFT_OBJECT_KIND_ACTIVE) {
			continue;
		}
		boundFlightGroupIdx = g_players[playerIdx].boundFlightGroupIdx;
		playerSlot = g_missionFlightGroups[boundFlightGroupIdx].fg.playerNumber - 1;
		if (flightGroupIdx != boundFlightGroupIdx) {
			uint8_t globalUnit = g_missionFlightGroups[flightGroupIdx].fg.globalUnit;
			if ((globalUnit == 0 || g_missionFlightGroups[boundFlightGroupIdx].fg.globalUnit != globalUnit) &&
				g_missionFlightGroups[flightGroupIdx].fg.radio - playerSlot != 9) {
				continue;
			}
		}
		ai = &craft->aiController;
		if (commandId != 155) {
			const char* planName = g_planTable[ai->pendingPlanId].name;
			if (strcmp(planName, "nullpln") == 0 || strcmp(planName, "stationaryldrpln") == 0 ||
				strcmp(planName, "stationaryflwpln") == 0 || strcmp(planName, "formldr1pln") == 0 ||
				strcmp(planName, "formflw1pln") == 0 || strcmp(planName, "formevadeldr1pln") == 0 ||
				strcmp(planName, "formevadeflw1pln") == 0 || strcmp(planName, "flyhomeevadepln") == 0 ||
				strcmp(planName, "intohyperspacepln") == 0 || strcmp(planName, "enterhangarpln") == 0) {
				continue;
			}
			if (strcmp(planName, "craftwaitforgopln") == 0) {
				ai->pendingPlanId = ai->savedPlanId;
				g_curCraft = craft;
				pai_setupcraftcontext(objectIndex);
				pai_ApplyPendingPlanTargetAndManeuver(objectIndex);
			}
			ai->candidateTargetIdx = targetObjIdx;
			if (craft->playerCommandAvoidTargetObjIdx == targetObjIdx) {
				craft->playerCommandAvoidTargetObjIdx = UINT16_MAX;
			}
		} else {
			craft->playerCommandAvoidTargetObjIdx = targetObjIdx;
			if (ai->candidateTargetIdx == targetObjIdx) {
				ai->candidateTargetIdx = UINT16_MAX;
			}
		}
		matchingWingmen++;
		lastWingman = objectIndex;
	}
	if (playerIdx == g_localPlayer && lastWingman != UINT16_MAX) {
		uint8_t* wingmanCraft = (uint8_t*)g_objectTable[lastWingman].mobj->pCraft;
		if (matchingWingmen == 1) {
			msg_radioMessage(lastWingman, wingmanCraft, commandId, responseIndex, 0);
		} else {
			msg_radioMessage(lastWingman, wingmanCraft, commandId, responseIndex, 1);
		}
	}
}

// FUNCTION: XVT 0x4846F0
int16_t Player_FindAttackerOfTarget(uint16_t targetObjIdx, int16_t excludedObjIdx) {
	uint16_t nearest;
	unsigned int nearestDistance;
	uint16_t objectIdx;

	if (targetObjIdx == UINT16_MAX)
		return -1;
	nearestDistance = UINT_MAX;
	nearest = UINT16_MAX;
	for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		CraftData* craft;
		int qualifies;
		if (g_objectTable[objectIdx].objectType == 0 || targetObjIdx == objectIdx ||
			objectIdx == (uint16_t)excludedObjIdx ||
			g_objectTable[objectIdx].genusId == CRAFT_GENUS_EXPLOSION)
			continue;
		craft = g_objectTable[objectIdx].mobj->pCraft;
		qualifies = 0;
		if (craft->workingSubsystems == 0 || craft->objectKind != CRAFT_OBJECT_KIND_ACTIVE ||
			Object_HasActiveDecoyBeam(objectIdx))
			continue;
		if (g_objectTable[objectIdx].playerOwnerIdx == -1) {
			AiController* ai = &craft->aiController;
			if (ai->targetObjIdx != targetObjIdx || (ai->maneuverMode != AI_MANEUVER_MODE_ATTACK &&
													 ai->maneuverMode != AI_MANEUVER_MODE_ROCKET_ATTACK))
				continue;
			qualifies = 1;
		} else {
			int playerOwnerIdx;
			if (g_activeRegionCraftObjectSlotEnd > targetObjIdx) {
				CraftData* targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
				if (targetCraft->lastAttackerObjIdx == objectIdx &&
					(g_objectTable[targetObjIdx].playerOwnerIdx == -1 ||
					 (uint16_t)Mission_GameTimeToSeconds(g_missionElapsedClock.hours,
														 g_missionElapsedClock.minutes,
														 g_missionElapsedClock.seconds) -
							 targetCraft->lastHitTimestamp <
						 5))
					qualifies = 1;
			}
			playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;
			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == targetObjIdx) {
				pai_ObjectRefUpdateApproxRangeScore(objectIdx, targetObjIdx);
				if (g_targetRangeScore < 0x10000 && Targeting_ScoreCandidate(targetObjIdx, 0, playerOwnerIdx))
					qualifies = 1;
				if (craft->warheadLockTicks != 0)
					qualifies = 1;
			}
		}
		if (qualifies != 0) {
			pai_ObjectRefDirectionToObjectRef(targetObjIdx, objectIdx);
			if (nearestDistance > (unsigned int)trig2_polardistance) {
				nearestDistance = trig2_polardistance;
				nearest = objectIdx;
			}
		}
	}
	return (int16_t)nearest;
}

// FUNCTION: XVT 0x484A30
void Player_StartPostDestructionState(int playerIdx, unsigned int sourceObjectIndex, int sourcePlayerIdx) {
	enum {
		KILL_MESSAGE_NAME_SIZE = 64,
		OBJECT_DISPLAY_NAME_AND_TYPE = 3,
	};

	char sourceName[KILL_MESSAGE_NAME_SIZE];
	char assistingPlayerName[KILL_MESSAGE_NAME_SIZE];
	int localPlayer;

	g_players[playerIdx].hyperspacePhase = 0;
	if (g_players[playerIdx].mapCameraState != 0) {
		if (playerIdx == g_localPlayer)
			Hud_ClearReadyMessageQueue();
	} else {
		g_players[playerIdx].viewState.cameraFocusObjIdx = UINT16_MAX;
		g_players[playerIdx].viewState.savedTargetX =
			g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldX;
		g_players[playerIdx].viewState.savedTargetY =
			g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldY;
		localPlayer = g_localPlayer;
		g_players[playerIdx].viewState.savedTargetZ =
			g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldZ;
		g_players[playerIdx].viewState.externalCameraActive = 1;
		g_players[playerIdx].viewState.playerInputBlocked = 1;
		g_players[playerIdx].viewState.hudAimY = 0;
		g_players[playerIdx].viewState.hudAimX = 0;
		if (playerIdx == localPlayer)
			Hud_ClearReadyMessageQueue();

		fsfx_UpdateBeamSystemLoop(0, playerIdx);
		fsfx_UpdateIncomingMissileWarning(0);
		fsfx_PlaySound(FLIGHT_SOUND_MISSILE_LOCK_3, -1, playerIdx);
		Hud_SetHudViewState(HUD_VIEW_FULL_SCREEN, playerIdx);
		if (playerIdx == g_localPlayer && sourceObjectIndex != UINT_MAX &&
			sourceObjectIndex < (unsigned int)g_activeRegionCraftObjectSlotEnd) {
			if (g_activeFlightPlayerCount > 1) {
				if (sourcePlayerIdx != -1 &&
					g_objectTable[sourceObjectIndex].playerOwnerIdx != sourcePlayerIdx &&
					g_players[sourcePlayerIdx].objectIndex != -1) {
					Player_AppendKillMessageActorName(0, sourceName, (int)sourceObjectIndex);
					Player_AppendKillMessageActorName(1, assistingPlayerName,
													  g_players[sourcePlayerIdx].objectIndex);
					msg_emitInFlightMessage(IFMSG_399_YOU_WERE_KILLED_BY_ARG_MOST_DAMAGE_WAS_DONE_BY_ARG,
											g_localPlayer);
				} else {
					Player_AppendKillMessageActorName(0, sourceName, (int)sourceObjectIndex);
					msg_emitInFlightMessage(IFMSG_398_YOU_WERE_KILLED_BY_ARG, g_localPlayer);
				}
			} else {
				Hud_AppendObjectDisplayName((uint16_t)sourceObjectIndex, OBJECT_DISPLAY_NAME_AND_TYPE);
				msg_addMessagePtr(0, g_flightTextScratchBuffer);
				msg_emitInFlightMessage(IFMSG_398_YOU_WERE_KILLED_BY_ARG, g_localPlayer);
			}
		}
	}
	g_players[playerIdx].regionSessionId = 1;
}

// FUNCTION: XVT 0x484C50
void Player_AppendKillMessageActorName(int slot, char* text, int objectIndex) {
	ObjectRecord* object;
	int playerOwnerIdx;
	CraftData* craft;
	int playerIff;
	int team;
	int isEnemy;
	char* playerName;

	object = &g_objectTable[objectIndex];
	playerOwnerIdx = object->playerOwnerIdx;
	craft = object->mobj->pCraft;
	if (g_flightMissionState.locatePlayersEnabled != 0 ||
		craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] != 0) {
		isEnemy = 0;
	} else {
		playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
		team = g_missionFlightGroups[g_objectTable[(uint16_t)objectIndex].flightGroupIdx].fg.team;
		if (team == playerIff)
			isEnemy = 0;
		else
			isEnemy = g_missionTeams[playerIff].allies[team] == 0;
	}
	if (isEnemy == 1) {
		if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
			Hud_AppendObjectDisplayName((uint16_t)objectIndex, 1);
			playerName = g_flightTextScratchBuffer;
		} else {
			Hud_AppendObjectDisplayName((uint16_t)objectIndex, 3);
			playerName = g_flightTextScratchBuffer;
		}
	} else if (playerOwnerIdx != -1) {
		playerName = NetSession_GetPlayerName(playerOwnerIdx);
	} else {
		Hud_AppendObjectDisplayName((uint16_t)objectIndex, 3);
		playerName = g_flightTextScratchBuffer;
	}
	strcpy(text, playerName);
	msg_addMessagePtr((uint16_t)slot, text);
}

// FUNCTION: XVT 0x485000
void Player_ComputePolarToObjectRef(int playerIdx, unsigned int objectRef) {
	unsigned int objectIndex;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;

	objectIndex = g_players[playerIdx].objectIndex;
	if (objectIndex == UINT32_MAX) {
		Mission_ResolveObjectOrMissionPointWorldLoc(objectRef, 0);
		savedTargetX = g_players[playerIdx].viewState.savedTargetX;
		savedTargetY = g_players[playerIdx].viewState.savedTargetY;
		savedTargetZ = g_players[playerIdx].viewState.savedTargetZ;
		savedTargetX = worldlocx - savedTargetX;
		savedTargetY = worldlocy - savedTargetY;
		savedTargetZ = worldlocz - savedTargetZ;
		trig2_ctop(savedTargetX, savedTargetY, savedTargetZ);
	} else {
		pai_ObjectRefDirectionToObjectRef(objectIndex, objectRef);
	}
}

// FUNCTION: XVT 0x485080
void Player_EndFlightParticipation(int playerIdx) {
	enum {
		PLAYER_CONNECTED = 1,
		PLAYER_DISCONNECTED = 2,
		EXTERNAL_CAMERA_INITIAL_DISTANCE = 0x40000,
		CAMERA_TARGET_EXTENT_SCALE = 16,
	};

	unsigned int activePlayerCount;
	unsigned int playerIndex;

	g_players[playerIdx].connectedFlag = PLAYER_DISCONNECTED;
	activePlayerCount = 0;
	for (playerIndex = 0; playerIndex < sizeof(g_players) / sizeof(g_players[0]); ++playerIndex) {
		if (g_players[playerIndex].connectedFlag == PLAYER_CONNECTED)
			++activePlayerCount;
	}
#ifdef XVT_MODERN
	XvtFlightDiag_PlayerFinished(playerIdx, activePlayerCount);
#endif
	if (activePlayerCount != 0) {
		g_players[playerIdx].mapCameraState = UINT8_MAX;
		Hud_SetHudViewState(HUD_VIEW_CRAFT_LIST, playerIdx);
		g_players[playerIdx].viewState.playerInputBlocked = 1;
		g_players[playerIdx].viewState.externalCameraActive = 1;
		g_players[playerIdx].viewState.cameraDistance = EXTERNAL_CAMERA_INITIAL_DISTANCE;
		g_players[playerIdx].viewState.cameraFocusObjIdx = UINT16_MAX;
		g_players[playerIdx].viewState.aimTargetIdx = UINT16_MAX;
		g_players[playerIdx].viewState.savedTargetZ = EXTERNAL_CAMERA_INITIAL_DISTANCE;
		if ((uint16_t)g_players[playerIdx].currentTargetObjectIdx != UINT16_MAX) {
			g_players[playerIdx].viewState.savedTargetX =
				g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].world_x;
			g_players[playerIdx].viewState.savedTargetY =
				g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].world_y;
			g_players[playerIdx].viewState.cameraDistance =
				CAMERA_TARGET_EXTENT_SCALE *
				g_modelTypeTable[g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx]
									 .objectType]
					.maxBoundsExtent;
		}
		g_players[playerIdx].regionSessionId = 0;
		g_players[playerIdx].objectIndex = -1;
		g_players[playerIdx].pendingActionTimer = 0;
		g_players[playerIdx].pendingActionId = 0;
		fsfx_UpdatePlayerEngineLoop();
		fsfx_UpdateChaffLoop();
		fsfx_UpdateBeamEffectLoops();
	} else {
#ifdef XVT_MODERN
		XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_LAST_PLAYER_FINISHED, playerIdx);
#endif
		g_flightMissionState.missionEndPending = 1;
	}
}

// FUNCTION: XVT 0x4851D0
void Player_EmitRemotePlayerDepartedMessages(int playerIdx) {
	int connectedCount;
	int playerIndex;

	if (g_localPlayer != playerIdx) {
		msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
		msg_emitInFlightMessage(IFMSG_381_ARG_HAS_NO_MORE_CRAFT_AND_IS_OUT_OF_THE_MISSION, g_localPlayer);
		connectedCount = 0;
		for (playerIndex = 0; playerIndex < 8; ++playerIndex) {
			if (g_players[playerIndex].connectedFlag == 1) {
				++connectedCount;
			}
		}
		if (g_players[g_localPlayer].connectedFlag == 1) {
			if (connectedCount == 1) {
				msg_emitInFlightMessage(IFMSG_383_YOU_ARE_THE_ONLY_PLAYER_LEFT, g_localPlayer);
			} else {
				g_msgArgTable[0] = (uint16_t)connectedCount;
				msg_emitInFlightMessage(IFMSG_382_THERE_ARE_ARG_PLAYERS_LEFT_INCLUDING_YOURSELF,
										g_localPlayer);
			}
		}
	}
}

// FUNCTION: XVT 0x485270
void Player_ValidateCurrentTargets(int playerIdx) {
	enum {
		OBJECT_GENUS_EXPLOSION = 13,
	};

	uint16_t currentTargetObjectIdx;
	ObjectRecord* targetObject;
	int localPlayer;

	currentTargetObjectIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
	if (currentTargetObjectIdx == UINT16_MAX)
		return;

	targetObject = &g_objectTable[currentTargetObjectIdx];
	if (targetObject->mobj != NULL) {
		if (targetObject->objectType == 0 || targetObject->genusId == OBJECT_GENUS_EXPLOSION ||
			Object_HasActiveDecoyBeam(currentTargetObjectIdx) != 0) {
			g_players[playerIdx].currentTargetObjectIdx = -1;
		}
	} else if (targetObject->objectType == 0 || targetObject->genusId == OBJECT_GENUS_EXPLOSION) {
		g_players[playerIdx].currentTargetObjectIdx = -1;
	}

	if (g_players[playerIdx].mapCameraState == 0 &&
		(g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft->workingSubsystems &
		 CRAFT_SUBSYSTEM_FLAG_TARGETING_COMPUTER) == 0) {
		g_players[playerIdx].currentTargetObjectIdx = -1;
	}
	if ((uint16_t)g_players[playerIdx].currentTargetObjectIdx == UINT16_MAX) {
		g_players[playerIdx].targetCycleStart = (int16_t)currentTargetObjectIdx;
		g_players[playerIdx].targetingState = 0;
		if (g_players[playerIdx].viewState.hudStateLive == HUD_VIEW_TARGET_CAMERA) {
			g_players[playerIdx].viewState.externalCameraActive = 0;
			localPlayer = g_localPlayer;
			g_players[playerIdx].viewState.transitionTimer = 0;
			g_players[playerIdx].viewState.cameraFocusObjIdx = (uint16_t)g_players[playerIdx].objectIndex;
			g_players[playerIdx].viewState.playerInputBlocked = 0;
			if (playerIdx == localPlayer) {
				g_hudCachedTargetObjectIdx = -2;
				g_renderObjectRefFlags = 0;
			}
			Hud_SetHudViewState(HUD_VIEW_FORWARD, playerIdx);
			g_players[playerIdx].viewState.hudAimX = 0;
			g_players[playerIdx].viewState.hudAimY = 0;
			msg_emitInFlightMessage(IFMSG_224_THREAT_DISPLAY_TARGET_NO_LONGER_AVAILABLE, playerIdx);
		}
	}
}

// FUNCTION: XVT 0x4853C0
void Player_ValidateAllCurrentTargets(void) {
	unsigned int playerIdx;

	for (playerIdx = 0; playerIdx < sizeof(g_players) / sizeof(g_players[0]); ++playerIdx) {
		if (g_players[playerIdx].connectedFlag != 0)
			Player_ValidateCurrentTargets((int)playerIdx);
	}
}

// FUNCTION: XVT 0x4853F0
int Player_HasAvailableOwnedCraft(int playerIdx) {
	int objectIndex;
	unsigned int remainingObjects;
	ObjectRecord* object;
	CraftData* craft;
	uint8_t objectKind;

	objectIndex = -1;
	remainingObjects = g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart;
	if (remainingObjects != 0) {
		do {
			++objectIndex;
			if (objectIndex >= g_activeRegionCraftObjectSlotEnd) {
				objectIndex = g_activeRegionObjectSlotStart;
			}
			object = &g_objectTable[objectIndex];
			if (object->objectType != 0) {
				craft = object->mobj->pCraft;
				if (g_missionFlightGroups[object->flightGroupIdx].playerOwnerIdx == playerIdx) {
					objectKind = craft->objectKind;
					if (objectKind != CRAFT_OBJECT_KIND_BREAKING_UP &&
						objectKind != CRAFT_OBJECT_KIND_ENTERING_HYPERSPACE &&
						objectKind != CRAFT_OBJECT_KIND_EXPLODING) {
						break;
					}
				}
			}
			--remainingObjects;
		} while (remainingObjects != 0);
	}
	return remainingObjects != 0;
}

// FUNCTION: XVT 0x485490
void Player_UpdateParticipationState(void) {
	enum {
		PLAYER_DISCONNECTED_PENDING_DEPARTURE = 2,
	};

	unsigned int playerIdx;

	for (playerIdx = 0; playerIdx < sizeof(g_players) / sizeof(g_players[0]); ++playerIdx) {
		PlayerData* player;

		player = &g_players[playerIdx];
		if (player->connectedFlag != 0) {
			if (player->regionSessionId != 0) {
				if (g_flightSimSideEffectsSuppressed == 0 && player->objectIndex != -1) {
					ObjectRecord* object;

					object = &g_objectTable[player->objectIndex];
					if (object->objectType == 0 || player->boundObjectSignature != object->objectSignature) {
						Mission_ProcessFlightGroupWaveCompletion(player->boundFlightGroupIdx);
						if (Player_BindToAvailableCraft((int)playerIdx, UINT32_MAX, 0, 0) != 0) {
							Player_EndFlightParticipation((int)playerIdx);
							Player_EmitRemotePlayerDepartedMessages((int)playerIdx);
						} else if (g_localPlayer == (int)playerIdx) {
							msg_emitLocalPlayerCraftMessage(
								IFMSG_290_PREVIOUS_CRAFT_DESTROYED_NOW_PILOTING_ARG_ARG_ARG);
						}
					}
				}
			} else if (player->mapCameraState != 0 &&
					   player->connectedFlag != PLAYER_DISCONNECTED_PENDING_DEPARTURE &&
					   g_flightSimSideEffectsSuppressed == 0 &&
					   Player_HasAvailableOwnedCraft((int)playerIdx) == 0) {
				Mission_ProcessFlightGroupWaveCompletion(player->boundFlightGroupIdx);
				if (Player_HasAvailableOwnedCraft((int)playerIdx) == 0) {
					Player_EndFlightParticipation((int)playerIdx);
					Player_EmitRemotePlayerDepartedMessages((int)playerIdx);
				}
			}
		}
	}

	if (g_flightSimSideEffectsSuppressed == 0) {
		for (playerIdx = 0; playerIdx < sizeof(g_playerAbortFlags) / sizeof(g_playerAbortFlags[0]);
			 ++playerIdx) {
			if (g_playerAbortFlags[playerIdx] != 0 && g_players[playerIdx].connectedFlag != 0) {
				if (g_players[playerIdx].objectIndex != -1)
					Player_UnbindFromCurrentCraft((int)playerIdx, 0, 1);
#ifdef XVT_MODERN
				if (g_localPlayer == (int)playerIdx)
					XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_LOCAL_DEPARTED, (int)playerIdx);
#endif
				if (g_localPlayer == (int)playerIdx)
					g_flightMissionState.missionEndPending = 1;
				g_players[playerIdx].connectedFlag = 0;
#ifdef XVT_MODERN
				XvtFlightDiag_PlayerRemoved((int)playerIdx);
#endif
				Flight_UpdateActivePlayerCount();
				if (NetSession_GetHostDplayId() != g_players[playerIdx].network.directPlayId)
					FlightNet_MarkPilotNetworkPlayerLeft((int)playerIdx);
				msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
				msg_emitInFlightMessage(IFMSG_380_ARG_HAS_QUIT_THE_MISSION, g_localPlayer);
				if (g_localPlayer == (int)playerIdx && NetSession_GetLocalPlayerId() != 0)
					FlightNet_BroadcastLocalPlayerLeft();
			}
		}
	}
}

// FUNCTION: XVT 0x485660
int Player_FindNearestEnemyFighter(int playerIdx, int excludedObjectIdx) {
	int16_t objectIdx;
	int playerIff;
	int team;
	uint8_t genusId;
	CraftData* craft;
	uint8_t objectKind;
	uint16_t staticObjectIdx;
	ObjectRecord* staticObject;
	int staticObjectTeam;
	int localPlayerIff;
	int isEnemy;
	uint32_t nearestDistance;
	ObjectRecord* object;
	int nearestObjectIdx;

	nearestDistance = UINT32_MAX;
	nearestObjectIdx = UINT16_MAX;
	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		object = &g_objectTable[objectIdx];
		if (object->objectType != 0 && g_players[playerIdx].objectIndex != objectIdx &&
			excludedObjectIdx != objectIdx) {
			playerIff = (uint16_t)g_players[playerIdx].playerIff;
			if (object->mobj->team != playerIff) {
				team = g_missionFlightGroups[g_objectTable[(uint16_t)objectIdx].flightGroupIdx].fg.team;
				if (team == playerIff)
					isEnemy = 0;
				else
					isEnemy = g_missionTeams[playerIff].allies[team] == 0;
				if (isEnemy) {
					genusId = object->genusId;
					if (genusId != CRAFT_GENUS_EXPLOSION && genusId != CRAFT_GENUS_STARSHIP &&
						genusId != CRAFT_GENUS_FREIGHTER && genusId != CRAFT_GENUS_PLATFORM &&
						Object_HasActiveDecoyBeam(objectIdx) == 0) {
						craft = g_objectTable[objectIdx].mobj->pCraft;
						if (craft->workingSubsystems != 0) {
							objectKind = craft->objectKind;
							if (objectKind == CRAFT_OBJECT_KIND_ACTIVE ||
								objectKind == CRAFT_OBJECT_KIND_ARRIVING_FROM_HYPERSPACE) {
								Player_ComputePolarToObjectRef(playerIdx, objectIdx);
								if ((uint32_t)trig2_polardistance < nearestDistance) {
									nearestObjectIdx = objectIdx;
									nearestDistance = trig2_polardistance;
								}
							}
						}
					}
				}
			}
		}
	}

	for (staticObjectIdx = g_regionMainObjectSlotEnd;
		 (int)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount) > (int16_t)staticObjectIdx;
		 ++staticObjectIdx) {
		staticObject = &g_objectTable[(int16_t)staticObjectIdx];
		if (staticObject->objectType != 0 && staticObject->genusId == CRAFT_GENUS_MINE &&
			excludedObjectIdx != (int16_t)staticObjectIdx && staticObject->typeSpecificWord != 0) {
			staticObjectTeam = g_missionFlightGroups[g_objectTable[staticObjectIdx].flightGroupIdx].fg.team;
			localPlayerIff = (uint16_t)g_players[playerIdx].playerIff;
			if (localPlayerIff == staticObjectTeam)
				isEnemy = 0;
			else
				isEnemy = g_missionTeams[localPlayerIff].allies[staticObjectTeam] == 0;
			if (isEnemy == 1) {
				Player_ComputePolarToObjectRef(playerIdx, (int16_t)staticObjectIdx);
				if ((uint32_t)trig2_polardistance < nearestDistance) {
					nearestObjectIdx = (int16_t)staticObjectIdx;
					nearestDistance = trig2_polardistance;
				}
			}
		}
	}
	return nearestObjectIdx;
}

// FUNCTION: XVT 0x485900
void Player_HandleHyperspaceCommand(struct CraftData* craft, unsigned int playerIdx) {
	enum {
		PLAYER_CONNECTED_PENDING_DEPARTURE = 2,
		S_FOIL_CLOSING_MASK = 1,
		S_FOIL_CLOSED_MASK = 2,
		HYPERDRIVE_SYSTEM_NAME_MESSAGE_ARG = 99,
		DAMAGED_SYSTEM_STATE_MESSAGE_ARG = 87,
	};

	if (g_flightSimSideEffectsSuppressed == 0) {
		if ((craft->systemFlags & CRAFT_SUBSYSTEM_FLAG_HYPERDRIVE) != 0) {
			if (g_flightMissionState.provingGroundsModeActive != 0) {
#ifdef XVT_MODERN
				XvtFlightDiag_NoteEnd(XVT_FLIGHT_END_PROVING_GROUNDS, playerIdx);
#endif
				g_flightMissionState.missionEndPending = 1;
				g_players[playerIdx].connectedFlag = PLAYER_CONNECTED_PENDING_DEPARTURE;
			} else if ((craft->workingSubsystems & CRAFT_SUBSYSTEM_FLAG_HYPERDRIVE) != 0) {
				int16_t scanObjectIdx;
				int16_t interdictorPresent;

				interdictorPresent = 0;
				for (scanObjectIdx = g_activeRegionObjectSlotStart;
					 scanObjectIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjectIdx) {
					ObjectRecord* object = &g_objectTable[scanObjectIdx];

					if ((object->objectType == CRAFT_SPECIES_INTERDICTOR ||
						 object->objectType == CRAFT_SPECIES_MODIFIED_STRIKE_CRUISER) &&
						(uint16_t)g_players[playerIdx].iff != (uint8_t)object->mobj->iff &&
						object->mobj->pCraft->workingSubsystems != 0)
						interdictorPresent = 1;
				}

				if (interdictorPresent != 0) {
					msg_emitInFlightMessage(IFMSG_109_INTERDICTOR_PREVENTS_HYPERDRIVE_UNIT_FROM_FUNCTIONING,
											playerIdx);
				} else {
					uint8_t objectType;

					if (g_localPlayer == (int)playerIdx)
						Hud_ClearReadyMessageQueue();
					msg_emitInFlightMessage(IFMSG_106_PREPARING_FOR_JUMP_TO_LIGHT_SPEED, playerIdx);
					g_players[playerIdx].hyperspacePhase = 1;
					g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks = 0;
					g_players[playerIdx].viewState.transitionTimer = 0;
					g_players[playerIdx].viewState.externalCameraActive = 0;
					g_players[playerIdx].viewState.playerInputBlocked = 0;
					g_players[playerIdx].viewState.cameraFocusObjIdx =
						(uint16_t)g_players[playerIdx].objectIndex;
					Hud_SetHudViewState(HUD_VIEW_FORWARD, playerIdx);
					g_players[playerIdx].viewState.hudAimX = 0;
					g_players[playerIdx].viewState.hudAimY = 0;
					craft->throttleSpeed = 0;

					objectType = g_objectTable[g_players[playerIdx].objectIndex].objectType;
					if ((objectType == CRAFT_SPECIES_X_WING || objectType == CRAFT_SPECIES_B_WING) &&
						(craft->sFoilState & S_FOIL_CLOSED_MASK) == 0) {
						craft->sFoilState |= S_FOIL_CLOSED_MASK;
						craft->sFoilState |= S_FOIL_CLOSING_MASK;
						fsfx_PlaySound(FLIGHT_SOUND_S_FOIL, -1, playerIdx);
					}

					if (g_localPlayer != (int)playerIdx) {
						msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
						msg_emitInFlightMessage(IFMSG_113_ARG_IS_INITIATING_HYPERJUMP, g_localPlayer);
					}
				}
			} else {
				g_msgArgTable[0] = HYPERDRIVE_SYSTEM_NAME_MESSAGE_ARG;
				g_msgArgTable[1] = DAMAGED_SYSTEM_STATE_MESSAGE_ARG;
				msg_emitInFlightMessage(IFMSG_086_ARG_SYSTEM_IS_ARG, playerIdx);
			}
		} else {
			uint16_t departureMothershipObjIdx;
			uint16_t alternateMothershipObjIdx;
			uint16_t objectIdx;

			departureMothershipObjIdx = UINT16_MAX;
			alternateMothershipObjIdx = departureMothershipObjIdx;
			for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
				 objectIdx < g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
				if (g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
						.fg.departureMethod != 0) {
					if (g_objectTable[objectIdx].objectType != CRAFT_SPECIES_UNKNOWN &&
						g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
								.fg.departureMothership == g_objectTable[objectIdx].flightGroupIdx)
						departureMothershipObjIdx = objectIdx;
				}
				if (g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
						.fg.alternateMothershipUsed != 0) {
					if (g_objectTable[objectIdx].objectType != CRAFT_SPECIES_UNKNOWN &&
						g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
								.fg.alternateMothership == g_objectTable[objectIdx].flightGroupIdx)
						alternateMothershipObjIdx = objectIdx;
				}
			}

			if (departureMothershipObjIdx != UINT16_MAX && alternateMothershipObjIdx != UINT16_MAX) {
				msg_formatObjectName(departureMothershipObjIdx, 0, g_flightTextScratchBuffer);
				msg_addMessagePtr(0, g_flightTextScratchBuffer);
				msg_formatObjectName(alternateMothershipObjIdx, 0, g_flightSecondaryObjectNameBuffer);
				msg_addMessagePtr(1, g_flightSecondaryObjectNameBuffer);
				msg_emitInFlightMessage(IFMSG_111_NO_HYPERDRIVE_RETURN_TO_ARG_OR_TO_ARG, playerIdx);
			} else if (departureMothershipObjIdx != UINT16_MAX) {
				msg_formatObjectName(departureMothershipObjIdx, 0, g_flightTextScratchBuffer);
				msg_addMessagePtr(0, g_flightTextScratchBuffer);
				msg_emitInFlightMessage(IFMSG_110_NO_HYPERDRIVE_RETURN_TO_ARG, playerIdx);
			} else if (alternateMothershipObjIdx != UINT16_MAX) {
				msg_formatObjectName(alternateMothershipObjIdx, 0, g_flightTextScratchBuffer);
				msg_addMessagePtr(0, g_flightTextScratchBuffer);
				msg_emitInFlightMessage(IFMSG_110_NO_HYPERDRIVE_RETURN_TO_ARG, playerIdx);
			}
		}
	}
}
