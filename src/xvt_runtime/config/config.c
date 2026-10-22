#include "xvt_runtime/config/config.h"
#include "aeron/aeron.h"
#include "aeron/compat/dplay_directory.h"
#include "aeron/log.h"
#include "xvt/net/net.h"
#include "xvt_runtime/config/controller_config.h"
#include "xvt_runtime/config/keyboard_config.h"
#include "xvt_runtime/storage/file_io.h"
#include "xvt_runtime/storage/storage.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct XvtConfigField {
	const char* legacy;
	const char* path;
	size_t offset;
	size_t size;
	int string;
	int64_t maximum;
} XvtConfigField;

static const XvtConfigField g_fields[] = {
	{ "lastpilot", "game.lastpilot", offsetof(GameConfig, lastPilotName),
	  sizeof(((GameConfig*)0)->lastPilotName), 1, 1 },
	{ "backdrop1", "game.single_player.backdrop", offsetof(GameConfig, backdrop[0]),
	  sizeof(((GameConfig*)0)->backdrop[0]), 0, 1 },
	{ "backdrop2", "game.multiplayer.backdrop", offsetof(GameConfig, backdrop[1]),
	  sizeof(((GameConfig*)0)->backdrop[1]), 0, 1 },
	{ "stardensity1", "game.single_player.stardensity", offsetof(GameConfig, starDensity[0]),
	  sizeof(((GameConfig*)0)->starDensity[0]), 0, 2 },
	{ "stardensity2", "game.multiplayer.stardensity", offsetof(GameConfig, starDensity[1]),
	  sizeof(((GameConfig*)0)->starDensity[1]), 0, 2 },
	{ "debris1", "game.single_player.debris", offsetof(GameConfig, debris[0]),
	  sizeof(((GameConfig*)0)->debris[0]), 0, 1 },
	{ "debris2", "game.multiplayer.debris", offsetof(GameConfig, debris[1]),
	  sizeof(((GameConfig*)0)->debris[1]), 0, 1 },
	{ "locallights1", "game.single_player.locallights", offsetof(GameConfig, localLights[0]),
	  sizeof(((GameConfig*)0)->localLights[0]), 0, 1 },
	{ "locallights2", "game.multiplayer.locallights", offsetof(GameConfig, localLights[1]),
	  sizeof(((GameConfig*)0)->localLights[1]), 0, 1 },
	{ "specular1", "game.single_player.specular", offsetof(GameConfig, specular[0]),
	  sizeof(((GameConfig*)0)->specular[0]), 0, 1 },
	{ "specular2", "game.multiplayer.specular", offsetof(GameConfig, specular[1]),
	  sizeof(((GameConfig*)0)->specular[1]), 0, 1 },
	{ "diffuse1", "game.single_player.diffuse", offsetof(GameConfig, diffuse[0]),
	  sizeof(((GameConfig*)0)->diffuse[0]), 0, 1 },
	{ "diffuse2", "game.multiplayer.diffuse", offsetof(GameConfig, diffuse[1]),
	  sizeof(((GameConfig*)0)->diffuse[1]), 0, 1 },
	{ "dither1", "game.single_player.dither", offsetof(GameConfig, dither[0]),
	  sizeof(((GameConfig*)0)->dither[0]), 0, 1 },
	{ "dither2", "game.multiplayer.dither", offsetof(GameConfig, dither[1]),
	  sizeof(((GameConfig*)0)->dither[1]), 0, 1 },
	{ "textureres1", "game.single_player.textureres", offsetof(GameConfig, textureRes[0]),
	  sizeof(((GameConfig*)0)->textureRes[0]), 0, 2 },
	{ "textureres2", "game.multiplayer.textureres", offsetof(GameConfig, textureRes[1]),
	  sizeof(((GameConfig*)0)->textureRes[1]), 0, 2 },
	{ "mipmap1", "game.single_player.mipmap", offsetof(GameConfig, mipmap[0]),
	  sizeof(((GameConfig*)0)->mipmap[0]), 0, 19 },
	{ "mipmap2", "game.multiplayer.mipmap", offsetof(GameConfig, mipmap[1]),
	  sizeof(((GameConfig*)0)->mipmap[1]), 0, 19 },
	{ "lod1", "game.single_player.lod", offsetof(GameConfig, lod[0]), sizeof(((GameConfig*)0)->lod[0]), 0,
	  19 },
	{ "lod2", "game.multiplayer.lod", offsetof(GameConfig, lod[1]), sizeof(((GameConfig*)0)->lod[1]), 0, 19 },
	{ "screenres1", "game.single_player.screenres", offsetof(GameConfig, screenRes[0]),
	  sizeof(((GameConfig*)0)->screenRes[0]), 0, 2 },
	{ "screenres2", "game.multiplayer.screenres", offsetof(GameConfig, screenRes[1]),
	  sizeof(((GameConfig*)0)->screenRes[1]), 0, 2 },
	{ "windowsize1", "game.single_player.windowsize", offsetof(GameConfig, windowSize[0]),
	  sizeof(((GameConfig*)0)->windowSize[0]), 0, 2 },
	{ "windowsize2", "game.multiplayer.windowsize", offsetof(GameConfig, windowSize[1]),
	  sizeof(((GameConfig*)0)->windowSize[1]), 0, 2 },
	{ "bpp1", "game.single_player.bpp", offsetof(GameConfig, bpp[0]), sizeof(((GameConfig*)0)->bpp[0]), 0,
	  1 },
	{ "bpp2", "game.multiplayer.bpp", offsetof(GameConfig, bpp[1]), sizeof(((GameConfig*)0)->bpp[1]), 0, 1 },
	{ "brightness1", "game.single_player.brightness", offsetof(GameConfig, brightness[0]),
	  sizeof(((GameConfig*)0)->brightness[0]), 0, 7 },
	{ "brightness2", "game.multiplayer.brightness", offsetof(GameConfig, brightness[1]),
	  sizeof(((GameConfig*)0)->brightness[1]), 0, 7 },
	{ "use_3d_hardware1", "game.single_player.use_3d_hardware", offsetof(GameConfig, use3dHardware[0]),
	  sizeof(((GameConfig*)0)->use3dHardware[0]), 0, 1 },
	{ "use_3d_hardware2", "game.multiplayer.use_3d_hardware", offsetof(GameConfig, use3dHardware[1]),
	  sizeof(((GameConfig*)0)->use3dHardware[1]), 0, 1 },
	{ "bilinear1", "game.single_player.bilinear", offsetof(GameConfig, bilinear[0]),
	  sizeof(((GameConfig*)0)->bilinear[0]), 0, 1 },
	{ "bilinear2", "game.multiplayer.bilinear", offsetof(GameConfig, bilinear[1]),
	  sizeof(((GameConfig*)0)->bilinear[1]), 0, 1 },
	{ "server_update_rate", "game.server_update_rate", offsetof(GameConfig, serverUpdateRate),
	  sizeof(((GameConfig*)0)->serverUpdateRate), 0, 255 },
	{ "sfx_exterior", "game.sfx_exterior", offsetof(GameConfig, sfxExteriorEnabled),
	  sizeof(((GameConfig*)0)->sfxExteriorEnabled), 0, 1 },
	{ "sfx_interior", "game.sfx_interior", offsetof(GameConfig, sfxInteriorEnabled),
	  sizeof(((GameConfig*)0)->sfxInteriorEnabled), 0, 1 },
	{ "sfx_engine", "game.sfx_engine", offsetof(GameConfig, sfxEngineEnabled),
	  sizeof(((GameConfig*)0)->sfxEngineEnabled), 0, 1 },
	{ "sfx_datapad", "game.sfx_datapad", offsetof(GameConfig, sfxDatapadEnabled),
	  sizeof(((GameConfig*)0)->sfxDatapadEnabled), 0, 1 },
	{ "voice_pilot", "game.voice_pilot", offsetof(GameConfig, voicePilotEnabled),
	  sizeof(((GameConfig*)0)->voicePilotEnabled), 0, 2 },
	{ "voice_tactical_officer", "game.voice_tactical_officer",
	  offsetof(GameConfig, voiceTacticalOfficerEnabled),
	  sizeof(((GameConfig*)0)->voiceTacticalOfficerEnabled), 0, 2 },
	{ "voice_commander", "game.voice_commander", offsetof(GameConfig, voiceCommanderEnabled),
	  sizeof(((GameConfig*)0)->voiceCommanderEnabled), 0, 1 },
	{ "voice_special", "game.voice_special", offsetof(GameConfig, voiceSpecialEnabled),
	  sizeof(((GameConfig*)0)->voiceSpecialEnabled), 0, 1 },
	{ "music", "game.music", offsetof(GameConfig, musicEnabled), sizeof(((GameConfig*)0)->musicEnabled), 0,
	  1 },
	{ "sfx_datapad_volume", "game.sfx_datapad_volume", offsetof(GameConfig, sfxDatapadVolume),
	  sizeof(((GameConfig*)0)->sfxDatapadVolume), 0, 9 },
	{ "sfx_exterior_volume", "game.sfx_exterior_volume", offsetof(GameConfig, sfxExteriorVolume),
	  sizeof(((GameConfig*)0)->sfxExteriorVolume), 0, 9 },
	{ "sfx_interior_volume", "game.sfx_interior_volume", offsetof(GameConfig, sfxInteriorVolume),
	  sizeof(((GameConfig*)0)->sfxInteriorVolume), 0, 9 },
	{ "sfx_engine_volume", "game.sfx_engine_volume", offsetof(GameConfig, sfxEngineVolume),
	  sizeof(((GameConfig*)0)->sfxEngineVolume), 0, 9 },
	{ "voice_volume", "game.voice_volume", offsetof(GameConfig, voiceVolume),
	  sizeof(((GameConfig*)0)->voiceVolume), 0, 9 },
	{ "music_volume", "game.music_volume", offsetof(GameConfig, musicVolume),
	  sizeof(((GameConfig*)0)->musicVolume), 0, 9 },
	{ "datapad_music", "game.datapad_music", offsetof(GameConfig, datapadMusicEnabled),
	  sizeof(((GameConfig*)0)->datapadMusicEnabled), 0, 1 },
	{ "joybutton1", "game.joybutton1", offsetof(GameConfig, joyButtons[0]),
	  sizeof(((GameConfig*)0)->joyButtons[0]), 0, 255 },
	{ "joybutton2", "game.joybutton2", offsetof(GameConfig, joyButtons[1]),
	  sizeof(((GameConfig*)0)->joyButtons[1]), 0, 255 },
	{ "joybutton3", "game.joybutton3", offsetof(GameConfig, joyButtons[2]),
	  sizeof(((GameConfig*)0)->joyButtons[2]), 0, 255 },
	{ "joybutton4", "game.joybutton4", offsetof(GameConfig, joyButtons[3]),
	  sizeof(((GameConfig*)0)->joyButtons[3]), 0, 255 },
	{ "joybutton5", "game.joybutton5", offsetof(GameConfig, joyButtons[4]),
	  sizeof(((GameConfig*)0)->joyButtons[4]), 0, 255 },
	{ "joybutton6", "game.joybutton6", offsetof(GameConfig, joyButtons[5]),
	  sizeof(((GameConfig*)0)->joyButtons[5]), 0, 255 },
	{ "joybutton7", "game.joybutton7", offsetof(GameConfig, joyButtons[6]),
	  sizeof(((GameConfig*)0)->joyButtons[6]), 0, 255 },
	{ "joybutton8", "game.joybutton8", offsetof(GameConfig, joyButtons[7]),
	  sizeof(((GameConfig*)0)->joyButtons[7]), 0, 255 },
	{ "joybutton9", "game.joybutton9", offsetof(GameConfig, joyButtons[8]),
	  sizeof(((GameConfig*)0)->joyButtons[8]), 0, 255 },
	{ "joybutton10", "game.joybutton10", offsetof(GameConfig, joyButtons[9]),
	  sizeof(((GameConfig*)0)->joyButtons[9]), 0, 255 },
	{ "joybutton11", "game.joybutton11", offsetof(GameConfig, joyButtons[10]),
	  sizeof(((GameConfig*)0)->joyButtons[10]), 0, 255 },
	{ "joybutton12", "game.joybutton12", offsetof(GameConfig, joyButtons[11]),
	  sizeof(((GameConfig*)0)->joyButtons[11]), 0, 255 },
	{ "joybutton13", "game.joybutton13", offsetof(GameConfig, joyButtons[12]),
	  sizeof(((GameConfig*)0)->joyButtons[12]), 0, 255 },
	{ "joybutton14", "game.joybutton14", offsetof(GameConfig, joyButtons[13]),
	  sizeof(((GameConfig*)0)->joyButtons[13]), 0, 255 },
	{ "joybutton15", "game.joybutton15", offsetof(GameConfig, joyButtons[14]),
	  sizeof(((GameConfig*)0)->joyButtons[14]), 0, 255 },
	{ "joybutton16", "game.joybutton16", offsetof(GameConfig, joyButtons[15]),
	  sizeof(((GameConfig*)0)->joyButtons[15]), 0, 255 },
	{ "joybutton17", "game.joybutton17", offsetof(GameConfig, joyButtons[16]),
	  sizeof(((GameConfig*)0)->joyButtons[16]), 0, 255 },
	{ "joybutton18", "game.joybutton18", offsetof(GameConfig, joyButtons[17]),
	  sizeof(((GameConfig*)0)->joyButtons[17]), 0, 255 },
	{ "joybutton19", "game.joybutton19", offsetof(GameConfig, joyButtons[18]),
	  sizeof(((GameConfig*)0)->joyButtons[18]), 0, 255 },
	{ "joybutton20", "game.joybutton20", offsetof(GameConfig, joyButtons[19]),
	  sizeof(((GameConfig*)0)->joyButtons[19]), 0, 255 },
	{ "difficulty", "game.difficulty", offsetof(GameConfig, difficulty), sizeof(((GameConfig*)0)->difficulty),
	  0, 3 },
	{ "collisions", "game.collisions", offsetof(GameConfig, collisions), sizeof(((GameConfig*)0)->collisions),
	  0, 1 },
	{ "craft_jumping", "game.craft_jumping", offsetof(GameConfig, craftJumping),
	  sizeof(((GameConfig*)0)->craftJumping), 0, 1 },
	{ "random_setup", "game.random_setup", offsetof(GameConfig, randomSetup),
	  sizeof(((GameConfig*)0)->randomSetup), 0, 1 },
	{ "handicapping", "game.handicapping", offsetof(GameConfig, battleLengthIndex),
	  sizeof(((GameConfig*)0)->battleLengthIndex), 0, 2 },
	{ "require_password", "game.require_password", offsetof(GameConfig, requirePassword),
	  sizeof(((GameConfig*)0)->requirePassword), 0, 1 },
	{ "in_progress_join", "game.in_progress_join", offsetof(GameConfig, inProgressJoin),
	  sizeof(((GameConfig*)0)->inProgressJoin), 0, 1 },
	{ "craft_selection", "game.craft_selection", offsetof(GameConfig, craftSelection),
	  sizeof(((GameConfig*)0)->craftSelection), 0, 2 },
	{ "locate_players", "game.locate_players", offsetof(GameConfig, locatePlayers),
	  sizeof(((GameConfig*)0)->locatePlayers), 0, 1 },
	{ "craft_waves", "game.craft_waves", offsetof(GameConfig, craftWaves),
	  sizeof(((GameConfig*)0)->craftWaves), 0, 2 },
	{ "mission_time_limit", "game.mission_time_limit", offsetof(GameConfig, missionTimeLimit),
	  sizeof(((GameConfig*)0)->missionTimeLimit), 0, 255 },
	{ "last_time_limit", "game.last_time_limit", offsetof(GameConfig, lastTeamTimeLimitMinutes),
	  sizeof(((GameConfig*)0)->lastTeamTimeLimitMinutes), 0, 255 },
	{ "random_seed", "game.random_seed", offsetof(GameConfig, randomSeed),
	  sizeof(((GameConfig*)0)->randomSeed), 0, 4294967295 },
	{ "password", "game.password", offsetof(GameConfig, password), sizeof(((GameConfig*)0)->password), 1, 1 },
	{ "async_flag", "game.async_flag", offsetof(GameConfig, asyncFlag), sizeof(((GameConfig*)0)->asyncFlag),
	  0, 1 },
	{ "ai_opponents", "game.ai_opponents", offsetof(GameConfig, aiOpponents),
	  sizeof(((GameConfig*)0)->aiOpponents), 0, 1 },
	{ "help_on", "game.help_on", offsetof(GameConfig, helpOn), sizeof(((GameConfig*)0)->helpOn), 0, 1 },
	{ "combat_balance", "game.combat_balance", offsetof(GameConfig, combatBalance),
	  sizeof(((GameConfig*)0)->combatBalance), 0, 3 },
	{ "taunt1", "game.taunt1", offsetof(GameConfig, taunts[0]), sizeof(((GameConfig*)0)->taunts[0]), 1, 1 },
	{ "taunt2", "game.taunt2", offsetof(GameConfig, taunts[1]), sizeof(((GameConfig*)0)->taunts[1]), 1, 1 },
	{ "taunt3", "game.taunt3", offsetof(GameConfig, taunts[2]), sizeof(((GameConfig*)0)->taunts[2]), 1, 1 },
	{ "taunt4", "game.taunt4", offsetof(GameConfig, taunts[3]), sizeof(((GameConfig*)0)->taunts[3]), 1, 1 },
	{ "continue_sequence", "game.continue_sequence", offsetof(GameConfig, continueBattleOrCampaign),
	  sizeof(((GameConfig*)0)->continueBattleOrCampaign), 0, 1 },
};
static AeronConfigFile* g_defaults;
static AeronConfigFile* g_user;
static AeronConfigFile* g_resolved;
static AeronVfs* g_configVfs;
static XvtSettings g_settings;
static XvtSettings g_defaultSettings;
static XvtSceneSettings g_sceneDefaults;
static uint64_t g_generation;
static int g_writable;

const AeronConfigFile* XvtConfig_UserDocument(void) { return g_user; }

const AeronConfigFile* XvtConfig_ResolvedDocument(void) { return g_resolved; }

const XvtSettings* XvtConfig_Settings(void) { return g_resolved ? &g_settings : NULL; }

const XvtSettings* XvtConfig_DefaultSettings(void) { return g_defaults ? &g_defaultSettings : NULL; }

uint64_t XvtConfig_Generation(void) { return g_generation; }

int XvtConfig_CanReplace(void) { return g_defaults != NULL; }

void XvtConfig_Shutdown(void) {
	AeronConfigFile_Destroy(g_resolved);
	AeronConfigFile_Destroy(g_user);
	AeronConfigFile_Destroy(g_defaults);
	g_defaults = g_user = g_resolved = NULL;
	g_configVfs = NULL;
	g_writable = 0;
}

static int XvtConfig_Error(char* error, size_t capacity, const char* message, const char* path) {
	snprintf(error, capacity, "%s: %s", path ? path : "USER/config.yaml", message);
	return 0;
}

enum { XVT_CONFIG_VERSION = 3 };

static int XvtConfig_Version(const AeronConfigFile* document, int minimum, char* error, size_t capacity) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(document, "version");
	if (AeronConfigNode_Type(AeronConfigFile_Root(document)) != AERON_CONFIG_MAP)
		return XvtSettings_NodeError(document, "", "The settings file has an invalid format.", error,
									 capacity);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT ||
		(AeronConfigNode_Int(node, -1) < minimum || AeronConfigNode_Int(node, -1) > XVT_CONFIG_VERSION))
		return XvtSettings_NodeError(document, "version", "This settings file uses an unsupported format.",
									 error, capacity);
	return 1;
}

static int XvtConfig_ApplyDocument(const AeronConfigFile* document, GameConfig* game, char* error,
								   size_t capacity) {
	GameConfig candidate = *game;
	for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); ++i) {
		const XvtConfigField* field = &g_fields[i];
		const AeronConfigNode* node = AeronConfigFile_GetNode(document, field->path);
		uint8_t* destination = (uint8_t*)&candidate + field->offset;
		if (field->string) {
			const char* value = AeronConfigNode_String(node, NULL);
			if (!value || strlen(value) >= field->size)
				return XvtSettings_NodeError(
					document, field->path, "required text is missing, invalid or too long", error, capacity);
			memset(destination, 0, field->size);
			memcpy(destination, value, strlen(value));
		} else {
			int64_t value = AeronConfigNode_Int(node, -1);
			if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > field->maximum)
				return XvtSettings_NodeError(document, field->path,
											 "expected an integer in the supported range", error, capacity);
			if (field->size == 1)
				*destination = (uint8_t)value;
			else {
				uint32_t narrowed = (uint32_t)value;
				memcpy(destination, &narrowed, sizeof(narrowed));
			}
		}
	}
	for (int i = 0; i < 2; ++i)
		if (candidate.windowSize[i] > candidate.screenRes[i])
			candidate.windowSize[i] = candidate.screenRes[i];
	candidate.networkType = NET_TRANSPORT_TCPIP;
	memset(candidate.ipAddress, 0, sizeof(candidate.ipAddress));
	*game = candidate;
	return 1;
}

static int XvtConfig_Validate(const AeronConfigFile* document, XvtSettings* settings, char* error,
							  size_t capacity) {
	GameConfig game = { 0 };
	return XvtConfig_Version(document, XVT_CONFIG_VERSION, error, capacity) &&
		   XvtConfig_ApplyDocument(document, &game, error, capacity) &&
		   XvtSettings_Parse(document, &g_sceneDefaults, settings, error, capacity);
}

int XvtConfig_Apply(GameConfig* game, char* error, size_t capacity) {
	if (!g_resolved || !game)
		return XvtConfig_Error(error, capacity, "configuration is not loaded", NULL);
	return XvtConfig_ApplyDocument(g_resolved, game, error, capacity);
}

static int XvtConfig_RemoveObsoleteKeys(AeronConfigFile* document, AeronConfigError* detail) {
	const char* obsolete[] = { "network.port",     "network.join_address", "game.ipaddress",
							   "game.networktype", "game.phonenumber",     "input.mouse_mode" };
	for (size_t i = 0; i < sizeof(obsolete) / sizeof(obsolete[0]); ++i)
		if (AeronConfigFile_Has(document, obsolete[i]) &&
			!AeronConfigFile_Remove(document, obsolete[i], detail))
			return 0;
	return 1;
}

static int XvtConfig_UpgradeBindings(AeronConfigFile* document, AeronConfigError* detail) {
	int version = (int)AeronConfigFile_GetInt(document, "version", 0);
	if (version == 1 && AeronConfigFile_Has(document, "input.keyboard") &&
		!AeronConfigFile_Remove(document, "input.keyboard", detail))
		return 0;
	if (version < 3) {
		const char* paths[] = { "input.device", "input.gamepad", "input.joystick", "input.controller" };
		for (size_t i = 0; i < sizeof paths / sizeof paths[0]; ++i)
			if (AeronConfigFile_Has(document, paths[i]) &&
				!AeronConfigFile_Remove(document, paths[i], detail))
				return 0;
		for (int button = 1; button <= 20; ++button) {
			char path[32];
			snprintf(path, sizeof path, "game.joybutton%d", button);
			if (AeronConfigFile_Has(document, path) && !AeronConfigFile_Remove(document, path, detail))
				return 0;
		}
		AeronConfigValue empty = { .type = AERON_CONFIG_SEQUENCE };
		if (!AeronConfigFile_SetValue(document, "input.controllers", &empty, detail))
			return 0;
	}
	if (AeronConfigFile_Has(document, "input.gamepad_defaults")) {
		Aeron_LogWarn("xvt.config", "input.gamepad_defaults is shipped-only; ignoring user override");
		if (!AeronConfigFile_Remove(document, "input.gamepad_defaults", detail))
			return 0;
	}
	return AeronConfigFile_SetInt(document, "version", XVT_CONFIG_VERSION, detail);
}

int XvtConfig_UpdateUser(const AeronConfigFile* candidate, int save, char* error, size_t capacity) {
	AeronConfigFile* user_root = NULL;
	AeronConfigFile* updated = NULL;
	AeronConfigFile* resolved = NULL;
	AeronConfigError detail;
	XvtSettings settings;
	int success = 0;
	if (!g_defaults || !g_configVfs)
		return XvtConfig_Error(error, capacity, "valid shipped defaults are required",
							   "RESOURCE/config.yaml");
	if (!XvtConfig_Version(candidate, 1, error, capacity))
		return 0;
	/* The destination always belongs to USER, independently of node provenance. */
	if (!AeronConfigFile_CreateMap(AERON_VFS_ROOT_USER, "config.yaml", &user_root, &detail) ||
		!AeronConfigFile_Overlay(user_root, candidate, &updated, &detail) ||
		!XvtConfig_RemoveObsoleteKeys(updated, &detail) || !XvtConfig_UpgradeBindings(updated, &detail) ||
		!AeronConfigFile_Overlay(g_defaults, updated, &resolved, &detail)) {
		XvtSettings_FileError(&detail, error, capacity);
		goto done;
	}
	if (!XvtKeyboardConfig_Resolve(&g_defaultSettings.keyboard, updated, resolved, error, capacity) ||
		!XvtConfig_Validate(resolved, &settings, error, capacity))
		goto done;
	if (save && !AeronConfigFile_SaveYaml(g_configVfs, updated, &detail)) {
		XvtSettings_FileError(&detail, error, capacity);
		goto done;
	}
	AeronConfigFile_Destroy(g_user);
	AeronConfigFile_Destroy(g_resolved);
	g_user = updated;
	g_resolved = resolved;
	updated = resolved = NULL;
	g_settings = settings;
	g_writable = 1;
	++g_generation;
	success = 1;
done:
	AeronConfigFile_Destroy(user_root);
	AeronConfigFile_Destroy(updated);
	AeronConfigFile_Destroy(resolved);
	return success;
}

int XvtConfig_Replace(char* error, size_t capacity) {
	AeronConfigFile* replacement = NULL;
	AeronConfigError detail;
	int success;
	if (!g_defaults)
		return XvtConfig_Error(error, capacity, "cannot reset without valid shipped defaults",
							   "RESOURCE/config.yaml");
	if (!AeronConfigFile_CreateMap(AERON_VFS_ROOT_USER, "config.yaml", &replacement, &detail) ||
		!AeronConfigFile_SetInt(replacement, "version", XVT_CONFIG_VERSION, &detail)) {
		AeronConfigFile_Destroy(replacement);
		return XvtSettings_FileError(&detail, error, capacity);
	}
	success = XvtConfig_UpdateUser(replacement, 0, error, capacity);
	AeronConfigFile_Destroy(replacement);
	return success;
}

int XvtConfig_Load(AeronVfs* vfs, char* error, size_t capacity) {
	AeronConfigError detail;
	AeronConfigFile* scene_defaults = NULL;
	AeronConfigFile* user = NULL;
	int success;
	XvtConfig_Shutdown();
	XvtKeyboardMapping_SetPolicy(Aeron_DebugUiAvailable() != 0);
	g_configVfs = vfs;
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_RESOURCE, "aeron/scene3d_defaults.yaml",
									&scene_defaults, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	success = AeronSceneSettings_Load(AeronConfigFile_Root(scene_defaults), &g_sceneDefaults.ssao,
									  &g_sceneDefaults.shadows, &g_sceneDefaults.tonemap, &detail);
	AeronConfigFile_Destroy(scene_defaults);
	if (!success)
		return XvtSettings_FileError(&detail, error, capacity);
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_RESOURCE, "config.yaml", &g_defaults, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	if (!XvtConfig_Validate(g_defaults, &g_defaultSettings, error, capacity)) {
		AeronConfigFile_Destroy(g_defaults);
		g_defaults = NULL;
		return 0;
	}
	if (XvtStorage_Probe(AERON_VFS_ROOT_USER, "config.yaml") < 0)
		return XvtConfig_Error(error, capacity, "cannot inspect configuration; file preserved",
							   "USER/config.yaml");
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_USER, "config.yaml", &user, &detail)) {
		if (detail.code == AERON_CONFIG_ERROR_NOT_FOUND)
			return XvtConfig_Replace(error, capacity);
		return XvtSettings_FileError(&detail, error, capacity);
	}
	success = XvtConfig_UpdateUser(user, 0, error, capacity);
	AeronConfigFile_Destroy(user);
	return success;
}

int XvtConfig_Save(char* error, size_t capacity) {
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	return XvtConfig_UpdateUser(g_user, 1, error, capacity);
}

int XvtConfig_SetGameData(const char* path, int save, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	int success;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration is not loaded", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail) ||
		!AeronConfigFile_SetString(updated, "paths.game_data", path, &detail)) {
		AeronConfigFile_Destroy(updated);
		return XvtSettings_FileError(&detail, error, capacity);
	}
	success = XvtConfig_UpdateUser(updated, save, error, capacity);
	AeronConfigFile_Destroy(updated);
	return success;
}

bool XvtConfig_SetFlightRate(bool unlocked, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = unlocked == (XvtConfig_DefaultSettings()->flight_unlocked != 0)
					   ? AeronConfigFile_Remove(updated, "flight.update_rate", &detail)
					   : AeronConfigFile_SetString(updated, "flight.update_rate",
												   unlocked ? "unlocked" : "native", &detail);
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);
	return success;
}

bool XvtConfig_SetSkipIntro(bool enabled, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = enabled == (XvtConfig_DefaultSettings()->skip_intro != 0)
					   ? AeronConfigFile_Remove(updated, "startup.skip_intro", &detail)
					   : AeronConfigFile_SetBool(updated, "startup.skip_intro", enabled, &detail);
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);
	return success;
}

bool XvtConfig_SetTouchControls(int mode, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	const char* value = NULL;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (mode == XVT_TOUCH_CONTROLS_ON)
		value = "on";
	else if (mode == XVT_TOUCH_CONTROLS_OFF)
		value = "off";
	else if (mode != XVT_TOUCH_CONTROLS_AUTOMATIC)
		return XvtConfig_Error(error, capacity, "unsupported touch control mode", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = value ? AeronConfigFile_SetString(updated, "input.touch_controls", value, &detail)
						 : AeronConfigFile_Remove(updated, "input.touch_controls", &detail);
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);
	return success;
}

bool XvtConfig_SetTouchTransparency(float transparency, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!isfinite(transparency) || transparency < 0.0f || transparency > 1.0f)
		return XvtConfig_Error(error, capacity, "expected a transparency from 0 to 1", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = transparency <= 0.0f
					   ? (AeronConfigFile_GetNode(updated, "input.touch_transparency")
							  ? AeronConfigFile_Remove(updated, "input.touch_transparency", &detail) != 0
							  : true)
					   : AeronConfigFile_SetFloat(updated, "input.touch_transparency", transparency, &detail) !=
							 0;
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);

	return success;
}

bool XvtConfig_SetTouchButton(int index, int action, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	char path[96];
	const char* key;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	key = XvtTouchButton_Key(index);
	if (!key[0])
		return XvtConfig_Error(error, capacity, "unknown touch button", NULL);
	if (action != XVT_INPUT_ACTION_NONE && !XvtInputActions_KeyboardBindable((XvtInputAction)action))
		return XvtConfig_Error(error, capacity, "expected a bindable command", NULL);
	if (action == (int)XvtTouchButton_Default(index))
		action = XVT_INPUT_ACTION_NONE;
	snprintf(path, sizeof path, "input.touch_buttons.%s", key);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = true;
	if (action == XVT_INPUT_ACTION_NONE) {
		if (AeronConfigFile_GetNode(updated, path))
			success = AeronConfigFile_Remove(updated, path, &detail) != 0;
	} else {
		success = AeronConfigFile_SetString(updated, path, XvtInputActions_ToName((XvtInputAction)action),
											&detail) != 0;
	}
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);

	return success;
}

bool XvtConfig_ClearTouchButtons(char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = true;
	if (AeronConfigFile_GetNode(updated, "input.touch_buttons"))
		success = AeronConfigFile_Remove(updated, "input.touch_buttons", &detail) != 0;
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);

	return success;
}

bool XvtConfig_SetTouchPlaces(const float* x, const float* y, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!x || !y)
		return XvtConfig_Error(error, capacity, "touch positions are missing", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = true;
	if (AeronConfigFile_GetNode(updated, "input.touch_layout"))
		success = AeronConfigFile_Remove(updated, "input.touch_layout", &detail) != 0;
	for (int i = 0; success && i < XVT_TOUCH_PLACE_COUNT; ++i) {
		char path[96];
		if (x[i] < 0.0f || y[i] < 0.0f)
			continue;
		snprintf(path, sizeof path, "input.touch_layout.%s.x", XvtTouchPlace_Key(i));
		success = AeronConfigFile_SetFloat(updated, path, x[i] < 0.0f ? 0.0 : (x[i] > 1.0f ? 1.0 : x[i]),
										   &detail) != 0;
		if (!success)
			break;
		snprintf(path, sizeof path, "input.touch_layout.%s.y", XvtTouchPlace_Key(i));
		success = AeronConfigFile_SetFloat(updated, path, y[i] < 0.0f ? 0.0 : (y[i] > 1.0f ? 1.0 : y[i]),
										   &detail) != 0;
	}
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);

	return success;
}

bool XvtConfig_ClearTouchPlaces(char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	bool success = true;
	if (AeronConfigFile_GetNode(updated, "input.touch_layout"))
		success = AeronConfigFile_Remove(updated, "input.touch_layout", &detail) != 0;
	if (success)
		success = XvtConfig_UpdateUser(updated, 0, error, capacity);
	else
		XvtSettings_FileError(&detail, error, capacity);
	AeronConfigFile_Destroy(updated);

	return success;
}

int XvtConfig_Write(const GameConfig* game, char* error, size_t capacity) {
	AeronConfigFile* updated = NULL;
	AeronConfigError detail;
	int committed;
	if (!g_writable || !game)
		return XvtConfig_Error(error, capacity, "configuration saving is disabled", NULL);
	if (!AeronConfigFile_Clone(g_user, &updated, &detail))
		return XvtSettings_FileError(&detail, error, capacity);
	for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); ++i) {
		const XvtConfigField* field = &g_fields[i];
		const uint8_t* source = (const uint8_t*)game + field->offset;
		int same, success;
		if (field->string) {
			if (!memchr(source, 0, field->size)) {
				AeronConfigFile_Destroy(updated);
				return XvtConfig_Error(error, capacity, "unterminated game option", field->path);
			}
			same = !strcmp((const char*)source, AeronConfigFile_GetString(g_defaults, field->path, ""));
			success = same || AeronConfigFile_SetString(updated, field->path, (const char*)source, &detail);
		} else {
			uint32_t value = *source;
			if (field->size == 4)
				memcpy(&value, source, sizeof(value));
			if (value > field->maximum) {
				AeronConfigFile_Destroy(updated);
				return XvtConfig_Error(error, capacity, "game option outside supported range", field->path);
			}
			same = value == AeronConfigFile_GetInt(g_defaults, field->path, -1);
			success = same || AeronConfigFile_SetInt(updated, field->path, value, &detail);
		}
		if (success && same && AeronConfigFile_Has(updated, field->path))
			success = AeronConfigFile_Remove(updated, field->path, &detail);
		if (!success) {
			AeronConfigFile_Destroy(updated);
			return XvtSettings_FileError(&detail, error, capacity);
		}
	}
	committed = XvtConfig_UpdateUser(updated, 1, error, capacity);
	AeronConfigFile_Destroy(updated);
	return committed;
}

int XvtConfig_Import(const char* path, char* error, size_t capacity) {
	char line[512];
	AeronFile* file;
	AeronConfigFile* imported = NULL;
	AeronConfigError detail;
	int success = 1, recognized = 0;
	const char* base = strrchr(path, '/');
	int legacy = strcmp(base ? base + 1 : path, "config.cfg") == 0;
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "Configuration saving is disabled", "config.yaml");
	file = XvtStorage_OpenRoot(AERON_VFS_ROOT_ASSET, path, "r");
	if (!file)
		return XvtConfig_Error(error, capacity, "Cannot open legacy configuration", path);
	if (!AeronConfigFile_Clone(g_user, &imported, &detail)) {
		AeronVfs_Close(file);
		return XvtConfig_Error(error, capacity, detail.message, path);
	}
	while (success && XvtFile_Gets(line, sizeof(line), file)) {
		if (!strchr(line, '\n') && strlen(line) == sizeof(line) - 1) {
			success = 0;
			break;
		}
		char* value = line;
		while (*value && !isspace((unsigned char)*value))
			++value;
		if (!*value)
			continue;
		*value++ = 0;
		while (*value == ' ' || *value == '\t')
			++value;
		value[strcspn(value, "\r\n")] = 0;
		for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); ++i) {
			const XvtConfigField* field = &g_fields[i];
			if (strcmp(line, field->legacy))
				continue;
			++recognized;
			if (field->string) {
				success = strlen(value) < field->size &&
						  AeronConfigFile_SetString(imported, field->path, value, &detail);
			} else {
				char* end;
				int64_t number;
				errno = 0;
				number = strtoll(value, &end, 10);
				while (isspace((unsigned char)*end))
					++end;
				success = !errno && end != value && !*end;
				if (legacy && strncmp(line, "joybutton", 9) == 0 && number >= 124 && number <= 229)
					number += 4;
				success = success && number >= 0 && number <= field->maximum &&
						  AeronConfigFile_SetInt(imported, field->path, number, &detail);
			}
			break;
		}
	}
	if (!recognized || AeronVfs_HasError(file))
		success = 0;
	if (!AeronVfs_Close(file))
		success = 0;
	if (!success) {
		AeronConfigFile_Destroy(imported);
		return XvtConfig_Error(error, capacity,
							   "Invalid legacy option or import I/O failure; existing YAML preserved", path);
	}
	int committed = XvtConfig_UpdateUser(imported, 1, error, capacity);
	AeronConfigFile_Destroy(imported);
	return committed;
}

bool XvtConfig_SetKeyboard(const XvtKeyboardBindings* bindings, char* error, size_t capacity) {
	AeronConfigFile* candidate = NULL;
	AeronConfigError detail = { 0 };
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration is not loaded", NULL);
	if (!AeronConfigFile_Clone(g_user, &candidate, &detail) ||
		!XvtKeyboardConfig_Write(candidate, bindings, &detail)) {
		AeronConfigFile_Destroy(candidate);
		return XvtSettings_FileError(&detail, error, capacity);
	}
	bool ok = XvtConfig_UpdateUser(candidate, 0, error, capacity) != 0;
	AeronConfigFile_Destroy(candidate);
	return ok;
}

bool XvtConfig_RestoreKeyboard(char* error, size_t capacity) {
	AeronConfigFile* candidate = NULL;
	AeronConfigError detail = { 0 };
	if (!g_writable)
		return XvtConfig_Error(error, capacity, "configuration is not loaded", NULL);
	if (!AeronConfigFile_Clone(g_user, &candidate, &detail) ||
		!AeronConfigFile_Remove(candidate, "input.keyboard", &detail)) {
		AeronConfigFile_Destroy(candidate);
		return XvtSettings_FileError(&detail, error, capacity);
	}
	bool ok = XvtConfig_UpdateUser(candidate, 0, error, capacity) != 0;
	AeronConfigFile_Destroy(candidate);
	return ok;
}
