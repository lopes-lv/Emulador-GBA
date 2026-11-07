#ifndef FRONTEND_VIEW_H
#define FRONTEND_VIEW_H

#include "gfx.h"
#include "library.h"
#include "settings.h"
#include "textedit.h"

/* Estado e eventos da tela da biblioteca (e do painel Configurações). Sem desenho e sem janela:
 * recebe eventos já convertidos para o espaço 1280x720 e devolve ações para o main. */

typedef enum { FOCUS_NONE, FOCUS_TOOLBAR, FOCUS_SEARCH, FOCUS_TABLE } FocusZone;
/* Itens focáveis do painel, na ordem do Tab. */
typedef enum {
	PF_SLIDER, PF_MUTE, PF_FF, PF_FF_ALWAYS, PF_TURBO, PF_TURBO_RATE, PF_TURBO_BUTTONS, PF_STICK, PF_STICK_DEADZONE,
	PF_SCALE, PF_FULLSCREEN, PF_AUTOCLICK, PF_CLOSE, PF_COUNT
} PanelFocus;
/* O que está sob o mouse (só para o desenho). */
typedef enum { HOVER_NONE, HOVER_WINBTN, HOVER_TOOL, HOVER_SEARCH, HOVER_ROW } HoverKind;

typedef struct {
	const Library *lib;   /* não é dono */
	Settings *settings;   /* não é dono; editado pelo painel */
	TextEdit search;
	int *visible;         /* índices da biblioteca que passam no filtro da busca */
	int visible_count;
	int selected;         /* índice em `visible`; -1 = nenhum */
	int scroll;           /* primeiro índice de `visible` na tela */
	FocusZone focus;
	int focus_item;       /* botão focado na barra de ferramentas */
	HoverKind hover_kind;
	int hover_item;
	int can_play;         /* 0 na demonstração */
	int settings_open;    /* painel visível: captura todos os eventos */
	PanelFocus panel_focus;
	int panel_button;     /* chip focado em PF_TURBO_BUTTONS (0..BTN_COUNT-1) */
	int dragging_slider;
	int dragging_deadzone; /* arrastando a trilha da zona morta do analógico */
} View;

typedef enum { VEV_MOUSE_MOVE, VEV_MOUSE_DOWN, VEV_MOUSE_UP, VEV_MOUSE_DBL, VEV_WHEEL, VEV_KEY, VEV_TEXT } ViewEventType;
typedef struct {
	ViewEventType type;
	int x, y;          /* mouse, no espaço 1280x720 */
	int key, mods;     /* SDL_Scancode e SDL_Keymod (VEV_KEY) */
	int dy;            /* roda: positivo = para cima (como no SDL) */
	const char *text;  /* VEV_TEXT, UTF-8 */
} ViewEvent;

typedef enum {
	VACT_NONE, VACT_PLAY, VACT_ADD_GAME, VACT_CONTROLS, VACT_SETTINGS_CHANGED, VACT_AUTOCLICK, VACT_WIN_MIN, VACT_WIN_MAX, VACT_QUIT
} ViewAction;

void view_init(View *v, Settings *settings);
/* Troca a biblioteca: reaplica a busca, limpa a seleção e volta ao topo. */
void view_set_library(View *v, const Library *lib, int can_play);
/* NULL se nada está selecionado. */
const LibEntry *view_selected(const View *v);
ViewAction view_handle(View *v, ViewEvent ev);
void view_free(View *v);

#endif
