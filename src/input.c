#include "input.h"

#include <SDL.h>
#include <mgba/internal/gba/input.h>

static const int GBA_KEY_OF[BTN_COUNT] = {
	[BTN_A] = GBA_KEY_A,
	[BTN_B] = GBA_KEY_B,
	[BTN_L] = GBA_KEY_L,
	[BTN_R] = GBA_KEY_R,
	[BTN_START] = GBA_KEY_START,
	[BTN_SELECT] = GBA_KEY_SELECT,
	[BTN_UP] = GBA_KEY_UP,
	[BTN_DOWN] = GBA_KEY_DOWN,
	[BTN_LEFT] = GBA_KEY_LEFT,
	[BTN_RIGHT] = GBA_KEY_RIGHT,
};

uint32_t input_button_bit(GbaButton b) {
	if ((int) b < 0 || (int) b >= BTN_COUNT) return 0;
	return 1u << GBA_KEY_OF[b];
}

uint32_t input_map(const Bindings *b, const uint8_t *keyboard_state, const uint8_t *pad_pressed) {
	uint32_t keys = 0;
	for (int i = 0; i < BTN_COUNT; i++) {
		int pressed = 0;
		if (b->key[i] != SDL_SCANCODE_UNKNOWN && keyboard_state[b->key[i]]) pressed = 1;
		if (pad_pressed && b->pad[i] != SDL_CONTROLLER_BUTTON_INVALID && pad_pressed[b->pad[i]]) pressed = 1;
		if (pressed) keys |= 1u << GBA_KEY_OF[i];
	}
	return keys;
}
