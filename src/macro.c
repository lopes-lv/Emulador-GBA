#include "macro.h"

#include <stdlib.h>
#include <string.h>

void macro_init(Macro *m, const char *name) {
	memset(m, 0, sizeof(*m));
	m->mode = MACRO_ONCE;
	m->repeat = 1;
	int chars = 0, len = 0;
	for (const unsigned char *p = (const unsigned char *) (name ? name : ""); *p && len < MACRO_NAME_BYTES - 1; p++) {
		if ((*p & 0xC0) != 0x80) { /* byte inicial de um caractere */
			if (chars == MACRO_NAME_MAX) break;
			chars++;
		}
		m->name[len++] = (char) *p;
	}
	m->name[len] = '\0';
}

void macro_free(Macro *m) {
	free(m->steps);
	m->steps = NULL;
	m->count = 0;
	m->total_frames = 0;
}

int macro_add_step(Macro *m, uint16_t keys, uint32_t frames) {
	if (frames == 0 || (keys & ~MACRO_KEY_MASK)) return -1;
	if (m->total_frames + frames > MACRO_MAX_FRAMES) return -1;
	if (m->count > 0 && m->steps[m->count - 1].keys == keys) {
		m->steps[m->count - 1].frames += frames;
		m->total_frames += frames;
		return 0;
	}
	if (m->count >= MACRO_MAX_STEPS) return -1;
	MacroStep *grown = realloc(m->steps, (size_t) (m->count + 1) * sizeof(MacroStep));
	if (!grown) return -1;
	m->steps = grown;
	m->steps[m->count++] = (MacroStep) {keys, frames};
	m->total_frames += frames;
	return 0;
}

/* ---- edição ---- */

static void recalc_total(Macro *m) {
	m->total_frames = 0;
	for (int i = 0; i < m->count; i++) m->total_frames += m->steps[i].frames;
}

int macro_step_insert(Macro *m, int index, uint16_t keys, uint32_t frames) {
	if (index < 0 || index > m->count) return -1;
	if (frames == 0 || frames > MACRO_MAX_FRAMES || (keys & ~MACRO_KEY_MASK)) return -1;
	if (m->count >= MACRO_MAX_STEPS || m->total_frames + frames > MACRO_MAX_FRAMES) return -1;
	MacroStep *grown = realloc(m->steps, (size_t) (m->count + 1) * sizeof(MacroStep));
	if (!grown) return -1;
	m->steps = grown;
	memmove(m->steps + index + 1, m->steps + index, (size_t) (m->count - index) * sizeof(MacroStep));
	m->steps[index] = (MacroStep) {keys, frames};
	m->count++;
	m->total_frames += frames;
	return 0;
}

int macro_step_remove(Macro *m, int index) {
	if (index < 0 || index >= m->count) return -1;
	memmove(m->steps + index, m->steps + index + 1, (size_t) (m->count - index - 1) * sizeof(MacroStep));
	m->count--;
	recalc_total(m);
	if (m->count == 0) {
		free(m->steps);
		m->steps = NULL;
	}
	return 0;
}

int macro_step_duplicate(Macro *m, int index) {
	if (index < 0 || index >= m->count) return -1;
	MacroStep s = m->steps[index];
	return macro_step_insert(m, index + 1, s.keys, s.frames);
}

int macro_step_move(Macro *m, int index, int delta) {
	if (index < 0 || index >= m->count || (delta != -1 && delta != 1)) return -1;
	int to = index + delta;
	if (to < 0 || to >= m->count) return -1;
	MacroStep tmp = m->steps[index];
	m->steps[index] = m->steps[to];
	m->steps[to] = tmp;
	return to;
}

int macro_step_set_frames(Macro *m, int index, uint32_t frames) {
	if (index < 0 || index >= m->count || frames == 0 || frames > MACRO_MAX_FRAMES) return -1;
	if (m->total_frames - m->steps[index].frames + frames > MACRO_MAX_FRAMES) return -1;
	m->steps[index].frames = frames;
	recalc_total(m);
	return 0;
}

int macro_step_toggle_key(Macro *m, int index, uint16_t bit) {
	if (index < 0 || index >= m->count || bit == 0 || (bit & (bit - 1)) != 0 || (bit & ~MACRO_KEY_MASK)) return -1;
	m->steps[index].keys ^= bit;
	return 0;
}

int macro_set_repeat(Macro *m, int repeat) {
	if (repeat < 1) repeat = 1;
	if (repeat > MACRO_MAX_REPEAT) repeat = MACRO_MAX_REPEAT;
	m->repeat = repeat;
	return 0;
}

int macro_copy(Macro *dst, const Macro *src) {
	MacroStep *steps = NULL;
	if (src->count > 0) {
		steps = malloc((size_t) src->count * sizeof(MacroStep));
		if (!steps) return -1;
		memcpy(steps, src->steps, (size_t) src->count * sizeof(MacroStep));
	}
	*dst = *src;
	dst->steps = steps;
	return 0;
}

void macro_rec_begin(MacroRecorder *r, Macro *dst) {
	r->macro = dst;
	r->armed = 0;
	r->arming = 1;
	macro_free(dst);
}

int macro_rec_frame(MacroRecorder *r, uint32_t keys) {
	keys &= MACRO_KEY_MASK;
	if (r->arming) {
		r->armed = keys;
		r->arming = 0;
	}
	r->armed &= keys; /* soltar libera o botão para valer da próxima vez */
	return macro_add_step(r->macro, (uint16_t) (keys & ~r->armed), 1) == 0;
}

void macro_play_start(MacroPlayer *p, const Macro *m) {
	memset(p, 0, sizeof(*p));
	p->macro = m;
	p->active = 1;
	p->arming = 1;
	p->need_reload = m->from_state;
	p->left = m->count > 0 ? m->steps[0].frames : 0;
}

MacroPlayEvent macro_play_step(MacroPlayer *p, uint32_t user_keys, uint32_t *keys) {
	const Macro *m = p->macro;
	if (!p->active || p->finished || m->count == 0) {
		p->active = 0;
		*keys = user_keys;
		return MP_DONE;
	}
	if (p->arming) {
		p->armed = user_keys;
		p->arming = 0;
	} else {
		if (user_keys & ~p->armed) { /* um botão novo: o usuário quer a mão de volta */
			p->active = 0;
			*keys = user_keys;
			return MP_INTERRUPTED;
		}
		p->armed &= user_keys; /* soltou: o botão volta a poder interromper */
	}
	if (p->need_reload) {
		p->need_reload = 0;
		*keys = 0;
		return MP_RELOAD;
	}
	*keys = m->steps[p->step].keys;
	if (--p->left == 0) {
		if (++p->step == m->count) {
			p->step = 0;
			p->cycle++;
			if (m->mode == MACRO_ONCE || (m->mode == MACRO_COUNT && p->cycle >= m->repeat)) p->finished = 1;
			else if (m->from_state) p->need_reload = 1;
		}
		p->left = m->steps[p->step].frames;
	}
	return MP_RUNNING;
}

void macro_rec_end(MacroRecorder *r) {
	Macro *m = r->macro;
	int first = 0, last = m->count;
	while (first < last && m->steps[first].keys == 0) first++;
	while (last > first && m->steps[last - 1].keys == 0) last--;
	if (first > 0) memmove(m->steps, m->steps + first, (size_t) (last - first) * sizeof(MacroStep));
	m->count = last - first;
	m->total_frames = 0;
	for (int i = 0; i < m->count; i++) m->total_frames += m->steps[i].frames;
	if (m->count == 0) macro_free(m);
}
