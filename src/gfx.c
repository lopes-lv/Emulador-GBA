#include "gfx.h"

#include <math.h>

int rect_contains(Rect r, int px, int py) {
	return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

void gfx_fill(uint32_t *fb, int fb_w, int fb_h, Rect r, uint32_t color) {
	int x0 = r.x < 0 ? 0 : r.x;
	int y0 = r.y < 0 ? 0 : r.y;
	int x1 = r.x + r.w > fb_w ? fb_w : r.x + r.w;
	int y1 = r.y + r.h > fb_h ? fb_h : r.y + r.h;
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) fb[y * fb_w + x] = color;
	}
}

uint32_t gfx_blend(uint32_t dst, uint32_t src, int alpha) {
	if (alpha <= 0) return dst;
	if (alpha >= 255) return src | 0xFF000000u;
	uint32_t out = 0xFF000000u;
	for (int shift = 0; shift <= 16; shift += 8) {
		uint32_t d = (dst >> shift) & 0xFF;
		uint32_t s = (src >> shift) & 0xFF;
		out |= ((d * (uint32_t) (255 - alpha) + s * (uint32_t) alpha + 127) / 255) << shift;
	}
	return out;
}

static float dist_to_segment(float px, float py, float x0, float y0, float x1, float y1) {
	float vx = x1 - x0, vy = y1 - y0;
	float len2 = vx * vx + vy * vy;
	float t = len2 > 0.0f ? ((px - x0) * vx + (py - y0) * vy) / len2 : 0.0f;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	float dx = px - (x0 + t * vx), dy = py - (y0 + t * vy);
	return sqrtf(dx * dx + dy * dy);
}

void gfx_line(uint32_t *fb, int fb_w, int fb_h, float x0, float y0, float x1, float y1, float width, uint32_t color) {
	float half = width / 2.0f + 0.5f;
	int minx = (int) floorf(fminf(x0, x1) - half);
	int maxx = (int) ceilf(fmaxf(x0, x1) + half);
	int miny = (int) floorf(fminf(y0, y1) - half);
	int maxy = (int) ceilf(fmaxf(y0, y1) + half);
	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (maxx > fb_w - 1) maxx = fb_w - 1;
	if (maxy > fb_h - 1) maxy = fb_h - 1;
	for (int y = miny; y <= maxy; y++) {
		for (int x = minx; x <= maxx; x++) {
			float d = dist_to_segment((float) x + 0.5f, (float) y + 0.5f, x0, y0, x1, y1);
			float cover = width / 2.0f + 0.5f - d;
			if (cover <= 0.0f) continue;
			if (cover > 1.0f) cover = 1.0f;
			fb[y * fb_w + x] = gfx_blend(fb[y * fb_w + x], color, (int) (cover * 255.0f + 0.5f));
		}
	}
}

static double channel_lin(uint32_t c8) {
	double c = (double) c8 / 255.0;
	return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double luminance(uint32_t abgr) {
	return 0.2126 * channel_lin(abgr & 0xFF) + 0.7152 * channel_lin((abgr >> 8) & 0xFF) +
	       0.0722 * channel_lin((abgr >> 16) & 0xFF);
}

double gfx_contrast(uint32_t abgr_a, uint32_t abgr_b) {
	double la = luminance(abgr_a), lb = luminance(abgr_b);
	if (la < lb) {
		double t = la; la = lb; lb = t;
	}
	return (la + 0.05) / (lb + 0.05);
}
