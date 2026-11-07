#include "ui_metrics.h"

#include <math.h>

UiMetrics ui_metrics_fit(int out_w, int out_h) {
	if (out_w < 1) out_w = 1;
	if (out_h < 1) out_h = 1;
	float sx = (float) out_w / (float) UI_LOGICAL_W, sy = (float) out_h / (float) UI_LOGICAL_H;
	float s = sx < sy ? sx : sy;
	if (s < UI_SCALE_MIN) s = UI_SCALE_MIN;
	if (s > UI_SCALE_MAX) s = UI_SCALE_MAX;
	UiMetrics m;
	m.scale = s;
	m.w = (int) lroundf((float) UI_LOGICAL_W * s);
	m.h = (int) lroundf((float) UI_LOGICAL_H * s);
	m.ox = (out_w - m.w) / 2;
	m.oy = (out_h - m.h) / 2;
	return m;
}

int ui_metrics_window_to_logical(const UiMetrics *m, int px, int py, int *lx, int *ly) {
	int x = (int) floorf((float) (px - m->ox) / m->scale);
	int y = (int) floorf((float) (py - m->oy) / m->scale);
	int inside = px >= m->ox && px < m->ox + m->w && py >= m->oy && py < m->oy + m->h;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x > UI_LOGICAL_W - 1) x = UI_LOGICAL_W - 1;
	if (y > UI_LOGICAL_H - 1) y = UI_LOGICAL_H - 1;
	*lx = x;
	*ly = y;
	return inside;
}
