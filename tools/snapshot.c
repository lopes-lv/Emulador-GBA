/* Ferramenta de desenvolvimento: desenha a biblioteca de demonstração e grava snapshot.bmp (1280x720, 24 bits).
 * Uso: snapshot [--select N] [--search TEXTO] [--settings] [--menu N] [--hover-row N] [--overlay N] [--out arquivo.bmp]
 * (--overlay N desenha o menu do jogo com o foco no item N, sobre um fundo cinza; --macros N finge N macros e
 * --macro-state 1|2 finge tocando|gravando) */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "canvas.h"
#include "layout.h"
#include "menu.h"
#include "library.h"
#include "theme.h"
#include "ui_draw.h"
#include "view.h"

static uint32_t fb[UI_W * UI_H * 16];
static int fb_w = UI_W, fb_h = UI_H;

static int write_bmp(const char *path) {
	FILE *f = fopen(path, "wb");
	if (!f) return -1;
	uint32_t row_bytes = (uint32_t) ((fb_w * 3 + 3) / 4 * 4), size = 54 + row_bytes * (uint32_t) fb_h;
	unsigned char head[54] = {'B', 'M'};
	head[2] = (unsigned char) size; head[3] = (unsigned char) (size >> 8); head[4] = (unsigned char) (size >> 16); head[5] = (unsigned char) (size >> 24);
	head[10] = 54; head[14] = 40;
	head[18] = (unsigned char) fb_w; head[19] = (unsigned char) (fb_w >> 8); head[20] = (unsigned char) (fb_w >> 16);
	head[22] = (unsigned char) fb_h; head[23] = (unsigned char) (fb_h >> 8); head[24] = (unsigned char) (fb_h >> 16);
	head[26] = 1; head[28] = 24;
	fwrite(head, 1, sizeof(head), f);
	unsigned char pad[4] = {0, 0, 0, 0};
	for (int y = fb_h - 1; y >= 0; y--) {
		for (int x = 0; x < fb_w; x++) {
			uint32_t p = fb[y * fb_w + x];
			unsigned char bgr[3] = {(unsigned char) (p >> 16), (unsigned char) (p >> 8), (unsigned char) p};
			fwrite(bgr, 1, 3, f);
		}
		fwrite(pad, 1, row_bytes - (uint32_t) fb_w * 3, f);
	}
	return fclose(f);
}

int main(int argc, char **argv) {
	const char *out = "snapshot.bmp", *search = NULL;
	int select = -1, settings_open = 0, hover_row = -1, keys = 0, overlay_focus = -1, snap_macros = 0, snap_macro_state = 0, editor_zone = -1;
	float scale = 1.0f;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--select") && i + 1 < argc) select = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--search") && i + 1 < argc) search = argv[++i];
		else if (!strcmp(argv[i], "--settings")) settings_open = 1;
		else if (!strcmp(argv[i], "--keys")) keys = 1;
		else if (!strcmp(argv[i], "--overlay") && i + 1 < argc) overlay_focus = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--editor") && i + 1 < argc) editor_zone = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--macros") && i + 1 < argc) snap_macros = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--macro-state") && i + 1 < argc) snap_macro_state = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = (float) atof(argv[++i]);
		else if (!strcmp(argv[i], "--hover-row") && i + 1 < argc) hover_row = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
	}

	Settings st;
	settings_defaults(&st);
	Library lib;
	library_demo(&lib);
	View v;
	view_init(&v, &st);
	view_set_library(&v, &lib, 0);
	if (search) {
		v.focus = FOCUS_SEARCH;
		view_handle(&v, (ViewEvent) {VEV_TEXT, 0, 0, 0, 0, 0, search});
	}
	if (select >= 0 && select < v.visible_count) v.selected = select;
	if (hover_row >= 0) {
		v.hover_kind = HOVER_ROW;
		v.hover_item = hover_row;
	}
	v.settings_open = settings_open;

	StatusInfo info = {60, 1, 0, 1, (int64_t) time(NULL), NULL};
	Bindings bind;
	bindings_defaults(&bind);
	Menu mn;
	menu_init(&mn, &bind);
	menu_open(&mn);
	fb_w = (int) (UI_W * scale + 0.5f);
	fb_h = (int) (UI_H * scale + 0.5f);
	Canvas cv;
	canvas_init(&cv, fb, fb_w, fb_h, scale, 0, 0);
	if (overlay_focus >= 0) { /* menu do jogo sobre um fundo cinza no lugar do jogo */
		for (int i = 0; i < fb_w * fb_h; i++) fb[i] = 0xFF404040u;
		Overlay ov;
		overlay_init(&ov, &st);
		overlay_open(&ov);
		ov.focus = (OverlayItem) (overlay_focus % OI_COUNT);
		MacroList ml = {0};
		if (snap_macros > 0) { /* N macros de mentira para ver a linha da macro */
			ml.items = calloc((size_t) snap_macros, sizeof(Macro));
			for (int i = 0; i < snap_macros; i++) {
				macro_init(&ml.items[i], i == 0 ? "Farmar moedas" : "Outra macro");
				macro_add_step(&ml.items[i], 1, 90 + 30 * i);
			}
			ml.count = snap_macros;
			ov.macros = &ml;
		}
		ov.macro_state = (MacroUiState) snap_macro_state;
		ui_draw_overlay(&cv, &ov);
		macrolist_free(&ml);
	} else if (editor_zone >= 0) { /* editor de macros com dados de mentira; N = zona em foco (0 lista ... 6 botoes; 7 = editando o nome) */
		MacroList ml = {0};
		ml.items = calloc(3, sizeof(Macro));
		ml.count = 3;
		macro_init(&ml.items[0], "Farmar moedas");
		ml.items[0].mode = MACRO_COUNT;
		ml.items[0].repeat = 3;
		static const uint16_t demo_keys[] = {1, 3, 0, 64, 2, 16};
		for (int i = 0; i < 6; i++) macro_step_insert(&ml.items[0], i, demo_keys[i], (uint32_t) (10 + 15 * i));
		macro_init(&ml.items[1], "Outra macro");
		macro_step_insert(&ml.items[1], 0, 1, 90);
		macro_init(&ml.items[2], "Zeta");
		Editor ed;
		editor_init(&ed);
		editor_open(&ed, "Pok\xC3\xA9mon Esmeralda", &ml, "pasta_que_nao_existe");
		ed.has_state[0] = 1;
		ed.step = 1;
		ed.zone = editor_zone >= 7 ? EZ_NAME : (EditorZone) editor_zone;
		ed.chip = 2;
		if (editor_zone >= 7) {
			ed.name_editing = 1;
			textedit_insert(&ed.name, "Farmar moedas");
		}
		ui_draw_editor(&cv, &v, &info, &ed);
		editor_close(&ed);
	} else if (keys) {
		ui_draw_keys(&cv, &v, &info, &mn);
	} else {
		ui_draw_library(&cv, &v, &info);
	}
	if (write_bmp(out) != 0) {
		fprintf(stderr, "Erro ao gravar %s\n", out);
		return 1;
	}
	printf("Gravado %s\n", out);
	view_free(&v);
	library_free(&lib);
	return 0;
}
