#ifndef FRONTEND_MACRO_H
#define FRONTEND_MACRO_H

#include <stdint.h>

/* Macro: lista de passos {botões, duração em quadros emulados}. Lógica pura, sem janela e sem arquivos
 * (os arquivos ficam em macro_store.c). */

#define MACRO_NAME_MAX 64                  /* caracteres do nome */
#define MACRO_NAME_BYTES (MACRO_NAME_MAX * 4 + 1)
#define MACRO_MAX_STEPS 4096
#define MACRO_MAX_FRAMES 36000             /* 10 minutos a 60 quadros por segundo */
#define MACRO_MAX_REPEAT 999
#define MACRO_KEY_MASK 0x03FF              /* os 10 botões do GBA (bits de input_button_bit) */

typedef struct {
	uint16_t keys;     /* 0 = espera */
	uint32_t frames;   /* >= 1 */
} MacroStep;

typedef enum { MACRO_ONCE, MACRO_COUNT, MACRO_LOOP } MacroMode;

typedef struct {
	char name[MACRO_NAME_BYTES]; /* UTF-8 */
	MacroMode mode;
	int repeat;            /* 1..MACRO_MAX_REPEAT; só vale em MACRO_COUNT */
	int from_state;        /* 1 = cada repetição começa do estado salvo da macro */
	MacroStep *steps;
	int count;
	uint32_t total_frames; /* soma das durações */
} Macro;

/* Uma vez, repeat 1, sem estado salvo, sem passos. O nome é cortado em 64 caracteres (nunca no meio de um UTF-8). */
void macro_init(Macro *m, const char *name);
/* Libera os passos e zera; pode ser chamada de novo. */
void macro_free(Macro *m);
/* Acrescenta um passo. 0 ok; -1 (nada muda) se frames == 0, keys fora de MACRO_KEY_MASK, ou se passar de
 * MACRO_MAX_STEPS passos / MACRO_MAX_FRAMES quadros. Botões iguais aos do último passo somam a duração a ele. */
int macro_add_step(Macro *m, uint16_t keys, uint32_t frames);

/* ---- edição (editor de macros) ----
 * Todas devolvem 0 em sucesso e -1 sem mudar nada em falha; recalculam total_frames. Não juntam passos iguais. */

/* Insere em 0..count. -1 se frames fora de 1..MACRO_MAX_FRAMES, keys fora da máscara, ou se passar dos limites. */
int macro_step_insert(Macro *m, int index, uint16_t keys, uint32_t frames);
int macro_step_remove(Macro *m, int index);
/* Copia o passo logo depois dele (os limites valem). */
int macro_step_duplicate(Macro *m, int index);
/* Troca o passo com o vizinho (delta -1 ou +1). Devolve o novo índice, ou -1 se não há para onde ir. */
int macro_step_move(Macro *m, int index, int delta);
/* 1..MACRO_MAX_FRAMES, e o total da macro não pode passar de MACRO_MAX_FRAMES. */
int macro_step_set_frames(Macro *m, int index, uint32_t frames);
/* Liga/desliga UM bit da máscara. Um passo que fica sem botões vira espera (continua válido). */
int macro_step_toggle_key(Macro *m, int index, uint16_t bit);
/* Recorta em 1..MACRO_MAX_REPEAT. Sempre 0. */
int macro_set_repeat(Macro *m, int repeat);
/* Cópia profunda de src em dst (que não pode ter passos: use macro_init antes). 0 ou -1 sem memória. */
int macro_copy(Macro *dst, const Macro *src);

/* ---- gravador ---- */

typedef struct {
	Macro *macro;
	uint32_t armed;  /* botões já segurados ao começar (ex.: o Enter que confirmou "Gravar"): não entram até serem soltos */
	int arming;
} MacroRecorder;

/* Começa a gravar em dst: libera os passos que ele tinha. */
void macro_rec_begin(MacroRecorder *r, Macro *dst);
/* Grava 1 quadro (só os bits de MACRO_KEY_MASK). O primeiro quadro só registra o que já estava segurado; um botão
 * segurado desde o começo só passa a valer depois de solto e apertado de novo. 1 se gravou; 0 se o limite foi atingido
 * (nada gravado). */
int macro_rec_frame(MacroRecorder *r, uint32_t keys);
/* Apara os passos sem botões do começo e do fim e recalcula total_frames. */
void macro_rec_end(MacroRecorder *r);

/* ---- tocador ---- */

typedef enum {
	MP_RUNNING,      /* *keys = botões da macro neste quadro (consome 1 quadro) */
	MP_RELOAD,       /* recarregar o estado salvo da macro e chamar de novo (não consome quadro; *keys = 0) */
	MP_DONE,         /* terminou (ou já estava parada); *keys = teclas do usuário */
	MP_INTERRUPTED   /* o usuário apertou um botão novo; *keys = teclas do usuário */
} MacroPlayEvent;

typedef struct {
	const Macro *macro;
	int step, cycle;
	uint32_t left;      /* quadros que faltam no passo atual */
	uint32_t armed;     /* botões já segurados (não interrompem até serem soltos) */
	int active, arming, need_reload, finished;
} MacroPlayer;

void macro_play_start(MacroPlayer *p, const Macro *m);
/* Um quadro por chamada. user_keys = teclas do usuário neste quadro (para detectar a interrupção). Tocando, a saída
 * substitui as teclas do usuário. A primeira chamada só registra o que já estava segurado. */
MacroPlayEvent macro_play_step(MacroPlayer *p, uint32_t user_keys, uint32_t *keys);

#endif
