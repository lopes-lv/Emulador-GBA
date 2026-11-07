#include "menu.h"

#include <stdio.h>
#include <string.h>

#include "hotkeys.h"

void menu_init(Menu *m, Bindings *bind) {
	memset(m, 0, sizeof(*m));
	m->bind = bind;
}

void menu_open(Menu *m) {
	m->open = 1;
	m->key_row = 0;
	m->capturing = 0;
	m->message[0] = '\0';
}

void menu_set_message(Menu *m, const char *msg_utf8) {
	snprintf(m->message, sizeof(m->message), "%s", msg_utf8); /* trunca sem estourar */
}

static MenuAction close_menu(Menu *m) {
	m->open = 0;
	m->capturing = 0;
	m->message[0] = '\0';
	return ACT_CLOSE;
}

static MenuAction handle_capture(Menu *m, MenuEvent ev) {
	switch (ev.type) {
	case MEV_KEY_PRESSED:
		if (hotkey_is_reserved((SDL_Scancode) ev.code)) {
			menu_set_message(m, "Tecla reservada para atalho");
			return ACT_NONE; /* a captura continua */
		}
		bindings_assign_key(m->bind, (GbaButton) m->key_row, (SDL_Scancode) ev.code);
		m->capturing = 0;
		m->message[0] = '\0';
		return ACT_CONFIG_CHANGED;
	case MEV_PAD_PRESSED:
		bindings_assign_pad(m->bind, (GbaButton) m->key_row, (SDL_GameControllerButton) ev.code);
		m->capturing = 0;
		m->message[0] = '\0';
		return ACT_CONFIG_CHANGED;
	case MEV_ESC:
		m->capturing = 0; /* só cancela a captura */
		m->message[0] = '\0';
		return ACT_NONE;
	default:
		return ACT_NONE;
	}
}

MenuAction menu_handle(Menu *m, MenuEvent ev) {
	if (!m->open) return ACT_NONE;
	if (m->capturing) return handle_capture(m, ev);

	switch (ev.type) {
	case MEV_UP:
		if (m->key_row > 0) m->key_row--;
		return ACT_NONE;
	case MEV_DOWN:
		if (m->key_row < KEY_ROW_BACK) m->key_row++;
		return ACT_NONE;
	case MEV_ENTER:
		if (m->key_row < BTN_COUNT) {
			m->capturing = 1;
			m->message[0] = '\0';
			return ACT_NONE;
		}
		if (m->key_row == KEY_ROW_RESET) {
			bindings_defaults(m->bind);
			return ACT_CONFIG_CHANGED;
		}
		return close_menu(m); /* KEY_ROW_BACK */
	case MEV_ESC:
		return close_menu(m);
	default:
		return ACT_NONE;
	}
}
