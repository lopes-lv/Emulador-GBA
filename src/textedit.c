#include "textedit.h"

#include <string.h>

#include "utf8.h"

void textedit_clear(TextEdit *t) {
	t->text[0] = '\0';
	t->len_chars = 0;
}

int textedit_insert(TextEdit *t, const char *utf8) {
	if (!utf8) return 0;
	int changed = 0;
	size_t used = strlen(t->text);
	while (*utf8 && t->len_chars < TEXTEDIT_MAX) {
		const char *start = utf8;
		uint32_t cp = utf8_next_codepoint(&utf8);
		if (cp == 0) break;
		size_t n = (size_t) (utf8 - start);
		int invalid = cp == 0xFFFD && !(n == 3 && (unsigned char) start[0] == 0xEF);
		if (invalid || cp < 0x20 || cp == 0x7F) continue;
		memcpy(t->text + used, start, n);
		used += n;
		t->text[used] = '\0';
		t->len_chars++;
		changed = 1;
	}
	return changed;
}

int textedit_backspace(TextEdit *t) {
	size_t len = strlen(t->text);
	if (len == 0) return 0;
	size_t i = len - 1;
	while (i > 0 && (((unsigned char) t->text[i]) & 0xC0) == 0x80) i--;
	t->text[i] = '\0';
	if (t->len_chars > 0) t->len_chars--;
	return 1;
}
