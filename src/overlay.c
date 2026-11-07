#include "overlay.h"

#include "gfx.h"
#include "layout.h"

_Static_assert(OI_COUNT == OVERLAY_ROWS, "uma linha do painel por item");
_Static_assert(OI_VOLUME == 0 && OI_SPEED == 2, "o layout põe a trilha na linha 0 e os segmentos na linha 2");

void overlay_init(Overlay *o, Settings *settings) {
	o->settings = settings;
	o->open = 0;
	o->focus = OI_VOLUME;
	o->dragging_volume = 0;
	o->macros = NULL;
	o->macro_sel = 0;
	o->macro_state = MUI_IDLE;
	o->confirm_quit = 0;
}

void overlay_open(Overlay *o) {
	o->open = 1;
	o->focus = OI_VOLUME;
	o->dragging_volume = 0;
	o->confirm_quit = 0;
	int count = o->macros ? o->macros->count : 0;
	if (o->macro_sel >= count) o->macro_sel = count > 0 ? count - 1 : 0;
	if (o->macro_sel < 0) o->macro_sel = 0;
}

static OverlayAction changed(int did_change) {
	return did_change ? OACT_SETTINGS_CHANGED : OACT_NONE;
}

static OverlayAction close_panel(Overlay *o) {
	o->open = 0;
	o->dragging_volume = 0;
	return OACT_CLOSE;
}

static OverlayAction set_volume(Overlay *o, int value) {
	int did = o->settings->volume != value;
	o->settings->volume = value;
	return changed(did);
}

static OverlayAction set_speed(Overlay *o, int index) {
	int did = o->settings->ff_index != index;
	o->settings->ff_index = index;
	return changed(did);
}

/* Setas laterais: o volume (+-5), a velocidade (+-1) e a macro escolhida (volta circular) respondem. */
static OverlayAction adjust(Overlay *o, int direction) {
	if (o->focus == OI_VOLUME) return changed(settings_step_volume(o->settings, 5 * direction));
	if (o->focus == OI_SPEED) return changed(settings_step_speed(o->settings, direction));
	if (o->focus == OI_MACRO) {
		int count = o->macros ? o->macros->count : 0;
		if (count > 0) o->macro_sel = (o->macro_sel + direction + count) % count;
	}
	return OACT_NONE;
}

/* `armed` = "Sair do jogo" já tinha sido acionado uma vez (a confirmação pendente é desarmada em overlay_handle). */
static OverlayAction accept(Overlay *o, int armed) {
	switch (o->focus) {
	case OI_QUIT:
		if (armed) return OACT_QUIT_GAME;
		o->confirm_quit = 1;
		return OACT_NONE;
	case OI_MUTE: settings_toggle_mute(o->settings); return OACT_SETTINGS_CHANGED;
	case OI_TURBO: o->settings->turbo = !o->settings->turbo; return OACT_SETTINGS_CHANGED;
	case OI_SAVE: return OACT_SAVE_STATE;
	case OI_LOAD: return OACT_LOAD_STATE;
	case OI_MACRO_PLAY:
		if (o->macro_state == MUI_PLAYING) return OACT_MACRO_STOP;
		return (o->macro_state == MUI_IDLE && o->macros && o->macros->count > 0) ? OACT_MACRO_PLAY : OACT_NONE;
	case OI_MACRO_REC:
		if (o->macro_state == MUI_RECORDING) return OACT_MACRO_STOP;
		return o->macro_state == MUI_IDLE ? OACT_MACRO_RECORD : OACT_NONE;
	case OI_BACK: return close_panel(o);
	default: return OACT_NONE;
	}
}

/* Linha sob o ponto, ou -1. */
static int row_at(int x, int y) {
	for (int i = 0; i < OVERLAY_ROWS; i++) {
		if (rect_contains(layout_overlay_row(i), x, y)) return i;
	}
	return -1;
}

static OverlayAction mouse_down(Overlay *o, OverlayEvent ev, int armed) {
	if (!rect_contains(layout_overlay_panel(), ev.x, ev.y)) return OACT_NONE;
	if (rect_contains(layout_overlay_slider(), ev.x, ev.y)) {
		o->focus = OI_VOLUME;
		o->dragging_volume = 1;
		return set_volume(o, layout_overlay_volume_value(ev.x));
	}
	for (int i = 0; i < FF_SPEED_COUNT; i++) {
		if (rect_contains(layout_overlay_speed(i), ev.x, ev.y)) {
			o->focus = OI_SPEED;
			return set_speed(o, i);
		}
	}
	int row = row_at(ev.x, ev.y);
	if (row < 0) return OACT_NONE;
	o->focus = (OverlayItem) row;
	if (row == OI_MACRO) { /* metade esquerda = anterior, direita = próxima */
		Rect r = layout_overlay_row(row);
		return adjust(o, ev.x < r.x + r.w / 2 ? -1 : 1);
	}
	return accept(o, armed);
}

OverlayAction overlay_handle(Overlay *o, OverlayEvent ev) {
	if (!o->open) return OACT_NONE;
	/* Qualquer ação que não seja mexer o mouse desarma o "Sair do jogo" (o acionamento de novo o rearma ou confirma). */
	int armed = o->confirm_quit;
	if (ev.type != OEV_MOUSE_MOVE && ev.type != OEV_MOUSE_UP) o->confirm_quit = 0;
	switch (ev.type) {
	case OEV_UP: o->focus = (OverlayItem) ((o->focus + OI_COUNT - 1) % OI_COUNT); return OACT_NONE;
	case OEV_DOWN: o->focus = (OverlayItem) ((o->focus + 1) % OI_COUNT); return OACT_NONE;
	case OEV_LEFT: return adjust(o, -1);
	case OEV_RIGHT: return adjust(o, 1);
	case OEV_ACCEPT: return accept(o, armed);
	case OEV_BACK: return close_panel(o);
	case OEV_MOUSE_DOWN: return mouse_down(o, ev, armed);
	case OEV_MOUSE_UP: o->dragging_volume = 0; return OACT_NONE;
	case OEV_MOUSE_MOVE: {
		if (o->dragging_volume) return set_volume(o, layout_overlay_volume_value(ev.x));
		int row = row_at(ev.x, ev.y);
		if (row >= 0) o->focus = (OverlayItem) row;
		return OACT_NONE;
	}
	}
	return OACT_NONE;
}
