#include "save.h"

#include <string.h>

int save_sibling_path(const char *rom_path, const char *ext, char *out, size_t out_size) {
	const char *last_sep = NULL;
	for (const char *p = rom_path; *p; p++) {
		if (*p == '/' || *p == '\\') last_sep = p;
	}
	const char *dot = strrchr(rom_path, '.');
	size_t stem_len = (dot && (!last_sep || dot > last_sep)) ? (size_t) (dot - rom_path) : strlen(rom_path);
	size_t ext_len = strlen(ext);
	if (stem_len + ext_len + 1 > out_size) return -1;
	memcpy(out, rom_path, stem_len);
	memcpy(out + stem_len, ext, ext_len + 1);
	return 0;
}
