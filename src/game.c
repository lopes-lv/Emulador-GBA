#include "game.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <mgba-util/vfs.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/serialize.h>

#include "audio.h"
#include "save.h"

#define STATE_SLOT 1

struct Game {
	struct mCore *core;
	uint32_t *pixels;
	char state_path[1024];
	char sav_path[1024];
};

static void fail(char *err, size_t err_size, const char *fmt, const char *rom) {
	if (err && err_size) snprintf(err, err_size, fmt, rom);
}

Game *game_open(const char *rom, char *err, size_t err_size) {
	Game *g = calloc(1, sizeof(*g));
	if (!g) {
		fail(err, err_size, "Erro: sem memória para abrir '%s'.", rom);
		return NULL;
	}
	if (save_sibling_path(rom, ".ss1", g->state_path, sizeof(g->state_path)) != 0 ||
	    save_sibling_path(rom, ".sav", g->sav_path, sizeof(g->sav_path)) != 0) {
		fail(err, err_size, "Erro: caminho da ROM longo demais: '%s'.", rom);
		free(g);
		return NULL;
	}

	g->core = mCoreFind(rom);
	if (!g->core) {
		fail(err, err_size, "Erro: não é uma ROM de GBA válida ou não existe: '%s'.", rom);
		free(g);
		return NULL;
	}
	g->core->init(g->core);
	mCoreInitConfig(g->core, "gba-frontend");

	unsigned width, height;
	g->core->desiredVideoDimensions(g->core, &width, &height);
	g->pixels = calloc((size_t) width * height, sizeof(uint32_t));
	if (!g->pixels) {
		fail(err, err_size, "Erro: sem memória para abrir '%s'.", rom);
		g->core->deinit(g->core);
		free(g);
		return NULL;
	}
	g->core->setVideoBuffer(g->core, g->pixels, width);

	if (!mCoreLoadFile(g->core, rom)) {
		fail(err, err_size, "Erro: não foi possível carregar a ROM: '%s'.", rom);
		goto fail_core;
	}
	if (!mCoreAutoloadSave(g->core)) {
		fail(err, err_size, "Erro: não foi possível criar o save ao lado da ROM: '%s'.", rom);
		goto fail_core;
	}
	g->core->reset(g->core);

	if (audio_init(g->core) != 0) {
		fail(err, err_size, "Erro: não foi possível iniciar o áudio para a ROM: '%s'.", rom);
		goto fail_core;
	}
	printf("Save em %s | Estado (F5/F8) em %s\n", g->sav_path, g->state_path);
	return g;

fail_core:
	g->core->deinit(g->core);
	free(g->pixels);
	free(g);
	return NULL;
}

void game_set_keys(Game *g, uint32_t keys) {
	g->core->setKeys(g->core, keys);
}

void game_run_frames(Game *g, int frames, int with_audio) {
	for (int i = 0; i < frames; i++) {
		g->core->runFrame(g->core);
		if (with_audio) audio_push(g->core); else audio_discard(g->core);
	}
}

void game_run_frame(Game *g) {
	game_run_frames(g, 1, 1);
}

const uint32_t *game_pixels(const Game *g) {
	return g->pixels;
}

int game_save_state(Game *g) {
	return mCoreSaveState(g->core, STATE_SLOT, SAVESTATE_ALL) ? 1 : 0;
}

int game_load_state(Game *g) {
	/* Sem SAVESTATE_SAVEDATA: carregar o estado não pode sobrescrever o .sav do jogo. */
	return mCoreLoadState(g->core, STATE_SLOT, SAVESTATE_SCREENSHOT | SAVESTATE_RTC) ? 1 : 0;
}

int game_save_state_to(Game *g, const char *path_utf8) {
	/* O_RDWR como o próprio mGBA usa para estados: ele grava o estado por mapeamento em memória. */
	struct VFile *vf = VFileOpen(path_utf8, O_CREAT | O_TRUNC | O_RDWR);
	if (!vf) return 0;
	bool ok = mCoreSaveStateNamed(g->core, vf, SAVESTATE_ALL);
	vf->close(vf);
	return ok ? 1 : 0;
}

int game_load_state_from(Game *g, const char *path_utf8) {
	struct VFile *vf = VFileOpen(path_utf8, O_RDONLY);
	if (!vf) return 0;
	/* Mesmos flags de game_load_state: sem SAVESTATE_SAVEDATA, para não sobrescrever o .sav. */
	bool ok = mCoreLoadStateNamed(g->core, vf, SAVESTATE_SCREENSHOT | SAVESTATE_RTC);
	vf->close(vf);
	return ok ? 1 : 0;
}

const char *game_state_path(const Game *g) {
	return g->state_path;
}

const char *game_sav_path(const Game *g) {
	return g->sav_path;
}

void game_close(Game *g) {
	if (!g) return;
	audio_shutdown();
	g->core->deinit(g->core);
	free(g->pixels);
	free(g);
}
