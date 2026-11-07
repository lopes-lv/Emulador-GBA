#ifndef FRONTEND_NAV_H
#define FRONTEND_NAV_H

#include <stdint.h>

/* Navegação por direção segurada (direcional e analógico) com repetição: lógica pura, o tempo vem de fora. */

typedef enum { NAV_NONE, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT } NavDir;

typedef struct { NavDir dir; uint32_t next_ms; } NavRepeat;

#define NAV_FIRST_DELAY_MS 400 /* segurando: tempo até a primeira repetição */
#define NAV_REPEAT_MS 120      /* depois disso, uma repetição a cada tanto */

/* Bits do GBA (os de input_button_bit) -> direção. Cima e baixo juntos se anulam, esquerda e direita também; sobrando a
 * vertical, ela vence a horizontal. */
NavDir nav_from_bits(uint32_t keys);
/* Começa a acompanhar `held`: o que já está segurado só passa a repetir depois do atraso (não dispara na hora). */
void navrepeat_reset(NavRepeat *n, NavDir held, uint32_t now_ms);
/* Direção a executar neste instante, ou NAV_NONE. Apertar ou trocar de direção dispara na hora. */
NavDir navrepeat_step(NavRepeat *n, NavDir held, uint32_t now_ms);

#endif
