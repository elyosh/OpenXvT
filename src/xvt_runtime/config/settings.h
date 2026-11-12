#ifndef XVT_RUNTIME_CONFIG_SETTINGS_H
#define XVT_RUNTIME_CONFIG_SETTINGS_H

#include "aeron/compat/dplay_directory.h"
#include "aeron/config_file.h"
#include "aeron/input.h"
#include "aeron/scene/settings.h"
#include "aeron/temporal.h"
#include "xvt_runtime/config/mouse_config.h"
#include "xvt_runtime/input/controller_options.h"
#include "xvt_runtime/input/keyboard_mapping.h"
#include "xvt_runtime/storage/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XvtModelSettings {
	float smooth_angle_degrees;
	float opt_emissive_strength, opt_projectile_emissive_strength, engine_emissive_strength;
} XvtModelSettings;

typedef struct XvtPointLightSettings {
	int enabled, cluster_depth_slices, cluster_debug;
	float scale, range_scale;
} XvtPointLightSettings;

typedef struct XvtLightingSettings {
	int spec_geom_adapt;
	float ambient[3];
} XvtLightingSettings;

typedef struct XvtSkySettings {
	int enabled;
	float exposure, star_brightness;
} XvtSkySettings;

typedef struct XvtHyperspaceSettings {
	float travel_speed, rotation_speed, noise_scale, brightness, highlight_strength;
	float focal_length, twist, cap_radius, cap_falloff;
	float mesh_ambient_strength, mesh_environment_roughness, mesh_key_strength;
	float dark_color[3], body_color[3], highlight_color[3], cap_color[3];
} XvtHyperspaceSettings;

typedef struct XvtMotionBlurSettings {
	int quality, camera_blur, pause_keep_blur, velocity_viz;
	float shutter;
} XvtMotionBlurSettings;

typedef struct XvtPresentationSettings {
	int vsync_divisor, hdr_output;
	/* Negative gamma follows the platform default; zero selects piecewise sRGB. */
	float sdr_gamma;
	/* Zero follows the OS reference white. */
	float paper_white_nits;
} XvtPresentationSettings;

typedef struct XvtSceneSettings {
	AeronSceneSsaoSettings ssao;
	AeronSceneShadowSettings shadows;
	AeronSceneTonemapSettings tonemap;
} XvtSceneSettings;

typedef struct XvtRenderSettings {
	int cockpit_undither;
	XvtSceneSettings scene;
	int msaa_samples;
	AeronTemporalMode temporal_mode;
	float temporal_sharpness;
	XvtMotionBlurSettings motion_blur;
	XvtPresentationSettings presentation;
	int anisotropic;
	float max_anisotropy;
	XvtModelSettings models;
	XvtPointLightSettings point_lights;
	XvtLightingSettings lighting;
	XvtSkySettings sky;
	XvtHyperspaceSettings hyperspace;
	float bloom_intensity, explosion_emissive_strength;
} XvtRenderSettings;

typedef struct XvtSettings {
	int skip_intro;
	int flight_unlocked;
	char game_data[XVT_PATH_CAPACITY];
	char ui_font[XVT_PATH_CAPACITY];
	char lobby_url[AERON_DPLAY_DIRECTORY_URL_CAPACITY];
	int fullscreen;
	XvtControllerOptions controller;
	XvtControllerProfile gamepad_defaults;
	XvtKeyboardBindings keyboard;
	XvtMouseOptions mouse;
	XvtRenderSettings render;
} XvtSettings;

/* Layers game/user render overrides over the required Aeron scene defaults. */
int XvtSettings_Parse(const AeronConfigFile* document, const XvtSceneSettings* scene_defaults,
					  XvtSettings* out, char* error, size_t capacity);
int XvtSettings_NodeError(const AeronConfigFile* document, const char* path, const char* message, char* error,
						  size_t capacity);
int XvtSettings_FileError(const AeronConfigError* detail, char* error, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
