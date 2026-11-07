#include "editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>

#include "gfx.h"
#include "input.h"
#include "layout.h"

#define WHEEL_LINES 3
#define NEW_STEP_FRAMES 10

/* ---- auxiliares ---- */

static EditorAction act(EditorActionType type, int index) {
	EditorAction a;
	memset(&a, 0, sizeof(a));
	a.type = type;
	a.index = index;
	return a;
}

static EditorAction none(void) {
	return act(EA_NONE, 0);
}

static Macro *cur(Editor *e) {
	return (e->sel >= 0 && e->sel < e->list.count) ? &e->list.items[e->sel] : NULL;
}

static void set_msg(Editor *e, const char *fmt, const char *arg) {
	snprintf(e->message, sizeof(e->message), fmt, arg ? arg : "");
}

void editor_set_message(Editor *e, const char *msg_utf8) {
	snprintf(e->message, sizeof(e->message), "%s", msg_utf8 ? msg_utf8 : "");
}

/* Copia os primeiros `max_chars` caracteres (UTF-8) de `s`. */
static void utf8_prefix(const char *s, int max_chars, char *out, size_t out_size) {
	int chars = 0;
	size_t len = 0;
	for (const unsigned char *p = (const unsigned char *) s; *p && len + 1 < out_size; p++) {
		if ((*p & 0xC0) != 0x80) {
			if (chars == max_chars) break;
			chars++;
		}
		out[len++] = (char) *p;
	}
	out[len] = '\0';
}

/* O nome de arquivo de `name` já é de outra macro da lista (menos a de índice `except`)? */
static int name_taken(const Editor *e, const char *name, int except) {
	char wanted[MACRO_NAME_BYTES + 4], other[MACRO_NAME_BYTES + 4];
	macro_file_name(name, wanted, sizeof(wanted));
	for (int j = 0; j < e->list.count; j++) {
		if (j == except) continue;
		macro_file_name(e->list.items[j].name, other, sizeof(other));
		if (_stricmp(wanted, other) == 0) return 1;
	}
	return 0;
}

/* ---- lista ---- */

static void clamp_list_scroll(Editor *e) {
	int max = e->list.count - EDITOR_LIST_ROWS;
	if (max < 0) max = 0;
	if (e->list_scroll > max) e->list_scroll = max;
	if (e->list_scroll < 0) e->list_scroll = 0;
}

static void ensure_list_visible(Editor *e) {
	if (e->sel >= 0) {
		if (e->sel < e->list_scroll) e->list_scroll = e->sel;
		if (e->sel >= e->list_scroll + EDITOR_LIST_ROWS) e->list_scroll = e->sel - EDITOR_LIST_ROWS + 1;
	}
	clamp_list_scroll(e);
}

static void select_macro(Editor *e, int index) {
	if (index >= e->list.count) index = e->list.count - 1;
	if (index < 0) index = e->list.count > 0 ? 0 : -1;
	e->sel = index;
	Macro *m = cur(e);
	e->step = (m && m->count > 0) ? 0 : -1;
	e->step_scroll = 0;
	e->name_editing = 0;
	e->dur_editing = 0;
	ensure_list_visible(e);
}

/* Insere `*m` (a lista passa a ser dona) na posição alfabética. Devolve o índice, ou -1 sem memória. */
static int list_insert_sorted(Editor *e, const Macro *m, int has_state) {
	Macro *items = realloc(e->list.items, (size_t) (e->list.count + 1) * sizeof(Macro));
	if (items) e->list.items = items;
	int *flags = realloc(e->has_state, (size_t) (e->list.count + 1) * sizeof(int));
	if (flags) e->has_state = flags;
	if (!items || !flags) return -1;
	int pos = e->list.count;
	for (int i = 0; i < e->list.count; i++) {
		if (_stricmp(e->list.items[i].name, m->name) > 0) {
			pos = i;
			break;
		}
	}
	memmove(e->list.items + pos + 1, e->list.items + pos, (size_t) (e->list.count - pos) * sizeof(Macro));
	memmove(e->has_state + pos + 1, e->has_state + pos, (size_t) (e->list.count - pos) * sizeof(int));
	e->list.items[pos] = *m;
	e->has_state[pos] = has_state;
	e->list.count++;
	return pos;
}

/* Tira a macro `index` da lista, entregando a struct (e o flag de estado) a quem chamou. */
static void list_remove(Editor *e, int index, Macro *out, int *has_state_out) {
	*out = e->list.items[index];
	*has_state_out = e->has_state[index];
	memmove(e->list.items + index, e->list.items + index + 1, (size_t) (e->list.count - index - 1) * sizeof(Macro));
	memmove(e->has_state + index, e->has_state + index + 1, (size_t) (e->list.count - index - 1) * sizeof(int));
	e->list.count--;
}

/* ---- ações sobre macros ---- */

static EditorAction do_new(Editor *e) {
	char name[MACRO_NAME_BYTES];
	if (macro_unique_name(&e->list, "Macro", name, sizeof(name)) != 0) return none();
	Macro m;
	macro_init(&m, name);
	macro_step_insert(&m, 0, (uint16_t) input_button_bit(BTN_A), NEW_STEP_FRAMES);
	int pos = list_insert_sorted(e, &m, 0);
	if (pos < 0) {
		macro_free(&m);
		editor_set_message(e, "Sem mem\xC3\xB3ria para criar a macro");
		return none();
	}
	select_macro(e, pos);
	return act(EA_SAVE, pos);
}

static EditorAction do_duplicate(Editor *e) {
	Macro *src = cur(e);
	if (!src) return none();
	char old_name[MACRO_NAME_BYTES], base[MACRO_NAME_BYTES], name[MACRO_NAME_BYTES];
	snprintf(old_name, sizeof(old_name), "%s", src->name);
	utf8_prefix(src->name, MACRO_NAME_MAX - 8, base, sizeof(base)); /* sobra espaço para " N" sem passar de 64 */
	if (macro_unique_name(&e->list, base, name, sizeof(name)) != 0) return none();
	Macro dup;
	macro_init(&dup, "");
	if (macro_copy(&dup, src) != 0) return none();
	snprintf(dup.name, sizeof(dup.name), "%s", name);
	int pos = list_insert_sorted(e, &dup, e->has_state[e->sel]);
	if (pos < 0) {
		macro_free(&dup);
		editor_set_message(e, "Sem mem\xC3\xB3ria para duplicar a macro");
		return none();
	}
	select_macro(e, pos);
	EditorAction a = act(EA_DUPLICATE, pos);
	snprintf(a.old_name, sizeof(a.old_name), "%s", old_name);
	return a;
}

/* Primeiro acionamento arma a confirmação; o segundo apaga. */
static EditorAction do_delete(Editor *e, int armed) {
	Macro *m = cur(e);
	if (!m) return none();
	if (!armed) {
		e->confirm_delete = 1;
		set_msg(e, "Apagar '%s'? Acione de novo para confirmar", m->name);
		return none();
	}
	int index = e->sel, has;
	Macro removed;
	list_remove(e, index, &removed, &has);
	EditorAction a = act(EA_DELETE, index);
	snprintf(a.old_name, sizeof(a.old_name), "%s", removed.name);
	macro_free(&removed);
	select_macro(e, index < e->list.count ? index : e->list.count - 1);
	editor_set_message(e, "Macro apagada");
	return a;
}

static void begin_rename(Editor *e) {
	Macro *m = cur(e);
	if (!m) return;
	e->name_editing = 1;
	textedit_clear(&e->name);
	textedit_insert(&e->name, m->name);
}

static EditorAction commit_rename(Editor *e) {
	Macro *m = cur(e);
	if (!m) {
		e->name_editing = 0;
		return none();
	}
	char text[sizeof(e->name.text)];
	snprintf(text, sizeof(text), "%s", e->name.text);
	char *start = text;
	while (*start == ' ' || *start == '\t') start++;
	char *end = start + strlen(start);
	while (end > start && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
	if (*start == '\0') {
		editor_set_message(e, "O nome n\xC3\xA3o pode ficar vazio");
		return none();
	}
	if (strcmp(start, m->name) == 0) {
		e->name_editing = 0;
		return none();
	}
	if (name_taken(e, start, e->sel)) {
		editor_set_message(e, "J\xC3\xA1 existe uma macro com esse nome");
		return none();
	}
	char old_name[MACRO_NAME_BYTES];
	snprintf(old_name, sizeof(old_name), "%s", m->name);
	Macro moved;
	int has;
	list_remove(e, e->sel, &moved, &has);
	Macro named;
	macro_init(&named, start);
	snprintf(moved.name, sizeof(moved.name), "%s", named.name);
	int pos = list_insert_sorted(e, &moved, has);
	if (pos < 0) pos = 0; /* sem memória: a macro já estava na lista; não deve acontecer (só encolhemos antes) */
	e->sel = pos;
	e->name_editing = 0;
	ensure_list_visible(e);
	EditorAction a = act(EA_RENAME, pos);
	snprintf(a.old_name, sizeof(a.old_name), "%s", old_name);
	return a;
}

static EditorAction set_mode(Editor *e, MacroMode mode) {
	Macro *m = cur(e);
	if (!m || m->mode == mode) return none();
	m->mode = mode;
	return act(EA_SAVE, e->sel);
}

static EditorAction change_mode(Editor *e, int direction) {
	Macro *m = cur(e);
	if (!m) return none();
	int mode = (int) m->mode + direction;
	if (mode < MACRO_ONCE || mode > MACRO_LOOP) return none();
	return set_mode(e, (MacroMode) mode);
}

/* Repetições só valem em "N vezes"; nos outros modos o controle fica inativo. */
static EditorAction change_repeat(Editor *e, int delta) {
	Macro *m = cur(e);
	if (!m || m->mode != MACRO_COUNT) return none();
	int before = m->repeat;
	macro_set_repeat(m, before + delta);
	return m->repeat == before ? none() : act(EA_SAVE, e->sel);
}

static EditorAction toggle_state(Editor *e) {
	Macro *m = cur(e);
	if (!m) return none();
	if (!e->has_state[e->sel]) {
		editor_set_message(e, "Esta macro n\xC3\xA3o tem estado salvo (grave de novo para criar)");
		return none();
	}
	m->from_state = !m->from_state;
	return act(EA_SAVE, e->sel);
}

/* ---- passos ---- */

#define DUR_DIGITS 5

static void clamp_step_scroll(Editor *e) {
	Macro *m = cur(e);
	int max = (m ? m->count : 0) - EDITOR_STEP_ROWS;
	if (max < 0) max = 0;
	if (e->step_scroll > max) e->step_scroll = max;
	if (e->step_scroll < 0) e->step_scroll = 0;
}

static void select_step(Editor *e, int index) {
	Macro *m = cur(e);
	if (!m || m->count == 0) {
		e->step = -1;
		return;
	}
	if (index < 0) index = 0;
	if (index >= m->count) index = m->count - 1;
	e->step = index;
	if (e->step < e->step_scroll) e->step_scroll = e->step;
	if (e->step >= e->step_scroll + EDITOR_STEP_ROWS) e->step_scroll = e->step - EDITOR_STEP_ROWS + 1;
	clamp_step_scroll(e);
}

/* Dígito de uma tecla da fileira de cima ou do teclado numérico: 0..9, ou -1. */
static int digit_of(int sc) {
	if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return sc - SDL_SCANCODE_1 + 1;
	if (sc == SDL_SCANCODE_0) return 0;
	if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return sc - SDL_SCANCODE_KP_1 + 1;
	if (sc == SDL_SCANCODE_KP_0) return 0;
	return -1;
}

static int is_keypad_digit(int sc) {
	return (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_0);
}

/* A operação sobre os passos foi recusada pelos limites: avisa qual. */
static EditorAction step_limit(Editor *e) {
	Macro *m = cur(e);
	if (m && m->count >= MACRO_MAX_STEPS) editor_set_message(e, "Limite de 4096 passos");
	else editor_set_message(e, "A macro passou de 36000 quadros (10 min)");
	return none();
}

static EditorAction do_step_add(Editor *e) {
	Macro *m = cur(e);
	if (!m) return none();
	int at = e->step >= 0 ? e->step + 1 : m->count;
	if (macro_step_insert(m, at, (uint16_t) input_button_bit(BTN_A), NEW_STEP_FRAMES) != 0) return step_limit(e);
	select_step(e, at);
	return act(EA_SAVE, e->sel);
}

static EditorAction do_step_duplicate(Editor *e) {
	Macro *m = cur(e);
	if (!m || e->step < 0) return none();
	if (macro_step_duplicate(m, e->step) != 0) return step_limit(e);
	select_step(e, e->step + 1);
	return act(EA_SAVE, e->sel);
}

static EditorAction do_step_move(Editor *e, int direction) {
	Macro *m = cur(e);
	if (!m || e->step < 0) return none();
	int to = macro_step_move(m, e->step, direction);
	if (to < 0) return none();
	select_step(e, to);
	return act(EA_SAVE, e->sel);
}

static EditorAction do_step_remove(Editor *e) {
	Macro *m = cur(e);
	if (!m || e->step < 0) return none();
	if (macro_step_remove(m, e->step) != 0) return none();
	select_step(e, e->step); /* o vizinho (ou o anterior, se era o último; nenhum se esvaziou) */
	clamp_step_scroll(e);
	return act(EA_SAVE, e->sel);
}

static EditorAction adjust_frames(Editor *e, int delta) {
	Macro *m = cur(e);
	if (!m || e->step < 0) return none();
	long frames = (long) m->steps[e->step].frames + delta;
	if (frames < 1 || frames > MACRO_MAX_FRAMES) {
		editor_set_message(e, "Dura\xC3\xA7\xC3\xA3o entre 1 e 36000 quadros");
		return none();
	}
	if (macro_step_set_frames(m, e->step, (uint32_t) frames) != 0) return step_limit(e);
	return act(EA_SAVE, e->sel);
}

static EditorAction toggle_chip(Editor *e, int button) {
	Macro *m = cur(e);
	if (!m || e->step < 0 || button < 0 || button >= BTN_COUNT) return none();
	if (macro_step_toggle_key(m, e->step, (uint16_t) input_button_bit((GbaButton) button)) != 0) return none();
	return act(EA_SAVE, e->sel);
}

static void begin_duration(Editor *e) {
	Macro *m = cur(e);
	if (!m || e->step < 0) return;
	e->dur_editing = 1;
	e->dur_buf[0] = '\0';
}

static void duration_digit(Editor *e, int digit) {
	size_t len = strlen(e->dur_buf);
	if (len < DUR_DIGITS) {
		e->dur_buf[len] = (char) ('0' + digit);
		e->dur_buf[len + 1] = '\0';
	}
}

static EditorAction commit_duration(Editor *e) {
	Macro *m = cur(e);
	if (!m || e->step < 0) {
		e->dur_editing = 0;
		return none();
	}
	long frames = e->dur_buf[0] ? atol(e->dur_buf) : 0;
	if (frames < 1 || frames > MACRO_MAX_FRAMES) {
		editor_set_message(e, "Dura\xC3\xA7\xC3\xA3o entre 1 e 36000 quadros");
		return none();
	}
	if (macro_step_set_frames(m, e->step, (uint32_t) frames) != 0) return step_limit(e);
	e->dur_editing = 0;
	return act(EA_SAVE, e->sel);
}

/* ---- abrir, fechar ---- */

void editor_init(Editor *e) {
	memset(e, 0, sizeof(*e));
	e->sel = e->step = -1;
	textedit_clear(&e->name);
}

void editor_open(Editor *e, const char *game_title, MacroList *list, const char *dir_utf8) {
	editor_close(e);
	editor_init(e);
	e->open = 1;
	snprintf(e->title, sizeof(e->title), "%s", game_title ? game_title : "");
	e->list = *list;
	memset(list, 0, sizeof(*list));
	e->has_state = calloc((size_t) (e->list.count > 0 ? e->list.count : 1), sizeof(int));
	for (int i = 0; e->has_state && i < e->list.count; i++) e->has_state[i] = macro_has_state(dir_utf8, e->list.items[i].name);
	select_macro(e, e->list.count > 0 ? 0 : -1);
}

void editor_select(Editor *e, int index) {
	select_macro(e, index);
}

void editor_close(Editor *e) {
	macrolist_free(&e->list);
	free(e->has_state);
	e->has_state = NULL;
	e->open = 0;
	e->sel = e->step = -1;
	e->name_editing = 0;
	e->confirm_delete = 0;
}

/* ---- teclado ---- */

static int is_enter(int sc) {
	return sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
}

static EditorAction key_list(Editor *e, int sc, int mods, int armed) {
	int rows = EDITOR_LIST_ROWS, from = e->sel < 0 ? 0 : e->sel;
	if ((mods & KMOD_CTRL) && sc == SDL_SCANCODE_D) return do_duplicate(e);
	switch (sc) {
	case SDL_SCANCODE_DOWN: select_macro(e, from + 1); break;
	case SDL_SCANCODE_UP: select_macro(e, from - 1); break;
	case SDL_SCANCODE_PAGEDOWN: select_macro(e, from + rows); break;
	case SDL_SCANCODE_PAGEUP: select_macro(e, from - rows); break;
	case SDL_SCANCODE_INSERT: return do_new(e);
	case SDL_SCANCODE_DELETE: return do_delete(e, armed);
	default: break;
	}
	return none();
}

/* Teclas das zonas EZ_STEPS (duração) e EZ_BUTTONS (botões do passo). */
static EditorAction key_steps(Editor *e, int sc, int mods) {
	int big = (mods & KMOD_SHIFT) ? 10 : 1;
	if (sc == SDL_SCANCODE_INSERT) return do_step_add(e);
	if (sc == SDL_SCANCODE_DELETE) return do_step_remove(e);
	if ((mods & KMOD_CTRL) && sc == SDL_SCANCODE_D) return do_step_duplicate(e);
	if ((mods & KMOD_ALT) && (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_DOWN)) return do_step_move(e, sc == SDL_SCANCODE_UP ? -1 : 1);
	int from = e->step < 0 ? 0 : e->step;
	switch (sc) {
	case SDL_SCANCODE_DOWN: select_step(e, from + 1); return none();
	case SDL_SCANCODE_UP: select_step(e, from - 1); return none();
	case SDL_SCANCODE_PAGEDOWN: select_step(e, from + EDITOR_STEP_ROWS); return none();
	case SDL_SCANCODE_PAGEUP: select_step(e, from - EDITOR_STEP_ROWS); return none();
	default: break;
	}
	if (e->zone == EZ_BUTTONS) {
		if (sc == SDL_SCANCODE_LEFT && e->chip > 0) e->chip--;
		else if (sc == SDL_SCANCODE_RIGHT && e->chip < BTN_COUNT - 1) e->chip++;
		else if (is_enter(sc) || sc == SDL_SCANCODE_SPACE) return toggle_chip(e, e->chip);
		return none();
	}
	if (sc == SDL_SCANCODE_LEFT) return adjust_frames(e, -big);
	if (sc == SDL_SCANCODE_RIGHT) return adjust_frames(e, big);
	if (is_enter(sc)) {
		begin_duration(e);
		return none();
	}
	int digit = digit_of(sc);
	if (digit >= 0) {
		if (is_keypad_digit(sc)) { /* teclado numérico: digitar a duração */
			begin_duration(e);
			if (e->dur_editing) duration_digit(e, digit);
			return none();
		}
		return toggle_chip(e, digit == 0 ? 9 : digit - 1); /* fileira de cima: 1..0 = A, B, L, R, Start, Select, Cima... */
	}
	return none();
}

static EditorAction on_key(Editor *e, int sc, int mods, int armed) {
	if (e->name_editing) { /* durante a edição do nome só valem estas teclas */
		if (is_enter(sc)) return commit_rename(e);
		if (sc == SDL_SCANCODE_ESCAPE) e->name_editing = 0;
		else if (sc == SDL_SCANCODE_BACKSPACE) textedit_backspace(&e->name);
		return none();
	}
	if (e->dur_editing) { /* idem para a duração: dígitos, Backspace, Enter e ESC */
		int digit = digit_of(sc);
		if (digit >= 0) duration_digit(e, digit);
		else if (is_enter(sc)) return commit_duration(e);
		else if (sc == SDL_SCANCODE_ESCAPE) e->dur_editing = 0;
		else if (sc == SDL_SCANCODE_BACKSPACE && e->dur_buf[0]) e->dur_buf[strlen(e->dur_buf) - 1] = '\0';
		return none();
	}
	if (sc == SDL_SCANCODE_ESCAPE) return act(EA_CLOSE, 0);
	if (sc == SDL_SCANCODE_TAB) {
		int step = (mods & KMOD_SHIFT) ? EZ_COUNT - 1 : 1;
		e->zone = (EditorZone) (((int) e->zone + step) % EZ_COUNT);
		return none();
	}
	int left = sc == SDL_SCANCODE_LEFT, right = sc == SDL_SCANCODE_RIGHT;
	int big = (mods & KMOD_SHIFT) ? 10 : 1;
	switch (e->zone) {
	case EZ_LIST: return key_list(e, sc, mods, armed);
	case EZ_NAME:
		if (is_enter(sc)) begin_rename(e);
		break;
	case EZ_MODE:
		if (left) return change_mode(e, -1);
		if (right) return change_mode(e, 1);
		break;
	case EZ_REPEAT:
		if (left) return change_repeat(e, -big);
		if (right) return change_repeat(e, big);
		break;
	case EZ_STATE:
		if (is_enter(sc) || sc == SDL_SCANCODE_SPACE) return toggle_state(e);
		break;
	case EZ_STEPS:
	case EZ_BUTTONS: return key_steps(e, sc, mods);
	default: break;
	}
	return none();
}

/* ---- mouse ---- */

/* Cliques na tabela de passos, nos botões de passo e nos controles de cada linha. */
static EditorAction mouse_steps(Editor *e, int x, int y, int clicks, int *handled) {
	*handled = 1;
	Macro *m = cur(e);
	for (int b = 0; b < ESB_COUNT; b++) {
		if (!rect_contains(layout_editor_step_btn(b), x, y)) continue;
		e->zone = EZ_STEPS;
		switch (b) {
		case ESB_ADD: return do_step_add(e);
		case ESB_DUP: return do_step_duplicate(e);
		case ESB_UP: return do_step_move(e, -1);
		case ESB_DOWN: return do_step_move(e, 1);
		default: return do_step_remove(e);
		}
	}
	for (int i = 0; m && i < EDITOR_STEP_ROWS; i++) {
		int index = e->step_scroll + i;
		if (index >= m->count || !rect_contains(layout_editor_step_row(i), x, y)) continue;
		select_step(e, index);
		for (int b = 0; b < BTN_COUNT; b++) {
			if (rect_contains(layout_editor_chip(i, b), x, y)) {
				e->zone = EZ_BUTTONS;
				e->chip = b;
				return toggle_chip(e, b);
			}
		}
		e->zone = EZ_STEPS;
		if (rect_contains(layout_editor_dur_minus(i), x, y)) return adjust_frames(e, -1);
		if (rect_contains(layout_editor_dur_plus(i), x, y)) return adjust_frames(e, 1);
		if (rect_contains(layout_editor_dur_value(i), x, y) && clicks >= 2) begin_duration(e);
		return none();
	}
	*handled = 0;
	return none();
}

static EditorAction on_mouse_down(Editor *e, int x, int y, int clicks, int armed) {
	if (e->name_editing && !rect_contains(layout_editor_name(), x, y)) e->name_editing = 0; /* clicar fora cancela */
	e->dur_editing = 0; /* qualquer clique cancela a digitação da duração (o duplo clique no valor a recomeça) */
	if (rect_contains(layout_editor_close(), x, y)) return act(EA_CLOSE, 0);

	for (int b = 0; b < EB_COUNT; b++) {
		if (!rect_contains(layout_editor_list_btn(b), x, y)) continue;
		e->zone = EZ_LIST;
		if (b == EB_NEW) return do_new(e);
		if (b == EB_DUP) return do_duplicate(e);
		return do_delete(e, armed);
	}
	for (int i = 0; i < EDITOR_LIST_ROWS; i++) {
		if (!rect_contains(layout_editor_list_row(i), x, y)) continue;
		e->zone = EZ_LIST;
		if (e->list_scroll + i < e->list.count) select_macro(e, e->list_scroll + i);
		return none();
	}
	if (rect_contains(layout_editor_name(), x, y)) {
		e->zone = EZ_NAME;
		if (!e->name_editing) begin_rename(e);
		return none();
	}
	for (int i = 0; i < 3; i++) {
		if (rect_contains(layout_editor_mode(i), x, y)) {
			e->zone = EZ_MODE;
			return set_mode(e, (MacroMode) i);
		}
	}
	if (rect_contains(layout_editor_repeat_minus(), x, y)) {
		e->zone = EZ_REPEAT;
		return change_repeat(e, -1);
	}
	if (rect_contains(layout_editor_repeat_plus(), x, y)) {
		e->zone = EZ_REPEAT;
		return change_repeat(e, 1);
	}
	if (rect_contains(layout_editor_from_state(), x, y)) {
		e->zone = EZ_STATE;
		return toggle_state(e);
	}
	int handled;
	EditorAction a = mouse_steps(e, x, y, clicks, &handled);
	return handled ? a : none();
}

static void on_wheel(Editor *e, int x, int y, int dy) {
	Rect list = {layout_editor_list_row(0).x, layout_editor_list_row(0).y, layout_editor_list_row(0).w,
	             layout_editor_list_row(EDITOR_LIST_ROWS - 1).y + layout_editor_list_row(EDITOR_LIST_ROWS - 1).h - layout_editor_list_row(0).y};
	if (rect_contains(list, x, y)) {
		e->list_scroll -= dy * WHEEL_LINES;
		clamp_list_scroll(e);
	}
	Rect first = layout_editor_step_row(0), last = layout_editor_step_row(EDITOR_STEP_ROWS - 1);
	if (rect_contains((Rect) {first.x, first.y, first.w, last.y + last.h - first.y}, x, y)) {
		e->step_scroll -= dy * WHEEL_LINES;
		clamp_step_scroll(e);
	}
}

/* ---- entrada principal ---- */

EditorAction editor_handle(Editor *e, EditorEvent ev) {
	if (!e->open) return none();
	/* Qualquer ação que não seja o Apagar de novo cancela a confirmação (e o aviso dela). */
	int armed = e->confirm_delete;
	if (armed && (ev.type == EEV_KEY || ev.type == EEV_TEXT || ev.type == EEV_MOUSE_DOWN)) {
		e->confirm_delete = 0;
		e->message[0] = '\0';
	}
	switch (ev.type) {
	case EEV_KEY: return on_key(e, ev.key, ev.mods, armed);
	case EEV_TEXT:
		if (e->name_editing && ev.text) textedit_insert(&e->name, ev.text);
		return none();
	case EEV_MOUSE_DOWN: return on_mouse_down(e, ev.x, ev.y, ev.clicks, armed);
	case EEV_WHEEL: on_wheel(e, ev.x, ev.y, ev.dy); return none();
	case EEV_MOUSE_MOVE:
	case EEV_MOUSE_UP: return none();
	}
	return none();
}
