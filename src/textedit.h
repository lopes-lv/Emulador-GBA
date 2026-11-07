#ifndef FRONTEND_TEXTEDIT_H
#define FRONTEND_TEXTEDIT_H

#define TEXTEDIT_MAX 64 /* caracteres (não bytes) */

typedef struct {
	char text[TEXTEDIT_MAX * 4 + 1]; /* UTF-8 */
	int len_chars;
} TextEdit;

void textedit_clear(TextEdit *t);
/* Insere texto UTF-8 (vindo de SDL_TEXTINPUT). Descarta bytes inválidos, caracteres de controle e o que passar de
 * TEXTEDIT_MAX caracteres. Devolve 1 se algo mudou. */
int textedit_insert(TextEdit *t, const char *utf8);
/* Remove o último caractere inteiro (nunca meio byte). Devolve 1 se algo mudou. */
int textedit_backspace(TextEdit *t);

#endif
