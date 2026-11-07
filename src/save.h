#ifndef SAVE_H
#define SAVE_H

#include <stddef.h>

/* Monta o caminho irmão da ROM trocando a extensão por `ext` (ex.: ".ss1", ".sav").
 * Só considera o último '.' se vier depois do último separador; sem extensão, acrescenta `ext`.
 * Retorna 0, ou -1 se `out` for pequeno demais. */
int save_sibling_path(const char *rom_path, const char *ext, char *out, size_t out_size);

#endif
