#ifndef FRONTEND_ROMSCAN_H
#define FRONTEND_ROMSCAN_H

#include <stddef.h>

typedef struct {
	char path[1024]; /* caminho UTF-8 com '/' */
	char name[256];  /* nome do arquivo sem a extensão */
} RomEntry;

typedef struct {
	RomEntry *items;
	int count;
} RomList;

/* Varre `dir_utf8` atrás de .gba/.GBA (recursivo até profundidade 3; a pasta raiz é a 0).
 * Devolve 0 (lista possivelmente vazia) ou -1 se a pasta não puder ser aberta (lista vazia).
 * Ordem: nome sem diferenciar maiúsculas, depois caminho. Entradas com path >= 1024 ou name >= 256 bytes são ignoradas. */
int romscan_scan(const char *dir_utf8, RomList *out);
/* Normaliza a pasta raiz: '\' vira '/' e a barra final é removida, exceto em "X:/" (raiz de disco, onde
 * "X:" sozinho significaria "pasta atual do disco X") e em "/". 0 em sucesso, -1 se não couber em `out`. */
int romscan_normalize_root(const char *dir_utf8, char *out, size_t out_size);
/* Junta pasta e nome com uma única '/' (sem duplicar quando a pasta já termina em '/', como "C:/"). 0 ou -1. */
int romscan_join_path(const char *dir, const char *name, char *out, size_t out_size);
/* Libera a lista; seguro com lista vazia. */
void romscan_free(RomList *list);

#endif
