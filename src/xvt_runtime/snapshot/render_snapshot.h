#ifndef XVT_RUNTIME_SNAPSHOT_RENDER_SNAPSHOT_H
#define XVT_RUNTIME_SNAPSHOT_RENDER_SNAPSHOT_H

#include "xvt_runtime/snapshot/cockpit_state.h"
#include "xvt_runtime/snapshot/render_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XvtSnapMapObject {
	uint16_t object_index, icon_frame;
	uint16_t icon_width, icon_height;
	uint32_t label_offset;
	uint32_t label_color_argb;
	uint32_t line_color_argb;
	int32_t box_extent;
	int16_t move_x, move_y;
	uint16_t range_value;
	uint8_t render_kind, cull_kind, effective_iff;
	uint8_t label_visible, movement_visible, box_visible, box_color;
	uint8_t overlay_visible, range_visible;
} XvtSnapMapObject;

enum { XVT_MAP_MODEL_OR_ICON, XVT_MAP_EFFECT };

typedef struct XvtSnapMap {
	uint8_t active, endpoint_valid;
	XvtSnapObjectId target;
	int32_t grid_z, order_endpoint[3];
	uint64_t icon_asset_id;
	uint64_t font_asset_id;
	uint32_t object_count, label_bytes;
	XvtSnapMapObject objects[XVT_SNAP_OBJECTS];
	char labels[XVT_SNAP_LABEL_BYTES];
} XvtSnapMap;

typedef struct XvtSnapSky {
	uint16_t star_density;
	uint16_t checkpoint_slot, craft_slot_end;
	uint8_t backdrop_enabled, debris_enabled, proving_grounds;
	uint8_t backdrop_types[64], backdrop_directions[64];
	uint16_t direction_counts[6];
} XvtSnapSky;

typedef struct XvtSnapStreak {
	int32_t offset[3], half_width;
	uint16_t roll;
} XvtSnapStreak;

typedef struct XvtSnapHyperspace {
	uint8_t valid, phase, iff;
	uint32_t elapsed_ticks, count;
	XvtSnapStreak streaks[1024];
} XvtSnapHyperspace;

typedef struct XvtSnapCursor {
	uint64_t asset_id;
	int32_t x, y;
	uint16_t width, height;
	uint8_t visible;
} XvtSnapCursor;

typedef struct XvtRenderSnapshot {
	uint64_t tick_index, flight_frame_serial, capture_host_us;
	uint64_t component_event_serial;
	int32_t component_event_time;
	uint8_t flight_unlocked;
	uint64_t presentation_serial, frontend_generation;
	uint32_t presented_scene;
	uint16_t presented_target;
	uint8_t frontend_surfaces_released;
	uint64_t mission_generation, world_generation;
	int32_t game_time_ticks, view_time_ticks;
	uint32_t scene_kind, dropped_records;
	uint8_t flight_valid, focused, paused, text_entry_active, text_field_selected;
	XvtSnapCamera camera;
	XvtSnapLighting lighting;
	XvtCockpitState cockpit;
	XvtCockpitResources cockpit_resources;
	XvtSnapMap map;
	XvtSnapSky sky;
	XvtSnapHyperspace hyperspace;
	XvtSnapCursor cursor;
	uint32_t flight_palette_argb[256];
	int16_t fuselage_sequence[25];
	XvtSnapType types[201];
	XvtSnapObject objects[XVT_SNAP_OBJECTS];
	uint32_t object_count;
	XvtSnapTargetBox target_boxes[XVT_SNAP_TARGET_BOXES];
	uint32_t target_box_count;
	/* Ordered frontend/movie composition; flight HUD content is in cockpit. */
	XvtSnapSprite sprites[XVT_SNAP_SPRITES];
	uint32_t sprite_count;
	XvtSnapGlyph glyphs[XVT_SNAP_GLYPHS];
	uint32_t glyph_count;
	XvtSnapPaint paint[XVT_SNAP_PAINT];
	uint32_t paint_count;
	XvtSnapCopyRect copies[XVT_SNAP_COPIES];
	uint32_t copy_count;
	XvtSnapSurfaceEvent surface_events[XVT_SNAP_SURFACE_EVENTS];
	uint32_t surface_event_count;
	XvtSnapPreview previews[XVT_SNAP_PREVIEWS];
	uint32_t preview_count;
	uint64_t opt_asset_generation, texture_asset_generation;
	uint64_t image_asset_generation;
	XvtSnapOptAsset opt_assets[XVT_SNAP_ASSETS];
	uint32_t opt_asset_count;
	XvtSnapTextureAsset texture_assets[XVT_SNAP_TYPES];
	uint32_t texture_asset_count;
	XvtSnapImageAsset image_assets[XVT_SNAP_ASSETS];
	uint32_t image_asset_count;
} XvtRenderSnapshot;

/* The application initializes before port startup and shuts down after the
 * port and remaster. All access is confined to the host thread. */
void XvtRenderSnapshot_Init(void);
void XvtRenderSnapshot_Shutdown(void);
/* BeginTick is idempotent while a tick is open (including paused-frame routing). */
void XvtRenderSnapshot_BeginTick(void);
void XvtRenderSnapshot_SetSceneKind(XvtSceneKind kind);
void XvtRenderSnapshot_Commit(int32_t game_time_ticks, int focused, int paused);
/* Views stay valid until the next commit; NULL before their first publication. */
const XvtRenderSnapshot* XvtRenderSnapshot_Current(void);
const XvtRenderSnapshot* XvtRenderSnapshot_Previous(void);

#ifdef __cplusplus
}
#endif

#endif
