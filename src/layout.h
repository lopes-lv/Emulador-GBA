#ifndef FRONTEND_LAYOUT_H
#define FRONTEND_LAYOUT_H

#include <stdint.h>

#include "gfx.h"
#include "settings.h" /* FF_SPEED_COUNT, TURBO_RATE_COUNT, BTN_COUNT */

/* Retângulos de cada região da interface 1280x720. Tudo é cálculo puro (sem estado, sem SDL). */

typedef struct { Rect title, toolbar, body, table_header, table_body, status; } Regions;
Regions layout_regions(void);

typedef enum { TB_ADD, TB_PLAY, TB_SETTINGS, TB_CONTROLS, TB_COUNT } ToolButton;
Rect layout_tool_button(ToolButton b); /* há um separador entre TB_PLAY e TB_SETTINGS */
Rect layout_search(void);              /* 280 px, encostada à direita com 12 px de margem */

typedef enum { WB_MIN, WB_MAX, WB_CLOSE } WindowButton;
Rect layout_window_button(WindowButton b); /* 46x32 cada, à direita do título */

#define TABLE_COLS 6 /* Título, Plataforma, Gênero, Ano, Tempo de jogo, Última sessão */
Rect layout_column(int c);                  /* cabeçalho e linhas compartilham x e w */
int layout_visible_rows(void);              /* linhas inteiras que cabem no corpo da tabela */
Rect layout_row(int visible_index);
/* Índice absoluto (visible_index + scroll) da linha sob o ponto, ou -1 fora das linhas inteiras. */
int layout_row_at(int px, int py, int scroll);

/* Painel Configurações (880x460, centrado no corpo, duas colunas: Som/Velocidade/Tela à esquerda, Turbo à direita). */
Rect layout_settings_panel(void);
Rect layout_settings_close(void);
Rect layout_slider(void);          /* trilha do volume (a área clicável inteira) */
Rect layout_mute_box(void);        /* linha inteira da caixa "Mudo" */
Rect layout_ff_segment(int i);     /* 6 segmentos iguais */
Rect layout_ff_always_box(void);   /* linha da caixa "Manter o avanço rápido sempre ativo" */
Rect layout_turbo_box(void);       /* linha da caixa "Repetir botão ao segurar (turbo)" */
Rect layout_turbo_rate(int i);     /* 4 segmentos iguais: 5/s, 10/s, 15/s, 30/s */
Rect layout_turbo_button(int i);   /* chip do botão i (ordem GbaButton), 2 fileiras de 5 */
Rect layout_stick_box(void);       /* linha da caixa "Usar o analógico como direcional" (coluna do turbo, abaixo dos chips) */
Rect layout_deadzone_slider(void); /* trilha da zona morta do analógico (a área clicável inteira) */
Rect layout_scale_radio(int i);    /* 0 = Preencher, 1 = Inteira (linha inteira) */
Rect layout_fullscreen_box(void);  /* linha inteira da caixa "Abrir jogos em tela cheia" */
Rect layout_autoclick_button(void); /* botão "Auto clique" (abre o editor de macros), canto inferior esquerdo, na altura do Fechar */
/* x da janela -> volume 0..100 em passos de 5; fora da trilha é recortado. */
int layout_slider_value(int px);

/* Painel em jogo (menu do jogo, aberto por F12): 560 de largura (584 de altura com 11 linhas) centrado em 1280x720,
 * cabeçalho de 56 px, uma linha de 44 px por item e 44 px de rodapé. Mesmas medidas lógicas da biblioteca; o desenho usa a escala real da janela. */
#define OVERLAY_ROWS 11
Rect layout_overlay_panel(void);
Rect layout_overlay_row(int i);          /* 0..OVERLAY_ROWS-1, 528x44, coladas */
Rect layout_overlay_slider(void);        /* trilha do volume, na linha 0 (240x16) */
Rect layout_overlay_speed(int i);        /* 6 segmentos de 48x32 na linha 2 */
/* x da janela -> volume 0..100 em passos de 5 pela trilha do painel; fora da trilha é recortado. */
int layout_overlay_volume_value(int px);
/* x da janela -> zona morta 20..80 em passos de 5; fora da trilha é recortado. */
int layout_deadzone_value(int px);

/* Editor de macros (tela "Auto clique", aberta pelas Configurações): painel 1120x600 centrado no corpo da janela.
 * Coluna da esquerda: lista de macros (linhas de 36 px) e os botões Novo/Duplicar/Apagar. À direita: nome, resumo,
 * modo, repetições, caixa "Começar do estado salvo", a tabela de passos (linhas de 34 px: número, 10 chips de botões de
 * 44 px e a duração com - e +) e os botões de passo. "Voltar" no canto inferior direito. */
#define EDITOR_LIST_ROWS 11
#define EDITOR_STEP_ROWS 9
typedef enum { EB_NEW, EB_DUP, EB_DEL, EB_COUNT } EditorListButton;
typedef enum { ESB_ADD, ESB_DUP, ESB_UP, ESB_DOWN, ESB_REMOVE, ESB_COUNT } EditorStepButton;
Rect layout_editor_panel(void);
Rect layout_editor_list_row(int i);
Rect layout_editor_list_btn(int i);        /* EditorListButton */
Rect layout_editor_name(void);             /* campo de texto, 360 px */
Rect layout_editor_summary(void);          /* "N passos · X,Y s", à direita do campo do nome */
Rect layout_editor_mode(int i);            /* 3 segmentos: Uma vez, N vezes, Repetir */
Rect layout_editor_repeat_minus(void);
Rect layout_editor_repeat_value(void);
Rect layout_editor_repeat_plus(void);
Rect layout_editor_from_state(void);       /* linha da caixa "Começar do estado salvo" */
Rect layout_editor_step_row(int i);
Rect layout_editor_chip(int row, int button); /* botão na ordem de GbaButton */
Rect layout_editor_dur_minus(int row);
Rect layout_editor_dur_value(int row);
Rect layout_editor_dur_plus(int row);
Rect layout_editor_step_btn(int i);        /* EditorStepButton */
Rect layout_editor_close(void);
/* Quadros (60 por segundo) -> milissegundos. */
int layout_editor_dur_ms(uint32_t frames);

#endif
