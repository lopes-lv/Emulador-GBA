/* Ferramenta de desenvolvimento: compara o desenho ANTES (interface a 100% esticada com filtro bilinear, como o Windows e o
 * SDL faziam) e DEPOIS (desenhada no tamanho final) em recortes ampliados 4x.
 * Uso: crop_compare ESCALA GAMA PREFIXO   -> grava PREFIXO_titulo.bmp, PREFIXO_tabela.bmp, PREFIXO_painel.bmp */
#define SDL_MAIN_HANDLED
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "canvas.h"
#include "library.h"
#include "theme.h"
#include "ttf.h"
#include "ui_draw.h"
#include "view.h"

#define ZOOM 4

static void render(uint32_t *buf, int w, int h, float s, View *v, StatusInfo *info) {
	Canvas cv;
	canvas_init(&cv, buf, w, h, s, 0, 0);
	ui_draw_library(&cv, v, info);
}

static void stretch(const uint32_t *src, int w0, int h0, uint32_t *dst, int w1, int h1) {
	for (int y = 0; y < h1; y++) {
		float fy = ((float) y + 0.5f) * (float) h0 / (float) h1 - 0.5f;
		int y0 = (int) floorf(fy);
		float ty = fy - (float) y0;
		int ya = y0 < 0 ? 0 : y0, yb = y0 + 1 >= h0 ? h0 - 1 : y0 + 1;
		for (int x = 0; x < w1; x++) {
			float fx = ((float) x + 0.5f) * (float) w0 / (float) w1 - 0.5f;
			int x0 = (int) floorf(fx);
			float tx = fx - (float) x0;
			int xa = x0 < 0 ? 0 : x0, xb = x0 + 1 >= w0 ? w0 - 1 : x0 + 1;
			uint32_t out = 0xFF000000u;
			for (int sh = 0; sh <= 16; sh += 8) {
				float a = (float) ((src[ya * w0 + xa] >> sh) & 0xFF) * (1 - tx) + (float) ((src[ya * w0 + xb] >> sh) & 0xFF) * tx;
				float b = (float) ((src[yb * w0 + xa] >> sh) & 0xFF) * (1 - tx) + (float) ((src[yb * w0 + xb] >> sh) & 0xFF) * tx;
				out |= (uint32_t) (a * (1 - ty) + b * ty + 0.5f) << sh;
			}
			dst[y * w1 + x] = out;
		}
	}
}

static int write_bmp(const char *path, const uint32_t *px, int w, int h) {
	FILE *f = fopen(path, "wb");
	if (!f) return -1;
	uint32_t row_bytes = (uint32_t) ((w * 3 + 3) / 4 * 4), size = 54 + row_bytes * (uint32_t) h;
	unsigned char head[54] = {'B', 'M'};
	head[2] = (unsigned char) size; head[3] = (unsigned char) (size >> 8); head[4] = (unsigned char) (size >> 16); head[5] = (unsigned char) (size >> 24);
	head[10] = 54; head[14] = 40;
	head[18] = (unsigned char) w; head[19] = (unsigned char) (w >> 8); head[20] = (unsigned char) (w >> 16);
	head[22] = (unsigned char) h; head[23] = (unsigned char) (h >> 8); head[24] = (unsigned char) (h >> 16);
	head[26] = 1; head[28] = 24;
	fwrite(head, 1, sizeof(head), f);
	unsigned char pad[4] = {0, 0, 0, 0};
	for (int y = h - 1; y >= 0; y--) {
		for (int x = 0; x < w; x++) {
			uint32_t p = px[y * w + x];
			unsigned char bgr[3] = {(unsigned char) (p >> 16), (unsigned char) (p >> 8), (unsigned char) p};
			fwrite(bgr, 1, 3, f);
		}
		fwrite(pad, 1, row_bytes - (uint32_t) w * 3, f);
	}
	return fclose(f);
}

/* Recorte (em unidades lógicas) ampliado ZOOM vezes: ANTES em cima, DEPOIS embaixo. */
static void crop_pair(const char *path, const uint32_t *before, const uint32_t *after, int w, int h, float s, Rect r) {
	int x0 = (int) lroundf((float) r.x * s), y0 = (int) lroundf((float) r.y * s);
	int cw = (int) lroundf((float) r.w * s), ch = (int) lroundf((float) r.h * s);
	if (x0 + cw > w) cw = w - x0;
	if (y0 + ch > h) ch = h - y0;
	int gap = 8, ow = cw * ZOOM, oh = ch * ZOOM * 2 + gap;
	uint32_t *out = malloc((size_t) ow * (size_t) oh * sizeof(uint32_t));
	for (int i = 0; i < ow * oh; i++) out[i] = 0xFF808080u;
	for (int half = 0; half < 2; half++) {
		const uint32_t *src = half == 0 ? before : after;
		for (int y = 0; y < ch * ZOOM; y++) {
			for (int x = 0; x < ow; x++) {
				out[(half * (ch * ZOOM + gap) + y) * ow + x] = src[(y0 + y / ZOOM) * w + x0 + x / ZOOM];
			}
		}
	}
	write_bmp(path, out, ow, oh);
	free(out);
}

int main(int argc, char **argv) {
	float s = argc > 1 ? (float) atof(argv[1]) : 1.25f;
	float gamma = argc > 2 ? (float) atof(argv[2]) : 0.85f;
	const char *prefix = argc > 3 ? argv[3] : "cmp";
	ttf_init();
	ttf_set_gamma(gamma);

	Settings st;
	settings_defaults(&st);
	Library lib;
	library_demo(&lib);
	View v;
	view_init(&v, &st);
	view_set_library(&v, &lib, 0);
	v.selected = 3;
	StatusInfo info = {60, 1, 0, 0, (int64_t) time(NULL), NULL};

	int w1 = (int) lroundf((float) UI_W * s), h1 = (int) lroundf((float) UI_H * s);
	uint32_t *base = malloc((size_t) UI_W * UI_H * sizeof(uint32_t));
	uint32_t *before = malloc((size_t) w1 * (size_t) h1 * sizeof(uint32_t));
	uint32_t *after = malloc((size_t) w1 * (size_t) h1 * sizeof(uint32_t));
	char path[512];

	for (int scene = 0; scene < 2; scene++) {
		v.settings_open = scene;
		render(base, UI_W, UI_H, 1.0f, &v, &info);          /* a interface como era: 1280x720 ... */
		stretch(base, UI_W, UI_H, before, w1, h1);          /* ... esticada pelo Windows/SDL */
		render(after, w1, h1, s, &v, &info);                /* a interface nova: desenhada no tamanho final */
		if (scene == 0) {
			snprintf(path, sizeof(path), "%s_titulo.bmp", prefix);
			crop_pair(path, before, after, w1, h1, s, (Rect) {1100, 0, 180, 32});
			snprintf(path, sizeof(path), "%s_tabela.bmp", prefix);
			crop_pair(path, before, after, w1, h1, s, (Rect) {0, 196, 340, 80});
		} else {
			snprintf(path, sizeof(path), "%s_painel.bmp", prefix);
			crop_pair(path, before, after, w1, h1, s, (Rect) {224, 200, 430, 130});
		}
	}
	printf("escala %.2f gama %.2f: gravados %s_titulo.bmp, %s_tabela.bmp, %s_painel.bmp\n", (double) s, (double) gamma, prefix, prefix, prefix);
	free(base);
	free(before);
	free(after);
	view_free(&v);
	library_free(&lib);
	return 0;
}
