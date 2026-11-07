#include "audio.h"

#include <stdio.h>
#include <SDL.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>

#include "volume.h"

#define SAMPLE_RATE 48000
#define CORE_BUFFER_SAMPLES 2048

static SDL_AudioDeviceID device;
static short samples[CORE_BUFFER_SAMPLES * 2];
static int gain_q8 = 256; /* 256 = volume original */

int audio_init(struct mCore *core) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "Erro ao iniciar o áudio: %s\n", SDL_GetError());
		return -1;
	}
	SDL_AudioSpec want = {0}, have;
	want.freq = SAMPLE_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!device) {
		fprintf(stderr, "Erro ao abrir o dispositivo de áudio: %s\n", SDL_GetError());
		return -1;
	}
	core->setAudioBufferSize(core, CORE_BUFFER_SAMPLES);
	blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), have.freq);
	blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), have.freq);
	SDL_PauseAudioDevice(device, 0);
	return 0;
}

unsigned audio_queued_bytes(void) {
	return device ? SDL_GetQueuedAudioSize(device) : 0;
}

void audio_push(struct mCore *core) {
	blip_t *left = core->getAudioChannel(core, 0);
	blip_t *right = core->getAudioChannel(core, 1);
	int available = blip_samples_avail(left);
	if (available > CORE_BUFFER_SAMPLES) available = CORE_BUFFER_SAMPLES;
	if (available <= 0) return;
	blip_read_samples(left, samples, available, 1);
	blip_read_samples(right, samples + 1, available, 1);
	volume_apply(samples, (size_t) available * 2, gain_q8);
	SDL_QueueAudio(device, samples, (Uint32) available * 2 * sizeof(short));
}

void audio_set_volume(int percent, int muted) {
	gain_q8 = volume_gain_q8(percent, muted);
}

void audio_discard(struct mCore *core) {
	blip_clear(core->getAudioChannel(core, 0));
	blip_clear(core->getAudioChannel(core, 1));
}

void audio_clear_queue(void) {
	if (device) SDL_ClearQueuedAudio(device);
}

void audio_shutdown(void) {
	if (device) SDL_CloseAudioDevice(device);
	device = 0;
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
