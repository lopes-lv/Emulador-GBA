#ifndef GAME_H
#define GAME_H

#include <stddef.h>
#include <stdint.h>

/* Uma ROM aberta no núcleo mGBA, junto com o áudio dela. */
typedef struct Game Game;

/* Abre a ROM (caminho UTF-8 com '/'). Em erro devolve NULL e escreve a mensagem em `err`. */
Game *game_open(const char *rom_utf8, char *err, size_t err_size);
void game_set_keys(Game *g, uint32_t keys);
/* Roda um frame e envia o áudio gerado para a fila do SDL. */
void game_run_frame(Game *g);
/* Roda `frames` quadros. with_audio = 1 envia o áudio de cada um (com o volume atual); 0 descarta (avanço rápido).
 * frames <= 0 não faz nada. */
void game_run_frames(Game *g, int frames, int with_audio);
/* Pixels ABGR8888 de 240x160 do último frame. */
const uint32_t *game_pixels(const Game *g);
/* 1 em sucesso, 0 em falha. */
int game_save_state(Game *g);
int game_load_state(Game *g);
/* Estado em um arquivo qualquer (caminho UTF-8 com '/'), sem tocar no estado normal do usuário (F5/F8): é o que as
 * macros usam para recomeçar do mesmo ponto. 1 em sucesso, 0 em falha (pasta ou arquivo ausente, arquivo inválido).
 * Carregar nunca sobrescreve o .sav do jogo. */
int game_save_state_to(Game *g, const char *path_utf8);
int game_load_state_from(Game *g, const char *path_utf8);
const char *game_state_path(const Game *g);
const char *game_sav_path(const Game *g);
void game_close(Game *g);

#endif
