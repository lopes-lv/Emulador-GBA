#ifndef FRONTEND_TTF_H
#define FRONTEND_TTF_H

#include <stdint.h>

#include "gfx.h"

/* Tamanhos lógicos da interface (pixels do em, com a escala 1). */
typedef enum { FS_BODY = 13, FS_SMALL = 12, FS_CAPS = 11 } FontSize;

/* Carrega a Inter embutida. 0 ou -1; idempotente (as demais funções chamam sozinhas se preciso). */
int ttf_init(void);
void ttf_shutdown(void);
/* Gama da cobertura dos glifos (padrão 0,85; menor = texto mais firme). Esvazia o cache. Usado na calibração. */
void ttf_set_gamma(float gamma);

/* ---- API em pixels reais: o texto é gerado no tamanho final, nunca esticado ---- */

/* round(logical * scale), com mínimo de 6 px. */
int ttf_px_size(FontSize logical, float scale);
int ttf_width_px(int px_size, const char *utf8);
int ttf_line_height_px(int px_size);
/* `x` pode ser fracionário (passos de 1/4 de pixel); `baseline_y` é a linha-base em pixel inteiro.
 * Mistura o glifo com o fundo (com correção de gama) e recorta no buffer. */
void ttf_draw_px(uint32_t *fb, int fb_w, int fb_h, int px_size, float x, int baseline_y, const char *utf8, uint32_t color);
/* Centraliza o texto na vertical dentro de `box` (em pixels), começando em box.x. Se não couber na largura, corta e
 * termina com "…". Nunca pinta fora de `box`. */
void ttf_draw_fit_px(uint32_t *fb, int fb_w, int fb_h, int px_size, Rect box, const char *utf8, uint32_t color);

/* ---- atalhos com escala 1 (tamanho lógico = pixels) ---- */

int ttf_width(FontSize size, const char *utf8);
int ttf_line_height(FontSize size);
/* `y_top` = topo da caixa de linha (ttf_line_height). */
void ttf_draw(uint32_t *fb, int fb_w, int fb_h, FontSize size, int x, int y_top, const char *utf8, uint32_t color);
void ttf_draw_fit(uint32_t *fb, int fb_w, int fb_h, FontSize size, Rect box, const char *utf8, uint32_t color);

#endif
