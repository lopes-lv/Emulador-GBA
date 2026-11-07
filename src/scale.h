#ifndef FRONTEND_SCALE_H
#define FRONTEND_SCALE_H

#include "gfx.h"
#include "settings.h" /* ScaleMode */

/* Retângulo de destino da imagem src_w x src_h numa janela win_w x win_h: centrado, sem deformar, nunca com
 * w ou h menor que 1. SCALE_FILL = o maior possível; SCALE_INTEGER = o maior múltiplo inteiro que cabe
 * (se nem 1x cabe, cai em SCALE_FILL). */
Rect scale_fit(int win_w, int win_h, int src_w, int src_h, ScaleMode mode);
/* Fator da ampliação com pixels duros que antecede o filtro linear: ceil(dst_w / src_w) em 1..8. */
int scale_prescale(int dst_w, int src_w);
/* Posição do mouse na janela -> coordenadas da imagem. Devolve 1 se caiu dentro de `dst` (0 nas barras pretas);
 * *sx e *sy saem sempre recortados em 0..src-1. */
int scale_window_to_source(Rect dst, int src_w, int src_h, int wx, int wy, int *sx, int *sy);

#endif
