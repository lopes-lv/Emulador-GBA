#ifndef FRONTEND_TURBO_H
#define FRONTEND_TURBO_H

#include <stdint.h>

#include "settings.h"

/* Turbo: botão segurado repete (apertado/solto). O estado conta quadros emulados por botão. */
typedef struct {
	int held[BTN_COUNT]; /* quadros seguidos com o botão apertado (0 = solto) */
} Turbo;

void turbo_reset(Turbo *t);
/* Chame uma vez por quadro emulado com a máscara de teclas do GBA (input_map). Com o turbo ligado, os botões da
 * máscara de `s` ficam apertados nos primeiros N quadros de cada aperto, soltos nos N seguintes e assim por diante
 * (N = settings_turbo_half_frames). Soltar o botão reinicia o ciclo. Outros bits passam intactos. */
uint32_t turbo_step(Turbo *t, const Settings *s, uint32_t keys);

#endif
