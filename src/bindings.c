#include "bindings.h"

static const char *const LABELS[BTN_COUNT] = {
	"A", "B", "L", "R", "Start", "Select", "Cima", "Baixo", "Esquerda", "Direita",
};

static const char *const IDS[BTN_COUNT] = {
	"A", "B", "L", "R", "Start", "Select", "Up", "Down", "Left", "Right",
};

void bindings_defaults(Bindings *b) {
	static const SDL_Scancode keys[BTN_COUNT] = {
		[BTN_A] = SDL_SCANCODE_Z,
		[BTN_B] = SDL_SCANCODE_X,
		[BTN_L] = SDL_SCANCODE_A,
		[BTN_R] = SDL_SCANCODE_S,
		[BTN_START] = SDL_SCANCODE_RETURN,
		[BTN_SELECT] = SDL_SCANCODE_BACKSPACE,
		[BTN_UP] = SDL_SCANCODE_UP,
		[BTN_DOWN] = SDL_SCANCODE_DOWN,
		[BTN_LEFT] = SDL_SCANCODE_LEFT,
		[BTN_RIGHT] = SDL_SCANCODE_RIGHT,
	};
	static const SDL_GameControllerButton pads[BTN_COUNT] = {
		[BTN_A] = SDL_CONTROLLER_BUTTON_A,
		[BTN_B] = SDL_CONTROLLER_BUTTON_B,
		[BTN_L] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		[BTN_R] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
		[BTN_START] = SDL_CONTROLLER_BUTTON_START,
		[BTN_SELECT] = SDL_CONTROLLER_BUTTON_BACK,
		[BTN_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP,
		[BTN_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		[BTN_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		[BTN_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
	};
	for (int i = 0; i < BTN_COUNT; i++) {
		b->key[i] = keys[i];
		b->pad[i] = pads[i];
	}
}

void bindings_assign_key(Bindings *b, GbaButton btn, SDL_Scancode sc) {
	if (btn < 0 || btn >= BTN_COUNT) return;
	if (sc != SDL_SCANCODE_UNKNOWN) {
		for (int i = 0; i < BTN_COUNT; i++) {
			if (i != (int) btn && b->key[i] == sc) b->key[i] = SDL_SCANCODE_UNKNOWN;
		}
	}
	b->key[btn] = sc;
}

void bindings_assign_pad(Bindings *b, GbaButton btn, SDL_GameControllerButton button) {
	if (btn < 0 || btn >= BTN_COUNT) return;
	if (button != SDL_CONTROLLER_BUTTON_INVALID) {
		for (int i = 0; i < BTN_COUNT; i++) {
			if (i != (int) btn && b->pad[i] == button) b->pad[i] = SDL_CONTROLLER_BUTTON_INVALID;
		}
	}
	b->pad[btn] = button;
}

const char *bindings_button_label(GbaButton btn) {
	return (btn >= 0 && btn < BTN_COUNT) ? LABELS[btn] : "";
}

const char *bindings_button_id(GbaButton btn) {
	return (btn >= 0 && btn < BTN_COUNT) ? IDS[btn] : "";
}
