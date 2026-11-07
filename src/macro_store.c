#include "macro_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "bindings.h"
#include "input.h"

#define LINE_MAX_LEN 1024

/* ---- texto ---- */

char *macro_to_text(const Macro *m) {
	static const char *const MODES[] = {"once", "count", "loop"};
	size_t cap = 512 + (size_t) m->count * 130;
	char *out = malloc(cap);
	if (!out) return NULL;
	int n = snprintf(out, cap, "nome=%s\nmodo=%s\nrepetir=%d\ndo_estado=%d\n---\n", m->name, MODES[m->mode], m->repeat,
	                 m->from_state);
	for (int i = 0; i < m->count; i++) {
		uint16_t keys = m->steps[i].keys;
		if (keys == 0) {
			n += snprintf(out + n, cap - (size_t) n, "-");
		} else {
			int first = 1;
			for (int b = 0; b < BTN_COUNT; b++) {
				if (!(keys & input_button_bit((GbaButton) b))) continue;
				n += snprintf(out + n, cap - (size_t) n, "%s%s", first ? "" : "+", bindings_button_label((GbaButton) b));
				first = 0;
			}
		}
		n += snprintf(out + n, cap - (size_t) n, " %u\n", (unsigned) m->steps[i].frames);
	}
	return out;
}

static char *trim(char *s) {
	while (*s == ' ' || *s == '\t') s++;
	char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
	return s;
}

/* Inteiro estrito dentro de [lo, hi]. 1 em sucesso. */
static int parse_int(const char *s, long lo, long hi, long *out) {
	if (!*s) return 0;
	char *end = NULL;
	long v = strtol(s, &end, 10);
	if (*end != '\0' || v < lo || v > hi) return 0;
	*out = v;
	return 1;
}

/* Botões ("A+B", "cima", "-") -> máscara. 1 em sucesso. */
static int parse_buttons(char *token, uint16_t *keys, char *bad, size_t bad_size) {
	*keys = 0;
	if (!strcmp(token, "-")) return 1;
	char *save = NULL;
	for (char *name = strtok_s(token, "+", &save); name; name = strtok_s(NULL, "+", &save)) {
		int found = -1;
		for (int b = 0; b < BTN_COUNT && found < 0; b++) {
			if (_stricmp(name, bindings_button_label((GbaButton) b)) == 0 || _stricmp(name, bindings_button_id((GbaButton) b)) == 0) found = b;
		}
		if (found < 0) {
			snprintf(bad, bad_size, "%s", name);
			return 0;
		}
		*keys |= (uint16_t) input_button_bit((GbaButton) found);
	}
	return *keys != 0;
}

static int fail(Macro *m, Macro *tmp, char *err, size_t err_size, int line, const char *fmt, const char *arg) {
	macro_free(tmp);
	macro_init(m, "");
	if (err && err_size) {
		int n = line > 0 ? snprintf(err, err_size, "linha %d: ", line) : 0;
		if (n < 0) n = 0;
		if ((size_t) n < err_size) snprintf(err + n, err_size - (size_t) n, fmt, arg ? arg : "");
	}
	return -1;
}

int macro_from_text(Macro *m, const char *text, char *err, size_t err_size) {
	Macro tmp;
	macro_init(&tmp, "");
	int in_header = 1, line_no = 0;
	char name[MACRO_NAME_BYTES * 2] = "";
	const char *p = text ? text : "";
	while (*p) {
		line_no++;
		const char *eol = strchr(p, '\n');
		size_t len = eol ? (size_t) (eol - p) : strlen(p);
		char line[LINE_MAX_LEN];
		if (len >= sizeof(line)) return fail(m, &tmp, err, err_size, line_no, "linha longa demais", NULL);
		memcpy(line, p, len);
		line[len] = '\0';
		p = eol ? eol + 1 : p + len;

		char *s = trim(line);
		if (*s == '\0' || *s == '#') continue;
		if (in_header && !strncmp(s, "nome=", 5)) { /* o nome é inteiro: '#' faz parte dele */
			snprintf(name, sizeof(name), "%s", trim(s + 5));
			continue;
		}
		char *hash = strchr(s, '#');
		if (hash) {
			*hash = '\0';
			s = trim(s);
		}
		if (in_header) {
			if (!strcmp(s, "---")) {
				in_header = 0;
				continue;
			}
			char *eq = strchr(s, '=');
			if (!eq) return fail(m, &tmp, err, err_size, line_no, "esperava chave=valor ou a linha '---'", NULL);
			*eq = '\0';
			char *key = trim(s), *value = trim(eq + 1);
			long v;
			if (!strcmp(key, "modo")) {
				if (!_stricmp(value, "once")) tmp.mode = MACRO_ONCE;
				else if (!_stricmp(value, "count")) tmp.mode = MACRO_COUNT;
				else if (!_stricmp(value, "loop")) tmp.mode = MACRO_LOOP;
				else return fail(m, &tmp, err, err_size, line_no, "modo deve ser once, count ou loop (veio '%s')", value);
			} else if (!strcmp(key, "repetir")) {
				if (!parse_int(value, 1, MACRO_MAX_REPEAT, &v)) return fail(m, &tmp, err, err_size, line_no, "repetir deve ficar entre 1 e 999", NULL);
				tmp.repeat = (int) v;
			} else if (!strcmp(key, "do_estado")) {
				if (!parse_int(value, 0, 1, &v)) return fail(m, &tmp, err, err_size, line_no, "do_estado deve ser 0 ou 1", NULL);
				tmp.from_state = (int) v;
			} /* chaves desconhecidas são ignoradas */
			continue;
		}

		char *buttons = s, *rest = s;
		while (*rest && *rest != ' ' && *rest != '\t') rest++;
		if (*rest) *rest++ = '\0';
		char *frames = trim(rest);
		if (*frames == '\0') return fail(m, &tmp, err, err_size, line_no, "falta a duração (em quadros) depois dos botões", NULL);
		if (strchr(frames, ' ') || strchr(frames, '\t')) return fail(m, &tmp, err, err_size, line_no, "coisa a mais depois da duração", NULL);
		long dur;
		if (!parse_int(frames, 1, MACRO_MAX_FRAMES, &dur)) return fail(m, &tmp, err, err_size, line_no, "duração inválida '%s' (use 1 a 36000 quadros)", frames);
		uint16_t keys;
		char bad[64] = "";
		if (!parse_buttons(buttons, &keys, bad, sizeof(bad)) && bad[0]) return fail(m, &tmp, err, err_size, line_no, "botão desconhecido '%s'", bad);
		if (keys == 0 && strcmp(buttons, "-") != 0 && strcmp(buttons, "") != 0 && bad[0] == '\0') {
			/* nome vazio entre '+' (ex.: "A+ 5") */
			return fail(m, &tmp, err, err_size, line_no, "botões inválidos", NULL);
		}
		if (macro_step_insert(&tmp, tmp.count, keys, (uint32_t) dur) != 0) { /* cada linha é um passo: não junta iguais */
			return fail(m, &tmp, err, err_size, line_no, "passos ou quadros demais (limite de 4096 passos e 36000 quadros)", NULL);
		}
	}
	if (in_header) return fail(m, &tmp, err, err_size, 0, "falta a linha '---' depois do cabeçalho", NULL);
	if (name[0] == '\0') return fail(m, &tmp, err, err_size, 0, "falta o nome (nome=...)", NULL);

	Macro named;
	macro_init(&named, name);
	named.mode = tmp.mode;
	named.repeat = tmp.repeat;
	named.from_state = tmp.from_state;
	named.steps = tmp.steps;
	named.count = tmp.count;
	named.total_frames = tmp.total_frames;
	*m = named;
	return 0;
}

/* ---- nomes de arquivo ---- */

static int is_reserved(const char *name) {
	static const char *const FIXED[] = {"CON", "PRN", "AUX", "NUL"};
	char base[16];
	size_t n = 0;
	while (name[n] && name[n] != '.' && n < sizeof(base) - 1) base[n] = name[n], n++;
	base[n] = '\0';
	if (name[n] && name[n] != '.') return 0; /* base longa demais para ser reservada */
	for (size_t i = 0; i < sizeof(FIXED) / sizeof(FIXED[0]); i++) {
		if (!_stricmp(base, FIXED[i])) return 1;
	}
	if (n == 4 && base[3] >= '1' && base[3] <= '9' && (!_strnicmp(base, "COM", 3) || !_strnicmp(base, "LPT", 3))) return 1;
	return 0;
}

void macro_file_name(const char *name, char *out, size_t out_size) {
	if (!out || out_size == 0) return;
	char tmp[MACRO_NAME_BYTES + 2];
	size_t len = 0;
	int chars = 0;
	for (const unsigned char *p = (const unsigned char *) (name ? name : ""); *p && len < MACRO_NAME_BYTES - 1; p++) {
		if ((*p & 0xC0) != 0x80) {
			if (chars == MACRO_NAME_MAX) break;
			chars++;
		}
		unsigned char c = *p;
		if (c < 0x20 || strchr("<>:\"/\\|?*", c)) c = '_';
		tmp[len++] = (char) c;
	}
	while (len > 0 && (tmp[len - 1] == '.' || tmp[len - 1] == ' ')) len--;
	tmp[len] = '\0';
	char full[MACRO_NAME_BYTES + 4];
	if (len == 0) snprintf(full, sizeof(full), "macro");
	else snprintf(full, sizeof(full), "%s%s", is_reserved(tmp) ? "_" : "", tmp);

	size_t n = strlen(full);
	if (n > out_size - 1) {
		n = out_size - 1;
		while (n > 0 && ((unsigned char) full[n] & 0xC0) == 0x80) n--; /* não corta no meio de um caractere */
	}
	memcpy(out, full, n);
	out[n] = '\0';
}

/* ---- disco ---- */

#define PATH_W 2048

static int to_wide(const char *utf8, wchar_t *out, int n) {
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, n) == 0) return 0;
	for (wchar_t *p = out; *p; p++) {
		if (*p == L'/') *p = L'\\';
	}
	return 1;
}

static int path_join(char *out, size_t n, const char *dir, const char *file, const char *ext) {
	int len = snprintf(out, n, "%s/%s%s", dir, file, ext);
	return (len < 0 || (size_t) len >= n) ? -1 : 0;
}

void macrolist_free(MacroList *l) {
	for (int i = 0; i < l->count; i++) macro_free(&l->items[i]);
	free(l->items);
	l->items = NULL;
	l->count = 0;
	l->skipped = 0;
}

int macro_dir_for_rom(const char *base_dir, const char *rom_utf8, char *out, size_t out_size) {
	const char *name = rom_utf8;
	for (const char *p = rom_utf8; *p; p++) {
		if (*p == '/' || *p == '\\') name = p + 1;
	}
	char stem[512];
	snprintf(stem, sizeof(stem), "%s", name);
	char *dot = strrchr(stem, '.');
	if (dot && dot != stem) *dot = '\0';
	char file[MACRO_NAME_BYTES + 4];
	macro_file_name(stem, file, sizeof(file));
	return path_join(out, out_size, base_dir, file, "");
}

int macro_ensure_dir(const char *dir_utf8) {
	wchar_t w[PATH_W];
	if (!to_wide(dir_utf8, w, PATH_W)) return -1;
	for (wchar_t *p = w + 1; *p; p++) {
		if (*p == L'\\') {
			*p = L'\0';
			CreateDirectoryW(w, NULL); /* já existir não é erro */
			*p = L'\\';
		}
	}
	CreateDirectoryW(w, NULL);
	DWORD attrs = GetFileAttributesW(w);
	return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) ? 0 : -1;
}

/* Conteúdo inteiro do arquivo (até 2 MB) como texto terminado em zero; NULL se não abrir ou for grande demais. */
static char *read_file_w(const wchar_t *wpath) {
	FILE *f = _wfopen(wpath, L"rb");
	if (!f) return NULL;
	char *buf = NULL;
	if (fseek(f, 0, SEEK_END) == 0) {
		long size = ftell(f);
		if (size >= 0 && size <= 2 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
			buf = malloc((size_t) size + 1);
			if (buf) {
				size_t got = fread(buf, 1, (size_t) size, f);
				buf[got] = '\0';
			}
		}
	}
	fclose(f);
	return buf;
}

static int by_name(const void *a, const void *b) {
	return _stricmp(((const Macro *) a)->name, ((const Macro *) b)->name);
}

int macro_list_dir(const char *dir_utf8, MacroList *out) {
	memset(out, 0, sizeof(*out));
	wchar_t dir[PATH_W], pattern[PATH_W];
	if (!to_wide(dir_utf8, dir, PATH_W) || swprintf(pattern, PATH_W, L"%ls\\*.txt", dir) < 0) return -1;
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) {
		DWORD e = GetLastError();
		return (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) ? 0 : -1;
	}
	do {
		size_t len = wcslen(fd.cFileName);
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || len <= 4 || _wcsicmp(fd.cFileName + len - 4, L".txt") != 0) continue;
		wchar_t full[PATH_W];
		if (swprintf(full, PATH_W, L"%ls\\%ls", dir, fd.cFileName) < 0) continue;
		char *text = read_file_w(full);
		Macro m;
		char err[200];
		if (!text || macro_from_text(&m, text, err, sizeof(err)) != 0) {
			out->skipped++;
			free(text);
			continue;
		}
		free(text);
		Macro *grown = realloc(out->items, (size_t) (out->count + 1) * sizeof(Macro));
		if (!grown) {
			macro_free(&m);
			FindClose(h);
			macrolist_free(out);
			return -1;
		}
		out->items = grown;
		out->items[out->count++] = m;
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	if (out->count > 1) qsort(out->items, (size_t) out->count, sizeof(Macro), by_name);
	return 0;
}

int macro_save_file(const Macro *m, const char *dir_utf8) {
	if (macro_ensure_dir(dir_utf8) != 0) return -1;
	char file[MACRO_NAME_BYTES + 4], dest[2048], tmp[2056];
	macro_file_name(m->name, file, sizeof(file));
	if (path_join(dest, sizeof(dest), dir_utf8, file, ".txt") != 0 || path_join(tmp, sizeof(tmp), dir_utf8, file, ".txt.tmp") != 0) return -1;
	char *text = macro_to_text(m);
	if (!text) return -1;
	wchar_t wdest[PATH_W], wtmp[PATH_W];
	int ok = to_wide(dest, wdest, PATH_W) && to_wide(tmp, wtmp, PATH_W);
	if (ok) {
		FILE *f = _wfopen(wtmp, L"wb");
		ok = f != NULL;
		if (f) {
			ok = fwrite(text, 1, strlen(text), f) == strlen(text);
			ok = (fclose(f) == 0) && ok;
		}
		if (ok) ok = MoveFileExW(wtmp, wdest, MOVEFILE_REPLACE_EXISTING) != 0;
		if (!ok) DeleteFileW(wtmp);
	}
	free(text);
	return ok ? 0 : -1;
}

static int delete_one(const char *dir, const char *file, const char *ext) {
	char path[2048];
	wchar_t w[PATH_W];
	if (path_join(path, sizeof(path), dir, file, ext) != 0 || !to_wide(path, w, PATH_W)) return -1;
	if (DeleteFileW(w)) return 0;
	DWORD e = GetLastError();
	return (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) ? 0 : -1;
}

int macro_delete_file(const char *dir_utf8, const char *name) {
	char file[MACRO_NAME_BYTES + 4];
	macro_file_name(name, file, sizeof(file));
	int a = delete_one(dir_utf8, file, ".txt");
	int b = delete_one(dir_utf8, file, ".state");
	return (a == 0 && b == 0) ? 0 : -1;
}

/* Existe um arquivo (não pasta) neste caminho UTF-8? */
static int file_exists(const char *utf8) {
	wchar_t w[PATH_W];
	if (!to_wide(utf8, w, PATH_W)) return 0;
	DWORD attrs = GetFileAttributesW(w);
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int macro_has_state(const char *dir_utf8, const char *name) {
	char file[MACRO_NAME_BYTES + 4], path[2048];
	macro_file_name(name, file, sizeof(file));
	return path_join(path, sizeof(path), dir_utf8, file, ".state") == 0 && file_exists(path);
}

int macro_rename_files(const Macro *renamed, const char *dir_utf8, const char *old_name) {
	char f_new[MACRO_NAME_BYTES + 4], f_old[MACRO_NAME_BYTES + 4];
	macro_file_name(renamed->name, f_new, sizeof(f_new));
	macro_file_name(old_name, f_old, sizeof(f_old));
	if (_stricmp(f_new, f_old) == 0) return macro_save_file(renamed, dir_utf8) == 0 ? 0 : -1;

	char new_txt[2048], old_state[2048], new_state[2048];
	if (path_join(new_txt, sizeof(new_txt), dir_utf8, f_new, ".txt") != 0 ||
	    path_join(old_state, sizeof(old_state), dir_utf8, f_old, ".state") != 0 ||
	    path_join(new_state, sizeof(new_state), dir_utf8, f_new, ".state") != 0) {
		return -1;
	}
	if (file_exists(new_txt)) return -2;
	if (macro_save_file(renamed, dir_utf8) != 0) return -1;
	if (file_exists(old_state)) {
		wchar_t wo[PATH_W], wn[PATH_W];
		if (!to_wide(old_state, wo, PATH_W) || !to_wide(new_state, wn, PATH_W) || !MoveFileExW(wo, wn, MOVEFILE_REPLACE_EXISTING)) {
			delete_one(dir_utf8, f_new, ".txt"); /* desfaz: o nome antigo continua valendo */
			return -1;
		}
	}
	return delete_one(dir_utf8, f_old, ".txt") == 0 ? 0 : -1;
}

int macro_copy_state(const char *dir_utf8, const char *from_name, const char *to_name) {
	if (!macro_has_state(dir_utf8, from_name)) return 0;
	char f_from[MACRO_NAME_BYTES + 4], f_to[MACRO_NAME_BYTES + 4], from[2048], to[2048];
	macro_file_name(from_name, f_from, sizeof(f_from));
	macro_file_name(to_name, f_to, sizeof(f_to));
	wchar_t wf[PATH_W], wt[PATH_W];
	if (path_join(from, sizeof(from), dir_utf8, f_from, ".state") != 0 || path_join(to, sizeof(to), dir_utf8, f_to, ".state") != 0 ||
	    !to_wide(from, wf, PATH_W) || !to_wide(to, wt, PATH_W)) {
		return -1;
	}
	return CopyFileW(wf, wt, FALSE) ? 0 : -1;
}

void macro_state_path(const char *dir_utf8, const char *name, char *out, size_t out_size) {
	char file[MACRO_NAME_BYTES + 4];
	macro_file_name(name, file, sizeof(file));
	if (path_join(out, out_size, dir_utf8, file, ".state") != 0 && out_size > 0) out[0] = '\0';
}

int macro_unique_name(const MacroList *l, const char *base, char *out, size_t out_size) {
	for (int i = 1; i < 10000; i++) {
		char candidate[MACRO_NAME_BYTES + 16], file[MACRO_NAME_BYTES + 4];
		snprintf(candidate, sizeof(candidate), "%s %d", base, i);
		macro_file_name(candidate, file, sizeof(file));
		int used = 0;
		for (int j = 0; j < l->count && !used; j++) {
			char other[MACRO_NAME_BYTES + 4];
			macro_file_name(l->items[j].name, other, sizeof(other));
			used = _stricmp(file, other) == 0;
		}
		if (used) continue;
		int len = snprintf(out, out_size, "%s", candidate);
		return (len < 0 || (size_t) len >= out_size) ? -1 : 0;
	}
	return -1;
}
