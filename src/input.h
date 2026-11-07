#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#include "bindings.h"

/* Converte teclado e controle na máscara de teclas do GBA (1 << GBA_KEY_*) esperada por core->setKeys.
 * `keyboard_state`: SDL_GetKeyboardState (indexado por SDL_Scancode).
 * `pad_pressed`: 1 por botão pressionado, indexado por SDL_GameControllerButton
 * (SDL_CONTROLLER_BUTTON_MAX posições); pode ser NULL quando não há controle. */
uint32_t input_map(const Bindings *b, const uint8_t *keyboard_state, const uint8_t *pad_pressed);

/* Bit de `b` na máscara de core->setKeys (1 << GBA_KEY_*); 0 para botão inválido. */
uint32_t input_button_bit(GbaButton b);

#endif
