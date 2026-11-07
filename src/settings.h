#ifndef FRONTEND_SETTINGS_H
#define FRONTEND_SETTINGS_H

#include "bindings.h" /* GbaButton, BTN_COUNT */

typedef enum { SCALE_FILL, SCALE_INTEGER } ScaleMode;

#define FF_SPEED_COUNT 6
extern const double FF_SPEEDS[FF_SPEED_COUNT]; /* 1,5x 2x 3x 4x 6x 8x */

#define TURBO_RATE_COUNT 4
extern const int TURBO_RATES[TURBO_RATE_COUNT]; /* ciclos apertar+soltar por segundo: 5, 10, 15, 30 */

#define STICK_DEADZONE_MIN 20
#define STICK_DEADZONE_MAX 80
#define STICK_DEADZONE_STEP 5

typedef struct {
	int volume;           /* 0..100 */
	int muted;            /* 0/1 */
	int ff_index;         /* índice em FF_SPEEDS */
	ScaleMode scale;
	int start_fullscreen; /* 0/1: abrir os jogos em tela cheia */
	int ff_always;        /* 0/1: o jogo já começa com o avanço rápido ligado */
	int turbo;            /* 0/1: botão segurado repete (apertar/soltar) */
	int turbo_rate;       /* índice em TURBO_RATES */
	unsigned turbo_mask;  /* bit i = GbaButton i repete; só os bits 0..BTN_COUNT-1 valem */
	int stick_on;         /* 0/1: o analógico esquerdo age como direcional */
	int stick_deadzone;   /* STICK_DEADZONE_MIN..STICK_DEADZONE_MAX, em % do curso do eixo */
} Settings;

/* Volume 80, sem mudo, 2x, preencher, janela, avanço normal, turbo desligado a 10/s nos botões A e B,
 * analógico ligado com zona morta de 50%. */
void settings_defaults(Settings *s);
/* Multiplicador do avanço rápido (índice fora da faixa é recortado). */
double settings_ff_speed(const Settings *s);
/* Soma delta ao volume recortando em 0..100. 1 se o valor mudou. */
int settings_step_volume(Settings *s, int delta);
/* Soma delta ao índice da velocidade recortando em 0..FF_SPEED_COUNT-1. 1 se mudou. */
int settings_step_speed(Settings *s, int delta);
/* Soma delta à zona morta recortando em STICK_DEADZONE_MIN..STICK_DEADZONE_MAX. 1 se mudou. */
int settings_step_deadzone(Settings *s, int delta);
/* Alterna o mudo e devolve o novo valor. */
int settings_toggle_mute(Settings *s);
/* Quadros emulados por fase (apertado ou solto) do turbo: 6, 3, 2 ou 1. Taxa fora da faixa é recortada. */
int settings_turbo_half_frames(const Settings *s);
/* 1 se o botão está na máscara do turbo (botão inválido = 0). */
int settings_turbo_has(const Settings *s, GbaButton b);
/* Liga/desliga o turbo de um botão. 1 se mudou (0 para botão inválido). */
int settings_turbo_toggle_button(Settings *s, GbaButton b);

#endif
