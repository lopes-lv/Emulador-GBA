#include "nav.h"

#include "input.h"

NavDir nav_from_bits(uint32_t keys) {
	int up = (keys & input_button_bit(BTN_UP)) != 0, down = (keys & input_button_bit(BTN_DOWN)) != 0;
	int left = (keys & input_button_bit(BTN_LEFT)) != 0, right = (keys & input_button_bit(BTN_RIGHT)) != 0;
	if (up != down) return up ? NAV_UP : NAV_DOWN;
	if (left != right) return left ? NAV_LEFT : NAV_RIGHT;
	return NAV_NONE;
}

void navrepeat_reset(NavRepeat *n, NavDir held, uint32_t now_ms) {
	n->dir = held;
	n->next_ms = now_ms + NAV_FIRST_DELAY_MS;
}

NavDir navrepeat_step(NavRepeat *n, NavDir held, uint32_t now_ms) {
	if (held != n->dir) {
		n->dir = held;
		if (held == NAV_NONE) return NAV_NONE;
		n->next_ms = now_ms + NAV_FIRST_DELAY_MS;
		return held;
	}
	if (held != NAV_NONE && (int32_t) (now_ms - n->next_ms) >= 0) {
		n->next_ms = now_ms + NAV_REPEAT_MS;
		return held;
	}
	return NAV_NONE;
}
