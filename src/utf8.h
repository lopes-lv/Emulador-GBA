#ifndef FRONTEND_UTF8_H
#define FRONTEND_UTF8_H

#include <stdint.h>

/* Decodifica um caractere UTF-8 e avança `*s`. Devolve 0 no fim da string;
 * sequência inválida ou truncada devolve 0xFFFD e avança um byte. */
uint32_t utf8_next_codepoint(const char **s);

#endif
