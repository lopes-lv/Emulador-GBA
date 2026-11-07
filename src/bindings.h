#ifndef FRONTEND_BINDINGS_H
#define FRONTEND_BINDINGS_H

#include <SDL.h>

typedef enum {
	BTN_A, BTN_B, BTN_L, BTN_R, BTN_START, BTN_SELECT, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
	BTN_COUNT
} GbaButton;

typedef struct {
	SDL_Scancode key[BTN_COUNT];            /* SDL_SCANCODE_UNKNOWN = sem tecla */
	SDL_GameControllerButton pad[BTN_COUNT]; /* SDL_CONTROLLER_BUTTON_INVALID = sem botão */
} Bindings;

/* Teclado e controle padrão: as teclas do MVP e o mapeamento óbvio do controle. */
void bindings_defaults(Bindings *b);
/* Associa a tecla ao botão; se outro botão já a usava, esse outro fica sem tecla. */
void bindings_assign_key(Bindings *b, GbaButton btn, SDL_Scancode sc);
/* Idem para o botão do controle. */
void bindings_assign_pad(Bindings *b, GbaButton btn, SDL_GameControllerButton button);
/* Nome exibido ao usuário: "A", "B", "L", "R", "Start", "Select", "Cima", "Baixo", "Esquerda", "Direita". */
const char *bindings_button_label(GbaButton btn);
/* Nome usado no config.ini: A, B, L, R, Start, Select, Up, Down, Left, Right. */
const char *bindings_button_id(GbaButton btn);

#endif
