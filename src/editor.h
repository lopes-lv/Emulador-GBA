#ifndef FRONTEND_EDITOR_H
#define FRONTEND_EDITOR_H

#include "macro_store.h"
#include "textedit.h"

/* Editor de macros (tela "Auto clique", aberta pelas Configurações): lógica pura, sem janela e sem disco, como view.c e
 * overlay.c. Edita uma cópia da lista de macros do jogo e devolve ações; o main grava os arquivos e mostra mensagens.
 * Eventos chegam já convertidos (mouse no espaço 1280x720). */

/* Zonas de foco, na ordem do Tab. */
typedef enum { EZ_LIST, EZ_NAME, EZ_MODE, EZ_REPEAT, EZ_STATE, EZ_STEPS, EZ_BUTTONS, EZ_COUNT } EditorZone;

typedef struct {
	int open;
	char title[256];     /* título do jogo, para o cabeçalho */
	MacroList list;      /* do editor; sempre em ordem alfabética sem diferenciar maiúsculas */
	int sel;             /* macro escolhida; -1 = nenhuma */
	int step;            /* passo escolhido; -1 = nenhum */
	int list_scroll, step_scroll;
	EditorZone zone;
	TextEdit name;       /* texto do nome durante a renomeação */
	int name_editing;
	int confirm_delete;  /* o Apagar foi acionado uma vez e espera a confirmação */
	int chip;            /* botão focado (0..BTN_COUNT-1) na zona EZ_BUTTONS */
	char dur_buf[8];     /* dígitos da duração em digitação (até 5) */
	int dur_editing;
	int *has_state;      /* has_state[i] = a macro i tem arquivo .state (do editor) */
	char message[160];   /* aviso ao usuário (UTF-8); vazio = nenhum */
} Editor;

typedef enum { EA_NONE, EA_CLOSE, EA_SAVE, EA_RENAME, EA_DELETE, EA_DUPLICATE } EditorActionType;

typedef struct {
	EditorActionType type;
	int index;                       /* macro afetada, na lista atual (em EA_DELETE: onde ela estava) */
	char old_name[MACRO_NAME_BYTES]; /* EA_RENAME: nome antigo; EA_DELETE: nome apagado; EA_DUPLICATE: nome da origem */
} EditorAction;

typedef enum { EEV_KEY, EEV_TEXT, EEV_MOUSE_MOVE, EEV_MOUSE_DOWN, EEV_MOUSE_UP, EEV_WHEEL } EditorEventType;

typedef struct {
	EditorEventType type;
	int x, y;          /* mouse, no espaço 1280x720 */
	int key, mods;     /* EEV_KEY: SDL_Scancode e SDL_Keymod */
	int dy;            /* EEV_WHEEL: positivo = para cima (como no SDL) */
	const char *text;  /* EEV_TEXT, UTF-8 */
	int clicks;        /* EEV_MOUSE_DOWN: 1 clique simples, 2 ou mais = duplo clique (como SDL_MouseButtonEvent.clicks) */
} EditorEvent;

void editor_init(Editor *e);
/* Abre a tela. TOMA POSSE de *list (o original fica zerado), escolhe a primeira macro e calcula has_state com
 * macro_has_state(dir, ...). A lista deve vir em ordem alfabética (como macro_list_dir devolve). */
void editor_open(Editor *e, const char *game_title, MacroList *list, const char *dir_utf8);
/* Escolhe a macro `index` (recortada na lista; zera o passo e cancela edições em andamento). */
void editor_select(Editor *e, int index);
/* Libera tudo e fecha. */
void editor_close(Editor *e);
void editor_set_message(Editor *e, const char *msg_utf8);
/* Fechado, todo evento é ignorado. Toda mudança que precisa ir para o disco devolve a ação correspondente. */
EditorAction editor_handle(Editor *e, EditorEvent ev);

#endif
