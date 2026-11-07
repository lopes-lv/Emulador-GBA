#ifndef FRONTEND_OVERLAY_H
#define FRONTEND_OVERLAY_H

#include "macro_store.h"
#include "settings.h"

/* Estado e eventos do menu do jogo (painel por cima do jogo pausado). Sem desenho e sem janela: recebe eventos já
 * convertidos (mouse no espaço 1280x720) e devolve ações para o main, como o view.c faz com a biblioteca. */

/* Itens na ordem das linhas do painel (layout_overlay_row). Os três do meio são o auto clique: escolher a macro,
 * tocar/parar e gravar/parar. */
typedef enum {
	OI_VOLUME, OI_MUTE, OI_SPEED, OI_TURBO, OI_SAVE, OI_LOAD, OI_MACRO, OI_MACRO_PLAY, OI_MACRO_REC, OI_QUIT, OI_BACK,
	OI_COUNT
} OverlayItem;

/* O que as macros estão fazendo agora (o main atualiza). */
typedef enum { MUI_IDLE, MUI_PLAYING, MUI_RECORDING } MacroUiState;

typedef struct {
	Settings *settings;   /* não é dono; editado pelo painel */
	int open;
	OverlayItem focus;
	int dragging_volume;  /* arrastando a trilha do volume */
	const MacroList *macros; /* macros do jogo; não é dono; NULL = nenhuma */
	int macro_sel;        /* índice da macro escolhida em macros->items */
	MacroUiState macro_state;
	int confirm_quit;     /* "Sair do jogo" foi acionado uma vez e espera a confirmação */
} Overlay;

typedef enum {
	OEV_MOUSE_MOVE, OEV_MOUSE_DOWN, OEV_MOUSE_UP,
	OEV_UP, OEV_DOWN, OEV_LEFT, OEV_RIGHT, OEV_ACCEPT, OEV_BACK
} OverlayEventType;

typedef struct {
	OverlayEventType type;
	int x, y; /* mouse, no espaço 1280x720; (-1,-1) = fora da imagem */
} OverlayEvent;

typedef enum {
	OACT_NONE, OACT_CLOSE, OACT_SETTINGS_CHANGED, OACT_SAVE_STATE, OACT_LOAD_STATE,
	OACT_MACRO_PLAY, OACT_MACRO_RECORD, OACT_MACRO_STOP, /* o main faz; Tocar e Gravar também fecham o painel */
	OACT_QUIT_GAME /* confirmado: o main fecha o jogo e volta à biblioteca */
} OverlayAction;

/* Fechado, com o foco no volume. */
void overlay_init(Overlay *o, Settings *settings);
/* Abre com o foco no volume e sem arrasto pendente; recorta macro_sel em 0..count-1. */
void overlay_open(Overlay *o);
/* Com o painel fechado não faz nada. OACT_CLOSE também fecha (open = 0). "Sair do jogo" pede confirmação: o primeiro
 * acionamento só arma (confirm_quit); o segundo devolve OACT_QUIT_GAME; qualquer outra ação desarma. OACT_SETTINGS_CHANGED só quando um valor
 * realmente mudou (volume, mudo, velocidade ou turbo). */
OverlayAction overlay_handle(Overlay *o, OverlayEvent ev);

#endif
