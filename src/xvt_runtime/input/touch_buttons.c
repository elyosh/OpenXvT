#include "xvt_runtime/input/touch_buttons.h"

#include "aeron/input.h"

#include <string.h>

typedef struct XvtTouchButtonDesc {
	const char* key;
	const char* label;
	XvtInputAction action;
	int gamepad;
} XvtTouchButtonDesc;

static const XvtTouchButtonDesc k_buttons[XVT_TOUCH_BUTTON_COUNT] = {
	{ "fire", "Fire", XVT_INPUT_ACTION_FIRE_WEAPON, AERON_GAMEPAD_BUTTON_SOUTH },
	{ "front", "Front", XVT_INPUT_ACTION_AUTO_TARGET, AERON_GAMEPAD_BUTTON_EAST },
	{ "next", "Next", XVT_INPUT_ACTION_TARGET_NEXT, AERON_GAMEPAD_BUTTON_WEST },
	{ "attacker", "Atk", XVT_INPUT_ACTION_TARGET_MY_ATTACKER, AERON_GAMEPAD_BUTTON_NORTH },
	{ "weapons", "Wep", XVT_INPUT_ACTION_CYCLE_WEAPON_GROUP, AERON_GAMEPAD_BUTTON_LEFT_SHOULDER },
	{ "rate", "Rate", XVT_INPUT_ACTION_CYCLE_WEAPON_FIRING_MODE, AERON_GAMEPAD_BUTTON_RIGHT_SHOULDER },
	{ "shields", "Shld", XVT_INPUT_ACTION_CYCLE_SHIELD_MODE, AERON_GAMEPAD_BUTTON_DPAD_LEFT },
	{ "weapon_energy", "W+", XVT_INPUT_ACTION_CYCLE_CANNON_RECHARGE_RATE, AERON_GAMEPAD_BUTTON_DPAD_RIGHT },
	{ "shield_energy", "S+", XVT_INPUT_ACTION_CYCLE_SHIELD_RECHARGE_RATE, AERON_GAMEPAD_BUTTON_START },
	{ "throttle_up", "T+", XVT_INPUT_ACTION_THROTTLE_UP, AERON_GAMEPAD_BUTTON_DPAD_UP },
	{ "throttle_down", "T-", XVT_INPUT_ACTION_THROTTLE_DOWN, AERON_GAMEPAD_BUTTON_DPAD_DOWN },
	{ "map", "Map", XVT_INPUT_ACTION_INFO_MAP, AERON_GAMEPAD_BUTTON_LEFT_STICK },
};

static const char* const k_places[XVT_TOUCH_PLACE_COUNT] = {
	"left",        "right",          "fire",           "front",        "next",
	"attacker",    "weapons",        "rate",           "shields",      "weapon_energy",
	"shield_energy", "throttle_up",  "throttle_down",  "map",          "esc",
};

static const char* const k_retired[] = { "mod", "near", "lock", "view", "match", "clear" };

static int XvtTouchButton_Valid(int index) { return index >= 0 && index < XVT_TOUCH_BUTTON_COUNT; }

const char* XvtTouchPlace_Key(int index) {
	if (index < 0 || index >= XVT_TOUCH_PLACE_COUNT)
		return "";

	return k_places[index];
}

const char* XvtTouchButton_Key(int index) {
	if (!XvtTouchButton_Valid(index))
		return "";

	return k_buttons[index].key;
}

const char* XvtTouchButton_Label(int index) {
	if (!XvtTouchButton_Valid(index))
		return "";

	return k_buttons[index].label;
}

XvtInputAction XvtTouchButton_Default(int index) {
	if (!XvtTouchButton_Valid(index))
		return XVT_INPUT_ACTION_NONE;

	return k_buttons[index].action;
}

int XvtTouchButton_Gamepad(int index) {
	if (!XvtTouchButton_Valid(index))
		return -1;

	return k_buttons[index].gamepad;
}

XvtInputAction XvtTouchButton_Resolved(const int* stored, int index) {
	XvtInputAction action;
	if (!XvtTouchButton_Valid(index))
		return XVT_INPUT_ACTION_NONE;
	action = stored && stored[index] > XVT_INPUT_ACTION_NONE ? (XvtInputAction)stored[index]
															 : XvtTouchButton_Default(index);
	if (!XvtInputActions_KeyboardBindable(action))
		action = XvtTouchButton_Default(index);

	return action;
}

int XvtTouchControl_Retired(const char* key) {
	int i;
	if (!key || !key[0])

		return 0;
	for (i = 0; i < (int)(sizeof k_retired / sizeof k_retired[0]); ++i)
		if (!strcmp(key, k_retired[i]))

			return 1;

	return 0;
}
