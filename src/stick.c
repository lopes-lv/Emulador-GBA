#include "stick.h"

#include <string.h>

#include "input.h"
#include "settings.h"

void stick_reset(Stick *s) {
	memset(s, 0, sizeof(*s));
}

/* Atualiza as duas direções de um eixo (negativa e positiva) com histerese. */
static void step_axis(int value, int on, int off, int *negative, int *positive) {
	*negative = *negative ? value < -off : value < -on;
	*positive = *positive ? value > off : value > on;
}

uint32_t stick_step(Stick *s, int axis_x, int axis_y, int deadzone_pct) {
	if (deadzone_pct < STICK_DEADZONE_MIN) deadzone_pct = STICK_DEADZONE_MIN;
	if (deadzone_pct > STICK_DEADZONE_MAX) deadzone_pct = STICK_DEADZONE_MAX;
	int on = 32767 * deadzone_pct / 100;
	int off = on * 70 / 100;
	step_axis(axis_x, on, off, &s->left, &s->right);
	step_axis(axis_y, on, off, &s->up, &s->down);

	uint32_t keys = 0;
	if (s->up) keys |= input_button_bit(BTN_UP);
	if (s->down) keys |= input_button_bit(BTN_DOWN);
	if (s->left) keys |= input_button_bit(BTN_LEFT);
	if (s->right) keys |= input_button_bit(BTN_RIGHT);
	return keys;
}
