#ifndef FRONTEND_CONFIG_H
#define FRONTEND_CONFIG_H

#include <stddef.h>

#include "bindings.h"
#include "settings.h"

typedef struct {
	char rom_dir[1024]; /* pasta das ROMs (UTF-8, '/'); vazio = nenhuma escolhida */
	Bindings bind;
	Settings settings; /* som, avanço rápido e tela */
} Config;

/* rom_dir vazio e bindings padrão. */
void config_defaults(Config *c);
/* Parte dos padrões e aplica o arquivo. 0 se leu o arquivo; -1 se não existe/não abre (fica nos padrões).
 * Linhas inválidas e chaves desconhecidas são ignoradas; valor vazio em tecla./pad. = botão sem associação. */
int config_load(Config *c, const char *path_utf8);
/* 0 em sucesso, -1 em erro. */
int config_save(const Config *c, const char *path_utf8);
/* %APPDATA%\gba-frontend\config.ini em UTF-8 (cria a pasta). -1 se APPDATA indisponível ou `out` pequeno. */
int config_default_path(char *out, size_t out_size);

#endif
