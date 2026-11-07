#include "pacer.h"

void pacer_reset(Pacer *p, uint64_t now) {
	p->carry = 0.0;
	p->last = now;
}

int pacer_frames_due(Pacer *p, uint64_t now, uint64_t freq, double multiplier) {
	if (freq == 0) return 0;
	if (now < p->last) {
		p->last = now;
		return 0;
	}
	if (now == p->last) return 0;
	double seconds = (double) (now - p->last) / (double) freq;
	p->last = now;
	p->carry += seconds * multiplier * GBA_FPS;
	int n = (int) p->carry;
	if (n > PACER_MAX_FRAMES) {
		p->carry = 0.0;
		return PACER_MAX_FRAMES;
	}
	p->carry -= n;
	return n;
}
