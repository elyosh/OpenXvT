#ifndef XVT_RUNTIME_INPUT_BRIDGE_H
#define XVT_RUNTIME_INPUT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XvtKeyboardRoute {
	XVT_KEYBOARD_RAW,
	XVT_KEYBOARD_GAMEPLAY,
	XVT_KEYBOARD_BLOCKED
} XvtKeyboardRoute;

XvtKeyboardRoute XvtInput_ReconcileKeyboard(void);
void XvtInput_Init(void);
/* Screen-keyboard platforms only. Desktop text input stays as Aeron started it. */
void XvtInput_SyncScreenKeyboard(void);
void XvtInput_Update(int suppress);
void XvtInput_UpdateFlight(int suppress);
void XvtInput_Shutdown(void);
int XvtInput_CanReacquireKeyboard(void);
int XvtInput_RendererShortcutAllowed(void);
/* Host-frame frontend coordinates, including game-requested pointer warps. */
void XvtInput_FrontendCursorPosition(int* x, int* y);

#ifdef __cplusplus
}
#endif

#endif
