#include "xvt_runtime/snapshot/cockpit_readouts.h"

#include "xvt/flight/flight.h"
#include "xvt/flight/hud/flight_text.h"
#include "xvt/flight/object/object.h"
#include "xvt/flight/player/player.h"
#include "xvt/render/flight_sw.h"
#include "xvt/render/renderer.h"
#include "xvt_runtime/snapshot/cockpit_text.h"
#include "xvt_runtime/snapshot/render_hud.h"
#include <string.h>

static XvtCockpitNumber g_numbers[XVT_COCKPIT_NUMBER_COUNT];
static XvtCockpitTarget g_target;
static int g_targetUpdated;
static XvtCockpitProvingGrounds g_course;

void XvtCockpitReadouts_Reset(void) {
	memset(g_numbers, 0, sizeof g_numbers);
	memset(&g_target, 0, sizeof g_target);
	g_target.object.slot = UINT16_MAX;
	g_targetUpdated = 0;
	memset(&g_course, 0, sizeof g_course);
}

void XvtCockpitReadouts_BeginUpdate(void) {
	g_targetUpdated = 0;
	g_course.visible = 0;
}

void XvtCockpitReadouts_RecordCourse(int x, int y, int width, int height) {
	g_course.visible = 1;
	g_course.bounds = (XvtSnapRect) { x, y, width, height };
}

void XvtCockpitReadouts_RecordNumber(XvtCockpitNumberId id, unsigned value, unsigned width, unsigned digits) {
	if ((unsigned)id >= XVT_COCKPIT_NUMBER_COUNT)
		return;
	XvtCockpitNumber* number = &g_numbers[id];
	memset(number, 0, sizeof *number);
	number->value = id == XVT_COCKPIT_NUMBER_COURSE_SCORE ? (int32_t)value : (uint16_t)value;
	number->bounds = (XvtSnapRect) { g_flightClipLeft, g_flightClipTop, g_flightClipRight - g_flightClipLeft,
									 g_flightClipBottom - g_flightClipTop };
	number->x = g_flightCursorX;
	number->y = g_flightCursorY;
	number->minimum_digits = (uint16_t)digits;
	number->field_width = (uint16_t)width;
	number->visible = 1;
	number->font_tier = g_flightFontTier;
	number->foreground = g_flightTextColorIndex;
	number->background = g_flightTextBgColor;
	number->shadow_enabled = g_flightTextShadowEnabled;
	number->shadow_color = g_flightTextShadowEnabled ? g_flightTextShadowColor : 0;
	number->phase = XVT_COCKPIT_BEFORE_CRT;
	number->word_wrap = g_flightWordWrapEnabled != 0;
	number->clear_line = g_flightClearLineBgEnabled != 0;
	number->narrow = g_flightDrawCharFn == FlightText_DrawNarrowGlyph8bpp ||
					 g_flightDrawCharFn == FlightText_DrawNarrowGlyph;
	number->keyed = g_flightSwFramebufferBase == g_flightOffscreenBuffer;
	if (number->keyed)
		number->color_key_argb = XvtRenderDraw_Color(g_flightColorEscapeBypassChar);
	if (id != XVT_COCKPIT_NUMBER_COURSE_SCORE && (uint16_t)value == UINT16_MAX) {
		number->foreground = XvtCockpitText_ResolveColor('@', g_flightColorEscapeBypassChar);
		number->shadow_enabled = number->shadow_color = 0;
	}
	number->clear_background =
		id == XVT_COCKPIT_NUMBER_COUNTERMEASURES || id == XVT_COCKPIT_NUMBER_ORDER_MINUTES;
	number->trailing_space =
		id >= XVT_COCKPIT_NUMBER_COURSE_REMAINING && id <= XVT_COCKPIT_NUMBER_COURSE_SCORE;
}

void XvtCockpitReadouts_RecordCachedNumber(unsigned binding, unsigned value, unsigned digits) {
	if (binding >= HUD_INSTRUMENT_COUNT)
		return;
	XvtCockpitNumberId id;
	switch (binding % HUD_INSTRUMENTS_PER_SET) {
		case 40:
			id = XVT_COCKPIT_NUMBER_SPEED;
			break;
		case 41:
			id = XVT_COCKPIT_NUMBER_THROTTLE;
			break;
		case 82:
			id = XVT_COCKPIT_NUMBER_TARGET_SYSTEMS;
			break;
		case 83:
			id = XVT_COCKPIT_NUMBER_TARGET_RANGE;
			break;
		case 84:
			id = XVT_COCKPIT_NUMBER_TARGET_RANGE_FRACTION;
			break;
		case 85:
		case 102:
			id = XVT_COCKPIT_NUMBER_TARGET_SHIELDS;
			break;
		case 86:
		case 103:
			id = XVT_COCKPIT_NUMBER_TARGET_HULL;
			break;
		default:
			return;
	}
	XvtCockpitReadouts_RecordNumber(id, value, g_hudElementLayouts[binding].selector, digits);
	g_numbers[id].clear_background = 1;
}

void XvtCockpitReadouts_BeginTarget(int cmd) {
	const PlayerData* player = &g_players[g_localPlayer];
	XvtSnapObjectId object = { UINT16_MAX, 0 };
	unsigned slot = (uint16_t)player->currentTargetObjectIdx;
	if (g_objectTable && slot < (unsigned)(g_regionMainObjectSlotEnd + g_regionStaticObjectSlotCount))
		object = (XvtSnapObjectId) { (uint16_t)slot, g_objectTable[slot].objectSignature };
	if (object.slot != g_target.object.slot || object.signature != g_target.object.signature ||
		cmd != g_target.cmd_mode) {
		memset(&g_target, 0, sizeof g_target);
		for (unsigned id = XVT_COCKPIT_NUMBER_TARGET_SYSTEMS; id <= XVT_COCKPIT_NUMBER_ORDER_SECONDS; ++id)
			memset(&g_numbers[id], 0, sizeof g_numbers[id]);
		if (cmd) {
			/* Recapture threat-display values even when the new target has the same percentages. */
			g_hudElementStateCache[102] = -2;
			g_hudElementStateCache[103] = -2;
		}
		XvtCockpitText_ClearTargetFields();
	}
	g_target.object = object;
	g_target.type = object.slot == UINT16_MAX ? 0 : g_objectTable[slot].objectType;
	g_target.component = (uint16_t)player->selectedTargetComponent;
	g_target.box_visible = player->targetBoxEnabled;
	g_target.visible = 1;
	g_target.cmd_mode = cmd != 0;
	g_target.panel_cover = 0;
	g_target.labels_visible = cmd || object.slot != UINT16_MAX;
	g_targetUpdated = 1;
}

void XvtCockpitReadouts_HideTarget(void) {
	g_target.visible = g_target.panel_cover;
	XvtCockpitText_ClearTargetFields();
}

void XvtCockpitReadouts_RecordTargetCover(unsigned binding) {
	g_target.visible = 1;
	g_target.panel_cover = 1;
	g_target.cover_binding = (uint16_t)binding;
	g_target.labels_visible = 0;
}

void XvtCockpitReadouts_RecordArmament(unsigned index, unsigned value) {
	if (index < 4)
		g_target.armament[index] = (XvtCockpitIndicator) { 1, (uint8_t)value, 0, XVT_COCKPIT_BEFORE_CRT };
}

void XvtCockpitReadouts_ClearOrderRange(void) {
	XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_ORDER_RANGE_SEPARATOR);
	g_numbers[XVT_COCKPIT_NUMBER_ORDER_RANGE].visible = 0;
	g_numbers[XVT_COCKPIT_NUMBER_ORDER_RANGE_FRACTION].visible = 0;
}

void XvtCockpitReadouts_ClearOrderTime(void) {
	XvtCockpitText_ClearField(XVT_COCKPIT_TEXT_ORDER_TIME_SEPARATOR);
	g_numbers[XVT_COCKPIT_NUMBER_ORDER_MINUTES].visible = 0;
	g_numbers[XVT_COCKPIT_NUMBER_ORDER_SECONDS].visible = 0;
}

void XvtCockpitReadouts_ClearLauncher(unsigned launcher) {
	if (launcher < 4)
		g_numbers[XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + launcher].visible = 0;
}

void XvtCockpitReadouts_CopyLauncher(XvtCockpitNumber* number, unsigned launcher) {
	if (launcher < 4)
		*number = g_numbers[XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + launcher];
}

static void CopyVisibleNumber(XvtCockpitNumber* destination, XvtCockpitNumberId id, int visible) {
	*destination = g_numbers[id];
	destination->visible &= visible != 0;
}

void XvtCockpitReadouts_CopyState(XvtCockpitState* state) {
	state->proving_grounds = g_course;
	CopyVisibleNumber(&state->proving_grounds.level, XVT_COCKPIT_NUMBER_COURSE_LEVEL, g_course.visible);
	CopyVisibleNumber(&state->proving_grounds.remaining, XVT_COCKPIT_NUMBER_COURSE_REMAINING,
					  g_course.visible);
	CopyVisibleNumber(&state->proving_grounds.passed, XVT_COCKPIT_NUMBER_COURSE_PASSED, g_course.visible);
	CopyVisibleNumber(&state->proving_grounds.targets, XVT_COCKPIT_NUMBER_COURSE_TARGETS, g_course.visible);
	CopyVisibleNumber(&state->proving_grounds.score, XVT_COCKPIT_NUMBER_COURSE_SCORE, g_course.visible);
	CopyVisibleNumber(&state->readouts.speed, XVT_COCKPIT_NUMBER_SPEED, state->readouts.speed.visible);
	CopyVisibleNumber(&state->readouts.throttle, XVT_COCKPIT_NUMBER_THROTTLE,
					  state->readouts.throttle.visible);
	CopyVisibleNumber(&state->readouts.clock_minutes, XVT_COCKPIT_NUMBER_CLOCK_MINUTES,
					  state->readouts.clock_minutes.visible);
	CopyVisibleNumber(&state->readouts.clock_seconds, XVT_COCKPIT_NUMBER_CLOCK_SECONDS,
					  state->readouts.clock_seconds.visible);
	CopyVisibleNumber(&state->systems.countermeasure_count, XVT_COCKPIT_NUMBER_COUNTERMEASURES,
					  state->systems.countermeasure_count.visible);
	for (unsigned slot = 0; slot < 4; ++slot)
		CopyVisibleNumber(&state->weapons.launchers[slot].count,
						  (XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LAUNCHER_FIRST + slot),
						  state->weapons.launchers[slot].count.visible);
	for (unsigned slot = 0; slot < XVT_HUD_WEAPON_SLOTS; ++slot)
		CopyVisibleNumber(&state->weapons.slots[slot].charge_percent,
						  (XvtCockpitNumberId)(XVT_COCKPIT_NUMBER_LASER_FIRST + slot),
						  state->weapons.slots[slot].charge_percent.visible);
	state->target = g_target;
	state->target.visible &= g_targetUpdated && state->view.instruments_visible;
	int values_visible = state->target.visible && !state->target.panel_cover;
	CopyVisibleNumber(&state->target.systems, XVT_COCKPIT_NUMBER_TARGET_SYSTEMS,
					  values_visible && !g_target.cmd_mode);
	CopyVisibleNumber(&state->target.shields, XVT_COCKPIT_NUMBER_TARGET_SHIELDS, values_visible);
	CopyVisibleNumber(&state->target.hull, XVT_COCKPIT_NUMBER_TARGET_HULL, values_visible);
	CopyVisibleNumber(&state->target.distance, XVT_COCKPIT_NUMBER_TARGET_RANGE,
					  values_visible && !g_target.cmd_mode);
	CopyVisibleNumber(&state->target.distance_fraction, XVT_COCKPIT_NUMBER_TARGET_RANGE_FRACTION,
					  values_visible && !g_target.cmd_mode);
	CopyVisibleNumber(&state->target.order_distance, XVT_COCKPIT_NUMBER_ORDER_RANGE,
					  values_visible && g_target.cmd_mode);
	CopyVisibleNumber(&state->target.order_distance_fraction, XVT_COCKPIT_NUMBER_ORDER_RANGE_FRACTION,
					  values_visible && g_target.cmd_mode);
	CopyVisibleNumber(&state->target.order_minutes, XVT_COCKPIT_NUMBER_ORDER_MINUTES,
					  values_visible && g_target.cmd_mode);
	CopyVisibleNumber(&state->target.order_seconds, XVT_COCKPIT_NUMBER_ORDER_SECONDS,
					  values_visible && g_target.cmd_mode);
}
