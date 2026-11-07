#include "settings.h"

const double FF_SPEEDS[FF_SPEED_COUNT] = {1.5, 2.0, 3.0, 4.0, 6.0, 8.0};
const int TURBO_RATES[TURBO_RATE_COUNT] = {5, 10, 15, 30};

void settings_defaults(Settings *s) {
	s->volume = 80;
	s->muted = 0;
	s->ff_index = 1;
	s->scale = SCALE_FILL;
	s->start_fullscreen = 0;
	s->ff_always = 0;
	s->turbo = 0;
	s->turbo_rate = 1;
	s->turbo_mask = (1u << BTN_A) | (1u << BTN_B);
	s->stick_on = 1;
	s->stick_deadzone = 50;
}

double settings_ff_speed(const Settings *s) {
	int i = s->ff_index;
	if (i < 0) i = 0;
	if (i >= FF_SPEED_COUNT) i = FF_SPEED_COUNT - 1;
	return FF_SPEEDS[i];
}

int settings_step_volume(Settings *s, int delta) {
	int v = s->volume + delta;
	if (v < 0) v = 0;
	if (v > 100) v = 100;
	int changed = v != s->volume;
	s->volume = v;
	return changed;
}

int settings_step_speed(Settings *s, int delta) {
	int i = s->ff_index + delta;
	if (i < 0) i = 0;
	if (i >= FF_SPEED_COUNT) i = FF_SPEED_COUNT - 1;
	int changed = i != s->ff_index;
	s->ff_index = i;
	return changed;
}

int settings_step_deadzone(Settings *s, int delta) {
	int z = s->stick_deadzone + delta;
	if (z < STICK_DEADZONE_MIN) z = STICK_DEADZONE_MIN;
	if (z > STICK_DEADZONE_MAX) z = STICK_DEADZONE_MAX;
	int changed = z != s->stick_deadzone;
	s->stick_deadzone = z;
	return changed;
}

int settings_toggle_mute(Settings *s) {
	s->muted = !s->muted;
	return s->muted;
}

int settings_turbo_half_frames(const Settings *s) {
	int i = s->turbo_rate;
	if (i < 0) i = 0;
	if (i >= TURBO_RATE_COUNT) i = TURBO_RATE_COUNT - 1;
	return 60 / (2 * TURBO_RATES[i]);
}

int settings_turbo_has(const Settings *s, GbaButton b) {
	if ((int) b < 0 || (int) b >= BTN_COUNT) return 0;
	return (s->turbo_mask >> b) & 1u;
}

int settings_turbo_toggle_button(Settings *s, GbaButton b) {
	if ((int) b < 0 || (int) b >= BTN_COUNT) return 0;
	s->turbo_mask ^= 1u << b;
	return 1;
}
