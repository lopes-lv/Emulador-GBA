#include "ui_draw.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include "icons.h"
#include "input.h"
#include "layout.h"
#include "theme.h"
#include "ttf.h"

/* Todas as medidas abaixo são LÓGICAS (as do layout, 1280x720); o Canvas as converte para pixels reais com a escala
 * atual, alinhando bordas à grade, e o texto é gerado no tamanho final em pixels. */

static const char *const COLUMN_TITLES[TABLE_COLS] = {
	"T\xC3\xADtulo", "Plataforma", "G\xC3\xAAnero", "Ano", "Tempo de jogo", "\xC3\x9Altima sess\xC3\xA3o"};

static const char *const TOOL_LABELS[TB_COUNT] = {"Adicionar jogo", "Jogar", "Configura\xC3\xA7\xC3\xB5""es", "Controles"};
static const IconId TOOL_ICONS[TB_COUNT] = {ICON_ADD, ICON_PLAY, ICON_SETTINGS, ICON_GAMEPAD};

/* Cores de fundo das miniaturas sem arte; todas têm contraste >= 4,5:1 com o branco da inicial. */
static const uint32_t PLACEHOLDERS[] = {RGB(0x0F, 0x76, 0x6E), RGB(0x9D, 0x17, 0x4D), RGB(0xB4, 0x53, 0x09),
                                        RGB(0x1D, 0x4E, 0xD8), RGB(0x6D, 0x28, 0xD9), RGB(0x4D, 0x7C, 0x0F)};

/* ---- helpers ---- */

static void fill(Canvas *c, Rect r, uint32_t color) { canvas_fill(c, r, color); }

static Rect inset(Rect r, int d) { return (Rect) {r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d}; }

static int px_of(const Canvas *c, FontSize size) { return ttf_px_size(size, c->scale); }

static void text_left(Canvas *c, FontSize size, Rect box, const char *s, uint32_t color) {
	ttf_draw_fit_px(c->px, c->w, c->h, px_of(c, size), canvas_device_rect(c, box), s, color);
}

static void text_center(Canvas *c, FontSize size, Rect r, const char *s, uint32_t color) {
	int px = px_of(c, size);
	Rect d = canvas_device_rect(c, r);
	int tw = ttf_width_px(px, s);
	if (tw > d.w) tw = d.w;
	Rect box = {d.x + (d.w - tw) / 2, d.y, tw + 2, d.h};
	if (box.x + box.w > d.x + d.w) box.w = d.x + d.w - box.x;
	ttf_draw_fit_px(c->px, c->w, c->h, px, box, s, color);
}

/* Anel de foco: 2 linhas, por fora do retângulo. */
static void ring(Canvas *c, Rect r, uint32_t color) {
	canvas_frame_n(c, (Rect) {r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 2, color);
}

static void check_mark(Canvas *c, Rect r, uint32_t color) {
	canvas_line(c, (float) r.x + 3.5f, (float) r.y + 8.5f, (float) r.x + 6.5f, (float) r.y + 11.5f, 1.8f, color);
	canvas_line(c, (float) r.x + 6.5f, (float) r.y + 11.5f, (float) r.x + 12.5f, (float) r.y + 4.5f, 1.8f, color);
}

static void checkbox(Canvas *c, Rect row, int checked) {
	Rect b = {row.x, row.y + (row.h - 16) / 2, 16, 16};
	if (checked) {
		canvas_fill_round(c, b, 4, THEME_ACCENT);
		check_mark(c, b, THEME_WHITE);
	} else {
		canvas_box(c, b, 4, THEME_BORDER2, THEME_WINDOW);
	}
}

static void radio(Canvas *c, Rect row, int selected) {
	float cx = (float) row.x + 8.0f, cy = (float) row.y + (float) (row.h / 2);
	canvas_disc(c, cx, cy, 8.0f, selected ? THEME_ACCENT : THEME_BORDER2);
	canvas_disc(c, cx, cy, 7.0f, THEME_WINDOW);
	if (selected) canvas_disc(c, cx, cy, 4.0f, THEME_ACCENT);
}

/* ---- barra de título e ferramentas ---- */

static void draw_title(Canvas *c, const View *v, const StatusInfo *st) {
	Rect t = layout_regions().title;
	fill(c, t, THEME_BAR);
	icon_draw_c(c, ICON_APP, 12, 8, 16, THEME_TEXT);
	text_left(c, FS_BODY, (Rect) {36, 0, 220, t.h}, "Retr\xC3\xB4 Emu", THEME_TEXT);
	for (int b = 0; b < 3; b++) {
		Rect r = layout_window_button((WindowButton) b);
		if (v->hover_kind == HOVER_WINBTN && v->hover_item == b) fill(c, r, THEME_HOVER);
		IconId id = b == WB_MIN ? ICON_MINIMIZE : b == WB_MAX ? (st->maximized ? ICON_RESTORE : ICON_MAXIMIZE) : ICON_CLOSE;
		icon_draw_c(c, id, r.x + (r.w - 16) / 2, r.y + (r.h - 16) / 2, 16, THEME_TEXT);
	}
}

static int tool_enabled(const View *v, ToolButton b) {
	return b != TB_PLAY || (v->can_play && v->selected >= 0);
}

static void draw_toolbar(Canvas *c, const View *v, const StatusInfo *st) {
	Rect tb = layout_regions().toolbar;
	fill(c, tb, THEME_PANEL2);
	canvas_edge(c, tb, EDGE_BOTTOM, THEME_BORDER);

	for (int b = 0; b < TB_COUNT; b++) {
		Rect r = layout_tool_button((ToolButton) b);
		int enabled = tool_enabled(v, (ToolButton) b);
		uint32_t color = enabled ? THEME_TEXT : THEME_DIM;
		if (enabled && v->hover_kind == HOVER_TOOL && v->hover_item == b) canvas_fill_round(c, r, 4, THEME_HOVER);
		icon_draw_c(c, TOOL_ICONS[b], r.x + 12, r.y + (r.h - 16) / 2, 16, color);
		text_left(c, FS_BODY, (Rect) {r.x + 36, r.y, r.w - 44, r.h}, TOOL_LABELS[b], color);
		if (v->focus == FOCUS_TOOLBAR && v->focus_item == b) ring(c, r, THEME_ACCENT);
	}
	Rect play = layout_tool_button(TB_PLAY);
	canvas_edge(c, (Rect) {play.x + play.w + 8, play.y + 6, 1, play.h - 12}, EDGE_LEFT, THEME_BORDER2);

	Rect s = layout_search();
	int focused = v->focus == FOCUS_SEARCH;
	canvas_box(c, s, 4, focused ? THEME_ACCENT : THEME_BORDER2, THEME_WINDOW);
	icon_draw_c(c, ICON_SEARCH, s.x + s.w - 28, s.y + (s.h - 16) / 2, 16, THEME_DIM);
	Rect text_box = {s.x + 12, s.y, s.w - 52, s.h};
	if (v->search.text[0]) {
		text_left(c, FS_BODY, text_box, v->search.text, THEME_TEXT);
	} else {
		text_left(c, FS_BODY, text_box, "Buscar", THEME_DIM);
	}
	if (focused && st->cursor_on) {
		int tw = (int) ((float) ttf_width_px(px_of(c, FS_BODY), v->search.text) / c->scale + 0.5f);
		if (tw > text_box.w - 2) tw = text_box.w - 2;
		canvas_edge(c, (Rect) {text_box.x + tw + 1, s.y + 8, 1, s.h - 16}, EDGE_LEFT, THEME_TEXT);
	}
}

/* ---- tabela ---- */

static uint32_t hash_title(const char *s) {
	uint32_t h = 2166136261u;
	for (; *s; s++) h = (h ^ (unsigned char) *s) * 16777619u;
	return h;
}

static void draw_thumb(Canvas *c, Rect r, const LibEntry *e) {
	if (e->has_thumb) {
		canvas_blit(c, r, e->thumb, LIB_THUMB, LIB_THUMB, 4);
		return;
	}
	canvas_fill_round(c, r, 4, PLACEHOLDERS[hash_title(e->title) % (sizeof(PLACEHOLDERS) / sizeof(PLACEHOLDERS[0]))]);
	char initial[5] = {0};
	if (e->title[0]) {
		unsigned char ch = (unsigned char) e->title[0];
		int n = ch < 0x80 ? 1 : ch >= 0xF0 ? 4 : ch >= 0xE0 ? 3 : ch >= 0xC0 ? 2 : 1;
		memcpy(initial, e->title, (size_t) n);
		if (n == 1) initial[0] = (char) toupper(ch);
	}
	text_center(c, FS_BODY, r, initial, THEME_WHITE);
}

static void draw_table(Canvas *c, const View *v, const StatusInfo *st) {
	Regions rg = layout_regions();
	fill(c, rg.table_header, THEME_PANEL2);
	canvas_edge(c, rg.table_header, EDGE_BOTTOM, THEME_BORDER);
	for (int col_i = 0; col_i < TABLE_COLS; col_i++) {
		Rect col = layout_column(col_i);
		text_left(c, FS_SMALL, (Rect) {col.x + 12, rg.table_header.y, col.w - 24, rg.table_header.h}, COLUMN_TITLES[col_i], THEME_DIM);
	}
	fill(c, rg.table_body, THEME_WINDOW);

	int rows = layout_visible_rows();
	for (int i = 0; i < rows; i++) {
		int abs = v->scroll + i;
		if (abs >= v->visible_count) break;
		const LibEntry *e = &v->lib->items[v->visible[abs]];
		Rect row = layout_row(i);
		int selected = abs == v->selected;
		int hover = v->hover_kind == HOVER_ROW && v->hover_item == abs;
		uint32_t bg = selected ? THEME_ACCENT : hover ? THEME_HOVER : (abs % 2 ? THEME_STRIPE : THEME_WINDOW);
		uint32_t main_color = selected ? THEME_WHITE : THEME_TEXT;
		uint32_t dim_color = selected ? THEME_WHITE : THEME_DIM;
		fill(c, row, bg);

		Rect col0 = layout_column(0);
		Rect thumb = {col0.x + 12, row.y + (row.h - THUMB) / 2, THUMB, THUMB};
		draw_thumb(c, thumb, e);
		int tx = thumb.x + THUMB + 12;
		text_left(c, FS_BODY, (Rect) {tx, row.y, col0.x + col0.w - 12 - tx, row.h}, e->title, main_color);

		char buf[64];
		const char *cells[TABLE_COLS] = {NULL};
		cells[1] = e->platform;
		cells[2] = e->genre[0] ? e->genre : "\xE2\x80\x94";
		if (e->year > 0) {
			snprintf(buf, sizeof(buf), "%d", e->year);
			cells[3] = buf;
		} else {
			cells[3] = "\xE2\x80\x94";
		}
		char playtime[32], last[32];
		library_format_playtime(e->playtime_s, playtime, sizeof(playtime));
		library_format_last(e->last_played, st->now, last, sizeof(last));
		cells[4] = playtime;
		cells[5] = last;
		for (int col_i = 1; col_i < TABLE_COLS; col_i++) {
			Rect col = layout_column(col_i);
			text_left(c, FS_BODY, (Rect) {col.x + 12, row.y, col.w - 24, row.h}, cells[col_i], dim_color);
		}
		if (selected && v->focus == FOCUS_TABLE) canvas_frame(c, inset(row, 1), THEME_WHITE);
	}

	if (v->visible_count > rows) {
		Rect body = rg.table_body;
		int track = body.h - 4;
		int thumb_h = track * rows / v->visible_count;
		if (thumb_h < 24) thumb_h = 24;
		int span = v->visible_count - rows;
		int y = body.y + 2 + (track - thumb_h) * v->scroll / (span > 0 ? span : 1);
		canvas_fill_round(c, (Rect) {body.x + body.w - 8, y, 6, thumb_h}, 3, THEME_BORDER2);
	}
}

/* ---- barra de status ---- */

static void status_strings(const StatusInfo *st, char *fps, size_t n) {
	snprintf(fps, n, "%d FPS", st->fps);
}

/* Posições da barra de status em unidades lógicas, com as larguras do texto na escala 1. */
Rect ui_status_dot_rect(const StatusInfo *st) {
	Rect s = layout_regions().status;
	char fps[32];
	status_strings(st, fps, sizeof(fps));
	int fps_x = s.x + s.w - 16 - ttf_width(FS_SMALL, fps);
	return (Rect) {fps_x - 8 - 8, s.y + (s.h - 8) / 2, 8, 8};
}

static void draw_status(Canvas *c, const View *v, const StatusInfo *st) {
	Rect s = layout_regions().status;
	fill(c, s, THEME_BAR);

	char left[512];
	if (st->message && st->message[0]) {
		snprintf(left, sizeof(left), "%s", st->message);
	} else {
		const LibEntry *sel = view_selected(v);
		snprintf(left, sizeof(left), "%d %s \xC2\xB7 Selecionado: %s", v->visible_count, v->visible_count == 1 ? "jogo" : "jogos",
		         sel ? sel->title : "\xE2\x80\x94");
	}
	Rect dot = ui_status_dot_rect(st);
	char ready[] = "N\xC3\xBA""cleo pronto";
	int ready_w = ttf_width(FS_SMALL, ready);
	int right_edge = st->core_ready ? dot.x - 16 - ready_w : dot.x;
	text_left(c, FS_SMALL, (Rect) {16, s.y, right_edge - 32, s.h}, left, THEME_TEXT);

	char fps[32];
	status_strings(st, fps, sizeof(fps));
	text_left(c, FS_SMALL, (Rect) {dot.x + 16, s.y, s.x + s.w - 8 - (dot.x + 16), s.h}, fps, THEME_TEXT);
	canvas_disc(c, (float) dot.x + 4.0f, (float) dot.y + 4.0f, 4.0f, THEME_FPS);
	if (st->core_ready) text_left(c, FS_SMALL, (Rect) {dot.x - 16 - ready_w, s.y, ready_w + 2, s.h}, ready, THEME_TEXT);
}

/* ---- painel Configurações ---- */

static void draw_check_row(Canvas *c, Rect row, int checked, const char *label) {
	checkbox(c, row, checked);
	text_left(c, FS_BODY, (Rect) {row.x + 28, row.y, row.w - 28, row.h}, label, THEME_TEXT);
}

static void draw_segments(Canvas *c, Rect (*seg)(int), int count, int active, const char *const *labels) {
	for (int i = 0; i < count; i++) {
		Rect r = seg(i);
		int on = active == i;
		fill(c, r, on ? THEME_ACCENT : THEME_PANEL2);
		canvas_frame(c, r, on ? THEME_ACCENT : THEME_BORDER2);
		text_center(c, FS_BODY, r, labels[i], on ? THEME_WHITE : THEME_TEXT);
	}
}

static void draw_panel(Canvas *c, const View *v) {
	const Settings *s = v->settings;
	Rect p = layout_settings_panel();
	canvas_box(c, p, 4, THEME_BORDER2, THEME_PANEL);
	text_left(c, FS_BODY, (Rect) {p.x + 24, p.y + 14, 400, 24}, "Configura\xC3\xA7\xC3\xB5""es", THEME_TEXT);

	/* Som */
	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + 56, 80, 20}, "Som", THEME_DIM);
	text_left(c, FS_BODY, (Rect) {p.x + 104, p.y + 56, 200, 20}, "Volume", THEME_TEXT);
	Rect tr = layout_slider();
	int track_y = tr.y + (tr.h - 4) / 2;
	int fw = tr.w * s->volume / 100;
	canvas_fill_round(c, (Rect) {tr.x, track_y, tr.w, 4}, 2, THEME_BORDER2);
	if (fw > 0) canvas_fill_round(c, (Rect) {tr.x, track_y, fw, 4}, 2, THEME_ACCENT);
	canvas_disc(c, (float) (tr.x + fw), (float) (tr.y + tr.h / 2), 7.0f, THEME_WHITE);
	char vol[16];
	snprintf(vol, sizeof(vol), "%d%%", s->volume);
	text_left(c, FS_BODY, (Rect) {tr.x + tr.w + 16, tr.y - 4, 56, 24}, vol, THEME_TEXT);
	Rect mute = layout_mute_box();
	draw_check_row(c, mute, s->muted, "Mudo");

	/* Velocidade */
	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + 158, 80, 20}, "Velocidade", THEME_DIM);
	text_left(c, FS_BODY, (Rect) {p.x + 104, p.y + 158, 200, 20}, "Avan\xC3\xA7o r\xC3\xA1pido", THEME_TEXT);
	static const char *const SPEED_LABELS[FF_SPEED_COUNT] = {"1,5x", "2x", "3x", "4x", "6x", "8x"};
	draw_segments(c, layout_ff_segment, FF_SPEED_COUNT, s->ff_index, SPEED_LABELS);
	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + 224, 400, 20},
	          "Segure Tab (ou RT no controle) para acelerar; F4 liga e desliga.", THEME_DIM);
	Rect always = layout_ff_always_box();
	draw_check_row(c, always, s->ff_always, "Manter o avan\xC3\xA7o r\xC3\xA1pido sempre ativo");

	/* Tela */
	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + 290, 200, 20}, "Tela", THEME_DIM);
	static const char *const SCALE_LABELS[2] = {"Preencher a tela (mant\xC3\xA9m a propor\xC3\xA7\xC3\xA3o)", "Inteira (pixels n\xC3\xADtidos)"};
	for (int i = 0; i < 2; i++) {
		Rect r = layout_scale_radio(i);
		radio(c, r, (int) s->scale == i);
		text_left(c, FS_BODY, (Rect) {r.x + 28, r.y, r.w - 28, r.h}, SCALE_LABELS[i], THEME_TEXT);
	}
	Rect fs = layout_fullscreen_box();
	draw_check_row(c, fs, s->start_fullscreen, "Abrir jogos em tela cheia");

	/* Turbo */
	Rect turbo = layout_turbo_box();
	text_left(c, FS_SMALL, (Rect) {turbo.x, p.y + 56, 120, 20}, "Turbo", THEME_DIM);
	draw_check_row(c, turbo, s->turbo, "Repetir bot\xC3\xA3o ao segurar (turbo)");
	text_left(c, FS_SMALL, (Rect) {turbo.x, p.y + 116, 300, 20}, "Velocidade da repeti\xC3\xA7\xC3\xA3o", THEME_DIM);
	static const char *const RATE_LABELS[TURBO_RATE_COUNT] = {"5/s", "10/s", "15/s", "30/s"};
	draw_segments(c, layout_turbo_rate, TURBO_RATE_COUNT, s->turbo_rate, RATE_LABELS);
	text_left(c, FS_SMALL, (Rect) {turbo.x, p.y + 186, 300, 20}, "Bot\xC3\xB5""es que repetem", THEME_DIM);
	for (int i = 0; i < BTN_COUNT; i++) {
		Rect r = layout_turbo_button(i);
		int on = settings_turbo_has(s, (GbaButton) i);
		Rect chip = inset(r, 2);
		fill(c, chip, on ? THEME_ACCENT : THEME_PANEL2);
		canvas_frame(c, chip, on ? THEME_ACCENT : THEME_BORDER2);
		text_center(c, FS_BODY, chip, bindings_button_label((GbaButton) i), on ? THEME_WHITE : THEME_TEXT);
	}

	/* Analógico */
	Rect stick = layout_stick_box();
	text_left(c, FS_SMALL, (Rect) {stick.x, p.y + 296, 200, 20}, "Anal\xC3\xB3gico", THEME_DIM);
	draw_check_row(c, stick, s->stick_on, "Usar o anal\xC3\xB3gico como direcional");
	text_left(c, FS_SMALL, (Rect) {stick.x, p.y + 352, 300, 20}, "Zona morta", THEME_DIM);
	Rect dz = layout_deadzone_slider();
	int dz_y = dz.y + (dz.h - 4) / 2;
	int dz_fill = dz.w * (s->stick_deadzone - STICK_DEADZONE_MIN) / (STICK_DEADZONE_MAX - STICK_DEADZONE_MIN);
	canvas_fill_round(c, (Rect) {dz.x, dz_y, dz.w, 4}, 2, THEME_BORDER2);
	if (dz_fill > 0) canvas_fill_round(c, (Rect) {dz.x, dz_y, dz_fill, 4}, 2, THEME_ACCENT);
	canvas_disc(c, (float) (dz.x + dz_fill), (float) (dz.y + dz.h / 2), 7.0f, THEME_WHITE);
	char dz_text[16];
	snprintf(dz_text, sizeof(dz_text), "%d%%", s->stick_deadzone);
	text_left(c, FS_BODY, (Rect) {dz.x + dz.w + 16, dz.y - 4, 56, 24}, dz_text, THEME_TEXT);

	Rect close = layout_settings_close();
	canvas_box(c, close, 4, THEME_BORDER2, THEME_PANEL2);
	text_center(c, FS_BODY, close, "Fechar", THEME_TEXT);
	Rect autoclick = layout_autoclick_button();
	canvas_box(c, autoclick, 4, THEME_BORDER2, THEME_PANEL2);
	text_center(c, FS_BODY, autoclick, "Auto clique", THEME_TEXT);

	/* anel de foco */
	switch (v->panel_focus) {
	case PF_SLIDER: ring(c, tr, THEME_ACCENT); break;
	case PF_MUTE: ring(c, (Rect) {mute.x - 4, mute.y, 80, mute.h}, THEME_ACCENT); break;
	case PF_FF: ring(c, layout_ff_segment(s->ff_index), THEME_WHITE); break;
	case PF_FF_ALWAYS: ring(c, (Rect) {always.x - 4, always.y, 300, always.h}, THEME_ACCENT); break;
	case PF_TURBO: ring(c, (Rect) {turbo.x - 4, turbo.y, 270, turbo.h}, THEME_ACCENT); break;
	case PF_TURBO_RATE: ring(c, layout_turbo_rate(s->turbo_rate), THEME_WHITE); break;
	case PF_TURBO_BUTTONS: ring(c, layout_turbo_button(v->panel_button), THEME_WHITE); break;
	case PF_STICK: ring(c, (Rect) {stick.x - 4, stick.y, 300, stick.h}, THEME_ACCENT); break;
	case PF_STICK_DEADZONE: ring(c, dz, THEME_ACCENT); break;
	case PF_SCALE: {
		Rect r = layout_scale_radio((int) s->scale);
		ring(c, (Rect) {r.x - 4, r.y, 330, r.h}, THEME_ACCENT);
		break;
	}
	case PF_FULLSCREEN: ring(c, (Rect) {fs.x - 4, fs.y, 240, fs.h}, THEME_ACCENT); break;
	case PF_AUTOCLICK: ring(c, autoclick, THEME_ACCENT); break;
	case PF_CLOSE: ring(c, close, THEME_ACCENT); break;
	default: break;
	}
}

/* ---- menu do jogo ---- */

static void draw_overlay_check(Canvas *c, Rect row, int checked, const char *label) {
	checkbox(c, (Rect) {row.x + 12, row.y, 24, row.h}, checked);
	text_left(c, FS_BODY, (Rect) {row.x + 44, row.y, row.w - 56, row.h}, label, THEME_TEXT);
}

/* Linha de ação com a tecla do atalho à direita. */
static void draw_overlay_action(Canvas *c, Rect row, const char *label, const char *shortcut) {
	text_left(c, FS_BODY, (Rect) {row.x + 12, row.y, row.w - 100, row.h}, label, THEME_TEXT);
	text_left(c, FS_SMALL, (Rect) {row.x + row.w - 60, row.y, 48, row.h}, shortcut, THEME_DIM);
}

/* Linha só com rótulo; THEME_DIM quando o item está desligado. */
static void draw_overlay_label(Canvas *c, Rect row, const char *label, uint32_t color) {
	text_left(c, FS_BODY, (Rect) {row.x + 12, row.y, row.w - 24, row.h}, label, color);
}

static void draw_overlay_macros(Canvas *c, const Overlay *o) {
	const MacroList *ml = o->macros;
	int has_macros = ml && ml->count > 0;
	int playing = o->macro_state == MUI_PLAYING, recording = o->macro_state == MUI_RECORDING;

	Rect rm = layout_overlay_row(OI_MACRO);
	text_left(c, FS_BODY, (Rect) {rm.x + 12, rm.y, 80, rm.h}, "Macro", THEME_TEXT);
	if (has_macros) {
		const Macro *m = &ml->items[o->macro_sel < ml->count ? o->macro_sel : 0];
		char name[MACRO_NAME_BYTES + 8];
		if (ml->count > 1) snprintf(name, sizeof(name), "\xE2\x80\xB9 %s \xE2\x80\xBA", m->name); /* ‹ nome › */
		else snprintf(name, sizeof(name), "%s", m->name);
		text_left(c, FS_BODY, (Rect) {rm.x + 100, rm.y, 270, rm.h}, name, THEME_TEXT);
		int tenths = (int) (m->total_frames * 10 / 60);
		char info[48];
		snprintf(info, sizeof(info), "%d passo%s \xC2\xB7 %d,%d s", m->count, m->count == 1 ? "" : "s", tenths / 10, tenths % 10);
		text_left(c, FS_SMALL, (Rect) {rm.x + rm.w - 150, rm.y, 140, rm.h}, info, THEME_DIM);
	} else {
		text_left(c, FS_BODY, (Rect) {rm.x + 100, rm.y, rm.w - 112, rm.h}, "Nenhuma macro gravada", THEME_DIM);
	}
	draw_overlay_label(c, layout_overlay_row(OI_MACRO_PLAY), playing ? "Parar macro" : "Tocar macro",
	                   (playing || (has_macros && !recording)) ? THEME_TEXT : THEME_DIM);
	draw_overlay_label(c, layout_overlay_row(OI_MACRO_REC), recording ? "Parar e salvar grava\xC3\xA7\xC3\xA3o" : "Gravar nova macro",
	                   playing ? THEME_DIM : THEME_TEXT);
}

void ui_draw_overlay(Canvas *c, const Overlay *o) {
	const Settings *s = o->settings;
	Rect p = layout_overlay_panel();
	canvas_box(c, p, 4, THEME_BORDER2, THEME_PANEL);
	text_left(c, FS_BODY, (Rect) {p.x + 24, p.y + 16, 200, 24}, "Menu do jogo", THEME_TEXT);
	text_left(c, FS_SMALL, (Rect) {p.x + 150, p.y + 19, 200, 20}, "Jogo pausado", THEME_DIM);

	for (int i = 0; i < OVERLAY_ROWS; i++) {
		if ((int) o->focus != i) continue;
		Rect r = layout_overlay_row(i);
		fill(c, r, THEME_HOVER);
		fill(c, (Rect) {r.x, r.y, 3, r.h}, THEME_ACCENT);
	}

	Rect vol = layout_overlay_row(OI_VOLUME);
	text_left(c, FS_BODY, (Rect) {vol.x + 12, vol.y, 200, vol.h}, "Volume", THEME_TEXT);
	Rect tr = layout_overlay_slider();
	int track_y = tr.y + (tr.h - 4) / 2;
	int fw = tr.w * s->volume / 100;
	canvas_fill_round(c, (Rect) {tr.x, track_y, tr.w, 4}, 2, THEME_BORDER2);
	if (fw > 0) canvas_fill_round(c, (Rect) {tr.x, track_y, fw, 4}, 2, THEME_ACCENT);
	canvas_disc(c, (float) (tr.x + fw), (float) (tr.y + tr.h / 2), 7.0f, THEME_WHITE);
	char vol_text[16];
	snprintf(vol_text, sizeof(vol_text), "%d%%", s->volume);
	text_left(c, FS_BODY, (Rect) {tr.x + tr.w + 16, vol.y, 48, vol.h}, vol_text, THEME_TEXT);

	draw_overlay_check(c, layout_overlay_row(OI_MUTE), s->muted, "Mudo");

	Rect speed = layout_overlay_row(OI_SPEED);
	text_left(c, FS_BODY, (Rect) {speed.x + 12, speed.y, 200, speed.h}, "Avan\xC3\xA7o r\xC3\xA1pido", THEME_TEXT);
	static const char *const SPEED_LABELS[FF_SPEED_COUNT] = {"1,5x", "2x", "3x", "4x", "6x", "8x"};
	draw_segments(c, layout_overlay_speed, FF_SPEED_COUNT, s->ff_index, SPEED_LABELS);

	draw_overlay_check(c, layout_overlay_row(OI_TURBO), s->turbo, "Turbo (repetir bot\xC3\xA3o ao segurar)");
	draw_overlay_action(c, layout_overlay_row(OI_SAVE), "Salvar estado", "F5");
	draw_overlay_action(c, layout_overlay_row(OI_LOAD), "Carregar estado", "F8");
	draw_overlay_macros(c, o);
	Rect quit = layout_overlay_row(OI_QUIT);
	if (o->confirm_quit) fill(c, quit, THEME_ACCENT); /* aguardando o segundo acionamento */
	draw_overlay_label(c, quit, o->confirm_quit ? "Confirmar: sair do jogo?" : "Sair do jogo", o->confirm_quit ? THEME_WHITE : THEME_TEXT);
	draw_overlay_action(c, layout_overlay_row(OI_BACK), "Voltar ao jogo", "F12");

	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + p.h - 34, p.w - 48, 20},
	          "Setas ou anal\xC3\xB3gico: navegar \xC2\xB7 Enter ou A: confirmar \xC2\xB7 Esc ou B: voltar", THEME_DIM);
}

/* ---- editor de macros ---- */

static const char *const CHIP_LABELS[BTN_COUNT] = {"A", "B", "L", "R", "Sta", "Sel", "Cim", "Bai", "Esq", "Dir"};

/* Botão com borda; hot = ação armada (em destaque); dim = desligado (texto apagado). */
static void editor_button(Canvas *c, Rect r, const char *label, int hot, int dim) {
	canvas_box(c, r, 4, hot ? THEME_ACCENT : THEME_BORDER2, hot ? THEME_ACCENT : THEME_PANEL2);
	text_center(c, FS_BODY, r, label, hot ? THEME_WHITE : (dim ? THEME_DIM : THEME_TEXT));
}

static Rect rect_union(Rect a, Rect b) {
	int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
	int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
	return (Rect) {x0, y0, x1 - x0, y1 - y0};
}

/* Cursor piscante depois de `shown`, dentro de `box`. */
static void editor_cursor(Canvas *c, Rect box, const char *shown, int inset_y) {
	int tw = (int) ((float) ttf_width_px(px_of(c, FS_BODY), shown) / c->scale + 0.5f);
	if (tw > box.w - 2) tw = box.w - 2;
	canvas_edge(c, (Rect) {box.x + tw + 1, box.y + inset_y, 1, box.h - 2 * inset_y}, EDGE_LEFT, THEME_TEXT);
}

static void draw_editor_steps(Canvas *c, const StatusInfo *st, const Editor *e, const Macro *m) {
	Rect hr = layout_editor_step_row(0);
	text_left(c, FS_SMALL, (Rect) {hr.x + 8, hr.y - 24, 32, 20}, "#", THEME_DIM);
	text_left(c, FS_SMALL, (Rect) {hr.x + 40, hr.y - 24, 200, 20}, "Bot\xC3\xB5""es", THEME_DIM);
	text_left(c, FS_SMALL, (Rect) {hr.x + 500, hr.y - 24, 200, 20}, "Dura\xC3\xA7\xC3\xA3o", THEME_DIM);
	if (m->count == 0) text_left(c, FS_BODY, (Rect) {hr.x + 8, hr.y, 500, hr.h}, "Sem passos. Use Adicionar.", THEME_DIM);
	int in_steps = e->zone == EZ_STEPS || e->zone == EZ_BUTTONS;
	for (int i = 0; i < EDITOR_STEP_ROWS; i++) {
		int idx = e->step_scroll + i;
		if (idx >= m->count) break;
		Rect r = layout_editor_step_row(i);
		int sel = idx == e->step;
		if (sel) {
			fill(c, r, THEME_HOVER);
			fill(c, (Rect) {r.x, r.y, 3, r.h}, in_steps ? THEME_ACCENT : THEME_BORDER2);
		}
		char num[12];
		snprintf(num, sizeof(num), "%d", idx + 1);
		text_left(c, FS_SMALL, (Rect) {r.x + 8, r.y, 28, r.h}, num, THEME_DIM);
		for (int b = 0; b < BTN_COUNT; b++) {
			int on = (m->steps[idx].keys & input_button_bit((GbaButton) b)) != 0;
			Rect chip = inset(layout_editor_chip(i, b), 2);
			fill(c, chip, on ? THEME_ACCENT : THEME_PANEL2);
			canvas_frame(c, chip, on ? THEME_ACCENT : THEME_BORDER2);
			text_center(c, FS_SMALL, chip, CHIP_LABELS[b], on ? THEME_WHITE : THEME_DIM);
			if (sel && e->zone == EZ_BUTTONS && b == e->chip) ring(c, chip, THEME_WHITE);
		}
		editor_button(c, layout_editor_dur_minus(i), "-", 0, 0);
		editor_button(c, layout_editor_dur_plus(i), "+", 0, 0);
		Rect dv = layout_editor_dur_value(i);
		if (sel && e->dur_editing) {
			canvas_box(c, dv, 4, THEME_ACCENT, THEME_WINDOW);
			Rect tb = {dv.x + 8, dv.y, dv.w - 16, dv.h};
			text_left(c, FS_BODY, tb, e->dur_buf, THEME_TEXT);
			if (st->cursor_on) editor_cursor(c, tb, e->dur_buf, 5);
		} else {
			char frames[24], ms[24];
			snprintf(frames, sizeof(frames), "%u qd", (unsigned) m->steps[idx].frames);
			snprintf(ms, sizeof(ms), "%d ms", layout_editor_dur_ms(m->steps[idx].frames));
			text_left(c, FS_BODY, (Rect) {dv.x + 6, dv.y, 62, dv.h}, frames, THEME_TEXT);
			text_left(c, FS_SMALL, (Rect) {dv.x + 66, dv.y, 54, dv.h}, ms, THEME_DIM);
		}
	}
	static const char *const BUTTONS[ESB_COUNT] = {"Adicionar", "Duplicar", "Subir", "Descer", "Remover"};
	for (int b = 0; b < ESB_COUNT; b++) editor_button(c, layout_editor_step_btn(b), BUTTONS[b], 0, e->step < 0 && b != ESB_ADD);
}

void ui_draw_editor(Canvas *c, const View *v, const StatusInfo *st, const Editor *e) {
	View backdrop = *v;
	backdrop.settings_open = 0;
	ui_draw_library(c, &backdrop, st);
	Rect p = layout_editor_panel();
	canvas_box(c, p, 4, THEME_BORDER2, THEME_PANEL);
	char title[320];
	snprintf(title, sizeof(title), "Auto clique: %s", e->title);
	text_left(c, FS_BODY, (Rect) {p.x + 24, p.y + 14, 900, 24}, title, THEME_TEXT);

	/* lista de macros */
	const MacroList *l = &e->list;
	Rect row0 = layout_editor_list_row(0), rowN = layout_editor_list_row(EDITOR_LIST_ROWS - 1);
	text_left(c, FS_SMALL, (Rect) {p.x + 16, p.y + 38, 200, 18}, "Macros", THEME_DIM);
	if (l->count == 0) text_left(c, FS_BODY, (Rect) {row0.x + 12, row0.y, row0.w - 24, row0.h}, "Nenhuma macro. Use Novo.", THEME_DIM);
	for (int i = 0; i < EDITOR_LIST_ROWS; i++) {
		int idx = e->list_scroll + i;
		if (idx >= l->count) break;
		Rect r = layout_editor_list_row(i);
		int sel = idx == e->sel;
		if (sel) canvas_fill_round(c, r, 4, THEME_ACCENT);
		text_left(c, FS_BODY, (Rect) {r.x + 12, r.y, r.w - 36, r.h}, l->items[idx].name, sel ? THEME_WHITE : THEME_TEXT);
		if (e->has_state && e->has_state[idx]) {
			canvas_disc(c, (float) (r.x + r.w - 14), (float) (r.y + r.h / 2), 3.0f, sel ? THEME_WHITE : THEME_FPS);
		}
	}
	if (e->zone == EZ_LIST) ring(c, rect_union(row0, rowN), THEME_ACCENT);
	editor_button(c, layout_editor_list_btn(EB_NEW), "Novo", 0, 0);
	editor_button(c, layout_editor_list_btn(EB_DUP), "Duplicar", 0, e->sel < 0);
	editor_button(c, layout_editor_list_btn(EB_DEL), e->confirm_delete ? "Confirmar?" : "Apagar", e->confirm_delete, e->sel < 0);

	/* detalhes da macro escolhida */
	const Macro *m = (e->sel >= 0 && e->sel < l->count) ? &l->items[e->sel] : NULL;
	Rect nm = layout_editor_name();
	if (!m) {
		text_left(c, FS_BODY, (Rect) {nm.x, nm.y, 700, nm.h}, "Escolha uma macro na lista ou crie uma nova.", THEME_DIM);
	} else {
		text_left(c, FS_SMALL, (Rect) {nm.x, nm.y - 18, 100, 18}, "Nome", THEME_DIM);
		canvas_box(c, nm, 4, (e->name_editing || e->zone == EZ_NAME) ? THEME_ACCENT : THEME_BORDER2, THEME_WINDOW);
		Rect tb = {nm.x + 12, nm.y, nm.w - 24, nm.h};
		const char *shown = e->name_editing ? e->name.text : m->name;
		text_left(c, FS_BODY, tb, shown, THEME_TEXT);
		if (e->name_editing && st->cursor_on) editor_cursor(c, tb, shown, 8);
		int tenths = (int) (m->total_frames * 10 / 60);
		char summary[64];
		snprintf(summary, sizeof(summary), "%d passo%s \xC2\xB7 %d,%d s", m->count, m->count == 1 ? "" : "s", tenths / 10, tenths % 10);
		text_left(c, FS_SMALL, layout_editor_summary(), summary, THEME_DIM);

		Rect mode0 = layout_editor_mode(0);
		text_left(c, FS_SMALL, (Rect) {mode0.x, mode0.y - 18, 120, 18}, "Modo", THEME_DIM);
		static const char *const MODE_LABELS[3] = {"Uma vez", "N vezes", "Repetir"};
		draw_segments(c, layout_editor_mode, 3, (int) m->mode, MODE_LABELS);
		if (e->zone == EZ_MODE) ring(c, rect_union(mode0, layout_editor_mode(2)), THEME_ACCENT);

		int counting = m->mode == MACRO_COUNT;
		char repeat[16];
		snprintf(repeat, sizeof(repeat), "%dx", m->repeat);
		editor_button(c, layout_editor_repeat_minus(), "-", 0, !counting);
		text_center(c, FS_BODY, layout_editor_repeat_value(), repeat, counting ? THEME_TEXT : THEME_DIM);
		editor_button(c, layout_editor_repeat_plus(), "+", 0, !counting);
		if (e->zone == EZ_REPEAT) ring(c, rect_union(layout_editor_repeat_minus(), layout_editor_repeat_plus()), THEME_ACCENT);

		int has_state = e->has_state && e->has_state[e->sel];
		Rect fs = layout_editor_from_state();
		checkbox(c, (Rect) {fs.x, fs.y, 24, fs.h}, m->from_state);
		text_left(c, FS_BODY, (Rect) {fs.x + 28, fs.y, fs.w - 28, fs.h},
		          has_state ? "Come\xC3\xA7""ar do estado salvo" : "Come\xC3\xA7""ar do estado salvo (sem estado)",
		          has_state ? THEME_TEXT : THEME_DIM);
		if (e->zone == EZ_STATE) ring(c, (Rect) {fs.x - 4, fs.y, 300, fs.h}, THEME_ACCENT);

		draw_editor_steps(c, st, e, m);
	}

	/* rodapé */
	if (e->message[0]) text_left(c, FS_BODY, (Rect) {p.x + 16, p.y + 550, 960, 24}, e->message, THEME_TEXT);
	const char *hint = e->name_editing ? "Digite o nome | Enter confirma | Esc cancela"
	                   : e->dur_editing ? "Digite a dura\xC3\xA7\xC3\xA3o em quadros | Enter confirma | Esc cancela"
	                                    : "Tab: mudar de \xC3\xA1rea | Insert / Delete: novo / apagar | 1 a 0: bot\xC3\xB5""es do passo | Alt+setas: mover passo | Esc: voltar";
	text_left(c, FS_SMALL, (Rect) {p.x + 16, p.y + 574, 960, 20}, hint, THEME_DIM);
	editor_button(c, layout_editor_close(), "Voltar", 0, 0);
}

/* ---- tela Controles ---- */

#define KEYS_PANEL_W 640
#define KEYS_PANEL_H 456
#define KEYS_ROW_H 26

static Rect keys_panel(void) {
	Rect body = layout_regions().body;
	return (Rect) {(UI_W - KEYS_PANEL_W) / 2, body.y + (body.h - KEYS_PANEL_H) / 2, KEYS_PANEL_W, KEYS_PANEL_H};
}

Rect ui_keys_panel_rect(void) {
	return keys_panel();
}

static void draw_keys_panel(Canvas *c, const Menu *m) {
	Rect p = keys_panel();
	canvas_box(c, p, 4, THEME_BORDER2, THEME_PANEL);
	text_left(c, FS_BODY, (Rect) {p.x + 24, p.y + 14, 400, 24}, "Controles", THEME_TEXT);

	const int col_label = p.x + 24, col_key = p.x + 200, col_pad = p.x + 420;
	text_left(c, FS_SMALL, (Rect) {col_label, p.y + 48, 160, 20}, "Bot\xC3\xA3o", THEME_DIM);
	text_left(c, FS_SMALL, (Rect) {col_key, p.y + 48, 200, 20}, "Teclado", THEME_DIM);
	text_left(c, FS_SMALL, (Rect) {col_pad, p.y + 48, 200, 20}, "Controle", THEME_DIM);

	for (int row = 0; row <= KEY_ROW_BACK; row++) {
		Rect r = {p.x + 12, p.y + 72 + row * KEYS_ROW_H, p.w - 24, KEYS_ROW_H};
		int current = row == m->key_row;
		if (current) canvas_fill_round(c, r, 4, THEME_ACCENT);
		uint32_t main_color = current ? THEME_WHITE : THEME_TEXT;
		uint32_t dim_color = current ? THEME_WHITE : THEME_DIM;
		if (row < BTN_COUNT) {
			text_left(c, FS_BODY, (Rect) {col_label, r.y, 160, r.h}, bindings_button_label((GbaButton) row), main_color);
			if (current && m->capturing) {
				text_left(c, FS_BODY, (Rect) {col_key, r.y, 200, r.h}, "...", THEME_WHITE);
				text_left(c, FS_BODY, (Rect) {col_pad, r.y, 200, r.h}, "...", THEME_WHITE);
				continue;
			}
			SDL_Scancode sc = m->bind->key[row];
			char key_buf[32];
			const char *key_name = "-";
			if (sc != SDL_SCANCODE_UNKNOWN) {
				key_name = SDL_GetScancodeName(sc);
				if (!key_name || !*key_name) { /* tecla sem nome no SDL (ABNT2/JIS) */
					snprintf(key_buf, sizeof(key_buf), "Tecla %d", (int) sc);
					key_name = key_buf;
				}
			}
			SDL_GameControllerButton pb = m->bind->pad[row];
			const char *pad_name = pb == SDL_CONTROLLER_BUTTON_INVALID ? NULL : SDL_GameControllerGetStringForButton(pb);
			text_left(c, FS_BODY, (Rect) {col_key, r.y, 200, r.h}, key_name, dim_color);
			text_left(c, FS_BODY, (Rect) {col_pad, r.y, 180, r.h}, pad_name ? pad_name : "-", dim_color);
		} else if (row == KEY_ROW_RESET) {
			text_left(c, FS_BODY, (Rect) {col_label, r.y, 300, r.h}, "Restaurar padr\xC3\xA3o", main_color);
		} else {
			text_left(c, FS_BODY, (Rect) {col_label, r.y, 300, r.h}, "Voltar", main_color);
		}
	}

	Rect msg = {p.x + 24, p.y + p.h - 64, p.w - 48, 22};
	if (m->message[0]) {
		text_left(c, FS_BODY, msg, m->message, THEME_TEXT);
	} else if (m->capturing) {
		text_left(c, FS_BODY, msg, "Aperte uma tecla ou bot\xC3\xA3o do controle (ESC cancela)", THEME_TEXT);
	}
	text_left(c, FS_SMALL, (Rect) {p.x + 24, p.y + p.h - 36, p.w - 48, 20},
	          m->capturing ? "ESC cancela" : "Enter altera | Cima/Baixo escolhem | ESC volta", THEME_DIM);
}

void ui_draw_keys(Canvas *c, const View *v, const StatusInfo *st, const Menu *m) {
	View backdrop = *v;
	backdrop.settings_open = 0;
	ui_draw_library(c, &backdrop, st);
	draw_keys_panel(c, m);
}

void ui_draw_library(Canvas *c, const View *v, const StatusInfo *st) {
	gfx_fill(c->px, c->w, c->h, (Rect) {0, 0, c->w, c->h}, THEME_WINDOW); /* inclui as faixas fora da área lógica */
	draw_table(c, v, st);
	draw_toolbar(c, v, st);
	draw_title(c, v, st);
	draw_status(c, v, st);
	if (v->settings_open) draw_panel(c, v);
}
