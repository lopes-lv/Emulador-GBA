#ifndef FRONTEND_GFX_H
#define FRONTEND_GFX_H

#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

int rect_contains(Rect r, int px, int py);

/* Todas as funções recebem o tamanho do buffer (pixels ABGR8888) e recortam nele. */
void gfx_fill(uint32_t *fb, int fb_w, int fb_h, Rect r, uint32_t color);
/* Linha anti-aliased com a espessura dada, misturada com o que já está no buffer. */
void gfx_line(uint32_t *fb, int fb_w, int fb_h, float x0, float y0, float x1, float y1, float width, uint32_t color);
/* Mistura `src` sobre `dst` com cobertura alpha (0..255); o resultado é opaco. */
uint32_t gfx_blend(uint32_t dst, uint32_t src, int alpha);
/* Razão de contraste WCAG 2.x entre duas cores ABGR (1.0 a 21.0). */
double gfx_contrast(uint32_t abgr_a, uint32_t abgr_b);

#endif
