#ifndef XVT_FRONTEND_MISSION_SETUP_H
#define XVT_FRONTEND_MISSION_SETUP_H

#include "xvt/assets/file.h"
#include "xvt/assets/object_type.h"
#include "xvt/frontend/pilot.h"
#include "xvt/xvt_typedefs.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stored as uint8_t in the binary (IDB enum BattleLength). */
typedef uint8_t BattleLength;

enum {
	BATTLE_LENGTH_TWO_WINS = 0x0,
	BATTLE_LENGTH_THREE_WINS = 0x1,
	BATTLE_LENGTH_FOUR_WINS = 0x2,
};

/* Stored as uint8_t in the binary (IDB enum CraftSelectionMode). */
typedef uint8_t CraftSelectionMode;

enum {
	CRAFT_SELECTION_OFF = 0x0,
	CRAFT_SELECTION_ON = 0x1,
	CRAFT_SELECTION_HOST_ONLY = 0x2,
};

/* Stored as uint8_t in the binary (IDB enum CraftWaveMode). */
typedef uint8_t CraftWaveMode;

enum {
	CRAFT_WAVES_NONE = 0x0,
	CRAFT_WAVES_DEFAULT = 0x1,
	CRAFT_WAVES_UNLIMITED = 0x2,
};

/* Stored as uint8_t in the binary (IDB enum CombatBalanceMode). */
typedef uint8_t CombatBalanceMode;

enum {
	COMBAT_BALANCE_AUTOBALANCE = 0x0,
	COMBAT_BALANCE_FAVOR_IMPERIAL = 0x1,
	COMBAT_BALANCE_NEUTRAL = 0x2,
	COMBAT_BALANCE_FAVOR_REBEL = 0x3,
};

/* Stored as int8_t in the binary (IDB enum SequenceContinuationChoice). */
typedef int8_t SequenceContinuationChoice;

enum {
	SEQUENCE_RESTART = 0x0,
	SEQUENCE_CONTINUE = 0x1,
};

#pragma pack(push, 1)

struct MpRosterEntry {
	char name[14];
	int playerId;
	PilotRating pilotRating;
	int craftTypeOverride;  ///< Nonzero exact craft type; zero selects the assigned flight group's base or
							///< optional craft.
	int craftOptionIndex;   ///< Optional craft index; values above 9 fall back to the assigned flight group's
							///< base craft.
	int warheadOptionIndex; ///< Warhead loadout option index; zero uses the mission default.
	int beamOptionIndex;    ///< Beam loadout option index; zero uses the mission default.
	int countermeasureOptionIndex; ///< Countermeasure loadout option index; zero uses the mission default.
};

#pragma pack(pop)
typedef char xvt_size_MpRosterEntry[(sizeof(MpRosterEntry) == 42) ? 1 : -1];

struct MissionSetupPlayerAssignments {
	int teamPlayerIds[10][8];
	int activePlayerIds[8];
};

struct ShipListEntry {
	char modelFileName[64];
	int typeId; ///< CraftSpecies value read from FRONTRES/frntspec.lst; stored as int by fscanf.
};

typedef enum MissionSetupActivePanel {
	MISSION_SETUP_PANEL_PLAYERS = 0x0,
	MISSION_SETUP_PANEL_SETTINGS = 0x1,
} MissionSetupActivePanel;

typedef enum MissionSetupDebriefTransition {
	MISSION_SETUP_DEBRIEF_TRANSITION_NONE = 0x0,
	MISSION_SETUP_DEBRIEF_TRANSITION_ENTER_CURRENT_MISSION = 0x1,
	MISSION_SETUP_DEBRIEF_TRANSITION_ADVANCE_MISSION_DIRECTORY = 0x2,
} MissionSetupDebriefTransition;

struct MeleeTournamentTeamStandings {
	int aiOpponentSourceTeamAndTypeFlag;
	int totalScore;
	int firstPlaceCount;
	int secondPlaceCount;
	int thirdPlaceCount;
};

struct MeleeTournamentSequenceState {
	int reserved[9];
	int currentMissionIndex;
	int missionCount;
	MeleeTournamentTeamStandings teamStandings[10];
	unsigned int humanPlayerCount;
	int participatingTeamCount;
	int quickStartAiBoostTeam;
};

typedef enum BattleMissionResult {
	BATTLE_MISSION_RESULT_IMPERIAL_VICTORY = 0x0,
	BATTLE_MISSION_RESULT_REBEL_VICTORY = 0x1,
	BATTLE_MISSION_RESULT_DRAW = 0x2,
} BattleMissionResult;

struct BattleSequenceState {
	uint32_t currentMissionIndex;
	int currentMissionId;
	int victoriesNeeded;
	BattleMissionResult missionResults[10];
	int missionOrdinals[10];
	int missionListIndices[10];
	unsigned int humanPlayerCount;
	int cumulativeScore;
};

struct CampaignSequenceState {
	int32_t field00; ///< Unresolved campaign-sequence field; cleared and persisted with the full state.
	int32_t currentMissionIndex;  ///< Zero-based position of the current campaign mission.
	int32_t missionCount;         ///< Mission count read from the selected campaign descriptor.
	int32_t lastMissionCompleted; ///< Whether the just-finished campaign mission completed successfully.
	int32_t humanPlayerCount;     ///< Human players participating in the campaign mission.
	int32_t cumulativeScore;      ///< Campaign score accumulated across completed missions.
};

struct BattleContinuation {
	int32_t field00;           ///< Unresolved persisted battle-continuation field.
	uint32_t randomSeed;       ///< Random seed used to reproduce mission selection.
	int32_t isActive;          ///< Continuation slot contains an unfinished battle.
	int32_t battleLengthIndex; ///< Configured battle-length selector.
	int32_t randomSetup;       ///< Configured sequential, random, or player-choice selection mode.
	BattleSequenceState state; ///< Saved battle sequence state.
};

struct CampaignContinuation {
	int32_t field00;             ///< Unresolved persisted campaign-continuation field.
	uint32_t randomSeed;         ///< Random seed used to reproduce mission selection.
	int32_t isActive;            ///< Continuation slot contains an unfinished campaign.
	int32_t randomSetup;         ///< Configured sequential, random, or player-choice selection mode.
	CampaignSequenceState state; ///< Saved campaign sequence state.
};

extern int g_teamFgCountScratch[10];
extern int g_missionSetupDraggedPlayerId;
extern int g_missionSetupReservedPlayerIds[8];
extern int g_missionSetupReservedPlayerCount;
extern int g_missionSetupTeamAssignmentSkipped;
extern int g_missionSetupLaunchCountdownCurrentTick;
extern int g_missionSetupLaunchCountdownMs;
extern int g_missionSetupLaunchCountdownPreviousTick;
extern int g_missionSetupLaunchSignalSent;
extern int g_teamCount;
extern int g_missionSetupLastBroadcastCountdownSecond;
extern int g_missionSetupPlayerFlightGroupIndices[80];
extern int g_textShadeRamps[5][8];
extern MissionSetupPlayerAssignments g_missionSetupPlayerAssignments;
extern ShipListEntry* g_shipList;
extern int g_shipTypeToShipListIndex[18];
extern int g_shipCount;
extern int g_missionSetupSelectedFlightGroupIndex;
extern int g_missionSetupSelectedFlightGroupCraftOptionIndex;
extern int g_missionSetupSelectedPresetCraftOptionIndex;
extern int g_missionSetupPresetCraftOptionCount;
extern int g_missionSetupSelectedWarheadOptionIndex;
extern int g_missionSetupSelectedBeamOptionIndex;
extern int g_missionSetupSelectedCountermeasureOptionIndex;
extern int g_missionSetupSelectedWaveCountMinusOne;
extern int g_missionSetupSelectedCraftCount;
extern int g_missionSetupFlightGroupCraftOptionCount;
extern int g_missionSetupWarheadOptionCount;
extern int g_missionSetupBeamOptionCount;
extern int g_missionSetupCountermeasureOptionCount;
extern const int g_warheadTypeMap[11];
extern const int g_presetCraftTypes[11];
extern const uint8_t g_craftIffCounterpart[20];
extern unsigned int g_missionCount;
extern int g_selectedMissionListIndex;
extern MissionListEntry* g_missionList;
extern int g_frontendSinglePlayerFlightSessionActive;
extern int g_missionSetupIsHost;
extern int g_missionSetupRosterAuthoritative;
extern int g_missionSetupBeginButtonLockoutFrames;
extern int g_skipFrontendEntryMovie;
extern const char* g_campaignDirNames[6];
extern MpRosterEntry g_mpRoster[8];
extern int g_mpRosterReadyFlags[8];
extern int g_battleMissionListCount;
extern MissionListEntry* g_battleMissionList;
extern int g_battleChoiceTick;
extern int g_battleChoiceScrollOffset;
extern int g_battleChoiceRemainingMs;
extern int g_battleChoiceLastTick;
extern int g_battleChoiceRowCount;
extern int g_battleChoiceLastSentSecond;
extern int g_battleChoiceTimeoutHandled;
extern int g_missionSetupUseCombatSimPilotState;
extern int g_missionSetupMissionListRowCount;
extern int g_missionSetupMissionListScrollOffset;
extern int g_remoteBattleSequenceActive;
extern int g_remoteBattleSequenceContinuationChoice;
extern int g_remoteBattleLastCompletedMissionIndex;
extern int g_remoteBattleRebelVictoryCount;
extern int g_remoteBattleImperialVictoryCount;
extern int g_missionSetupSelectedPlayerRosterIndex;
extern MissionSetupActivePanel g_missionSetupActivePanel;
extern int g_missionSetupLastHostBroadcastTick;
extern int g_localPilotNetworkPlayerIndex;
extern int g_missionSetupShowDescriptionPanel;
extern MissionSetupDebriefTransition g_missionSetupDebriefTransition;

int MissionSetup_Exit(int frameCounter);
int MissionSetup_Update(int frameCounter);
int MissionSetup_DrawMissionTypeControls(void);
void MissionSetup_LoadMissionList(int missionDirectoryId);
void MissionSetup_LoadMissionDescText(char* outText4096);
int MissionSetup_DrawMissionDescription(void);
int MissionSetup_DrawPlayerRoster(int frameCounter);
int MissionSetup_BroadcastLobbySelection(void);
int MissionSetup_BroadcastStatePacket(int toPlayerId);
int MissionSetup_BroadcastReadyRoster(int toPlayerId);
int MissionSetup_DrawMissionList(int frameCounter);
int MissionSetup_SelectFirstSequenceMission(void);
int MissionSetup_DrawGameSettings(void);
int MissionSetup_CountMissionListEntries(XvtFile* stream);
int MissionSetup_DrawBackgroundAndPreview(void);
int MissionSetup_UseRebelBackground(void);
void MissionSetup_DrawCraftLoadout(void);
void MissionSetup_DrawPlayerLoadouts(int frameCounter);
int MissionSetup_UpdateCraftLoadout(void);
void MissionSetup_InitCraftLoadout(void);
int MissionSetup_GetWarheadType(int playerRosterIndex);
int MissionSetup_GetBeamType(int playerRosterIndex);
int MissionSetup_GetCountermeasureType(int playerRosterIndex);
int MissionSetup_GetCraftType(int playerRosterIndex);
void ShipList_Load(void);
int MissionSetup_ExitNextMission(void);
int MissionSetup_EnterNextMission(int frameCounter);
int MissionSetup_PruneDisconnectedPlayers(void);
int MpRoster_CompactActiveEntries(void);
int MissionSetup_SelectNextSequenceMission(void);
int MissionSetup_ExitCurrentMission(void);
int MissionSetup_EnterCurrentMission(int frameCounter);
int MissionSetup_TeamAssignmentUpdate(int frameCounter);
int MissionSetup_DrawUnassignedPlayers(int frameCounter);
void MissionSetup_CountActiveTeams(void);
void MissionSetup_DrawTeamAssignments(int frameCounter);
void MissionSetup_PruneTeamAssignments(void);
int MissionSetup_IsTeamAssignmentValid(void);
int MissionSetup_UpdateTeamControls(void);
int MissionSetup_RandomizeTeamAssignments(void);
int MissionSetup_ClearTeamAssignments(void);
int MissionSetup_BroadcastTeamAssignments(void);
int MissionSetup_TryContinueBattle(void);
int MissionSetup_TryContinueCampaign(void);
int MissionSetup_DrawTeamMissionDescription(void);
int MissionSetup_FreeScreenResources(int frameCounter);
int MissionSetup_FlightAssignmentUpdate(int frameCounter);
int MissionSetup_DrawAssignmentControls(void);
int MissionSetup_DrawFlightAssignments(int frameCounter);
void MissionSetup_PruneFlightAssignments(void);
int MissionSetup_AreFlightAssignmentsComplete(void);
int MissionSetup_RandomizeFlightAssignments(void);
int MissionSetup_ClearFlightAssignments(void);
int MissionSetup_FillFlightAssignments(void);
int MissionSetup_DrawAssignedPlayers(int frameCounter);
int MissionSetup_DrawAssignmentBriefing(void);
int MissionSetup_BattleChoice_Exit(void);
int MissionSetup_BattleChoice_Update(int frameCounter);
int MissionSetup_BattleChoice_DrawDescription(void);
int MissionSetup_BattleChoice_DrawRoster(int frameCounter);
int MissionSetup_BattleChoice_BuildList(void);
int MissionSetup_BattleChoice_DrawList(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
