#include "xvt_app/touch_controls.h"

#include "xvt_app/duo_pose.h"
#include "aeron/aeron.h"
#include "aeron/log.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/font_atlas.h"
#include "aeron/scene/text2d.h"
#include "aeron/scene/ui.h"
#include "xvt_app/settings/settings.h"
#include "xvt_runtime/config/config.h"
#include "xvt_runtime/input/controller_mapping.h"
#include "xvt_runtime/input/touch_buttons.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/port.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
	TOUCH_LEFT = 0,
	TOUCH_RIGHT,
	TOUCH_FIRE,
	TOUCH_FRONT,
	TOUCH_NEXT,
	TOUCH_ATTACKER,
	TOUCH_WEAPONS,
	TOUCH_RATE,
	TOUCH_SHIELDS,
	TOUCH_WEAPON_ENERGY,
	TOUCH_SHIELD_ENERGY,
	TOUCH_THROTTLE_UP,
	TOUCH_THROTTLE_DOWN,
	TOUCH_MAP,
	TOUCH_ESC,
	TOUCH_COUNT
};

enum { TOUCH_PLACE_COUNT_MATCH = 1 / (TOUCH_COUNT == XVT_TOUCH_PLACE_COUNT) };

enum { TOUCH_FINGER_MAX = 8 };

typedef struct TouchControl {
	float x, y, w, h;
	float cx, cy, radius;
	int visible;
} TouchControl;

typedef struct TouchFinger {
	SDL_FingerID id;
	float x, y;
	float origin_x, origin_y;
	float grab_x, grab_y;
	int control;
	int mouse;
	int down;
	int moved;
} TouchFinger;

typedef struct TouchBounds {
	float left, top, right, bottom;
	int content_x, content_y, content_w, content_h;
	int pixel_w, pixel_h;
	int space_x, space_y;
	int ok;
} TouchBounds;

static const int k_button[TOUCH_COUNT] = {
	-1,
	-1,
	SDL_GAMEPAD_BUTTON_SOUTH,
	SDL_GAMEPAD_BUTTON_EAST,
	SDL_GAMEPAD_BUTTON_WEST,
	SDL_GAMEPAD_BUTTON_NORTH,
	SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
	SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
	SDL_GAMEPAD_BUTTON_DPAD_LEFT,
	SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
	SDL_GAMEPAD_BUTTON_START,
	SDL_GAMEPAD_BUTTON_DPAD_UP,
	SDL_GAMEPAD_BUTTON_DPAD_DOWN,
	SDL_GAMEPAD_BUTTON_LEFT_STICK,
	SDL_GAMEPAD_BUTTON_BACK,
};

static const AeronFontAtlas* g_font;
static AeronDrawList2D* g_list;
static AeronRenderTarget* g_target;
static int g_target_w, g_target_h;
static SDL_Joystick* g_joystick;
static SDL_Gamepad* g_gamepad;
static SDL_JoystickID g_id;
static char g_guid[33];
static TouchControl g_controls[TOUCH_COUNT];
static TouchFinger g_fingers[TOUCH_FINGER_MAX];
static TouchBounds g_bounds;
static uint8_t g_logical[TOUCH_COUNT];
static uint8_t g_pulse[TOUCH_COUNT];
static Sint16 g_axis_cache[SDL_GAMEPAD_AXIS_COUNT];
static uint8_t g_axis_sent[SDL_GAMEPAD_AXIS_COUNT];
static uint8_t g_button_cache[SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1];
static uint8_t g_button_sent[SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1];
static int g_ready, g_show_sticks, g_show_esc, g_model_live, g_logged_active;
static int g_block_touch_mouse, g_block_until_release;
static int g_arrange;
static uint64_t g_place_generation;
static float g_norm_x[TOUCH_COUNT];
static float g_norm_y[TOUCH_COUNT];
static float g_opacity = 1.0f;
static uint8_t g_throttle_phase[TOUCH_COUNT];
static uint64_t g_throttle_next[TOUCH_COUNT];
static int g_split_logged;
static XvtDuoSplit g_split;
static int g_bind_open, g_bind_index, g_bind_category;
static size_t g_bind_selected = SIZE_MAX;
static char g_menu_error[256];

static int TouchLeftCluster(int index) {
	return index == TOUCH_LEFT || index == TOUCH_THROTTLE_UP || index == TOUCH_THROTTLE_DOWN ||
		   index == TOUCH_ESC;
}

static void TouchPma(float out[4], float r, float g, float b, float a) {
	out[0] = r * a;
	out[1] = g * a;
	out[2] = b * a;
	out[3] = a;
}

static SDL_Window* TouchWindow(void) {
	SDL_Window* window = SDL_GetKeyboardFocus();
	int count = 0;
	SDL_Window** windows;
	if (window)
		return window;
	windows = SDL_GetWindows(&count);
	if (windows && count > 0)
		window = windows[0];
	SDL_free(windows);

	return window;
}

static int TouchReadBounds(TouchBounds* out) {
	SDL_Window* window = TouchWindow();
	int pixel_w = 0, pixel_h = 0, point_w = 0, point_h = 0;
	AeronRectI content = { 0 };
	SDL_Rect safe;
	float scale_x, scale_y, left, top, right, bottom;
	memset(out, 0, sizeof *out);
	if (!window || !SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h) || pixel_w <= 0 || pixel_h <= 0)
		return 0;
	if (!SDL_GetWindowSize(window, &point_w, &point_h) || point_w <= 0 || point_h <= 0)
		return 0;
	if (!Aeron_GetContentPixelRect(&content) || content.width <= 0 || content.height <= 0)
		return 0;
	scale_x = (float)pixel_w / (float)point_w;
	scale_y = (float)pixel_h / (float)point_h;
	if (g_split.active) {
		left = 0.0f;
		top = (float)g_split.controls_top;
		right = (float)pixel_w;
		bottom = (float)pixel_h;
	} else {
		left = 0.0f;
		top = 0.0f;
		right = (float)content.width;
		bottom = (float)content.height;
	}
	if (SDL_GetWindowSafeArea(window, &safe) && safe.w > 0 && safe.h > 0) {
		float safe_left = (float)safe.x * scale_x;
		float safe_top = (float)safe.y * scale_y;
		float safe_right = (float)(safe.x + safe.w) * scale_x;
		float safe_bottom = (float)(safe.y + safe.h) * scale_y;
		if (!g_split.active) {
			safe_left -= (float)content.x;
			safe_top -= (float)content.y;
			safe_right -= (float)content.x;
			safe_bottom -= (float)content.y;
		}
		if (safe_left > left)
			left = safe_left;
		if (safe_top > top)
			top = safe_top;
		if (safe_right < right)
			right = safe_right;
		if (safe_bottom < bottom)
			bottom = safe_bottom;
	}
	if (!g_split.active) {
		if (left < 0.0f)
			left = 0.0f;
		if (top < 0.0f)
			top = 0.0f;
		if (right > (float)content.width)
			right = (float)content.width;
		if (bottom > (float)content.height)
			bottom = (float)content.height;
	}
	if (right - left < 64.0f || bottom - top < 64.0f)
		return 0;
	out->left = left;
	out->top = top;
	out->right = right;
	out->bottom = bottom;
	out->content_x = content.x;
	out->content_y = content.y;
	out->content_w = content.width;
	out->content_h = content.height;
	out->pixel_w = pixel_w;
	out->pixel_h = pixel_h;
	out->space_x = g_split.active ? 0 : content.x;
	out->space_y = g_split.active ? 0 : content.y;
	out->ok = 1;

	return 1;
}

static void TouchPlaceControl(int index, float x, float y, float w, float h, int visible) {
	TouchControl* control = &g_controls[index];
	control->x = x;
	control->y = y;
	control->w = w;
	control->h = h;
	control->cx = x + w * 0.5f;
	control->cy = y + h * 0.5f;
	control->radius = w < h ? w * 0.5f : h * 0.5f;
	control->visible = visible;
}

static int TouchLayoutFits(const TouchBounds* bounds) {
	float left_edge = bounds->left;
	float right_edge = bounds->right;
	int right_controls = 0;
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		const TouchControl* control = &g_controls[i];
		if (!control->visible)
			continue;
		if (control->x < bounds->left - 0.5f || control->y < bounds->top - 0.5f ||
			control->x + control->w > bounds->right + 0.5f || control->y + control->h > bounds->bottom + 0.5f)
			return 0;
		if (!g_show_sticks)
			continue;
		if (TouchLeftCluster(i)) {
			if (control->x + control->w > left_edge)
				left_edge = control->x + control->w;
		} else {
			right_controls = 1;
			if (control->x < right_edge)
				right_edge = control->x;
		}
	}
	if (g_show_sticks && right_controls && left_edge + 12.0f > right_edge)
		return 0;

	return 1;
}

static int TouchHasCustom(void) {
	for (int i = 0; i < TOUCH_COUNT; ++i)
		if (g_norm_x[i] >= 0.0f && g_norm_y[i] >= 0.0f)
			return 1;

	return 0;
}

static void TouchLoadPlaces(void) {
	const XvtSettings* settings;
	uint64_t generation;
	if (g_arrange)
		return;
	generation = XvtConfig_Generation();
	if (generation == g_place_generation)
		return;
	g_place_generation = generation;
	settings = XvtConfig_Settings();
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		g_norm_x[i] = settings ? settings->touch_place_x[i] : -1.0f;
		g_norm_y[i] = settings ? settings->touch_place_y[i] : -1.0f;
	}
}

static void TouchSetCenter(int index, float cx, float cy, const TouchBounds* bounds) {
	TouchControl* control = &g_controls[index];
	float half_w = control->w * 0.5f;
	float half_h = control->h * 0.5f;
	if (half_w * 2.0f > bounds->right - bounds->left)
		half_w = (bounds->right - bounds->left) * 0.5f;
	if (half_h * 2.0f > bounds->bottom - bounds->top)
		half_h = (bounds->bottom - bounds->top) * 0.5f;
	if (cx - half_w < bounds->left)
		cx = bounds->left + half_w;
	if (cy - half_h < bounds->top)
		cy = bounds->top + half_h;
	if (cx + half_w > bounds->right)
		cx = bounds->right - half_w;
	if (cy + half_h > bounds->bottom)
		cy = bounds->bottom - half_h;
	control->w = half_w * 2.0f;
	control->h = half_h * 2.0f;
	control->cx = cx;
	control->cy = cy;
	control->x = cx - half_w;
	control->y = cy - half_h;
	control->radius = control->w < control->h ? control->w * 0.5f : control->h * 0.5f;
}

static void TouchApplyPlaces(const TouchBounds* bounds) {
	float width = bounds->right - bounds->left;
	float height = bounds->bottom - bounds->top;
	if (width < 1.0f || height < 1.0f)
		return;
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		if (!g_controls[i].visible || g_norm_x[i] < 0.0f || g_norm_y[i] < 0.0f)
			continue;
		TouchSetCenter(i, bounds->left + g_norm_x[i] * width, bounds->top + g_norm_y[i] * height, bounds);
	}
}

static void TouchRememberCenter(int index, const TouchBounds* bounds) {
	float width = bounds->right - bounds->left;
	float height = bounds->bottom - bounds->top;
	if (width < 1.0f || height < 1.0f)
		return;
	g_norm_x[index] = (g_controls[index].cx - bounds->left) / width;
	g_norm_y[index] = (g_controls[index].cy - bounds->top) / height;
	if (g_norm_x[index] < 0.0f)
		g_norm_x[index] = 0.0f;
	if (g_norm_y[index] < 0.0f)
		g_norm_y[index] = 0.0f;
	if (g_norm_x[index] > 1.0f)
		g_norm_x[index] = 1.0f;
	if (g_norm_y[index] > 1.0f)
		g_norm_y[index] = 1.0f;
}

static int TouchButtonIndex(int control);

static int TouchRepeats(int index) {
	int button = TouchButtonIndex(index);
	const XvtSettings* settings;
	XvtInputAction action;
	if (button < 0)
		return 0;
	settings = XvtConfig_Settings();
	action = XvtTouchButton_Resolved(settings ? settings->touch_button : NULL, button);

	return action == XVT_INPUT_ACTION_THROTTLE_UP || action == XVT_INPUT_ACTION_THROTTLE_DOWN ||
		   action == XVT_INPUT_ACTION_THROTTLE_UP_OR_TRACK_MAP_TARGET ||
		   action == XVT_INPUT_ACTION_THROTTLE_DOWN_OR_STOP_TRACKING_MAP_TARGET;
}

static void TouchPlaceRow(const int* ids, int count, float right, float y, float bw, float bh, float gap) {
	float x;
	int i;
	if (count <= 0)

		return;
	x = right - (bw * (float)count + gap * (float)(count - 1));
	for (i = 0; i < count; ++i)
		TouchPlaceControl(ids[i], x + (bw + gap) * (float)i, y, bw, bh, 1);
}

static void TouchPlaceSpan(const int* ids, int count, float left, float right, float y, float bh, float gap) {
	float width = right - left;
	float bw;
	float x;
	float max_bw = bh * 2.4f;
	int i;
	if (count <= 0 || width < 1.0f)

		return;
	bw = (width - gap * (float)(count - 1)) / (float)count;
	if (bw < 1.0f)
		bw = 1.0f;
	if (bw > max_bw)
		bw = max_bw;
	x = left + (width - (bw * (float)count + gap * (float)(count - 1))) * 0.5f;
	for (i = 0; i < count; ++i)
		TouchPlaceControl(ids[i], x + (bw + gap) * (float)i, y, bw, bh, 1);
}

static void TouchPlace(const TouchBounds* bounds, float radius) {
	static const int k_row_top[] = { TOUCH_FRONT, TOUCH_NEXT, TOUCH_ATTACKER, TOUCH_WEAPONS, TOUCH_RATE };
	static const int k_row_low[] = { TOUCH_SHIELDS, TOUCH_WEAPON_ENERGY, TOUCH_SHIELD_ENERGY, TOUCH_MAP };
	float margin = radius * 0.2f;
	float esc_w, esc_h, stick, gap, bw, bh, fire;
	float width, height;
	if (margin < 16.0f)
		margin = 16.0f;
	esc_w = radius * 1.85f;
	esc_h = radius * 0.82f;
	memset(g_controls, 0, sizeof g_controls);
	if (g_show_esc)
		TouchPlaceControl(TOUCH_ESC, bounds->left + margin, bounds->top + margin, esc_w, esc_h, 1);
	if (!g_show_sticks)
		return;
	stick = radius * 2.0f;
	gap = radius * 0.14f;
	if (gap < 8.0f)
		gap = 8.0f;
	bw = radius * 1.2f;
	bh = radius * 0.74f;
	fire = radius * 1.35f;
	width = bounds->right - bounds->left;
	height = bounds->bottom - bounds->top;
	TouchPlaceControl(TOUCH_LEFT, bounds->left + margin, bounds->bottom - margin - stick, stick, stick, 1);
	TouchPlaceControl(TOUCH_RIGHT, bounds->right - margin - stick, bounds->bottom - margin - stick, stick,
					  stick, 1);
	TouchPlaceControl(TOUCH_THROTTLE_UP, g_controls[TOUCH_LEFT].x, g_controls[TOUCH_LEFT].y - gap - bh, bw, bh,
					  1);
	TouchPlaceControl(TOUCH_THROTTLE_DOWN, g_controls[TOUCH_LEFT].x + bw + gap,
					  g_controls[TOUCH_LEFT].y - gap - bh, bw, bh, 1);
	TouchPlaceControl(TOUCH_FIRE, g_controls[TOUCH_RIGHT].cx - fire * 0.5f,
					  g_controls[TOUCH_RIGHT].y - gap - fire, fire, fire, 1);
	if (width > height * 1.35f) {
		float inner_left = g_controls[TOUCH_THROTTLE_DOWN].x + g_controls[TOUCH_THROTTLE_DOWN].w + gap;
		float inner_right = g_controls[TOUCH_FIRE].x - gap;
		float occupied = g_controls[TOUCH_FIRE].y;
		float top_limit = bounds->top + margin;
		float y0, y1;
		if (g_controls[TOUCH_THROTTLE_UP].y < occupied)
			occupied = g_controls[TOUCH_THROTTLE_UP].y;
		if (g_show_esc && g_controls[TOUCH_ESC].visible)
			top_limit = g_controls[TOUCH_ESC].y + g_controls[TOUCH_ESC].h + gap;
		y1 = occupied - gap - bh;
		y0 = y1 - gap - bh;
		if (y0 < top_limit) {
			y0 = top_limit;
			y1 = y0 + bh + gap;
		}
		if (inner_right > inner_left + bh * 4.0f) {
			TouchPlaceSpan(k_row_top, 5, inner_left, inner_right, y0, bh, gap);
			TouchPlaceSpan(k_row_low, 4, inner_left, inner_right, y1, bh, gap);
		} else {
			float row_right = g_controls[TOUCH_RIGHT].x + g_controls[TOUCH_RIGHT].w;
			TouchPlaceRow(k_row_low, 4, row_right, y1, bw, bh, gap);
			TouchPlaceRow(k_row_top, 5, row_right, y0, bw, bh, gap);
		}
	} else {
		float row_right = g_controls[TOUCH_RIGHT].x + g_controls[TOUCH_RIGHT].w;
		float y_low = g_controls[TOUCH_FIRE].y - gap - bh;
		TouchPlaceRow(k_row_low, 4, row_right, y_low, bw, bh, gap);
		TouchPlaceRow(k_row_top, 5, row_right, y_low - gap - bh, bw, bh, gap);
	}
}

static int TouchLayout(void) {
	TouchBounds bounds;
	float short_side, radius;
	int custom;
	TouchLoadPlaces();
	if (!TouchReadBounds(&bounds)) {
		g_bounds.ok = 0;
		return 0;
	}
	g_bounds = bounds;
	short_side = bounds.right - bounds.left;
	if (bounds.bottom - bounds.top < short_side)
		short_side = bounds.bottom - bounds.top;
	radius = short_side * 0.105f;
	if (radius > 170.0f)
		radius = 170.0f;
	if (radius < 96.0f)
		radius = 96.0f;
	custom = TouchHasCustom();
	for (int attempt = 0; attempt < 8; ++attempt) {
		TouchPlace(&bounds, radius);
		TouchApplyPlaces(&bounds);
		if (custom || TouchLayoutFits(&bounds))
			return 1;
		radius *= 0.86f;
	}
	TouchPlace(&bounds, radius);
	TouchApplyPlaces(&bounds);
	if (!g_show_sticks && g_show_esc) {
		TouchControl* esc = &g_controls[TOUCH_ESC];
		if (esc->x < bounds.left)
			esc->x = bounds.left;
		if (esc->y < bounds.top)
			esc->y = bounds.top;
		if (esc->x + esc->w > bounds.right)
			esc->w = bounds.right - esc->x;
		if (esc->y + esc->h > bounds.bottom)
			esc->h = bounds.bottom - esc->y;
		esc->cx = esc->x + esc->w * 0.5f;
		esc->cy = esc->y + esc->h * 0.5f;
		esc->visible = esc->w >= 48.0f && esc->h >= 36.0f;
	}

	return TouchLayoutFits(&bounds) || (g_show_esc && g_controls[TOUCH_ESC].visible);
}

static int TouchMapFinger(float normalized_x, float normalized_y, float* x, float* y) {
	if (!g_bounds.ok && !TouchLayout())
		return 0;
	*x = normalized_x * (float)g_bounds.pixel_w - (float)g_bounds.space_x;
	*y = normalized_y * (float)g_bounds.pixel_h - (float)g_bounds.space_y;

	return 1;
}

static int TouchHit(float x, float y) {
	int best = -1;
	float best_distance = 0.0f;
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		const TouchControl* control = &g_controls[i];
		float dx, dy, distance;
		if (!control->visible)
			continue;
		dx = x - control->cx;
		dy = y - control->cy;
		if (i == TOUCH_LEFT || i == TOUCH_RIGHT) {
			distance = dx * dx + dy * dy;
			if (distance > control->radius * control->radius)
				continue;
		} else if (x < control->x - 12.0f || y < control->y - 12.0f || x > control->x + control->w + 12.0f ||
				   y > control->y + control->h + 12.0f) {
			continue;
		} else {
			distance = dx * dx + dy * dy;
		}
		if (best < 0 || distance < best_distance) {
			best = i;
			best_distance = distance;
		}
	}

	return best;
}

static int TouchFindFinger(SDL_FingerID id) {
	for (int i = 0; i < TOUCH_FINGER_MAX; ++i)
		if (g_fingers[i].down && g_fingers[i].id == id)
			return i;

	return -1;
}

static int TouchMouseFinger(void) {
	for (int i = 0; i < TOUCH_FINGER_MAX; ++i)
		if (g_fingers[i].down && g_fingers[i].mouse)
			return i;

	return -1;
}

static int TouchControlTaken(int control, int self) {
	for (int i = 0; i < TOUCH_FINGER_MAX; ++i)
		if (i != self && g_fingers[i].down && g_fingers[i].control == control)
			return 1;

	return 0;
}

static void TouchSetAxis(int axis, Sint16 value) {
	if (!g_joystick || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT)
		return;
	if (g_axis_sent[axis] && g_axis_cache[axis] == value)
		return;
	if (!SDL_SetJoystickVirtualAxis(g_joystick, axis, value))
		return;
	g_axis_cache[axis] = value;
	g_axis_sent[axis] = 1;
}

static void TouchSetButton(int button, int down) {
	Uint8 value;
	if (!g_joystick || button < 0 || button > SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
		return;
	value = down ? 1 : 0;
	if (g_button_sent[button] && g_button_cache[button] == value)
		return;
	if (!SDL_SetJoystickVirtualButton(g_joystick, button, value))
		return;
	g_button_cache[button] = value;
	g_button_sent[button] = 1;
}

static Sint16 TouchAxisValue(float unit) {
	if (unit > 1.0f)
		unit = 1.0f;
	if (unit < -1.0f)
		unit = -1.0f;

	return (Sint16)(unit * 32767.0f);
}

static void TouchStick(int index, float* x, float* y) {
	const TouchControl* control = &g_controls[index];
	*x = 0.0f;
	*y = 0.0f;
	if (!control->visible || control->radius <= 1.0f)
		return;
	for (int i = 0; i < TOUCH_FINGER_MAX; ++i) {
		float dx, dy, magnitude;
		if (!g_fingers[i].down || g_fingers[i].control != index)
			continue;
		dx = (g_fingers[i].x - control->cx) / control->radius;
		dy = (g_fingers[i].y - control->cy) / control->radius;
		magnitude = sqrtf(dx * dx + dy * dy);
		if (magnitude > 1.0f) {
			dx /= magnitude;
			dy /= magnitude;
		}
		*x = dx;
		*y = dy;
		return;
	}
}

static void TouchWriteOutputs(void) {
	float left_x = 0.0f, left_y = 0.0f, right_x = 0.0f, right_y = 0.0f;
	if (!g_joystick)
		return;
	if (g_arrange) {
		TouchSetAxis(SDL_GAMEPAD_AXIS_LEFTX, 0);
		TouchSetAxis(SDL_GAMEPAD_AXIS_LEFTY, 0);
		TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHTX, 0);
		TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHTY, 0);
		TouchSetAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
		TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
		for (int i = 0; i < TOUCH_COUNT; ++i)
			if (k_button[i] >= 0)
				TouchSetButton(k_button[i], 0);

		return;
	}
	TouchStick(TOUCH_LEFT, &left_x, &left_y);
	TouchStick(TOUCH_RIGHT, &right_x, &right_y);
	TouchSetAxis(SDL_GAMEPAD_AXIS_LEFTX, TouchAxisValue(left_x));
	TouchSetAxis(SDL_GAMEPAD_AXIS_LEFTY, TouchAxisValue(left_y));
	TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHTX, TouchAxisValue(right_x));
	TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHTY, TouchAxisValue(right_y));
	TouchSetAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
	TouchSetAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		int down = 0;
		if (k_button[i] < 0)
			continue;
		if (!g_controls[i].visible) {
			g_logical[i] = 0;
			g_pulse[i] = 0;
			g_throttle_phase[i] = 0;
		} else if (TouchRepeats(i) && g_logical[i]) {
			down = g_throttle_phase[i] == 1;
		} else {
			down = g_logical[i] || g_pulse[i];
		}
		TouchSetButton(k_button[i], down);
	}
}

static void TouchVirtualUpdate(void* userdata) {
	(void)userdata;
	if (!g_joystick)
		return;
	memset(g_pulse, 0, sizeof g_pulse);
	TouchWriteOutputs();
}

static void TouchEscPressed(void) {
	if (!XvtFlightTask_IsActive() || !g_model_live)
		XvtPort_RequestSettings();
}

static void TouchFinishArrange(void) {
	char error[256];
	g_arrange = 0;
	if (!XvtConfig_SetTouchPlaces(g_norm_x, g_norm_y, error, sizeof error))
		Aeron_LogWarn("xvt.input", "%s", error);
	g_place_generation = XvtConfig_Generation();
}

static void TouchHandleFinger(const SDL_Event* event) {
	float x = 0.0f, y = 0.0f;
	int slot, hit;
	int up = event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED;
	if (!TouchMapFinger(event->tfinger.x, event->tfinger.y, &x, &y))
		return;
	if (event->type == SDL_EVENT_FINGER_DOWN) {
		slot = TouchFindFinger(event->tfinger.fingerID);
		if (slot < 0) {
			for (int i = 0; i < TOUCH_FINGER_MAX; ++i) {
				if (!g_fingers[i].down) {
					slot = i;
					break;
				}
			}
		}
		if (slot < 0)
			return;
		memset(&g_fingers[slot], 0, sizeof g_fingers[slot]);
		g_fingers[slot].id = event->tfinger.fingerID;
		g_fingers[slot].x = x;
		g_fingers[slot].y = y;
		g_fingers[slot].down = 1;
		g_fingers[slot].moved = 0;
		g_fingers[slot].origin_x = x;
		g_fingers[slot].origin_y = y;
		g_fingers[slot].control = -1;
		g_fingers[slot].mouse = TouchMouseFinger() < 0;
		hit = TouchHit(x, y);
		if (hit >= 0 && !TouchControlTaken(hit, slot)) {
			g_fingers[slot].control = hit;
			g_fingers[slot].grab_x = x - g_controls[hit].cx;
			g_fingers[slot].grab_y = y - g_controls[hit].cy;
			if (g_fingers[slot].mouse)
				g_block_touch_mouse = 1;
			if (!g_arrange) {
				g_logical[hit] = 1;
				g_pulse[hit] = 1;
				if (TouchRepeats(hit)) {
					g_throttle_phase[hit] = 1;
					g_throttle_next[hit] = Aeron_NowUs() + 70000ull;
				}
				if (hit == TOUCH_ESC)
					TouchEscPressed();
			}
		}
	} else {
		slot = TouchFindFinger(event->tfinger.fingerID);
		if (slot < 0)
			return;
		g_fingers[slot].x = x;
		g_fingers[slot].y = y;
		if (fabsf(x - g_fingers[slot].origin_x) > 18.0f || fabsf(y - g_fingers[slot].origin_y) > 18.0f)
			g_fingers[slot].moved = 1;
		if (g_arrange && g_fingers[slot].control >= 0 && g_bounds.ok) {
			TouchSetCenter(g_fingers[slot].control, x - g_fingers[slot].grab_x, y - g_fingers[slot].grab_y,
						   &g_bounds);
			TouchRememberCenter(g_fingers[slot].control, &g_bounds);
		}
		if (up) {
			if (g_arrange && g_fingers[slot].control == TOUCH_ESC && !g_fingers[slot].moved)
				TouchFinishArrange();
			if (g_fingers[slot].control >= 0)
				g_logical[g_fingers[slot].control] = 0;
			if (g_fingers[slot].mouse) {
				g_block_until_release = g_block_touch_mouse;
				g_block_touch_mouse = 0;
			}
			g_fingers[slot].down = 0;
		}
	}
	TouchWriteOutputs();
}

static bool TouchEventFilter(void* userdata, SDL_Event* event) {
	SDL_MouseID which;
	(void)userdata;
	if (!g_ready)
		return true;
	if (event->type == SDL_EVENT_FINGER_DOWN || event->type == SDL_EVENT_FINGER_MOTION ||
		event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) {
		TouchHandleFinger(event);
		return true;
	}
	if (event->type != SDL_EVENT_MOUSE_MOTION && event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
		event->type != SDL_EVENT_MOUSE_BUTTON_UP)
		return true;
	which = event->type == SDL_EVENT_MOUSE_MOTION ? event->motion.which : event->button.which;
	if (which == SDL_TOUCH_MOUSEID && (g_block_touch_mouse || g_block_until_release)) {
		if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
			g_block_until_release = 0;
		return false;
	}

	return true;
}

static void TouchClearFingers(void) {
	memset(g_fingers, 0, sizeof g_fingers);
	memset(g_logical, 0, sizeof g_logical);
	memset(g_pulse, 0, sizeof g_pulse);
	g_block_touch_mouse = 0;
	g_block_until_release = 0;
}

static int TouchPhysicalPresent(const AeronInputSnapshot* input) {
	if (!input)
		return 0;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		const AeronControllerSnapshot* device = &input->controllers[i];
		if (!device->connected || device->kind != AERON_CONTROLLER_KIND_GAMEPAD)
			continue;
		if (device->instance_id == g_id || SDL_IsJoystickVirtual(device->instance_id))
			continue;
		return 1;
	}

	return 0;
}

static void TouchApplyButtons(XvtControllerProfile* profile) {
	const XvtSettings* settings = XvtConfig_Settings();
	uint8_t owned[AERON_GAMEPAD_BUTTON_COUNT];
	size_t write = 0;
	if (!profile || !settings)
		return;
	memset(owned, 0, sizeof owned);
	for (int i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i) {
		int button = XvtTouchButton_Gamepad(i);
		if (button >= 0 && button < AERON_GAMEPAD_BUTTON_COUNT)
			owned[button] = 1;
	}
	for (size_t i = 0; i < profile->binding_count; ++i) {
		const AeronControllerDigitalSource* source = &profile->bindings[i].source;
		if (source->kind == AERON_CONTROLLER_DIGITAL_BUTTON && source->index < AERON_GAMEPAD_BUTTON_COUNT &&
			owned[source->index])
			continue;
		profile->bindings[write++] = profile->bindings[i];
	}
	profile->binding_count = write;
	for (int i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i) {
		AeronControllerDigitalSource source;
		int button = XvtTouchButton_Gamepad(i);
		if (button < 0 || profile->binding_count >= XVT_CONTROLLER_BINDING_CAP)
			continue;
		memset(&source, 0, sizeof source);
		source.kind = AERON_CONTROLLER_DIGITAL_BUTTON;
		source.index = (uint8_t)button;
		source.threshold = 0.5f;
		profile->bindings[profile->binding_count].source = source;
		profile->bindings[profile->binding_count].action = XvtTouchButton_Resolved(settings->touch_button, i);
		profile->binding_count++;
	}
}

static void TouchInstallLive(int active) {
	const XvtSettings* settings = XvtConfig_Settings();
	XvtControllerOptions live;
	int index;
	if (!settings || !g_guid[0]) {
		g_model_live = 0;
		return;
	}
	if (!active) {
		g_model_live = 0;
		if (!XvtControllerOptions_Equals(&settings->controller, XvtControllerMapping_Options()))
			XvtControllerMapping_SetOptions(&settings->controller);
		return;
	}
	live = settings->controller;
	index = XvtControllerOptions_FindModel(&live, g_guid);
	if (index < 0) {
		AeronControllerSnapshot device;
		char error[192];
		memset(&device, 0, sizeof device);
		device.connected = 1;
		device.kind = AERON_CONTROLLER_KIND_GAMEPAD;
		device.instance_id = g_id;
		memcpy(device.guid, g_guid, sizeof device.guid);
		snprintf(device.name, sizeof device.name, "Touch Controls");
		if (!XvtControllerOptions_AddModel(&live, &device, error, sizeof error)) {
			Aeron_LogWarn("xvt.input", "%s", error);
			g_model_live = 0;
			return;
		}
		index = (int)live.count - 1;
	}
	live.models[index].kind = AERON_CONTROLLER_KIND_GAMEPAD;
	live.models[index].profile = XvtConfig_DefaultSettings()->gamepad_defaults;
	TouchApplyButtons(&live.models[index].profile);
	for (size_t i = 0; i < live.count; ++i) {
		if ((int)i == index)
			continue;
		for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis)
			live.models[i].profile.mapping.axes[axis].source = -1;
	}
	if (!XvtControllerOptions_Equals(&live, XvtControllerMapping_Options()))
		XvtControllerMapping_SetOptions(&live);
	g_model_live = 1;
}

static int TouchAttach(void) {
	static const char name[] = "Touch Controls";
	SDL_VirtualJoystickDesc desc;
	SDL_GUID guid;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.vendor_id = 0x1209;
	desc.product_id = 0x4F58;
	desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
	desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
	desc.nbuttons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
	desc.button_mask = (1u << desc.nbuttons) - 1u;
	desc.name = name;
	desc.Update = TouchVirtualUpdate;
	g_id = SDL_AttachVirtualJoystick(&desc);
	if (!g_id) {
		Aeron_LogWarn("xvt.input", "touch controls were not attached (%s)", SDL_GetError());
		return 0;
	}
	g_joystick = SDL_OpenJoystick(g_id);
	g_gamepad = SDL_OpenGamepad(g_id);
	if (!g_joystick) {
		Aeron_LogWarn("xvt.input", "touch controls did not open (%s)", SDL_GetError());
		SDL_DetachVirtualJoystick(g_id);
		g_id = 0;
		return 0;
	}
	guid = SDL_GetJoystickGUID(g_joystick);
	SDL_GUIDToString(guid, g_guid, (int)sizeof g_guid);
	g_axis_cache[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = SDL_JOYSTICK_AXIS_MIN;
	g_axis_cache[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = SDL_JOYSTICK_AXIS_MIN;
	g_axis_sent[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 1;
	g_axis_sent[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 1;

	return 1;
}

static AeronRenderTarget* TouchEnsureTarget(int width, int height) {
	if (g_target && g_target_w == width && g_target_h == height)
		return g_target;
	if (g_target) {
		Aeron_DestroyRenderTarget(g_target);
		g_target = NULL;
	}
	g_target = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
		.width = width,
		.height = height,
		.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
		.debug_name = "xvt.touch",
	});
	if (!g_target) {
		Aeron_LogWarn("xvt.input", "touch overlay target was not created");
		g_target_w = 0;
		g_target_h = 0;
		return NULL;
	}
	g_target_w = width;
	g_target_h = height;

	return g_target;
}

static void TouchAddControl(const TouchControl* control, int pressed, int esc) {
	float fill[4], border[4];
	float opacity = g_opacity;
	AeronDrawList2DRRect rect;
	if (opacity < 0.0f)
		opacity = 0.0f;
	if (opacity > 1.0f)
		opacity = 1.0f;
	if (pressed)
		TouchPma(fill, 0.55f, 0.06f, 0.08f, 0.88f * opacity);
	else
		TouchPma(fill, 0.015f, 0.02f, 0.03f, (esc ? 0.78f : 0.62f) * opacity);
	if (esc)
		TouchPma(border, 0.92f, 0.08f, 0.1f, opacity);
	else
		TouchPma(border, 0.72f, 0.1f, 0.12f, 0.9f * opacity);
	memset(&rect, 0, sizeof rect);
	rect.dst_x = control->x;
	rect.dst_y = control->y;
	rect.dst_w = control->w;
	rect.dst_h = control->h;
	rect.radius_px = control->radius;
	rect.border_px = esc ? 3.0f : 2.0f;
	rect.soft_px = 1.0f;
	memcpy(rect.fill_top, fill, sizeof fill);
	memcpy(rect.fill_bottom, fill, sizeof fill);
	memcpy(rect.border, border, sizeof border);
	AeronDrawList_AddRRect(g_list, &rect);
}

static void TouchAddLabel(const TouchControl* control, const char* label) {
	AeronTextStyle style;
	float scale, width, height;
	if (!g_font || !g_font->cell_h || !label)
		return;
	memset(&style, 0, sizeof style);
	style.font = g_font;
	style.filter = AERON_BLIT2D_FILTER_NEAREST;
	style.tint[0] = style.tint[1] = style.tint[2] = 0.95f;
	style.tint[3] = g_opacity;
	style.shadow[3] = 0.9f * g_opacity;
	scale = (control->h * 0.42f) / (float)g_font->cell_h;
	style.scale = scale;
	width = AeronText_MeasureWidth(&style, label, -1);
	if (width > control->w - 10.0f && width > 1.0f)
		style.scale = scale * ((control->w - 10.0f) / width);
	height = (float)g_font->cell_h * style.scale;
	AeronText_Draw(g_list, &style, control->cx, control->cy - height * 0.5f, AERON_TEXT_CENTER, label, -1,
				   NULL);
}

static int TouchButtonIndex(int control) {
	if (control < 0 || control >= TOUCH_COUNT || k_button[control] < 0)
		return -1;
	for (int i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i)
		if (XvtTouchButton_Gamepad(i) == k_button[control])
			return i;

	return -1;
}

static const char* TouchLabel(int control) {
	int button = TouchButtonIndex(control);
	const XvtSettings* settings;
	XvtInputAction action;
	if (control == TOUCH_ESC)
		return g_arrange ? "Done" : "ESC";
	if (button < 0)
		return NULL;
	settings = XvtConfig_Settings();
	action = XvtTouchButton_Resolved(settings ? settings->touch_button : NULL, button);
	if (action == XvtTouchButton_Default(button))
		return XvtTouchButton_Label(button);

	return XvtInputActions_DisplayName(action);
}

static void TouchPaint(void) {
	const XvtSettings* settings = XvtConfig_Settings();
	g_opacity = 1.0f;
	if (settings && isfinite(settings->touch_transparency))
		g_opacity = 1.0f - settings->touch_transparency;
	if (g_opacity < 0.0f)
		g_opacity = 0.0f;
	if (g_opacity > 1.0f)
		g_opacity = 1.0f;
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		const TouchControl* control = &g_controls[i];
		int pressed;
		if (!control->visible)
			continue;
		pressed = g_logical[i] != 0;
		if (i == TOUCH_LEFT || i == TOUCH_RIGHT) {
			float dx = 0.0f, dy = 0.0f;
			TouchControl puck;
			TouchAddControl(control, 0, 0);
			TouchStick(i, &dx, &dy);
			puck = *control;
			puck.w = control->radius * 0.85f;
			puck.h = puck.w;
			puck.x = control->cx + dx * control->radius * 0.48f - puck.w * 0.5f;
			puck.y = control->cy + dy * control->radius * 0.48f - puck.h * 0.5f;
			puck.radius = puck.w * 0.5f;
			TouchAddControl(&puck, 1, 0);
			continue;
		}
		TouchAddControl(control, pressed, i == TOUCH_ESC);
		TouchAddLabel(control, TouchLabel(i));
	}
	if (g_arrange && g_bounds.ok) {
		TouchControl hint;
		memset(&hint, 0, sizeof hint);
		hint.x = g_bounds.left;
		hint.y = g_bounds.top + 8.0f;
		hint.w = g_bounds.right - g_bounds.left;
		hint.h = 42.0f;
		hint.cx = (g_bounds.left + g_bounds.right) * 0.5f;
		hint.cy = hint.y + hint.h * 0.5f;
		TouchAddLabel(&hint, "Drag a control. Tap ESC when finished.");
	}
}

static void TouchSwapchainRender(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass,
								 AeronRenderTarget* target, int target_width, int target_height,
								 void* userdata) {
	AeronRectI full = { 0, 0, target_width, target_height };
	(void)userdata;
	Aeron_SetViewport(render_pass, &full);
	Aeron_SetScissor(render_pass, &full);
	AeronDrawList_RenderIntoPass(g_list, command_buffer, render_pass, target);
}

void XvtTouchControls_Init(const struct AeronFontAtlas* font) {
	if (g_ready)
		return;
	g_font = font;
	g_logged_active = -1;
	g_arrange = 0;
	g_place_generation = 0;
	for (int i = 0; i < TOUCH_COUNT; ++i) {
		g_norm_x[i] = -1.0f;
		g_norm_y[i] = -1.0f;
	}
	g_list = AeronDrawList_Create(256);
	if (!g_list)
		Aeron_LogWarn("xvt.input", "touch overlay draw list was not created");
	if (!TouchAttach())
		return;
	SDL_SetEventFilter(TouchEventFilter, NULL);
	g_ready = 1;
	Aeron_LogInfo("xvt.input", "touch controls attached");
}

void XvtTouchControls_Shutdown(void) {
	g_arrange = 0;
	XvtPort_SetSimulationHeld(0);
	if (!g_ready && !g_id && !g_list && !g_target)

		return;
	SDL_SetEventFilter(NULL, NULL);
	if (g_gamepad) {
		SDL_CloseGamepad(g_gamepad);
		g_gamepad = NULL;
	}
	if (g_joystick) {
		SDL_CloseJoystick(g_joystick);
		g_joystick = NULL;
	}
	if (g_id) {
		SDL_DetachVirtualJoystick(g_id);
		g_id = 0;
	}
	if (g_target) {
		Aeron_DestroyRenderTarget(g_target);
		g_target = NULL;
	}
	if (g_list) {
		AeronDrawList_Destroy(g_list);
		g_list = NULL;
	}
	g_font = NULL;
	g_ready = 0;
	g_guid[0] = 0;
	TouchClearFingers();
}

void XvtTouchControls_Sync(const AeronInputSnapshot* input) {
	const XvtSettings* settings = XvtConfig_Settings();
	int mode = settings ? settings->touch_controls : XVT_TOUCH_CONTROLS_AUTOMATIC;
	int physical, active, menu;
	if (!g_ready) {
		XvtPort_SetSimulationHeld(0);

		return;
	}
	if (mode != XVT_TOUCH_CONTROLS_ON && mode != XVT_TOUCH_CONTROLS_OFF)
		mode = XVT_TOUCH_CONTROLS_AUTOMATIC;
	physical = TouchPhysicalPresent(input);
	if (!XvtDuoPose_Query(&g_split))
		g_split.active = 0;
	if (g_split.active && g_split.pixel_h > g_split.game_bottom)
		Aeron_SetPresentationInsets(0, 0, 0, g_split.pixel_h - g_split.game_bottom);
	else {
		g_split.active = 0;
		Aeron_SetPresentationInsets(0, 0, 0, 0);
	}
	if (g_split_logged != g_split.active) {
		int point_w = 0;
		int point_h = 0;
		AeronRectI content = { 0 };
		SDL_Window* window = TouchWindow();
		if (window)
			SDL_GetWindowSize(window, &point_w, &point_h);
		Aeron_GetContentPixelRect(&content);
		Aeron_LogInfo("xvt.input",
					  "duo laptop split %s game_bottom %d controls_top %d window %dx%d pt %dx%d px content %d %d %d %d",
					  g_split.active ? "active" : "inactive", g_split.game_bottom, g_split.controls_top, point_w,
					  point_h, g_split.pixel_w, g_split.pixel_h, content.x, content.y, content.width,
					  content.height);
		g_split_logged = g_split.active;
	}
	menu = XvtSettingsMenu_Open();
	active = mode == XVT_TOUCH_CONTROLS_ON || (mode == XVT_TOUCH_CONTROLS_AUTOMATIC && !physical);
	g_show_sticks = !menu && (active || g_arrange);
	g_show_esc = !menu && (active || !physical || g_arrange);
	XvtPort_SetSimulationHeld(g_arrange);
	{
		uint64_t now = Aeron_NowUs();
		for (int i = 0; i < TOUCH_COUNT; ++i) {
			if (g_arrange || !g_logical[i] || !TouchRepeats(i)) {
				if (!g_logical[i])
					g_throttle_phase[i] = 0;
				continue;
			}
			if (!g_throttle_phase[i]) {
				g_throttle_phase[i] = 1;
				g_throttle_next[i] = now + 70000ull;
				continue;
			}
			if (now < g_throttle_next[i])
				continue;
			g_throttle_phase[i] = g_throttle_phase[i] == 1 ? 2 : 1;
			g_throttle_next[i] = now + (g_throttle_phase[i] == 1 ? 55000ull : 35000ull);
		}
	}
	if (!g_show_sticks && !g_show_esc)
		TouchClearFingers();
	else {
		for (int i = 0; i < TOUCH_COUNT; ++i) {
			int visible = (i == TOUCH_ESC && g_show_esc) || (i != TOUCH_ESC && g_show_sticks);
			if (visible)
				continue;
			g_logical[i] = 0;
			g_pulse[i] = 0;
			for (int finger = 0; finger < TOUCH_FINGER_MAX; ++finger)
				if (g_fingers[finger].control == i)
					g_fingers[finger].control = -1;
		}
	}
	TouchLayout();
	TouchWriteOutputs();
	if (menu)
		return;
	TouchInstallLive(active);
	if (g_logged_active != active) {
		Aeron_LogInfo("xvt.input", "touch flight controls %s", active ? "active" : "inactive");
		g_logged_active = active;
	}
}

void XvtTouchControls_Draw(void) {
	int direct, overlay, list_w, list_h;
	AeronCommandBuffer* command;
	if (!g_ready || !g_list || XvtSettingsMenu_Open() || (!g_show_esc && !g_show_sticks))
		return;
	if (!TouchLayout() || g_bounds.content_w <= 0 || g_bounds.content_h <= 0)
		return;
	overlay = g_split.active && g_bounds.pixel_w > 0 && g_bounds.pixel_h > 0;
	list_w = overlay ? g_bounds.pixel_w : g_bounds.content_w;
	list_h = overlay ? g_bounds.pixel_h : g_bounds.content_h;
	direct = !overlay && Aeron_CanRenderDirectToSwapchain(g_bounds.content_w, g_bounds.content_h);
	if (direct || overlay) {
		if (g_target) {
			Aeron_DestroyRenderTarget(g_target);
			g_target = NULL;
			g_target_w = 0;
			g_target_h = 0;
		}
		AeronDrawList_Begin(g_list, NULL, list_w, list_h, AERON_DRAWLIST2D_LOAD, NULL);
	} else {
		AeronRenderTarget* target = TouchEnsureTarget(g_bounds.content_w, g_bounds.content_h);
		if (!target)
			return;
		AeronDrawList_Begin(g_list, target, g_bounds.content_w, g_bounds.content_h, AERON_DRAWLIST2D_CLEAR,
							NULL);
	}
	TouchPaint();
	command = Aeron_AcquireCommandBuffer();
	if (!command)
		return;
	if (direct || overlay) {
		if (!AeronDrawList_Prepare(g_list, command)) {
			Aeron_CancelCommandBuffer(command);
			return;
		}
		if (!Aeron_SubmitCommandBuffer(command))
			return;
		Aeron_SubmitSwapchainRenderLayer(&(AeronSwapchainRenderLayerDesc) {
			.callback = TouchSwapchainRender,
			.required_width = list_w,
			.required_height = list_h,
			.drawable_overlay = overlay,
			.debug_label = "xvt.touch",
		});
		return;
	}
	AeronDrawList_Render(g_list, command);
	if (!Aeron_SubmitCommandBuffer(command))
		return;
	{
		int logical_w = 0, logical_h = 0;
		if (!Aeron_GetLogicalSize(&logical_w, &logical_h)) {
			logical_w = g_bounds.content_w;
			logical_h = g_bounds.content_h;
		}
		Aeron_SubmitTextureLayer(&(AeronTextureLayerDesc) {
			.texture = Aeron_RenderTargetGetTexture(g_target),
			.logical_rect = { 0, 0, logical_w, logical_h },
			.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
			.color_space = AERON_COLOR_SPACE_LINEAR_DISPLAY,
		});
	}
}

void XvtTouchControls_DrawMenu(struct AeronUiContext* ui, const AeronInputSnapshot* input) {
	static const char* const modes[] = { "Automatic", "On", "Off" };
	const XvtSettings* settings = XvtConfig_Settings();
	int mode = settings ? settings->touch_controls : XVT_TOUCH_CONTROLS_AUTOMATIC;
	int next, shown, physical;
	if (!ui)
		return;
	if (mode != XVT_TOUCH_CONTROLS_ON && mode != XVT_TOUCH_CONTROLS_OFF)
		mode = XVT_TOUCH_CONTROLS_AUTOMATIC;
	next = mode;
	AeronUi_Header(ui, "Touch Controls");
	if (AeronUi_SegmentedSelector(ui, "touch-controls", &next, modes, 3) && next != mode) {
		char error[256];
		if (XvtConfig_SetTouchControls(next, error, sizeof error)) {
			g_menu_error[0] = 0;
			mode = next;
		} else {
			snprintf(g_menu_error, sizeof g_menu_error, "%s", error);
		}
	}
	if (g_menu_error[0])
		AeronUi_Error(ui, g_menu_error);
	shown = mode;
	physical = TouchPhysicalPresent(input);
	if (shown == XVT_TOUCH_CONTROLS_ON || (shown == XVT_TOUCH_CONTROLS_AUTOMATIC && !physical))
		AeronUi_Help(ui, "Sticks are on. ESC opens this menu.");
	else if (physical)
		AeronUi_Help(ui,
					 "A gamepad is connected, so the sticks stay hidden. Choose On to show them. The "
					 "gamepad keeps its buttons.");
	else
		AeronUi_Help(ui, "Sticks are off. ESC stays on screen while no gamepad is connected.");
	AeronUi_Help(ui, "Automatic shows the sticks and ESC unless a gamepad is connected. On keeps the "
					 "sticks available with a gamepad. Off hides the sticks. In the Duo laptop pose the "
					 "game stays on the upper half and these controls fill the lower half.");
	AeronUi_Header(ui, "Buttons");
	AeronUi_Help(ui, "Each button can send another command. ESC stays the menu button. Hold T+ or T- "
					 "to change speed gradually.");
	if (AeronUi_Button(ui, "Move Controls")) {
		g_arrange = 1;
		XvtSettingsMenu_RequestClose();
	}
	AeronUi_Help(ui, "Move Controls lets you drag each control. The mission stays paused until you tap ESC.");
	{
		const XvtSettings* opacity_settings = XvtConfig_Settings();
		int transparency = 0;
		if (opacity_settings && isfinite(opacity_settings->touch_transparency))
			transparency = (int)(opacity_settings->touch_transparency * 100.0f + 0.5f);
		if (transparency < 0)
			transparency = 0;
		if (transparency > 100)
			transparency = 100;
		if (AeronUi_SliderInt(ui, "Transparency", &transparency, 0, 100, 5, "%d%%")) {
			char error[256];
			if (XvtConfig_SetTouchTransparency((float)transparency / 100.0f, error, sizeof error))
				g_menu_error[0] = 0;
			else
				snprintf(g_menu_error, sizeof g_menu_error, "%s", error);
		}
	}
	AeronUi_Help(ui, "Transparency fades the on-screen controls. Zero is solid.");
	if (AeronUi_Button(ui, "Reset Control Positions")) {
		char error[256];
		for (int i = 0; i < TOUCH_COUNT; ++i) {
			g_norm_x[i] = -1.0f;
			g_norm_y[i] = -1.0f;
		}
		if (XvtConfig_ClearTouchPlaces(error, sizeof error))
			g_place_generation = XvtConfig_Generation();
		else
			snprintf(g_menu_error, sizeof g_menu_error, "%s", error);
	}
	for (int i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i) {
		char label[160];
		XvtInputAction action = XvtTouchButton_Resolved(settings ? settings->touch_button : NULL, i);
		snprintf(label, sizeof label, "%s  %s##touch-button-%s", XvtTouchButton_Label(i),
				 XvtInputActions_DisplayName(action), XvtTouchButton_Key(i));
		if (AeronUi_Button(ui, label)) {
			g_bind_open = 1;
			g_bind_index = i;
			g_bind_category = (int)XvtInputActions_Category(action);
			g_bind_selected = SIZE_MAX;
		}
	}
	if (AeronUi_Button(ui, "Restore Button Defaults")) {
		char error[256];
		if (XvtConfig_ClearTouchButtons(error, sizeof error))
			g_menu_error[0] = 0;
		else
			snprintf(g_menu_error, sizeof g_menu_error, "%s", error);
	}
	if (g_bind_open && g_bind_index >= 0 && g_bind_index < XVT_TOUCH_BUTTON_COUNT &&
		AeronUi_BeginModal(ui, XvtTouchButton_Label(g_bind_index), &g_bind_open,
						   &(AeronUiWindowDesc) { .width_ref = 760.0f, .height_ref = 720.0f, .centered = 1 })) {
		static const char* const categories[] = { "Weapons", "Targets", "Throttle", "View",
												  "Info",    "System",  "Comms" };
		AeronUiListItem items[XVT_INPUT_ACTION_COUNT];
		size_t count = 0;
		if (AeronUi_SegmentedSelector(ui, "touch-command-category", &g_bind_category, categories,
									  XVT_INPUT_ACTION_CATEGORY_COUNT))
			g_bind_selected = SIZE_MAX;
		for (int action = XVT_INPUT_ACTION_NONE + 1; action < XVT_INPUT_ACTION_COUNT; ++action) {
			if (!XvtInputActions_KeyboardBindable((XvtInputAction)action) ||
				(int)XvtInputActions_Category((XvtInputAction)action) != g_bind_category)
				continue;
			items[count++] = (AeronUiListItem) {
				.id = (uint64_t)action,
				.label = XvtInputActions_DisplayName((XvtInputAction)action),
			};
		}
		if ((AeronUi_ListBox(ui, "Commands", items, count, &g_bind_selected, 420.0f) &
			 AERON_UI_LIST_ACTIVATED) &&
			g_bind_selected < count) {
			char error[256];
			if (XvtConfig_SetTouchButton(g_bind_index, (int)items[g_bind_selected].id, error, sizeof error)) {
				g_menu_error[0] = 0;
				g_bind_open = 0;
			} else {
				snprintf(g_menu_error, sizeof g_menu_error, "%s", error);
			}
		}
		if (AeronUi_Button(ui, "Cancel"))
			g_bind_open = 0;
		AeronUi_EndModal(ui);
	}
}

void XvtTouchControls_Omit(AeronInputSnapshot* copy, const AeronInputSnapshot* input) {
	if (!copy)
		return;
	if (!input) {
		memset(copy, 0, sizeof *copy);
		return;
	}
	*copy = *input;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i)
		if (XvtTouchControls_IsVirtual(copy->controllers[i].instance_id))
			copy->controllers[i].connected = 0;
}

int XvtTouchControls_IsVirtual(uint32_t instance_id) {
	return g_id != 0 && instance_id == g_id;
}
