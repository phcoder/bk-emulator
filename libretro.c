#include "defines.h"
#include "libretro.h"
#include "conf.h"
#include "tty.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

static const char *const focal10rom = "FOCAL10.ROM";
static const char *const basic10rom = "BASIC10.ROM"; 

retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;
struct retro_vfs_interface *vfs_interface;

static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static int joystick_enabled;

float frame_time = 0;
int breakpoint = -1;
extern unsigned short framebuf[512][512];
static int joystick_cur_state = 0;

void platform_joystick_init() {
}

d_word platform_joystick_get_state() {
  return joystick_cur_state;
}


static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list va;

   (void)level;

   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)
{
}

void retro_deinit(void)
{
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "bk";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version  = "v1.0" GIT_VERSION;
   info->need_fullpath    = false;
   info->valid_extensions = NULL; /* Anything is fine, we don't care. */
}

#define FPS 25
#define SAMPLE_RATE io_sound_freq

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = FPS;
   info->timing.sample_rate = SAMPLE_RATE;

   info->geometry.base_width   = 512;
   info->geometry.base_height  = 512;
   info->geometry.max_width    = 512;
   info->geometry.max_height   = 512;
   info->geometry.aspect_ratio = 1.0;
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback logging;
   bool no_rom = true;

   environ_cb = cb;

   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   struct retro_variable variables[] =
   {
      {
	      "bk_model",
	      "Model; BK-0010|BK-0010.01|BK-0010.01 + FDD|BK-0011M + FDD|Terak 8510/a|Slow BK-0011M",
      },
      {
	      "bk_peripheral",
	      "Peripheral (UP port); none|covox|ay_3_8910|mouse_high|mouse_low|joystick",
      },
      {
	      "bk_layout",
	      "Keyboard layout; qwerty|jcuken",
      },
      {
	      "bk_doublespeed",
	      "Double CPU speed; disabled|enabled",
      },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

   struct retro_vfs_interface_info vfs_interface_info;
   vfs_interface_info.required_interface_version = 1;
   vfs_interface_info.iface = NULL;
   if (cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info))
     vfs_interface = vfs_interface_info.iface;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
}

#define MAX_SAMPLES_PER_FRAME 5000
static const int16_t zero_samples[MAX_SAMPLES_PER_FRAME * 2];

static const void * game_data;
static size_t game_size;
static int hasgame = 0;

void retro_run(void)
{
	if (hasgame == 1 && framectr > 2)
	{
		hasgame = 0;
                load_and_run_bin(game_data, game_size);
	}

	if (mouseflag) {
		relx += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
		rely += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
		mouse_button_state = mouse_but0 * (
			(input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
					RETRO_DEVICE_ID_MOUSE_LEFT) ? 1 : 0)
			| (input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
					  RETRO_DEVICE_ID_MOUSE_RIGHT) ? 2 : 0));
	}

	if (joystick_enabled) {
		int new_state = 0;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) << 1;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) << 2;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) << 3;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) << 4;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) << 5;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) << 9;
		new_state |= !!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) << 10;
		joystick_cur_state = new_state;
	}

  run_cpu_until(&pdp, ++framectr * TICK_RATE/FPS);

  scr_flush();

  video_cb(framebuf, 512, 512, 1024);
  if (!nflag) {
	  int samplegoal = SAMPLE_RATE / FPS;
	  if (samplegoal > MAX_SAMPLES_PER_FRAME)
		  samplegoal = MAX_SAMPLES_PER_FRAME;
	  audio_batch_cb(zero_samples, samplegoal);
  }

  input_poll_cb();

  /*   key_state_t ks;

   ks.up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   ks.right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   ks.down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   ks.left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   ks.start = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   ks.select = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);

   game_update(frame_time, &ks);
   game_render();*/
}

int game_init_pixelformat(void)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
      return 0;
   }

   return 1;
}

bool retro_load_game(const struct retro_game_info *info)
{
  cflag = 1;
  //   struct retro_frame_time_callback frame_cb;
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "3" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "4" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

	if (info && info->data) {
                game_data = malloc(info->size);
                memcpy (game_data, info->data, info->size);
                game_size = info->size;
		hasgame = 1;
	}

        if (info && info->path) {
                char *slash = strrchr(info->path, '/');
                if (slash) {
                        tape_prefix = strdup(info->path);
                        tape_prefix[slash - info->path + 1] = '\0';
                }
        }

	const char *dir;
//	init_config();

	nflag = 1;		/* enable sound */
	/* nothing is connected to the port by default, use ~/.bkrc */

	/* Set ROM configuration */

	if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
	{
		romdir = strdup(dir);
	}

	struct retro_variable var;
	
	var.key = "bk_model";
	var.value = NULL;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (strcmp (var.value, "BK-0010") == 0)
			bkmodel = 0;
		else if (strcmp (var.value, "BK-0010.01") == 0)
			bkmodel = 1;
		else if (strcmp (var.value, "BK-0010.01 + FDD") == 0)
			bkmodel = 2;
		else if (strcmp (var.value, "BK-0011M + FDD") == 0)
			bkmodel = 3;
		else if (strcmp (var.value, "Slow BK-0011M") == 0)
			bkmodel = 4;
		else if (strcmp (var.value, "Terak 8510/a") == 0) {
			bkmodel = 9;
			// Terak has no sound yet, turn sound off
			nflag = 0;
		} else
			bkmodel = 3;
	} else
		bkmodel = 3;

	switch( bkmodel ) {
	case 0: /* BK0010 */
		rompath10 = monitor10rom;
		rompath12 = focal10rom;
		rompath16 = 0;
		TICK_RATE = 3000000;
		break;
	case 1: /* BK0010.01 */
		rompath10 = monitor10rom;
		rompath12 = basic10rom;
		rompath16 = 0;
		TICK_RATE = 3000000;
		break;
	case 2: /* BK0010.01+FDD */
		rompath10 = monitor10rom;
		rompath12 = 0;
		rompath16 = diskrom;
		TICK_RATE = 3000000;
		break;
	case 3:	/* BK-0011M */
	case 9: /* Terak 8510/a */
		rompath10 = rompath12 = rompath16 = 0;
		TICK_RATE = 4000000;
		break;
	case 4: /* Slow BK-0011M */
		rompath10 = rompath12 = rompath16 = 0;
		TICK_RATE = 3000000;
		break;
	default: /* Unknown ROM configuration */
	  log_cb(RETRO_LOG_ERROR, "Unknown BK model. Bailing out.\n");
		exit( -1 );
	}

	var.key = "bk_doublespeed";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL && strcmp (var.value, "enabled") == 0) {
		TICK_RATE *= 2;
	}

	/* Convert BK model to 0010/0011 flag */
	fake_disk &= bkmodel >= 2;
	terak = bkmodel == 9;
	bkmodel = bkmodel >= 3;
	tty_open();             /* initialize the tty stuff */
	ev_init();		/* initialize the event system */
	sim_init();		/* ...the simulated cpu */
	mem_init();		/* ...main memory */
	bk_scr_init();		/* video display */
	boot_init();		/* ROM blocks */
	q_reset();             /* ...any devices */

	mouseflag = 0;
	joystick_enabled = 0;
	if (!terak) {
		var.key = "bk_peripheral";
		var.value = NULL;
		if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value == NULL) {
			var.value = "none";
		}
		if (strcmp (var.value, "covox") == 0)		
			plug_covox();
		if (strcmp(var.value, "ay_3_8910") == 0)
			plug_synth();
		if (strcmp(var.value, "mouse_low") == 0)
			mouseflag = 1;
		if (strcmp(var.value, "mouse_high") == 0)
			mouseflag = 2;
		if (mouseflag) {
			mouse_init();
			plug_mouse();
		}
		if (strcmp(var.value, "joystick") == 0) {
		       joystick_enabled = 1;
		       plug_joystick();
		}
	}

	/* Starting frame rate */ 
	frame_delay = TICK_RATE/25;
	half_frame_delay = TICK_RATE/50;

	if (terak) {
		pdp.regs[PC] = 0173000;
	} else {
		lc_word(0177716, &pdp.regs[PC]);
		pdp.regs[PC] &= 0177400;
	}

	if (!game_init_pixelformat())
		return false;

	/*   frame_cb.callback  = frame_time_cb;
   frame_cb.reference = 1000000 / 25;
   frame_cb.callback(frame_cb.reference);
   environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_cb);*/

   (void)info;
   return true;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
	return sizeof (current_state);
}

bool retro_serialize(void *data_, size_t size)
{
	memcpy(data_, &current_state, sizeof (current_state));
	return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
	if (size < sizeof (current_state))
		return false;

	memcpy(&current_state, data_, sizeof (current_state));
	scr_dirty = 1;
	return true;
}

void *retro_get_memory_data(unsigned id)
{
	return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

void platform_sound_flush() {
}

void sound_write_sample(short val) {
	soundctr++;
	audio_cb (val, val);
}

void sound_discard() {
}

void platform_sound_init() {
}

void platform_disk_init(disk_t *disks) {
}


void *load_rom_file(const char * rompath, size_t *sz, size_t min_sz, size_t max_sz)
{
	FILE * romf;

	char *path = malloc(strlen(romdir)+strlen(rompath)+2);

	if (!path) {
		log_cb(RETRO_LOG_ERROR, "No memory");
		environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
		return NULL;
	}

	/* If rompath is a real path, do not apply romdir to it */
	if (*romdir && !strchr(rompath, '/'))
		sprintf(path, "%s/%s", romdir, rompath);
	else
		strcpy(path, rompath);

	log_cb(RETRO_LOG_INFO, "Loading %s...", path);

	char *ret = NULL;

	if (vfs_interface)
	{
		struct retro_vfs_file_handle *romf = vfs_interface->open(
			path,
			RETRO_VFS_FILE_ACCESS_READ,
			RETRO_VFS_FILE_ACCESS_HINT_NONE);
		
		if (!romf) {
			log_cb(RETRO_LOG_ERROR, "Couldn't open file.\n");
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
			return NULL;
		}

		size_t fsz = vfs_interface->size(romf);
		if (fsz > max_sz)
			fsz = max_sz;

		if (fsz < min_sz) {
			log_cb(RETRO_LOG_ERROR, "Incomplete or damaged file.\n");
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
			return NULL;
		}

		char *ret = malloc (fsz + 1);
		vfs_interface->read(romf, ret, fsz);
		vfs_interface->close(romf);
		
		*sz = fsz;

		free(path);

		return ret;
	} else {
		FILE * romf = fopen(path, "r");
		if (!romf) {
			log_cb(RETRO_LOG_ERROR, "Couldn't open file.\n");
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
			return NULL;
		}

		char *ret = malloc (max_sz);
		int c, i;

		while ((c = fgetc(romf)) >= 0)
			ret[i++] = c;

		fclose(romf);

		if (i < min_sz) {
			log_cb(RETRO_LOG_ERROR, "Incomplete or damaged file.\n");
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
			return NULL;
		}

		*sz = i;

		free(path);

		return ret;
	}
}

struct libretro_handle
{
	FILE *stdio;
	struct retro_vfs_file_handle *lr;
};

struct libretro_handle *
libretro_vfs_open(const char *filename, const char *mode)
{
	if (!vfs_interface) {
		FILE *f = fopen(filename, mode);
		if (!f)
			return NULL;
		struct libretro_handle *ret = malloc(sizeof(*ret));
		assert(ret != NULL);
		ret->stdio = f;
		ret->lr = NULL;
		return ret;
	}

	assert((mode[0] == 'r' || mode[0] == 'w') && mode[1] == '\0');

	struct retro_vfs_file_handle *lr =
		vfs_interface->open(filename, mode[0] == 'r'
				    ? RETRO_VFS_FILE_ACCESS_READ
				    : RETRO_VFS_FILE_ACCESS_WRITE,
				    RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!lr)
		return NULL;
	struct libretro_handle *ret = malloc(sizeof(*ret));
	assert(ret != NULL);
	ret->stdio = NULL;
	ret->lr = lr;
	return ret;	
}

void libretro_vfs_close(struct libretro_handle *h)
{
	if (h->lr)
		vfs_interface->close(h->lr);
	if(h->stdio)
		fclose(h->stdio);
	free (h);
}

int libretro_vfs_getc(struct libretro_handle *h)
{
	if (h->lr) {
		unsigned char c = 0;
		int r;
		r = vfs_interface->read(h->lr, &c, 1);
		if (r != 1)
			return -1;
		return c;
	}

	return fgetc(h->stdio);
}

void libretro_vfs_putc(int c, struct libretro_handle *h)
{
	if (h->lr) {
		unsigned char c0 = c;
		vfs_interface->write(h->lr, &c0, 1);
		return;
	}

	fputc(c, h->stdio);
}

void libretro_vfs_flush(struct libretro_handle *h)
{
	if (h->lr) {
		vfs_interface->flush(h->lr);
		return;
	}

	fflush(h->stdio);
}
