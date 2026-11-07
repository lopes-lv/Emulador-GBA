/* Ferramenta de desenvolvimento: confere o desenho real do vídeo numa janela de verdade (precisa de tela).
 * Imprime OK/FALHOU por caso e sai com código != 0 se algo falhar. Não é um teste do CTest. */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include <math.h>

#include "canvas.h"
#include "library.h"
#include "scale.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_metrics.h"
#include "video.h"
#include "view.h"

static int failures;

#define CHECK(cond, nome) do { \
	if (cond) printf("OK: %s\n", nome); else { printf("FALHOU: %s\n", nome); failures++; } \
} while (0)

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u

static uint32_t frame[240 * 160];

static uint32_t at(const uint32_t *shot, int w, int x, int y) { return shot[y * w + x]; }

static void capture_case(int win_w, int win_h, ScaleMode mode, const char *nome) {
	SDL_Window *win = SDL_GetWindowFromID(1);
	SDL_SetWindowSize(win, win_w, win_h);
	SDL_PumpEvents();
	int ow = 0, oh = 0;
	SDL_GetRendererOutputSize(SDL_GetRenderer(win), &ow, &oh);
	video_set_scale_mode(mode);
	uint32_t *shot = calloc((size_t) ow * oh, sizeof(uint32_t));
	video_capture_next(shot, ow, oh);
	video_present(frame);
	Rect dst = scale_fit(ow, oh, 240, 160, mode);
	char msg[160];
	snprintf(msg, sizeof(msg), "%s: janela %dx%d, imagem em {%d,%d,%d,%d}", nome, ow, oh, dst.x, dst.y, dst.w, dst.h);
	int ok = 1;
	/* dentro da imagem: tudo branco (longe da borda, para tolerar o filtro) */
	int mx = dst.x + dst.w / 2, my = dst.y + dst.h / 2;
	ok &= at(shot, ow, mx, my) == WHITE;
	ok &= at(shot, ow, dst.x + 3, dst.y + 3) == WHITE;
	ok &= at(shot, ow, dst.x + dst.w - 4, dst.y + dst.h - 4) == WHITE;
	/* fora da imagem: barras pretas */
	if (dst.x > 2) ok &= at(shot, ow, dst.x - 2, my) == BLACK && at(shot, ow, 0, my) == BLACK;
	if (dst.x + dst.w < ow - 2) ok &= at(shot, ow, dst.x + dst.w + 1, my) == BLACK && at(shot, ow, ow - 1, my) == BLACK;
	if (dst.y > 2) ok &= at(shot, ow, mx, dst.y - 2) == BLACK && at(shot, ow, mx, 0) == BLACK;
	if (dst.y + dst.h < oh - 2) ok &= at(shot, ow, mx, dst.y + dst.h + 1) == BLACK && at(shot, ow, mx, oh - 1) == BLACK;
	CHECK(ok, msg);
	free(shot);
}

/* Camada do menu do jogo: escurece o jogo, mostra o que foi desenhado nela e some ao ser desligada. */
static void camada_sobre_o_jogo(void) {
	SDL_Window *win = SDL_GetWindowFromID(1);
	SDL_SetWindowSize(win, 720, 480);
	SDL_PumpEvents();
	SDL_Delay(150);
	SDL_PumpEvents();
	int ow, oh;
	video_ui_size(&ow, &oh);
	video_set_scale_mode(SCALE_FILL);
	Rect dst = scale_fit(ow, oh, 240, 160, SCALE_FILL);
	int mx = dst.x + dst.w / 2, my = dst.y + dst.h / 2;
	int fx = dst.x + 6, fy = dst.y + 6; /* dentro da imagem, longe do retangulo da camada */
	uint32_t *layer = calloc((size_t) ow * oh, sizeof(uint32_t));
	uint32_t *shot = calloc((size_t) ow * oh, sizeof(uint32_t));
	for (int y = my - 10; y < my + 10; y++) {
		for (int x = mx - 10; x < mx + 10; x++) layer[y * ow + x] = 0xFF00FF00u; /* verde opaco */
	}

	video_capture_next(shot, ow, oh);
	video_present(frame);
	CHECK(!video_overlay_active() && at(shot, ow, mx, my) == WHITE, "sem camada: o jogo aparece inteiro");

	video_overlay_set(layer, ow, oh);
	memset(shot, 0, (size_t) ow * oh * sizeof(uint32_t));
	video_capture_next(shot, ow, oh);
	video_present(frame);
	uint32_t dim = at(shot, ow, fx, fy);
	int canal = (int) (dim & 0xFF);
	CHECK(video_overlay_active(), "camada ligada");
	CHECK(at(shot, ow, mx, my) == 0xFF00FF00u, "camada: o que foi desenhado aparece por cima do jogo");
	CHECK(canal >= 100 && canal <= 110 && ((dim >> 8) & 0xFF) == (uint32_t) canal && ((dim >> 16) & 0xFF) == (uint32_t) canal,
	      "camada: o resto do jogo fica escurecido (branco com alfa 150 -> cinza ~105)");

	video_overlay_clear();
	memset(shot, 0, (size_t) ow * oh * sizeof(uint32_t));
	video_capture_next(shot, ow, oh);
	video_present(frame);
	CHECK(!video_overlay_active() && at(shot, ow, mx, my) == WHITE && at(shot, ow, fx, fy) == WHITE,
	      "depois de desligar, o jogo volta ao normal");
	free(layer);
	free(shot);
}

/* Biblioteca em janela real: o quadro desenhado tem o tamanho da saída e chega à tela sem ser esticado. */
static void biblioteca_em_janela_real(void) {
	Settings st;
	settings_defaults(&st);
	Library lib;
	library_demo(&lib);
	View v;
	view_init(&v, &st);
	view_set_library(&v, &lib, 0);
	v.selected = 2;
	StatusInfo info = {60, 1, 0, 0, 1800000000, NULL};

	const int pontos[][2] = {{1280, 720}, {1000, 700}, {960, 540}, {1500, 600}, {800, 900}};
	SDL_Window *win = SDL_GetWindowFromID(1);
	for (size_t k = 0; k < sizeof(pontos) / sizeof(pontos[0]); k++) {
		SDL_SetWindowSize(win, pontos[k][0], pontos[k][1]);
		SDL_PumpEvents();
		SDL_Delay(150);
		SDL_PumpEvents();
		int ww, wh, ow, oh;
		SDL_GetWindowSize(win, &ww, &wh);
		video_ui_size(&ow, &oh);
		uint32_t *buf = malloc((size_t) ow * oh * sizeof(uint32_t));
		uint32_t *shot = calloc((size_t) ow * oh, sizeof(uint32_t));
		UiMetrics m = ui_metrics_fit(ow, oh);
		Canvas cv;
		canvas_init(&cv, buf, ow, oh, m.scale, m.ox, m.oy);
		ui_draw_library(&cv, &v, &info);
		video_capture_next(shot, ow, oh);
		video_present_ui(buf, ow, oh);
		long diff = 0;
		for (int i = 0; i < ow * oh; i++) diff += (buf[i] & 0x00FFFFFFu) != (shot[i] & 0x00FFFFFFu);
		char msg[200];
		snprintf(msg, sizeof(msg), "janela %dx%d pontos, saida %dx%d px (S=%.3f, dpi %.2f): quadro chega 1:1 (%ld pixels diferentes)",
		         ww, wh, ow, oh, (double) m.scale, (double) video_dpi_scale(), diff);
		CHECK(diff == 0 && ww >= 1 && ow >= ww, msg);

		/* mouse: o Windows so enderaca pontos inteiros (1 ponto = 1,25 px em 125%), entao compara-se o resultado com o
		 * mapeamento ideal calculado a partir do proprio ponto da janela. */
		unsigned seed = 7u + (unsigned) k;
		int ruins = 0, dentro_ok = 0;
		for (int i = 0; i < 400; i++) {
			seed = seed * 1103515245u + 12345u;
			int wx = (int) ((seed >> 8) % (unsigned) ww);
			seed = seed * 1103515245u + 12345u;
			int wy = (int) ((seed >> 8) % (unsigned) wh);
			float px = (float) wx * (float) ow / (float) ww, py = (float) wy * (float) oh / (float) wh;
			int ex = (int) floorf((px - (float) m.ox) / m.scale), ey = (int) floorf((py - (float) m.oy) / m.scale);
			int rx, ry;
			int dentro = video_window_to_ui(wx, wy, &rx, &ry);
			if (ex >= 2 && ex <= 1277 && ey >= 2 && ey <= 717) {
				dentro_ok++;
				if (!dentro || abs(rx - ex) > 1 || abs(ry - ey) > 1) ruins++;
			} else if (ex < -2 || ex > 1281 || ey < -2 || ey > 721) {
				if (dentro) ruins++;
			}
		}
		snprintf(msg, sizeof(msg), "mouse na janela %dx%d: %d pontos dentro da area e todos os de fora batem com o mapeamento ideal (%d divergentes)",
		         ww, wh, dentro_ok, ruins);
		CHECK(ruins == 0 && dentro_ok > 50, msg);
		free(buf);
		free(shot);
	}
	view_free(&v);
	library_free(&lib);
}

int main(void) {
	for (int i = 0; i < 240 * 160; i++) frame[i] = WHITE;
	if (video_init(240, 160, 3) != 0) return 2;
	if (video_set_mode(VIDEO_GAME) != 0) return 2;

	capture_case(720, 480, SCALE_FILL, "720x480 preencher");
	capture_case(1000, 500, SCALE_FILL, "1000x500 (2:1) preencher");
	capture_case(1000, 500, SCALE_INTEGER, "1000x500 inteira");
	capture_case(500, 800, SCALE_FILL, "500x800 retrato preencher");
	capture_case(1100, 650, SCALE_FILL, "1100x650 preencher (escala nao inteira)");

	camada_sobre_o_jogo();

	/* tela cheia: abre, confere que a imagem nao deforma e fecha */
	if (video_set_fullscreen(1) == 0) {
		SDL_PumpEvents();
		SDL_Delay(300);
		SDL_PumpEvents();
		CHECK(video_is_fullscreen(), "entrou em tela cheia");
		SDL_Window *win = SDL_GetWindowFromID(1);
		int ow = 0, oh = 0;
		SDL_GetRendererOutputSize(SDL_GetRenderer(win), &ow, &oh);
		video_set_scale_mode(SCALE_FILL);
		uint32_t *shot = calloc((size_t) ow * oh, sizeof(uint32_t));
		video_capture_next(shot, ow, oh);
		video_present(frame);
		Rect dst = scale_fit(ow, oh, 240, 160, SCALE_FILL);
		int ok = at(shot, ow, dst.x + dst.w / 2, dst.y + dst.h / 2) == WHITE;
		if (dst.x > 2) ok &= at(shot, ow, 0, oh / 2) == BLACK;
		if (dst.y > 2) ok &= at(shot, ow, ow / 2, 0) == BLACK;
		char msg[120];
		snprintf(msg, sizeof(msg), "tela cheia %dx%d: imagem {%d,%d,%d,%d}, barras pretas", ow, oh, dst.x, dst.y, dst.w, dst.h);
		CHECK(ok, msg);
		free(shot);
		video_set_fullscreen(0);
		SDL_PumpEvents();
		SDL_Delay(200);
		CHECK(!video_is_fullscreen(), "saiu da tela cheia");
	} else {
		CHECK(0, "video_set_fullscreen(1) falhou");
	}

	/* conversao de mouse na biblioteca: janela 1280x720 e ampliada */
	video_set_mode(VIDEO_UI);
	int ux = -1, uy = -1;
	SDL_PumpEvents();
	CHECK(video_window_to_ui(640, 360, &ux, &uy) == 1 && ux >= 635 && ux <= 645 && uy >= 355 && uy <= 365, "mouse no centro da biblioteca = centro 1280x720");
	CHECK(video_is_fullscreen() == 0, "a biblioteca nunca fica em tela cheia");
	CHECK(video_set_fullscreen(1) == -1, "tela cheia recusada fora do jogo");

	biblioteca_em_janela_real();
	video_shutdown();
	printf(failures ? "RESULTADO: %d falha(s)\n" : "RESULTADO: tudo OK\n", failures);
	return failures ? 1 : 0;
}
