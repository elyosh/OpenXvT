#ifndef XVT_RUNTIME_CONFIG_SETTINGS_H
#define XVT_RUNTIME_CONFIG_SETTINGS_H

#include "aeron/compat/dplay_directory.h"
#include "aeron/config_file.h"
#include "aeron/input.h"
#include "aeron/scene/settings.h"
#include "aeron/temporal.h"
#include "xvt_runtime/config/mouse_config.h"
#include "xvt_runtime/input/controller_options.h"
#include "xvt_runtime/input/touch_buttons.h"
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
	int enabled, clustered, cluster_depth_slices, cluster_debug;
	float scale, range_scale, min_distance, spec_weight, diffuse_wrap, contrib_cap;
} XvtPointLightSettings;

typedef struct XvtLightingSettings {
	float intensity, spec_mul, wrap;
	int spec_geom_adapt;
	float ambient[3];
} XvtLightingSettings;

enum { XVT_SKY_STARS, XVT_SKY_CUBE };

typedef struct XvtSkySettings {
	int enabled, mode;
	char path[XVT_PATH_CAPACITY];
	float exposure, star_brightness;
} XvtSkySettings;

typedef struct XvtHyperspaceSettings {
	float travel_speed, rotation_speed, noise_scale, brightness, highlight_strength;
	float focal_length, twist, cap_radius, cap_falloff;
	float mesh_ambient_strength, mesh_environment_roughness, mesh_key_strength;
	float dark_color[3], body_color[3], highlight_color[3], cap_color[3];
} XvtHyperspaceSettings;

typedef struct XvtMotionBlurSettings {
	int quality, camera_blur, pause_keep_blur, velocity_viz, fsr_direct_motion;
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

typedef enum XvtTouchControlsMode {
	XVT_TOUCH_CONTROLS_AUTOMATIC = 0,
	XVT_TOUCH_CONTROLS_ON = 1,
	XVT_TOUCH_CONTROLS_OFF = 2
} XvtTouchControlsMode;

typedef struct XvtSettings {
	int skip_intro;
	int flight_unlocked;
	/* XvtTouchControlsMode. A missing input.touch_controls key means automatic. */
	int touch_controls;
	/* 0 is the shipped solid look. 1 hides the controls. A missing key means 0. */
	float touch_transparency;
	/* Command buttons. Missing config keeps the shipped 12. ESC is not a slot. */
	XvtTouchButtonSlot touch_buttons[XVT_TOUCH_BUTTON_CAP];
	/* Center of a dragged stick or ESC, as a fraction of the touch area.
	 * Negative keeps the shipped position. */
	float touch_fixed_x[XVT_TOUCH_FIXED_COUNT];
	float touch_fixed_y[XVT_TOUCH_FIXED_COUNT];
	/* Missing key means 0.08. */
	float touch_stick_deadzone;
	/* Missing key means 1. */
	float touch_stick_sensitivity;
	float touch_fixed_scale[XVT_TOUCH_FIXED_COUNT];
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
