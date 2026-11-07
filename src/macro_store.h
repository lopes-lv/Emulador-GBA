#ifndef FRONTEND_MACRO_STORE_H
#define FRONTEND_MACRO_STORE_H

#include <stddef.h>

#include "macro.h"

/* Macros em arquivos de texto legíveis, uma pasta por jogo. Caminhos em UTF-8.
 *
 * Formato (um arquivo por macro):
 *   nome=Farmar
 *   modo=loop          once | count | loop
 *   repetir=1          1..999 (só vale em count)
 *   do_estado=0        1 = cada repetição recomeça do estado salvo da macro
 *   ---
 *   A+B 12             botões unidos por '+' (A B L R Start Select Cima Baixo Esquerda Direita) e a duração em quadros
 *   - 30               '-' = espera
 *   Cima 8
 * Linhas vazias e comentários com '#' são ignorados (menos no nome). */

/* Texto da macro, alocado com malloc (o chamador faz free); NULL sem memória. */
char *macro_to_text(const Macro *m);
/* Lê o texto. 0 em sucesso; -1 com a mensagem em português (cita a linha) em err. Em erro, m fica vazia. */
int macro_from_text(Macro *m, const char *text, char *err, size_t err_size);
/* Nome de arquivo seguro (sem extensão) para o nome da macro: troca < > : " / \ | ? * e controles por '_', tira pontos e
 * espaços do fim, prefixa '_' em nomes reservados do Windows (CON, NUL, COM1..9...), vazio vira "macro", no máximo
 * 64 caracteres. Sempre cabe em out_size (cortando em limite de caractere UTF-8). */
void macro_file_name(const char *name, char *out, size_t out_size);

/* ---- disco, uma pasta por jogo ---- */

typedef struct {
	Macro *items;
	int count;
	int skipped; /* arquivos .txt inválidos ou ilegíveis, que não entraram em items */
} MacroList;

void macrolist_free(MacroList *l);

/* base_dir/<nome da ROM sem pasta e sem extensão, como macro_file_name>, com '/'. -1 se não couber em out. */
int macro_dir_for_rom(const char *base_dir, const char *rom_utf8, char *out, size_t out_size);
/* Cria a pasta e as que faltam no caminho. 0 se ela existe no fim. */
int macro_ensure_dir(const char *dir_utf8);
/* Lê todos os *.txt da pasta, em ordem de nome sem diferenciar maiúsculas. Pasta ausente = lista vazia (0). Arquivo
 * inválido ou ilegível não entra e soma em skipped. -1 só em erro de memória ou de leitura da pasta. */
int macro_list_dir(const char *dir_utf8, MacroList *out);
/* Cria a pasta se preciso e grava dir/<macro_file_name(nome)>.txt por arquivo temporário + renomeação. 0 em sucesso;
 * em falha não deixa o .tmp. */
int macro_save_file(const Macro *m, const char *dir_utf8);
/* Apaga o .txt e o .state da macro. 0 se apagou ou se não existia. */
int macro_delete_file(const char *dir_utf8, const char *name);
/* dir/<macro_file_name(nome)>.state */
void macro_state_path(const char *dir_utf8, const char *name, char *out, size_t out_size);
/* 1 se existe dir/<macro_file_name(nome)>.state. */
int macro_has_state(const char *dir_utf8, const char *name);
/* Grava `renamed` (já com o nome novo), move o .state e remove o .txt antigo. 0 ok; -1 erro; -2 se já existe OUTRA macro
 * com o nome de arquivo novo (nada é tocado). Se o nome de arquivo novo é igual ao antigo (só maiúsculas, ou o mesmo
 * arquivo depois de macro_file_name), só regrava o próprio arquivo. */
int macro_rename_files(const Macro *renamed, const char *dir_utf8, const char *old_name);
/* Copia o .state de uma macro para outra. 0 se copiou OU se a origem não tem .state; -1 em erro. */
int macro_copy_state(const char *dir_utf8, const char *from_name, const char *to_name);
/* "base 1", "base 2"...: o primeiro cujo nome de arquivo (sem diferenciar maiúsculas) não está na lista. 0 ok, -1 se não couber. */
int macro_unique_name(const MacroList *l, const char *base, char *out, size_t out_size);

#endif
