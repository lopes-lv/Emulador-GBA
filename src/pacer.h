#ifndef FRONTEND_PACER_H
#define FRONTEND_PACER_H

#include <stdint.h>

#define GBA_FPS 59.7275      /* 16777216 / 280896 */
#define PACER_MAX_FRAMES 16  /* teto de quadros emulados por volta do laço */

typedef struct {
	double carry;  /* fração de quadro acumulada */
	uint64_t last; /* instante da última chamada, em ticks do relógio */
} Pacer;

/* Zera o acumulado e usa `now` como referência. */
void pacer_reset(Pacer *p, uint64_t now);
/* Quadros a emular desde a última chamada: (now − last) / freq × multiplier × GBA_FPS, com a fração acumulada.
 * No máximo PACER_MAX_FRAMES; o excedente é descartado. Relógio parado devolve 0; relógio que retrocede devolve 0
 * e refaz a referência. */
int pacer_frames_due(Pacer *p, uint64_t now, uint64_t freq, double multiplier);

#endif
