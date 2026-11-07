#ifndef FRONTEND_ICONS_H
#define FRONTEND_ICONS_H

#include <stdint.h>

#include "canvas.h"

typedef enum {
	ICON_APP, ICON_MINIMIZE, ICON_MAXIMIZE, ICON_RESTORE, ICON_CLOSE, ICON_ADD, ICON_PLAY, ICON_SETTINGS,
	ICON_GAMEPAD, ICON_SEARCH, ICON_COUNT
} IconId;

/* Ícone linear (traço de 1,8 px lógicos) numa caixa quadrada `size` x `size` (unidades lógicas) com canto superior
 * esquerdo em (x, y). Traços horizontais e verticais são alinhados à grade de pixels (linhas inteiras e opacas);
 * diagonais e arcos são suavizados. Nada é desenhado a mais de 1 px da caixa. Id inválido não desenha nada. */
void icon_draw_c(Canvas *c, IconId id, int x, int y, int size, uint32_t color);

/* Atalho com escala 1 sobre um buffer solto. */
void icon_draw(uint32_t *fb, int fb_w, int fb_h, IconId id, int x, int y, int size, uint32_t color);

#endif
