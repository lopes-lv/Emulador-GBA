#include "canvas.h"

#include <math.h>

static int snap(float v) {
	return (int) lroundf(v);
}

static float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

void canvas_init(Canvas *c, uint32_t *px, int w, int h, float scale, int ox, int oy) {
	c->px = px;
	c->w = w;
	c->h = h;
	c->scale = scale;
	c->ox = ox;
	c->oy = oy;
}

Rect canvas_device_rect(const Canvas *c, Rect l) {
	int x0 = c->ox + snap((float) l.x * c->scale);
	int y0 = c->oy + snap((float) l.y * c->scale);
	int x1 = c->ox + snap((float) (l.x + l.w) * c->scale);
	int y1 = c->oy + snap((float) (l.y + l.h) * c->scale);
	if (l.w > 0 && x1 - x0 < 1) x1 = x0 + 1;
	if (l.h > 0 && y1 - y0 < 1) y1 = y0 + 1;
	return (Rect) {x0, y0, x1 - x0, y1 - y0};
}

int canvas_line_px(const Canvas *c) {
	int t = snap(c->scale);
	return t < 1 ? 1 : t;
}

void canvas_fill(Canvas *c, Rect logical, uint32_t color) {
	gfx_fill(c->px, c->w, c->h, canvas_device_rect(c, logical), color);
}

/* Cobertura (0..1) do pixel (i, j) de um retângulo w x h com cantos de raio r, pelo centro do pixel. */
static float corner_coverage(int i, int j, int w, int h, float r) {
	float cx = (float) i + 0.5f, cy = (float) j + 0.5f;
	float dx = fmaxf(0.0f, fmaxf(r - cx, cx - ((float) w - r)));
	float dy = fmaxf(0.0f, fmaxf(r - cy, cy - ((float) h - r)));
	if (dx <= 0.0f || dy <= 0.0f) return 1.0f;
	return clampf(r - sqrtf(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f);
}

static void put_cover(Canvas *c, int x, int y, uint32_t color, float cover) {
	if (x < 0 || y < 0 || x >= c->w || y >= c->h || cover <= 0.0f) return;
	int alpha = (int) (cover * 255.0f + 0.5f);
	c->px[y * c->w + x] = alpha >= 255 ? (color | 0xFF000000u) : gfx_blend(c->px[y * c->w + x], color, alpha);
}

/* Preenche o retângulo de pixels `d` com cantos de raio `r` (em pixels), com cobertura exata nos cantos. */
static void round_device(Canvas *c, Rect d, float r, uint32_t color) {
	if (d.w < 1 || d.h < 1) return;
	float max_r = (float) (d.w < d.h ? d.w : d.h) / 2.0f;
	if (r > max_r) r = max_r;
	if (r < 0.5f) {
		gfx_fill(c->px, c->w, c->h, d, color);
		return;
	}
	int band = (int) ceilf(r);
	for (int j = 0; j < d.h; j++) {
		int y = d.y + j;
		if (y < 0 || y >= c->h) continue;
		if (j >= band && j < d.h - band) {
			gfx_fill(c->px, c->w, c->h, (Rect) {d.x, y, d.w, 1}, color);
			continue;
		}
		for (int i = 0; i < d.w; i++) {
			put_cover(c, d.x + i, y, color, corner_coverage(i, j, d.w, d.h, r));
		}
	}
}

void canvas_fill_round(Canvas *c, Rect logical, int radius_logical, uint32_t color) {
	round_device(c, canvas_device_rect(c, logical), (float) radius_logical * c->scale, color);
}

void canvas_box(Canvas *c, Rect logical, int radius_logical, uint32_t border, uint32_t body) {
	Rect d = canvas_device_rect(c, logical);
	int t = canvas_line_px(c);
	float r = (float) radius_logical * c->scale;
	round_device(c, d, r, border);
	Rect inner = {d.x + t, d.y + t, d.w - 2 * t, d.h - 2 * t};
	float ri = r - (float) t;
	round_device(c, inner, ri < 0.0f ? 0.0f : ri, body);
}

/* Componente `shift` (0, 8, 16, 24) do pixel. */
static float chan(uint32_t p, int shift) {
	return (float) ((p >> shift) & 0xFF);
}

static uint32_t sample_bilinear(const uint32_t *src, int sw, int sh, float u, float v) {
	int x0 = (int) floorf(u), y0 = (int) floorf(v);
	float tx = u - (float) x0, ty = v - (float) y0;
	int xa = x0 < 0 ? 0 : (x0 >= sw ? sw - 1 : x0), xb = x0 + 1 < 0 ? 0 : (x0 + 1 >= sw ? sw - 1 : x0 + 1);
	int ya = y0 < 0 ? 0 : (y0 >= sh ? sh - 1 : y0), yb = y0 + 1 < 0 ? 0 : (y0 + 1 >= sh ? sh - 1 : y0 + 1);
	uint32_t out = 0xFF000000u;
	for (int shift = 0; shift <= 16; shift += 8) {
		float a = chan(src[ya * sw + xa], shift) * (1.0f - tx) + chan(src[ya * sw + xb], shift) * tx;
		float b = chan(src[yb * sw + xa], shift) * (1.0f - tx) + chan(src[yb * sw + xb], shift) * tx;
		out |= (uint32_t) (a * (1.0f - ty) + b * ty + 0.5f) << shift;
	}
	return out;
}

/* Média de área da região [x0, x1) x [y0, y1) da imagem (coordenadas fracionárias). */
static uint32_t sample_box(const uint32_t *src, int sw, int sh, float x0, float y0, float x1, float y1) {
	float sum[3] = {0, 0, 0}, total = 0;
	int ia = (int) floorf(x0), ib = (int) ceilf(x1), ja = (int) floorf(y0), jb = (int) ceilf(y1);
	for (int j = ja; j < jb; j++) {
		float wy = fminf((float) (j + 1), y1) - fmaxf((float) j, y0);
		if (wy <= 0.0f || j < 0 || j >= sh) continue;
		for (int i = ia; i < ib; i++) {
			float wx = fminf((float) (i + 1), x1) - fmaxf((float) i, x0);
			if (wx <= 0.0f || i < 0 || i >= sw) continue;
			float w = wx * wy;
			for (int k = 0; k < 3; k++) sum[k] += chan(src[j * sw + i], 8 * k) * w;
			total += w;
		}
	}
	if (total <= 0.0f) return 0xFF000000u;
	uint32_t out = 0xFF000000u;
	for (int k = 0; k < 3; k++) out |= (uint32_t) (sum[k] / total + 0.5f) << (8 * k);
	return out;
}

void canvas_blit(Canvas *c, Rect logical, const uint32_t *src, int sw, int sh, int radius_logical) {
	if (!src || sw < 1 || sh < 1) return;
	Rect d = canvas_device_rect(c, logical);
	if (d.w < 1 || d.h < 1) return;
	float r = (float) radius_logical * c->scale;
	float max_r = (float) (d.w < d.h ? d.w : d.h) / 2.0f;
	if (r > max_r) r = max_r;
	int down = d.w < sw || d.h < sh;
	for (int j = 0; j < d.h; j++) {
		int y = d.y + j;
		if (y < 0 || y >= c->h) continue;
		for (int i = 0; i < d.w; i++) {
			int x = d.x + i;
			if (x < 0 || x >= c->w) continue;
			float cover = r < 0.5f ? 1.0f : corner_coverage(i, j, d.w, d.h, r);
			if (cover <= 0.0f) continue;
			uint32_t color;
			if (down) {
				color = sample_box(src, sw, sh, (float) i * (float) sw / (float) d.w, (float) j * (float) sh / (float) d.h,
				                   (float) (i + 1) * (float) sw / (float) d.w, (float) (j + 1) * (float) sh / (float) d.h);
			} else {
				color = sample_bilinear(src, sw, sh, ((float) i + 0.5f) * (float) sw / (float) d.w - 0.5f,
				                        ((float) j + 0.5f) * (float) sh / (float) d.h - 0.5f);
			}
			put_cover(c, x, y, color, cover);
		}
	}
}

void canvas_frame_n(Canvas *c, Rect logical, int lines, uint32_t color) {
	Rect d = canvas_device_rect(c, logical);
	int t = canvas_line_px(c) * (lines < 1 ? 1 : lines);
	if (d.w < 1 || d.h < 1) return;
	int th = t < d.h ? t : d.h, tw = t < d.w ? t : d.w;
	gfx_fill(c->px, c->w, c->h, (Rect) {d.x, d.y, d.w, th}, color);
	gfx_fill(c->px, c->w, c->h, (Rect) {d.x, d.y + d.h - th, d.w, th}, color);
	gfx_fill(c->px, c->w, c->h, (Rect) {d.x, d.y, tw, d.h}, color);
	gfx_fill(c->px, c->w, c->h, (Rect) {d.x + d.w - tw, d.y, tw, d.h}, color);
}

void canvas_frame(Canvas *c, Rect logical, uint32_t color) {
	canvas_frame_n(c, logical, 1, color);
}

void canvas_edge(Canvas *c, Rect logical, Edge edge, uint32_t color) {
	Rect d = canvas_device_rect(c, logical);
	int t = canvas_line_px(c);
	if (d.w < 1 || d.h < 1) return;
	Rect r = d;
	switch (edge) {
	case EDGE_TOP: r.h = t < d.h ? t : d.h; break;
	case EDGE_BOTTOM: r.h = t < d.h ? t : d.h; r.y = d.y + d.h - r.h; break;
	case EDGE_LEFT: r.w = t < d.w ? t : d.w; break;
	case EDGE_RIGHT: r.w = t < d.w ? t : d.w; r.x = d.x + d.w - r.w; break;
	}
	gfx_fill(c->px, c->w, c->h, r, color);
}

void canvas_disc(Canvas *c, float cx, float cy, float radius, uint32_t color) {
	float px = (float) c->ox + cx * c->scale, py = (float) c->oy + cy * c->scale;
	float r = radius * c->scale;
	int x0 = (int) floorf(px - r - 1.0f), x1 = (int) ceilf(px + r + 1.0f);
	int y0 = (int) floorf(py - r - 1.0f), y1 = (int) ceilf(py + r + 1.0f);
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			float dx = (float) x + 0.5f - px, dy = (float) y + 0.5f - py;
			put_cover(c, x, y, color, clampf(r - sqrtf(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f));
		}
	}
}

void canvas_line(Canvas *c, float x0, float y0, float x1, float y1, float width, uint32_t color) {
	gfx_line(c->px, c->w, c->h, (float) c->ox + x0 * c->scale, (float) c->oy + y0 * c->scale,
	         (float) c->ox + x1 * c->scale, (float) c->oy + y1 * c->scale, width * c->scale, color);
}
