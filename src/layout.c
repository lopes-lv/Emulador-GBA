#include "layout.h"

#include "theme.h"

#define PANEL_W 880
#define PANEL_H 460
#define PAD 24
#define COL_W 400
#define RIGHT_X 456
#define SLIDER_W 320
#define FF_SEG_W 66
#define RATE_SEG_W 100
#define CHIP_W 80
#define CHIP_H 30

Regions layout_regions(void) {
	Regions r;
	int body_y = TITLE_H + TOOL_H;
	int body_h = UI_H - body_y - STATUS_H;
	r.title = (Rect) {0, 0, UI_W, TITLE_H};
	r.toolbar = (Rect) {0, TITLE_H, UI_W, TOOL_H};
	r.body = (Rect) {0, body_y, UI_W, body_h};
	r.table_header = (Rect) {0, body_y, UI_W, HEADER_H};
	r.table_body = (Rect) {0, body_y + HEADER_H, UI_W, body_h - HEADER_H};
	r.status = (Rect) {0, UI_H - STATUS_H, UI_W, STATUS_H};
	return r;
}

Rect layout_tool_button(ToolButton b) {
	static const int widths[TB_COUNT] = {136, 88, 152, 108};
	const int gap = 8, separator = 17, y = TITLE_H + 6, h = TOOL_H - 12;
	int x = 12;
	for (int k = 0; k < (int) b; k++) {
		x += widths[k] + (k == TB_PLAY ? separator : gap);
	}
	return (Rect) {x, y, widths[b], h};
}

Rect layout_search(void) {
	return (Rect) {UI_W - 12 - SEARCH_W, TITLE_H + (TOOL_H - 32) / 2, SEARCH_W, 32};
}

Rect layout_window_button(WindowButton b) {
	int index_from_right = b == WB_CLOSE ? 0 : b == WB_MAX ? 1 : 2;
	return (Rect) {UI_W - 46 * (index_from_right + 1), 0, 46, TITLE_H};
}

Rect layout_column(int c) {
	static const int widths[TABLE_COLS] = {636, 120, 180, 64, 140, 140};
	int x = 0;
	for (int k = 0; k < c && k < TABLE_COLS; k++) x += widths[k];
	int w = (c >= 0 && c < TABLE_COLS) ? widths[c] : 0;
	return (Rect) {x, layout_regions().table_body.y, w, ROW_H};
}

int layout_visible_rows(void) {
	return layout_regions().table_body.h / ROW_H;
}

Rect layout_row(int visible_index) {
	Rect body = layout_regions().table_body;
	return (Rect) {body.x, body.y + visible_index * ROW_H, body.w, ROW_H};
}

int layout_row_at(int px, int py, int scroll) {
	Rect body = layout_regions().table_body;
	if (px < body.x || px >= body.x + body.w || py < body.y) return -1;
	int row = (py - body.y) / ROW_H;
	if (row >= layout_visible_rows()) return -1;
	return scroll + row;
}

Rect layout_settings_panel(void) {
	Rect body = layout_regions().body;
	return (Rect) {(UI_W - PANEL_W) / 2, body.y + (body.h - PANEL_H) / 2, PANEL_W, PANEL_H};
}

Rect layout_slider(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + 88, SLIDER_W, 16};
}

Rect layout_mute_box(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + 120, COL_W, 24};
}

Rect layout_ff_segment(int i) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD + FF_SEG_W * i, p.y + 184, FF_SEG_W, 32};
}

Rect layout_ff_always_box(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + 252, COL_W, 24};
}

Rect layout_turbo_box(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + RIGHT_X, p.y + 80, COL_W, 24};
}

Rect layout_turbo_rate(int i) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + RIGHT_X + RATE_SEG_W * i, p.y + 140, RATE_SEG_W, 32};
}

Rect layout_turbo_button(int i) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + RIGHT_X + CHIP_W * (i % 5), p.y + 210 + 36 * (i / 5), CHIP_W, CHIP_H};
}

Rect layout_stick_box(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + RIGHT_X, p.y + 318, COL_W, 24};
}

Rect layout_deadzone_slider(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + RIGHT_X, p.y + 378, SLIDER_W, 16};
}

Rect layout_scale_radio(int i) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + 312 + 28 * i, COL_W, 24};
}

Rect layout_fullscreen_box(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + 372, COL_W, 24};
}

Rect layout_autoclick_button(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PAD, p.y + PANEL_H - 16 - 32, 168, 32};
}

Rect layout_settings_close(void) {
	Rect p = layout_settings_panel();
	return (Rect) {p.x + PANEL_W - PAD - 96, p.y + PANEL_H - 16 - 32, 96, 32};
}

/* x -> 0..100 em passos de 5 ao longo da trilha `s`; fora dela é recortado. */
static int track_percent(Rect s, int px) {
	int offset = px - s.x;
	if (offset < 0) offset = 0;
	if (offset > s.w) offset = s.w;
	int percent = (offset * 100 + s.w / 2) / s.w;
	return ((percent + 2) / 5) * 5;
}

int layout_slider_value(int px) {
	return track_percent(layout_slider(), px);
}

#define OV_W 560
#define OV_HEAD 56
#define OV_ROW_H 44
#define OV_FOOT 44
#define OV_H (OV_HEAD + OVERLAY_ROWS * OV_ROW_H + OV_FOOT)

Rect layout_overlay_panel(void) {
	return (Rect) {(UI_W - OV_W) / 2, (UI_H - OV_H) / 2, OV_W, OV_H};
}

Rect layout_overlay_row(int i) {
	Rect p = layout_overlay_panel();
	return (Rect) {p.x + 16, p.y + OV_HEAD + OV_ROW_H * i, OV_W - 32, OV_ROW_H};
}

Rect layout_overlay_slider(void) {
	Rect r = layout_overlay_row(0);
	return (Rect) {r.x + 220, r.y + 14, 240, 16};
}

Rect layout_overlay_speed(int i) {
	Rect r = layout_overlay_row(2);
	return (Rect) {r.x + 220 + 48 * i, r.y + 6, 48, 32};
}

int layout_overlay_volume_value(int px) {
	return track_percent(layout_overlay_slider(), px);
}

/* ---- editor de macros ---- */

#define ED_W 1120
#define ED_H 600
#define ED_RIGHT_X 312 /* início da área da direita, a partir do painel */
#define ED_LIST_ROW_H 36
#define ED_STEP_ROW_H 34
#define ED_CHIP_W 44

Rect layout_editor_panel(void) {
	Rect body = layout_regions().body;
	return (Rect) {(UI_W - ED_W) / 2, body.y + (body.h - ED_H) / 2, ED_W, ED_H};
}

Rect layout_editor_list_row(int i) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + 16, p.y + 56 + ED_LIST_ROW_H * i, 280, ED_LIST_ROW_H};
}

Rect layout_editor_list_btn(int i) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + 16 + 96 * i, p.y + 460, 88, 32};
}

Rect layout_editor_name(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X, p.y + 56, 360, 32};
}

Rect layout_editor_summary(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 592, p.y + 56, 200, 32};
}

Rect layout_editor_mode(int i) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 120 * i, p.y + 104, 120, 32};
}

Rect layout_editor_repeat_minus(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 392, p.y + 104, 32, 32};
}

Rect layout_editor_repeat_value(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 424, p.y + 104, 56, 32};
}

Rect layout_editor_repeat_plus(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 480, p.y + 104, 32, 32};
}

Rect layout_editor_from_state(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X, p.y + 144, 400, 24};
}

Rect layout_editor_step_row(int i) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X, p.y + 200 + ED_STEP_ROW_H * i, 792, ED_STEP_ROW_H};
}

Rect layout_editor_chip(int row, int button) {
	Rect r = layout_editor_step_row(row);
	return (Rect) {r.x + 40 + ED_CHIP_W * button, r.y + 2, ED_CHIP_W, ED_STEP_ROW_H - 4};
}

Rect layout_editor_dur_minus(int row) {
	Rect r = layout_editor_step_row(row);
	return (Rect) {r.x + 500, r.y + 2, 32, ED_STEP_ROW_H - 4};
}

Rect layout_editor_dur_value(int row) {
	Rect r = layout_editor_step_row(row);
	return (Rect) {r.x + 532, r.y + 2, 120, ED_STEP_ROW_H - 4};
}

Rect layout_editor_dur_plus(int row) {
	Rect r = layout_editor_step_row(row);
	return (Rect) {r.x + 652, r.y + 2, 32, ED_STEP_ROW_H - 4};
}

Rect layout_editor_step_btn(int i) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_RIGHT_X + 152 * i, p.y + 514, 144, 32};
}

Rect layout_editor_close(void) {
	Rect p = layout_editor_panel();
	return (Rect) {p.x + ED_W - 16 - 96, p.y + ED_H - 16 - 32, 96, 32};
}

int layout_editor_dur_ms(uint32_t frames) {
	return (int) ((uint64_t) frames * 1000 / 60);
}

int layout_deadzone_value(int px) {
	Rect s = layout_deadzone_slider();
	int offset = px - s.x;
	if (offset < 0) offset = 0;
	if (offset > s.w) offset = s.w;
	int span = STICK_DEADZONE_MAX - STICK_DEADZONE_MIN;
	int value = STICK_DEADZONE_MIN + (offset * span + s.w / 2) / s.w;
	return ((value + STICK_DEADZONE_STEP / 2) / STICK_DEADZONE_STEP) * STICK_DEADZONE_STEP;
}
