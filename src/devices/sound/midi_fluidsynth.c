/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Interface to FluidSynth MIDI.
 *
 *		Note that on Windows, the fluidsynth DLL is not normally
 *		installed, and it can be a pain to find it. Many people
 *		have reported that the files offered on the:
 *
 *		  https://zdoom.org/wiki/FluidSynth
 *
 *		website (for 32bit and 64bit Windows) are working, and
 *		need no additional support files other than sound fonts.
 *
 * Version:	@(#)midi_fluidsynth.c	1.0.19	2020/07/17
 *
 *		Code borrowed from scummvm.
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <fluidsynth.h>
#define dbglog sound_midi_log
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "midi.h"
#include "sound.h"


#ifdef USE_FLUIDSYNTH


#define RENDER_RATE 100
#define BUFFER_SEGMENTS 10

#ifdef _WIN32
# define PATH_FS_DLL	"libfluidsynth.dll"
#else
# define PATH_FS_DLL	"libfluidsynth.so"
#endif


static void	*fluidsynth_handle = NULL;	/* handle to FluidSynth DLL */

/* Pointers to the real functions. */
static fluid_settings_t*(*f_new_fluid_settings)(void);
static void (*f_delete_fluid_settings)(fluid_settings_t *settings);
static int (*f_fluid_settings_setnum)(fluid_settings_t *settings, const char *name, double val);
static int (*f_fluid_settings_getnum)(fluid_settings_t *settings, const char *name, double *val);
static fluid_synth_t *(*f_new_fluid_synth)(fluid_settings_t *settings);
static int (*f_delete_fluid_synth)(fluid_synth_t *synth);
static int (*f_fluid_synth_noteon)(fluid_synth_t *synth, int chan, int key, int vel);
static int (*f_fluid_synth_noteoff)(fluid_synth_t *synth, int chan, int key);
static int (*f_fluid_synth_cc)(fluid_synth_t *synth, int chan, int ctrl, int val);
static int (*f_fluid_synth_sysex)(fluid_synth_t *synth, const char *data, int len, char *response, int *response_len, int *handled, int dryrun);
static int (*f_fluid_synth_pitch_bend)(fluid_synth_t *synth, int chan, int val);
static int (*f_fluid_synth_program_change)(fluid_synth_t *synth, int chan, int program);
static int (*f_fluid_synth_sfload)(fluid_synth_t *synth, const char *filename, int reset_presets);
static int (*f_fluid_synth_set_interp_method)(fluid_synth_t *synth, int chan, int interp_method);
static void (*f_fluid_synth_set_reverb)(fluid_synth_t *synth, double roomsize, double damping, double width, double level);
static void (*f_fluid_synth_set_reverb_on)(fluid_synth_t *synth, int on);
static void (*f_fluid_synth_set_chorus)(fluid_synth_t *synth, int nr, double level, double speed, double depth_ms, int type);
static void (*f_fluid_synth_set_chorus_on)(fluid_synth_t *synth, int on);
static int (*f_fluid_synth_write_s16)(fluid_synth_t *synth, int len, void *lout, int loff, int lincr, void *rout, int roff, int rincr);
static int (*f_fluid_synth_write_float)(fluid_synth_t *synth, int len, void *lout, int loff, int lincr, void *rout, int roff, int rincr);
static char *(*f_fluid_version_str)(void);

static const dllimp_t imports[] = {
  { "new_fluid_settings",		&f_new_fluid_settings		},
  { "delete_fluid_settings",		&f_delete_fluid_settings	},
  { "fluid_settings_setnum",		&f_fluid_settings_setnum	},
  { "fluid_settings_getnum",		&f_fluid_settings_getnum	},
  { "new_fluid_synth",			&f_new_fluid_synth		},
  { "delete_fluid_synth",		&f_delete_fluid_synth		},
  { "fluid_synth_noteon",		&f_fluid_synth_noteon		},
  { "fluid_synth_noteoff",		&f_fluid_synth_noteoff		},
  { "fluid_synth_cc",			&f_fluid_synth_cc		},
  { "fluid_synth_sysex",		&f_fluid_synth_sysex		},
  { "fluid_synth_pitch_bend",		&f_fluid_synth_pitch_bend	},
  { "fluid_synth_program_change",	&f_fluid_synth_program_change	},
  { "fluid_synth_sfload",		&f_fluid_synth_sfload		},
  { "fluid_synth_set_interp_method",	&f_fluid_synth_set_interp_method},
  { "fluid_synth_set_reverb",		&f_fluid_synth_set_reverb	},
  { "fluid_synth_set_reverb_on",	&f_fluid_synth_set_reverb_on	},
  { "fluid_synth_set_chorus",		&f_fluid_synth_set_chorus	},
  { "fluid_synth_set_chorus_on",	&f_fluid_synth_set_chorus_on	},
  { "fluid_synth_write_s16",		&f_fluid_synth_write_s16	},
  { "fluid_synth_write_float",		&f_fluid_synth_write_float	},
  { "fluid_version_str",		&f_fluid_version_str		},
  { NULL,				NULL				}
};


typedef struct fluidsynth {
    fluid_settings_t	*settings;
    fluid_synth_t	*synth;
    int			samplerate;
    int			sound_font;

    thread_t		*thread_h;
    event_t		*event, *start_event;

    int			buf_size;
    float		*buffer;
    int16_t		*buffer_int16;
    int			midi_pos;
    volatile int	on;
} fluidsynth_t;


fluidsynth_t fsdev;


static int
fluidsynth_available(void)
{
    return((fluidsynth_handle != NULL)?1:0);
}


static void
fluidsynth_poll(void)
{
    fluidsynth_t* data = &fsdev;

    data->midi_pos++;
    if (data->midi_pos == 48000/RENDER_RATE) {
	data->midi_pos = 0;
	thread_set_event(data->event);
    }
}


static void
fluidsynth_thread(void *param)
{
    fluidsynth_t* data = (fluidsynth_t*)param;
    int buf_pos = 0;
    int buf_size = data->buf_size / BUFFER_SEGMENTS;

    thread_set_event(data->start_event);

    while (data->on) {
	thread_wait_event(data->event, -1);
	thread_reset_event(data->event);

	if (config.sound_is_float) {
		float *buf = (float*)((uint8_t*)data->buffer + buf_pos);
		memset(buf, 0, buf_size);
		if (data->synth)
			f_fluid_synth_write_float(data->synth, buf_size/(2 * sizeof(float)), buf, 0, 2, buf, 1, 2);
		buf_pos += buf_size;
		if (buf_pos >= data->buf_size) {
			openal_buffer_midi(data->buffer, data->buf_size / sizeof(float));
			buf_pos = 0;
		}
	} else {
		int16_t *buf = (int16_t*)((uint8_t*)data->buffer_int16 + buf_pos);
		memset(buf, 0, buf_size);
		if (data->synth)
			f_fluid_synth_write_s16(data->synth, buf_size/(2 * sizeof(int16_t)), buf, 0, 2, buf, 1, 2);
		buf_pos += buf_size;
		if (buf_pos >= data->buf_size) {
			openal_buffer_midi(data->buffer_int16, data->buf_size / sizeof(int16_t));
			buf_pos = 0;
		}
	}
    }
}


void
fluidsynth_msg(uint8_t *msg)
{
    fluidsynth_t* data = &fsdev;
    uint32_t val = *((uint32_t*)msg);
    uint32_t param2 = (uint8_t) ((val >> 16) & 0xFF);
    uint32_t param1 = (uint8_t) ((val >>  8) & 0xFF);
    uint8_t cmd    = (uint8_t) (val & 0xF0);
    uint8_t chan   = (uint8_t) (val & 0x0F);

    switch (cmd) {
	case 0x80:      /* Note Off */
		f_fluid_synth_noteoff(data->synth, chan, param1);
		break;

	case 0x90:      /* Note On */
		f_fluid_synth_noteon(data->synth, chan, param1, param2);
		break;

	case 0xA0:      /* Aftertouch */
		break;

	case 0xB0:      /* Control Change */
		f_fluid_synth_cc(data->synth, chan, param1, param2);
		break;

	case 0xC0:      /* Program Change */
		f_fluid_synth_program_change(data->synth, chan, param1);
		break;

	case 0xD0:      /* Channel Pressure */
		break;

	case 0xE0:      /* Pitch Bend */
		f_fluid_synth_pitch_bend(data->synth, chan, (param2 << 7) | param1);
		break;

	case 0xF0:      /* SysEx */
		break;

	default:
		DEBUG("fluidsynth: unknown send() command 0x%02X", cmd);
		break;
	}
}


void
fluidsynth_sysex(uint8_t *data, unsigned int len)
{
    fluidsynth_t* d = &fsdev;

    f_fluid_synth_sysex(d->synth, (const char *)data, len, 0, 0, 0, 0);
}


static void *
fluidsynth_init(const device_t *info, UNUSED(void *parent))
{
    fluidsynth_t *data = &fsdev;
    midi_device_t* dev;

    memset(data, 0x00, sizeof(fluidsynth_t));

    data->settings = f_new_fluid_settings();

    f_fluid_settings_setnum(data->settings, "synth.sample-rate", 44100);
    f_fluid_settings_setnum(data->settings, "synth.gain", device_get_config_int("output_gain")/100.0f);

    data->synth = f_new_fluid_synth(data->settings);

    const char *sound_font = device_get_config_string("sound_font");
    data->sound_font = f_fluid_synth_sfload(data->synth, sound_font, 1);

    if (device_get_config_int("chorus")) {
	f_fluid_synth_set_chorus_on(data->synth, 1);

	int chorus_voices = device_get_config_int("chorus_voices");
	double chorus_level = device_get_config_int("chorus_level") / 100.0;
	double chorus_speed = device_get_config_int("chorus_speed") / 100.0;
	double chorus_depth = device_get_config_int("chorus_depth") / 10.0;

	int chorus_waveform = FLUID_CHORUS_MOD_SINE;
	if (device_get_config_int("chorus_waveform") == 0)
		chorus_waveform = FLUID_CHORUS_MOD_SINE;
	  else
		chorus_waveform = FLUID_CHORUS_MOD_TRIANGLE;

	f_fluid_synth_set_chorus(data->synth, chorus_voices, chorus_level, chorus_speed, chorus_depth, chorus_waveform);
    } else
	f_fluid_synth_set_chorus_on(data->synth, 0);

    if (device_get_config_int("reverb")) {
	f_fluid_synth_set_reverb_on(data->synth, 1);

	double reverb_room_size = device_get_config_int("reverb_room_size") / 100.0;
	double reverb_damping = device_get_config_int("reverb_damping") / 100.0;
	int reverb_width = device_get_config_int("reverb_width");
	double reverb_level = device_get_config_int("reverb_level") / 100.0;

	f_fluid_synth_set_reverb(data->synth, reverb_room_size, reverb_damping, reverb_width, reverb_level);
    } else
	f_fluid_synth_set_reverb_on(data->synth, 0);

    int interpolation = device_get_config_int("interpolation");
    int fs_interpolation = FLUID_INTERP_4THORDER;

    if (interpolation == 0)
	fs_interpolation = FLUID_INTERP_NONE;
      else if (interpolation == 1)
	fs_interpolation = FLUID_INTERP_LINEAR;
      else if (interpolation == 2)
	fs_interpolation = FLUID_INTERP_4THORDER;
      else if (interpolation == 3)
	fs_interpolation = FLUID_INTERP_7THORDER;

    f_fluid_synth_set_interp_method(data->synth, -1, fs_interpolation);

    double samplerate;
    f_fluid_settings_getnum(data->settings, "synth.sample-rate", &samplerate);
    data->samplerate = (int)samplerate;
    if (config.sound_is_float) {
	data->buf_size = (data->samplerate/RENDER_RATE)*2*sizeof(float)*BUFFER_SEGMENTS;
	data->buffer = (float *)mem_alloc(data->buf_size);
	data->buffer_int16 = NULL;
    } else {
	data->buf_size = (data->samplerate/RENDER_RATE)*2*sizeof(int16_t)*BUFFER_SEGMENTS;
	data->buffer = NULL;
	data->buffer_int16 = (int16_t *)mem_alloc(data->buf_size);
    }

    openal_set_midi(data->samplerate, data->buf_size);

    DEBUG("fluidsynth (%s) initialized, samplerate %d, buf_size %d\n",
	  f_fluid_version_str(), data->samplerate, data->buf_size);

    dev = (midi_device_t *)mem_alloc(sizeof(midi_device_t));
    memset(dev, 0, sizeof(midi_device_t));

    dev->play_msg = fluidsynth_msg;
    dev->play_sysex = fluidsynth_sysex;
    dev->poll = fluidsynth_poll;

    midi_init(dev);

    data->on = 1;

    data->start_event = thread_create_event();
    data->event = thread_create_event();

    data->thread_h = thread_create(fluidsynth_thread, data);

    thread_wait_event(data->start_event, -1);
    thread_reset_event(data->start_event);

    return(dev);
}


static void
fluidsynth_close(void* priv)
{
    if (priv == NULL) return;
    if (fluidsynth_handle == NULL) return;

    fluidsynth_t* data = &fsdev;

    data->on = 0;
    thread_set_event(data->event);
    thread_wait(data->thread_h, -1);

    if (data->synth) {
	f_delete_fluid_synth(data->synth);
	data->synth = NULL;
    }

    if (data->settings) {
	f_delete_fluid_settings(data->settings);
	data->settings = NULL;
    }

    if (data->buffer) {
	free(data->buffer);
	data->buffer = NULL;
    }

    if (data->buffer_int16) {
	free(data->buffer_int16);
	data->buffer_int16 = NULL;
    }

    /* Unload the DLL if possible. */
    if (fluidsynth_handle != NULL) {
	dynld_close(fluidsynth_handle);
	fluidsynth_handle = NULL;
    }
}


void
fluidsynth_global_init(void)
{
    wchar_t temp[512];
    const char *fn = PATH_FS_DLL;

    /* Try loading the DLL. */
    fluidsynth_handle = dynld_module(fn, imports);
    if (fluidsynth_handle == NULL) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOLIB), "FluidSynth", fn);
	ui_msgbox(MBX_ERROR, temp);
	ERRLOG("SOUND: unable to load '%s', FluidSynth not available!\n", fn);
    } else {
	INFO("SOUND: module '%s' loaded, version %s.\n",
				fn, f_fluid_version_str());
    }
}


static const device_config_t fluidsynth_config[] = {
#if 0
    {
	"sound_font","Sound Font",CONFIG_FNAME,"",0,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		{
			"SF2 Sound Fonts","sf2"
		},
		{
			NULL
		}
	}
    },
    {
	"output_gain","Output Gain",CONFIG_SPINNER,"",100,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 100, 1
	}
    },
#endif
    {
	"chorus","Chorus",CONFIG_BINARY,"",0
    },
#if 0
    {
	"chorus_voices","Chorus Voices",CONFIG_SPINNER,"",3,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 99, 1
	}
    },
    {
	"chorus_level","Chorus Level",CONFIG_SPINNER,"",100,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 100, 1
	}
    },
    {
	"chorus_speed","Chorus Speed",CONFIG_SPINNER,"",30,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		30, 500, 10
	}
    },
    {
	"chorus_depth","Chorus Depth",CONFIG_SPINNER,"",80,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 210, 10
	}
    },
#endif
    {
	"chorus_waveform","Chorus Waveform",CONFIG_SELECTION,"",0,
	{
		{
		       "Sine",0
		},
		{
			"Triangle",1
		},
		{
			NULL
		}
	}
    },
    {
	"reverb","Reverb",CONFIG_BINARY,"",0
    },
#if 0
    {
	"reverb_room_size","Reverb Room Size",CONFIG_SPINNER,"",20,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 120, 1
	}
    },
    {
	"reverb_damping","Reverb Damping",CONFIG_SPINNER,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 100, 1
	}
    },
    {
	"reverb_width","Reverb Width",CONFIG_SPINNER,"",1,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 100, 1
	}
    },
    {
	"reverb_level","Reverb Level",CONFIG_SPINNER,"",90,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{
		0, 100, 1
	}
    },
#endif
    {
	"interpolation","Interpolation Method",CONFIG_SELECTION,"",2,
	{
		{
			"None",0
		},
		{
			"Linear",1
		},
		{
			"4th Order",2
		},
		{
			"7th Order",3
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


const device_t fluidsynth_device = {
    "FluidSynth",
    0, 0, NULL,
    fluidsynth_init, fluidsynth_close, NULL,
    fluidsynth_available,
    NULL,
    NULL,
    NULL,
    fluidsynth_config
};


#endif	/*USE_FLUIDSYNTH*/
