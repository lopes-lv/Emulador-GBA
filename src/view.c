#include "view.h"

#include <stdlib.h>
#include <string.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>

#include "layout.h"

#define WHEEL_LINES 3
#define CHIPS_PER_ROW 5

/* ---- filtro e seleção ---- */

static int max_scroll(const View *v) {
	int m = v->visible_count - layout_visible_rows();
	return m > 0 ? m : 0;
}

static void clamp_scroll(View *v) {
	int m = max_scroll(v);
	if (v->scroll > m) v->scroll = m;
	if (v->scroll < 0) v->scroll = 0;
}

static void ensure_selected_visible(View *v) {
	if (v->selected >= 0) {
		int rows = layout_visible_rows();
		if (v->selected < v->scroll) v->scroll = v->selected;
		if (v->selected >= v->scroll + rows) v->scroll = v->selected - rows + 1;
	}
	clamp_scroll(v);
}

/* Refaz a lista filtrada mantendo a seleção no mesmo jogo, se ele ainda estiver na lista. */
static void refilter(View *v) {
	int previous = v->selected >= 0 && v->selected < v->visible_count ? v->visible[v->selected] : -1;
	if (!v->lib || !v->visible) {
		v->visible_count = 0;
		v->selected = -1;
		return;
	}
	v->visible_count = library_filter(v->lib, NULL, v->search.text, v->visible);
	v->selected = -1;
	for (int i = 0; i < v->visible_count; i++) {
		if (v->visible[i] == previous) {
			v->selected = i;
			break;
		}
	}
	ensure_selected_visible(v);
}

void view_init(View *v, Settings *settings) {
	memset(v, 0, sizeof(*v));
	v->settings = settings;
	v->selected = -1;
	v->focus = FOCUS_TABLE;
	v->hover_kind = HOVER_NONE;
	textedit_clear(&v->search);
}

void view_set_library(View *v, const Library *lib, int can_play) {
	free(v->visible);
	v->lib = lib;
	v->can_play = can_play;
	v->visible = lib ? malloc(((size_t) lib->count + 1) * sizeof(int)) : NULL;
	v->visible_count = 0;
	v->selected = -1;
	v->scroll = 0;
	refilter(v);
}

const LibEntry *view_selected(const View *v) {
	if (!v->lib || v->selected < 0 || v->selected >= v->visible_count) return NULL;
	return &v->lib->items[v->visible[v->selected]];
}

void view_free(View *v) {
	free(v->visible);
	v->visible = NULL;
	v->visible_count = 0;
}

static void select_index(View *v, int index) {
	if (v->visible_count == 0) return;
	if (index < 0) index = 0;
	if (index >= v->visible_count) index = v->visible_count - 1;
	v->selected = index;
	ensure_selected_visible(v);
}

/* ---- ações ---- */

static ViewAction play_action(const View *v) {
	return v->can_play && v->selected >= 0 ? VACT_PLAY : VACT_NONE;
}

static void open_settings(View *v) {
	v->settings_open = 1;
	v->panel_focus = PF_SLIDER;
	v->panel_button = 0;
	v->dragging_slider = 0;
	v->dragging_deadzone = 0;
}

static ViewAction tool_action(View *v, ToolButton b) {
	switch (b) {
	case TB_ADD: return VACT_ADD_GAME;
	case TB_PLAY: return play_action(v);
	case TB_SETTINGS: open_settings(v); return VACT_NONE;
	case TB_CONTROLS: return VACT_CONTROLS;
	default: return VACT_NONE;
	}
}

static void update_hover(View *v, int x, int y) {
	v->hover_kind = HOVER_NONE;
	v->hover_item = -1;
	for (int b = 0; b < 3; b++) {
		if (rect_contains(layout_window_button((WindowButton) b), x, y)) {
			v->hover_kind = HOVER_WINBTN;
			v->hover_item = b;
			return;
		}
	}
	for (int b = 0; b < TB_COUNT; b++) {
		if (rect_contains(layout_tool_button((ToolButton) b), x, y)) {
			v->hover_kind = HOVER_TOOL;
			v->hover_item = b;
			return;
		}
	}
	if (rect_contains(layout_search(), x, y)) {
		v->hover_kind = HOVER_SEARCH;
		return;
	}
	int row = layout_row_at(x, y, v->scroll);
	if (row >= 0 && row < v->visible_count) {
		v->hover_kind = HOVER_ROW;
		v->hover_item = row;
	}
}

/* ---- mouse na biblioteca ---- */

static ViewAction mouse_down(View *v, ViewEvent ev) {
	int x = ev.x, y = ev.y;
	for (int b = 0; b < 3; b++) {
		if (!rect_contains(layout_window_button((WindowButton) b), x, y)) continue;
		return b == WB_MIN ? VACT_WIN_MIN : b == WB_MAX ? VACT_WIN_MAX : VACT_QUIT;
	}
	for (int b = 0; b < TB_COUNT; b++) {
		if (rect_contains(layout_tool_button((ToolButton) b), x, y)) {
			v->focus = FOCUS_TOOLBAR;
			v->focus_item = b;
			return tool_action(v, (ToolButton) b);
		}
	}
	if (rect_contains(layout_search(), x, y)) {
		v->focus = FOCUS_SEARCH;
		return VACT_NONE;
	}
	int row = layout_row_at(x, y, v->scroll);
	if (row >= 0 && row < v->visible_count) {
		v->focus = FOCUS_TABLE;
		v->selected = row;
		return ev.type == VEV_MOUSE_DBL ? play_action(v) : VACT_NONE;
	}
	if (row >= 0) return VACT_NONE; /* linha vazia da tabela */
	if (v->focus == FOCUS_SEARCH) v->focus = FOCUS_NONE;
	return VACT_NONE;
}

/* ---- teclado na biblioteca ---- */

static const FocusZone ZONES[] = {FOCUS_TOOLBAR, FOCUS_SEARCH, FOCUS_TABLE};
#define ZONE_COUNT ((int) (sizeof(ZONES) / sizeof(ZONES[0])))

static void cycle_focus(View *v, int backwards) {
	int index = -1;
	for (int i = 0; i < ZONE_COUNT; i++) {
		if (ZONES[i] == v->focus) index = i;
	}
	if (index < 0) index = backwards ? 0 : ZONE_COUNT - 1;
	index = (index + (backwards ? ZONE_COUNT - 1 : 1)) % ZONE_COUNT;
	v->focus = ZONES[index];
	v->focus_item = 0;
}

static int is_activate(int sc) {
	return sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE;
}

static ViewAction key_table(View *v, int sc) {
	int rows = layout_visible_rows();
	int from = v->selected < 0 ? 0 : v->selected;
	switch (sc) {
	case SDL_SCANCODE_DOWN: select_index(v, v->selected < 0 ? 0 : from + 1); break;
	case SDL_SCANCODE_UP: select_index(v, v->selected < 0 ? 0 : from - 1); break;
	case SDL_SCANCODE_PAGEDOWN: select_index(v, from + rows); break;
	case SDL_SCANCODE_PAGEUP: select_index(v, from - rows); break;
	case SDL_SCANCODE_HOME: select_index(v, 0); break;
	case SDL_SCANCODE_END: select_index(v, v->visible_count - 1); break;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER: return play_action(v);
	default: break;
	}
	return VACT_NONE;
}

static ViewAction key_toolbar(View *v, int sc) {
	if (sc == SDL_SCANCODE_LEFT) v->focus_item = (v->focus_item + TB_COUNT - 1) % TB_COUNT;
	else if (sc == SDL_SCANCODE_RIGHT) v->focus_item = (v->focus_item + 1) % TB_COUNT;
	else if (is_activate(sc)) return tool_action(v, (ToolButton) v->focus_item);
	return VACT_NONE;
}

static ViewAction key_search(View *v, int sc) {
	if (sc == SDL_SCANCODE_BACKSPACE) {
		if (textedit_backspace(&v->search)) refilter(v);
	} else if (sc == SDL_SCANCODE_ESCAPE) {
		if (v->search.len_chars > 0) {
			textedit_clear(&v->search);
			refilter(v);
		} else {
			v->focus = FOCUS_NONE;
		}
	} else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
		v->focus = FOCUS_TABLE;
	}
	return VACT_NONE;
}

static ViewAction key_library(View *v, ViewEvent ev) {
	int sc = ev.key;
	if (sc == SDL_SCANCODE_F && (ev.mods & KMOD_CTRL)) {
		v->focus = FOCUS_SEARCH;
		return VACT_NONE;
	}
	if (sc == SDL_SCANCODE_TAB) {
		cycle_focus(v, (ev.mods & KMOD_SHIFT) != 0);
		return VACT_NONE;
	}
	switch (v->focus) {
	case FOCUS_TABLE: return key_table(v, sc);
	case FOCUS_TOOLBAR: return key_toolbar(v, sc);
	case FOCUS_SEARCH: return key_search(v, sc);
	default: return VACT_NONE;
	}
}

/* ---- painel Configurações ---- */

static ViewAction changed(int did_change) {
	return did_change ? VACT_SETTINGS_CHANGED : VACT_NONE;
}

static int set_volume(View *v, int value) {
	int did = v->settings->volume != value;
	v->settings->volume = value;
	return did;
}

static int set_ff(View *v, int index) {
	int did = v->settings->ff_index != index;
	v->settings->ff_index = index;
	return did;
}

static int set_rate(View *v, int index) {
	if (index < 0) index = 0;
	if (index >= TURBO_RATE_COUNT) index = TURBO_RATE_COUNT - 1;
	int did = v->settings->turbo_rate != index;
	v->settings->turbo_rate = index;
	return did;
}

static int set_deadzone(View *v, int value) {
	int did = v->settings->stick_deadzone != value;
	v->settings->stick_deadzone = value;
	return did;
}

static int set_scale(View *v, ScaleMode mode) {
	int did = v->settings->scale != mode;
	v->settings->scale = mode;
	return did;
}

static int toggle_flag(int *flag) {
	*flag = !*flag;
	return 1;
}

static void close_panel(View *v) {
	v->settings_open = 0;
	v->dragging_slider = 0;
	v->dragging_deadzone = 0;
}

static ViewAction panel_mouse_down(View *v, ViewEvent ev) {
	int x = ev.x, y = ev.y;
	Settings *s = v->settings;
	if (rect_contains(layout_slider(), x, y)) {
		v->panel_focus = PF_SLIDER;
		v->dragging_slider = 1;
		return changed(set_volume(v, layout_slider_value(x)));
	}
	if (rect_contains(layout_mute_box(), x, y)) {
		v->panel_focus = PF_MUTE;
		return changed(toggle_flag(&s->muted));
	}
	for (int i = 0; i < FF_SPEED_COUNT; i++) {
		if (rect_contains(layout_ff_segment(i), x, y)) {
			v->panel_focus = PF_FF;
			return changed(set_ff(v, i));
		}
	}
	if (rect_contains(layout_ff_always_box(), x, y)) {
		v->panel_focus = PF_FF_ALWAYS;
		return changed(toggle_flag(&s->ff_always));
	}
	if (rect_contains(layout_turbo_box(), x, y)) {
		v->panel_focus = PF_TURBO;
		return changed(toggle_flag(&s->turbo));
	}
	for (int i = 0; i < TURBO_RATE_COUNT; i++) {
		if (rect_contains(layout_turbo_rate(i), x, y)) {
			v->panel_focus = PF_TURBO_RATE;
			return changed(set_rate(v, i));
		}
	}
	for (int i = 0; i < BTN_COUNT; i++) {
		if (rect_contains(layout_turbo_button(i), x, y)) {
			v->panel_focus = PF_TURBO_BUTTONS;
			v->panel_button = i;
			return changed(settings_turbo_toggle_button(s, (GbaButton) i));
		}
	}
	if (rect_contains(layout_stick_box(), x, y)) {
		v->panel_focus = PF_STICK;
		return changed(toggle_flag(&s->stick_on));
	}
	if (rect_contains(layout_deadzone_slider(), x, y)) {
		v->panel_focus = PF_STICK_DEADZONE;
		v->dragging_deadzone = 1;
		return changed(set_deadzone(v, layout_deadzone_value(x)));
	}
	for (int i = 0; i < 2; i++) {
		if (rect_contains(layout_scale_radio(i), x, y)) {
			v->panel_focus = PF_SCALE;
			return changed(set_scale(v, i == 0 ? SCALE_FILL : SCALE_INTEGER));
		}
	}
	if (rect_contains(layout_fullscreen_box(), x, y)) {
		v->panel_focus = PF_FULLSCREEN;
		return changed(toggle_flag(&s->start_fullscreen));
	}
	if (rect_contains(layout_autoclick_button(), x, y)) {
		v->panel_focus = PF_AUTOCLICK;
		return VACT_AUTOCLICK; /* o painel continua aberto: quem abre o editor é o main */
	}
	if (rect_contains(layout_settings_close(), x, y)) close_panel(v);
	return VACT_NONE;
}

static ViewAction panel_key_buttons(View *v, int sc) {
	if (sc == SDL_SCANCODE_LEFT && v->panel_button > 0) v->panel_button--;
	else if (sc == SDL_SCANCODE_RIGHT && v->panel_button < BTN_COUNT - 1) v->panel_button++;
	else if (sc == SDL_SCANCODE_DOWN && v->panel_button + CHIPS_PER_ROW < BTN_COUNT) v->panel_button += CHIPS_PER_ROW;
	else if (sc == SDL_SCANCODE_UP && v->panel_button >= CHIPS_PER_ROW) v->panel_button -= CHIPS_PER_ROW;
	else if (is_activate(sc)) return changed(settings_turbo_toggle_button(v->settings, (GbaButton) v->panel_button));
	return VACT_NONE;
}

static ViewAction panel_key(View *v, ViewEvent ev) {
	int sc = ev.key;
	Settings *s = v->settings;
	if (sc == SDL_SCANCODE_ESCAPE) {
		close_panel(v);
		return VACT_NONE;
	}
	if (sc == SDL_SCANCODE_TAB) {
		int step = (ev.mods & KMOD_SHIFT) ? PF_COUNT - 1 : 1;
		v->panel_focus = (PanelFocus) (((int) v->panel_focus + step) % PF_COUNT);
		return VACT_NONE;
	}
	int left = sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_UP;
	int right = sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_DOWN;
	switch (v->panel_focus) {
	case PF_SLIDER:
		if (sc == SDL_SCANCODE_LEFT) return changed(settings_step_volume(s, -5));
		if (sc == SDL_SCANCODE_RIGHT) return changed(settings_step_volume(s, 5));
		break;
	case PF_MUTE:
		if (is_activate(sc)) return changed(toggle_flag(&s->muted));
		break;
	case PF_FF:
		if (sc == SDL_SCANCODE_LEFT) return changed(settings_step_speed(s, -1));
		if (sc == SDL_SCANCODE_RIGHT) return changed(settings_step_speed(s, 1));
		break;
	case PF_FF_ALWAYS:
		if (is_activate(sc)) return changed(toggle_flag(&s->ff_always));
		break;
	case PF_TURBO:
		if (is_activate(sc)) return changed(toggle_flag(&s->turbo));
		break;
	case PF_TURBO_RATE:
		if (sc == SDL_SCANCODE_LEFT) return changed(set_rate(v, s->turbo_rate - 1));
		if (sc == SDL_SCANCODE_RIGHT) return changed(set_rate(v, s->turbo_rate + 1));
		break;
	case PF_TURBO_BUTTONS: return panel_key_buttons(v, sc);
	case PF_STICK:
		if (is_activate(sc)) return changed(toggle_flag(&s->stick_on));
		break;
	case PF_STICK_DEADZONE:
		if (sc == SDL_SCANCODE_LEFT) return changed(settings_step_deadzone(s, -STICK_DEADZONE_STEP));
		if (sc == SDL_SCANCODE_RIGHT) return changed(settings_step_deadzone(s, STICK_DEADZONE_STEP));
		break;
	case PF_SCALE:
		if (left) return changed(set_scale(v, SCALE_FILL));
		if (right) return changed(set_scale(v, SCALE_INTEGER));
		break;
	case PF_FULLSCREEN:
		if (is_activate(sc)) return changed(toggle_flag(&s->start_fullscreen));
		break;
	case PF_AUTOCLICK:
		if (is_activate(sc)) return VACT_AUTOCLICK;
		break;
	case PF_CLOSE:
		if (is_activate(sc)) close_panel(v);
		break;
	default: break;
	}
	return VACT_NONE;
}

static ViewAction handle_panel(View *v, ViewEvent ev) {
	switch (ev.type) {
	case VEV_MOUSE_DOWN:
	case VEV_MOUSE_DBL: return panel_mouse_down(v, ev);
	case VEV_MOUSE_MOVE:
		if (v->dragging_slider) return changed(set_volume(v, layout_slider_value(ev.x)));
		if (v->dragging_deadzone) return changed(set_deadzone(v, layout_deadzone_value(ev.x)));
		return VACT_NONE;
	case VEV_MOUSE_UP: v->dragging_slider = v->dragging_deadzone = 0; return VACT_NONE;
	case VEV_KEY: return panel_key(v, ev);
	default: return VACT_NONE;
	}
}

/* ---- entrada principal ---- */

ViewAction view_handle(View *v, ViewEvent ev) {
	if (v->settings_open) return handle_panel(v, ev);
	switch (ev.type) {
	case VEV_MOUSE_MOVE: update_hover(v, ev.x, ev.y); return VACT_NONE;
	case VEV_MOUSE_DOWN:
	case VEV_MOUSE_DBL: return mouse_down(v, ev);
	case VEV_MOUSE_UP: return VACT_NONE;
	case VEV_WHEEL:
		if (rect_contains(layout_regions().table_body, ev.x, ev.y)) {
			v->scroll -= ev.dy * WHEEL_LINES;
			clamp_scroll(v);
		}
		return VACT_NONE;
	case VEV_KEY: return key_library(v, ev);
	case VEV_TEXT:
		if (v->focus == FOCUS_SEARCH && ev.text && textedit_insert(&v->search, ev.text)) refilter(v);
		return VACT_NONE;
	}
	return VACT_NONE;
}
