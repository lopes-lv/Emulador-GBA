#include "library.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utf8.h"

const char *const PLATFORMS[PLATFORM_COUNT] = {"GBA"};

#define PLATFORM_NAME "GBA"

static void copy(char *dst, size_t n, const char *src) {
	snprintf(dst, n, "%s", src);
}

void library_free(Library *lib) {
	free(lib->items);
	lib->items = NULL;
	lib->count = 0;
}

void library_from_roms(Library *lib, const RomList *roms) {
	lib->items = NULL;
	lib->count = 0;
	if (!roms || roms->count <= 0) return;
	lib->items = calloc((size_t) roms->count, sizeof(LibEntry));
	if (!lib->items) return;
	lib->count = roms->count;
	for (int i = 0; i < roms->count; i++) {
		LibEntry *e = &lib->items[i];
		copy(e->title, sizeof(e->title), roms->items[i].name);
		copy(e->path, sizeof(e->path), roms->items[i].path);
		copy(e->platform, sizeof(e->platform), PLATFORM_NAME);
		e->playable = 1;
	}
}

void library_demo(Library *lib) {
	static const struct {
		const char *title, *genre;
		int year;
		int64_t playtime_s;
		int days_ago; /* -1 = nunca */
	} games[] = {
		{"Neon Drifter", "Corrida futurista", 1995, 87000, 1},
		{"Kori Blade", "A\xC3\xA7\xC3\xA3o e aventura", 1998, 45900, 2},
		{"Starfall Arcadia", "Plataforma 3D", 1999, 30600, 0},
		{"Pocket Wyrm", "RPG de criaturas", 2002, 198000, 3},
		{"Hollow Lantern", "Survival horror", 1999, 12300, 7},
		{"Iron Meridian", "Run and gun", 1993, 5400, 14},
		{"Sable & Soot", "RPG t\xC3\xA1tico", 1996, 64800, 21},
		{"Cobalt Fist", "Luta", 2000, 9000, 35},
		{"Lunar Loop", "Quebra-cabe\xC3\xA7""a", 2003, 2700, 60},
		{"Echo Garrison", "Estrat\xC3\xA9gia", 2001, 0, -1},
	};
	int n = (int) (sizeof(games) / sizeof(games[0]));
	lib->items = calloc((size_t) n, sizeof(LibEntry));
	lib->count = lib->items ? n : 0;
	int64_t now = (int64_t) time(NULL);
	for (int i = 0; i < lib->count; i++) {
		LibEntry *e = &lib->items[i];
		copy(e->title, sizeof(e->title), games[i].title);
		copy(e->genre, sizeof(e->genre), games[i].genre);
		copy(e->platform, sizeof(e->platform), PLATFORM_NAME);
		e->year = games[i].year;
		e->playtime_s = games[i].playtime_s;
		e->last_played = games[i].days_ago < 0 ? 0 : now - (int64_t) games[i].days_ago * 86400;
		e->playable = 0;
	}
}

/* Minúsculas ASCII, sem acentos (Latin-1); outros caracteres viram '?'. */
static void fold(const char *utf8, char *out, size_t n) {
	size_t len = 0;
	uint32_t cp;
	while (len + 1 < n && (cp = utf8_next_codepoint(&utf8)) != 0) {
		char c = '?';
		if (cp < 0x80) {
			c = (char) tolower((int) cp);
		} else if ((cp >= 0xC0 && cp <= 0xC5) || (cp >= 0xE0 && cp <= 0xE5)) {
			c = 'a';
		} else if (cp == 0xC7 || cp == 0xE7) {
			c = 'c';
		} else if ((cp >= 0xC8 && cp <= 0xCB) || (cp >= 0xE8 && cp <= 0xEB)) {
			c = 'e';
		} else if ((cp >= 0xCC && cp <= 0xCF) || (cp >= 0xEC && cp <= 0xEF)) {
			c = 'i';
		} else if (cp == 0xD1 || cp == 0xF1) {
			c = 'n';
		} else if ((cp >= 0xD2 && cp <= 0xD6) || (cp >= 0xF2 && cp <= 0xF6)) {
			c = 'o';
		} else if ((cp >= 0xD9 && cp <= 0xDC) || (cp >= 0xF9 && cp <= 0xFC)) {
			c = 'u';
		} else if (cp == 0xDD || cp == 0xFD || cp == 0xFF) {
			c = 'y';
		}
		out[len++] = c;
	}
	out[len] = '\0';
}

static int contains_folded(const char *haystack, const char *folded_needle) {
	char buf[1100];
	fold(haystack, buf, sizeof(buf));
	return strstr(buf, folded_needle) != NULL;
}

int library_filter(const Library *lib, const char *platform, const char *query, int *out) {
	char needle[300];
	fold(query ? query : "", needle, sizeof(needle));
	int n = 0;
	for (int i = 0; i < lib->count; i++) {
		const LibEntry *e = &lib->items[i];
		if (platform && _stricmp(platform, e->platform) != 0) continue;
		if (needle[0] && !contains_folded(e->title, needle) && !contains_folded(e->platform, needle) &&
		    !contains_folded(e->genre, needle)) {
			continue;
		}
		out[n++] = i;
	}
	return n;
}

int library_count_platform(const Library *lib, const char *platform) {
	int n = 0;
	for (int i = 0; i < lib->count; i++) {
		if (!platform || _stricmp(platform, lib->items[i].platform) == 0) n++;
	}
	return n;
}

void library_format_playtime(int64_t seconds, char *out, size_t n) {
	if (seconds < 0) seconds = 0;
	int64_t minutes = seconds / 60;
	if (minutes >= 60) {
		snprintf(out, n, "%d h %d min", (int) (minutes / 60), (int) (minutes % 60));
	} else {
		snprintf(out, n, "%d min", (int) minutes);
	}
}

/* Número do dia (calendário local) para comparar datas sem se preocupar com horário de verão. */
static long day_number(int64_t t) {
	time_t tt = (time_t) t;
	struct tm *tm = localtime(&tt);
	if (!tm) return 0;
	long y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
	if (m <= 2) { y--; m += 12; }
	return 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d;
}

void library_format_last(int64_t last_played, int64_t now, char *out, size_t n) {
	if (last_played <= 0) {
		snprintf(out, n, "Nunca");
		return;
	}
	long days = day_number(now) - day_number(last_played);
	if (days <= 0) {
		snprintf(out, n, "Hoje");
	} else if (days == 1) {
		snprintf(out, n, "Ontem");
	} else if (days < 7) {
		snprintf(out, n, "H\xC3\xA1 %ld dias", days);
	} else if (days < 30) {
		long w = days / 7;
		snprintf(out, n, "H\xC3\xA1 %ld %s", w, w == 1 ? "semana" : "semanas");
	} else if (days < 365) {
		long m = days / 30;
		snprintf(out, n, "H\xC3\xA1 %ld %s", m, m == 1 ? "m\xC3\xAAs" : "meses");
	} else {
		long y = days / 365;
		snprintf(out, n, "H\xC3\xA1 %ld %s", y, y == 1 ? "ano" : "anos");
	}
}

static FILE *open_utf8(const char *path, int write) {
#ifdef _WIN32
	wchar_t wpath[2048];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 2048) == 0) return NULL;
	return _wfopen(wpath, write ? L"wb" : L"rb");
#else
	return fopen(path, write ? "wb" : "rb");
#endif
}

int library_meta_load(Library *lib, const char *ini_path) {
	FILE *f = open_utf8(ini_path, 0);
	if (!f) return -1;
	static char line[2400];
	while (fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
		char *t1 = strchr(line, '\t');
		if (!t1) continue;
		*t1++ = '\0';
		char *t2 = strchr(t1, '\t');
		if (!t2) continue;
		*t2++ = '\0';
		for (int i = 0; i < lib->count; i++) {
			if (strcmp(lib->items[i].path, line) != 0) continue;
			lib->items[i].playtime_s = _strtoi64(t1, NULL, 10);
			lib->items[i].last_played = _strtoi64(t2, NULL, 10);
			break;
		}
	}
	fclose(f);
	return 0;
}

int library_meta_save(const Library *lib, const char *ini_path) {
	FILE *f = open_utf8(ini_path, 1);
	if (!f) return -1;
	int ok = 1;
	for (int i = 0; i < lib->count && ok; i++) {
		const LibEntry *e = &lib->items[i];
		if (e->playtime_s == 0 && e->last_played == 0) continue;
		ok = fprintf(f, "%s\t%lld\t%lld\n", e->path, (long long) e->playtime_s, (long long) e->last_played) >= 0;
	}
	if (fclose(f) != 0) ok = 0;
	return ok ? 0 : -1;
}

void library_add_session(LibEntry *e, int64_t seconds, int64_t now) {
	if (seconds > 0) e->playtime_s += seconds;
	e->last_played = now;
}

/* ---- miniaturas ---- */

#define THUMB_BYTES (LIB_THUMB * LIB_THUMB * (int) sizeof(uint32_t))

void library_thumb_make(const uint32_t *frame, uint32_t out[LIB_THUMB * LIB_THUMB]) {
	const int crop_x = 40, crop = 160;
	const float step = (float) crop / (float) LIB_THUMB; /* 2,5 pixels do quadro por pixel da miniatura */
	for (int ty = 0; ty < LIB_THUMB; ty++) {
		float y0 = (float) ty * step, y1 = y0 + step;
		for (int tx = 0; tx < LIB_THUMB; tx++) {
			float x0 = (float) tx * step, x1 = x0 + step;
			float sum[3] = {0, 0, 0}, total = 0;
			for (int y = (int) y0; y < (int) ceilf(y1) && y < crop; y++) {
				float wy = fminf((float) (y + 1), y1) - fmaxf((float) y, y0);
				for (int x = (int) x0; x < (int) ceilf(x1) && x < crop; x++) {
					float wx = fminf((float) (x + 1), x1) - fmaxf((float) x, x0);
					float w = wx * wy;
					uint32_t p = frame[y * 240 + crop_x + x];
					for (int c = 0; c < 3; c++) sum[c] += (float) ((p >> (8 * c)) & 0xFF) * w;
					total += w;
				}
			}
			uint32_t px = 0xFF000000u;
			for (int c = 0; c < 3; c++) px |= (uint32_t) (sum[c] / total + 0.5f) << (8 * c);
			out[ty * LIB_THUMB + tx] = px;
		}
	}
}

int library_thumb_file(const char *dir, const char *rom_path, char *out, size_t n) {
	uint32_t h = 2166136261u;
	for (const char *p = rom_path; *p; p++) h = (h ^ (unsigned char) *p) * 16777619u;
	int len = snprintf(out, n, "%s/%08x.thumb", dir, (unsigned) h);
	return (len < 0 || (size_t) len >= n) ? -1 : 0;
}

static void make_dir(const char *path) {
#ifdef _WIN32
	wchar_t wpath[2048];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 2048) != 0) CreateDirectoryW(wpath, NULL); /* já existir não é erro */
#else
	mkdir(path, 0755);
#endif
}

int library_thumb_save(const char *dir, const char *rom_path, const uint32_t px[LIB_THUMB * LIB_THUMB]) {
	char file[2048];
	if (library_thumb_file(dir, rom_path, file, sizeof(file)) != 0) return -1;
	make_dir(dir);
	FILE *f = open_utf8(file, 1);
	if (!f) return -1;
	int ok = fwrite(px, 1, THUMB_BYTES, f) == (size_t) THUMB_BYTES;
	if (fclose(f) != 0) ok = 0;
	return ok ? 0 : -1;
}

int library_thumb_load(const char *dir, const char *rom_path, uint32_t px[LIB_THUMB * LIB_THUMB]) {
	char file[2048];
	if (library_thumb_file(dir, rom_path, file, sizeof(file)) != 0) return -1;
	FILE *f = open_utf8(file, 0);
	if (!f) return -1;
	uint32_t buf[LIB_THUMB * LIB_THUMB];
	int ok = fread(buf, 1, THUMB_BYTES, f) == (size_t) THUMB_BYTES && fgetc(f) == EOF;
	fclose(f);
	if (ok) memcpy(px, buf, sizeof(buf));
	return ok ? 0 : -1;
}

void library_thumbs_load(Library *lib, const char *dir) {
	for (int i = 0; i < lib->count; i++) {
		lib->items[i].has_thumb = library_thumb_load(dir, lib->items[i].path, lib->items[i].thumb) == 0;
	}
}
