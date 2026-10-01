#include "xvt/flight/fediskio.h"

#ifdef XVT_MODERN
#include "xvt_runtime/snapshot/cockpit_messages.h"
#include "xvt_runtime/snapshot/render_assets.h"
#endif
#include "xvt/assets/model_bounds.h"
#include "xvt/assets/model_mesh.h"
#include "xvt/assets/object_type.h"
#include "xvt/assets/opt_model.h"
#include "xvt/assets/string_table.h"
#include "xvt/audio/fsfx.h"
#include "xvt/flight/craft.h"
#include "xvt/flight/flight.h"
#include "xvt/flight/flight_display.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/flight_surface.h"
#include "xvt/flight/hud/flight_map.h"
#include "xvt/flight/hud/flight_text.h"
#include "xvt/flight/hud/hud.h"
#include "xvt/flight/mission/mission.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/frontend/config.h"
#include "xvt/frontend/frontend_mission_list.h"
#include "xvt/frontend/pilot_record.h"
#include "xvt/render/flight_palette.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/image_quantizer.h"
#include "xvt/render/render_list.h"
#include "xvt/render/render_scene.h"
#include "xvt/render/renderer.h"
#include "xvt/render/tex_level.h"
#include "xvt/util/memory.h"

#include <stdlib.h>
#include <string.h>

#ifndef XVT_MODERN
typedef struct Msvc42FilePrefix {
	uint8_t reserved[12];
	int flags;
} Msvc42FilePrefix;
#endif

// GLOBAL: XVT 0x9D8A60
char g_fileName[256] = { 0 };
// GLOBAL: XVT 0x9A8C34
uint8_t* g_flightLog1Buffer = NULL;
// GLOBAL: XVT 0x9D8C1C
uint8_t* g_flightAuxBufferMirror = NULL;
// GLOBAL: XVT 0x9A20AC
uint16_t g_fileReadAbortFlag = 0;
// GLOBAL: XVT 0xA609C0
char* g_strDiskIoMessages[32] = { 0 };
// GLOBAL: XVT 0xA07BD8
XvtFile* g_stream = NULL;
// GLOBAL: XVT 0x5236B0
char g_flightPaletteResourceFileName[12] = {
	'n', 'e', 'w', 'p', 'a', 'l', '.', 'a', 'c', 't', '\0', '\0',
};
// GLOBAL: XVT 0x527508
unsigned int g_paletteGenerationEnabled = 0;
// GLOBAL: XVT 0x527620
char g_specListPrefixes[3][9] = {
	{ 'S', 'P', 'E', 'C', '\0', '\0', '\0', '\0', '\0' },
	{ 'S', 'P', 'E', 'C', '2', '\0', '\0', '\0', '\0' },
	{ 'S', 'P', 'E', 'C', '3', '\0', '\0', '\0', '\0' },
};
// GLOBAL: XVT 0x999400
uint16_t g_warheadGuidancePoolHandle = 0;
// GLOBAL: XVT 0x999402
uint16_t g_craftDataPoolHandle = 0;
// GLOBAL: XVT 0x999404
uint16_t g_mobileObjectPoolHandle = 0;
// GLOBAL: XVT 0x999406
uint16_t g_flightAuxBufferHandle = 0;
// GLOBAL: XVT 0x999408
uint16_t g_mobileObjectCharDataHandle = 0;
// GLOBAL: XVT 0x99940A
uint16_t g_objectTableHandle = 0;
// GLOBAL: XVT 0x9D7670
uint16_t g_stringDataHandle = 0;
// GLOBAL: XVT 0x612B94
uint16_t g_visibleObjectsHandle = 0;
// GLOBAL: XVT 0x612B98
uint16_t g_flightLog1BufferHandle = 0;
// GLOBAL: XVT 0x612BA0
uint8_t g_rgb565ToPaletteIndexLut[UINT16_MAX + 1u] = { 0 };
// GLOBAL: XVT 0x622BA0
uint16_t g_flightTinyFontHandle = 0;
// GLOBAL: XVT 0x622BA4
uint16_t g_flightOffscreenBufferHandle = 0;
// GLOBAL: XVT 0x622BA8
uint16_t g_flightMicroFontHandle = 0;
// GLOBAL: XVT 0x622BAC
uint16_t g_flightSmallFontHandle = 0;
// GLOBAL: XVT 0x527510
const int g_pilotKillScoreBaseByAiLevel[7] = { 3, 4, 4, 8, 12, 14, 0 };
// GLOBAL: XVT 0x52752C
const int g_pilotRatingPromotionPointThresholds[25] = {
	250,  500,  750,  1250, 1750, 2250, 2750, 3250, 3750, 4250,  4750,  5250,  5750,
	6250, 6500, 6500, 7000, 7250, 7500, 7750, 8000, 9000, 10000, 11000, 11000,
};
// GLOBAL: XVT 0x5275A8
const uint8_t g_placementAwardLevels[24] = {
	0, 0, 0, 5, 0, 0, 5, 0, 0, 4, 5, 0, 3, 4, 0, 2, 3, 5, 1, 2, 4, 1, 2, 3,
};
// GLOBAL: XVT 0x5275C4
const int g_missionAwardWinThresholds[5] = { 50000, 40000, 30000, 20000, 10000 };
// GLOBAL: XVT 0x5275E4
const int g_missionAwardScoreMarginThresholds[3] = { 50000, 20000, 0 };

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x498400
int16_t FeDiskIo_CommitFlightResults(int arg1, int arg2) {
	enum {
		PLAYER_COUNT = sizeof(g_players) / sizeof(g_players[0]),
		TEAM_COUNT = sizeof(g_flightMissionState.runtime.teamGoalStatus) /
					 sizeof(g_flightMissionState.runtime.teamGoalStatus[0]),
		FLIGHT_GROUP_CAPACITY = sizeof(g_missionFlightGroups) / sizeof(g_missionFlightGroups[0]),
		OBJECT_TYPE_STAT_COUNT = sizeof(g_pilotData.objectStats.killsPerCraftPerMT[0]) /
								 sizeof(g_pilotData.objectStats.killsPerCraftPerMT[0][0]),
		PLAYER_RATING_COUNT = sizeof(g_players[0].perMissionKills.killsFullOnPlayerRating) /
							  sizeof(g_players[0].perMissionKills.killsFullOnPlayerRating[0]),
		AI_RATING_COUNT = sizeof(g_players[0].perMissionKills.killsFullOnAiRating) /
						  sizeof(g_players[0].perMissionKills.killsFullOnAiRating[0]),
		TEAM_KILL_STAT_FULL = 0,
		TEAM_KILL_STAT_SHARED = 1,
		TEAM_KILL_STAT_ASSIST = 3,
		TEAM_GOAL_PRIMARY = 0,
		TEAM_GOAL_SECONDARY = 1,
		MISSION_STAT_TRAINING = 0,
		MISSION_STAT_MELEE = 1,
		MISSION_STAT_COMBAT = 2,
		FAILED_AWARD = 6,
		FIRST_PLACE = 1,
		MAX_STORED_AWARD = 5,
		PROMOTION_LOSS_LIMIT = -2000,
		MAX_PROMOTION_PERCENT = 100,
		UNLIMITED_WAVE_COUNT = 99,
		WAVE_REPLACEMENT_SCORE_FACTOR = 80,
	};

	uint8_t activeTeamFgCount[TEAM_COUNT];
	int connectedHumanCount;
	unsigned int activeTeamCount;
	unsigned int playerIdx;
	unsigned int networkIdx;
	unsigned int fgIdx;
	unsigned int teamIdx;
	unsigned int ratingIdx;
	unsigned int aiRatingIdx;
	unsigned int awardThresholdIdx;
	int award;
	int placement;
	int statType;
	int localPlayerIff;
	int score;
	int margin;
	int team0PlayerFgCount;
	int team1PlayerFgCount;
	int promotionThreshold;

	(void)arg1;
	(void)arg2;

	connectedHumanCount = 0;
	for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].network.directPlayId != 0) {
			++connectedHumanCount;
		}
	}

	memset(activeTeamFgCount, 0, sizeof(activeTeamFgCount));
	for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
		if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 && g_missionFgStats[fgIdx].hasArrived != 0) {
			++activeTeamFgCount[g_missionFlightGroups[fgIdx].fg.team];
		}
	}
	activeTeamCount = 0;
	for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
		if (activeTeamFgCount[teamIdx] != 0) {
			++activeTeamCount;
		}
	}

	if (g_missionHeader.missionType != MISSION_TYPE_SIMULATOR_1) {
		if (g_missionHeader.missionType == MISSION_TYPE_JUNKYARD ||
			g_missionHeader.missionType == MISSION_TYPE_SIMULATOR_2) {
			statType = MISSION_STAT_TRAINING;
		} else if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START) {
			statType = MISSION_STAT_MELEE;
		} else {
			statType = MISSION_STAT_COMBAT;
			team1PlayerFgCount = 0;
			team0PlayerFgCount = 0;
			for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
					g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
					if (g_missionFlightGroups[fgIdx].fg.team == 0) {
						++team0PlayerFgCount;
					} else if (g_missionFlightGroups[fgIdx].fg.team == 1) {
						++team1PlayerFgCount;
					}
				}
			}
			if (g_pilotData.missionSequenceActive == 1) {
				if (team1PlayerFgCount == 0) {
					if (g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_PRIMARY] != 1 &&
						g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_PRIMARY] == 0) {
						g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_PRIMARY] = 1;
					}
				} else if (team0PlayerFgCount == 0 &&
						   g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_PRIMARY] != 1 &&
						   g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_PRIMARY] == 0) {
					g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_PRIMARY] = 1;
				}
			} else if (connectedHumanCount == 1) {
				if (team1PlayerFgCount == 0) {
					if (g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_PRIMARY] != 1) {
						g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_SECONDARY] = 1;
						g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_PRIMARY] = 1;
					}
				} else if (team0PlayerFgCount == 0 &&
						   g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_PRIMARY] != 1) {
					g_flightMissionState.runtime.teamGoalStatus[1][TEAM_GOAL_SECONDARY] = 1;
					g_flightMissionState.runtime.teamGoalStatus[0][TEAM_GOAL_PRIMARY] = 1;
				}
			}
		}

		for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].network.directPlayId == 0) {
				continue;
			}
			for (networkIdx = 0; networkIdx < PLAYER_COUNT; ++networkIdx) {
				unsigned int playerFgIdx;
				unsigned int objectType;
				unsigned int modelIndex;
				int remainingCraftCount;

				if (g_pilotData.networkPlayers[networkIdx].directPlayId !=
					g_players[playerIdx].network.directPlayId) {
					continue;
				}
				playerFgIdx = g_players[networkIdx].boundFlightGroupIdx;
				if (g_flightMissionState.runtime
							.teamGoalStatus[g_players[networkIdx].playerIff][TEAM_GOAL_PRIMARY] != 1 ||
					statType == MISSION_STAT_MELEE ||
					g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_DEFAULT ||
					g_missionFlightGroups[playerFgIdx].fg.numberOfWaves == UNLIMITED_WAVE_COUNT) {
					continue;
				}
				remainingCraftCount = g_missionFlightGroups[playerFgIdx].fg.numberOfCraft *
									  g_missionFgStats[playerFgIdx].wavesRemaining;
				objectType = g_craftTypeToObjectType[g_missionFlightGroups[playerFgIdx].fg.craftType];
				modelIndex = GetModelIndexFromType(objectType);
				g_players[networkIdx].missionStats.missionScore += WAVE_REPLACEMENT_SCORE_FACTOR *
																   remainingCraftCount *
																   g_modelDefs[modelIndex].craftPointValue;
			}
		}

		memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
		localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff;
		score = g_players[g_localPlayer].missionStats.missionScore +
				g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][localPlayerIff];
		++g_pilotData.totalMissionsPlayedCount;
		++g_pilotData.factionStatistics[g_pilotData.currentFactionId].totalMissionsPlayedCount;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalScorePerMT[statType] += score;
		g_pilotData.mainStats.totalScorePerMT[statType] += score;
		if (g_pilotData.missionSequenceActive == 0) {
			++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				  .stats.standaloneMissionsPlayedPerMT[statType];
			++g_pilotData.mainStats.standaloneMissionsPlayedPerMT[statType];
		} else {
			++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				  .stats.sequenceMissionsPlayedPerMT[statType];
			++g_pilotData.mainStats.sequenceMissionsPlayedPerMT[statType];
		}

		for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
			unsigned int objectType;
			uint16_t kills;

			objectType = g_craftTypeToObjectType[g_missionFlightGroups[fgIdx].fg.craftType];
			if (objectType >= OBJECT_TYPE_STAT_COUNT) {
				continue;
			}
			kills = g_players[g_localPlayer].perMissionKills.killsFullOnFlightGroup[fgIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalKillsPerMT[statType] +=
				kills;
			g_pilotData.mainStats.totalKillsPerMT[statType] += kills;
			g_pilotData.objectStats.totalKillsPerMT[0] += kills;
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.mainStats.killsPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.objectStats.killsPerCraftPerMT[0][objectType] += kills;
			kills = g_players[g_localPlayer].perMissionKills.killsSharedOnFlightGroup[fgIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsSharedPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.mainStats.killsSharedPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.objectStats.killsSharedPerCraftPerMT[0][objectType] += kills;
			kills = g_players[g_localPlayer].perMissionKills.killsAssistOnFlightGroup[fgIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsAssistsPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.mainStats.killsAssistsPerCraftPerMT[statType][objectType] += kills;
			g_pilotData.objectStats.killsAssistsPerCraftPerMT[0][objectType] += kills;
		}

		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.totalFriendliesKilledPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.friendliesKilled;
		g_pilotData.mainStats.totalFriendliesKilledPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.friendliesKilled;
		g_pilotData.objectStats.totalFriendliesKilledPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.friendliesKilled;
		for (ratingIdx = 0; ratingIdx < PLAYER_RATING_COUNT; ++ratingIdx) {
			uint16_t value;

			value = g_players[g_localPlayer].perMissionKills.killsFullOnPlayerRating[ratingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsFullOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.mainStats.killsFullOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.objectStats.killsFullOnPlayerRatingPerMT[0][ratingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killsSharedOnPlayerRating[ratingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsSharedOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.mainStats.killsSharedOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.objectStats.killsSharedOnPlayerRatingPerMT[0][ratingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killsAssistOnPlayerRating[ratingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsAssistOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.mainStats.killsAssistOnPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.objectStats.killsAssistOnPlayerRatingPerMT[0][ratingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killedByPlayerRating[ratingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killedByPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.mainStats.killedByPlayerRatingPerMT[statType][ratingIdx] += value;
			g_pilotData.objectStats.killedByPlayerRatingPerMT[0][ratingIdx] += value;
		}
		for (aiRatingIdx = 0; aiRatingIdx < AI_RATING_COUNT; ++aiRatingIdx) {
			uint16_t value;

			value = g_players[g_localPlayer].perMissionKills.killsFullOnAiRating[aiRatingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsFullOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.mainStats.killsFullOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.objectStats.killsFullOnAIRatingPerMT[0][aiRatingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killsSharedOnAiRating[aiRatingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsSharedOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.mainStats.killsSharedOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.objectStats.killsSharedOnAIRatingPerMT[0][aiRatingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killsAssistOnAiRating[aiRatingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killsAssistOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.mainStats.killsAssistOnAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.objectStats.killsAssistOnAIRatingPerMT[0][aiRatingIdx] += value;
			value = g_players[g_localPlayer].perMissionKills.killedByAiRating[aiRatingIdx];
			g_pilotData.factionStatistics[g_pilotData.currentFactionId]
				.stats.killedByAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.mainStats.killedByAIRatingPerMT[statType][aiRatingIdx] += value;
			g_pilotData.objectStats.killedByAIRatingPerMT[0][aiRatingIdx] += value;
		}

		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.numSpecialInspectedPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.numSpecialInspected;
		g_pilotData.mainStats.numSpecialInspectedPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.numSpecialInspected;
		g_pilotData.objectStats.numSpecialInspectedPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.numSpecialInspected;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyFiredPerMT[statType] +=
			g_players[g_localPlayer].missionStats.laserShotsFired;
		g_pilotData.mainStats.energyFiredPerMT[statType] +=
			g_players[g_localPlayer].missionStats.laserShotsFired;
		g_pilotData.objectStats.energyFiredPerMT[0] += g_players[g_localPlayer].missionStats.laserShotsFired;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyFiredPerMT[statType] +=
			g_players[g_localPlayer].missionStats.ionShotsFired;
		g_pilotData.mainStats.energyFiredPerMT[statType] +=
			g_players[g_localPlayer].missionStats.ionShotsFired;
		g_pilotData.objectStats.energyFiredPerMT[0] += g_players[g_localPlayer].missionStats.ionShotsFired;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyHitsPerMT[statType] +=
			g_players[g_localPlayer].missionStats.laserHitsScored;
		g_pilotData.mainStats.energyHitsPerMT[statType] +=
			g_players[g_localPlayer].missionStats.laserHitsScored;
		g_pilotData.objectStats.energyHitsPerMT[0] += g_players[g_localPlayer].missionStats.laserHitsScored;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyHitsPerMT[statType] +=
			g_players[g_localPlayer].missionStats.ionHitsScored;
		g_pilotData.mainStats.energyHitsPerMT[statType] +=
			g_players[g_localPlayer].missionStats.ionHitsScored;
		g_pilotData.objectStats.energyHitsPerMT[0] += g_players[g_localPlayer].missionStats.ionHitsScored;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.warheadsFiredPerMT[statType] +=
			g_players[g_localPlayer].warheadsFired;
		g_pilotData.mainStats.warheadsFiredPerMT[statType] += g_players[g_localPlayer].warheadsFired;
		g_pilotData.objectStats.warheadsFiredPerMT[0] += g_players[g_localPlayer].warheadsFired;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.warheadsHitsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.warheadHits;
		g_pilotData.mainStats.warheadsHitsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.warheadHits;
		g_pilotData.objectStats.warheadsHitsPerMT[0] += g_players[g_localPlayer].perMissionKills.warheadHits;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalCraftLossesPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.totalCraftLosses;
		g_pilotData.mainStats.totalCraftLossesPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.totalCraftLosses;
		g_pilotData.objectStats.totalCraftLossesPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.totalCraftLosses;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByCollisionsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByCollisions;
		g_pilotData.mainStats.lossesByCollisionsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByCollisions;
		g_pilotData.objectStats.lossesByCollisionsPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.lossesByCollisions;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByStarshipsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByStarships;
		g_pilotData.mainStats.lossesByStarshipsPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByStarships;
		g_pilotData.objectStats.lossesByStarshipsPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.lossesByStarships;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByMinesPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByMines;
		g_pilotData.mainStats.lossesByMinesPerMT[statType] +=
			g_players[g_localPlayer].perMissionKills.lossesByMines;
		g_pilotData.objectStats.lossesByMinesPerMT[0] +=
			g_players[g_localPlayer].perMissionKills.lossesByMines;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].totalScore += score;
		g_pilotData.totalScore += score;
		g_pilotData.missionScore = score;
		sprintf(g_missionDebugBuffer, "Promo points: %d    Worse Promo points: %d\n",
				g_players[g_localPlayer].missionStats.ratingPromoPoints,
				g_players[g_localPlayer].missionStats.worseRatingPromoPoints);

		g_pilotData.promotionDelta = PILOT_PROMOTION_NONE;
		if (statType == MISSION_STAT_TRAINING) {
			int maximumTrainingRating;

			maximumTrainingRating = g_missionHeader.missionType == MISSION_TYPE_SIMULATOR_2
										? PILOT_RATING_JEDI_MASTER
										: PILOT_RATING_OFFICER_1ST_CLASS;
			if ((unsigned int)g_pilotData.rating < (unsigned int)maximumTrainingRating) {
				g_pilotData.currentRatingPromoPoints +=
					g_players[g_localPlayer].missionStats.ratingPromoPoints;
				promotionThreshold = g_pilotRatingPromotionPointThresholds[g_pilotData.rating];
				if (g_pilotData.currentRatingWorsePromoPoints < promotionThreshold / 2) {
					g_pilotData.currentRatingWorsePromoPoints +=
						g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
					g_pilotData.currentRatingPromoPoints +=
						g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				}
				if (g_pilotData.currentRatingPromoPoints >= promotionThreshold) {
					g_pilotData.currentRatingPromoPoints = 0;
					g_pilotData.currentRatingWorsePromoPoints = 0;
					g_pilotData.promotionDelta = PILOT_PROMOTION_PROMOTION;
					++g_pilotData.rating;
					g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.rating] =
						g_pilotData.totalMissionsPlayedCount;
				}
				if (g_pilotData.currentRatingPromoPoints >= 0) {
					g_pilotData.nextPromotionPercent =
						MAX_PROMOTION_PERCENT * g_pilotData.currentRatingPromoPoints /
						g_pilotRatingPromotionPointThresholds[g_pilotData.rating];
					if (g_pilotData.nextPromotionPercent > MAX_PROMOTION_PERCENT) {
						g_pilotData.nextPromotionPercent = MAX_PROMOTION_PERCENT;
					}
				} else if (g_pilotData.rating != PILOT_RATING_TARGET_DRONE) {
					g_pilotData.nextPromotionPercent =
						MAX_PROMOTION_PERCENT * g_pilotData.currentRatingPromoPoints / -PROMOTION_LOSS_LIMIT;
					if (g_pilotData.nextPromotionPercent < -MAX_PROMOTION_PERCENT) {
						g_pilotData.nextPromotionPercent = -MAX_PROMOTION_PERCENT;
					}
				} else {
					g_pilotData.currentRatingPromoPoints = 0;
					g_pilotData.currentRatingWorsePromoPoints = 0;
					g_pilotData.nextPromotionPercent = 0;
				}
			} else if (g_players[g_localPlayer].missionStats.ratingPromoPoints < 0) {
				g_pilotData.currentRatingPromoPoints +=
					g_players[g_localPlayer].missionStats.ratingPromoPoints;
			}
			if (g_pilotData.rating != PILOT_RATING_TARGET_DRONE &&
				g_pilotData.currentRatingPromoPoints < PROMOTION_LOSS_LIMIT) {
				g_pilotData.currentRatingPromoPoints = 0;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				--g_pilotData.rating;
				g_pilotData.nextPromotionPercent = 0;
				g_pilotData.promotionDelta = PILOT_PROMOTION_DEMOTION;
				if ((unsigned int)g_pilotData.rating < PILOT_RATING_TRAINEE) {
					g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.rating] =
						g_pilotData.totalMissionsPlayedCount;
				}
			}
		} else if (statType == MISSION_STAT_MELEE || statType == MISSION_STAT_COMBAT) {
			if ((unsigned int)g_pilotData.rating < PILOT_RATING_JEDI_MASTER) {
				g_pilotData.currentRatingPromoPoints +=
					g_players[g_localPlayer].missionStats.ratingPromoPoints;
				promotionThreshold = g_pilotRatingPromotionPointThresholds[g_pilotData.rating];
				if (g_pilotData.currentRatingWorsePromoPoints < promotionThreshold / 2) {
					g_pilotData.currentRatingWorsePromoPoints +=
						g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
					g_pilotData.currentRatingPromoPoints +=
						g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				}
				if (g_pilotData.currentRatingPromoPoints >= promotionThreshold) {
					g_pilotData.currentRatingWorsePromoPoints = 0;
					g_pilotData.currentRatingPromoPoints -= promotionThreshold;
					g_pilotData.promotionDelta = PILOT_PROMOTION_PROMOTION;
					++g_pilotData.rating;
					g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.rating] =
						g_pilotData.totalMissionsPlayedCount;
				}
				if (g_pilotData.currentRatingPromoPoints >= 0) {
					g_pilotData.nextPromotionPercent =
						MAX_PROMOTION_PERCENT * g_pilotData.currentRatingPromoPoints /
						g_pilotRatingPromotionPointThresholds[g_pilotData.rating];
					if (g_pilotData.nextPromotionPercent > MAX_PROMOTION_PERCENT) {
						g_pilotData.nextPromotionPercent = MAX_PROMOTION_PERCENT;
					}
				} else if (g_pilotData.rating != PILOT_RATING_TARGET_DRONE) {
					g_pilotData.nextPromotionPercent =
						MAX_PROMOTION_PERCENT * g_pilotData.currentRatingPromoPoints / -PROMOTION_LOSS_LIMIT;
					if (g_pilotData.nextPromotionPercent < -MAX_PROMOTION_PERCENT) {
						g_pilotData.nextPromotionPercent = -MAX_PROMOTION_PERCENT;
					}
				} else {
					g_pilotData.currentRatingPromoPoints = 0;
					g_pilotData.currentRatingWorsePromoPoints = 0;
					g_pilotData.nextPromotionPercent = 0;
				}
			} else if (g_players[g_localPlayer].missionStats.ratingPromoPoints < 0) {
				g_pilotData.currentRatingPromoPoints +=
					g_players[g_localPlayer].missionStats.ratingPromoPoints;
			}
			if (g_pilotData.rating != PILOT_RATING_TARGET_DRONE &&
				g_pilotData.currentRatingPromoPoints < PROMOTION_LOSS_LIMIT) {
				g_pilotData.currentRatingPromoPoints = 0;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				--g_pilotData.rating;
				g_pilotData.nextPromotionPercent = 0;
				g_pilotData.promotionDelta = PILOT_PROMOTION_DEMOTION;
				if ((unsigned int)g_pilotData.rating < PILOT_RATING_TRAINEE) {
					g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.rating] =
						g_pilotData.totalMissionsPlayedCount;
				}
			}
		}

		for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].network.directPlayId == 0) {
				continue;
			}
			for (networkIdx = 0; networkIdx < PLAYER_COUNT; ++networkIdx) {
				if (g_pilotData.networkPlayers[networkIdx].directPlayId ==
					g_players[playerIdx].network.directPlayId) {
					g_pilotData.killsFullOnPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsFullOnPlayer[playerIdx];
					g_pilotData.killsSharedOnPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsSharedOnPlayer[playerIdx];
					g_pilotData.killsFullFromPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsFullFromPlayer[playerIdx];
					g_pilotData.killsSharedFromPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsSharedFromPlayer[playerIdx];
					break;
				}
			}
		}
		for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
			int groupAI;
			int flightGroupRating;

			g_pilotData.killsFullOnFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsFullOnFlightGroup[fgIdx];
			g_pilotData.killsSharedOnFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsSharedOnFlightGroup[fgIdx];
			g_pilotData.killsFullFromFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsFullFromFlightGroup[fgIdx];
			g_pilotData.killsSharedFromFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsSharedFromFlightGroup[fgIdx];
			if (g_missionHeader.missionType == MISSION_TYPE_QUICK_START &&
				(g_pilotData.missionSequenceActive != 1 ||
				 g_pilotData.meleeTournamentSequenceState.currentMissionIndex == 0)) {
				groupAI = g_missionFlightGroups[fgIdx].fg.groupAI;
				flightGroupRating = g_pilotKillScoreBaseByAiLevel[groupAI];
				if (groupAI != 0) {
					flightGroupRating += fgIdx & 3;
				}
				g_pilotData.flightGroupRating[fgIdx] = flightGroupRating;
			}
		}

		for (teamIdx = 0; teamIdx < PLAYER_COUNT; ++teamIdx) {
			PilotTeam* team;

			team = &g_pilotData.teams[teamIdx];
			team->isMissionCompleted =
				g_flightMissionState.runtime.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] == 1 &&
				g_flightMissionState.runtime.teamGoalStatus[teamIdx][TEAM_GOAL_SECONDARY] != 1;
			team->missionScore = g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx];
			team->missionTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds[teamIdx];
			team->kills = g_flightMissionState.runtime.teamKillStats[TEAM_KILL_STAT_FULL][teamIdx];
			team->killsShared = g_flightMissionState.runtime.teamKillStats[TEAM_KILL_STAT_SHARED][teamIdx];
			team->killsAssist = g_flightMissionState.runtime.teamKillStats[TEAM_KILL_STAT_ASSIST][teamIdx];
			if (statType == MISSION_STAT_MELEE) {
				team->missionScore += g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx];
			} else {
				for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
					if (g_players[playerIdx].network.directPlayId != 0 &&
						(uint16_t)g_players[playerIdx].playerIff == teamIdx) {
						team->missionScore += g_players[playerIdx].missionStats.missionScore;
					}
				}
			}
		}
		for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].network.directPlayId == 0) {
				continue;
			}
			for (networkIdx = 0; networkIdx < PLAYER_COUNT; ++networkIdx) {
				if (g_pilotData.networkPlayers[networkIdx].directPlayId ==
					g_players[playerIdx].network.directPlayId) {
					for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
						g_pilotData.networkPlayers[networkIdx].kills +=
							g_players[playerIdx].perMissionKills.killsFullOnFlightGroup[fgIdx];
						g_pilotData.networkPlayers[networkIdx].killsShared +=
							g_players[playerIdx].perMissionKills.killsSharedOnFlightGroup[fgIdx];
						g_pilotData.networkPlayers[networkIdx].killsAssist +=
							g_players[playerIdx].perMissionKills.killsAssistOnFlightGroup[fgIdx];
					}
					g_pilotData.networkPlayers[networkIdx].totalScore =
						g_players[playerIdx].missionStats.missionScore +
						g_flightMissionState.runtime
							.teamScores[TEAM_SCORE_BONUS_TENTHS][(uint16_t)g_players[playerIdx].playerIff];
					g_pilotData.networkPlayers[networkIdx].totalLosses =
						g_players[playerIdx].perMissionKills.totalCraftLosses;
					break;
				}
			}
		}

		for (awardThresholdIdx = 0;
			 awardThresholdIdx < sizeof(g_pilotData.factionStatistics[0].missionAwards) /
									 sizeof(g_pilotData.factionStatistics[0].missionAwards[0]);
			 ++awardThresholdIdx) {
			g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[awardThresholdIdx] = 0;
		}
		award = 0;
		if (statType == MISSION_STAT_TRAINING) {
			if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
														   [TEAM_GOAL_PRIMARY] == 1 &&
				g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
														   [TEAM_GOAL_SECONDARY] != 1) {
				award = 1;
				for (awardThresholdIdx = 0;
					 awardThresholdIdx < sizeof(g_missionAwardScoreMarginThresholds) /
											 sizeof(g_missionAwardScoreMarginThresholds[0]);
					 ++awardThresholdIdx) {
					if (score >= g_missionAwardScoreMarginThresholds[awardThresholdIdx]) {
						break;
					}
					++award;
				}
				if (g_flightMissionState.difficulty == GAME_DIFFICULTY_EASY) {
					award += 2;
				} else if (g_flightMissionState.difficulty == GAME_DIFFICULTY_MEDIUM) {
					++award;
				}
				if (g_flightMissionState.playerFlightGroupWaveMode == CRAFT_WAVES_UNLIMITED && award < 5) {
					award = 5;
				}
				if (award > MAX_STORED_AWARD) {
					award = 0;
				}
			} else if (score <= 0) {
				award = FAILED_AWARD;
			}
			if (g_pilotData.missionSequenceActive == 1 &&
				(g_gameConfig.difficulty > GAME_DIFFICULTY_HARD ||
				 g_flightMissionState.playerFlightGroupWaveMode == CRAFT_WAVES_UNLIMITED)) {
				award = FAILED_AWARD;
			}
			if (award != 0) {
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[2] = award;
			}
		} else if (statType == MISSION_STAT_MELEE) {
			int playerTeamScore;
			int betterTeamCount;

			betterTeamCount = 0;
			margin = 0;
			playerTeamScore =
				g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][localPlayerIff] +
				g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][localPlayerIff];
			for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
				int hasOpponent;
				int opponentScore;

				if (teamIdx == (unsigned int)localPlayerIff) {
					continue;
				}
				hasOpponent = 0;
				for (fgIdx = 0; fgIdx < (unsigned int)g_missionHeader.numFlightGroups; ++fgIdx) {
					if (g_missionFlightGroups[fgIdx].fg.team == teamIdx &&
						(g_missionFlightGroups[fgIdx].playerOwnerIdx != -1 ||
						 (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
						  g_flightMissionState.aiOpponentsEnabled != 0))) {
						hasOpponent = 1;
					}
				}
				if (!hasOpponent) {
					continue;
				}
				opponentScore = g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] +
								g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx];
				if (playerTeamScore < opponentScore) {
					++betterTeamCount;
				} else if (playerTeamScore - opponentScore < margin || margin == 0) {
					margin = playerTeamScore - opponentScore;
				}
			}
			placement = betterTeamCount + 1;
			if (activeTeamCount > 1 && connectedHumanCount > 1) {
				if ((unsigned int)placement == activeTeamCount && activeTeamCount >= 4) {
					award = FAILED_AWARD;
				} else if (score > 5000 && placement < 4) {
					award = g_placementAwardLevels[3 * activeTeamCount - 4 + placement];
				}
				if (award != 0 && award != FAILED_AWARD) {
					if (score < 1250) {
						award += 2;
						if (award > MAX_STORED_AWARD) {
							award = MAX_STORED_AWARD;
						}
					} else if (score < 2500) {
						++award;
						if (award > MAX_STORED_AWARD) {
							award = MAX_STORED_AWARD;
						}
					}
				}
				if (placement == FIRST_PLACE && award > 0 && award != FAILED_AWARD) {
					if (margin > 15000) {
						award -= 3;
					} else if (margin > 10000) {
						award -= 2;
					} else if (margin > 5000) {
						--award;
					}
					if (award < 1) {
						award = 1;
					}
				}
			} else {
				switch (g_flightMissionState.difficulty) {
					case GAME_DIFFICULTY_EASY:
						if (placement == 1) {
							award = 5;
						} else if (placement > 4) {
							award = 6;
						}
						break;
					case GAME_DIFFICULTY_MEDIUM:
						if (placement == 1) {
							if (score <= 0) {
								award = 5;
							} else {
								award = 4;
								if (margin > 10000) {
									award = 2;
								} else if (margin > 5000) {
									award = 3;
								}
							}
						} else if (placement == 2 && activeTeamCount > 2) {
							award = 5;
						} else if ((unsigned int)placement == activeTeamCount || placement > 6) {
							award = 6;
						}
						break;
					case GAME_DIFFICULTY_HARD:
						if (placement == 1) {
							if (score <= 0) {
								award = 5;
							} else {
								award = 3;
								if (margin > 10000) {
									award = 1;
								} else if (margin > 5000) {
									award = 2;
								}
							}
						} else if (placement == 2) {
							if (activeTeamCount > 4) {
								award = 4;
							} else if (activeTeamCount > 2) {
								award = 5;
							}
						} else if (placement == 3 && activeTeamCount > 4) {
							award = 5;
						} else if (placement > 7) {
							award = 6;
						}
						break;
				}
			}
			if (award != 0) {
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[0] = award;
			}
			sprintf(g_missionDebugBuffer, "Player's team score: %d   Place: %d   Margin: %d   Award: %d\n",
					playerTeamScore, placement, margin, award);
		} else if (statType == MISSION_STAT_COMBAT) {
			if (g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] == 2 ||
				g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] == 1) {
				award = FAILED_AWARD;
			} else if (connectedHumanCount == 1) {
				if (g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] == 1 &&
					g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] != 1) {
					award = 1;
					for (awardThresholdIdx = 0;
						 awardThresholdIdx < sizeof(g_missionAwardScoreMarginThresholds) /
												 sizeof(g_missionAwardScoreMarginThresholds[0]);
						 ++awardThresholdIdx) {
						if (score >= g_missionAwardScoreMarginThresholds[awardThresholdIdx]) {
							break;
						}
						++award;
					}
					if (g_flightMissionState.difficulty == GAME_DIFFICULTY_EASY) {
						award += 2;
					} else if (g_flightMissionState.difficulty == GAME_DIFFICULTY_MEDIUM) {
						++award;
					}
					if (g_flightMissionState.playerFlightGroupWaveMode == CRAFT_WAVES_UNLIMITED) {
						award = MAX_STORED_AWARD;
					}
					if (award >= FAILED_AWARD) {
						award = 0;
					}
				} else if (score <= 0) {
					award = FAILED_AWARD;
				}
			} else if (g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] == 1 &&
					   g_flightMissionState.runtime.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] !=
						   1) {
				award = 1;
				for (awardThresholdIdx = 0; awardThresholdIdx <= 4; ++awardThresholdIdx) {
					if (score >= g_missionAwardWinThresholds[awardThresholdIdx]) {
						break;
					}
					++award;
				}
				if (g_flightMissionState.playerFlightGroupWaveMode == CRAFT_WAVES_UNLIMITED) {
					award = MAX_STORED_AWARD;
				}
				if (award >= FAILED_AWARD) {
					award = 0;
				}
			} else if (score < -25000) {
				award = FAILED_AWARD;
			}
			if (award != 0) {
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[2] = award;
			}
		}

		if (connectedHumanCount == 1) {
			switch (statType) {
				case MISSION_STAT_TRAINING: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TRAINING_EXERCISES];
					if (g_pilotData.missionSequenceActive == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spCampaignMissions[missionId - 1]
							  .numberTimesFlown;
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaignMissions[missionId - 1]
							.campaignId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_CAMPAIGNS];
						if (g_pilotData.teams[g_pilotData.team].isMissionCompleted != 0) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCampaignMissions[missionId - 1]
								.isCompleted = 1;
						}
						if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.bestScore < (unsigned int)score) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.bestScore = score;
							}
							if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
								(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										 [(uint16_t)g_players[g_localPlayer].playerIff] <
									 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .spCampaignMissions[missionId - 1]
										 .bestTime ||
								 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .spCampaignMissions[missionId - 1]
										 .bestTime == 0)) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
													[(uint16_t)g_players[g_localPlayer].playerIff];
							}
							if (g_pilotData.teams[g_pilotData.team].isMissionCompleted != 0 &&
								g_gameConfig.difficulty <= GAME_DIFFICULTY_HARD) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.awardEligible = 1;
							}
						}
						if (award != 0) {
							oldAward =
								(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.awardId;
							if (oldAward > (unsigned int)award || oldAward == 0) {
								if (oldAward != 0 &&
									g_pilotData.factionStatistics[g_pilotData.currentFactionId]
											.missionEvaluations[oldAward - 1] != 0) {
									--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										  .missionEvaluations[oldAward - 1];
								}
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spCampaignMissions[missionId - 1]
									.awardId = (int)award;
								++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[award - 1];
							}
						} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .spCampaignMissions[missionId - 1]
									   .awardId == FAILED_AWARD) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCampaignMissions[missionId - 1]
								.awardId = 0;
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.missionEvaluations[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[FAILED_AWARD - 1];
							}
						}
					} else {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spTrainingMissions[missionId]
							  .numberTimesFlown;
						if (g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_PRIMARY] == 1) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .spTrainingMissions[missionId]
								  .completedCount;
						}
						if (g_flightMissionState.runtime
									.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
												   [TEAM_GOAL_PRIMARY] == 2 ||
							g_flightMissionState.runtime
									.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
												   [TEAM_GOAL_SECONDARY] == 1) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .spTrainingMissions[missionId]
								  .failedCount;
						}
						if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spTrainingMissions[missionId]
									.bestScore < score) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spTrainingMissions[missionId]
									.bestScore = score;
							}
							if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
								(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										 [(uint16_t)g_players[g_localPlayer].playerIff] <
									 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .spTrainingMissions[missionId]
										 .bestTime ||
								 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .spTrainingMissions[missionId]
										 .bestTime == 0)) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spTrainingMissions[missionId]
									.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
													[(uint16_t)g_players[g_localPlayer].playerIff];
							}
						}
						if (award != 0) {
							oldAward =
								(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spTrainingMissions[missionId]
									.awardId;
							if (oldAward > (unsigned int)award || oldAward == 0) {
								if (oldAward != 0 &&
									g_pilotData.factionStatistics[g_pilotData.currentFactionId]
											.missionEvaluations[oldAward - 1] != 0) {
									--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										  .missionEvaluations[oldAward - 1];
								}
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.spTrainingMissions[missionId]
									.awardId = (int)award;
								++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[award - 1];
							}
						} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .spTrainingMissions[missionId]
									   .awardId == FAILED_AWARD) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spTrainingMissions[missionId]
								.awardId = 0;
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.missionEvaluations[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[FAILED_AWARD - 1];
							}
						}
					}
					break;
				}
				case MISSION_STAT_MELEE: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_MELEES];
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spMeleeMissions[missionId]
						  .numberTimesFlown;
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spMeleeMissions[missionId]
							  .completedCount;
					}
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 2 ||
						g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_SECONDARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spMeleeMissions[missionId]
							  .failedCount;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.bestScore < score) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.bestScore = score;
					}
					if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
								[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
						(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
								 [(uint16_t)g_players[g_localPlayer].playerIff] <
							 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .spMeleeMissions[missionId]
								 .bestTime ||
						 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .spMeleeMissions[missionId]
								 .bestTime == 0)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
											[(uint16_t)g_players[g_localPlayer].playerIff];
					}
					if (placement != 0 &&
						((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .spMeleeMissions[missionId]
								 .bestPlacement > (unsigned int)placement ||
						 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .spMeleeMissions[missionId]
								 .bestPlacement == 0)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.bestPlacement = (int)placement;
					}
					if (placement == FIRST_PLACE &&
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spMeleeMissions[missionId]
								.bestBonus < (unsigned int)margin &&
						margin > 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.bestBonus = margin;
					}
					if (award != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .spMeleeMissions[missionId]
									   .awardId;
						if (oldAward > (unsigned int)award || oldAward == 0) {
							if (oldAward != 0 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
														 .meleePlaques[oldAward - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .meleePlaques[oldAward - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spMeleeMissions[missionId]
								.awardId = (int)award;
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .meleePlaques[award - 1];
						}
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .spMeleeMissions[missionId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spMeleeMissions[missionId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.meleePlaques[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .meleePlaques[FAILED_AWARD - 1];
						}
					}
					break;
				}
				case MISSION_STAT_COMBAT: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_COMBAT_ENGAGEMENTS];
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spCombatMissions[missionId]
						  .numberTimesFlown;
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spCombatMissions[missionId]
							  .completedCount;
					}
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 2 ||
						g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_SECONDARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spCombatMissions[missionId]
							  .failedCount;
					}
					if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCombatMissions[missionId]
								.bestScore < score) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCombatMissions[missionId]
								.bestScore = score;
						}
						if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
									[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
							(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
									 [(uint16_t)g_players[g_localPlayer].playerIff] <
								 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									 .spCombatMissions[missionId]
									 .bestTime ||
							 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									 .spCombatMissions[missionId]
									 .bestTime == 0)) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCombatMissions[missionId]
								.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
												[(uint16_t)g_players[g_localPlayer].playerIff];
						}
					}
					if (award != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .spCombatMissions[missionId]
									   .awardId;
						if (oldAward > (unsigned int)award || oldAward == 0) {
							if (oldAward != 0 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
														 .missionEvaluations[oldAward - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[oldAward - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spCombatMissions[missionId]
								.awardId = (int)award;
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .missionEvaluations[award - 1];
						}
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .spCombatMissions[missionId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCombatMissions[missionId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.missionEvaluations[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .missionEvaluations[FAILED_AWARD - 1];
						}
					}
					break;
				}
			}
		} else {
			switch (statType) {
				case MISSION_STAT_TRAINING: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TRAINING_EXERCISES];
					if (g_pilotData.missionSequenceActive == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpCampaignMissions[missionId - 1]
							  .numberTimesFlown;
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaignMissions[missionId - 1]
							.campaignId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_CAMPAIGNS];
						if (g_pilotData.teams[g_pilotData.team].isMissionCompleted != 0) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCampaignMissions[missionId - 1]
								.isCompleted = 1;
						}
						if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.bestScore < (unsigned int)score) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.bestScore = score;
							}
							if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
								(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										 [(uint16_t)g_players[g_localPlayer].playerIff] <
									 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .mpCampaignMissions[missionId - 1]
										 .bestTime ||
								 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .mpCampaignMissions[missionId - 1]
										 .bestTime == 0)) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
													[(uint16_t)g_players[g_localPlayer].playerIff];
							}
							if (g_pilotData.teams[g_pilotData.team].isMissionCompleted != 0 &&
								g_gameConfig.difficulty <= GAME_DIFFICULTY_HARD) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.awardEligible = 1;
							}
						}
						if (award != 0) {
							oldAward =
								(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.awardId;
							if (oldAward > (unsigned int)award || oldAward == 0) {
								if (oldAward == FAILED_AWARD &&
									g_pilotData.factionStatistics[g_pilotData.currentFactionId]
											.missionEvaluations[FAILED_AWARD - 1] != 0) {
									--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										  .missionEvaluations[FAILED_AWARD - 1];
								}
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCampaignMissions[missionId - 1]
									.awardId = (int)award;
							}
							if (award != FAILED_AWARD ||
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										.mpCampaignMissions[missionId - 1]
										.awardId == 0) {
								++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[award - 1];
							}
						} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .mpCampaignMissions[missionId - 1]
									   .awardId == FAILED_AWARD) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCampaignMissions[missionId - 1]
								.awardId = 0;
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.missionEvaluations[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[FAILED_AWARD - 1];
							}
						}
					} else {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpTrainingMissions[missionId]
							  .numberTimesFlown;
						if (g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_PRIMARY] == 1) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .mpTrainingMissions[missionId]
								  .completedCount;
						}
						if (g_flightMissionState.runtime
									.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
												   [TEAM_GOAL_PRIMARY] == 2 ||
							g_flightMissionState.runtime
									.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
												   [TEAM_GOAL_SECONDARY] == 1) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .mpTrainingMissions[missionId]
								  .failedCount;
						}
						if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpTrainingMissions[missionId]
									.bestScore < score) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpTrainingMissions[missionId]
									.bestScore = score;
							}
							if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
								(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
										 [(uint16_t)g_players[g_localPlayer].playerIff] <
									 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .mpTrainingMissions[missionId]
										 .bestTime ||
								 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										 .mpTrainingMissions[missionId]
										 .bestTime == 0)) {
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpTrainingMissions[missionId]
									.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
													[(uint16_t)g_players[g_localPlayer].playerIff];
							}
						}
						if (award != 0) {
							oldAward =
								(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpTrainingMissions[missionId]
									.awardId;
							if (oldAward > (unsigned int)award || oldAward == 0) {
								if (oldAward == FAILED_AWARD &&
									g_pilotData.factionStatistics[g_pilotData.currentFactionId]
											.missionEvaluations[FAILED_AWARD - 1] != 0) {
									--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										  .missionEvaluations[FAILED_AWARD - 1];
								}
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpTrainingMissions[missionId]
									.awardId = (int)award;
							}
							if (award != FAILED_AWARD ||
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										.mpTrainingMissions[missionId]
										.awardId == 0) {
								++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[award - 1];
							}
						} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .mpTrainingMissions[missionId]
									   .awardId == FAILED_AWARD) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpTrainingMissions[missionId]
								.awardId = 0;
							if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.missionEvaluations[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[FAILED_AWARD - 1];
							}
						}
					}
					break;
				}
				case MISSION_STAT_MELEE: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_MELEES];
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpMeleeMissions[missionId]
						  .numberTimesFlown;
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpMeleeMissions[missionId]
							  .completedCount;
					}
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 2 ||
						g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_SECONDARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpMeleeMissions[missionId]
							  .failedCount;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.bestScore < score) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.bestScore = score;
					}
					if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
								[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
						(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
								 [(uint16_t)g_players[g_localPlayer].playerIff] <
							 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .mpMeleeMissions[missionId]
								 .bestTime ||
						 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .mpMeleeMissions[missionId]
								 .bestTime == 0)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
											[(uint16_t)g_players[g_localPlayer].playerIff];
					}
					if (placement == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpMeleeMissions[missionId]
							  .firstPlaceCount;
					} else if (placement == 2) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpMeleeMissions[missionId]
							  .secondPlaceCount;
					} else if (placement == 3) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpMeleeMissions[missionId]
							  .thirdPlaceCount;
					}
					if (placement != 0 &&
						((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .mpMeleeMissions[missionId]
								 .bestPlacement > (unsigned int)placement ||
						 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								 .mpMeleeMissions[missionId]
								 .bestPlacement == 0)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.bestPlacement = (int)placement;
					}
					if (placement == FIRST_PLACE &&
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpMeleeMissions[missionId]
								.bestBonus < (unsigned int)margin &&
						margin > 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.bestBonus = margin;
					}
					if (award != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .mpMeleeMissions[missionId]
									   .awardId;
						if (oldAward > (unsigned int)award || oldAward == 0) {
							if (oldAward == FAILED_AWARD &&
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										.meleePlaques[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .meleePlaques[FAILED_AWARD - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpMeleeMissions[missionId]
								.awardId = (int)award;
						}
						if (award != FAILED_AWARD ||
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpMeleeMissions[missionId]
									.awardId == 0) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .meleePlaques[award - 1];
						}
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .mpMeleeMissions[missionId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpMeleeMissions[missionId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.meleePlaques[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .meleePlaques[FAILED_AWARD - 1];
						}
					}
					break;
				}
				case MISSION_STAT_COMBAT: {
					unsigned int oldAward;
					int missionId;

					missionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_COMBAT_ENGAGEMENTS];
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpCombatMissions[missionId]
						  .numberTimesFlown;
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpCombatMissions[missionId]
							  .completedCount;
					}
					if (g_flightMissionState.runtime.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																		.playerIff][TEAM_GOAL_PRIMARY] == 2 ||
						g_flightMissionState.runtime
								.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff]
											   [TEAM_GOAL_SECONDARY] == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpCombatMissions[missionId]
							  .failedCount;
					}
					if (g_flightMissionState.playerFlightGroupWaveMode != CRAFT_WAVES_UNLIMITED) {
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCombatMissions[missionId]
								.bestScore < score) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCombatMissions[missionId]
								.bestScore = score;
						}
						if (g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
									[(uint16_t)g_players[g_localPlayer].playerIff] != 0 &&
							(g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
									 [(uint16_t)g_players[g_localPlayer].playerIff] <
								 (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									 .mpCombatMissions[missionId]
									 .bestTime ||
							 g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									 .mpCombatMissions[missionId]
									 .bestTime == 0)) {
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCombatMissions[missionId]
								.bestTime = g_flightMissionState.runtime.teamMissionCompletionTimeSeconds
												[(uint16_t)g_players[g_localPlayer].playerIff];
						}
					}
					if (award != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .mpCombatMissions[missionId]
									   .awardId;
						if (oldAward > (unsigned int)award || oldAward == 0) {
							if (oldAward == FAILED_AWARD &&
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										.missionEvaluations[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .missionEvaluations[FAILED_AWARD - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpCombatMissions[missionId]
								.awardId = (int)award;
						}
						if (award != FAILED_AWARD ||
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.mpCombatMissions[missionId]
									.awardId == 0) {
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .missionEvaluations[award - 1];
						}
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .mpCombatMissions[missionId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCombatMissions[missionId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.missionEvaluations[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .missionEvaluations[FAILED_AWARD - 1];
						}
					}
					break;
				}
			}
		}

		if (g_pilotData.missionSequenceActive == 1 && statType == MISSION_STAT_TRAINING) {
			int campaignId;

			campaignId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_CAMPAIGNS];
			g_pilotData.campaignSequenceState.lastMissionCompleted =
				g_pilotData.teams[g_pilotData.team].isMissionCompleted;
			if (g_pilotData.campaignSequenceState.lastMissionCompleted != 0) {
				if (g_pilotData.campaignSequenceState.currentMissionIndex == 0) {
					g_pilotData.campaignSequenceState.cumulativeScore = g_pilotData.missionScore;
				} else {
					g_pilotData.campaignSequenceState.cumulativeScore += g_pilotData.missionScore;
				}
			}
			if (g_pilotData.campaignSequenceState.humanPlayerCount < 2) {
				if (g_pilotData.campaignSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spCampaigns[campaignId]
						  .attemptCount;
				}
				if (g_pilotData.campaignSequenceState.lastMissionCompleted != 0) {
					if ((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaigns[campaignId]
							.nextMissionIndex <
						(unsigned int)(g_pilotData.campaignSequenceState.currentMissionIndex + 1)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaigns[campaignId]
							.nextMissionIndex = g_pilotData.campaignSequenceState.currentMissionIndex + 1;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaigns[campaignId]
							.bestScore < g_pilotData.campaignSequenceState.cumulativeScore) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaigns[campaignId]
							.bestScore = g_pilotData.campaignSequenceState.cumulativeScore;
					}
					if (g_pilotData.campaignSequenceState.missionCount ==
						g_pilotData.campaignSequenceState.currentMissionIndex) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spCampaigns[campaignId]
							.isFinished = 1;
					}
				} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							   .spCampaigns[campaignId]
							   .bestScore <
						   g_pilotData.campaignSequenceState.cumulativeScore + g_pilotData.missionScore) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.spCampaigns[campaignId]
						.bestScore =
						g_pilotData.campaignSequenceState.cumulativeScore + g_pilotData.missionScore;
				}
			} else {
				if (g_pilotData.campaignSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpCampaigns[campaignId]
						  .attemptCount;
				}
				if (g_pilotData.campaignSequenceState.lastMissionCompleted != 0) {
					if ((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaigns[campaignId]
							.nextMissionIndex <
						(unsigned int)(g_pilotData.campaignSequenceState.currentMissionIndex + 1)) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaigns[campaignId]
							.nextMissionIndex = g_pilotData.campaignSequenceState.currentMissionIndex + 1;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaigns[campaignId]
							.bestScore < g_pilotData.campaignSequenceState.cumulativeScore) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaigns[campaignId]
							.bestScore = g_pilotData.campaignSequenceState.cumulativeScore;
					}
					if (g_pilotData.campaignSequenceState.missionCount ==
						g_pilotData.campaignSequenceState.currentMissionIndex) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpCampaigns[campaignId]
							.isFinished = 1;
					}
				} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							   .mpCampaigns[campaignId]
							   .bestScore <
						   g_pilotData.campaignSequenceState.cumulativeScore + g_pilotData.missionScore) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.mpCampaigns[campaignId]
						.bestScore =
						g_pilotData.campaignSequenceState.cumulativeScore + g_pilotData.missionScore;
				}
			}
		}

		if (g_pilotData.missionSequenceActive == 1 && statType == MISSION_STAT_MELEE) {
			int overallPlacement;
			int tournamentAward;
			int betterTeamCount;
			int tournamentId;
			int overallMargin;
			int localTeamTotalScore;
			unsigned int participatingTeamCount;
			int teamMissionScore;

			tournamentId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOURNAMENTS];
			participatingTeamCount = g_pilotData.meleeTournamentSequenceState.participatingTeamCount;
#ifdef XVT_MODERN
			/* Inactive standings keep the previous team's score; the first one would read an uninitialized
			 * local. */
			teamMissionScore = 0;
#endif
			for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
				unsigned int missionPlacement;
				unsigned int betterMissionTeamCount;

				if (g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx]
						.aiOpponentSourceTeamAndTypeFlag != -1) {
					teamMissionScore =
						g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] +
						g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][teamIdx];
					betterMissionTeamCount = 0;
					for (networkIdx = 0; networkIdx < TEAM_COUNT; ++networkIdx) {
						if (networkIdx != teamIdx &&
							g_pilotData.meleeTournamentSequenceState.teamStandings[networkIdx]
									.aiOpponentSourceTeamAndTypeFlag != -1 &&
							g_flightMissionState.runtime.teamScores[TEAM_SCORE_BONUS_TENTHS][networkIdx] +
									g_flightMissionState.runtime.teamScores[TEAM_SCORE_MISSION][networkIdx] >
								teamMissionScore) {
							++betterMissionTeamCount;
						}
					}
					missionPlacement = betterMissionTeamCount + 1;
				} else {
					missionPlacement = 0;
				}
				g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].totalScore +=
					teamMissionScore;
				if (missionPlacement == 1) {
					++g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].firstPlaceCount;
				} else if (missionPlacement == 2) {
					++g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].secondPlaceCount;
				} else if (missionPlacement == 3) {
					++g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].thirdPlaceCount;
				}
			}

			betterTeamCount = 0;
			overallMargin = 0;
			localTeamTotalScore = g_pilotData.meleeTournamentSequenceState
									  .teamStandings[(uint16_t)g_players[g_localPlayer].playerIff]
									  .totalScore;
			for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
				if (teamIdx != (uint16_t)g_players[g_localPlayer].playerIff &&
					g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx]
							.aiOpponentSourceTeamAndTypeFlag != -1) {
					if (localTeamTotalScore <
						g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].totalScore) {
						++betterTeamCount;
					} else if (localTeamTotalScore -
									   g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx]
										   .totalScore <
								   overallMargin ||
							   overallMargin == 0) {
						overallMargin =
							localTeamTotalScore -
							g_pilotData.meleeTournamentSequenceState.teamStandings[teamIdx].totalScore;
					}
				}
			}
			overallPlacement = betterTeamCount + 1;
			tournamentAward = 0;
			if (localTeamTotalScore > 5000 && overallPlacement < 4) {
				tournamentAward = g_placementAwardLevels[3 * participatingTeamCount - 4 + overallPlacement];
				if (tournamentAward != 0) {
					if (g_pilotData.meleeTournamentSequenceState.humanPlayerCount < 3) {
						if (g_flightMissionState.difficulty != GAME_DIFFICULTY_HARD) {
							tournamentAward += 2;
						} else {
							++tournamentAward;
						}
						if (tournamentAward > MAX_STORED_AWARD) {
							tournamentAward = MAX_STORED_AWARD;
						}
					} else if (g_pilotData.meleeTournamentSequenceState.humanPlayerCount < 5) {
						if (g_flightMissionState.difficulty != GAME_DIFFICULTY_HARD) {
							++tournamentAward;
						}
						if (tournamentAward > MAX_STORED_AWARD) {
							tournamentAward = MAX_STORED_AWARD;
						}
					}
					if (overallPlacement == FIRST_PLACE) {
						if (overallMargin > 20000) {
							tournamentAward -= 2;
						} else if (overallMargin > 10000) {
							--tournamentAward;
						}
						if (tournamentAward < 1) {
							tournamentAward = 1;
						}
					}
				}
			}
			if (tournamentAward == 0 && participatingTeamCount >= 4 &&
				participatingTeamCount == (unsigned int)overallPlacement) {
				tournamentAward = FAILED_AWARD;
			}

			if (g_pilotData.meleeTournamentSequenceState.humanPlayerCount == 1) {
				unsigned int oldAward;

				if (g_pilotData.meleeTournamentSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spTournaments[tournamentId]
						  .attemptCount;
				} else if (g_pilotData.meleeTournamentSequenceState.missionCount -
							   g_pilotData.meleeTournamentSequenceState.currentMissionIndex ==
						   1) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spTournaments[tournamentId]
						  .completedCount;
					if (overallPlacement == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spTournaments[tournamentId]
							  .firstPlaceCount;
					} else if (overallPlacement == 2) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spTournaments[tournamentId]
							  .secondPlaceCount;
					} else if (overallPlacement == 3) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spTournaments[tournamentId]
							  .thirdPlaceCount;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spTournaments[tournamentId]
							.bestScore < localTeamTotalScore) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spTournaments[tournamentId]
							.bestScore = localTeamTotalScore;
					}
					if ((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spTournaments[tournamentId]
								.bestPlacement > (unsigned int)overallPlacement ||
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spTournaments[tournamentId]
								.bestPlacement == 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spTournaments[tournamentId]
							.bestPlacement = (int)overallPlacement;
					}
					if (overallPlacement == FIRST_PLACE &&
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spTournaments[tournamentId]
								.bestMargin < (unsigned int)overallMargin &&
						overallMargin > 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spTournaments[tournamentId]
							.bestMargin = overallMargin;
					}
					if (tournamentAward != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .spTournaments[tournamentId]
									   .awardId;
						g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[1] =
							(int)tournamentAward;
						if (oldAward > (unsigned int)tournamentAward || oldAward == 0) {
							if (oldAward != 0 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
														 .tournamentTrophies[oldAward - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .tournamentTrophies[oldAward - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.spTournaments[tournamentId]
								.awardId = (int)tournamentAward;
							++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .tournamentTrophies[tournamentAward - 1];
						}
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .spTournaments[tournamentId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spTournaments[tournamentId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.tournamentTrophies[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .tournamentTrophies[FAILED_AWARD - 1];
						}
					}
				}
			} else {
				unsigned int oldAward;

				if (g_pilotData.meleeTournamentSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpTournaments[tournamentId]
						  .attemptCount;
				} else if (g_pilotData.meleeTournamentSequenceState.missionCount -
							   g_pilotData.meleeTournamentSequenceState.currentMissionIndex ==
						   1) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpTournaments[tournamentId]
						  .completedCount;
					if (overallPlacement == 1) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpTournaments[tournamentId]
							  .firstPlaceCount;
					} else if (overallPlacement == 2) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpTournaments[tournamentId]
							  .secondPlaceCount;
					} else if (overallPlacement == 3) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpTournaments[tournamentId]
							  .thirdPlaceCount;
					}
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpTournaments[tournamentId]
							.bestScore < localTeamTotalScore) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpTournaments[tournamentId]
							.bestScore = localTeamTotalScore;
					}
					if ((unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpTournaments[tournamentId]
								.bestPlacement > (unsigned int)overallPlacement ||
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpTournaments[tournamentId]
								.bestPlacement == 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpTournaments[tournamentId]
							.bestPlacement = (int)overallPlacement;
					}
					if (overallPlacement == FIRST_PLACE &&
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpTournaments[tournamentId]
								.bestMargin < (unsigned int)overallMargin &&
						overallMargin > 0) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpTournaments[tournamentId]
							.bestMargin = overallMargin;
					}
					if (tournamentAward != 0) {
						oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									   .mpTournaments[tournamentId]
									   .awardId;
						g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[1] =
							(int)tournamentAward;
						if (oldAward > (unsigned int)tournamentAward || oldAward == 0) {
							if (oldAward == FAILED_AWARD &&
								g_pilotData.factionStatistics[g_pilotData.currentFactionId]
										.tournamentTrophies[FAILED_AWARD - 1] != 0) {
								--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									  .tournamentTrophies[FAILED_AWARD - 1];
							}
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.mpTournaments[tournamentId]
								.awardId = (int)tournamentAward;
						}
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .tournamentTrophies[tournamentAward - 1];
					} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .mpTournaments[tournamentId]
								   .awardId == FAILED_AWARD) {
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpTournaments[tournamentId]
							.awardId = 0;
						if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.tournamentTrophies[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .tournamentTrophies[FAILED_AWARD - 1];
						}
					}
				}
			}
		}

		if (g_pilotData.missionSequenceActive == 1 && statType == MISSION_STAT_COMBAT) {
			BattleMissionResult missionResult;
			unsigned int resultIdx;
			unsigned int battleAward;
			int battleId;
			int imperialVictories;
			int rebelVictories;
			int overallWinner;
			int victoryMargin;
			int playerResult;

			battleId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_BATTLES];
			for (teamIdx = 0; teamIdx < TEAM_COUNT; ++teamIdx) {
				if (g_flightMissionState.runtime.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] == 1 &&
					g_flightMissionState.runtime.teamGoalStatus[teamIdx][TEAM_GOAL_SECONDARY] != 1) {
					break;
				}
			}
			if (teamIdx == TEAM_COUNT) {
				missionResult = BATTLE_MISSION_RESULT_DRAW;
			} else if (teamIdx == 0) {
				missionResult = BATTLE_MISSION_RESULT_IMPERIAL_VICTORY;
			} else if (teamIdx == 1) {
				missionResult = BATTLE_MISSION_RESULT_REBEL_VICTORY;
			} else {
				missionResult = BATTLE_MISSION_RESULT_DRAW;
			}
			imperialVictories = 0;
			rebelVictories = 0;
			g_pilotData.battleSequenceState
				.missionResults[g_pilotData.battleSequenceState.currentMissionIndex] = missionResult;
			for (resultIdx = 0; resultIdx <= g_pilotData.battleSequenceState.currentMissionIndex;
				 ++resultIdx) {
				if (g_pilotData.battleSequenceState.missionResults[resultIdx] ==
					BATTLE_MISSION_RESULT_IMPERIAL_VICTORY) {
					++imperialVictories;
				} else if (g_pilotData.battleSequenceState.missionResults[resultIdx] ==
						   BATTLE_MISSION_RESULT_REBEL_VICTORY) {
					++rebelVictories;
				}
			}
			if (imperialVictories == g_pilotData.battleSequenceState.victoriesNeeded) {
				overallWinner = 0;
				victoryMargin = imperialVictories - rebelVictories;
			} else if (rebelVictories == g_pilotData.battleSequenceState.victoriesNeeded) {
				overallWinner = 1;
				victoryMargin = rebelVictories - imperialVictories;
			} else {
				overallWinner = 2;
				victoryMargin = 0;
			}
			playerResult = 0;
			if (overallWinner != 2) {
				if ((overallWinner == 0 && g_players[g_localPlayer].playerIff == 0) ||
					(overallWinner == 1 && g_players[g_localPlayer].playerIff == 1)) {
					playerResult = 1;
				} else {
					playerResult = 2;
				}
			}
			battleAward = 0;
			if (playerResult != 0) {
				if (playerResult == 2 && victoryMargin >= 2) {
					battleAward = FAILED_AWARD;
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[3] =
						FAILED_AWARD;
				} else if (playerResult == 1) {
					int alliedPlayers;
					int enemyPlayers;
					int playerBalance;
					int awardPerformance;

					enemyPlayers = 0;
					alliedPlayers = 0;
					for (playerIdx = 0; playerIdx < PLAYER_COUNT; ++playerIdx) {
						if (g_players[playerIdx].network.directPlayId != 0) {
							if (g_players[g_localPlayer].playerIff == g_players[playerIdx].playerIff) {
								++alliedPlayers;
							} else {
								++enemyPlayers;
							}
						}
					}
					playerBalance = enemyPlayers - alliedPlayers;
					if (playerBalance > 0) {
						playerBalance *= 2;
					}
					if (g_pilotData.battleSequenceState.humanPlayerCount == 1 &&
						g_flightMissionState.difficulty == GAME_DIFFICULTY_HARD) {
						victoryMargin *= 2;
					}
					awardPerformance = victoryMargin + playerBalance;
					if (awardPerformance >= 5) {
						battleAward = 1;
					} else if (awardPerformance >= 4) {
						battleAward = 2;
					} else if (awardPerformance >= 3) {
						battleAward = 3;
					} else if (awardPerformance >= 2) {
						battleAward = 4;
					} else {
						battleAward = 5;
					}
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionAwards[3] =
						(int)battleAward;
				}
			}

			if (g_pilotData.battleSequenceState.currentMissionIndex == 0) {
				g_pilotData.battleSequenceState.cumulativeScore = g_pilotData.missionScore;
			} else {
				g_pilotData.battleSequenceState.cumulativeScore += g_pilotData.missionScore;
			}
			if (g_pilotData.battleSequenceState.humanPlayerCount < 2) {
				if (g_pilotData.battleSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spBattles[battleId]
						  .attemptCount;
				}
				playerResult = 0;
				if (overallWinner != 2) {
					if ((overallWinner == 0 && g_players[g_localPlayer].playerIff == 0) ||
						(overallWinner == 1 && g_players[g_localPlayer].playerIff == 1)) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spBattles[battleId]
							  .victoryCount;
						playerResult = 1;
					} else {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .spBattles[battleId]
							  .defeatCount;
						playerResult = 2;
					}
				}
				if (g_pilotData.battleSequenceState.currentMissionIndex == 10) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .spBattles[battleId]
						  .drawCount;
					playerResult = 0;
				}
				if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.spBattles[battleId]
						.bestScore < g_pilotData.battleSequenceState.cumulativeScore) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.spBattles[battleId]
						.bestScore = g_pilotData.battleSequenceState.cumulativeScore;
				}
				if (playerResult == 1 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
												 .spBattles[battleId]
												 .bestVictoryMargin < (unsigned int)victoryMargin) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.spBattles[battleId]
						.bestVictoryMargin = victoryMargin;
				}
				if (battleAward != 0) {
					unsigned int oldAward;

					oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .spBattles[battleId]
								   .awardId;
					if (oldAward > battleAward || oldAward == 0) {
						if (oldAward != 0 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
													 .battleMedallions[oldAward - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .battleMedallions[oldAward - 1];
						}
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.spBattles[battleId]
							.awardId = (int)battleAward;
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .battleMedallions[battleAward - 1];
					}
				} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							   .spBattles[battleId]
							   .awardId == FAILED_AWARD) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].spBattles[battleId].awardId =
						0;
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.battleMedallions[FAILED_AWARD - 1] != 0) {
						--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .battleMedallions[FAILED_AWARD - 1];
					}
				}
			} else {
				if (g_pilotData.battleSequenceState.currentMissionIndex == 0) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpBattles[battleId]
						  .attemptCount;
				}
				playerResult = 0;
				if (overallWinner != 2) {
					if ((overallWinner == 0 && g_players[g_localPlayer].playerIff == 0) ||
						(overallWinner == 1 && g_players[g_localPlayer].playerIff == 1)) {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpBattles[battleId]
							  .victoryCount;
						playerResult = 1;
					} else {
						++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .mpBattles[battleId]
							  .defeatCount;
						playerResult = 2;
					}
				}
				if (g_pilotData.battleSequenceState.currentMissionIndex == 10) {
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .mpBattles[battleId]
						  .drawCount;
					playerResult = 0;
				}
				if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.mpBattles[battleId]
						.bestScore < g_pilotData.battleSequenceState.cumulativeScore) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.mpBattles[battleId]
						.bestScore = g_pilotData.battleSequenceState.cumulativeScore;
				}
				if (playerResult == 1 && g_pilotData.factionStatistics[g_pilotData.currentFactionId]
												 .mpBattles[battleId]
												 .bestVictoryMargin < (unsigned int)victoryMargin) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						.mpBattles[battleId]
						.bestVictoryMargin = victoryMargin;
				}
				if (battleAward != 0) {
					unsigned int oldAward;

					oldAward = (unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								   .mpBattles[battleId]
								   .awardId;
					if (oldAward > battleAward || oldAward == 0) {
						if (oldAward == FAILED_AWARD &&
							g_pilotData.factionStatistics[g_pilotData.currentFactionId]
									.battleMedallions[FAILED_AWARD - 1] != 0) {
							--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								  .battleMedallions[FAILED_AWARD - 1];
						}
						g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.mpBattles[battleId]
							.awardId = (int)battleAward;
					}
					++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
						  .battleMedallions[battleAward - 1];
				} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							   .mpBattles[battleId]
							   .awardId == FAILED_AWARD) {
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].mpBattles[battleId].awardId =
						0;
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							.battleMedallions[FAILED_AWARD - 1] != 0) {
						--g_pilotData.factionStatistics[g_pilotData.currentFactionId]
							  .battleMedallions[FAILED_AWARD - 1];
					}
				}
			}
		}
	}
	return 0;
}

// FUNCTION: XVT 0x49C3C0
uint16_t FeDiskIo_ReadAllBytesOrFatal(const char* fileName, void* dst) {
	XvtFile* stream;
	uint8_t* output;
	uint16_t totalBytes;
	uint16_t bytesRead;
	uint16_t byteIndex;
	uint8_t buffer[512];

	File_OpenGlobalStream(fileName, "rb", 1, 0);
	stream = g_stream;
	if (stream == NULL) {
		FeDiskIo_FatalError(FILE_ERROR_STR_FILE_MISSING);
#ifdef XVT_MODERN
		return 0;
#endif
	}
	totalBytes = 0;
	output = dst;
	bytesRead = 512;
	while (bytesRead == 512) {
		bytesRead = (uint16_t)File_RawRead(buffer, 1, bytesRead, stream);
		byteIndex = 0;
		while (byteIndex < bytesRead) {
			*output = buffer[byteIndex];
			++output;
			++byteIndex;
		}
		totalBytes += bytesRead;
	}
	FeDiskIo_CloseGlobalStream(0);
	return totalBytes;
}

// FUNCTION: XVT 0x49C460
void FeDiskIo_InitGlobalBuffers(void) {
	enum {
		STRING_DATA_BUFFER_BYTES = 32000,
		TINY_FONT_BUFFER_BYTES = 34600,
		MICRO_FONT_BUFFER_BYTES = 20600,
		SMALL_FONT_BUFFER_BYTES = 34600,
		HUD_PANEL_SPRITE_BUFFER_BYTES = 120000,
		FLIGHT_ICON_FRAME_POINTER_CAPACITY = 2010,
		FLIGHT_ICON_FRAME_DATA_BYTES = 31060,
		MESSAGE_LOG_BUFFER_BYTES = 32000,
		RENDER_OBJECT_LIST_CAPACITY = 296,
		FLIGHT_LOG_CLEAR_COLOR = 0x40,
		FLIGHT_TEXT_LOADING_COLOR = 0xfa,
		FLIGHT_TEXT_WARNING_COLOR = 0x36,
		BASE_FLIGHT_SFX_FIRST_SOUND_ID = 4,
	};

	int16_t allocationFailed;
	const char* loadingMessage;
	const char* unsupportedResolutionMessage;
	const char* fallbackResolutionMessage;
	const char* pixelFormatMessage;
	int requestedRenderTargetWidth;
	int requestedBytesPerPixel;
	int activeBytesPerPixel;
	int requestedHardware3D;
	int activeHardware3D;
	char soundListPath[40];

	allocationFailed = 0;
	g_objectTableHandle = 0;
	g_mobileObjectPoolHandle = 0;
	g_flightFontSmallSw = NULL;
	g_mobileObjectCharDataHandle = 0;
	g_craftDataPoolHandle = 0;
	g_warheadGuidancePoolHandle = 0;
	g_stringDataHandle = Memory_AllocHandle(STRING_DATA_BUFFER_BYTES, 0);
	if (g_stringDataHandle == 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
#ifdef XVT_MODERN
		return;
#endif
	}

	g_flightTinyFontHandle = Memory_AllocHandle(TINY_FONT_BUFFER_BYTES, 0);
	if (g_flightTinyFontHandle == 0) {
		allocationFailed = 1;
	}
	g_flightMicroFontHandle = Memory_AllocHandle(MICRO_FONT_BUFFER_BYTES, 0);
	if (g_flightMicroFontHandle == 0) {
		allocationFailed = 1;
	}
	g_flightSmallFontHandle = Memory_AllocHandle(SMALL_FONT_BUFFER_BYTES, 0);
	if (g_flightSmallFontHandle == 0) {
		allocationFailed = 1;
	}
	if (allocationFailed != 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
#ifdef XVT_MODERN
		return;
#endif
	}

	g_flightLog1BufferHandle =
		Memory_AllocHandle(g_screenHeight * (unsigned int)g_flight16bppBytesPerPixel * g_screenWidth, 0);
	if (g_flightLog1BufferHandle == 0) {
		allocationFailed = 1;
	}
	g_flightAuxBufferHandle =
		Memory_AllocHandle(g_screenHeight * (unsigned int)g_flight16bppBytesPerPixel * g_screenWidth, 0);
	if (g_flightAuxBufferHandle == 0) {
		allocationFailed = 1;
	}
	g_flightOffscreenBufferHandle =
		Memory_AllocHandle(g_screenHeight * (unsigned int)g_flight16bppBytesPerPixel * g_screenWidth, 0);
	if (g_flightOffscreenBufferHandle == 0) {
		allocationFailed = 1;
	}
	g_hudPanelSpriteDataHandle = Memory_AllocHandle(HUD_PANEL_SPRITE_BUFFER_BYTES, 0);
	if (g_hudPanelSpriteDataHandle == 0) {
		allocationFailed = 1;
	}
	g_flightIconFramesHandle = Memory_AllocHandle(
		FLIGHT_ICON_FRAME_POINTER_CAPACITY * sizeof(g_flightIconFrames[0]) + FLIGHT_ICON_FRAME_DATA_BYTES, 0);
	if (g_flightIconFramesHandle == 0) {
		allocationFailed = 1;
	}
	g_messageLogHandle = Memory_AllocHandle(MESSAGE_LOG_BUFFER_BYTES, 0);
	if (g_messageLogHandle == 0) {
		allocationFailed = 1;
	}
	g_visibleObjectsHandle =
		Memory_AllocHandle(RENDER_OBJECT_LIST_CAPACITY * sizeof(g_renderObjectListEntries[0]), 0);
	if (g_visibleObjectsHandle == 0) {
		allocationFailed = 1;
	}

	StringTable_LoadGameStrings(1);
	g_flightFontSmallSw = Memory_LockHandle(g_flightTinyFontHandle);
	g_flightFontMicroSw = Memory_LockHandle(g_flightMicroFontHandle);
	g_flightFontMediumSw = Memory_LockHandle(g_flightSmallFontHandle);
	switch (g_flightResolutionMode) {
		case FLIGHT_RESOLUTION_320X240:
			FeDiskIo_ReadAllBytesOrFatal("MICRO32.FNT", g_flightFontMicroSw);
			FeDiskIo_ReadAllBytesOrFatal("MICRO48.FNT", g_flightFontSmallSw);
			g_flightIconResourcePath = g_flightMapIcons320x240ResourcePath;
			break;
		case FLIGHT_RESOLUTION_640X480:
			FeDiskIo_ReadAllBytesOrFatal("MICRO32.FNT", g_flightFontMicroSw);
			FeDiskIo_ReadAllBytesOrFatal("MICRO48.FNT", g_flightFontSmallSw);
			FeDiskIo_ReadAllBytesOrFatal("MICRO64.FNT", g_flightFontMediumSw);
			g_flightIconResourcePath = g_flightIcons640x480ResourcePath;
			break;
		case FLIGHT_RESOLUTION_480X360:
			FeDiskIo_ReadAllBytesOrFatal("MICRO32.FNT", g_flightFontMicroSw);
			FeDiskIo_ReadAllBytesOrFatal("MICRO48.FNT", g_flightFontSmallSw);
			FeDiskIo_ReadAllBytesOrFatal("MICRO64.FNT", g_flightFontMediumSw);
			g_flightIconResourcePath = g_flightMapIcons480x360ResourcePath;
			break;
	}
#ifdef XVT_MODERN
	XvtRenderAssets_RegisterFlightFonts();
#endif
	FlightText_SetFontTier(1);
	if (allocationFailed != 0) {
		FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
#ifdef XVT_MODERN
		return;
#endif
	}

	g_renderObjectListEntries = Memory_LockHandle(g_visibleObjectsHandle);
	g_flightLog1Buffer = Memory_LockHandle(g_flightLog1BufferHandle);
	memset(g_flightLog1Buffer, FLIGHT_LOG_CLEAR_COLOR,
		   g_screenHeight * (unsigned int)g_flight16bppBytesPerPixel * g_screenWidth);
	g_flightOffscreenBuffer = Memory_LockHandle(g_flightOffscreenBufferHandle);
	FlightSw_SetRotatedSpriteDestBuffer(g_flightLog1Buffer);
	g_flightAuxBuffer = Memory_LockHandle(g_flightAuxBufferHandle);
	g_flightAuxBufferMirror = g_flightAuxBuffer;

	FlightText_SetClipRect(0, 0, (int16_t)g_screenWidth, (int16_t)g_screenHeight);
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	FlightText_SetClipRect(0, 0, (int16_t)g_screenWidth, (int16_t)g_screenHeight);
	g_flightTextBgColor = 0;
	g_flightTextColorIndex = FLIGHT_TEXT_LOADING_COLOR;
	g_flightTextShadowColor = 0;
	g_flightTextShadowEnabled = 0;
	FlightText_SetCursor(0, (g_screenHeight >> 1) - 3 * g_flightFontLineHeight);

	switch (g_pilotData.missionDirectoryId) {
		case MISSION_DIRECTORY_TRAINING_EXERCISES:
			if (g_pilotData.missionSequenceActive == 1) {
				loadingMessage = g_strDiskIoMessages[DISK_IO_STR_ENTERING_CAMPAIGN];
			} else {
				loadingMessage = g_strDiskIoMessages[DISK_IO_STR_ENTERING_TRAINING];
			}
			break;
		case MISSION_DIRECTORY_MELEES:
		case MISSION_DIRECTORY_TOURNAMENTS:
			loadingMessage = g_strDiskIoMessages[DISK_IO_STR_ENTERING_MELEE];
			break;
		case MISSION_DIRECTORY_CAMPAIGNS:
			loadingMessage = g_strDiskIoMessages[DISK_IO_STR_ENTERING_CAMPAIGN];
			break;
		default:
			loadingMessage = g_strDiskIoMessages[DISK_IO_STR_ENTERING_COMBAT];
			break;
	}
#ifdef XVT_MODERN
	XvtCockpitMessages_BeginLoadingText();
#endif
	FlightText_DrawStringCentered(loadingMessage);

	requestedRenderTargetWidth = g_renderTargetWidth;
	if (requestedRenderTargetWidth != width) {
		g_flightTextColorIndex = FLIGHT_TEXT_WARNING_COLOR;
		FlightText_SetCursor(0, (g_screenHeight >> 1) + 3 * g_flightFontLineHeight);
		switch (g_renderTargetWidth) {
			case 320:
				unsupportedResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_320_NOT_SUPPORTED];
				break;
			case 512:
				unsupportedResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_512_NOT_SUPPORTED];
				break;
			case 640:
				unsupportedResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_640_NOT_SUPPORTED];
				break;
			default:
				unsupportedResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_NOT_SUPPORTED];
				break;
		}
		FlightText_DrawStringCentered(unsupportedResolutionMessage);
		FlightText_SetCursor(0, (g_screenHeight >> 1) + 4 * g_flightFontLineHeight + 1);
		switch (width) {
			case 320:
				fallbackResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_320_USED_INSTEAD];
				break;
			case 512:
				fallbackResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_512_USED_INSTEAD];
				break;
			case 640:
				fallbackResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_RES_640_USED_INSTEAD];
				break;
			default:
				fallbackResolutionMessage = g_strDiskIoMessages[DISK_IO_STR_NEXT_RES_USED_INSTEAD];
				break;
		}
		FlightText_DrawStringCentered(fallbackResolutionMessage);
	}

	requestedBytesPerPixel = g_unusedFlightDisplayBytesPerPixelMirror;
	activeBytesPerPixel = g_flight16bppBytesPerPixel;
	if (requestedBytesPerPixel != activeBytesPerPixel) {
		g_flightTextColorIndex = FLIGHT_TEXT_WARNING_COLOR;
		FlightText_SetCursor(0, (g_screenHeight >> 1) + 6 * g_flightFontLineHeight);
		if (g_flight16bppBytesPerPixel == 2) {
			pixelFormatMessage = g_strDiskIoMessages[DISK_IO_STR_USING_16BPP];
		} else {
			pixelFormatMessage = g_strDiskIoMessages[DISK_IO_STR_USING_8BPP];
		}
		FlightText_DrawStringCentered(pixelFormatMessage);
	}
	requestedHardware3D = g_unusedFlightDisplayHardware3DMirror;
	activeHardware3D = g_useHardware3D;
	if (requestedHardware3D != activeHardware3D) {
		g_flightTextColorIndex = FLIGHT_TEXT_WARNING_COLOR;
		FlightText_SetCursor(0, (g_screenHeight >> 1) + 8 * g_flightFontLineHeight);
		FlightText_DrawStringCentered(g_strDiskIoMessages[DISK_IO_STR_HARDWARE_3D_NOT_SUPPORTED]);
	}
#ifdef XVT_MODERN
	XvtCockpitMessages_EndLoadingText();
#endif

	g_hudCockpitResourcesLoaded = 0;
	g_flightIconFrames = Memory_LockHandle(g_flightIconFramesHandle);
	g_flightIconFrameCount = FlightIcon_LoadFrames(
		(char*)g_flightIconResourcePath,
		(uint8_t*)g_flightIconFrames + FLIGHT_ICON_FRAME_POINTER_CAPACITY * sizeof(g_flightIconFrames[0]),
		g_flightIconFrames);
	if (g_flightConfSfxEnabled != 0) {
		FlightSurface_Unlock();
		fsfx_ResetFlightSfxState();
		strcpy(soundListPath, "wave\\");
		strcat(soundListPath, "SFXBLAST.LST");
		fsfx_LoadSfxList(soundListPath, BASE_FLIGHT_SFX_FIRST_SOUND_ID);
		FlightSurface_Lock();
	}
}

// FUNCTION: XVT 0x49CBA0
void FeDiskIo_UnlockGlobalBuffers(void) {
	uint16_t craftDataPoolHandle;

	if (g_objectTableHandle != 0) {
		Memory_UnlockHandle(g_objectTableHandle);
	}
	if (g_mobileObjectPoolHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectPoolHandle);
	}
	craftDataPoolHandle = g_craftDataPoolHandle;
	if (g_mobileObjectCharDataHandle != 0) {
		Memory_UnlockHandle(g_craftDataPoolHandle);
		craftDataPoolHandle = g_craftDataPoolHandle;
	}
	if (craftDataPoolHandle != 0) {
		Memory_UnlockHandle(craftDataPoolHandle);
	}
	if (g_warheadGuidancePoolHandle != 0) {
		Memory_UnlockHandle(g_warheadGuidancePoolHandle);
	}
	Memory_UnlockHandle(g_stringDataHandle);
	Memory_UnlockHandle(g_visibleObjectsHandle);
	Memory_UnlockHandle(g_flightTinyFontHandle);
	Memory_UnlockHandle(g_flightMicroFontHandle);
	Memory_UnlockHandle(g_flightSmallFontHandle);
	Memory_UnlockHandle(g_flightLog1BufferHandle);
	Memory_UnlockHandle(g_flightAuxBufferHandle);
	Memory_UnlockHandle(g_flightOffscreenBufferHandle);
}

// FUNCTION: XVT 0x49CC90
void FeDiskIo_LockGlobalBuffers(void) {
	if (g_mobileObjectCharDataHandle != 0) {
		g_mobileObjectCharDataPool = Memory_LockHandle(g_mobileObjectCharDataHandle);
	}
	if (g_craftDataPoolHandle != 0) {
		g_craftDataPoolBase = Memory_LockHandle(g_craftDataPoolHandle);
	}
	if (g_warheadGuidancePoolHandle != 0) {
		g_projectileGuidanceStates = Memory_LockHandle(g_warheadGuidancePoolHandle);
	}
	if (g_mobileObjectPoolHandle != 0) {
		g_mobileObjectPoolBase = Memory_LockHandle(g_mobileObjectPoolHandle);
	}
	if (g_objectTableHandle != 0) {
		g_objectTable = Memory_LockHandle(g_objectTableHandle);
		Object_RelinkMobileObjectPointers();
	}

	StringTable_LoadGameStrings(0);
	g_renderObjectListEntries = Memory_LockHandle(g_visibleObjectsHandle);
	g_flightFontSmallSw = Memory_LockHandle(g_flightTinyFontHandle);
	g_flightFontMicroSw = Memory_LockHandle(g_flightMicroFontHandle);
	g_flightFontMediumSw = Memory_LockHandle(g_flightSmallFontHandle);
	if (g_flightFontTier == 1) {
		g_flightFontGlyphTableSw = g_flightFontSmallSw;
	} else if (g_flightFontTier == 2) {
		g_flightFontGlyphTableSw = g_flightFontMicroSw;
	} else if (g_flightFontTier == 0) {
		g_flightFontGlyphTableSw = g_flightFontMediumSw;
	}

	g_flightLog1Buffer = Memory_LockHandle(g_flightLog1BufferHandle);
	g_flightOffscreenBuffer = Memory_LockHandle(g_flightOffscreenBufferHandle);
	FlightSw_SetRotatedSpriteDestBuffer(g_flightLog1Buffer);
	g_flightAuxBuffer = Memory_LockHandle(g_flightAuxBufferHandle);
	g_flightAuxBufferMirror = g_flightAuxBuffer;
}

// FUNCTION: XVT 0x49CDF0
void FeDiskIo_FreeModelResources(void) {
	int cockpitResourceIndex;
	int modelType;
	int previousModelType;
	uint16_t textureHandle;

#ifdef XVT_MODERN
	XvtRenderAssets_ClearMission();
#endif
	FeDiskIo_UnlockGlobalBuffers();
	if (g_objectTableHandle != 0) {
		Memory_FreeHandle(g_objectTableHandle);
		g_objectTableHandle = 0;
	}
	if (g_mobileObjectPoolHandle != 0) {
		Memory_FreeHandle(g_mobileObjectPoolHandle);
		g_mobileObjectPoolHandle = 0;
	}
	if (g_mobileObjectCharDataHandle != 0) {
		Memory_FreeHandle(g_mobileObjectCharDataHandle);
		g_mobileObjectCharDataHandle = 0;
	}
	if (g_craftDataPoolHandle != 0) {
		Memory_FreeHandle(g_craftDataPoolHandle);
		g_craftDataPoolHandle = 0;
	}
	if (g_warheadGuidancePoolHandle != 0) {
		Memory_FreeHandle(g_warheadGuidancePoolHandle);
		g_warheadGuidancePoolHandle = 0;
	}

	fsfx_UnloadAllEffects_Thunk();
#ifdef XVT_MODERN
	if (g_stringDataHandle)
#endif
		Memory_FreeHandle(g_stringDataHandle);
#ifdef XVT_MODERN
	if (g_visibleObjectsHandle)
#endif
		Memory_FreeHandle(g_visibleObjectsHandle);
#ifdef XVT_MODERN
	if (g_flightTinyFontHandle)
#endif
		Memory_FreeHandle(g_flightTinyFontHandle);
#ifdef XVT_MODERN
	if (g_flightMicroFontHandle)
#endif
		Memory_FreeHandle(g_flightMicroFontHandle);
#ifdef XVT_MODERN
	if (g_flightSmallFontHandle)
#endif
		Memory_FreeHandle(g_flightSmallFontHandle);
#ifdef XVT_MODERN
	if (g_flightLog1BufferHandle)
#endif
		Memory_FreeHandle(g_flightLog1BufferHandle);
#ifdef XVT_MODERN
	if (g_flightAuxBufferHandle)
#endif
		Memory_FreeHandle(g_flightAuxBufferHandle);
#ifdef XVT_MODERN
	if (g_flightOffscreenBufferHandle)
#endif
		Memory_FreeHandle(g_flightOffscreenBufferHandle);
#ifdef XVT_MODERN
	if (g_hudPanelSpriteDataHandle)
#endif
		Memory_FreeHandle(g_hudPanelSpriteDataHandle);
#ifdef XVT_MODERN
	if (g_flightIconFramesHandle)
#endif
		Memory_FreeHandle(g_flightIconFramesHandle);
#ifdef XVT_MODERN
	if (g_messageLogHandle)
#endif
		Memory_FreeHandle(g_messageLogHandle);
#ifdef XVT_MODERN
	g_stringDataHandle = 0;
	g_visibleObjectsHandle = 0;
	g_flightTinyFontHandle = 0;
	g_flightMicroFontHandle = 0;
	g_flightSmallFontHandle = 0;
	g_flightLog1BufferHandle = 0;
	g_flightAuxBufferHandle = 0;
	g_flightOffscreenBufferHandle = 0;
	g_hudPanelSpriteDataHandle = 0;
	g_flightIconFramesHandle = 0;
	g_messageLogHandle = 0;
#endif

	for (cockpitResourceIndex = 0; cockpitResourceIndex < 28; ++cockpitResourceIndex) {
		if (g_hudCockpitResources[cockpitResourceIndex].memoryHandle != 0) {
			Memory_FreeHandle((uint16_t)g_hudCockpitResources[cockpitResourceIndex].memoryHandle);
			g_hudCockpitResources[cockpitResourceIndex].memoryHandle = 0;
		}
	}

	for (modelType = 0; modelType < 201; ++modelType) {
		textureHandle = g_modelTypeTable[modelType].curTexLevel;
		if (textureHandle != 0) {
			for (previousModelType = 0; previousModelType < modelType; ++previousModelType) {
				if (g_modelTypeTable[previousModelType].curTexLevel == textureHandle) {
					break;
				}
			}
			if (previousModelType >= modelType) {
				Memory_FreeHandle(textureHandle);
			}
		}
	}

	memset(g_loadedModels, 0, sizeof(g_loadedModels));
	for (modelType = 0; modelType < 201; ++modelType) {
		g_modelTypeTable[modelType].curTexLevel = 0;
	}
	RenderScene_FreeBuffers();
	Mission_FreeOverrideStringHandles();
}

// FUNCTION: XVT 0x49D010
void FeDiskIo_LoadResources(void) {
	enum {
		MODEL_RECORD_HAS_RESOURCE = 0x02,
		MODEL_ASSET_OPT = 0x01,
		MODEL_ASSET_TEX_LEVEL = 0x02,
		MODEL_ASSET_ACTIVE_MASK = 0x18,
		MODEL_ASSET_PROVING_GROUNDS_ONLY = 0x40,
	};

	uint16_t modelType;
	uint16_t specListIndex;
	uint16_t listEntryIndex;
	uint16_t resourceHandle;
	uint8_t resourceAssetFlags;
	XvtFile* listStream;
	uint8_t resourceNeeded;
	int lineEndIndex;
	unsigned int resourceDataSize;
	int specListGroup;
	unsigned int paletteEntryCount;
	unsigned int* textureData;
	XvtFile* textureStream;
	char listPath[60];
	char resourceName[256];

	for (modelType = 0; modelType < sizeof(g_loadedModels) / sizeof(g_loadedModels[0]); ++modelType) {
		g_loadedModels[modelType] = 0;
		g_modelTypeTable[modelType].curTexLevel = 0;
	}
	FeDiskIo_UnlockGlobalBuffers();
	g_sceneEdgeFlagsCapacity = 0;
	g_vertexRemapCapacity = 0;

	for (specListIndex = 0; specListIndex < sizeof(g_specListPrefixes) / sizeof(g_specListPrefixes[0]);
		 ++specListIndex) {
		strcpy(listPath, "ivfiles\\");
		specListGroup = specListIndex;
		strcat(listPath, g_specListPrefixes[specListIndex]);
		if (g_flightResolutionMode == FLIGHT_RESOLUTION_640X480) {
			strcat(listPath, "640");
		} else if (g_flightResolutionMode == FLIGHT_RESOLUTION_480X360) {
			strcat(listPath, "640");
		} else {
			strcat(listPath, "320");
		}
		strcat(listPath, ".LST");
		File_OpenGlobalStream(listPath, "rb", 1, 0);
		listStream = (XvtFile*)g_stream;
		listEntryIndex = 0;

		while (File_Gets(resourceName, sizeof(resourceName), listStream) != NULL) {
#ifdef XVT_MODERN
			for (lineEndIndex = 0; resourceName[lineEndIndex] != '\0' && resourceName[lineEndIndex] != '\n';
				 ++lineEndIndex) {
#else
			for (lineEndIndex = 0; resourceName[lineEndIndex] != '\n'; ++lineEndIndex) {
#endif
				if (resourceName[lineEndIndex] == '\r') {
					break;
				}
			}
			resourceName[lineEndIndex] = '\0';
			if (resourceName[0] == '\0') {
				continue;
			}

			++listEntryIndex;
			resourceNeeded = 0;
			resourceAssetFlags = 0;
			for (modelType = 0; modelType < sizeof(g_modelTypeTable) / sizeof(g_modelTypeTable[0]);
				 ++modelType) {
				if ((g_modelTypeTable[modelType].recordFlags & MODEL_RECORD_HAS_RESOURCE) != 0 &&
					g_modelTypeTable[modelType].textureGroup == specListGroup &&
					g_modelTypeTable[modelType].frameCount == listEntryIndex - 1) {
					uint8_t assetFlags = g_modelTypeTable[modelType].assetFlags;

					if ((assetFlags & MODEL_ASSET_ACTIVE_MASK) != 0 &&
						((assetFlags & MODEL_ASSET_PROVING_GROUNDS_ONLY) == 0 ||
						 g_flightMissionState.provingGroundsModeActive != 0)) {
						resourceNeeded = 1;
						resourceAssetFlags = assetFlags;
					}
				}
			}
			if (resourceNeeded == 0) {
				continue;
			}

			if ((resourceAssetFlags & MODEL_ASSET_OPT) != 0) {
				resourceHandle = OptModel_LoadHandle(resourceName);
				Memory_LockHandle(resourceHandle);
			} else if ((resourceAssetFlags & MODEL_ASSET_TEX_LEVEL) != 0) {
				File_OpenGlobalStream(resourceName, "rb", 1, 0);
				textureStream = (XvtFile*)g_stream;
				FeDiskIo_ReadWithRetryPrompt(&resourceDataSize, sizeof(resourceDataSize), 1, textureStream);
				FeDiskIo_ReadWithRetryPrompt(&paletteEntryCount, sizeof(paletteEntryCount), 1, textureStream);
				resourceHandle =
					Memory_AllocHandle(resourceDataSize + g_flight16bppBytesPerPixel * paletteEntryCount, 0);
				if (resourceHandle == 0) {
					FeDiskIo_FatalError(FILE_ERROR_STR_NOT_ENOUGH_MEMORY);
#ifdef XVT_MODERN
					File_Close(textureStream);
					File_Close(listStream);
					g_stream = NULL;
					return;
#endif
				}
				textureData = (unsigned int*)Memory_LockHandle(resourceHandle);
				textureData[0] = resourceDataSize;
				textureData[1] = paletteEntryCount;
				FeDiskIo_ReadWithRetryPrompt(textureData + 2, resourceDataSize - 2 * sizeof(textureData[0]),
											 1, textureStream);
				g_stream = textureStream;
				FeDiskIo_CloseGlobalStream(0);
#ifdef XVT_MODERN
				XvtRenderAssets_RegisterTexture(resourceHandle, resourceName);
#endif
				if (g_flight16bppBytesPerPixel == 2) {
					TexLevel_Convert24BppPalettesTo16Bpp(textureData);
				} else {
					TexLevel_Convert24BppPalettesTo8Bpp(textureData);
				}
			}

			for (modelType = 0; modelType < sizeof(g_modelTypeTable) / sizeof(g_modelTypeTable[0]);
				 ++modelType) {
				if ((g_modelTypeTable[modelType].recordFlags & MODEL_RECORD_HAS_RESOURCE) != 0 &&
					g_modelTypeTable[modelType].textureGroup == specListGroup &&
					g_modelTypeTable[modelType].frameCount == listEntryIndex - 1) {
					uint8_t assetFlags = g_modelTypeTable[modelType].assetFlags;

					if ((assetFlags & MODEL_ASSET_ACTIVE_MASK) != 0 &&
						((assetFlags & MODEL_ASSET_PROVING_GROUNDS_ONLY) == 0 ||
						 g_flightMissionState.provingGroundsModeActive != 0)) {
						g_modelTypeTable[modelType].curTexLevel = resourceHandle;
						g_loadedModels[modelType] = resourceHandle;
#ifdef XVT_MODERN
						XvtRenderAssets_BindType(modelType, resourceHandle);
#endif
						if ((resourceAssetFlags & MODEL_ASSET_OPT) != 0) {
							FeDiskIo_BuildModelDef(g_modelTypeTable[modelType].modelIndex, modelType);
						}
					}
				}
			}
			Memory_UnlockHandle(resourceHandle);
		}

		g_stream = listStream;
		FeDiskIo_CloseGlobalStream(0);
	}

	RenderScene_AllocateBuffers();
	FeDiskIo_LockGlobalBuffers();
}

// FUNCTION: XVT 0x49D440
unsigned int FeDiskIo_InitResources(void) {
	enum {
		MISSION_EXTENSION_LENGTH = 3,
		FALLBACK_FILE_NAME_CAPACITY = 256,
		GENERATED_PALETTE_START = 64,
		GENERATED_PALETTE_COLOR_COUNT = 192,
		QUANTIZER_TREE_DEPTH = 8,
		PALETTE_COLOR_COUNT = 256,
		PALETTE_BYTE_COUNT = sizeof(g_swPalette),
	};

	unsigned int result;

	g_generateMissionPalette &= g_paletteGenerationEnabled;
	if (g_flight16bppBytesPerPixel == 1) {
		unsigned int extensionOffset;
		uint8_t savedExtension0;
		uint8_t savedExtension1;
		uint8_t savedExtension2;
		char fallbackFileName[FALLBACK_FILE_NAME_CAPACITY];

		g_activeRgb565ToPaletteIndexLut = g_rgb565ToPaletteIndexLut;
		extensionOffset = strlen(g_currentMissionFile);
		savedExtension0 = g_currentMissionFile[extensionOffset - MISSION_EXTENSION_LENGTH];
		savedExtension1 = g_currentMissionFile[extensionOffset - MISSION_EXTENSION_LENGTH + 1];
		savedExtension2 = g_currentMissionFile[extensionOffset - 1];
		extensionOffset -= MISSION_EXTENSION_LENGTH;
		g_currentMissionFile[extensionOffset] = 'i';
		g_currentMissionFile[extensionOffset + 1] = 'n';
		g_currentMissionFile[extensionOffset + 2] = 'v';

		if (File_OpenGlobalStream(g_currentMissionFile, "rb", 0, 0) == 0) {
			strcpy(fallbackFileName, "newpal.inv");
			if (File_OpenGlobalStream(fallbackFileName, "rb", 0, 0) == 0) {
				Color_BuildRgb565ToPaletteIndexLut(g_activeRgb565ToPaletteIndexLut, GENERATED_PALETTE_START,
												   PALETTE_COLOR_COUNT);
				if (File_OpenGlobalStream(g_currentMissionFile, "wb", 0, 1) != 0) {
					File_RawWrite(g_activeRgb565ToPaletteIndexLut, PALETTE_COLOR_COUNT, PALETTE_COLOR_COUNT,
								  g_stream);
					FeDiskIo_CloseGlobalStream(1);
				}
			} else {
				FeDiskIo_CloseGlobalStream(0);
				FeDiskIo_ReadAllBytesOrFatal(fallbackFileName, g_activeRgb565ToPaletteIndexLut);
			}
		} else {
			FeDiskIo_CloseGlobalStream(0);
			FeDiskIo_ReadAllBytesOrFatal(g_currentMissionFile, g_activeRgb565ToPaletteIndexLut);
		}

		g_currentMissionFile[extensionOffset] = savedExtension0;
		g_currentMissionFile[extensionOffset + 1] = savedExtension1;
		g_currentMissionFile[extensionOffset + 2] = savedExtension2;
	}

	if (g_generateMissionPalette != 0 && g_flight16bppBytesPerPixel == 1) {
		ImageQuantizer_BeginPaletteCollection(GENERATED_PALETTE_COLOR_COUNT, QUANTIZER_TREE_DEPTH);
	}
	g_loadingModel = 1;
	FeDiskIo_LoadResources();
	g_loadingModel = 0;
	ModelMesh_BuildObjectTypeMeshCache();

	{
		RgbTriplet targetRgb;

		if (g_generateMissionPalette != 0 && g_flight16bppBytesPerPixel == 1) {
			unsigned int extensionOffset;
			int paletteOffset;
			uint8_t savedExtension0;
			uint8_t savedExtension1;
			uint8_t savedExtension2;
			uint8_t temporaryComponent;

			ImageQuantizer_ExportPalette6BitAndDestroy(GENERATED_PALETTE_COLOR_COUNT, QUANTIZER_TREE_DEPTH,
													   (uint8_t*)&g_swPalette[GENERATED_PALETTE_START]);
			extensionOffset = strlen(g_currentMissionFile);
			savedExtension0 = g_currentMissionFile[extensionOffset - MISSION_EXTENSION_LENGTH];
			savedExtension1 = g_currentMissionFile[extensionOffset - MISSION_EXTENSION_LENGTH + 1];
			savedExtension2 = g_currentMissionFile[extensionOffset - 1];
			extensionOffset -= MISSION_EXTENSION_LENGTH;
			g_currentMissionFile[extensionOffset] = 'p';
			g_currentMissionFile[extensionOffset + 1] = 'a';
			g_currentMissionFile[extensionOffset + 2] = 'l';
			if (File_OpenGlobalStream(g_currentMissionFile, "wb", 0, 1) != 0) {
				File_RawWrite(&g_swPalette[GENERATED_PALETTE_START],
							  sizeof(g_swPalette[GENERATED_PALETTE_START]) * GENERATED_PALETTE_COLOR_COUNT, 1,
							  g_stream);
				FeDiskIo_CloseGlobalStream(1);
			}

			g_activeRgb565ToPaletteIndexLut = g_rgb565ToPaletteIndexLut;
			Color_BuildRgb565ToPaletteIndexLut(g_activeRgb565ToPaletteIndexLut, GENERATED_PALETTE_START,
											   PALETTE_COLOR_COUNT);
			g_currentMissionFile[extensionOffset] = 'i';
			g_currentMissionFile[extensionOffset + 1] = 'n';
			g_currentMissionFile[extensionOffset + 2] = 'v';
			if (File_OpenGlobalStream(g_currentMissionFile, "wb", 0, 1) != 0) {
				File_RawWrite(g_activeRgb565ToPaletteIndexLut, PALETTE_COLOR_COUNT, PALETTE_COLOR_COUNT,
							  g_stream);
				FeDiskIo_CloseGlobalStream(1);
			}
			g_currentMissionFile[extensionOffset] = savedExtension0;
			g_currentMissionFile[extensionOffset + 1] = savedExtension1;
			g_currentMissionFile[extensionOffset + 2] = savedExtension2;

			FeDiskIo_ReadAllBytesOrFatal(g_flightPaletteResourceFileName, g_flightAuxBuffer);
			for (paletteOffset = 0; paletteOffset < PALETTE_BYTE_COUNT / 2;
				 paletteOffset += sizeof(RgbTriplet)) {
				temporaryComponent = g_flightAuxBuffer[paletteOffset] >> 2;
				g_flightAuxBuffer[paletteOffset] =
					g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset] >> 2;
				g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset] =
					temporaryComponent;
				temporaryComponent = g_flightAuxBuffer[paletteOffset + 1] >> 2;
				g_flightAuxBuffer[paletteOffset + 1] =
					g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset + 1] >> 2;
				g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset + 1] =
					temporaryComponent;
				temporaryComponent = g_flightAuxBuffer[paletteOffset + 2] >> 2;
				g_flightAuxBuffer[paletteOffset + 2] =
					g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset + 2] >> 2;
				g_flightAuxBuffer[PALETTE_BYTE_COUNT - sizeof(RgbTriplet) - paletteOffset + 2] =
					temporaryComponent;
			}
			g_flightSetPaletteRangeFn((RgbTriplet*)g_flightAuxBuffer, 0, PALETTE_COLOR_COUNT);
		}

		targetRgb.r = 0;
		targetRgb.g = 0;
		targetRgb.b = 2;
		result = Color_FindNearestRgbTripletIndex((const uint8_t*)&targetRgb, (const uint8_t*)g_swPalette, 0,
												  PALETTE_COLOR_COUNT);
	}
	g_flightColorEscapeBypassChar = (uint8_t)result;
	g_unusedFlightRenderColorByte = (uint8_t)result;
	return result;
}

// FUNCTION: XVT 0x49D860
void FeDiskIo_BuildModelDef(uint8_t modelDefIndex, ObjectTypeId objectType) {
	unsigned int sizeX;
	unsigned int sizeY;
	unsigned int sizeZ;
	uint16_t boundShift;
	uint16_t meshIndex;
	uint16_t hardpointIndex;
	uint8_t weaponSlotCount;
	uint8_t slotStart;
	int outY;
	int outZ;
	MeshComponentType meshType;
	int hardpointCount;
	int outX;
	int hardpointType;
	int currentMeshIndex;
	int meshCount;

	g_modelTypeTable[(uint8_t)objectType].maxBoundsExtent = ModelBounds_GetMaxExtent((uint8_t)objectType);
	g_modelTypeTable[(uint8_t)objectType].halfBoundsExtent =
		g_modelTypeTable[(uint8_t)objectType].maxBoundsExtent >> 1;
	if (modelDefIndex == 0xFF) {
		return;
	}

	sizeX = ModelBounds_GetSizeX((uint8_t)objectType);
	sizeY = ModelBounds_GetSizeY((uint8_t)objectType);
	sizeZ = ModelBounds_GetSizeZ((uint8_t)objectType);
	boundShift = 0;
	while (sizeX > 0x280 || sizeY > 0x280 || sizeZ > 0x280) {
		sizeX >>= 1;
		sizeY >>= 1;
		sizeZ >>= 1;
		++boundShift;
	}
	g_modelDefs[modelDefIndex].boundSizeShift = boundShift;
	g_modelDefs[modelDefIndex].boundSizeX = (int16_t)sizeX;
	g_modelDefs[modelDefIndex].boundSizeY = (int16_t)sizeY;
	g_modelDefs[modelDefIndex].boundSizeZ = (int16_t)sizeZ;
	if (g_modelDefs[modelDefIndex].dockToUp[0] == 0) {
		g_modelDefs[modelDefIndex].dockToUp[0] = (int16_t)ModelBounds_GetMinZ((uint8_t)objectType);
		g_modelDefs[modelDefIndex].dockToUp[1] = (int16_t)ModelBounds_GetMinZ((uint8_t)objectType);
	}
	if (g_modelDefs[modelDefIndex].dockFromUp[0] == 0) {
		g_modelDefs[modelDefIndex].dockFromUp[0] = (int16_t)ModelBounds_GetMaxZ((uint8_t)objectType);
		g_modelDefs[modelDefIndex].dockFromUp[1] = (int16_t)ModelBounds_GetMaxZ((uint8_t)objectType);
	}

	meshCount = ModelMesh_GetObjectTypeMeshCount((uint8_t)objectType);
	{
		for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
			currentMeshIndex = meshIndex;
			hardpointCount = ModelMesh_CountHardpoints((uint8_t)objectType, currentMeshIndex);

			if (hardpointCount == 0) {
				continue;
			}
			meshType = ModelMesh_GetObjectTypeMeshType((uint8_t)objectType, currentMeshIndex);
			for (hardpointIndex = 0; hardpointIndex < hardpointCount; ++hardpointIndex) {
				int16_t handled;

				handled = 0;
				ModelMesh_GetHardpoint((uint8_t)objectType, currentMeshIndex, hardpointIndex, &hardpointType,
									   &outX, &outY, &outZ);
				switch (hardpointType) {
					case 25:
						g_modelDefs[modelDefIndex].hangarPoints.inside.side = outX;
						g_modelDefs[modelDefIndex].hangarPoints.inside.up = outZ;
						g_modelDefs[modelDefIndex].hangarPoints.inside.forward = outY;
						handled = 1;
						break;
					case 26:
						g_modelDefs[modelDefIndex].hangarPoints.outside.side = outX;
						g_modelDefs[modelDefIndex].hangarPoints.outside.up = outZ;
						g_modelDefs[modelDefIndex].hangarPoints.outside.forward = outY;
						handled = 1;
						break;
					case 27:
						g_modelDefs[modelDefIndex].dockFromUp[1] = (int16_t)outZ;
						g_modelDefs[modelDefIndex].dockForward = (int16_t)outY;
						handled = 1;
						break;
					case 28:
						g_modelDefs[modelDefIndex].dockFromUp[0] = (int16_t)outZ;
						g_modelDefs[modelDefIndex].dockForward = (int16_t)outY;
						handled = 1;
						break;
					case 29:
						g_modelDefs[modelDefIndex].dockToUp[1] = (int16_t)outZ;
						g_modelDefs[modelDefIndex].dockForward = (int16_t)outY;
						handled = 1;
						break;
					case 30:
						g_modelDefs[modelDefIndex].dockToUp[0] = (int16_t)outZ;
						g_modelDefs[modelDefIndex].dockForward = (int16_t)outY;
						handled = 1;
						break;
					case 31:
						g_modelDefs[modelDefIndex].primaryHardpointZ = (int16_t)outZ;
						g_modelDefs[modelDefIndex].primaryHardpointY = (int16_t)outY;
						handled = 1;
						break;
					default:
						break;
				}
				if (handled != 0) {
					continue;
				}
				{
					uint8_t weaponCode = (uint8_t)(hardpointType - 120);
					uint16_t groupSlot;

					for (groupSlot = 0; groupSlot < 2; ++groupSlot) {
						if (g_modelDefs[modelDefIndex].laserGroupWeaponType[groupSlot] == weaponCode) {
							break;
						}
					}
					if (groupSlot < 2) {
						continue;
					}
					for (groupSlot = 0; groupSlot < 2; ++groupSlot) {
						if (g_modelDefs[modelDefIndex].warheadLauncherType[groupSlot] == weaponCode) {
							break;
						}
					}
					if (groupSlot < 2) {
						continue;
					}
					if (g_optHardpointWeaponGroupKindByType[hardpointType] == 1) {
						for (groupSlot = 0; groupSlot < 2; ++groupSlot) {
							if (g_modelDefs[modelDefIndex].laserGroupWeaponType[groupSlot] == 0) {
								break;
							}
						}
						if (groupSlot < 2) {
							g_modelDefs[modelDefIndex].laserGroupWeaponType[groupSlot] =
								(uint8_t)(hardpointType - 120);
							if (meshType == MESH_COMPONENT_04_LASR_TUR ||
								meshType == MESH_COMPONENT_21_LASR_TUR ||
								meshType == MESH_COMPONENT_05_LASR_GUN ||
								g_modelTypeTable[(uint8_t)objectType].genusId == CRAFT_GENUS_FREIGHTER ||
								g_modelTypeTable[(uint8_t)objectType].genusId == CRAFT_GENUS_PLATFORM ||
								g_modelTypeTable[(uint8_t)objectType].genusId == CRAFT_GENUS_STARSHIP) {
								g_modelDefs[modelDefIndex].laserGroupMountType[groupSlot] = 2;
							} else {
								g_modelDefs[modelDefIndex].laserGroupMountType[groupSlot] =
									(hardpointType == 5 || hardpointType == 16);
							}
						}
					} else if (g_optHardpointWeaponGroupKindByType[hardpointType] == 2) {
						for (groupSlot = 0; groupSlot < 2; ++groupSlot) {
							if (g_modelDefs[modelDefIndex].warheadLauncherType[groupSlot] == 0) {
								break;
							}
						}
						if (groupSlot < 2) {
							g_modelDefs[modelDefIndex].warheadLauncherType[groupSlot] =
								(uint8_t)(hardpointType - 120);
						}
					}
				}
			}
		}
	}

	/* Rebuild laser hardpoint slots in mesh order, preserving paired turret hardpoints. */
	weaponSlotCount = 0;
	{
		uint16_t groupIndex;
		uint8_t targetType;

		for (groupIndex = 0; groupIndex < 2; ++groupIndex) {
			slotStart = (uint8_t)weaponSlotCount;

			if (weaponSlotCount == 16) {
				g_modelDefs[modelDefIndex].laserGroupWeaponType[groupIndex] = 0;
				g_modelDefs[modelDefIndex].laserGroupMountType[groupIndex] = 0;
				continue;
			}
			targetType = (uint8_t)(g_modelDefs[modelDefIndex].laserGroupWeaponType[groupIndex] + 120);
			currentMeshIndex = targetType;
			for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
				uint16_t alternateSlot;

				hardpointCount = ModelMesh_CountHardpoints((uint8_t)objectType, meshIndex);
				if (hardpointCount == 0) {
					continue;
				}
				alternateSlot = 0xFF;
				meshType = ModelMesh_GetObjectTypeMeshType((uint8_t)objectType, meshIndex);
				for (hardpointIndex = 0; hardpointIndex < hardpointCount; ++hardpointIndex) {
					ModelMesh_GetHardpoint((uint8_t)objectType, meshIndex, hardpointIndex, &hardpointType,
										   &outX, &outY, &outZ);
					if (hardpointType != currentMeshIndex) {
						continue;
					}
					if (alternateSlot == 0xFF) {
						if ((uint8_t)objectType == 53) {
							outX >>= 1;
							outY >>= 1;
							outZ >>= 1;
						}
						g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].x = (int16_t)outX;
						g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].y = (int16_t)outY;
						g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].z = (int16_t)outZ;
						g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].meshIdx =
							(uint8_t)meshIndex;
						g_modelDefs[modelDefIndex]
							.weaponHardpoints[weaponSlotCount]
							.alternateMeshHardpointIdx = 0xFF;
						if (meshType == MESH_COMPONENT_04_LASR_TUR ||
							meshType == MESH_COMPONENT_21_LASR_TUR) {
							alternateSlot = weaponSlotCount;
						}
						++weaponSlotCount;
						if (weaponSlotCount == 16) {
							break;
						}
					} else {
						g_modelDefs[modelDefIndex].weaponHardpoints[alternateSlot].alternateMeshHardpointIdx =
							(uint8_t)ModelMesh_GetAlternateHardpointIndex((uint8_t)objectType, meshIndex,
																		  hardpointIndex);
						alternateSlot = 0xFF;
					}
				}
				if (weaponSlotCount == 16) {
					break;
				}
			}
			if (slotStart != weaponSlotCount) {
				g_modelDefs[modelDefIndex].laserGroupFirstSlot[groupIndex] = slotStart;
				g_modelDefs[modelDefIndex].laserGroupLastSlot[groupIndex] = (uint8_t)(weaponSlotCount - 1);
				g_modelDefs[modelDefIndex].laserGroupSlotCount[groupIndex] =
					(uint8_t)(weaponSlotCount - slotStart);
			}
		}

		/* Rebuild warhead launcher slots after the laser slots. */
		for (groupIndex = 0; groupIndex < 2; ++groupIndex) {
			slotStart = (uint8_t)weaponSlotCount;

			if (weaponSlotCount == 16) {
				g_modelDefs[modelDefIndex].warheadLauncherType[groupIndex] = 0;
				continue;
			}
			targetType = (uint8_t)(g_modelDefs[modelDefIndex].warheadLauncherType[groupIndex] + 120);
			currentMeshIndex = targetType;
			for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
				hardpointCount = ModelMesh_CountHardpoints((uint8_t)objectType, meshIndex);
				if (hardpointCount == 0) {
					continue;
				}
				ModelMesh_GetObjectTypeMeshType((uint8_t)objectType, meshIndex);
				for (hardpointIndex = 0; hardpointIndex < hardpointCount; ++hardpointIndex) {
					ModelMesh_GetHardpoint((uint8_t)objectType, meshIndex, hardpointIndex, &hardpointType,
										   &outX, &outY, &outZ);
					if (hardpointType != currentMeshIndex) {
						continue;
					}
					if ((uint8_t)objectType == 53) {
						outX >>= 1;
						outY >>= 1;
						outZ >>= 1;
					}
					g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].x = (int16_t)outX;
					g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].y = (int16_t)outY;
					g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].z = (int16_t)outZ;
					g_modelDefs[modelDefIndex].weaponHardpoints[weaponSlotCount].meshIdx = (uint8_t)meshIndex;
					++weaponSlotCount;
					if (weaponSlotCount == 16) {
						break;
					}
				}
				if (weaponSlotCount == 16) {
					break;
				}
			}
			if (slotStart != weaponSlotCount) {
				g_modelDefs[modelDefIndex].warheadLauncherFirstSlot[groupIndex] = slotStart;
				g_modelDefs[modelDefIndex].warheadLauncherLastSlot[groupIndex] =
					(uint8_t)(weaponSlotCount - 1);
				g_modelDefs[modelDefIndex].warheadLauncherSlotCount[groupIndex] =
					(uint8_t)(weaponSlotCount - slotStart);
			}
		}
	}
}

#ifndef XVT_MODERN
// FUNCTION: XVT 0x49E060
char FeDiskIo_ShowRetryFailPrompt(void) {
	int16_t savedCursorX;
	int16_t savedCursorY;
	int16_t savedClipLeft;
	int16_t savedClipTop;
	int16_t savedClipRight;
	int16_t savedClipBottom;
	int16_t savedWordWrap;
	int16_t savedReservedState;
	int16_t savedClearLineBg;
	uint8_t savedTextColor;
	uint8_t savedBgColor;
	uint8_t savedShadowColor;
	uint8_t savedShadowEnabled;
	uint8_t savedFontTier;
	char nextKey;
	int lineHeight;
	uint8_t* savedPixels;
	int savedLockCount;
	int remainingLocks;
	int currentLockCount;
	char str[256];

	savedCursorX = g_flightCursorX;
	savedCursorY = g_flightCursorY;
	savedClipLeft = g_flightClipLeft;
	savedClipTop = g_flightClipTop;
	savedClipRight = g_flightClipRight;
	savedClipBottom = g_flightClipBottom;
	savedWordWrap = g_flightWordWrapEnabled;
	savedReservedState = g_flightTextReservedState91079E;
	savedTextColor = g_flightTextColorIndex;
	savedClearLineBg = g_flightClearLineBgEnabled;
	savedBgColor = g_flightTextBgColor;
	savedShadowColor = g_flightTextShadowColor;
	savedShadowEnabled = g_flightTextShadowEnabled;
	savedFontTier = g_flightFontTier;

	FlightSurface_Lock();
	FlightText_SetFontTier(1);
	lineHeight = 4 * g_flightFontLineHeight;
	savedPixels =
		g_flightLog1Buffer + g_screenWidth * g_flight16bppBytesPerPixel * (g_screenHeight - lineHeight - 1);
	g_flightSaveScreenRectFn(savedPixels, 0, ((unsigned int)g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
							 (int16_t)g_screenWidth, lineHeight + 1);
	FlightText_SetClipRect((int16_t)((unsigned int)g_screenWidth >> 4),
						   (int16_t)(((unsigned int)g_screenHeight >> 1) - 2 * g_flightFontLineHeight),
						   (int16_t)(g_screenWidth - ((unsigned int)g_screenWidth >> 4)),
						   (int16_t)(((unsigned int)g_screenHeight >> 1) + 2 * g_flightFontLineHeight));
	g_flightTextBgColor = 0xf9;
	g_flightFillClipRectFn();
	FlightText_SetClipRect((int16_t)(((unsigned int)g_screenWidth >> 4) + 1),
						   (int16_t)(((unsigned int)g_screenHeight >> 1) - 2 * g_flightFontLineHeight + 1),
						   (int16_t)(g_screenWidth - ((unsigned int)g_screenWidth >> 4) - 1),
						   (int16_t)(((unsigned int)g_screenHeight >> 1) + 2 * g_flightFontLineHeight - 1));
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	g_flightTextColorIndex = 0xf9;
	g_flightTextShadowColor = 0;
	g_flightTextShadowEnabled = 0;
	FlightText_SetCursor(0, ((unsigned int)g_screenHeight >> 1) - g_flightFontLineHeight - 2);
	strcpy(str, g_fileName);
	strcat(str, ": ");
	strcat(str, g_strDiskIoMessages[DISK_IO_STR_RES_320_NOT_SUPPORTED]);
	FlightText_DrawStringCentered(str);
	FlightText_SetCursor(0, ((unsigned int)g_screenHeight >> 1) + 2);
	FlightText_DrawStringCentered(g_strDiskIoMessages[DISK_IO_STR_RES_512_NOT_SUPPORTED]);

	savedLockCount = FlightSurface_GetLockCount();
	remainingLocks = savedLockCount;
	while (remainingLocks > 0) {
		FlightSurface_Unlock();
		--remainingLocks;
	}
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	nextKey = FlightInput_GetNextKey();
	while (savedLockCount > 0) {
		FlightSurface_Lock();
		--savedLockCount;
	}

	g_flightRestoreScreenRectFn(savedPixels, 0,
								((unsigned int)g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
								(int16_t)g_screenWidth, 4 * g_flightFontLineHeight + 1);
	FlightText_SetFontTier(savedFontTier);
	g_flightCursorX = savedCursorX;
	g_flightCursorY = savedCursorY;
	g_flightClipLeft = savedClipLeft;
	g_flightClipTop = savedClipTop;
	g_flightClipRight = savedClipRight;
	g_flightClipBottom = savedClipBottom;
	g_flightWordWrapEnabled = savedWordWrap;
	g_flightTextReservedState91079E = savedReservedState;
	g_flightTextColorIndex = savedTextColor;
	g_flightClearLineBgEnabled = savedClearLineBg;
	g_flightTextBgColor = savedBgColor;
	g_flightTextShadowColor = savedShadowColor;
	g_flightTextShadowEnabled = savedShadowEnabled;
	FlightSurface_Unlock();

	currentLockCount = FlightSurface_GetLockCount();
	remainingLocks = currentLockCount;
	while (remainingLocks > 0) {
		FlightSurface_Unlock();
		--remainingLocks;
	}
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	while (currentLockCount > 0) {
		FlightSurface_Lock();
		--currentLockCount;
	}
	return nextKey;
}

// FUNCTION: XVT 0x49E420
int FeDiskIo_ShowFatalErrorMessageAndWaitKey(const char* message) {
	int16_t savedCursorX;
	int16_t savedCursorY;
	int16_t savedClipLeft;
	int16_t savedClipTop;
	int16_t savedClipRight;
	int16_t savedClipBottom;
	int16_t savedWordWrap;
	int16_t savedReservedState;
	int16_t savedClearLineBg;
	uint8_t savedTextColor;
	uint8_t savedBgColor;
	uint8_t savedShadowColor;
	uint8_t savedShadowEnabled;
	uint8_t savedFontTier;
	int savedLockCount;
	int remainingLocks;
	uint8_t savedDisplaySurfacesActive;
	int8_t nextKey;
	int currentLockCount;
	int lineHeight;
	char str[256];

	savedCursorX = g_flightCursorX;
	savedCursorY = g_flightCursorY;
	savedClipLeft = g_flightClipLeft;
	savedClipTop = g_flightClipTop;
	savedClipRight = g_flightClipRight;
	savedClipBottom = g_flightClipBottom;
	savedWordWrap = g_flightWordWrapEnabled;
	savedReservedState = g_flightTextReservedState91079E;
	savedTextColor = g_flightTextColorIndex;
	savedClearLineBg = g_flightClearLineBgEnabled;
	savedBgColor = g_flightTextBgColor;
	savedShadowColor = g_flightTextShadowColor;
	savedShadowEnabled = g_flightTextShadowEnabled;
	savedFontTier = g_flightFontTier;

	savedLockCount = FlightSurface_GetLockCount();
	if (savedLockCount > 0) {
		remainingLocks = savedLockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	savedDisplaySurfacesActive = g_flightDisplaySurfacesActive;
	g_flightDisplaySurfacesActive = 1;
	FlightSurface_Lock();
	FlightText_SetFontTier(1);
	FlightText_SetClipRect(g_screenWidth >> 4, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
						   g_screenWidth - (g_screenWidth >> 4),
						   (g_screenHeight >> 1) + 2 * g_flightFontLineHeight);
	g_flightTextBgColor = 0xF9;
	g_flightFillClipRectFn();
	FlightText_SetClipRect((g_screenWidth >> 4) + 1, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight + 1,
						   g_screenWidth - (g_screenWidth >> 4) - 1,
						   (g_screenHeight >> 1) + 2 * g_flightFontLineHeight - 1);
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	lineHeight = g_flightFontLineHeight;
	g_flightTextColorIndex = 0xF9;
	g_flightTextShadowColor = 0;
	g_flightTextShadowEnabled = 0;
	FlightText_SetCursor(0, (g_screenHeight >> 1) - lineHeight - 2);
	strcpy(str, message);
	FlightText_DrawStringCentered(str);
	FlightText_SetCursor(0, (g_screenHeight >> 1) + 2);
	FlightText_DrawStringCentered(g_strFileErrorMessages[FILE_ERROR_STR_PRESS_KEY_TO_EXIT]);
	FlightSurface_Unlock();
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	nextKey = FlightInput_GetNextKey();

	g_flightDisplaySurfacesActive = savedDisplaySurfacesActive;
	if (savedLockCount > 0) {
		do {
			FlightSurface_Lock();
			--savedLockCount;
		} while (savedLockCount != 0);
	}

	FlightText_SetFontTier(savedFontTier);
	g_flightCursorX = savedCursorX;
	g_flightCursorY = savedCursorY;
	g_flightClipLeft = savedClipLeft;
	g_flightClipTop = savedClipTop;
	g_flightClipRight = savedClipRight;
	g_flightClipBottom = savedClipBottom;
	g_flightWordWrapEnabled = savedWordWrap;
	g_flightTextReservedState91079E = savedReservedState;
	g_flightTextColorIndex = savedTextColor;
	g_flightClearLineBgEnabled = savedClearLineBg;
	g_flightTextBgColor = savedBgColor;
	g_flightTextShadowColor = savedShadowColor;
	g_flightTextShadowEnabled = savedShadowEnabled;

	currentLockCount = FlightSurface_GetLockCount();
	if (currentLockCount > 0) {
		remainingLocks = currentLockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	if (currentLockCount > 0) {
		do {
			FlightSurface_Lock();
			--currentLockCount;
		} while (currentLockCount != 0);
	}
	return nextKey;
}

#endif

// FUNCTION: XVT 0x49E720
int File_OpenGlobalStream(const char* fileName, const char* mode, int promptOnFail, int locationMode) {
#ifdef XVT_MODERN
	(void)locationMode;
	g_stream = File_Open(fileName, mode);
	XvtStorage_CaptureGlobalStream();
	snprintf(g_fileName, sizeof(g_fileName), "%s", XvtStorage_LastPath());
	if (!g_stream && promptOnFail)
		XvtStorage_Fatal("Cannot open required file", 1);
	return g_stream != NULL;
#else

	int attemptsRemaining;
	char key;

	strcpy(g_fileName, fileName);
	while (1) {
		attemptsRemaining = 4;
		if (locationMode != 2) {
			while (attemptsRemaining-- != 0) {
				g_stream = File_RawOpen(fileName, mode);
				if (g_stream != NULL) {
					return 1;
				}
			}

			attemptsRemaining = 4;
			strcpy(g_fileName, File_GetBaseGameInstallPath());
			strcat(g_fileName, "\\");
			strcat(g_fileName, fileName);
			while (attemptsRemaining-- != 0) {
				g_stream = File_RawOpen(g_fileName, mode);
				if (g_stream != NULL) {
					return 1;
				}
			}
		}

		if (locationMode != 1) {
			strcpy(g_fileName, "D:\\BalanceOfPower\\");
			attemptsRemaining = 4;
			g_fileName[0] = File_GetCdDriveLetter();
			strcat(g_fileName, fileName);
			while (attemptsRemaining-- != 0) {
				g_stream = File_RawOpen(g_fileName, mode);
				if (g_stream != NULL) {
					return 1;
				}
			}

			attemptsRemaining = 4;
			strcpy(g_fileName, "D:\\");
			g_fileName[0] = File_GetCdDriveLetter();
			strcat(g_fileName, fileName);
			while (attemptsRemaining-- != 0) {
				g_stream = File_RawOpen(g_fileName, mode);
				if (g_stream != NULL) {
					return 1;
				}
			}
		}

		if (promptOnFail == 0) {
			break;
		}
		while (1) {
			key = FeDiskIo_ShowRetryFailPrompt();
			if (key == 'R' || key == 'r') {
				break;
			}
			if (key == 'F' || key == 'f') {
				FeDiskIo_FatalError(FILE_ERROR_STR_FILE_MISSING);
				g_stream = NULL;
				return 0;
			}
		}
	}
	g_stream = NULL;
	return 0;

#endif
}

// FUNCTION: XVT 0x49E9C0
int16_t FeDiskIo_CloseGlobalStream(int16_t removeFileOnError) {
#ifdef XVT_MODERN
	int failed = XvtStorage_CloseGlobalStream(g_stream, removeFileOnError);
	g_stream = NULL;
	return (int16_t)failed;
#else

	int16_t closeError;

	closeError = 0;
	if ((((Msvc42FilePrefix*)g_stream)->flags & 0x20) != 0 || File_RawClose((XvtFile*)g_stream) == EOF) {
		closeError = 1;
	}

	if (removeFileOnError != 0 && closeError != 0) {
		File_Remove(g_fileName);
	}

	return closeError;

#endif
}

// FUNCTION: XVT 0x49EA10
size_t FeDiskIo_ReadWithRetryPrompt(void* dst, size_t elemSize, size_t elemCount, XvtFile* stream) {
#ifdef XVT_MODERN
	size_t count;
	count = File_RawRead(dst, elemSize, elemCount, stream);
	g_fileReadAbortFlag = count != elemCount;
	if (g_fileReadAbortFlag)
		XvtStorage_Fatal("Incomplete required file read", 1);
	return count;
#else

	size_t requestedCount;
	size_t readCount;
	uint8_t* output;
	int retriesRemaining;
	char key;

	requestedCount = elemCount;
	output = dst;
	retriesRemaining = 15;
	while (1) {
		--retriesRemaining;
		readCount = File_RawRead(output, elemSize, elemCount, stream);
		output += elemSize * readCount;
		elemCount -= readCount;
		if (elemCount == 0) {
			break;
		}
		if (retriesRemaining == 0) {
			while (elemCount != 0) {
				key = FeDiskIo_ShowRetryFailPrompt();
				if (key == 'R' || key == 'r') {
					break;
				}
				if (key == 'F' || key == 'f') {
					g_fileReadAbortFlag = 1;
					FeDiskIo_FatalError(FILE_ERROR_STR_FILE_MISSING);
					return 0;
				}
			}
			retriesRemaining = 5;
		}
	}
	g_fileReadAbortFlag = 0;
	return requestedCount;

#endif
}

// FUNCTION: XVT 0x49EB60
void FeDiskIo_FatalError(FileErrorStringId errorCode) {
#ifdef XVT_MODERN
	(void)errorCode;
	XvtStorage_Fatal("Required resource could not be loaded", 1);
#else

	char message[128];
	const char* errorMessage;
	uint16_t i;
	uint16_t j;

	if ((uint16_t)errorCode < 4) {
		if (g_flightFontSmallSw == NULL) {
			message[0] = 0;
		} else {
			errorMessage = g_strFileErrorMessages[(uint16_t)errorCode];
			FeDiskIo_ShowFatalErrorMessageAndWaitKey(errorMessage);
			for (i = 0; i < sizeof(message); ++i) {
				message[i] = errorMessage[i];
				if (message[i] == 0) {
					break;
				}
			}
			if (errorCode == FILE_ERROR_STR_FILE_MISSING) {
				j = 0;
				for (; i < sizeof(message); ++i) {
					message[i] = g_fileName[j++];
					if (message[i] == 0) {
						break;
					}
				}
				message[i++] = '\n';
				message[i] = 0;
			}
		}
	}
	File_PrintFatalMessageAndExit(message, -255 - (uint16_t)errorCode);

#endif
}

// FUNCTION: XVT 0x4ACE60
void File_PrintFatalMessageAndExit(const char* message, int exitCode) {
#ifdef XVT_MODERN
	XvtStorage_Fatal(message, exitCode);
#else

	fprintf(stderr, message);
	exit(exitCode);

#endif
}
