#include "romscan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MAX_DEPTH 3
#define WIDE_MAX 4096

typedef struct {
	RomEntry *items;
	int count;
	int capacity;
} Acc;

/* UTF-8 -> caminho absoluto largo com prefixo \\?\ (funciona além de 260 caracteres). */
static int to_long_wide(const char *utf8, wchar_t *out, size_t out_count) {
	wchar_t tmp[WIDE_MAX], full[WIDE_MAX];
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, tmp, WIDE_MAX) == 0) return -1;
	for (wchar_t *c = tmp; *c; c++) {
		if (*c == L'/') *c = L'\\';
	}
	DWORD n = GetFullPathNameW(tmp, WIDE_MAX, full, NULL);
	if (n == 0 || n >= WIDE_MAX) return -1;
	if (swprintf(out, out_count, L"\\\\?\\%ls", full) < 0) return -1;
	return 0;
}

static int add_entry(Acc *acc, const char *path, const char *name, size_t name_len) {
	if (acc->count == acc->capacity) {
		int cap = acc->capacity ? acc->capacity * 2 : 64;
		RomEntry *grown = realloc(acc->items, (size_t) cap * sizeof(RomEntry));
		if (!grown) return -1;
		acc->items = grown;
		acc->capacity = cap;
	}
	RomEntry *e = &acc->items[acc->count++];
	strcpy(e->path, path);
	memcpy(e->name, name, name_len);
	e->name[name_len] = '\0';
	return 0;
}

int romscan_join_path(const char *dir, const char *name, char *out, size_t out_size) {
	size_t dir_len = strlen(dir);
	int needs_sep = dir_len > 0 && dir[dir_len - 1] != '/';
	int n = snprintf(out, out_size, "%s%s%s", dir, needs_sep ? "/" : "", name);
	return (n < 0 || (size_t) n >= out_size) ? -1 : 0;
}

/* Devolve 0 se a pasta pôde ser listada, -1 se não. */
static int scan_dir(const char *dir, int depth, Acc *acc) {
	wchar_t pattern[WIDE_MAX];
	if (to_long_wide(dir, pattern, WIDE_MAX - 3) != 0) return -1;
	wcscat(pattern, L"\\*");

	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return -1;

	do {
		if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;

		char name[1024];
		if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof(name), NULL, NULL) == 0) continue;
		char path[1024];
		if (romscan_join_path(dir, name, path, sizeof(path)) != 0) continue; /* >= 1024 bytes: ignora */

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (depth < MAX_DEPTH) scan_dir(path, depth + 1, acc);
			continue;
		}
		const char *dot = strrchr(name, '.');
		if (!dot || dot == name || _stricmp(dot, ".gba") != 0) continue;
		size_t stem = (size_t) (dot - name);
		if (stem >= 256) continue;
		if (add_entry(acc, path, name, stem) != 0) break;
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return 0;
}

static int compare_entries(const void *a, const void *b) {
	const RomEntry *x = a, *y = b;
	int c = _stricmp(x->name, y->name);
	return c ? c : strcmp(x->path, y->path);
}

int romscan_normalize_root(const char *dir_utf8, char *out, size_t out_size) {
	if (strlen(dir_utf8) >= out_size) return -1;
	strcpy(out, dir_utf8);
	for (char *c = out; *c; c++) {
		if (*c == '\\') *c = '/'; /* o mGBA só reconhece '/' */
	}
	size_t len = strlen(out);
	while (len > 1 && out[len - 1] == '/') {
		if (len == 3 && out[1] == ':') break; /* "X:/" é a raiz do disco; "X:" seria a pasta atual do disco */
		out[--len] = '\0';
	}
	if (len == 3 && out[1] == ':' && out[2] == '/') return 0;
	if (len == 2 && out[1] == ':') { /* "X:\\" reduzido por engano: devolve a raiz do disco */
		if (out_size < 4) return -1;
		out[2] = '/';
		out[3] = '\0';
	}
	return 0;
}

int romscan_scan(const char *dir_utf8, RomList *out) {
	out->items = NULL;
	out->count = 0;

	char root[1024];
	if (romscan_normalize_root(dir_utf8, root, sizeof(root)) != 0) return -1;

	Acc acc = {0};
	if (scan_dir(root, 0, &acc) != 0) {
		free(acc.items);
		return -1;
	}
	if (acc.count > 1) qsort(acc.items, (size_t) acc.count, sizeof(RomEntry), compare_entries);
	out->items = acc.items;
	out->count = acc.count;
	return 0;
}

void romscan_free(RomList *list) {
	free(list->items);
	list->items = NULL;
	list->count = 0;
}
