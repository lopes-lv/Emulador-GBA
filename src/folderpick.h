#ifndef FRONTEND_FOLDERPICK_H
#define FRONTEND_FOLDERPICK_H

#include <stddef.h>

/* Abre o seletor de pastas do Windows. Devolve 1 se o usuário escolheu (caminho UTF-8 com '/' em `out`),
 * 0 se cancelou, -1 em erro. */
int folder_pick(char *out_utf8, size_t out_size);

#endif
