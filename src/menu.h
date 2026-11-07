#ifndef FRONTEND_MENU_H
#define FRONTEND_MENU_H

#include "bindings.h"

/* Tela "Controles": edição das teclas e botões do GBA. A biblioteca (lista de jogos) vive em view.h. */

typedef enum {
	MEV_UP, MEV_DOWN, MEV_ENTER, MEV_ESC,
	MEV_KEY_PRESSED,  /* code = SDL_Scancode */
	MEV_PAD_PRESSED   /* code = SDL_GameControllerButton */
} MenuEventType;

typedef struct {
	MenuEventType type;
	int code; /* só usado em MEV_KEY_PRESSED e MEV_PAD_PRESSED */
} MenuEvent;

typedef enum { ACT_NONE, ACT_CONFIG_CHANGED, ACT_CLOSE } MenuAction;

/* Linhas da tela: 0..BTN_COUNT-1 são os botões do GBA, depois "Restaurar padrão" e "Voltar". */
#define KEY_ROW_RESET BTN_COUNT
#define KEY_ROW_BACK (BTN_COUNT + 1)

typedef struct {
	int open;          /* 1 = a tela Controles está visível */
	int key_row;       /* linha atual */
	int capturing;     /* 1 = esperando uma tecla/botão para o botão key_row */
	char message[160]; /* aviso ao usuário (UTF-8); vazio = nenhum */
	Bindings *bind;    /* associações editadas (não é dono) */
} Menu;

void menu_init(Menu *m, Bindings *bind);
/* Abre a tela na primeira linha, sem captura e sem aviso. */
void menu_open(Menu *m);
void menu_set_message(Menu *m, const char *msg_utf8);
/* Com a tela fechada todo evento é ignorado. ACT_CONFIG_CHANGED = bindings mudaram (gravar); ACT_CLOSE = voltar à lista.
 * Na captura, teclas reservadas para atalhos (hotkey_is_reserved) são recusadas com aviso e a captura continua. */
MenuAction menu_handle(Menu *m, MenuEvent ev);

#endif
