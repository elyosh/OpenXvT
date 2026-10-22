#ifndef XVT_APP_TOUCH_CONTROLS_H
#define XVT_APP_TOUCH_CONTROLS_H

#include "aeron/input.h"

#include <stdint.h>
#include <string.h>

struct AeronFontAtlas;
struct AeronUiContext;

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define XVT_TOUCH_CONTROLS_AVAILABLE 1
#else
#define XVT_TOUCH_CONTROLS_AVAILABLE 0
#endif

#if XVT_TOUCH_CONTROLS_AVAILABLE
void XvtTouchControls_Init(const struct AeronFontAtlas* font);
void XvtTouchControls_Shutdown(void);
void XvtTouchControls_Sync(const AeronInputSnapshot* input);
void XvtTouchControls_Draw(void);
void XvtTouchControls_DrawMenu(struct AeronUiContext* ui, const AeronInputSnapshot* input);
void XvtTouchControls_Omit(AeronInputSnapshot* copy, const AeronInputSnapshot* input);
int XvtTouchControls_IsVirtual(uint32_t instance_id);
#else
static inline void XvtTouchControls_Init(const struct AeronFontAtlas* font) { (void)font; }

static inline void XvtTouchControls_Shutdown(void) {}

static inline void XvtTouchControls_Sync(const AeronInputSnapshot* input) { (void)input; }

static inline void XvtTouchControls_Draw(void) {}

static inline void XvtTouchControls_DrawMenu(struct AeronUiContext* ui, const AeronInputSnapshot* input) {
	(void)ui;
	(void)input;
}

static inline void XvtTouchControls_Omit(AeronInputSnapshot* copy, const AeronInputSnapshot* input) {
	if (!copy)
		return;
	if (!input) {
		memset(copy, 0, sizeof *copy);
		return;
	}
	*copy = *input;
}

static inline int XvtTouchControls_IsVirtual(uint32_t instance_id) {
	(void)instance_id;

	return 0;
}
#endif

#endif
