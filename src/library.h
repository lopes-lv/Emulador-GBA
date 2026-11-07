#ifndef FRONTEND_LIBRARY_H
#define FRONTEND_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#include "romscan.h"

#define LIB_THUMB 64 /* lado da miniatura guardada; é reduzida ao desenhar (32 px lógicos x escala) */

typedef struct {
	char title[256], platform[16], genre[64], path[1024];
	int year;                 /* 0 = desconhecido (a interface mostra "—") */
	int64_t playtime_s;       /* segundos jogados */
	int64_t last_played;      /* time_t; 0 = nunca */
	int playable;             /* 0 nos jogos de demonstração */
	uint32_t thumb[LIB_THUMB * LIB_THUMB]; /* ABGR8888; válido se has_thumb */
	int has_thumb;
} LibEntry;

typedef struct {
	LibEntry *items;
	int count;
} Library;

#define PLATFORM_COUNT 1
extern const char *const PLATFORMS[PLATFORM_COUNT]; /* por enquanto só "GBA" */

/* Copia as ROMs: plataforma "GBA", gênero vazio, ano 0, jogável. A lista original continua do chamador. */
void library_from_roms(Library *lib, const RomList *roms);
/* Os 10 jogos fictícios do spec, todos "GBA" e não jogáveis. */
void library_demo(Library *lib);
void library_free(Library *lib);

/* Índices (na ordem da biblioteca) dos jogos que passam no filtro. `platform` NULL = todas; `query` NULL ou "" = sem
 * busca. A busca é por substring em título, plataforma e gênero, sem diferenciar maiúsculas nem acentos.
 * `out` precisa de lib->count posições. Devolve a quantidade. */
int library_filter(const Library *lib, const char *platform, const char *query, int *out);
int library_count_platform(const Library *lib, const char *platform); /* NULL = todos */

void library_format_playtime(int64_t seconds, char *out, size_t n); /* "24 h 10 min", "45 min" */
/* "Nunca", "Hoje", "Ontem", "Há 2 dias", "Há 3 semanas", "Há 5 meses", "Há 2 anos". */
void library_format_last(int64_t last_played, int64_t now, char *out, size_t n);

/* Aplica tempo jogado e última sessão (por caminho) vindos do arquivo. 0 em sucesso; -1 se não abrir (nada muda). */
int library_meta_load(Library *lib, const char *ini_path_utf8);
int library_meta_save(const Library *lib, const char *ini_path_utf8);
/* Soma `seconds` (se > 0) ao tempo jogado e marca `now` como última sessão. */
void library_add_session(LibEntry *e, int64_t seconds, int64_t now);


/* Miniaturas: recorta o centro 160x160 de um quadro 240x160 (ABGR8888) e reduz a 64x64 por média de área (cada pixel
 * cobre 2,5 x 2,5 pixels do quadro, com pesos fracionários). */
void library_thumb_make(const uint32_t *frame240x160, uint32_t out[LIB_THUMB * LIB_THUMB]);
/* Caminho do arquivo da miniatura: "<dir>/<hash FNV-1a do caminho da ROM, 8 hex>.thumb". 0 ou -1 (não coube). */
int library_thumb_file(const char *dir, const char *rom_path, char *out, size_t n);
/* Grava (criando a pasta, um nível) e lê o arquivo de 16384 bytes (os de 4096, do formato 32x32, são recusados). 0 ou -1; arquivo ausente ou de tamanho errado = -1. */
int library_thumb_save(const char *dir, const char *rom_path, const uint32_t px[LIB_THUMB * LIB_THUMB]);
int library_thumb_load(const char *dir, const char *rom_path, uint32_t px[LIB_THUMB * LIB_THUMB]);
/* Preenche thumb/has_thumb de cada jogo que tem miniatura gravada em `dir`. */
void library_thumbs_load(Library *lib, const char *dir);

#endif
