#ifndef FRONTEND_HOTKEYS_H
#define FRONTEND_HOTKEYS_H

#include <SDL.h>

typedef enum {
	HK_NONE, HK_FF_HOLD, HK_FF_TOGGLE, HK_SPEED_DOWN, HK_SPEED_UP, HK_VOL_DOWN, HK_VOL_UP, HK_MUTE,
	HK_FULLSCREEN, HK_SAVE, HK_LOAD, HK_ESCAPE, HK_OVERLAY
} Hotkey;

/* Atalho do jogo para a tecla; `mods` = SDL_Keymod. Alt+Enter também é tela cheia. F12 abre o menu do jogo. */
Hotkey hotkey_from_key(SDL_Scancode sc, Uint16 mods);
/* 1 se a tecla é de atalho (Tab, ESC, F3..F12) e não pode ser atribuída a um botão do GBA. */
int hotkey_is_reserved(SDL_Scancode sc);
/* 1 se o botão recém-apertado do controle abre o menu do jogo: Guide, ou L3 / R3 com os dois apertados juntos.
 * `pad_pressed` é indexado por SDL_GameControllerButton (SDL_CONTROLLER_BUTTON_MAX posições) e pode ser NULL. */
int hotkey_pad_opens_overlay(SDL_GameControllerButton pressed, const uint8_t *pad_pressed);

#endif
