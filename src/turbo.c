#include "turbo.h"

#include <string.h>

#include "input.h"

void turbo_reset(Turbo *t) {
	memset(t, 0, sizeof(*t));
}

uint32_t turbo_step(Turbo *t, const Settings *s, uint32_t keys) {
	int half = settings_turbo_half_frames(s);
	for (int i = 0; i < BTN_COUNT; i++) {
		uint32_t bit = input_button_bit((GbaButton) i);
		if (!(keys & bit)) {
			t->held[i] = 0;
			continue;
		}
		if (t->held[i] < 1000000) t->held[i]++;
		if (s->turbo && settings_turbo_has(s, (GbaButton) i) && ((t->held[i] - 1) / half) % 2 == 1) keys &= ~bit;
	}
	return keys;
}
