#include "xvt_runtime/config/settings.h"
#include "xvt_runtime/config/controller_config.h"
#include "xvt_runtime/config/keyboard_config.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum XvtSettingType {
	XVT_SETTING_BOOL,
	XVT_SETTING_INT,
	XVT_SETTING_FLOAT,
	XVT_SETTING_STRING
} XvtSettingType;

typedef struct XvtSettingField {
	const char* path;
	size_t offset;
	XvtSettingType type;
	double minimum, maximum;
} XvtSettingField;

#define SETTING_BOOL(path, member) { path, offsetof(XvtSettings, member), XVT_SETTING_BOOL, 0, 1 }
#define SETTING_INT(path, member, low, high)                                                                 \
	{ path, offsetof(XvtSettings, member), XVT_SETTING_INT, low, high }
#define SETTING_FLOAT(path, member, low, high)                                                               \
	{ path, offsetof(XvtSettings, member), XVT_SETTING_FLOAT, low, high }
#define SETTING_STRING(path, member)                                                                         \
	{ path, offsetof(XvtSettings, member), XVT_SETTING_STRING, 0, sizeof(((XvtSettings*)0)->member) }

static const XvtSettingField g_fields[] = {
	SETTING_BOOL("startup.skip_intro", skip_intro),
	SETTING_STRING("paths.game_data", game_data),
	SETTING_STRING("ui.font", ui_font),
	SETTING_STRING("network.lobby_url", lobby_url),
	SETTING_INT("presentation.vsync_divisor", render.presentation.vsync_divisor, 1, 2),
	SETTING_BOOL("presentation.hdr_output", render.presentation.hdr_output),
	SETTING_INT("render.msaa_samples", render.msaa_samples, 1, 8),
	SETTING_BOOL("render.cockpit_undither", render.cockpit_undither),
	SETTING_FLOAT("render.temporal_upscaling.sharpness", render.temporal_sharpness, 0, 1),
	SETTING_BOOL("render.motion_blur.camera_blur", render.motion_blur.camera_blur),
	SETTING_BOOL("render.motion_blur.pause_keep_blur", render.motion_blur.pause_keep_blur),
	SETTING_BOOL("render.motion_blur.velocity_viz", render.motion_blur.velocity_viz),
	SETTING_BOOL("render.motion_blur.fsr_direct_motion", render.motion_blur.fsr_direct_motion),
	SETTING_INT("render.motion_blur.quality", render.motion_blur.quality, 0, 2),
	SETTING_FLOAT("render.motion_blur.shutter", render.motion_blur.shutter, 0, 1),
	SETTING_BOOL("texture_filtering.anisotropic", render.anisotropic),
	SETTING_FLOAT("texture_filtering.max_anisotropy", render.max_anisotropy, 1, 16),
	SETTING_FLOAT("models.smooth_angle_degrees", render.models.smooth_angle_degrees, 0, 180),
	SETTING_FLOAT("models.opt_emissive_strength", render.models.opt_emissive_strength, 0, FLT_MAX),
	SETTING_FLOAT("models.opt_projectile_emissive_strength", render.models.opt_projectile_emissive_strength,
				  0, FLT_MAX),
	SETTING_FLOAT("models.engine_emissive_strength", render.models.engine_emissive_strength, 0, FLT_MAX),
	SETTING_BOOL("point_lights.enabled", render.point_lights.enabled),
	SETTING_BOOL("point_lights.clustered", render.point_lights.clustered),
	SETTING_BOOL("point_lights.cluster_debug", render.point_lights.cluster_debug),
	SETTING_INT("point_lights.cluster_depth_slices", render.point_lights.cluster_depth_slices, 4, 64),
	SETTING_FLOAT("point_lights.scale", render.point_lights.scale, 0, FLT_MAX),
	SETTING_FLOAT("point_lights.range_scale", render.point_lights.range_scale, FLT_MIN, FLT_MAX),
	SETTING_FLOAT("point_lights.min_distance", render.point_lights.min_distance, FLT_MIN, FLT_MAX),
	SETTING_FLOAT("point_lights.spec_weight", render.point_lights.spec_weight, 0, FLT_MAX),
	SETTING_FLOAT("point_lights.diffuse_wrap", render.point_lights.diffuse_wrap, 0, 1),
	SETTING_FLOAT("point_lights.contrib_cap", render.point_lights.contrib_cap, 0, FLT_MAX),
	SETTING_FLOAT("lighting.intensity", render.lighting.intensity, 0, FLT_MAX),
	SETTING_FLOAT("lighting.spec_mul", render.lighting.spec_mul, 0, FLT_MAX),
	SETTING_FLOAT("lighting.wrap", render.lighting.wrap, 0, 1),
	SETTING_BOOL("lighting.spec_geom_adapt", render.lighting.spec_geom_adapt),
	SETTING_FLOAT("lighting.ambient_r", render.lighting.ambient[0], 0, FLT_MAX),
	SETTING_FLOAT("lighting.ambient_g", render.lighting.ambient[1], 0, FLT_MAX),
	SETTING_FLOAT("lighting.ambient_b", render.lighting.ambient[2], 0, FLT_MAX),
	SETTING_BOOL("skybox.enabled", render.sky.enabled),
	SETTING_STRING("skybox.path", render.sky.path),
	SETTING_FLOAT("skybox.exposure", render.sky.exposure, 0, FLT_MAX),
	SETTING_FLOAT("skybox.star_brightness", render.sky.star_brightness, 0, FLT_MAX),
	SETTING_FLOAT("hyperspace_tunnel.travel_speed", render.hyperspace.travel_speed, FLT_MIN, 64),
	SETTING_FLOAT("hyperspace_tunnel.rotation_speed", render.hyperspace.rotation_speed, -8, 8),
	SETTING_FLOAT("hyperspace_tunnel.noise_scale", render.hyperspace.noise_scale, 0.125, 8),
	SETTING_FLOAT("hyperspace_tunnel.brightness", render.hyperspace.brightness, 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.highlight_strength", render.hyperspace.highlight_strength, 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.focal_length", render.hyperspace.focal_length, 0.25, 4),
	SETTING_FLOAT("hyperspace_tunnel.twist", render.hyperspace.twist, -2, 2),
	SETTING_FLOAT("hyperspace_tunnel.cap_radius", render.hyperspace.cap_radius, 0.001, 1),
	SETTING_FLOAT("hyperspace_tunnel.cap_falloff", render.hyperspace.cap_falloff, 0.1, 64),
	SETTING_FLOAT("hyperspace_tunnel.mesh_ambient_strength", render.hyperspace.mesh_ambient_strength, 0, 4),
	SETTING_FLOAT("hyperspace_tunnel.mesh_environment_roughness",
				  render.hyperspace.mesh_environment_roughness, 0, 1),
	SETTING_FLOAT("hyperspace_tunnel.mesh_key_strength", render.hyperspace.mesh_key_strength, 0, 4),
	SETTING_FLOAT("hyperspace_tunnel.dark_color_r", render.hyperspace.dark_color[0], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.dark_color_g", render.hyperspace.dark_color[1], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.dark_color_b", render.hyperspace.dark_color[2], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.body_color_r", render.hyperspace.body_color[0], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.body_color_g", render.hyperspace.body_color[1], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.body_color_b", render.hyperspace.body_color[2], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.highlight_color_r", render.hyperspace.highlight_color[0], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.highlight_color_g", render.hyperspace.highlight_color[1], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.highlight_color_b", render.hyperspace.highlight_color[2], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.cap_color_r", render.hyperspace.cap_color[0], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.cap_color_g", render.hyperspace.cap_color[1], 0, 16),
	SETTING_FLOAT("hyperspace_tunnel.cap_color_b", render.hyperspace.cap_color[2], 0, 16),
	SETTING_FLOAT("bloom.intensity", render.bloom_intensity, 0, FLT_MAX),
	SETTING_FLOAT("effects.explosion_genus_emissive_strength", render.explosion_emissive_strength, 0,
				  FLT_MAX),
};

#undef SETTING_BOOL
#undef SETTING_INT
#undef SETTING_FLOAT
#undef SETTING_STRING

static const char* XvtSettings_RootName(AeronVfsRoot root) {
	switch (root) {
		case AERON_VFS_ROOT_RESOURCE:
			return "RESOURCE";
		case AERON_VFS_ROOT_USER:
			return "USER";
		case AERON_VFS_ROOT_TEMP:
			return "TEMP";
		default:
			return "ASSET";
	}
}

int XvtSettings_FileError(const AeronConfigError* detail, char* error, size_t capacity) {
	snprintf(error, capacity, "%s/%s:%d:%d: %s", XvtSettings_RootName(detail->root), detail->path,
			 detail->line, detail->column, detail->message);
	return 0;
}

int XvtSettings_NodeError(const AeronConfigFile* document, const char* path, const char* message, char* error,
						  size_t capacity) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(document, path);
	const char* source;
	if (!node)
		node = AeronConfigFile_Root(document);
	source = AeronConfigNode_SourcePath(node);
	snprintf(error, capacity, "%s/%s:%d:%d: %s: %s", XvtSettings_RootName(AeronConfigNode_SourceRoot(node)),
			 source ? source : "config.yaml", AeronConfigNode_Line(node), AeronConfigNode_Column(node), path,
			 message);
	return 0;
}

static int XvtSettings_ReadField(const AeronConfigFile* document, const XvtSettingField* field,
								 XvtSettings* settings, char* error, size_t capacity) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(document, field->path);
	AeronConfigNodeType type = AeronConfigNode_Type(node);
	unsigned char* destination = (unsigned char*)settings + field->offset;
	const char* problem = "missing required setting or incorrect value type";
	if (field->type == XVT_SETTING_STRING && type == AERON_CONFIG_STRING) {
		const char* value = AeronConfigNode_String(node, "");
		if (strlen(value) < (size_t)field->maximum) {
			memcpy(destination, value, strlen(value) + 1);
			return 1;
		}
		problem = "text exceeds the supported length";
	} else if (field->type == XVT_SETTING_BOOL && type == AERON_CONFIG_BOOL) {
		int value = AeronConfigNode_Bool(node, 0);
		memcpy(destination, &value, sizeof(value));
		return 1;
	} else if (field->type == XVT_SETTING_INT && type == AERON_CONFIG_INT) {
		int64_t value = AeronConfigNode_Int(node, 0);
		if ((double)value >= field->minimum && (double)value <= field->maximum) {
			int integer = (int)value;
			memcpy(destination, &integer, sizeof(integer));
			return 1;
		}
		problem = "integer outside the supported range";
	} else if (field->type == XVT_SETTING_FLOAT && (type == AERON_CONFIG_FLOAT || type == AERON_CONFIG_INT)) {
		double value = AeronConfigNode_Float(node, 0);
		if (isfinite(value) && value >= field->minimum && value <= field->maximum) {
			float number = (float)value;
			memcpy(destination, &number, sizeof(number));
			return 1;
		}
		problem = "number must be finite and within the supported range";
	}
	return XvtSettings_NodeError(document, field->path, problem, error, capacity);
}

static int XvtSettings_ReadChoice(const AeronConfigFile* document, const char* path,
								  const char* const* choices, size_t count, int* out, char* error,
								  size_t capacity) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(document, path);
	const char* value = AeronConfigNode_String(node, NULL);
	if (value) {
		for (size_t i = 0; i < count; ++i) {
			if (!strcmp(value, choices[i])) {
				*out = (int)i;
				return 1;
			}
		}
	}
	return XvtSettings_NodeError(document, path, "missing or unsupported option", error, capacity);
}

static int XvtSettings_ReadDisplay(const AeronConfigFile* document, XvtPresentationSettings* out, char* error,
								   size_t capacity) {
	const char* gamma_path = "presentation.sdr_gamma";
	const char* white_path = "presentation.paper_white_nits";
	const AeronConfigNode* gamma = AeronConfigFile_GetNode(document, gamma_path);
	const AeronConfigNode* white = AeronConfigFile_GetNode(document, white_path);
	const char* name = AeronConfigNode_String(gamma, "");
	double value = AeronConfigNode_Float(gamma, -1);
	if (!strcmp(name, "auto"))
		out->sdr_gamma = -1.0f;
	else if (!strcmp(name, "srgb"))
		out->sdr_gamma = 0.0f;
	else if (value == 2.2 || !strcmp(name, "2.2"))
		out->sdr_gamma = 2.2f;
	else if (value == 2.4 || !strcmp(name, "2.4"))
		out->sdr_gamma = 2.4f;
	else
		return XvtSettings_NodeError(document, gamma_path, "expected auto, srgb, 2.2 or 2.4", error,
									 capacity);
	name = AeronConfigNode_String(white, "");
	value = AeronConfigNode_Float(white, -1);
	if (!strcmp(name, "auto"))
		out->paper_white_nits = 0.0f;
	else if (isfinite(value) && value > 0 && value <= FLT_MAX)
		out->paper_white_nits = (float)value;
	else
		return XvtSettings_NodeError(document, white_path, "expected auto or a positive finite luminance",
									 error, capacity);
	return 1;
}

int XvtSettings_Parse(const AeronConfigFile* document, const XvtSceneSettings* scene_defaults,
					  XvtSettings* out, char* error, size_t capacity) {
	XvtSettings candidate = { 0 };
	XvtTouchButtons_Reset(candidate.touch_buttons);
	for (int place = 0; place < XVT_TOUCH_FIXED_COUNT; ++place) {
		candidate.touch_fixed_x[place] = -1.0f;
		candidate.touch_fixed_y[place] = -1.0f;
	}
	AeronConfigError detail;
	static const char* const flight_rates[] = { "native", "unlocked" };
	static const char* const window_modes[] = { "windowed", "fullscreen" };
	static const char* const temporal_modes[] = { "off", "native_aa", "quality", "balanced", "performance" };
	static const char* const sky_modes[] = { "stars", "cube", "procedural" };
	int temporal_mode;
	if (!document || !scene_defaults || !out)
		return XvtSettings_NodeError(document, "", "configuration document and output are required", error,
									 capacity);
	for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); ++i)
		if (!XvtSettings_ReadField(document, &g_fields[i], &candidate, error, capacity))
			return 0;
	if (!XvtSettings_ReadChoice(document, "flight.update_rate", flight_rates, 2, &candidate.flight_unlocked,
								error, capacity) ||
		!XvtSettings_ReadChoice(document, "video.window_mode", window_modes, 2, &candidate.fullscreen, error,
								capacity) ||
		!XvtSettings_ReadChoice(document, "render.temporal_upscaling.mode", temporal_modes, 5, &temporal_mode,
								error, capacity) ||
		!XvtSettings_ReadChoice(document, "skybox.mode", sky_modes, 3, &candidate.render.sky.mode, error,
								capacity) ||
		!XvtSettings_ReadDisplay(document, &candidate.render.presentation, error, capacity))
		return 0;
	candidate.render.temporal_mode = (AeronTemporalMode)temporal_mode;
	if (candidate.render.sky.mode == 2)
		candidate.render.sky.mode = XVT_SKY_STARS;
	if (candidate.render.msaa_samples != 1 && candidate.render.msaa_samples != 2 &&
		candidate.render.msaa_samples != 4 && candidate.render.msaa_samples != 8)
		return XvtSettings_NodeError(document, "render.msaa_samples", "expected 1, 2, 4 or 8", error,
									 capacity);
	if (candidate.render.sky.mode == XVT_SKY_CUBE && !candidate.render.sky.path[0])
		return XvtSettings_NodeError(document, "skybox.path", "cube mode requires a path", error, capacity);
	candidate.render.scene = *scene_defaults;
	if (!AeronSceneSettings_Overlay(AeronConfigFile_GetNode(document, "render"), &candidate.render.scene.ssao,
									&candidate.render.scene.shadows, &candidate.render.scene.tonemap,
									&detail))
		return XvtSettings_FileError(&detail, error, capacity);
	if (!XvtKeyboardConfig_Read(document, &candidate.keyboard, error, capacity) ||
		!XvtControllerConfig_Parse(document, &candidate.controller, error, capacity) ||
		!XvtControllerConfig_ReadProfile(document, "input.gamepad_defaults", AERON_CONTROLLER_KIND_GAMEPAD,
										 &candidate.gamepad_defaults, error, capacity))
		return 0;
	if (!XvtMouseConfig_Parse(document, &candidate.mouse, error, capacity))
		return 0;
	const AeronConfigNode* touch = AeronConfigFile_GetNode(document, "input.touch_controls");
	if (touch) {
		const char* value = AeronConfigNode_String(touch, "");
		if (!strcmp(value, "automatic"))
			candidate.touch_controls = XVT_TOUCH_CONTROLS_AUTOMATIC;
		else if (!strcmp(value, "on"))
			candidate.touch_controls = XVT_TOUCH_CONTROLS_ON;
		else if (!strcmp(value, "off"))
			candidate.touch_controls = XVT_TOUCH_CONTROLS_OFF;
		else
			return XvtSettings_NodeError(document, "input.touch_controls", "expected automatic, on or off",
										 error, capacity);
	}
	{
		const AeronConfigNode* transparency = AeronConfigFile_GetNode(document, "input.touch_transparency");
		if (transparency) {
			double value = AeronConfigNode_Float(transparency, NAN);
			if (!isfinite(value) || value < 0.0 || value > 1.0)
				return XvtSettings_NodeError(document, "input.touch_transparency",
											 "expected a transparency from 0 to 1", error, capacity);
			candidate.touch_transparency = (float)value;
		}
	}
	{
		const AeronConfigNode* buttons = AeronConfigFile_GetNode(document, "input.touch_buttons");
		int listed = 0;
		if (buttons && AeronConfigNode_Type(buttons) == AERON_CONFIG_SEQUENCE) {
			listed = 1;
			for (int slot = 0; slot < XVT_TOUCH_BUTTON_CAP; ++slot) {
				candidate.touch_buttons[slot].present = 0;
				candidate.touch_buttons[slot].action = XVT_INPUT_ACTION_NONE;
				candidate.touch_buttons[slot].x = -1.0f;
				candidate.touch_buttons[slot].y = -1.0f;
			}
			for (size_t i = 0; i < AeronConfigNode_SequenceCount(buttons); ++i) {
				const AeronConfigNode* item = AeronConfigNode_SequenceGet(buttons, i);
				const AeronConfigNode* slot_node;
				const AeronConfigNode* action_node;
				const AeronConfigNode* x_node;
				const AeronConfigNode* y_node;
				int slot;
				XvtInputAction action;
				if (!item || AeronConfigNode_Type(item) != AERON_CONFIG_MAP)
					return XvtSettings_NodeError(document, "input.touch_buttons", "expected a button", error,
												 capacity);
				slot_node = AeronConfigNode_MapGet(item, "slot");
				action_node = AeronConfigNode_MapGet(item, "action");
				slot = (int)AeronConfigNode_Int(slot_node, -1);
				action = XvtInputActions_FromName(AeronConfigNode_String(action_node, ""));
				if (slot < 0 || slot >= XVT_TOUCH_BUTTON_CAP)
					return XvtSettings_NodeError(document, "input.touch_buttons", "unknown touch button", error,
												 capacity);
				if (candidate.touch_buttons[slot].present)
					return XvtSettings_NodeError(document, "input.touch_buttons", "repeated touch button", error,
												 capacity);
				if (!XvtInputActions_KeyboardBindable(action))
					return XvtSettings_NodeError(document, "input.touch_buttons",
												 "expected a bindable command", error, capacity);
				candidate.touch_buttons[slot].present = 1;
				candidate.touch_buttons[slot].action = (int)action;
				x_node = AeronConfigNode_MapGet(item, "x");
				y_node = AeronConfigNode_MapGet(item, "y");
				if (x_node || y_node) {
					double x = AeronConfigNode_Float(x_node, NAN);
					double y = AeronConfigNode_Float(y_node, NAN);
					if (!isfinite(x) || !isfinite(y) || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0)
						return XvtSettings_NodeError(document, "input.touch_buttons",
													 "expected a position from 0 to 1", error, capacity);
					candidate.touch_buttons[slot].x = (float)x;
					candidate.touch_buttons[slot].y = (float)y;
				}
			}
		} else if (buttons) {
			if (AeronConfigNode_Type(buttons) != AERON_CONFIG_MAP)
				return XvtSettings_NodeError(document, "input.touch_buttons", "expected a button map", error,
											 capacity);
			for (size_t i = 0; i < AeronConfigNode_MapCount(buttons); ++i) {
				const char* key = AeronConfigNode_MapKeyAt(buttons, i);
				const char* name = AeronConfigNode_String(AeronConfigNode_MapValueAt(buttons, i), "");
				XvtInputAction action = XvtInputActions_FromName(name);
				int slot = XvtTouchButton_SlotForKey(key);
				if (slot < 0) {
					if (XvtTouchControl_Retired(key))
						continue;
					return XvtSettings_NodeError(document, "input.touch_buttons", "unknown touch button", error,
												 capacity);
				}
				if (!XvtInputActions_KeyboardBindable(action))
					return XvtSettings_NodeError(document, "input.touch_buttons",
												 "expected a bindable command", error, capacity);
				candidate.touch_buttons[slot].action = (int)action;
			}
		}
		const AeronConfigNode* layout = AeronConfigFile_GetNode(document, "input.touch_layout");
		if (layout) {
			if (AeronConfigNode_Type(layout) != AERON_CONFIG_MAP)
				return XvtSettings_NodeError(document, "input.touch_layout", "expected a control map", error,
											 capacity);
			for (size_t i = 0; i < AeronConfigNode_MapCount(layout); ++i) {
				const char* key = AeronConfigNode_MapKeyAt(layout, i);
				const AeronConfigNode* value = AeronConfigNode_MapValueAt(layout, i);
				const AeronConfigNode* x_node;
				const AeronConfigNode* y_node;
				double x, y;
				int slot = XvtTouchButton_SlotForKey(key);
				int fixed = XvtTouchFixed_Index(key);
				if (slot < 0 && fixed < 0) {
					if (XvtTouchControl_Retired(key))
						continue;
					return XvtSettings_NodeError(document, "input.touch_layout", "unknown touch control", error,
												 capacity);
				}
				if (slot >= 0 && listed)
					continue;
				if (!value || AeronConfigNode_Type(value) != AERON_CONFIG_MAP)
					return XvtSettings_NodeError(document, "input.touch_layout", "expected a position", error,
												 capacity);
				x_node = AeronConfigNode_MapGet(value, "x");
				y_node = AeronConfigNode_MapGet(value, "y");
				x = AeronConfigNode_Float(x_node, NAN);
				y = AeronConfigNode_Float(y_node, NAN);
				if (!isfinite(x) || !isfinite(y) || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0)
					return XvtSettings_NodeError(document, "input.touch_layout",
												 "expected a position from 0 to 1", error, capacity);
				if (fixed >= 0) {
					candidate.touch_fixed_x[fixed] = (float)x;
					candidate.touch_fixed_y[fixed] = (float)y;
				} else {
					candidate.touch_buttons[slot].x = (float)x;
					candidate.touch_buttons[slot].y = (float)y;
				}
			}
		}
	}
	*out = candidate;
	return 1;
}
