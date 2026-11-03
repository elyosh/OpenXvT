#include "xvt_app/settings/bindings_editor.h"
#include <stdio.h>

void XvtBindingsEditor_Init(XvtBindingsEditor* editor) {
	*editor = (XvtBindingsEditor) { .action_selected = SIZE_MAX, .binding_selected = SIZE_MAX };
}

void XvtBindingsEditor_Select(XvtBindingsEditor* editor, XvtInputAction action, bool open_modal) {
	editor->selected_action = action;
	editor->category = XvtInputActions_Category(action);
	editor->action_selected = SIZE_MAX;
	editor->binding_selected = SIZE_MAX;
	if (open_modal)
		editor->binding_modal_open = 1;
}

void XvtBindingsEditor_Category(XvtBindingsEditor* editor, AeronUiContext* ui) {
	static const char* const categories[] = { "Weapons", "Targets", "Throttle", "View",
											  "Info",    "System",  "Comms" };
	if (AeronUi_SegmentedSelector(ui, "Action Category", &editor->category, categories,
								  XVT_INPUT_ACTION_CATEGORY_COUNT)) {
		editor->action_selected = SIZE_MAX;
		editor->selected_action = XVT_INPUT_ACTION_NONE;
	}
}

void XvtBindingsEditor_Actions(XvtBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							   size_t count, float trailing_height) {
	if (editor->action_selected == SIZE_MAX && editor->selected_action != XVT_INPUT_ACTION_NONE)
		for (size_t i = 0; i < count; ++i)
			if (items[i].id == (uint64_t)editor->selected_action) {
				editor->action_selected = i;
				break;
			}
	const AeronUiTheme* theme = AeronUi_GetTheme(ui);
	float available = AeronUi_AvailableHeight(ui);
	float height = available - trailing_height;
	if (available <= 1.0f)
		height = (float)count * (theme ? theme->row_height : 40.0f);
	if (height < 180.0f)
		height = 180.0f;
	uint32_t result = AeronUi_ListBox(ui, "Actions", items, count, &editor->action_selected, height);
	if ((result & AERON_UI_LIST_ACTIVATED) && editor->action_selected < count)
		XvtBindingsEditor_Select(editor, (XvtInputAction)items[editor->action_selected].id, true);
}

bool XvtBindingsEditor_BeginDetail(XvtBindingsEditor* editor, AeronUiContext* ui) {
	if (!editor->binding_modal_open || editor->selected_action == XVT_INPUT_ACTION_NONE)
		return false;
	return AeronUi_BeginModal(ui, XvtInputActions_DisplayName(editor->selected_action),
							  &editor->binding_modal_open,
							  &(AeronUiWindowDesc) { .width_ref = 720.0f, .centered = 1 }) != 0;
}

void XvtBindingsEditor_List(XvtBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							size_t count, const char* empty_text) {
	AeronUi_Header(ui, "Current Bindings");
	if (count)
		AeronUi_ListBox(ui, "Bindings", items, count, &editor->binding_selected, 180.0f);
	else {
		editor->binding_selected = SIZE_MAX;
		AeronUi_Help(ui, empty_text);
	}
}

bool XvtBindingsEditor_Remove(AeronUiContext* ui) { return AeronUi_Button(ui, "Remove Binding") != 0; }

void XvtBindingsEditor_EndDetail(XvtBindingsEditor* editor, AeronUiContext* ui) {
	if (AeronUi_Button(ui, "Done"))
		editor->binding_modal_open = 0;
	AeronUi_EndModal(ui);
}

bool XvtBindingsEditor_Conflict(AeronUiContext* ui, int* open, const char* source, XvtInputAction previous,
								XvtInputAction replacement) {
	if (!AeronUi_BeginModal(ui, "CONTROL ALREADY BOUND", open, NULL))
		return false;
	char text[512];
	snprintf(text, sizeof text, "%s is assigned to %s. Replace it with %s?", source,
			 XvtInputActions_DisplayName(previous), XvtInputActions_DisplayName(replacement));
	AeronUi_Error(ui, text);
	AeronUi_BeginColumns(ui, 2, NULL);
	bool replace = AeronUi_Button(ui, "Replace") != 0;
	if (replace)
		*open = 0;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Cancel"))
		*open = 0;
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
	return replace;
}
