#ifndef FRONTEND_CANVAS_H
#define FRONTEND_CANVAS_H

#include <stdint.h>

#include "gfx.h"

/* Superfície de desenho em pixels reais. Todas as funções recebem coordenadas LÓGICAS (as medidas de 1280x720 do
 * layout) e as convertem com o fator `scale`, alinhando as bordas à grade de pixels:
 *   um retângulo lógico (x, y, w, h) vira o retângulo de pixels com bordas round(x*S) e round((x+w)*S),
 * então retângulos vizinhos compartilham a borda e nunca deixam fresta. Tudo recorta no buffer. */
typedef struct {
	uint32_t *px; /* pixels ABGR8888, w*h */
	int w, h;
	float scale;  /* S */
	int ox, oy;   /* deslocamento (em pixels) da área lógica dentro do buffer */
} Canvas;

typedef enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT } Edge;

void canvas_init(Canvas *c, uint32_t *px, int w, int h, float scale, int ox, int oy);
/* Retângulo lógico -> retângulo em pixels (já com ox/oy). Largura/altura nunca menor que 1 quando a lógica é > 0. */
Rect canvas_device_rect(const Canvas *c, Rect logical);
/* Espessura das linhas finas em pixels: max(1, round(S)). */
int canvas_line_px(const Canvas *c);

void canvas_fill(Canvas *c, Rect logical, uint32_t color);
/* Cantos de `radius_logical`*S pixels com cobertura exata (rampa de 1 pixel); o miolo e as bordas retas ficam exatos. */
void canvas_fill_round(Canvas *c, Rect logical, int radius_logical, uint32_t color);
/* Caixa com cantos arredondados: borda de canvas_line_px pixels (espessura igual nos quatro lados) na cor `border` e miolo
 * na cor `body`, com o raio interno reduzido pela espessura. */
void canvas_box(Canvas *c, Rect logical, int radius_logical, uint32_t border, uint32_t body);
/* Desenha a imagem `src` (sw x sh, ABGR8888) no retângulo lógico: média de área na redução, bilinear na ampliação, com
 * cantos arredondados suavizados (raio lógico; 0 = retos). Imagem inválida não desenha nada. */
void canvas_blit(Canvas *c, Rect logical, const uint32_t *src, int sw, int sh, int radius_logical);
/* Moldura de canvas_line_px pixels, para dentro do retângulo, na cor exata. */
void canvas_frame(Canvas *c, Rect logical, uint32_t color);
/* Moldura de `lines` x canvas_line_px pixels (anel de foco), para dentro do retângulo. */
void canvas_frame_n(Canvas *c, Rect logical, int lines, uint32_t color);
/* Faixa de canvas_line_px pixels encostada no lado pedido, dentro do retângulo (nunca invade o vizinho). */
void canvas_edge(Canvas *c, Rect logical, Edge edge, uint32_t color);
/* Disco suavizado: centro e raio em unidades lógicas. */
void canvas_disc(Canvas *c, float cx, float cy, float radius, uint32_t color);
/* Linha suavizada: extremidades e espessura em unidades lógicas. */
void canvas_line(Canvas *c, float x0, float y0, float x1, float y1, float width, uint32_t color);

#endif
