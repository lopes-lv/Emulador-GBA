#ifndef FRONTEND_UI_METRICS_H
#define FRONTEND_UI_METRICS_H

/* Onde fica a área lógica da interface (1280x720) dentro da saída de vídeo em pixels. Cálculo puro, sem SDL. */

#define UI_LOGICAL_W 1280
#define UI_LOGICAL_H 720
#define UI_SCALE_MIN 0.75f
#define UI_SCALE_MAX 4.0f

typedef struct {
	float scale;  /* S: pixels por unidade lógica */
	int ox, oy;   /* canto superior esquerdo da área lógica, em pixels (pode ser negativo se a saída for menor que o mínimo) */
	int w, h;     /* tamanho da área lógica em pixels: round(1280*S) x round(720*S) */
} UiMetrics;

/* S = menor razão entre a saída e 1280x720, limitada a UI_SCALE_MIN..UI_SCALE_MAX; a área fica centrada. */
UiMetrics ui_metrics_fit(int out_w, int out_h);
/* Pixel da janela -> unidade lógica (piso). 1 se caiu dentro da área; senão 0, com *lx e *ly recortados à área. */
int ui_metrics_window_to_logical(const UiMetrics *m, int px, int py, int *lx, int *ly);

#endif
