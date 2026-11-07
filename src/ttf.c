#include "ttf.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "utf8.h"

extern const unsigned char inter_regular_ttf[];
extern const unsigned long inter_regular_ttf_size;

#define CACHE_SIZE 2048
#define PHASES 4        /* posições horizontais fracionárias: 0, 1/4, 1/2, 3/4 de pixel */
#define ELLIPSIS 0x2026u
#define MIN_PX 6
#define DEFAULT_GAMMA 0.85f /* < 1 reforça a cobertura parcial: texto claro em fundo escuro fica mais firme */

typedef struct {
	uint32_t codepoint;
	int px;                /* 0 = vaga livre */
	int phase;
	unsigned char *bitmap; /* w*h bytes de cobertura (já com gama); NULL quando o glifo é vazio (espaço) */
	int w, h, xoff, yoff;
	float advance;
} Glyph;

static stbtt_fontinfo font;
static int loaded;
static Glyph cache[CACHE_SIZE];
static int cache_used;
static unsigned char gamma_lut[256];
static float gamma_value = DEFAULT_GAMMA;

static void build_gamma_lut(void) {
	for (int i = 0; i < 256; i++) gamma_lut[i] = (unsigned char) (255.0f * powf((float) i / 255.0f, gamma_value) + 0.5f);
}

int ttf_init(void) {
	if (loaded) return 0;
	if (!stbtt_InitFont(&font, inter_regular_ttf, stbtt_GetFontOffsetForIndex(inter_regular_ttf, 0))) return -1;
	build_gamma_lut();
	loaded = 1;
	return 0;
}

static void cache_clear(void) {
	for (int i = 0; i < CACHE_SIZE; i++) {
		if (cache[i].bitmap) stbtt_FreeBitmap(cache[i].bitmap, NULL);
	}
	memset(cache, 0, sizeof(cache));
	cache_used = 0;
}

void ttf_set_gamma(float gamma) {
	gamma_value = gamma;
	if (loaded) {
		cache_clear();
		build_gamma_lut();
	}
}

void ttf_shutdown(void) {
	cache_clear();
	loaded = 0;
}

static float scale_for(int px) {
	return stbtt_ScaleForMappingEmToPixels(&font, (float) px);
}

static const Glyph *glyph_for(uint32_t cp, int px, int phase) {
	if (!stbtt_FindGlyphIndex(&font, (int) cp)) cp = '?';
	unsigned slot = (cp * 2654435761u + (unsigned) px * 40503u + (unsigned) phase * 9973u) % CACHE_SIZE;
	for (int probe = 0; probe < CACHE_SIZE; probe++) {
		Glyph *g = &cache[(slot + (unsigned) probe) % CACHE_SIZE];
		if (g->px == px && g->codepoint == cp && g->phase == phase) return g;
		if (g->px == 0) {
			if (cache_used >= CACHE_SIZE - 16) {
				cache_clear();
				return glyph_for(cp, px, phase);
			}
			float scale = scale_for(px);
			int advance, lsb;
			stbtt_GetCodepointHMetrics(&font, (int) cp, &advance, &lsb);
			g->codepoint = cp;
			g->px = px;
			g->phase = phase;
			g->advance = (float) advance * scale;
			g->bitmap = stbtt_GetCodepointBitmapSubpixel(&font, scale, scale, (float) phase / (float) PHASES, 0.0f, (int) cp,
			                                             &g->w, &g->h, &g->xoff, &g->yoff);
			if (g->bitmap) {
				for (int i = 0; i < g->w * g->h; i++) g->bitmap[i] = gamma_lut[g->bitmap[i]];
			} else {
				g->w = g->h = 0;
			}
			cache_used++;
			return g;
		}
	}
	return NULL;
}

int ttf_px_size(FontSize logical, float scale) {
	int px = (int) lroundf((float) logical * scale);
	return px < MIN_PX ? MIN_PX : px;
}

int ttf_width_px(int px, const char *utf8) {
	if (ttf_init() != 0 || !utf8) return 0;
	float total = 0.0f;
	for (uint32_t cp; (cp = utf8_next_codepoint(&utf8)) != 0;) {
		const Glyph *g = glyph_for(cp, px, 0);
		if (g) total += g->advance;
	}
	return (int) (total + 0.5f);
}

int ttf_line_height_px(int px) {
	if (ttf_init() != 0) return px;
	int ascent, descent, gap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
	return (int) lroundf((float) (ascent - descent + gap) * scale_for(px));
}

static int ascent_px(int px) {
	int ascent, descent, gap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
	return (int) lroundf((float) ascent * scale_for(px));
}

/* Desenha respeitando o buffer e o retângulo de recorte. Devolve a posição x final. */
static float draw_run(uint32_t *fb, int fb_w, int fb_h, Rect clip, int px, float x, int baseline, const char *utf8,
                      const char *end, uint32_t color) {
	int cx0 = clip.x < 0 ? 0 : clip.x;
	int cy0 = clip.y < 0 ? 0 : clip.y;
	int cx1 = clip.x + clip.w > fb_w ? fb_w : clip.x + clip.w;
	int cy1 = clip.y + clip.h > fb_h ? fb_h : clip.y + clip.h;
	while (utf8 < end || (!end && *utf8)) {
		uint32_t cp = utf8_next_codepoint(&utf8);
		if (cp == 0) break;
		float fx = floorf(x);
		int phase = (int) ((x - fx) * (float) PHASES);
		if (phase >= PHASES) phase = PHASES - 1;
		const Glyph *g = glyph_for(cp, px, phase);
		if (!g) continue;
		int gx = (int) fx + g->xoff;
		int gy = baseline + g->yoff;
		for (int j = 0; j < g->h; j++) {
			int py = gy + j;
			if (py < cy0 || py >= cy1) continue;
			for (int i = 0; i < g->w; i++) {
				int qx = gx + i;
				if (qx < cx0 || qx >= cx1) continue;
				int a = g->bitmap[j * g->w + i];
				if (a) fb[py * fb_w + qx] = gfx_blend(fb[py * fb_w + qx], color, a);
			}
		}
		x += g->advance;
	}
	return x;
}

void ttf_draw_px(uint32_t *fb, int fb_w, int fb_h, int px, float x, int baseline_y, const char *utf8, uint32_t color) {
	if (ttf_init() != 0 || !utf8) return;
	draw_run(fb, fb_w, fb_h, (Rect) {0, 0, fb_w, fb_h}, px, x, baseline_y, utf8, NULL, color);
}

void ttf_draw_fit_px(uint32_t *fb, int fb_w, int fb_h, int px, Rect box, const char *utf8, uint32_t color) {
	if (ttf_init() != 0 || !utf8 || box.w <= 0) return;
	int y_top = box.y + (box.h - ttf_line_height_px(px)) / 2;
	int baseline = y_top + ascent_px(px);
	if (ttf_width_px(px, utf8) <= box.w) {
		draw_run(fb, fb_w, fb_h, box, px, (float) box.x, baseline, utf8, NULL, color);
		return;
	}
	/* Não cabe: mantém o maior prefixo que deixa espaço para as reticências. */
	const Glyph *dots = glyph_for(ELLIPSIS, px, 0);
	float room = (float) box.w - (dots ? dots->advance : 0.0f);
	float used = 0.0f;
	const char *p = utf8, *cut = utf8;
	for (;;) {
		uint32_t cp = utf8_next_codepoint(&p);
		if (cp == 0) break;
		const Glyph *g = glyph_for(cp, px, 0);
		if (!g || used + g->advance > room) break;
		used += g->advance;
		cut = p;
	}
	float x = draw_run(fb, fb_w, fb_h, box, px, (float) box.x, baseline, utf8, cut, color);
	char tail[8] = "\xE2\x80\xA6";
	draw_run(fb, fb_w, fb_h, box, px, x, baseline, tail, NULL, color);
}

/* ---- atalhos com escala 1 ---- */

int ttf_width(FontSize size, const char *utf8) {
	return ttf_width_px((int) size, utf8);
}

int ttf_line_height(FontSize size) {
	return ttf_line_height_px((int) size);
}

void ttf_draw(uint32_t *fb, int fb_w, int fb_h, FontSize size, int x, int y_top, const char *utf8, uint32_t color) {
	if (ttf_init() != 0) return;
	ttf_draw_px(fb, fb_w, fb_h, (int) size, (float) x, y_top + ascent_px((int) size), utf8, color);
}

void ttf_draw_fit(uint32_t *fb, int fb_w, int fb_h, FontSize size, Rect box, const char *utf8, uint32_t color) {
	ttf_draw_fit_px(fb, fb_w, fb_h, (int) size, box, utf8, color);
}
