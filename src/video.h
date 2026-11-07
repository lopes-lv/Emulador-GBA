#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#include "settings.h"

typedef enum { VIDEO_UI, VIDEO_GAME } VideoMode;

/* Cria janela, renderer (vsync) e textura de origem width x height. 0 em sucesso, -1 em erro (mensagem em stderr). */
int video_init(int width, int height, int scale);
/* Troca o tamanho da imagem de origem e ajusta a janela (3x para 240x160, 2x para 480x320, 1x para 1280x720).
 * Mantido para a interface antiga; prefira video_set_mode. 0 ou -1. */
int video_set_size(int width, int height);
/* VIDEO_UI: biblioteca 1280x720, janela sem borda com barra de título própria (arrastável).
 * VIDEO_GAME: imagem 240x160, janela normal 720x480. Sai da tela cheia ao ir para VIDEO_UI. 0 ou -1. */
int video_set_mode(VideoMode mode);
/* Escala do jogo (a biblioteca sempre usa SCALE_FILL). */
void video_set_scale_mode(ScaleMode mode);
/* Tela cheia "desktop" (sem trocar a resolução), só no jogo; oculta o cursor. 0 ou -1. */
int video_set_fullscreen(int on);
int video_is_fullscreen(void);
void video_minimize(void);
void video_toggle_maximize(void);
int video_is_maximized(void);
/* Tamanho da saída de vídeo em pixels reais (com a escala do Windows, é maior que a janela em pontos). */
void video_ui_size(int *w, int *h);
/* Mostra o quadro da biblioteca w x h (ABGR8888) copiando 1:1 para a tela, sem ampliar nem filtrar. Se w x h for
 * diferente da saída atual (janela acabou de mudar de tamanho), o quadro é ajustado à janela só neste quadro. */
void video_present_ui(const uint32_t *pixels, int w, int h);
/* Pixels reais por ponto da janela (1,25 em 125%); 1 se não houver janela. */
float video_dpi_scale(void);
/* Posição do mouse na janela -> unidade lógica da biblioteca (1280x720), descontando as faixas. 1 se caiu na área. */
int video_window_to_ui(int wx, int wy, int *ux, int *uy);

/* Título fixo da janela (telas); no jogo o título mostra o FPS. */
void video_set_title(const char *title);
/* Mostra um frame de width*height pixels ABGR8888 (formato nativo de 32 bits do mGBA). */
void video_present(const uint32_t *pixels);
void video_set_title_fps(double fps);

/* Aviso no canto superior esquerdo, no tamanho real da janela. ms = 0 fica até video_osd_clear. */
void video_osd_show(const char *utf8, int ms);
void video_osd_clear(void);

/* Camada por cima do jogo (menu do jogo): o próximo video_present escurece a imagem do jogo e desenha a camada, com
 * transparência, em toda a saída. `pixels` é ABGR8888 w x h, do tamanho da saída (video_ui_size); é copiado.
 * Se w x h mudar, a textura é recriada. */
void video_overlay_set(const uint32_t *pixels, int w, int h);
void video_overlay_clear(void);
int video_overlay_active(void);

/* Ferramenta de verificação: o próximo video_present copia o que foi desenhado (antes do present) para `out`,
 * que precisa de w*h pixels do tamanho de saída do renderer. */
void video_capture_next(uint32_t *out, int w, int h);

void video_shutdown(void);

#endif
