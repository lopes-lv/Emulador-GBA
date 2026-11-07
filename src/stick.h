#ifndef FRONTEND_STICK_H
#define FRONTEND_STICK_H

#include <stdint.h>

/* Analógico -> direcional (8 direções). Lógica pura: recebe os eixos já lidos e devolve bits do GBA. */

/* Estado de histerese de cada direção; tudo zero = nada apertado. */
typedef struct { int up, down, left, right; } Stick;

void stick_reset(Stick *s);

/* Eixos do SDL em -32768..32767 (X negativo = esquerda, Y negativo = cima). deadzone_pct é recortado em 20..80.
 * Uma direção liga quando o eixo passa de 32767 * deadzone_pct / 100 e só desliga quando volta a 70% desse limite
 * ou menos. Devolve os bits do GBA de cima, baixo, esquerda e direita (os mesmos de input_button_bit). */
uint32_t stick_step(Stick *s, int axis_x, int axis_y, int deadzone_pct);

#endif
