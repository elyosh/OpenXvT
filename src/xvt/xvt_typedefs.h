#ifndef XVT_XVT_TYPEDEFS_H
#define XVT_XVT_TYPEDEFS_H

#include "aeron/compat/dplay.h"
#include "aeron/compat/dsound.h"

/* Forward typedefs for every structure recovered from the
 * IDB. The defining module header declares the layout; pointer
 * references only need these names. Generated with the skeleton;
 * keep in sync with the IDB local types. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ActiveSoundInstance ActiveSoundInstance;
typedef struct AiController AiController;
typedef struct AiFlightState AiFlightState;
typedef struct AiOrderScratch AiOrderScratch;
typedef struct BattleContinuation BattleContinuation;
typedef struct BattleSequenceState BattleSequenceState;
typedef struct BitmapFont BitmapFont;
typedef struct BriefingMapS16Pair BriefingMapS16Pair;
typedef struct CDAudioTrackCache CDAudioTrackCache;
typedef struct CampaignAwardSpriteEntry CampaignAwardSpriteEntry;
typedef struct CampaignContinuation CampaignContinuation;
typedef struct CampaignSequenceState CampaignSequenceState;
typedef struct ColorInfo ColorInfo;
typedef struct CraftDamageStats CraftDamageStats;
typedef struct CraftData CraftData;
typedef struct CraftLaserState CraftLaserState;
typedef struct CraftTechStats CraftTechStats;
typedef struct CraftWeaponSlot CraftWeaponSlot;
typedef struct CraftWeaponStats CraftWeaponStats;
typedef struct CutsceneEntry CutsceneEntry;
/* Original Windows spellings for Aeron's shared DirectSound ABI types. */
typedef DSBufferCaps DSBCAPS;
typedef DSBufferDesc DSBUFFERDESC;
typedef DSoundDeviceCaps DSCAPS;
typedef struct DirectSoundBufferSet DirectSoundBufferSet;
typedef struct EAIStruct EAIStruct;
typedef struct ECondStruct ECondStruct;
typedef struct EFGStruct EFGStruct;
typedef struct EMissionGoal EMissionGoal;
typedef struct EMissionStruct EMissionStruct;
typedef struct FaceRecord FaceRecord;
typedef struct FaceTextureGradients FaceTextureGradients;
typedef struct FlightCursorShapeOffset FlightCursorShapeOffset;
typedef struct FlightGlobalCountdownTimers FlightGlobalCountdownTimers;
typedef struct FlightGroupGoal FlightGroupGoal;
typedef struct FlightInputFrameRecord FlightInputFrameRecord;
typedef struct FlightLaunchArgs FlightLaunchArgs;
typedef struct FlightMissionState FlightMissionState;
typedef struct FlightNetInputDeltaBatchPacket FlightNetInputDeltaBatchPacket;
typedef struct FlightNetScratchPacket FlightNetScratchPacket;
typedef struct FlightNetWorldStateChunkPacket FlightNetWorldStateChunkPacket;
typedef struct FlightSwMarkerOffset FlightSwMarkerOffset;
typedef struct FlightSwRotSpriteCoeffState FlightSwRotSpriteCoeffState;
typedef struct FlightSwRotSpriteEdgePoint FlightSwRotSpriteEdgePoint;
typedef struct FlightSwRotSpriteScaleState FlightSwRotSpriteScaleState;
typedef struct FlightSwRotSpriteSpanRun FlightSwRotSpriteSpanRun;
typedef struct FlightViewportSaveState FlightViewportSaveState;
typedef struct FrontImageResourceRecord FrontImageResourceRecord;
typedef struct FrontImageRleRowBuffer FrontImageRleRowBuffer;
typedef struct FrontendBriefingScript FrontendBriefingScript;
typedef struct FrontendFileList FrontendFileList;
typedef struct FrontendFileListNode FrontendFileListNode;
typedef struct FrontendMission FrontendMission;
typedef struct FrontendMissionHeader FrontendMissionHeader;
typedef struct FrontendNetPacketScratch FrontendNetPacketScratch;
typedef struct FrontendNetSessionEntry FrontendNetSessionEntry;
typedef struct FrontendScreenState FrontendScreenState;
typedef struct FrontendSoundBufferRecord FrontendSoundBufferRecord;
typedef struct FrontendSoundVoice FrontendSoundVoice;
typedef struct GameConfig GameConfig;
typedef struct GlobalGoal GlobalGoal;
typedef struct HudBeamSegmentOffset HudBeamSegmentOffset;
typedef struct HudCockpitResource HudCockpitResource;
typedef struct HudCockpitResourceDescriptor HudCockpitResourceDescriptor;
typedef struct HudElementLayout HudElementLayout;
typedef struct HudInFlightMessageRecord HudInFlightMessageRecord;
typedef struct HudPanelSpriteFileInfo HudPanelSpriteFileInfo;
typedef struct HudRadarBlipPoint HudRadarBlipPoint;
typedef struct ImageQuantizerNode ImageQuantizerNode;
typedef struct ImageQuantizerPaletteEntry ImageQuantizerPaletteEntry;
typedef struct ImageResource ImageResource;
typedef struct InputFrame InputFrame;
typedef struct InventorNodeDef InventorNodeDef;
typedef struct JoystickCalibration JoystickCalibration;
typedef struct JoystickEntry JoystickEntry;
typedef struct MeleeTournamentSequenceState MeleeTournamentSequenceState;
typedef struct MeleeTournamentTeamStandings MeleeTournamentTeamStandings;
typedef struct MemoryHandleTableState MemoryHandleTableState;
typedef struct MeshDescriptor MeshDescriptor;
typedef struct MissionClock MissionClock;
typedef struct MissionFgRuntimeStats MissionFgRuntimeStats;
typedef struct MissionFlightGroup MissionFlightGroup;
typedef struct MissionFlightRuntimeState MissionFlightRuntimeState;
typedef struct MissionHeader MissionHeader;
typedef struct MissionListEntry MissionListEntry;
typedef struct MissionMessage MissionMessage;
typedef struct MissionOrder MissionOrder;
typedef struct MissionSetupPlayerAssignments MissionSetupPlayerAssignments;
typedef struct MissionTrigger MissionTrigger;
typedef struct MissionTriggerPair MissionTriggerPair;
typedef struct MobileObject MobileObject;
typedef struct MobileObjectCharData MobileObjectCharData;
typedef struct MobileObjectLinkIndices MobileObjectLinkIndices;
typedef struct MobileObjectProximityList MobileObjectProximityList;
typedef struct ModelDef ModelDef;
typedef struct ModelHangarPoints ModelHangarPoints;
typedef struct ModelLocalPoint ModelLocalPoint;
typedef struct ModelMeshObjectTypeCache ModelMeshObjectTypeCache;
typedef struct ModelPreviewCraftPosition ModelPreviewCraftPosition;
typedef struct ModelTextureDefaultTexture ModelTextureDefaultTexture;
typedef struct ModelTextureDefaultTextureData ModelTextureDefaultTextureData;
typedef struct ModelTypeInfo ModelTypeInfo;
typedef struct ModelWeaponHardpoint ModelWeaponHardpoint;
typedef struct MovieDirtyRect MovieDirtyRect;
typedef struct MovieMultiplayerSyncPlayer MovieMultiplayerSyncPlayer;
typedef struct MoviePlaybackParams MoviePlaybackParams;
typedef struct MpRosterEntry MpRosterEntry;
typedef struct MusicCdTrackCache MusicCdTrackCache;
typedef struct NetPendingPayload NetPendingPayload;
typedef struct NetPlayerConnectionStats NetPlayerConnectionStats;
typedef struct NetPlayerInfo NetPlayerInfo;
typedef struct NetQueuedPacket NetQueuedPacket;
typedef struct NetReliablePeerSlot NetReliablePeerSlot;
typedef struct NetSessionEnumEntry NetSessionEnumEntry;
typedef struct NetSessionScratchPacket NetSessionScratchPacket;
typedef struct ObjectPointLight ObjectPointLight;
typedef struct ObjectRecord ObjectRecord;
typedef struct ObjectSlotRange ObjectSlotRange;
typedef struct OptHardpoint OptHardpoint;
typedef struct OptLegacyFaceDataV0 OptLegacyFaceDataV0;
typedef struct OptLegacyFacePayload OptLegacyFacePayload;
typedef struct OptLegacyFacePayloadV0 OptLegacyFacePayloadV0;
typedef struct OptLegacyFaceRecordV0 OptLegacyFaceRecordV0;
typedef struct OptLegacyFaceStorage OptLegacyFaceStorage;
typedef struct OptLegacyFaceStorageV0 OptLegacyFaceStorageV0;
typedef struct OptNode OptNode;
typedef struct OptPackedFaceData OptPackedFaceData;
typedef struct OptPackedFaceNode OptPackedFaceNode;
typedef struct OptPackedFaceRecord OptPackedFaceRecord;
typedef struct OptTexCoord OptTexCoord;
typedef struct OptTextureData OptTextureData;
typedef struct OptVector OptVector;
typedef struct OptimizedPolyObject OptimizedPolyObject;
typedef struct PaiContext PaiContext;
typedef struct PaiPlanRecord PaiPlanRecord;
typedef struct PaiPlanTokenDef PaiPlanTokenDef;
typedef struct PerMissionKills PerMissionKills;
typedef struct PilotBattle PilotBattle;
typedef struct PilotCampaign PilotCampaign;
typedef struct PilotCampaignMission PilotCampaignMission;
typedef struct PilotData PilotData;
typedef struct PilotDataSelection PilotDataSelection;
typedef struct PilotFaction PilotFaction;
typedef struct PilotMission PilotMission;
typedef struct PilotMultiplayerBattle PilotMultiplayerBattle;
typedef struct PilotMultiplayerMission PilotMultiplayerMission;
typedef struct PilotMultiplayerTournament PilotMultiplayerTournament;
typedef struct PilotNetworkPlayer PilotNetworkPlayer;
typedef struct PilotStats PilotStats;
typedef struct PilotTeam PilotTeam;
typedef struct PilotTournament PilotTournament;
typedef struct PlayerData PlayerData;
typedef struct PlayerFlightTransientTimers PlayerFlightTransientTimers;
typedef struct PlayerHyperspaceRuntime PlayerHyperspaceRuntime;
typedef struct PlayerMissionRuntimeStats PlayerMissionRuntimeStats;
typedef struct PlayerNetworkRuntimeTail PlayerNetworkRuntimeTail;
typedef struct PlayerSavedCraftSettings PlayerSavedCraftSettings;
typedef struct PlayerViewState PlayerViewState;
typedef struct ProjVertex ProjVertex;
typedef struct RadarEllipseClampLimit RadarEllipseClampLimit;
typedef struct RemotePlayerRenderSample RemotePlayerRenderSample;
typedef struct RemotePlayerSavedRenderPose RemotePlayerSavedRenderPose;
typedef struct RenderClipVertex RenderClipVertex;
typedef struct RenderObjectListEntry RenderObjectListEntry;
typedef struct RgbTriplet RgbTriplet;
typedef struct SceneBillboardQueueEntry SceneBillboardQueueEntry;
typedef struct SceneEdge SceneEdge;
typedef struct SceneFace SceneFace;
typedef struct SceneMesh SceneMesh;
typedef struct SceneSpan SceneSpan;
typedef struct SessionPlayerInfo SessionPlayerInfo;
typedef struct ShipListEntry ShipListEntry;
typedef struct SoundEffectDef SoundEffectDef;
typedef struct SoundQueueEntry SoundQueueEntry;
typedef struct Std3DDevice Std3DDevice;
typedef struct Std3DDeviceCaps Std3DDeviceCaps;
typedef struct Std3DDeviceDesc Std3DDeviceDesc;
typedef struct Std3DErrorStringEntry Std3DErrorStringEntry;
typedef struct Std3DRasterInfo Std3DRasterInfo;
typedef struct Std3DRenderTargetDesc Std3DRenderTargetDesc;
typedef struct Std3DRenderTri Std3DRenderTri;
typedef struct Std3DSurfaceState Std3DSurfaceState;
typedef struct Std3DTexCacheNode Std3DTexCacheNode;
typedef struct Std3DTexFmt Std3DTexFmt;
typedef struct Std3DVBuffer Std3DVBuffer;
typedef struct Std3DViewportRect Std3DViewportRect;
typedef struct Std3DZBufferSurfaceBlock Std3DZBufferSurfaceBlock;
typedef struct Std3DZBufferTargetScratch Std3DZBufferTargetScratch;
typedef struct Team Team;
typedef struct TechLibrarySpecText TechLibrarySpecText;
typedef struct TieRadioMessage TieRadioMessage;
typedef struct TurretTargetState TurretTargetState;
typedef DSWaveFormat WAVEFORMATEX;
typedef struct WarheadGuidanceState WarheadGuidanceState;
typedef struct XvtFlightGroup XvtFlightGroup;
typedef struct XvtV10FlightGroupText XvtV10FlightGroupText;
typedef struct XvtV10MissionHeader XvtV10MissionHeader;

#ifdef __cplusplus
}
#endif

#endif
