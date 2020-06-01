/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Interface to the MuNT32 MIDI synthesizer.
 *
 * Version:	@(#)midi_mt32.c	1.0.10	2019/05/03
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "munt/c_interface/c_interface.h"
#define dbglog sound_midi_log
#include "../../emu.h"
#include "../../config.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "sound.h"
#include "midi.h"
#include "midi_mt32.h"


#define MT32_CTRL_ROM_PATH	L"sound/mt32/mt32_control.rom"
#define MT32_PCM_ROM_PATH	L"sound/mt32/mt32_pcm.rom"
#define CM32_CTRL_ROM_PATH	L"sound/cm32l/cm32l_control.rom"
#define CM32_PCM_ROM_PATH	L"sound/cm32l/cm32l_pcm.rom"


static const mt32emu_report_handler_i_v0 handler_v0 = {
    /** Returns the actual interface version ID */
    NULL, //mt32emu_report_handler_version (*getVersionID)(mt32emu_report_handler_i i);

    /** Callback for debug messages, in vprintf() format */
    NULL, //void (*printDebug)(void *instance_data, const char *fmt, va_list list);
    /** Callbacks for reporting errors */
    NULL, //void (*onErrorControlROM)(void *instance_data);
    NULL, //void (*onErrorPCMROM)(void *instance_data);
    /** Callback for reporting about displaying a new custom message on LCD */
    NULL, //void (*showLCDMessage)(void *instance_data, const char *message);
    /** Callback for reporting actual processing of a MIDI message */
    NULL, //void (*onMIDIMessagePlayed)(void *instance_data);
    /**
     * Callback for reporting an overflow of the input MIDI queue.
     * Returns MT32EMU_BOOL_TRUE if a recovery action was taken
     * and yet another attempt to enqueue the MIDI event is desired.
     */
    NULL, //mt32emu_boolean (*onMIDIQueueOverflow)(void *instance_data);
    /**
     * Callback invoked when a System Realtime MIDI message is detected in functions
     * mt32emu_parse_stream and mt32emu_play_short_message and the likes.
     */
    NULL, //void (*onMIDISystemRealtime)(void *instance_data, mt32emu_bit8u system_realtime);
    /** Callbacks for reporting system events */
    NULL, //void (*onDeviceReset)(void *instance_data);
    NULL, //void (*onDeviceReconfig)(void *instance_data);
    /** Callbacks for reporting changes of reverb settings */
    NULL, //void (*onNewReverbMode)(void *instance_data, mt32emu_bit8u mode);
    NULL, //void (*onNewReverbTime)(void *instance_data, mt32emu_bit8u time);
    NULL, //void (*onNewReverbLevel)(void *instance_data, mt32emu_bit8u level);
    /** Callbacks for reporting various information */
    NULL, //void (*onPolyStateChanged)(void *instance_data, mt32emu_bit8u part_num);
    NULL, //void (*onProgramChanged)(void *instance_data, mt32emu_bit8u part_num, const char *sound_group_name, const char *patch_name);
};


#define RENDER_RATE 100
#define BUFFER_SEGMENTS 10


static thread_t		*thread_h = NULL;
static event_t		*event = NULL;
static event_t		*start_event = NULL;
static volatile int	mt32_on = 0;
static uint32_t		samplerate = 44100;
static int		buf_size = 0;
static float		*buffer = NULL;
static int16_t		*buffer_int16 = NULL;
static int		midi_pos = 0;
static const mt32emu_report_handler_i handler = { &handler_v0 };
static mt32emu_context	context = NULL;
static int		mtroms_present[2] = {-1, -1};


int	/*mt32emu_return_code*/
mt32_check(const char *func, mt32emu_return_code ret, mt32emu_return_code expected)
{
    if (ret != expected) {
	ERRLOG("%s() failed, expected %d but returned %d\n", func, expected, ret);
        return 0;
    }

    return 1;
}


int
mt32_available(void)
{
    if (mtroms_present[0] < 0)
	mtroms_present[0] = (rom_present(MT32_CTRL_ROM_PATH) &&
			     rom_present(MT32_PCM_ROM_PATH));
    return mtroms_present[0];
}


int
cm32l_available(void)
{
    if (mtroms_present[1] < 0)
	mtroms_present[1] = (rom_present(CM32_CTRL_ROM_PATH) &&
			     rom_present(CM32_PCM_ROM_PATH));
    return mtroms_present[1];
}


void
mt32_stream(float* stream, int len)
{
    if (context) mt32emu_render_float(context, stream, len);
}


void
mt32_stream_int16(int16_t* stream, int len)
{
    if (context) mt32emu_render_bit16s(context, stream, len);
}


void
mt32_poll(void)
{
    midi_pos++;
    if (midi_pos == 48000/RENDER_RATE) {
	midi_pos = 0;
	thread_set_event(event);
    }
}


static void
mt32_thread(void *param)
{
    int buf_pos = 0;
    int bsize = buf_size / BUFFER_SEGMENTS;
    float *buf;
    int16_t *buf16;

    thread_set_event(start_event);
    while (mt32_on) {
	thread_wait_event(event, -1);
	thread_reset_event(event);

	if (config.sound_is_float) {
		buf = (float *) ((uint8_t*)buffer + buf_pos);

		memset(buf, 0, bsize);
		mt32_stream(buf, bsize / (2 * sizeof(float)));
		buf_pos += bsize;
		if (buf_pos >= buf_size) {
			openal_buffer_midi(buffer, buf_size / sizeof(float));
			buf_pos = 0;
		}
	} else {
		buf16 = (int16_t *) ((uint8_t*)buffer_int16 + buf_pos);

		memset(buf16, 0, bsize);
		mt32_stream_int16(buf16, bsize / (2 * sizeof(int16_t)));
		buf_pos += bsize;
		if (buf_pos >= buf_size) {
			openal_buffer_midi(buffer_int16, buf_size / sizeof(int16_t));
			buf_pos = 0;
		}
	}
    }
}


void
mt32_msg(uint8_t* val)
{
    if (context) mt32_check("mt32emu_play_msg", mt32emu_play_msg(context, *(uint32_t*)val), MT32EMU_RC_OK);
}


void
mt32_sysex(uint8_t* data, unsigned int len)
{
    if (context) mt32_check("mt32emu_play_sysex", mt32emu_play_sysex(context, data, len), MT32EMU_RC_OK);
}


static void *
mt32emu_init(wchar_t *control_rom, wchar_t *pcm_rom)
{
    wchar_t path[1024];
    midi_device_t *dev;
    char fn[1024];

    context = mt32emu_create_context(handler, NULL);

    pc_path(path, sizeof_w(path), rom_path(control_rom));
    wcstombs(fn, path, sizeof(fn));
    if (!mt32_check("mt32emu_add_rom_file", mt32emu_add_rom_file(context, fn), MT32EMU_RC_ADDED_CONTROL_ROM)) return 0;

    pc_path(path, sizeof_w(path), rom_path(pcm_rom));
    wcstombs(fn, path, sizeof(fn));
    if (!mt32_check("mt32emu_add_rom_file", mt32emu_add_rom_file(context, fn), MT32EMU_RC_ADDED_PCM_ROM)) return 0;

    if (!mt32_check("mt32emu_open_synth", mt32emu_open_synth(context), MT32EMU_RC_OK)) return 0;

    samplerate = mt32emu_get_actual_stereo_output_samplerate(context);

    /* buf_size = samplerate/RENDER_RATE*2; */
    if (config.sound_is_float) {
	buf_size = (samplerate/RENDER_RATE)*2*BUFFER_SEGMENTS*sizeof(float);
	buffer = (float *)mem_alloc(buf_size);
	buffer_int16 = NULL;
    } else {
	buf_size = (samplerate/RENDER_RATE)*2*BUFFER_SEGMENTS*sizeof(int16_t);
	buffer = NULL;
	buffer_int16 = (int16_t *)mem_alloc(buf_size);
    }

    mt32emu_set_output_gain(context, device_get_config_int("output_gain")/100.0f);
    mt32emu_set_reverb_enabled(context, (mt32emu_boolean) !!device_get_config_int("reverb"));
    mt32emu_set_reverb_output_gain(context, device_get_config_int("reverb_output_gain")/100.0f);
    mt32emu_set_reversed_stereo_enabled(context, (mt32emu_boolean) !!device_get_config_int("reversed_stereo"));
    mt32emu_set_nice_amp_ramp_enabled(context, (mt32emu_boolean) !!device_get_config_int("nice_ramp"));

    /* DEBUG("mt32 output gain: %f\n", mt32emu_get_output_gain(context));
    DEBUG("mt32 reverb output gain: %f\n", mt32emu_get_reverb_output_gain(context));
    DEBUG("mt32 reverb: %d\n", mt32emu_is_reverb_enabled(context));
    DEBUG("mt32 reversed stereo: %d\n", mt32emu_is_reversed_stereo_enabled(context)); */

    openal_set_midi(samplerate, buf_size);

    /* DEBUG("mt32 (Munt %s) initialized, samplerate %d, buf_size %d\n", mt32emu_get_library_version_string(), samplerate, buf_size); */

    dev = (midi_device_t *)mem_alloc(sizeof(midi_device_t));
    memset(dev, 0, sizeof(midi_device_t));

    dev->play_msg = mt32_msg;
    dev->play_sysex = mt32_sysex;
    dev->poll = mt32_poll;

    midi_init(dev);

    mt32_on = 1;

    start_event = thread_create_event();
    event = thread_create_event();
    thread_h = thread_create(mt32_thread, 0);

    thread_wait_event(start_event, -1);
    thread_reset_event(start_event);

    return dev;
}


static void *
mt32_init(const device_t *info, UNUSED(void *parent))
{
    return mt32emu_init(MT32_CTRL_ROM_PATH, MT32_PCM_ROM_PATH);
}


static void *
cm32l_init(const device_t *info, UNUSED(void *parent))
{
    return mt32emu_init(CM32_CTRL_ROM_PATH, CM32_PCM_ROM_PATH);
}


static void
mt32_close(void *priv)
{
    if (priv == NULL) return;

    mt32_on = 0;
    thread_set_event(event);
    thread_wait(thread_h, -1);

    event = NULL;
    start_event = NULL;
    thread_h = NULL;

    if (context) {
	mt32emu_close_synth(context);
	mt32emu_free_context(context);
    }
    context = NULL;

    if (buffer)
	free(buffer);
    buffer = NULL;

    if (buffer_int16)
	free(buffer_int16);
    buffer_int16 = NULL;
}


static const device_config_t mt32_config[] =
{
	{
		"output_gain","Output Gain",CONFIG_SPINNER,"",100,
		.spinner =
		{
			0, 100
		}
	},
	{
		"reverb","Reverb",CONFIG_BINARY,"",1
	},
	{
		"reverb_output_gain","Reverb Output Gain",CONFIG_SPINNER,"",100,
		.spinner =
		{
			0, 100
		}
	},
	{
		"reversed_stereo","Reversed stereo",CONFIG_BINARY,"",0
	},
	{
		"nice_ramp","Nice ramp",CONFIG_BINARY,"",1
	},
	{
		NULL
	}
};


const device_t mt32_device = {
    "Roland MT-32 Emulation",
    0, 0, NULL,
    mt32_init, mt32_close, NULL,
    mt32_available,
    NULL, NULL, NULL,
    mt32_config
};

const device_t cm32l_device = {
    "Roland CM-32L Emulation",
    0, 0, NULL,
    cm32l_init, mt32_close, NULL,
    cm32l_available,
    NULL, NULL, NULL,
    mt32_config
};
