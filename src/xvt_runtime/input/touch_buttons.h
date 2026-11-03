#ifndef XVT_RUNTIME_INPUT_TOUCH_BUTTONS_H
#define XVT_RUNTIME_INPUT_TOUCH_BUTTONS_H

#include "xvt_runtime/input/actions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shipped command buttons, in overlay order. ESC is not one of them. */
enum { XVT_TOUCH_BUTTON_COUNT = 13 };
/* Present command buttons cannot exceed this. Slots 0..12 keep the shipped positions. */
enum { XVT_TOUCH_BUTTON_CAP = 32 };

enum {
	XVT_TOUCH_FIXED_LEFT = 0,
	XVT_TOUCH_FIXED_RIGHT,
	XVT_TOUCH_FIXED_ESC,
	XVT_TOUCH_FIXED_COUNT
};

/* Overlay controls. FIRE..THROTTLE_FULL are slots 0..12. EXTRA begins at slot 13. */
enum {
	XVT_TOUCH_CONTROL_LEFT = 0,
	XVT_TOUCH_CONTROL_RIGHT,
	XVT_TOUCH_CONTROL_FIRE,
	XVT_TOUCH_CONTROL_FRONT,
	XVT_TOUCH_CONTROL_NEXT,
	XVT_TOUCH_CONTROL_ATTACKER,
	XVT_TOUCH_CONTROL_WEAPONS,
	XVT_TOUCH_CONTROL_RATE,
	XVT_TOUCH_CONTROL_SHIELDS,
	XVT_TOUCH_CONTROL_WEAPON_ENERGY,
	XVT_TOUCH_CONTROL_SHIELD_ENERGY,
	XVT_TOUCH_CONTROL_THROTTLE_ONE_THIRD,
	XVT_TOUCH_CONTROL_THROTTLE_TWO_THIRDS,
	XVT_TOUCH_CONTROL_MAP,
	XVT_TOUCH_CONTROL_THROTTLE_FULL,
	XVT_TOUCH_CONTROL_ESC,
	XVT_TOUCH_CONTROL_EXTRA,
	XVT_TOUCH_CONTROL_COUNT = XVT_TOUCH_CONTROL_EXTRA + (XVT_TOUCH_BUTTON_CAP - XVT_TOUCH_BUTTON_COUNT)
};

typedef struct XvtTouchButtonSlot {
	int present;
	int action;
	/* Negative coordinates keep the shipped position. */
	float x, y;
	float scale;
} XvtTouchButtonSlot;

const char* XvtTouchButton_Key(int index);
const char* XvtTouchButton_Label(int index);
XvtInputAction XvtTouchButton_Default(int index);
int XvtTouchButton_Gamepad(int index);
/* Gamepad button for a slot, or -1 when that slot has to report its command directly.
 * ESC keeps Back, so the last slots do not fit in the gamepad button set. */
int XvtTouchButton_Pad(int slot);
int XvtTouchButton_SlotForKey(const char* key);
const char* XvtTouchFixed_Key(int index);
int XvtTouchFixed_Index(const char* key);
int XvtTouchControl_Slot(int control);
int XvtTouchControl_Fixed(int control);
void XvtTouchButtons_Reset(XvtTouchButtonSlot* slots);
/* Slots 0..12 present and the rest empty. Actions may still be rebound. */
int XvtTouchButtons_ShippedPresence(const XvtTouchButtonSlot* slots);
int XvtTouchButtons_PresentCount(const XvtTouchButtonSlot* slots);
/* Nonzero for a control key removed from the shipped layout. Saved configs
 * may still name it. Callers skip the key instead of failing the parse. */
int XvtTouchControl_Retired(const char* key);

#ifdef __cplusplus
}
#endif

#endif
