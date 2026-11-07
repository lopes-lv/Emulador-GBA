/* Ferramenta de desenvolvimento: mostra como o SDL e o Windows tratam a escala (DPI).
 * Uso: dpi_probe a   (como o programa era: sem avisar nada ao Windows)
 *      dpi_probe b   (SDL_HINT_WINDOWS_DPI_AWARENESS=permonitorv2)
 *      dpi_probe c   (b + SDL_WINDOW_ALLOW_HIGHDPI + SDL_HINT_WINDOWS_DPI_SCALING=1)
 * Imprime o tamanho da janela, o tamanho de saída do renderer, o DPI e a área do monitor. */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include <SDL.h>

int main(int argc, char **argv) {
	const char *mode = argc > 1 ? argv[1] : "a";
	Uint32 flags = SDL_WINDOW_RESIZABLE;
	if (strcmp(mode, "b") == 0 || strcmp(mode, "c") == 0) {
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
	}
	if (strcmp(mode, "c") == 0) {
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	}
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *w = SDL_CreateWindow("dpi_probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
	SDL_Renderer *r = w ? SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (!r) {
		fprintf(stderr, "janela/renderer: %s\n", SDL_GetError());
		return 1;
	}
	SDL_PumpEvents();
	int ww, wh, ow, oh;
	SDL_GetWindowSize(w, &ww, &wh);
	SDL_GetRendererOutputSize(r, &ow, &oh);
	float ddpi = 0, hdpi = 0, vdpi = 0;
	SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
	SDL_Rect usable = {0}, bounds = {0};
	SDL_GetDisplayUsableBounds(0, &usable);
	SDL_GetDisplayBounds(0, &bounds);
	printf("modo %s: janela %dx%d | saida do renderer %dx%d | razao %.3f | dpi %.0f (%.0f%%) | monitor %dx%d | util %dx%d\n",
	       mode, ww, wh, ow, oh, (double) ow / (double) ww, (double) ddpi, (double) ddpi / 96.0 * 100.0, bounds.w, bounds.h,
	       usable.w, usable.h);
	SDL_DestroyRenderer(r);
	SDL_DestroyWindow(w);
	SDL_Quit();
	return 0;
}
