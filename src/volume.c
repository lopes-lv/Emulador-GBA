#include "volume.h"

int volume_gain_q8(int percent, int muted) {
	if (muted) return 0;
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	return (percent * percent * 256 + 5000) / 10000;
}

void volume_apply(int16_t *samples, size_t count, int gain_q8) {
	if (gain_q8 < 0) gain_q8 = 0;
	if (gain_q8 > 256) gain_q8 = 256;
	if (gain_q8 == 256) return;
	for (size_t i = 0; i < count; i++) samples[i] = (int16_t) (((int32_t) samples[i] * gain_q8) >> 8);
}
