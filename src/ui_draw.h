#ifndef FRONTEND_UI_DRAW_H
#define FRONTEND_UI_DRAW_H

#include <stdint.h>

#include "canvas.h"
#include "editor.h"
#include "gfx.h"
#include "menu.h"
#include "overlay.h"
#include "view.h"

typedef struct {
	int fps;            /* "<fps> FPS" na barra de status */
	int core_ready;     /* 1 = "Núcleo pronto" */
	int maximized;      /* 1 = o botão de maximizar mostra "restaurar" */
	int cursor_on;      /* fase do cursor piscante do campo de busca */
	int64_t now;        /* time_t atual, para "Última sessão" */
	const char *message; /* aviso temporário no lugar do texto da esquerda (UTF-8); NULL ou "" = nenhum */
} StatusInfo;

/* Desenha a biblioteca inteira no Canvas (medidas lógicas de 1280x720 convertidas pela escala do Canvas; o resto do
 * buffer, fora da área lógica, fica na cor da janela) e, se v->settings_open, o painel Configurações por cima. */
void ui_draw_library(Canvas *c, const View *v, const StatusInfo *st);
/* Biblioteca com a tela Controles (edição de teclas) como painel por cima. */
void ui_draw_keys(Canvas *c, const View *v, const StatusInfo *st, const Menu *m);
/* Menu do jogo (painel por cima do jogo pausado): desenha SÓ o painel; o resto do buffer não é tocado, então o chamador
 * o zera (transparente) antes. Medidas lógicas de 1280x720 pela escala do Canvas, como as demais telas. */
void ui_draw_overlay(Canvas *c, const Overlay *o);
/* Biblioteca com o editor de macros (tela "Auto clique") como painel por cima. `st->cursor_on` faz o cursor do nome e
 * da duração em digitação piscar. */
void ui_draw_editor(Canvas *c, const View *v, const StatusInfo *st, const Editor *e);
/* Retângulo do painel da tela Controles. */
Rect ui_keys_panel_rect(void);
/* Ponto verde ao lado de "<fps> FPS" na barra de status (unidades lógicas). */
Rect ui_status_dot_rect(const StatusInfo *st);

#endif
