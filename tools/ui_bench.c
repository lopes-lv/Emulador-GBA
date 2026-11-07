/* Ferramenta de desenvolvimento: mede o tempo de desenhar a biblioteca completa em varias resolucoes.
 * Criterio (secao 6.5 do spec): 1080p <= 8 ms, 1440p <= 16 ms, 4K <= 40 ms (media em regime). Sai com codigo 1 se passar. */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "canvas.h"
#include "library.h"
#include "ui_draw.h"
#include "ui_metrics.h"
#include "view.h"

typedef struct { int w, h; double budget_ms; const char *name; } Case;

static double now_ms(void) {
	return (double) SDL_GetPerformanceCounter() * 1000.0 / (double) SDL_GetPerformanceFrequency();
}

static void make_roms(Library *lib, int n) {
	RomList roms = {0};
	roms.count = n;
	roms.items = calloc((size_t) n, sizeof(RomEntry));
	for (int i = 0; i < n; i++) {
		snprintf(roms.items[i].name, sizeof(roms.items[i].name), "Jogo de teste numero %d com um nome razoavelmente comprido", i);
		snprintf(roms.items[i].path, sizeof(roms.items[i].path), "C:/roms/jogo%d.gba", i);
	}
	library_from_roms(lib, &roms);
	free(roms.items);
	for (int i = 0; i < lib->count; i++) {
		snprintf(lib->items[i].genre, sizeof(lib->items[i].genre), "Genero %d", i % 7);
		lib->items[i].year = 1995 + i % 12;
		lib->items[i].playtime_s = 3600 * (i % 50);
		lib->items[i].last_played = 1700000000 + i * 1000;
	}
}

int main(void) {
	const Case cases[] = {{1280, 720, 8.0, "720p"}, {1600, 900, 8.0, "900p (125%)"}, {1920, 1080, 8.0, "1080p"},
	                      {2560, 1440, 16.0, "1440p"}, {3840, 2160, 40.0, "4K"}};
	const char *cenas[] = {"10 jogos", "1000 jogos, rolada", "painel Configuracoes aberto"};
	int fails = 0;
	printf("%-14s %-30s %10s %10s %10s\n", "resolucao", "cena", "media ms", "pior ms", "orcamento");
	for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		int w = cases[k].w, h = cases[k].h;
		uint32_t *buf = malloc((size_t) w * (size_t) h * sizeof(uint32_t));
		UiMetrics m = ui_metrics_fit(w, h);
		Canvas cv;
		canvas_init(&cv, buf, w, h, m.scale, m.ox, m.oy);
		for (int sc = 0; sc < 3; sc++) {
			Settings st;
			settings_defaults(&st);
			Library lib;
			if (sc == 1) make_roms(&lib, 1000); else library_demo(&lib);
			View v;
			view_init(&v, &st);
			view_set_library(&v, &lib, sc == 1);
			if (sc == 1) { v.scroll = 300; v.selected = 305; }
			if (sc == 2) v.settings_open = 1;
			StatusInfo info = {60, 1, 0, 1, 1800000000, NULL};
			ui_draw_library(&cv, &v, &info); /* aquece o cache de glifos */
			const int n = 20;
			double sum = 0, worst = 0;
			for (int i = 0; i < n; i++) {
				double t0 = now_ms();
				ui_draw_library(&cv, &v, &info);
				double dt = now_ms() - t0;
				sum += dt;
				if (dt > worst) worst = dt;
			}
			double avg = sum / n;
			int ok = avg <= cases[k].budget_ms;
			if (!ok) fails++;
			printf("%-14s %-30s %10.2f %10.2f %9.0f ms %s\n", cases[k].name, cenas[sc], avg, worst, cases[k].budget_ms, ok ? "OK" : "ACIMA");
			view_free(&v);
			library_free(&lib);
		}
		free(buf);
	}
	printf(fails ? "RESULTADO: %d caso(s) acima do orcamento\n" : "RESULTADO: dentro do orcamento\n", fails);
	return fails ? 1 : 0;
}
