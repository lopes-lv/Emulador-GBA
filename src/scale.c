#include "scale.h"

#define MAX_PRESCALE 8

static int clamp(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

Rect scale_fit(int win_w, int win_h, int src_w, int src_h, ScaleMode mode) {
	if (win_w < 1) win_w = 1;
	if (win_h < 1) win_h = 1;
	if (src_w < 1) src_w = 1;
	if (src_h < 1) src_h = 1;

	if (mode == SCALE_INTEGER) {
		int n = win_w / src_w;
		if (win_h / src_h < n) n = win_h / src_h;
		if (n >= 1) {
			int w = src_w * n, h = src_h * n;
			return (Rect) {(win_w - w) / 2, (win_h - h) / 2, w, h};
		}
	}

	long long wide = (long long) win_w * src_h, tall = (long long) win_h * src_w;
	int w, h;
	if (wide <= tall) { /* limitado pela largura */
		w = win_w;
		h = (int) ((long long) win_w * src_h / src_w);
	} else {            /* limitado pela altura */
		h = win_h;
		w = (int) ((long long) win_h * src_w / src_h);
	}
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	if (w > win_w) w = win_w;
	if (h > win_h) h = win_h;
	return (Rect) {(win_w - w) / 2, (win_h - h) / 2, w, h};
}

int scale_prescale(int dst_w, int src_w) {
	if (src_w < 1 || dst_w <= src_w) return 1;
	return clamp((dst_w + src_w - 1) / src_w, 1, MAX_PRESCALE);
}

int scale_window_to_source(Rect dst, int src_w, int src_h, int wx, int wy, int *sx, int *sy) {
	int inside = rect_contains(dst, wx, wy);
	if (dst.w < 1 || dst.h < 1 || src_w < 1 || src_h < 1) {
		*sx = 0;
		*sy = 0;
		return 0;
	}
	long long x = (long long) (wx - dst.x) * src_w / dst.w;
	long long y = (long long) (wy - dst.y) * src_h / dst.h;
	if (wx < dst.x) x = 0;
	if (wy < dst.y) y = 0;
	*sx = clamp((int) (x > 100000 ? 100000 : x), 0, src_w - 1);
	*sy = clamp((int) (y > 100000 ? 100000 : y), 0, src_h - 1);
	return inside;
}
