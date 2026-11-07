#include "utf8.h"

uint32_t utf8_next_codepoint(const char **s) {
	const unsigned char *p = (const unsigned char *) *s;
	if (*p == 0) return 0;

	int extra;
	uint32_t cp;
	if (*p < 0x80) {
		*s += 1;
		return *p;
	} else if ((*p & 0xE0) == 0xC0) {
		extra = 1;
		cp = *p & 0x1F;
	} else if ((*p & 0xF0) == 0xE0) {
		extra = 2;
		cp = *p & 0x0F;
	} else if ((*p & 0xF8) == 0xF0) {
		extra = 3;
		cp = *p & 0x07;
	} else {
		*s += 1;
		return 0xFFFD;
	}
	for (int i = 1; i <= extra; i++) {
		if ((p[i] & 0xC0) != 0x80) { /* continuação ausente (inclui o fim da string) */
			*s += 1;
			return 0xFFFD;
		}
		cp = (cp << 6) | (p[i] & 0x3F);
	}
	*s += extra + 1;
	return cp;
}
