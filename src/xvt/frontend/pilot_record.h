#ifndef XVT_FRONTEND_PILOT_RECORD_H
#define XVT_FRONTEND_PILOT_RECORD_H

#include "xvt/frontend/frontend_mission_list.h"
#include "xvt/frontend/mission_setup.h"
#include "xvt/frontend/pilot.h"
#include "xvt/util/win32.h"
#include "xvt/xvt_typedefs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_pilotRecordPage;
extern POINT g_pilotRatingIconPos[25];
extern int g_campaignSingleplayerAwardCount;
extern int g_campaignMedalScrollOffset;
extern int g_campaignSingleplayerAwardFlags[];
extern int g_campaignMultiplayerAwardCount;
extern int g_campaignMultiplayerAwardFlags[];
extern int g_campaignMedalEntryCount;
extern int g_cutsceneViewerScrollRow;
extern int g_cutsceneViewerTotalRows;
extern char g_pilotRecordNameInput[14];
extern int g_pilotListScrollOffset;

#pragma pack(push, 1)

struct PilotNetworkPlayer {
	char formalName[14];
	char friendlyName[14];
	int flightGroupId;
	int directPlayId;
	int rating;
	int totalScore;
	int kills;
	int killsShared;
	int unknown34;
	int killsAssist;
	int totalLosses;
	int craftId;
	int craftOption;
	int warheadOption;
	int beamOption;
	int countermeasureOption;
	int hasLeft;
};

#pragma pack(pop)
typedef char xvt_size_PilotNetworkPlayer[(sizeof(PilotNetworkPlayer) == 88) ? 1 : -1];

#pragma pack(push, 1)

struct PilotTeam {
	int missionScore;
	int isMissionCompleted;
	int unknown08;
	int missionTime;
	int kills;
	int killsShared;
	int killsAssist;
};

#pragma pack(pop)
typedef char xvt_size_PilotTeam[(sizeof(PilotTeam) == 28) ? 1 : -1];

#pragma pack(push, 1)

struct PilotMission {
	int numberTimesFlown;
	int completedCount;
	int failedCount;
	int bestScore;
	int bestTime;
	int bestPlacement;
	int awardId;
	unsigned int bestBonus;
	int field20;
};

#pragma pack(pop)
typedef char xvt_size_PilotMission[(sizeof(PilotMission) == 36) ? 1 : -1];

#pragma pack(push, 1)

struct PilotMultiplayerMission {
	int numberTimesFlown;
	int firstPlaceCount;
	int secondPlaceCount;
	int thirdPlaceCount;
	int completedCount;
	int failedCount;
	int bestScore;
	int bestTime;
	int bestPlacement;
	int awardId;
	unsigned int bestBonus;
	int field2C;
};

#pragma pack(pop)
typedef char xvt_size_PilotMultiplayerMission[(sizeof(PilotMultiplayerMission) == 48) ? 1 : -1];

#pragma pack(push, 1)

struct PilotTournament {
	int attemptCount;
	int completedCount;
	int firstPlaceCount;
	int secondPlaceCount;
	int thirdPlaceCount;
	int bestScore;
	int bestPlacement;
	int awardId;
	unsigned int bestMargin;
	int field24;
};

#pragma pack(pop)
typedef char xvt_size_PilotTournament[(sizeof(PilotTournament) == 40) ? 1 : -1];

#pragma pack(push, 1)

struct PilotMultiplayerTournament {
	int attemptCount;
	int completedCount;
	int firstPlaceCount;
	int secondPlaceCount;
	int thirdPlaceCount;
	int bestScore;
	int bestPlacement;
	int field1C;
	int awardId;
	unsigned int bestMargin;
	int field28;
};

#pragma pack(pop)
typedef char xvt_size_PilotMultiplayerTournament[(sizeof(PilotMultiplayerTournament) == 44) ? 1 : -1];

#pragma pack(push, 1)

struct PilotBattle {
	int attemptCount;
	int victoryCount;
	int defeatCount;
	int drawCount; ///< Battles reaching the sequence mission limit without either side winning.
	int bestScore;
	int field14;
	int awardId;
	unsigned int bestVictoryMargin;
	int field20;
};

#pragma pack(pop)
typedef char xvt_size_PilotBattle[(sizeof(PilotBattle) == 36) ? 1 : -1];

#pragma pack(push, 1)

struct PilotMultiplayerBattle {
	int attemptCount;
	int victoryCount;
	int defeatCount;
	int drawCount; ///< Battles reaching the sequence mission limit without either side winning.
	int bestScore;
	int field14;
	int field18;
	int awardId;
	unsigned int bestVictoryMargin;
	int field24;
};

#pragma pack(pop)
typedef char xvt_size_PilotMultiplayerBattle[(sizeof(PilotMultiplayerBattle) == 40) ? 1 : -1];

#pragma pack(push, 1)

struct PilotCampaign {
	int attemptCount;
	int nextMissionIndex;
	int isFinished;
	int bestScore;
	int field10;
	int field14;
	int field18;
	int field1C;
	int field20;
};

#pragma pack(pop)
typedef char xvt_size_PilotCampaign[(sizeof(PilotCampaign) == 36) ? 1 : -1];

#pragma pack(push, 1)

struct PilotCampaignMission {
	int campaignId;
	int numberTimesFlown;
	int awardEligible;
	unsigned int bestScore;
	int awardId;
	int bestTime;
	int isCompleted;
	int field1C;
};

#pragma pack(pop)
typedef char xvt_size_PilotCampaignMission[(sizeof(PilotCampaignMission) == 32) ? 1 : -1];

#pragma pack(push, 1)

/* Accumulated combat statistics indexed by the game's three mission types. */
struct PilotStats {
	int totalScorePerMT[3];                  ///< Accumulated score by mission type (3).
	int standaloneMissionsPlayedPerMT[3];    ///< Standalone missions played by mission type (3).
	int sequenceMissionsPlayedPerMT[3];      ///< Sequence missions played by mission type (3).
	int totalKillsPerMT[3];                  ///< Full kills by mission type (3).
	int totalFriendliesKilledPerMT[3];       ///< Friendly kills by mission type (3).
	int killsPerCraftPerMT[3][100];          ///< Full kills by mission type and craft type (100).
	int killsSharedPerCraftPerMT[3][100];    ///< Shared kills by mission type and craft type (100).
	int killsAssistsPerCraftPerMT[3][100];   ///< Kill assists by mission type and craft type (100).
	int killsFullOnPlayerRatingPerMT[3][25]; ///< Full kills by mission type and victim player rating (25).
	int killsSharedOnPlayerRatingPerMT[3]
									  [25]; ///< Shared kills by mission type and victim player rating (25).
	int killsAssistOnPlayerRatingPerMT[3]
									  [25]; ///< Kill assists by mission type and victim player rating (25).
	int killsFullOnAIRatingPerMT[3][6];     ///< Full kills by mission type and victim AI rating (6).
	int killsSharedOnAIRatingPerMT[3][6];   ///< Shared kills by mission type and victim AI rating (6).
	int killsAssistOnAIRatingPerMT[3][6];   ///< Kill assists by mission type and victim AI rating (6).
	int numSpecialInspectedPerMT[3];        ///< Special-object inspections by mission type (3).
	int energyHitsPerMT[3];                 ///< Combined laser and ion hits by mission type (3).
	int energyFiredPerMT[3];                ///< Combined laser and ion shots fired by mission type (3).
	int warheadsHitsPerMT[3];               ///< Warhead hits by mission type (3).
	int warheadsFiredPerMT[3];              ///< Warheads fired by mission type (3).
	int totalCraftLossesPerMT[3];           ///< Total craft losses by mission type (3).
	int lossesByCollisionsPerMT[3];         ///< Collision losses by mission type (3).
	int lossesByStarshipsPerMT[3];          ///< Losses to starships by mission type (3).
	int lossesByMinesPerMT[3];              ///< Losses to mines by mission type (3).
	int killedByPlayerRatingPerMT[3][25];   ///< Deaths by mission type and opposing player rating (25).
	int killedByAIRatingPerMT[3][6];        ///< Deaths by mission type and opposing AI rating (6).
};

#pragma pack(pop)
typedef char xvt_size_PilotStats[(sizeof(PilotStats) == 5256) ? 1 : -1];

#pragma pack(push, 1)

struct PilotFaction {
	int totalMissionsPlayedCount; ///< Total missions played for this faction.
	int team;
	MissionDirectoryId missionDirectoryId;
	int missionDescriptionIds[6];
	uint8_t field24[32];
	int missionSequenceActive;        ///< Persisted active-sequence flag mirrored to
									  ///< PilotData.missionSequenceActive.
	int missionSequenceDescriptionId; ///< Persisted sequence descriptor ID mirrored to
									  ///< PilotData.missionSequenceDescriptionId.
	int meleePlaques[6];              ///< Melee plaque counts by award level 1-6.
	int tournamentTrophies[6];        ///< Tournament trophy counts by award level 1-6.
	int missionEvaluations[6];        ///< Mission evaluation counts by award level 1-6.
	int battleMedallions[6];          ///< Battle medallion counts by award level 1-6.
	int missionAwards[4]; ///< Current mission award levels: melee, tournament, evaluation, battle.
	uint8_t fieldBC[16];
	int totalScore;   ///< Overall score for this faction.
	PilotStats stats; ///< Accumulated combat statistics for this faction.
	uint8_t field1558[4];
	PilotMission spTrainingMissions[100];            ///< Single-player training history (100 records).
	PilotMission spMeleeMissions[250];               ///< Single-player melee history (250 records).
	PilotMission spCombatMissions[250];              ///< Single-player combat history (250 records).
	PilotMultiplayerMission mpTrainingMissions[100]; ///< Multiplayer training history (100 records).
	PilotMultiplayerMission mpMeleeMissions[250];    ///< Multiplayer melee history (250 records).
	PilotMultiplayerMission mpCombatMissions[250];   ///< Multiplayer combat history (250 records).
	PilotTournament spTournaments[25];               ///< Single-player tournament history (25 records).
	PilotMultiplayerTournament mpTournaments[25];    ///< Multiplayer tournament history (25 records).
	PilotBattle spBattles[25];                       ///< Single-player battle history (25 records).
	PilotMultiplayerBattle mpBattles[25];            ///< Multiplayer battle history (25 records).
	PilotCampaign spCampaigns[25];                   ///< Single-player campaign history (25 records).
	PilotCampaign mpCampaigns[25];                   ///< Multiplayer campaign history (25 records).
	uint8_t fieldF0E4[24]; ///< Unresolved bytes between multiplayer campaign history and the CD movie-check
						   ///< counter.
	uint32_t cdMovieCheckCounter;                ///< Concourse CD movie-check retry counter.
	PilotCampaignMission spCampaignMissions[99]; ///< Single-player campaign mission history indexed by
												 ///< one-based mission ID 1-99.
	uint8_t fieldFD60[32]; ///< Unresolved bytes preceding multiplayer campaign mission history.
	PilotCampaignMission mpCampaignMissions[99]; ///< Multiplayer campaign mission history indexed by
												 ///< one-based mission ID 1-99.
};

#pragma pack(pop)
typedef char xvt_size_PilotFaction[(sizeof(PilotFaction) == 68064) ? 1 : -1];

#pragma pack(push, 1)

struct PilotData {
	char name[14];
	int totalScore;
	int localPlayerId;
	int launchSessionMarker;
	int isHost;
	unsigned int numHumanPlayersLastMission;
	int gameMode;
	uint8_t xvtRecordPayload[672]; ///< Opaque 672-byte payload round-tripped by the XvT-compatible
								   ///< pilot-record reader and writer.
	int team;
	MissionDirectoryId missionDirectoryId;
	int32_t missionDescriptionIds[6]; ///< Selected mission or sequence descriptor ID for each of the six
									  ///< MissionDirectoryId values.
	char multiplayerGameName[32];
	char multiplayerHostName[32];
	int missionSequenceActive;
	int missionSequenceDescriptionId;
	int currentRatingPromoPoints;
	int currentRatingWorsePromoPoints;
	PilotPromotionDelta
		promotionDelta; ///< Persisted signed rank change: -1 demotion, 0 unchanged, +1 promotion.
	int nextPromotionPercent;
	PilotStats mainStats; ///< Pilot combat statistics.
	MeleeTournamentSequenceState meleeTournamentSequenceState;
	BattleSequenceState battleSequenceState;
	PilotRating rating;
	int totalMissionsPlayedCount;
	int totalMissionsPlayedCountPerRating[25];
	char ratingName[32];
	int missionScore;
	int killsFullOnPlayer[8];
	int killsSharedOnPlayer[8];
	int killsFullOnFlightGroup[48];
	int killsSharedOnFlightGroup[48];
	int killsFullFromPlayer[8];
	int killsSharedFromPlayer[8];
	int killsFullFromFlightGroup[48];
	int killsSharedFromFlightGroup[48];
	int flightGroupRating[48];
	PilotStats objectStats;               ///< Object-level combat statistics.
	PilotNetworkPlayer networkPlayers[8]; ///< Persisted network-player results (8).
	PilotTeam teams[10];                  ///< Persisted team results (10).
	int currentFactionId;                 ///< Selected faction-statistics record.
	PilotFaction factionStatistics[4];    ///< Per-faction pilot records (4 records, 0x109E0 bytes each).
	CampaignSequenceState campaignSequenceState; ///< Runtime state for an active campaign sequence.
	BattleContinuation
		spBattleContinuations[25]; ///< Saved single-player battle continuation slots indexed by battle ID.
	BattleContinuation
		mpBattleContinuations[25]; ///< Saved multiplayer battle continuation slots indexed by battle ID.
	CampaignContinuation spCampaignContinuations[25]; ///< Saved single-player campaign continuation slots
													  ///< indexed by campaign ID.
	CampaignContinuation mpCampaignContinuations[25]; ///< Saved multiplayer campaign continuation slots;
													  ///< client state uses the ID+12 partition.
};

#pragma pack(pop)
typedef char xvt_size_PilotData[(sizeof(PilotData) == 296238) ? 1 : -1];

extern PilotData g_pilotData;
extern unsigned int g_campaignAwardSpriteCount;
extern CampaignAwardSpriteEntry* g_campaignAwardSprites;
extern int g_pilotStatsAssists[3];
extern int g_pilotStatsPlayerKills[3];
extern int g_pilotStatsLossesToNonPlayers[3];
extern int g_pilotAchievementsScrollOffset;
extern int g_pilotSpTrainingHistoryCount;
extern int g_pilotSpMeleeHistoryCount;
extern int g_pilotSpCombatHistoryCount;
extern int g_pilotSpTournamentHistoryCount;
extern int g_pilotSpBattleHistoryCount;
extern int g_pilotSpCampaignHistoryRowCount;
extern int g_pilotMpTrainingHistoryCount;
extern int g_pilotMpMeleeHistoryCount;
extern int g_pilotMpCombatHistoryCount;
extern int g_pilotMpTournamentHistoryCount;
extern int g_pilotMpBattleHistoryCount;
extern int g_pilotStatsHasCraftKillsByType;
extern int g_pilotStatsRowHasData;
extern int g_pilotStatsHasLossesToPlayersByRank;
extern int g_pilotStatisticsScrollOffset;
extern int g_pilotStatsNonPlayerKillsShared[3];
extern int g_pilotStatsTotalKillsShared[3];
extern int g_pilotStatsNonPlayerKills[3];
extern int g_pilotStatsHasPlayerKillsByRating;
extern int g_pilotStatsPlayerKillsShared[3];
extern int g_pilotRecordPageRowCount;
extern int g_pilotStatsLossesToPlayers[3];
extern int g_pilotMpCampaignHistoryRowCount;

int PilotRecord_UpdatePilotSelectionPanel(int frameCounter);
int PilotRecord_DrawPilotList(const RECT* bounds, int firstVisibleIndex);
int PilotRecord_RebuildPilotList(int* selectedIndex);
int PilotRecord_DrawPilotStatisticsPage(void);
int PilotRecord_DrawMissionAchievementsPage(void);
int PilotRecord_DrawCutsceneViewerPage(void);
int PilotRecord_DrawCampaignMedalsPage(void);
int PilotRecord_DrawPilotAwardsPage(void);
int PilotRecord_DrawPilotRatingPage(void);
int PilotRecord_UpdateNavigationControls(void);
int PilotRecord_RedrawBackground(void);
int PilotRecord_LoadCampaignAwardSpriteTable(const char* fileName);

#ifdef __cplusplus
}
#endif

#endif
