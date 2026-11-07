#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "gfx.h"
#include "layout.h"
#include "canvas.h"
#include "scale.h"
#include "theme.h"
#include "ttf.h"
#include "ui_metrics.h"

#define GAME_W 240
#define GAME_H 160

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;   /* origem (streaming, nearest) */
static SDL_Texture *target;    /* origem ampliada com pixels duros (alvo de render, linear) */
static int src_w, src_h;
static int target_factor;
static ScaleMode scale_mode = SCALE_FILL;
static VideoMode current_mode = VIDEO_GAME;

static SDL_Texture *ui_texture; /* quadro da biblioteca, do tamanho da saída (cópia 1:1) */
static int ui_w, ui_h;

static SDL_Texture *osd_texture;
static int osd_w, osd_h;
static Uint32 osd_until;       /* 0 = sem aviso ou persistente (ver osd_persistent) */
static int osd_persistent;
static int osd_active;

static SDL_Texture *overlay_texture; /* camada do menu do jogo, do tamanho da saída */
static int overlay_w, overlay_h;
static int overlay_on;

static uint32_t *capture_buf;
static int capture_w, capture_h;

static Rect dest_rect(int out_w, int out_h) {
	ScaleMode mode = current_mode == VIDEO_UI ? SCALE_FILL : scale_mode;
	return scale_fit(out_w, out_h, src_w, src_h, mode);
}

static int make_textures(int width, int height) {
	if (texture) SDL_DestroyTexture(texture);
	if (target) SDL_DestroyTexture(target);
	texture = target = NULL;
	target_factor = 0;
	SDL_RenderSetLogicalSize(renderer, 0, 0);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!texture) {
		fprintf(stderr, "Erro ao criar a textura: %s\n", SDL_GetError());
		return -1;
	}
	src_w = width;
	src_h = height;
	return 0;
}

/* Alvo com `factor` x a origem; filtro linear na cópia para a janela. NULL se o renderer não suportar. */
static SDL_Texture *ensure_target(int factor) {
	if (target && target_factor == factor) return target;
	if (target) SDL_DestroyTexture(target);
	target = NULL;
	target_factor = 0;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, src_w * factor, src_h * factor);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	if (target) target_factor = factor;
	return target;
}

/* Reduz (width x height) em pontos, mantendo a proporção, até caber em 90% da área útil do monitor. */
static void fit_points(int *w, int *h) {
	SDL_Rect usable;
	if (SDL_GetDisplayUsableBounds(0, &usable) != 0 || usable.w < 1 || usable.h < 1) return;
	float f = fminf(0.9f * (float) usable.w / (float) *w, 0.9f * (float) usable.h / (float) *h);
	if (f < 1.0f) {
		*w = (int) lroundf((float) *w * f);
		*h = (int) lroundf((float) *h * f);
	}
}

int video_init(int width, int height, int scale) {
	/* O programa cuida da escala do Windows: a janela tem tamanho em pontos e a saída de vídeo, em pixels reais.
	 * Sem isso o Windows estica a janela inteira e tudo fica borrado (ver tools/dpi_probe). */
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Erro ao iniciar o vídeo: %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	int win_w = width * scale, win_h = height * scale;
	fit_points(&win_w, &win_h);
	window = SDL_CreateWindow("GBA", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
	                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!window) {
		fprintf(stderr, "Erro ao criar a janela: %s\n", SDL_GetError());
		return -1;
	}
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		fprintf(stderr, "Erro ao criar o renderer: %s\n", SDL_GetError());
		return -1;
	}
	return make_textures(width, height);
}

static void resize_window(int width, int height, int scale) {
	if (SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) SDL_RestoreWindow(window);
	int w = width * scale, h = height * scale;
	fit_points(&w, &h);
	SDL_SetWindowSize(window, w, h);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

int video_set_size(int width, int height) {
	if (!renderer) return -1;
	if (width == src_w && height == src_h && texture) return 0;
	int scale = width >= UI_W ? 1 : width >= 480 ? 2 : 3;
	resize_window(width, height, scale);
	return make_textures(width, height);
}

/* Barra de título própria: arrasta pela barra, exceto sobre os botões da janela. */
static SDL_HitTestResult hit_test(SDL_Window *win, const SDL_Point *area, void *data) {
	(void) win;
	(void) data;
	int ux, uy;
	if (!video_window_to_ui(area->x, area->y, &ux, &uy)) return SDL_HITTEST_NORMAL;
	if (uy >= TITLE_H) return SDL_HITTEST_NORMAL;
	for (int b = 0; b < 3; b++) {
		if (rect_contains(layout_window_button((WindowButton) b), ux, uy)) return SDL_HITTEST_NORMAL;
	}
	return SDL_HITTEST_DRAGGABLE;
}

int video_set_mode(VideoMode mode) {
	if (!renderer) return -1;
	if (mode == VIDEO_UI && video_is_fullscreen()) video_set_fullscreen(0);
	current_mode = mode;
	SDL_SetWindowMinimumSize(window, mode == VIDEO_UI ? 960 : 1, mode == VIDEO_UI ? 540 : 1);
	if (mode == VIDEO_UI) {
		SDL_SetWindowBordered(window, SDL_FALSE);
		SDL_SetWindowHitTest(window, hit_test, NULL);
		return video_set_size(UI_W, UI_H);
	}
	SDL_SetWindowHitTest(window, NULL, NULL);
	SDL_SetWindowBordered(window, SDL_TRUE);
	return video_set_size(GAME_W, GAME_H);
}

void video_set_scale_mode(ScaleMode mode) {
	scale_mode = mode;
}

int video_set_fullscreen(int on) {
	if (!window) return -1;
	if (on && current_mode != VIDEO_GAME) return -1;
	if (SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
		fprintf(stderr, "Erro ao alternar a tela cheia: %s\n", SDL_GetError());
		return -1;
	}
	SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
	return 0;
}

int video_is_fullscreen(void) {
	return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
}

void video_minimize(void) {
	if (window) SDL_MinimizeWindow(window);
}

int video_is_maximized(void) {
	return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
}

void video_toggle_maximize(void) {
	if (!window) return;
	if (video_is_maximized()) SDL_RestoreWindow(window); else SDL_MaximizeWindow(window);
}

int video_window_to_ui(int wx, int wy, int *ux, int *uy) {
	if (!window || !renderer) return 0;
	int ww, wh, ow, oh;
	SDL_GetWindowSize(window, &ww, &wh);
	SDL_GetRendererOutputSize(renderer, &ow, &oh);
	if (ww < 1 || wh < 1) return 0;
	int px = (int) ((long long) wx * ow / ww), py = (int) ((long long) wy * oh / wh);
	UiMetrics m = ui_metrics_fit(ow, oh);
	return ui_metrics_window_to_logical(&m, px, py, ux, uy);
}

void video_ui_size(int *w, int *h) {
	SDL_GetRendererOutputSize(renderer, w, h);
}

float video_dpi_scale(void) {
	if (!window || !renderer) return 1.0f;
	int ww, wh, ow, oh;
	SDL_GetWindowSize(window, &ww, &wh);
	SDL_GetRendererOutputSize(renderer, &ow, &oh);
	return ww > 0 ? (float) ow / (float) ww : 1.0f;
}

/* ---- OSD ---- */

void video_osd_clear(void) {
	osd_active = 0;
	osd_persistent = 0;
	osd_until = 0;
}

void video_osd_show(const char *utf8, int ms) {
	if (!renderer || !utf8 || !*utf8) return;
	/* O aviso é desenhado no tamanho real da janela: texto de 13 px lógicos x escala do Windows. */
	float ds = video_dpi_scale();
	int px = ttf_px_size(FS_BODY, ds);
	int lh = ttf_line_height_px(px);
	int pad = (int) lroundf(12.0f * ds), vpad = (int) lroundf(6.0f * ds);
	int w = ttf_width_px(px, utf8) + 2 * pad, h = lh + 2 * vpad;
	uint32_t *buf = calloc((size_t) w * h, sizeof(uint32_t));
	if (!buf) return;
	Canvas oc;
	canvas_init(&oc, buf, w, h, 1.0f, 0, 0);
	canvas_fill_round(&oc, (Rect) {0, 0, w, h}, (int) lroundf(4.0f * ds), THEME_BAR);
	ttf_draw_fit_px(buf, w, h, px, (Rect) {pad, 0, w - 2 * pad, h}, utf8, THEME_TEXT);
	if (osd_texture) SDL_DestroyTexture(osd_texture);
	osd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (osd_texture) {
		SDL_UpdateTexture(osd_texture, NULL, buf, w * (int) sizeof(uint32_t));
		SDL_SetTextureBlendMode(osd_texture, SDL_BLENDMODE_BLEND);
		osd_w = w;
		osd_h = h;
		osd_active = 1;
		osd_persistent = ms <= 0;
		osd_until = SDL_GetTicks() + (Uint32) (ms > 0 ? ms : 0);
	}
	free(buf);
}

static void draw_osd(void) {
	if (!osd_active || !osd_texture) return;
	if (!osd_persistent && SDL_TICKS_PASSED(SDL_GetTicks(), osd_until)) {
		osd_active = 0;
		return;
	}
	int margin = (int) lroundf(12.0f * video_dpi_scale());
	SDL_Rect dst = {margin, margin, osd_w, osd_h};
	SDL_RenderCopy(renderer, osd_texture, NULL, &dst);
}

/* ---- camada do menu do jogo ---- */

#define OVERLAY_DIM_ALPHA 150

void video_overlay_set(const uint32_t *pixels, int w, int h) {
	if (!renderer || !pixels || w < 1 || h < 1) return;
	if (!overlay_texture || overlay_w != w || overlay_h != h) {
		if (overlay_texture) SDL_DestroyTexture(overlay_texture);
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		overlay_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		overlay_w = w;
		overlay_h = h;
		if (!overlay_texture) {
			overlay_on = 0;
			return;
		}
		SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
	}
	SDL_UpdateTexture(overlay_texture, NULL, pixels, w * (int) sizeof(uint32_t));
	overlay_on = 1;
}

void video_overlay_clear(void) {
	overlay_on = 0;
}

int video_overlay_active(void) {
	return overlay_on;
}

static void draw_overlay(void) {
	if (!overlay_on || !overlay_texture) return;
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, OVERLAY_DIM_ALPHA);
	SDL_RenderFillRect(renderer, NULL);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	SDL_RenderCopy(renderer, overlay_texture, NULL, NULL); /* 1:1 quando w x h é o tamanho da saída */
}

/* ---- desenho ---- */

void video_capture_next(uint32_t *out, int w, int h) {
	capture_buf = out;
	capture_w = w;
	capture_h = h;
}

void video_present(const uint32_t *pixels) {
	int ow, oh;
	SDL_GetRendererOutputSize(renderer, &ow, &oh);
	Rect dst = dest_rect(ow, oh);
	SDL_UpdateTexture(texture, NULL, pixels, src_w * (int) sizeof(uint32_t));

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_Rect d = {dst.x, dst.y, dst.w, dst.h};
	SDL_Texture *big = ensure_target(scale_prescale(dst.w, src_w));
	if (big && SDL_SetRenderTarget(renderer, big) == 0) {
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_SetRenderTarget(renderer, NULL);
		SDL_RenderCopy(renderer, big, NULL, &d);
	} else {
		SDL_SetRenderTarget(renderer, NULL);
		SDL_RenderCopy(renderer, texture, NULL, &d);
	}
	draw_overlay();
	draw_osd();

	if (capture_buf) {
		if (capture_w == ow && capture_h == oh) {
			SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ABGR8888, capture_buf, ow * (int) sizeof(uint32_t));
		}
		capture_buf = NULL;
	}
	SDL_RenderPresent(renderer);
}

void video_present_ui(const uint32_t *pixels, int w, int h) {
	if (!renderer || w < 1 || h < 1) return;
	if (!ui_texture || ui_w != w || ui_h != h) {
		if (ui_texture) SDL_DestroyTexture(ui_texture);
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		ui_w = w;
		ui_h = h;
		if (!ui_texture) return;
	}
	SDL_UpdateTexture(ui_texture, NULL, pixels, w * (int) sizeof(uint32_t));
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, ui_texture, NULL, NULL); /* 1:1 quando w x h é o tamanho da saída */
	if (capture_buf) {
		int ow, oh;
		SDL_GetRendererOutputSize(renderer, &ow, &oh);
		if (capture_w == ow && capture_h == oh) {
			SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ABGR8888, capture_buf, ow * (int) sizeof(uint32_t));
		}
		capture_buf = NULL;
	}
	SDL_RenderPresent(renderer);
}

void video_set_title_fps(double fps) {
	char title[64];
	snprintf(title, sizeof(title), "Retr\xC3\xB4 Emu - %.1f FPS", fps);
	SDL_SetWindowTitle(window, title);
}

void video_set_title(const char *title) {
	SDL_SetWindowTitle(window, title);
}

void video_shutdown(void) {
	if (osd_texture) SDL_DestroyTexture(osd_texture);
	if (ui_texture) SDL_DestroyTexture(ui_texture);
	if (overlay_texture) SDL_DestroyTexture(overlay_texture);
	overlay_texture = NULL;
	overlay_on = 0;
	ui_texture = NULL;
	if (target) SDL_DestroyTexture(target);
	if (texture) SDL_DestroyTexture(texture);
	if (renderer) SDL_DestroyRenderer(renderer);
	if (window) SDL_DestroyWindow(window);
	osd_texture = target = texture = NULL;
	renderer = NULL;
	window = NULL;
	src_w = src_h = 0;
	target_factor = 0;
	osd_active = 0;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
