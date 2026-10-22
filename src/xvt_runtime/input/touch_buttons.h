#ifndef XVT_RUNTIME_INPUT_TOUCH_BUTTONS_H
#define XVT_RUNTIME_INPUT_TOUCH_BUTTONS_H

#include "xvt_runtime/input/actions.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { XVT_TOUCH_BUTTON_COUNT = 12 };
/* On-screen controls that can be dragged, including the sticks and ESC.
 * The order matches the touch overlay. */
enum { XVT_TOUCH_PLACE_COUNT = 15 };

const char* XvtTouchButton_Key(int index);
const char* XvtTouchPlace_Key(int index);
const char* XvtTouchButton_Label(int index);
XvtInputAction XvtTouchButton_Default(int index);
int XvtTouchButton_Gamepad(int index);
/* stored[index] == XVT_INPUT_ACTION_NONE keeps the shipped command. */
XvtInputAction XvtTouchButton_Resolved(const int* stored, int index);
/* Nonzero for a control key removed from the shipped layout. Saved configs
 * may still name it. Callers skip the key instead of failing the parse. */
int XvtTouchControl_Retired(const char* key);

#ifdef __cplusplus
}
#endif

#endif
