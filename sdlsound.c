#include "defines.h"
#include "conf.h"
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_mutex.h>
#include <libintl.h>
#define _(String) gettext (String)

//#define SOUND_EXPONENT	(8+io_sound_freq/20000)
//#define SOUND_BUFSIZE   (1<<SOUND_EXPONENT)		/* about 1/43 sec */
#define SOUND_BUFSIZE 512

#define NUMBUF 2

typedef struct {
	short * buf;
	unsigned int ptr;
} sound_buf_t;

sound_buf_t sound_buf[NUMBUF];
unsigned io_sound_bufsize;

int cur_buf;

static SDL_sem * sem;

static void callback(void * dummy, Uint8 * outbuf, int len)
{
	int i;
	static int cur_out_buf;
	if (SDL_SemValue(sem) == NUMBUF) {
		// Underflow: TODO fill the buffer with silence
		// fprintf(stderr, "!");
		return;
	}
	memcpy(outbuf, sound_buf[cur_out_buf].buf, len);
	cur_out_buf = (cur_out_buf + 1) % NUMBUF;
	SDL_SemPost(sem);
}

static void sound_finish() {
	/* release the write thread so it can terminate */
	SDL_PauseAudio(1); 
	SDL_DestroySemaphore(sem);
}

static SDL_AudioSpec desired;

void platform_sound_flush() {
	if (sound_buf[cur_buf].ptr != 0) {
		SDL_SemWait(sem);
		sound_buf[cur_buf].ptr = 0;
		cur_buf = (cur_buf + 1) % NUMBUF;
	}
}

void sound_write_sample(short val) {
	short * p = &sound_buf[cur_buf].buf[sound_buf[cur_buf].ptr++];
	if (sound_buf[cur_buf].buf == NULL)
	  return;
	*p = val;
	if (io_sound_bufsize == sound_buf[cur_buf].ptr) {
		platform_sound_flush();
	}
}

void sound_discard() {
	sound_buf[cur_buf].ptr = 0;
}

void platform_sound_init() {
	int iarg, i;

	if (fullspeed) {
		io_max_sound_age = 2 * SOUND_BUFSIZE;
		/* otherwise UINT_MAX */
	}

	fprintf(stderr, _("sound_init called\n"));

	if (-1 == SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		fprintf(stderr, _("Failed to initialize audio subsystem\n"));
	}

	desired.format = 16;
	desired.channels = 1;
	desired.freq = io_sound_freq;
	desired.samples = io_sound_bufsize = SOUND_BUFSIZE;
	desired.callback = callback;
	if (-1 == SDL_OpenAudio(&desired, 0)) {
		fprintf(stderr, _("Failed to initialize sound, freq %d, %d samples\n"), io_sound_freq, SOUND_BUFSIZE);
		nflag = 0;
		return;
	}

	sem = SDL_CreateSemaphore(NUMBUF);

	for (i = 0; i < NUMBUF; i++) {
		sound_buf[i].ptr = 0;
		sound_buf[i].buf = malloc(io_sound_bufsize * sizeof(short));
	}
	if (!sound_buf[NUMBUF-1].buf) {
		fprintf(stderr, _("Failed to allocate sound buffers\n"));
		exit(1);
	}

	atexit(sound_finish);
	SDL_PauseAudio(0);
}
