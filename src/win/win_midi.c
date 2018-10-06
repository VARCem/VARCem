/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the System MIDI interface.
 *
 * Version:	@(#)win_midi.c	1.0.5	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../plat.h"
#include "../devices/sound/midi.h"
#include "win.h"


typedef struct {
    int		id;
    HANDLE	event;
    HMIDIOUT	out_device;
    MIDIHDR	hdr;
} midi_t;


static midi_t	*pm = NULL;


void
plat_midi_init(void)
{
    MMRESULT hr;

    pm = (midi_t *)mem_alloc(sizeof(midi_t));
    memset(pm, 0x00, sizeof(midi_t));

    pm->id = config_get_int(SYSTEM_MIDI_NAME, "midi", 0);

    pm->event = CreateEvent(NULL, TRUE, TRUE, NULL);

    hr = midiOutOpen(&pm->out_device, pm->id,
		     (uintptr_t)pm->event, 0, CALLBACK_EVENT);
    if (hr != MMSYSERR_NOERROR) {
	pm->id = 0;
	hr = midiOutOpen(&pm->out_device, pm->id,
			 (uintptr_t)pm->event, 0, CALLBACK_EVENT);
	if (hr != MMSYSERR_NOERROR) {
		ERRLOG("WIN MIDI: midiOutOpen error - %08X\n", hr);
		free(pm);
		return;
	}
    }

    midiOutReset(pm->out_device);
}


void
plat_midi_close(void)
{
    if (pm == NULL) return;

    if (pm->out_device != NULL) {
	midiOutReset(pm->out_device);
	midiOutClose(pm->out_device);

	CloseHandle(pm->event);
    }

    free(pm);

    pm = NULL;
}


int
plat_midi_get_num_devs(void)
{
    return midiOutGetNumDevs();
}


void
plat_midi_get_dev_name(int num, char *s)
{
    MIDIOUTCAPS caps;

    midiOutGetDevCaps(num, &caps, sizeof(caps));
    strcpy(s, caps.szPname);
}


void
plat_midi_play_msg(uint8_t *msg)
{
    if (pm == NULL) return;

    midiOutShortMsg(pm->out_device, *(uint32_t *)msg);
}


void
plat_midi_play_sysex(uint8_t *sysex, unsigned int len)
{
    MMRESULT result;

    if (pm == NULL) return;

    if (WaitForSingleObject(pm->event, 2000) == WAIT_TIMEOUT) {
	ERRLOG("WIN MIDI: can't send MIDI message\n");
	return;
    }

    midiOutUnprepareHeader(pm->out_device, &pm->hdr, sizeof(pm->hdr));

    pm->hdr.lpData = (char *)sysex;
    pm->hdr.dwBufferLength = len;
    pm->hdr.dwBytesRecorded = len;
    pm->hdr.dwUser = 0;

    result = midiOutPrepareHeader(pm->out_device, &pm->hdr, sizeof(pm->hdr));
    if (result != MMSYSERR_NOERROR) return;

    ResetEvent(pm->event);

    result = midiOutLongMsg(pm->out_device, &pm->hdr, sizeof(pm->hdr));
    if (result != MMSYSERR_NOERROR)
	SetEvent(pm->event);
}


int
plat_midi_write(uint8_t val)
{
    return 0;
}
