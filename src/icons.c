#include "icons.h"

#include <math.h>

#define STROKE 1.8f
#define PI_F 3.14159265f

typedef struct {
	Canvas *c;
	float x, y, size; /* origem e lado da caixa, em unidades lógicas */
	uint32_t color;
} Pen;

/* Traço reto (horizontal ou vertical): vira um retângulo de pixels inteiros, com a espessura arredondada e
 * extremidades quadradas, para as linhas ficarem nítidas. Devolve 0 se o traço não é reto. */
static int straight(const Pen *p, float x0, float y0, float x1, float y1) {
	Canvas *c = p->c;
	float S = c->scale;
	int t = (int) lroundf(STROKE * S);
	if (t < 1) t = 1;
	float ax = (float) c->ox + (p->x + x0 * p->size) * S, ay = (float) c->oy + (p->y + y0 * p->size) * S;
	float bx = (float) c->ox + (p->x + x1 * p->size) * S, by = (float) c->oy + (p->y + y1 * p->size) * S;
	Rect r;
	if (y0 == y1) {
		int xa = (int) lroundf(fminf(ax, bx)) - t / 2, xb = (int) lroundf(fmaxf(ax, bx)) + (t - t / 2);
		r = (Rect) {xa, (int) lroundf(ay - (float) t / 2.0f), xb - xa, t};
	} else if (x0 == x1) {
		int ya = (int) lroundf(fminf(ay, by)) - t / 2, yb = (int) lroundf(fmaxf(ay, by)) + (t - t / 2);
		r = (Rect) {(int) lroundf(ax - (float) t / 2.0f), ya, t, yb - ya};
	} else {
		return 0;
	}
	gfx_fill(c->px, c->w, c->h, r, p->color);
	return 1;
}

/* Coordenadas dos ícones em 0..1, escaladas para a caixa. */
static void seg(const Pen *p, float x0, float y0, float x1, float y1) {
	if (straight(p, x0, y0, x1, y1)) return;
	canvas_line(p->c, p->x + x0 * p->size, p->y + y0 * p->size, p->x + x1 * p->size, p->y + y1 * p->size, STROKE, p->color);
}

static void poly(const Pen *p, const float *pts, int count, int closed) {
	for (int i = 0; i + 1 < count; i++) seg(p, pts[2 * i], pts[2 * i + 1], pts[2 * i + 2], pts[2 * i + 3]);
	if (closed && count > 2) seg(p, pts[2 * (count - 1)], pts[2 * (count - 1) + 1], pts[0], pts[1]);
}

static void circle(const Pen *p, float cx, float cy, float r) {
	const int n = 24;
	for (int i = 0; i < n; i++) {
		float a0 = 2.0f * PI_F * (float) i / (float) n, a1 = 2.0f * PI_F * (float) (i + 1) / (float) n;
		seg(p, cx + r * cosf(a0), cy + r * sinf(a0), cx + r * cosf(a1), cy + r * sinf(a1));
	}
}

static void gamepad(const Pen *p) {
	static const float body[] = {0.20f, 0.30f, 0.80f, 0.30f, 0.92f, 0.70f, 0.78f, 0.78f,
	                             0.65f, 0.62f, 0.35f, 0.62f, 0.22f, 0.78f, 0.08f, 0.70f};
	poly(p, body, 8, 1);
	seg(p, 0.30f, 0.40f, 0.30f, 0.52f); /* direcional */
	seg(p, 0.24f, 0.46f, 0.36f, 0.46f);
	seg(p, 0.66f, 0.46f, 0.72f, 0.46f); /* botão */
}

static void settings(const Pen *p) {
	circle(p, 0.5f, 0.5f, 0.27f);
	circle(p, 0.5f, 0.5f, 0.10f);
	for (int i = 0; i < 8; i++) {
		float a = 2.0f * PI_F * (float) i / 8.0f;
		seg(p, 0.5f + 0.27f * cosf(a), 0.5f + 0.27f * sinf(a), 0.5f + 0.40f * cosf(a), 0.5f + 0.40f * sinf(a));
	}
}

void icon_draw_c(Canvas *c, IconId id, int x, int y, int size, uint32_t color) {
	Pen pen = {c, (float) x, (float) y, (float) size, color};
	const Pen *p = &pen;
	switch (id) {
	case ICON_APP:
	case ICON_GAMEPAD: gamepad(p); break;
	case ICON_MINIMIZE: seg(p, 0.20f, 0.50f, 0.80f, 0.50f); break;
	case ICON_MAXIMIZE: {
		static const float sq[] = {0.20f, 0.20f, 0.80f, 0.20f, 0.80f, 0.80f, 0.20f, 0.80f};
		poly(p, sq, 4, 1);
		break;
	}
	case ICON_RESTORE: {
		static const float front[] = {0.15f, 0.35f, 0.65f, 0.35f, 0.65f, 0.85f, 0.15f, 0.85f};
		static const float back[] = {0.35f, 0.35f, 0.35f, 0.15f, 0.85f, 0.15f, 0.85f, 0.65f, 0.65f, 0.65f};
		poly(p, front, 4, 1);
		poly(p, back, 5, 0);
		break;
	}
	case ICON_CLOSE:
		seg(p, 0.20f, 0.20f, 0.80f, 0.80f);
		seg(p, 0.80f, 0.20f, 0.20f, 0.80f);
		break;
	case ICON_ADD:
		seg(p, 0.50f, 0.15f, 0.50f, 0.85f);
		seg(p, 0.15f, 0.50f, 0.85f, 0.50f);
		break;
	case ICON_PLAY: {
		static const float tri[] = {0.25f, 0.15f, 0.85f, 0.50f, 0.25f, 0.85f};
		poly(p, tri, 3, 1);
		break;
	}
	case ICON_SETTINGS: settings(p); break;
	case ICON_SEARCH:
		circle(p, 0.42f, 0.42f, 0.27f);
		seg(p, 0.62f, 0.62f, 0.88f, 0.88f);
		break;
	default: break;
	}
}

void icon_draw(uint32_t *fb, int fb_w, int fb_h, IconId id, int x, int y, int size, uint32_t color) {
	Canvas c;
	canvas_init(&c, fb, fb_w, fb_h, 1.0f, 0, 0);
	icon_draw_c(&c, id, x, y, size, color);
}
