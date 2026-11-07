#ifndef FRONTEND_VOLUME_H
#define FRONTEND_VOLUME_H

#include <stddef.h>
#include <stdint.h>

/* Ganho em ponto fixo (256 = 1,0) para o volume 0..100: curva quadrática (p/100)². Mudo ou fora da faixa:
 * 0 e 100 respectivamente. */
int volume_gain_q8(int percent, int muted);
/* Multiplica as amostras de 16 bits (intercaladas ou não) pelo ganho, no lugar. */
void volume_apply(int16_t *samples, size_t count, int gain_q8);

#endif
