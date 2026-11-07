#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define LINE_MAX_LEN 4096

static FILE *open_utf8(const char *path_utf8, const wchar_t *mode) {
	wchar_t wpath[2048];
	if (MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1, wpath, 2048) == 0) return NULL;
	return _wfopen(wpath, mode);
}

void config_defaults(Config *c) {
	c->rom_dir[0] = '\0';
	bindings_defaults(&c->bind);
	settings_defaults(&c->settings);
}

static int button_from_id(const char *id) {
	for (int i = 0; i < BTN_COUNT; i++) {
		if (_stricmp(id, bindings_button_id((GbaButton) i)) == 0) return i;
	}
	return -1;
}

/* Inteiro estrito: sem espaços, sem sobras, dentro de [lo, hi]. 1 em sucesso. */
static int parse_int(const char *s, long lo, long hi, int *out) {
	if (!(*s == '-' || (*s >= '0' && *s <= '9'))) return 0;
	char *end = NULL;
	long n = strtol(s, &end, 10);
	if (end == s || *end != '\0' || n < lo || n > hi) return 0;
	*out = (int) n;
	return 1;
}

static void apply_line(Config *c, char *key, const char *value) {
	int n;
	if (!strcmp(key, "volume")) {
		if (parse_int(value, 0, 100, &n)) c->settings.volume = n;
		return;
	} else if (!strcmp(key, "mudo")) {
		if (parse_int(value, 0, 1, &n)) c->settings.muted = n;
		return;
	} else if (!strcmp(key, "avanco")) {
		if (parse_int(value, 0, FF_SPEED_COUNT - 1, &n)) c->settings.ff_index = n;
		return;
	} else if (!strcmp(key, "escala")) {
		if (!strcmp(value, "preencher")) c->settings.scale = SCALE_FILL;
		else if (!strcmp(value, "inteira")) c->settings.scale = SCALE_INTEGER;
		return;
	} else if (!strcmp(key, "tela_cheia")) {
		if (parse_int(value, 0, 1, &n)) c->settings.start_fullscreen = n;
		return;
	} else if (!strcmp(key, "avanco_sempre")) {
		if (parse_int(value, 0, 1, &n)) c->settings.ff_always = n;
		return;
	} else if (!strcmp(key, "turbo")) {
		if (parse_int(value, 0, 1, &n)) c->settings.turbo = n;
		return;
	} else if (!strcmp(key, "turbo_taxa")) {
		if (parse_int(value, 0, TURBO_RATE_COUNT - 1, &n)) c->settings.turbo_rate = n;
		return;
	} else if (!strcmp(key, "turbo_botoes")) {
		if (parse_int(value, 0, (1 << BTN_COUNT) - 1, &n)) c->settings.turbo_mask = (unsigned) n;
		return;
	} else if (!strcmp(key, "analogico")) {
		if (parse_int(value, 0, 1, &n)) c->settings.stick_on = n;
		return;
	} else if (!strcmp(key, "zona_morta")) {
		if (parse_int(value, STICK_DEADZONE_MIN, STICK_DEADZONE_MAX, &n)) c->settings.stick_deadzone = n;
		return;
	}
	if (!strcmp(key, "pasta_roms")) {
		snprintf(c->rom_dir, sizeof(c->rom_dir), "%s", value); /* trunca sem estourar */
	} else if (!strncmp(key, "tecla.", 6)) {
		int btn = button_from_id(key + 6);
		if (btn < 0) return;
		if (*value == '\0') {
			bindings_assign_key(&c->bind, (GbaButton) btn, SDL_SCANCODE_UNKNOWN);
		} else {
			SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
			if (*value == '#') { /* tecla sem nome no SDL, gravada pelo número */
				char *end = NULL;
				long n = strtol(value + 1, &end, 10);
				if (end != value + 1 && *end == '\0' && n > 0 && n < SDL_NUM_SCANCODES) sc = (SDL_Scancode) n;
			} else {
				sc = SDL_GetScancodeFromName(value);
			}
			if (sc != SDL_SCANCODE_UNKNOWN) bindings_assign_key(&c->bind, (GbaButton) btn, sc);
		}
	} else if (!strncmp(key, "pad.", 4)) {
		int btn = button_from_id(key + 4);
		if (btn < 0) return;
		if (*value == '\0') {
			bindings_assign_pad(&c->bind, (GbaButton) btn, SDL_CONTROLLER_BUTTON_INVALID);
		} else {
			SDL_GameControllerButton pb = SDL_GameControllerGetButtonFromString(value);
			if (pb != SDL_CONTROLLER_BUTTON_INVALID) bindings_assign_pad(&c->bind, (GbaButton) btn, pb);
		}
	}
}

int config_load(Config *c, const char *path_utf8) {
	config_defaults(c);
	FILE *f = open_utf8(path_utf8, L"rb");
	if (!f) return -1;

	static char line[LINE_MAX_LEN];
	int skipping = 0; /* resto de uma linha longa demais */
	while (fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		int complete = len > 0 && line[len - 1] == '\n';
		if (skipping) {
			skipping = !complete;
			continue;
		}
		if (!complete && len == sizeof(line) - 1) skipping = 1; /* linha maior que o buffer: usa o começo */
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

		char *eq = strchr(line, '=');
		if (!eq || eq == line) continue; /* sem '=' ou sem chave */
		*eq = '\0';
		apply_line(c, line, eq + 1);
	}
	fclose(f);
	return 0;
}

int config_save(const Config *c, const char *path_utf8) {
	FILE *f = open_utf8(path_utf8, L"wb");
	if (!f) return -1;
	int ok = fprintf(f, "pasta_roms=%s\n", c->rom_dir) >= 0 &&
	         fprintf(f, "volume=%d\nmudo=%d\navanco=%d\nescala=%s\ntela_cheia=%d\n", c->settings.volume,
	                 c->settings.muted, c->settings.ff_index,
	                 c->settings.scale == SCALE_INTEGER ? "inteira" : "preencher", c->settings.start_fullscreen) >= 0 &&
	         fprintf(f, "avanco_sempre=%d\nturbo=%d\nturbo_taxa=%d\nturbo_botoes=%u\n", c->settings.ff_always, c->settings.turbo,
	                 c->settings.turbo_rate, c->settings.turbo_mask) >= 0 &&
	         fprintf(f, "analogico=%d\nzona_morta=%d\n", c->settings.stick_on, c->settings.stick_deadzone) >= 0;
	for (int i = 0; i < BTN_COUNT && ok; i++) {
		const char *id = bindings_button_id((GbaButton) i);
		char key[32] = "";
		if (c->bind.key[i] != SDL_SCANCODE_UNKNOWN) {
			const char *name = SDL_GetScancodeName(c->bind.key[i]);
			if (name && *name) {
				snprintf(key, sizeof(key), "%s", name);
			} else {
				snprintf(key, sizeof(key), "#%d", (int) c->bind.key[i]); /* o SDL não nomeia as teclas INTERNATIONAL e LANG */
			}
		}
		const char *pad = c->bind.pad[i] == SDL_CONTROLLER_BUTTON_INVALID
		                      ? "" : SDL_GameControllerGetStringForButton(c->bind.pad[i]);
		ok = fprintf(f, "tecla.%s=%s\n", id, key) >= 0 && fprintf(f, "pad.%s=%s\n", id, pad ? pad : "") >= 0;
	}
	if (fclose(f) != 0) ok = 0;
	return ok ? 0 : -1;
}

int config_default_path(char *out, size_t out_size) {
	const wchar_t *appdata = _wgetenv(L"APPDATA");
	if (!appdata || !*appdata) return -1;

	wchar_t wdir[2048];
	if (swprintf(wdir, 2048, L"%ls\\gba-frontend", appdata) < 0) return -1;
	CreateDirectoryW(wdir, NULL); /* já existir não é erro */

	char dir_utf8[2048];
	if (WideCharToMultiByte(CP_UTF8, 0, wdir, -1, dir_utf8, sizeof(dir_utf8), NULL, NULL) == 0) return -1;
	int n = snprintf(out, out_size, "%s\\config.ini", dir_utf8);
	return (n < 0 || (size_t) n >= out_size) ? -1 : 0;
}
