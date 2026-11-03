#include "xvt_runtime/input/input_bridge.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/input/controller_mapping.h"
#include "xvt_runtime/input/flight_controls.h"
#include "xvt_runtime/input/keyboard_mapping.h"
#include "xvt_runtime/runtime/movie_task.h"
#include "xvt_runtime/runtime/resync_task.h"

#include "aeron/aeron.h"
#include "aeron/compat/host.h"
#include "xvt/flight/player/player.h"
#include "xvt/frontend/config.h"
#include "xvt/frontend/frontend_joystick.h"
#include "xvt/frontend/frontend_state.h"
#include "xvt/input/keyboard.h"
#include "xvt_runtime/config/config.h"
#include "xvt_runtime/runtime/dialog_task.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/presentation.h"
#include "xvt_runtime/snapshot/render_snapshot.h"

#include <SDL3/SDL.h>

#include <string.h>

static AeronWinmmJoystickState g_joystick;
static int g_connected;
static uint64_t g_reacquireFrame = UINT64_MAX;

static XvtKeyboardRoute g_keyboardRoute;
static bool g_keyboardSuppressed;
static uint64_t g_keyboardFrame = UINT64_MAX;

XvtKeyboardRoute XvtInput_ReconcileKeyboard(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	XvtKeyboardRoute route = XVT_KEYBOARD_RAW;
	if (g_keyboardSuppressed || !input || !input->has_focus || XvtInput_IsCaptured() ||
		Aeron_DebugUiVisible())
		route = XVT_KEYBOARD_BLOCKED;
	else if (XvtFlightTask_IsActive() && !XvtFlightTask_IsLoading() && !XvtDialog_IsActive() &&
			 !XvtMovieTask_IsActive() && !XvtResync_IsActive() && (unsigned)g_localPlayer < 8 &&
			 g_players[g_localPlayer].msgTypeId == FLIGHT_CHAT_RECIPIENT_INACTIVE)
		route = XVT_KEYBOARD_GAMEPLAY;
	if (route != g_keyboardRoute) {
		/* Commands and text never cross a routing transition. Held keys must be released. */
		XvtInput_FlushKeyboard();
		g_keyboardRoute = route;
	}
	XvtKeyboardMapping_Enable(route == XVT_KEYBOARD_GAMEPLAY, input);
	return route;
}

static void XvtInput_UpdateKeyboard(bool suppress) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	g_keyboardSuppressed = suppress;
	XvtKeyboardRoute route = XvtInput_ReconcileKeyboard();
	if (!input || input->frame_id == g_keyboardFrame)
		return;
	g_keyboardFrame = input->frame_id;
	XvtKeyboardMapping_BeginFrame(input);
	if (route == XVT_KEYBOARD_GAMEPLAY) {
		for (uint16_t i = 0; !input->key_events_overflow && i < input->key_event_count; ++i) {
			const AeronKeyEvent* event = &input->key_events[i];
			XvtKeyboardMapping_Event(event, AeronCompat_IsKeySuppressed(event->chord.key) != 0);
		}
		/* DirectInput still serves raw consumers, but owns no gameplay backlog. */
		XvtInput_FlushRawKeyboard();
	} else if (route == XVT_KEYBOARD_BLOCKED) {
		XvtInput_FlushRawKeyboard();
	}
}

void XvtInput_FrontendCursorPosition(int* x, int* y) {
	*x = g_frontState.mouseX;
	*y = g_frontState.mouseY;
}

static int XvtInput_Joystick(AeronWinmmJoystickState* state, void* user) {
	(void)user;
	*state = g_joystick;
	return g_connected;
}

static void XvtInput_Append(unsigned int ch) {
	int next = (g_frontState.charWriteIdx + 1) % 1024;
	if (next == g_frontState.charReadIdx)
		g_frontState.charReadIdx = (g_frontState.charReadIdx + 1) % 1024;
	g_frontState.charRingBuffer[g_frontState.charWriteIdx] = (char)ch;
	g_frontState.charWriteIdx = next;
}

static unsigned int XvtInput_VirtualKey(int key) {
	static const unsigned char controls[] = { 13,   27, 8,    9,    32,   0xbd, 0xbb, 0xdb, 0xdd,
											  0xdc, 0,  0xba, 0xde, 0xc0, 0xbc, 0xbe, 0xbf, 0x14 };
	static const unsigned char navigation[] = { 0x2c, 0x91, 0x13, 0x2d, 0x24, 0x21, 0x2e,
												0x23, 0x22, 0x27, 0x25, 0x28, 0x26 };
	if (key >= AERON_KEY_A && key < AERON_KEY_A + 26)
		return 'A' + key - AERON_KEY_A;
	if (key >= AERON_KEY_1 && key < AERON_KEY_1 + 10)
		return key == AERON_KEY_1 + 9 ? '0' : '1' + key - AERON_KEY_1;
	if (key >= AERON_KEY_RETURN && key <= AERON_KEY_CAPSLOCK)
		return controls[key - AERON_KEY_RETURN];
	if (key >= AERON_KEY_F1 && key < AERON_KEY_F1 + 12)
		return 0x70 + key - AERON_KEY_F1;
	if (key >= AERON_KEY_PRINTSCREEN && key <= AERON_KEY_UP)
		return navigation[key - AERON_KEY_PRINTSCREEN];
	if (key >= AERON_KEY_LCTRL && key <= AERON_KEY_RGUI) {
		static const unsigned char modifiers[] = { 0xa2, 0xa0, 0xa4, 0x5b, 0xa3, 0xa1, 0xa5, 0x5c };
		return modifiers[key - AERON_KEY_LCTRL];
	}
	if (key >= AERON_KEY_KP_1 && key <= AERON_KEY_KP_9)
		return 0x61 + key - AERON_KEY_KP_1;
	if (key == AERON_KEY_KP_0)
		return 0x60;
	if (key == AERON_KEY_KP_ENTER)
		return 13;
	return 0;
}

static unsigned int XvtInput_Windows1252(unsigned int cp) {
	static const unsigned short extended[32] = { 0x20ac, 0,      0x201a, 0x0192, 0x201e, 0x2026, 0x2020,
												 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0,
												 0x017d, 0,      0,      0x2018, 0x2019, 0x201c, 0x201d,
												 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a,
												 0x0153, 0,      0x017e, 0x0178 };
	unsigned int i;
	if ((cp >= 32 && cp < 127) || (cp >= 160 && cp <= 255))
		return cp;
	for (i = 0; i < 32; ++i)
		if (cp && cp == extended[i])
			return 128 + i;
	return 0;
}

static void XvtInput_Text(const AeronInputSnapshot* input) {
	uint32_t i = 0;
	while (i < input->text_length) {
		unsigned int cp = (unsigned char)input->text[i++];
		unsigned int count = 0;
		unsigned int minimum = 0;
		if (cp >= 0xc2 && cp <= 0xdf) {
			cp &= 31;
			count = 1;
			minimum = 128;
		} else if (cp >= 0xe0 && cp <= 0xef) {
			cp &= 15;
			count = 2;
			minimum = 2048;
		} else if (cp >= 128) {
			continue;
		}
		if (count > input->text_length - i)
			break;
		while (count--) {
			unsigned int tail = (unsigned char)input->text[i++];
			if ((tail & 0xc0) != 0x80) {
				cp = 0;
				break;
			}
			cp = (cp << 6) | (tail & 63);
		}
		if (cp < minimum)
			continue;
		cp = XvtInput_Windows1252(cp);
		if (cp)
			XvtInput_Append(cp);
	}
}

static void XvtInput_Controller(int suppress) {
	int connected = XvtControllerMapping_Present();
	int changed = g_connected != connected;
	g_connected = connected;
	memset(&g_joystick, 0, sizeof g_joystick);
	g_joystick.name = "OpenXvT Controllers";
	g_joystick.button_count = 2;
	g_joystick.has_pov = 1;
	g_joystick.pov_direction = -1;
	for (int axis = 0; axis < 4; ++axis)
		g_joystick.axes[axis] = 32768;
	if (changed) {
		memset(g_frontState.joystickPresent, 0, sizeof g_frontState.joystickPresent);
		memset(g_frontState.joystickButtonHeld, 0, sizeof g_frontState.joystickButtonHeld);
		memset(g_frontState.joystickButtonReleased, 0, sizeof g_frontState.joystickButtonReleased);
		memset(g_frontState.joystickAxisX, 0, sizeof g_frontState.joystickAxisX);
		memset(g_frontState.joystickAxisY, 0, sizeof g_frontState.joystickAxisY);
		memset(g_frontState.joystickPovDirection, 0, sizeof g_frontState.joystickPovDirection);
		Joystick_InitDevices();
	}
	if (connected && !suppress) {
		static const int channels[] = { XVT_INPUT_AXIS_YAW, XVT_INPUT_AXIS_PITCH, -1, XVT_INPUT_AXIS_ROLL };
		for (int axis = 0; axis < 4; ++axis)
			if (channels[axis] >= 0)
				g_joystick.axes[axis] =
					(uint32_t)(32768 + 256 * XvtControllerMapping_MenuAxis((XvtInputAxis)channels[axis]));
		g_joystick.buttons = XvtControllerMapping_MenuButtons();
		unsigned hat = XvtControllerMapping_MenuHat();
		g_joystick.pov_direction = hat & 1 ? 0 : hat & 2 ? 1 : hat & 4 ? 2 : hat & 8 ? 3 : -1;
	} else {
		/* Suppression is a route transition, not a frontend button release. */
		memset(g_frontState.joystickButtonHeld, 0, sizeof g_frontState.joystickButtonHeld);
		memset(g_frontState.joystickButtonReleased, 0, sizeof g_frontState.joystickButtonReleased);
	}
}

static SDL_Window* XvtInput_TextWindow(void) {
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

static int XvtInput_CollectingText(void) {
	const XvtRenderSnapshot* snapshot = XvtRenderSnapshot_Current();
	if (snapshot && snapshot->text_field_selected)
		return 1;
	if (XvtDialog_IsTextPrompt())
		return 1;
	if (XvtFlightTask_IsActive() && (unsigned)g_localPlayer < 8 &&
		g_players[g_localPlayer].msgTypeId != FLIGHT_CHAT_RECIPIENT_INACTIVE)
		return 1;

	return 0;
}

void XvtInput_SyncScreenKeyboard(void) {
	SDL_Window* window;
	int collecting;
	if (!SDL_HasScreenKeyboardSupport())
		return;
	window = XvtInput_TextWindow();
	if (!window)
		return;
	/* Aeron starts text input so desktop fields receive characters. On a phone
	 * that same call presents the screen keyboard and leaves it up. */
	collecting = XvtInput_CollectingText();
	if (collecting) {
		if (!SDL_TextInputActive(window))
			SDL_StartTextInput(window);
	} else if (SDL_TextInputActive(window)) {
		SDL_StopTextInput(window);
	}
}

void XvtInput_Init(void) {
	const XvtSettings* settings = XvtConfig_Settings();
	if (!settings)
		return;
	XvtControllerMapping_Init(&settings->controller);
	XvtKeyboardMapping_Install(&settings->keyboard);
	g_keyboardRoute = XVT_KEYBOARD_RAW;
	g_keyboardSuppressed = true;
	g_keyboardFrame = UINT64_MAX;
	XvtFlightControls_Reset();
	g_reacquireFrame = UINT64_MAX;
	AeronCompat_SetJoystickSource(XvtInput_Joystick, NULL);
}

int XvtInput_CanReacquireKeyboard(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	if (!input || !input->has_focus || input->frame_id == g_reacquireFrame)
		return 0;
	g_reacquireFrame = input->frame_id;
	return 1;
}

void XvtInput_Update(int suppress) {
	XvtInput_SyncScreenKeyboard();
	XvtInput_UpdateKeyboard(suppress != 0);
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	int key;
	if (!input)
		return;
	suppress |= g_keyboardRoute == XVT_KEYBOARD_BLOCKED;
	XvtControllerMapping_Update(input);
	XvtFlightControls_UpdateThrottleContext();
	XvtInput_Controller(suppress);
	memset(g_frontState.keyState, 0, sizeof(g_frontState.keyState));
	if (suppress) {
		Keyboard_FlushCharBuffer();
		g_frontState.mouseLeftDown = g_frontState.mouseRightDown = 0;
		g_frontState.mouseClickLatch = g_frontState.mouseRightClickLatch = 0;
		return;
	}
	for (key = 0; key < AERON_KEY_COUNT; ++key) {
		if (AeronCompat_IsKeySuppressed(key))
			continue;
		unsigned int vk = XvtInput_VirtualKey(key);
		if (vk && input->key_down[key])
			g_frontState.keyState[vk] = 0x80;
		if (vk == 8 || vk == 9 || vk == 13 || vk == 27) {
			unsigned int repeat;
			for (repeat = 0; repeat < input->key_typed[key]; ++repeat)
				XvtInput_Append(vk);
		}
	}
	g_frontState.keyState[0x10] = g_frontState.keyState[0xa0] | g_frontState.keyState[0xa1];
	g_frontState.keyState[0x11] = g_frontState.keyState[0xa2] | g_frontState.keyState[0xa3];
	g_frontState.keyState[0x12] = g_frontState.keyState[0xa4] | g_frontState.keyState[0xa5];
	XvtInput_Text(input);
	int x, y;
	int inside = XvtPresentation_MouseToClassic(input, &x, &y);
	if (inside) {
		g_frontState.mouseX = x;
		g_frontState.mouseY = y;
		g_frontState.mouseClickLatch |=
			!!(XvtInput_FilterMouseButtons(input->mouse.released_buttons) & AERON_MOUSE_BUTTON_LEFT);
		g_frontState.mouseRightClickLatch |=
			!!(XvtInput_FilterMouseButtons(input->mouse.released_buttons) & AERON_MOUSE_BUTTON_RIGHT);
	}
	g_frontState.mouseLeftDown =
		inside && (XvtInput_FilterMouseButtons(input->mouse.buttons) & AERON_MOUSE_BUTTON_LEFT);
	g_frontState.mouseRightDown =
		inside && (XvtInput_FilterMouseButtons(input->mouse.buttons) & AERON_MOUSE_BUTTON_RIGHT);
}

int XvtInput_RendererShortcutAllowed(void) {
	if (XvtInput_IsCaptured())
		return 0;
	const XvtRenderSnapshot* s = XvtRenderSnapshot_Current();
	if (!s || s->text_entry_active || XvtDialog_IsTextPrompt())
		return 0;
	return !XvtFlightTask_IsActive() || (unsigned)g_localPlayer >= 8 ||
		   g_players[g_localPlayer].msgTypeId == FLIGHT_CHAT_RECIPIENT_INACTIVE;
}

void XvtInput_UpdateFlight(int suppress) {
	XvtInput_SyncScreenKeyboard();
	XvtInput_UpdateKeyboard(suppress != 0);
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	if (input) {
		XvtControllerMapping_Update(input);
		XvtFlightControls_UpdateThrottleContext();
		XvtInput_Controller(suppress || !input->has_focus || XvtInput_IsCaptured());
	}
	Keyboard_FlushCharBuffer();
}

void XvtInput_Shutdown(void) {
	XvtKeyboardMapping_Suspend();
	XvtInput_ResetCapture();
	XvtControllerMapping_Shutdown();
	XvtFlightControls_Reset();
	AeronCompat_SetKeySuppressed(AERON_KEY_TAB, 0);
	AeronCompat_SetJoystickSource(NULL, NULL);
	g_connected = 0;
}
