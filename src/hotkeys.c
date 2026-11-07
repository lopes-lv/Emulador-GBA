#include "hotkeys.h"

Hotkey hotkey_from_key(SDL_Scancode sc, Uint16 mods) {
	switch (sc) {
	case SDL_SCANCODE_TAB: return HK_FF_HOLD;
	case SDL_SCANCODE_F3: return HK_MUTE;
	case SDL_SCANCODE_F4: return HK_FF_TOGGLE;
	case SDL_SCANCODE_F5: return HK_SAVE;
	case SDL_SCANCODE_F6: return HK_SPEED_DOWN;
	case SDL_SCANCODE_F7: return HK_SPEED_UP;
	case SDL_SCANCODE_F8: return HK_LOAD;
	case SDL_SCANCODE_F9: return HK_VOL_DOWN;
	case SDL_SCANCODE_F10: return HK_VOL_UP;
	case SDL_SCANCODE_F11: return HK_FULLSCREEN;
	case SDL_SCANCODE_F12: return HK_OVERLAY;
	case SDL_SCANCODE_ESCAPE: return HK_ESCAPE;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER: return (mods & KMOD_ALT) ? HK_FULLSCREEN : HK_NONE;
	default: return HK_NONE;
	}
}

int hotkey_is_reserved(SDL_Scancode sc) {
	if (sc == SDL_SCANCODE_TAB || sc == SDL_SCANCODE_ESCAPE) return 1;
	return sc >= SDL_SCANCODE_F3 && sc <= SDL_SCANCODE_F12;
}

int hotkey_pad_opens_overlay(SDL_GameControllerButton pressed, const uint8_t *pad_pressed) {
	if (pressed == SDL_CONTROLLER_BUTTON_GUIDE) return 1;
	if (pressed != SDL_CONTROLLER_BUTTON_LEFTSTICK && pressed != SDL_CONTROLLER_BUTTON_RIGHTSTICK) return 0;
	return pad_pressed && pad_pressed[SDL_CONTROLLER_BUTTON_LEFTSTICK] && pad_pressed[SDL_CONTROLLER_BUTTON_RIGHTSTICK];
}
