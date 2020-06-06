/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		MIDI support module, main file.
 *
 * Version:	@(#)midi.c	1.0.11	2019/05/03
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog sound_midi_log
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../plat.h"
#include "sound.h"
#include "midi.h"
#include "midi_system.h"
#ifdef USE_FLUIDSYNTH
# include "midi_fluidsynth.h"
#endif
#ifdef USE_MUNT
# include "midi_mt32.h"
#endif


#define SYSEX_SIZE	1024
#define RAWBUF		1024


typedef struct {
    uint8_t	rt_buf[1024],
		cmd_buf[1024],
		status,
		sysex_data[1026];

    int		cmd_pos,
		cmd_len;

    unsigned int sysex_start,
		sysex_delay,
		pos;

    const midi_device_t *device;
} midi_t;


#ifdef ENABLE_SOUND_MIDI_LOG
int		sound_midi_do_log = ENABLE_SOUND_MIDI_LOG;
#endif


static const uint8_t	MIDI_evt_len[256] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

    3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
    3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
    3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0
    3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

    2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0
    2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0

    3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0

    0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0
};
static const struct {
    const char		*internal_name;
    const device_t	*device;
} devices[] = {
    {"none",		NULL			},

#ifdef USE_FLUIDSYNTH
    {"fluidsynth",	&fluidsynth_device	},
#endif
#ifdef USE_MUNT
    {"mt32",		&mt32_device		},
    {"cm32l",		&cm32l_device		},
#endif
    {SYSTEM_MIDI_INT,	&system_midi_device	},

    {NULL,		NULL			}
};
static midi_t		*midi = NULL;


int
midi_device_available(int card)
{
    if (devices[card].device != NULL)
	return(device_available(devices[card].device));

    return(1);
}


const char *
midi_device_getname(int card)
{
    if (devices[card].device != NULL)
	return(devices[card].device->name);

    return(NULL);
}


const device_t *
midi_device_getdevice(int card)
{
    return(devices[card].device);
}


int
midi_device_has_config(int card)
{
    if (devices[card].device != NULL)
	return(devices[card].device->config ? 1 : 0);

    return(0);
}


const char *
midi_device_get_internal_name(int card)
{
    return(devices[card].internal_name);
}


int
midi_device_get_from_internal_name(const char *s)
{
    int c;
	
    for (c = 0; devices[c].internal_name != NULL; c++)
	if (! strcmp(devices[c].internal_name, s))
		return(c);

    /* Not found. */
    return(0);
}


#ifdef _LOGGING
void
sound_midi_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SOUND_MIDI_LOG
    va_list ap;

    if (sound_midi_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
midi_device_init(void)
{
    if (devices[config.midi_device].device != NULL)
	device_add(devices[config.midi_device].device);
}


void
midi_init(const midi_device_t *dev)
{
    midi = (midi_t *)mem_alloc(sizeof(midi_t));
    memset(midi, 0, sizeof(midi_t));

    midi->device = dev;
}


void
midi_close(void)
{
    if (midi != NULL) {
#if 0
	/* is a const device .. */
	if (midi->device)
		free(midi->device);
#endif

	free(midi);

	midi = NULL;
    }
}


void
midi_poll(void)
{
    if (midi && midi->device && midi->device->poll)
	midi->device->poll();
}


void
play_msg(uint8_t *msg)
{
    if (midi && midi->device && midi->device->play_msg)
		midi->device->play_msg(msg);
}


void
play_sysex(uint8_t *sysex, unsigned int len)
{
    if (midi && midi->device && midi->device->play_sysex)
		midi->device->play_sysex(sysex, len);
}


void
midi_write(uint8_t val)
{
    uint32_t passed_ticks;

    if (!midi || !midi->device) return;

    if (midi->device->write && midi->device->write(val)) return;

    if (midi->sysex_start) {
	passed_ticks = plat_get_ticks() - midi->sysex_start;
	if (passed_ticks < midi->sysex_delay)
		plat_delay_ms(midi->sysex_delay - passed_ticks);
    }

    /* Test for a realtime MIDI message */
    if (val >= 0xf8) {
	midi->rt_buf[0] = val;
	play_msg(midi->rt_buf);
	return;
    }

    /* Test for a active sysex transfer */
    if (midi->status == 0xf0) {
	if (!(val & 0x80)) {
		if (midi->pos  < (SYSEX_SIZE-1))
			midi->sysex_data[midi->pos++] = val;
		return;
	} else {
		midi->sysex_data[midi->pos++] = 0xf7;
		if ((midi->sysex_start) && (midi->pos >= 4) &&
		    (midi->pos <= 9) && (midi->sysex_data[1] == 0x41) &&
		    (midi->sysex_data[3] == 0x16)) {
			DEBUG("MIDI: Skipping invalid MT-32 SysEx MIDI message\n");
		} else {
			play_sysex(midi->sysex_data, midi->pos);
			if (midi->sysex_start) {
				if (midi->sysex_data[5] == 0x7f)
					/* All parameters reset */
					midi->sysex_delay = 290;
				else if ((midi->sysex_data[5] == 0x10) &&
					 (midi->sysex_data[6] == 0x00) &&
					 (midi->sysex_data[7] == 0x04))
					midi->sysex_delay = 145;	/* Viking Child */
				else if ((midi->sysex_data[5] == 0x10) &&
					 (midi->sysex_data[6] == 0x00) &&
					 (midi->sysex_data[7] == 0x01))
					midi->sysex_delay = 30;	/* Dark Sun 1 */
				else
					midi->sysex_delay = (unsigned int) (((float) (midi->pos) * 1.25f) * 1000.0f / 3125.0f) + 2;
				midi->sysex_start = plat_get_ticks();
			}
		}
	}
    }

    if (val & 0x80) {
	midi->status = val;
	midi->cmd_pos = 0;
	midi->cmd_len = MIDI_evt_len[val];
	if (midi->status == 0xf0) {
		midi->sysex_data[0] = 0xf0;
		midi->pos = 1;
	}
    }

    if (midi->cmd_len) {
	midi->cmd_buf[midi->cmd_pos++] = val;
	if (midi->cmd_pos >= midi->cmd_len) {
		play_msg(midi->cmd_buf);
		midi->cmd_pos = 1;
	}
    }
}
