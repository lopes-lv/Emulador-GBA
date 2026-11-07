#ifndef AUDIO_H
#define AUDIO_H

struct mCore;

/* Abre o dispositivo SDL (48000 Hz, estéreo, 16 bits) e configura o ressample do núcleo.
 * Retorna 0 em sucesso, -1 em erro (mensagem em stderr). */
int audio_init(struct mCore *core);
/* Bytes ainda na fila do dispositivo; usado para ditar o ritmo do loop. */
unsigned audio_queued_bytes(void);
/* Move as amostras geradas no último frame do núcleo para a fila do SDL. */
void audio_push(struct mCore *core);
/* Volume 0..100 e mudo, aplicados a cada audio_push seguinte (curva quadrática, ver volume.h). Sobrevive a game_open/close. */
void audio_set_volume(int percent, int muted);
/* Esvazia o áudio gerado pelo núcleo sem enfileirar (avanço rápido). */
void audio_discard(struct mCore *core);
/* Esvazia a fila do dispositivo (ao entrar e sair do avanço rápido, para não tocar áudio velho). */
void audio_clear_queue(void);
void audio_shutdown(void);

/* Teto da fila (~4 frames de áudio): o ritmo é do relógio; acima disso o loop espera a fila drenar. */
#define AUDIO_QUEUE_MAX_BYTES (4 * 804 * 4)

#endif
