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

static const int k_extra_pad[] = {
	AERON_GAMEPAD_BUTTON_GUIDE,         AERON_GAMEPAD_BUTTON_RIGHT_STICK,    AERON_GAMEPAD_BUTTON_MISC1,
	AERON_GAMEPAD_BUTTON_RIGHT_PADDLE1, AERON_GAMEPAD_BUTTON_LEFT_PADDLE1,   AERON_GAMEPAD_BUTTON_RIGHT_PADDLE2,
	AERON_GAMEPAD_BUTTON_LEFT_PADDLE2,  AERON_GAMEPAD_BUTTON_TOUCHPAD,       AERON_GAMEPAD_BUTTON_MISC2,
	AERON_GAMEPAD_BUTTON_MISC3,         AERON_GAMEPAD_BUTTON_MISC4,          AERON_GAMEPAD_BUTTON_MISC5,
	AERON_GAMEPAD_BUTTON_MISC6,
};

static const char* const k_fixed[XVT_TOUCH_FIXED_COUNT] = { "left", "right", "esc" };

static const char* const k_retired[] = { "mod", "near", "lock", "view", "match", "clear" };

static int XvtTouchButton_Valid(int index) { return index >= 0 && index < XVT_TOUCH_BUTTON_COUNT; }

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

int XvtTouchButton_Pad(int slot) {
	int extra;
	if (XvtTouchButton_Valid(slot))

		return XvtTouchButton_Gamepad(slot);
	extra = slot - XVT_TOUCH_BUTTON_COUNT;
	if (extra < 0 || extra >= (int)(sizeof k_extra_pad / sizeof k_extra_pad[0]))

		return -1;

	return k_extra_pad[extra];
}

int XvtTouchButton_SlotForKey(const char* key) {
	int slot;
	if (!key || !key[0])

		return -1;
	for (slot = 0; slot < XVT_TOUCH_BUTTON_COUNT; ++slot)
		if (!strcmp(key, XvtTouchButton_Key(slot)))

			return slot;

	return -1;
}

const char* XvtTouchFixed_Key(int index) {
	if (index < 0 || index >= XVT_TOUCH_FIXED_COUNT)

		return "";

	return k_fixed[index];
}

int XvtTouchFixed_Index(const char* key) {
	int index;
	if (!key || !key[0])

		return -1;
	for (index = 0; index < XVT_TOUCH_FIXED_COUNT; ++index)
		if (!strcmp(key, k_fixed[index]))

			return index;

	return -1;
}

int XvtTouchControl_Slot(int control) {
	if (control >= XVT_TOUCH_CONTROL_FIRE && control <= XVT_TOUCH_CONTROL_MAP)

		return control - XVT_TOUCH_CONTROL_FIRE;
	if (control >= XVT_TOUCH_CONTROL_EXTRA && control < XVT_TOUCH_CONTROL_COUNT)

		return XVT_TOUCH_BUTTON_COUNT + (control - XVT_TOUCH_CONTROL_EXTRA);

	return -1;
}

int XvtTouchControl_Fixed(int control) {
	if (control == XVT_TOUCH_CONTROL_LEFT)

		return XVT_TOUCH_FIXED_LEFT;
	if (control == XVT_TOUCH_CONTROL_RIGHT)

		return XVT_TOUCH_FIXED_RIGHT;
	if (control == XVT_TOUCH_CONTROL_ESC)

		return XVT_TOUCH_FIXED_ESC;

	return -1;
}

void XvtTouchButtons_Reset(XvtTouchButtonSlot* slots) {
	int i;
	if (!slots)

		return;
	memset(slots, 0, sizeof(XvtTouchButtonSlot) * XVT_TOUCH_BUTTON_CAP);
	for (i = 0; i < XVT_TOUCH_BUTTON_CAP; ++i) {
		slots[i].x = -1.0f;
		slots[i].y = -1.0f;
	}
	for (i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i) {
		slots[i].present = 1;
		slots[i].action = (int)XvtTouchButton_Default(i);
	}
}

int XvtTouchButtons_ShippedPresence(const XvtTouchButtonSlot* slots) {
	int i;
	if (!slots)

		return 0;
	for (i = 0; i < XVT_TOUCH_BUTTON_COUNT; ++i)
		if (!slots[i].present)

			return 0;
	for (; i < XVT_TOUCH_BUTTON_CAP; ++i)
		if (slots[i].present)

			return 0;

	return 1;
}

int XvtTouchButtons_PresentCount(const XvtTouchButtonSlot* slots) {
	int count = 0;
	int i;
	if (!slots)

		return 0;
	for (i = 0; i < XVT_TOUCH_BUTTON_CAP; ++i)
		if (slots[i].present)
			++count;

	return count;
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
