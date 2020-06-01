/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Interface to the OpenAL sound processing library.
 *
 * Version:	@(#)openal.c	1.0.19	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#ifdef USE_OPENAL
# undef AL_API
# undef ALC_API
# define AL_LIBTYPE_STATIC
# define ALC_LIBTYPE_STATIC
# include <AL/al.h>
# include <AL/alc.h>
# include <AL/alext.h>
#endif
#define dbglog sound_log
#include "../../emu.h"
#include "../../config.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "sound.h"
#include "midi.h"


#define FREQ	48000
#define BUFLEN	SOUNDBUFLEN

#ifdef _WIN32
# define PATH_AL_DLL	"libopenal-1.dll"
#else
# define PATH_AL_DLL	"libopenal.so.1"
#endif


#ifdef USE_OPENAL
static ALuint		buffers[4],		/* front and back buffers */
			buffers_cd[4],		/* front and back buffers */
			buffers_midi[4],	/* front and back buffers */
			source[3];		/* audio source */
static int		nbuffers,
			nsources;
static void		*openal_handle = NULL;	/* handle to (open) DLL */

/* Pointers to the real functions. */
static ALC_API ALCdevice* (ALC_APIENTRY *f_alcOpenDevice)(const ALCchar *devicename);
static ALC_API ALCboolean (ALC_APIENTRY *f_alcCloseDevice)(ALCdevice *device);
static ALC_API ALCcontext* (ALC_APIENTRY *f_alcCreateContext)(ALCdevice *device, const ALCint* attrlist);
static ALC_API void        (ALC_APIENTRY *f_alcDestroyContext)(ALCcontext *context);
static ALC_API ALCboolean  (ALC_APIENTRY *f_alcMakeContextCurrent)(ALCcontext *context);
static ALC_API ALCcontext* (ALC_APIENTRY *f_alcGetCurrentContext)(void);
static ALC_API ALCdevice*  (ALC_APIENTRY *f_alcGetContextsDevice)(ALCcontext *context);
static AL_API void (AL_APIENTRY *f_alSource3f)(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3);
static AL_API void (AL_APIENTRY *f_alSourcef)(ALuint source, ALenum param, ALfloat value);
static AL_API void (AL_APIENTRY *f_alSourcei)(ALuint source, ALenum param, ALint value);
static AL_API void (AL_APIENTRY *f_alBufferData)(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
static AL_API void (AL_APIENTRY *f_alSourceQueueBuffers)(ALuint source, ALsizei nb, const ALuint *buffers);
static AL_API void (AL_APIENTRY *f_alSourceUnqueueBuffers)(ALuint source, ALsizei nb, ALuint *buffers);
static AL_API void (AL_APIENTRY *f_alSourcePlay)(ALuint source);
static AL_API void (AL_APIENTRY *f_alGetSourcei)(ALuint source,  ALenum param, ALint *value);
static AL_API void (AL_APIENTRY *f_alListenerf)(ALenum param, ALfloat value);
static AL_API void (AL_APIENTRY *f_alGenBuffers)(ALsizei n, ALuint *buffers);
static AL_API void (AL_APIENTRY *f_alGenSources)(ALsizei n, ALuint *sources);
static AL_API void (AL_APIENTRY *f_alDeleteSources)(ALsizei n, const ALuint *sources);
static AL_API void (AL_APIENTRY *f_alDeleteBuffers)(ALsizei n, const ALuint *buffers);

static dllimp_t openal_imports[] = {
  { "alcOpenDevice",		&f_alcOpenDevice		},
  { "alcCloseDevice",		&f_alcCloseDevice		},
  { "alcCreateContext",		&f_alcCreateContext		},
  { "alcDestroyContext",	&f_alcDestroyContext		},
  { "alcMakeContextCurrent",	&f_alcMakeContextCurrent	},
  { "alcGetCurrentContext",	&f_alcGetCurrentContext		},
  { "alcGetContextsDevice",	&f_alcGetContextsDevice		},
  { "alSource3f",		&f_alSource3f			},
  { "alSourcef",		&f_alSourcef			},
  { "alSourcei",		&f_alSourcei			},
  { "alBufferData",		&f_alBufferData			},
  { "alSourceQueueBuffers",	&f_alSourceQueueBuffers		},
  { "alSourceUnqueueBuffers",	&f_alSourceUnqueueBuffers	},
  { "alSourcePlay",		&f_alSourcePlay			},
  { "alGetSourcei",		&f_alGetSourcei			},
  { "alListenerf",		&f_alListenerf			},
  { "alGenBuffers",		&f_alGenBuffers			},
  { "alGenSources",		&f_alGenSources			},
  { "alDeleteBuffers",		&f_alDeleteBuffers		},
  { "alDeleteSources",		&f_alDeleteSources		},
  { NULL,			NULL				}
};
#endif


static int midi_freq = 44100;
static int midi_buf_size = 4410;


#ifdef USE_OPENAL
ALvoid
alutInit(ALint *argc, ALbyte **argv) 
{
    ALCcontext *Context;
    ALCdevice *Device;

    /* Only if DLL is loaded. */
    if (openal_handle == NULL) return;

    /* Open device */
    Device = f_alcOpenDevice(NULL);
    if (Device != NULL) {
	/* Create context(s) */
	Context = f_alcCreateContext(Device, NULL);
	if (Context != NULL) {
		/* Set active context */
		f_alcMakeContextCurrent(Context);
	}
    }
}


ALvoid
alutExit(ALvoid) 
{
    ALCcontext *Context;
    ALCdevice *Device;

    /* Only if DLL is loaded. */
    if (openal_handle == NULL) return;

    /* Get active context */
    Context = f_alcGetCurrentContext();
    if (Context != NULL) {
	/* Get device for active context */
	Device = f_alcGetContextsDevice(Context);

	/* Disable context */
	f_alcMakeContextCurrent(NULL);

	f_alDeleteSources(nsources, source);

	f_alDeleteBuffers(4, buffers);
	f_alDeleteBuffers(4, buffers_cd);
	if (nbuffers > 0)
		f_alDeleteBuffers(nbuffers, buffers_midi);

	/* Release context(s) */
	f_alcDestroyContext(Context);

	/* Close device */
	f_alcCloseDevice(Device);
    }

#if 0
    /* Unload the DLL if possible. */
    if (openal_handle != NULL) {
	dynld_close((void *)openal_handle);
	openal_handle = NULL;
    }
#endif
}
#endif


void
openal_close(void)
{
#ifdef USE_OPENAL
    alutExit();
#endif
}


void
openal_init(void)
{
#ifdef USE_OPENAL
    wchar_t temp[512];
    const char *fn = PATH_AL_DLL;

    /* Try loading the DLL if needed. */
    if (openal_handle == NULL) {
	openal_handle = dynld_module(fn, openal_imports);
	if (openal_handle == NULL) {
		ERRLOG("SOUND: unable to load '%s' - sound disabled!\n", fn);
		swprintf(temp, sizeof_w(temp),
			 get_string(IDS_ERR_NOLIB), "OpenAL", fn);
		ui_msgbox(MBX_ERROR, temp);
		return;
	} else {
		INFO("SOUND: module '%s' loaded.\n", fn);
	}
    }

    /* Perform a module initialization. */
    alutInit(NULL, NULL);

    /* Close up shop on application exit. */
    atexit(openal_close);
#endif
}


/* Reset the OpenAL interface and its buffers. */
//FIXME: isnt this a major memory leak, we never free buffers!  --FvK
void
openal_reset(void)
{
#ifdef USE_OPENAL
    float *buf = NULL, *cd_buf = NULL, *midi_buf = NULL;
    int16_t *buf_int16 = NULL, *cd_buf_int16 = NULL, *midi_buf_int16 = NULL;
    int c;
#endif
    int init_midi = 0;
    const char *str;

    /*
     * If the current MIDI device is neither "none", nor system MIDI,
     * initialize the MIDI buffer and source, otherwise, do not.
     */
    str = midi_device_get_internal_name(config.midi_device);
    if ((str != NULL) &&
    (strcmp(str, "none") && strcmp(str, SYSTEM_MIDI_INT)))
           init_midi = 1;


#ifdef USE_OPENAL
    if (config.sound_is_float) {
	buf = (float *)mem_alloc((BUFLEN << 1) * sizeof(float));
	cd_buf = (float *)mem_alloc((CD_BUFLEN << 1) * sizeof(float));
	if (init_midi)
		midi_buf = (float *)mem_alloc(midi_buf_size * sizeof(float));
    } else {
	buf_int16 = (int16_t *)mem_alloc((BUFLEN << 1) * sizeof(int16_t));
	cd_buf_int16 = (int16_t *)mem_alloc((CD_BUFLEN << 1) * sizeof(int16_t));
	if (init_midi)
		midi_buf_int16 = (int16_t *)mem_alloc(midi_buf_size * sizeof(int16_t));
    }

    f_alGenBuffers(4, buffers);
    f_alGenBuffers(4, buffers_cd);
    if (init_midi) {
	f_alGenBuffers(4, buffers_midi);
	f_alGenSources(3, source);
	nbuffers = 4;
	nsources = 3;
    } else {
	f_alGenSources(2, source);
	nbuffers = 0;
	nsources = 2;
    }

    f_alSource3f(source[0], AL_POSITION,        0.0, 0.0, 0.0);
    f_alSource3f(source[0], AL_VELOCITY,        0.0, 0.0, 0.0);
    f_alSource3f(source[0], AL_DIRECTION,       0.0, 0.0, 0.0);
    f_alSourcef (source[0], AL_ROLLOFF_FACTOR,  0.0          );
    f_alSourcei (source[0], AL_SOURCE_RELATIVE, AL_TRUE      );
    f_alSource3f(source[1], AL_POSITION,        0.0, 0.0, 0.0);
    f_alSource3f(source[1], AL_VELOCITY,        0.0, 0.0, 0.0);
    f_alSource3f(source[1], AL_DIRECTION,       0.0, 0.0, 0.0);
    f_alSourcef (source[1], AL_ROLLOFF_FACTOR,  0.0          );
    f_alSourcei (source[1], AL_SOURCE_RELATIVE, AL_TRUE      );
    if (init_midi) {
	f_alSource3f(source[2], AL_POSITION,        0.0, 0.0, 0.0);
	f_alSource3f(source[2], AL_VELOCITY,        0.0, 0.0, 0.0);
	f_alSource3f(source[2], AL_DIRECTION,       0.0, 0.0, 0.0);
	f_alSourcef (source[2], AL_ROLLOFF_FACTOR,  0.0          );
	f_alSourcei (source[2], AL_SOURCE_RELATIVE, AL_TRUE      );
    }

    if (config.sound_is_float) {
	memset(buf,0,BUFLEN*2*sizeof(float));
	memset(cd_buf,0,BUFLEN*2*sizeof(float));
	if (init_midi)
		memset(midi_buf,0,midi_buf_size*sizeof(float));
    } else {
	memset(buf_int16,0,BUFLEN*2*sizeof(int16_t));
	memset(cd_buf_int16,0,BUFLEN*2*sizeof(int16_t));
	if (init_midi)
		memset(midi_buf_int16,0,midi_buf_size*sizeof(int16_t));
    }

    for (c=0; c<4; c++) {
	if (config.sound_is_float) {
		f_alBufferData(buffers[c], AL_FORMAT_STEREO_FLOAT32, buf, BUFLEN*2*sizeof(float), FREQ);
		f_alBufferData(buffers_cd[c], AL_FORMAT_STEREO_FLOAT32, cd_buf, CD_BUFLEN*2*sizeof(float), CD_FREQ);
		if (init_midi)
			f_alBufferData(buffers_midi[c], AL_FORMAT_STEREO_FLOAT32, midi_buf, midi_buf_size*sizeof(float), midi_freq);
	} else {
		f_alBufferData(buffers[c], AL_FORMAT_STEREO16, buf_int16, BUFLEN*2*sizeof(int16_t), FREQ);
		f_alBufferData(buffers_cd[c], AL_FORMAT_STEREO16, cd_buf_int16, CD_BUFLEN*2*sizeof(int16_t), CD_FREQ);
		if (init_midi)
			f_alBufferData(buffers_midi[c], AL_FORMAT_STEREO16, midi_buf_int16, midi_buf_size*sizeof(int16_t), midi_freq);
	}
    }

    f_alSourceQueueBuffers(source[0], 4, buffers);
    f_alSourceQueueBuffers(source[1], 4, buffers_cd);
    if (init_midi)
	f_alSourceQueueBuffers(source[2], 4, buffers_midi);
    f_alSourcePlay(source[0]);
    f_alSourcePlay(source[1]);
    if (init_midi)
	f_alSourcePlay(source[2]);

    if (config.sound_is_float) {
	if (init_midi)
		free(midi_buf);
	free(cd_buf);
	free(buf);
    } else {
	if (init_midi)
		free(midi_buf_int16);
	free(cd_buf_int16);
	free(buf_int16);
    }
#endif
}


static void
openal_buffer_common(void *buf, uint8_t src, int size, int freq)
{
#ifdef USE_OPENAL
    int processed;
    int state;
    ALuint buffer;
    double gain;

    if (openal_handle == NULL) return;

    f_alGetSourcei(source[src], AL_SOURCE_STATE, &state);

    if (state == 0x1014) {
	f_alSourcePlay(source[src]);
    }

    f_alGetSourcei(source[src], AL_BUFFERS_PROCESSED, &processed);
    if (processed >= 1) {
	gain = pow(10.0, (double)config.sound_gain / 20.0);
	f_alListenerf(AL_GAIN, (float)gain);

	f_alSourceUnqueueBuffers(source[src], 1, &buffer);

	if (config.sound_is_float) {
		f_alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, buf, size * sizeof(float), freq);
	} else {
		f_alBufferData(buffer, AL_FORMAT_STEREO16, buf, size * sizeof(int16_t), freq);
	}

	f_alSourceQueueBuffers(source[src], 1, &buffer);
    }
#endif
}


void
openal_buffer(void *buf)
{
    openal_buffer_common(buf, 0, BUFLEN << 1, FREQ);
}


void
openal_buffer_cd(void *buf)
{
    openal_buffer_common(buf, 1, CD_BUFLEN << 1, CD_FREQ);
}


void
openal_buffer_midi(void *buf, uint32_t size)
{
    openal_buffer_common(buf, 2, size, midi_freq);
}


void
openal_set_midi(int freq, int buf_size)
{
    midi_freq = freq;
    midi_buf_size = buf_size;
}
