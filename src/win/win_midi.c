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
 * Version:	@(#)win_midi.c	1.0.3	2018/03/10
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
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../sound/midi.h"
#include "../plat.h"
#include "win.h"


int midi_id = 0;
HANDLE m_event;

static HMIDIOUT midi_out_device = NULL;
static uint8_t midi_rt_buf[1024];
static uint8_t midi_cmd_buf[1024];
static int midi_cmd_pos = 0;
static int midi_cmd_len = 0;
static uint8_t midi_status = 0;
static unsigned int midi_sysex_start = 0;
static unsigned int midi_sysex_delay = 0;

void plat_midi_init()
{
	/* This is for compatibility with old configuration files. */
	midi_id = config_get_int("Sound", "midi_host_device", -1);
	if (midi_id == -1)
	{
		midi_id = config_get_int(SYSTEM_MIDI_NAME, "midi", 0);
	}
	else
	{
		config_delete_var("Sound", "midi_host_device");
		config_set_int(SYSTEM_MIDI_NAME, "midi", midi_id);
	}

        MMRESULT hr = MMSYSERR_NOERROR;

	memset(midi_rt_buf, 0, sizeof(midi_rt_buf));
	memset(midi_cmd_buf, 0, sizeof(midi_cmd_buf));

	midi_cmd_pos = midi_cmd_len = 0;
	midi_status = 0;

	midi_sysex_start = midi_sysex_delay = 0;

	m_event = CreateEvent(NULL, TRUE, TRUE, NULL);

        hr = midiOutOpen(&midi_out_device, midi_id, (uintptr_t) m_event,
		   0, CALLBACK_EVENT);
        if (hr != MMSYSERR_NOERROR) {
                printf("midiOutOpen error - %08X\n",hr);
                midi_id = 0;
                hr = midiOutOpen(&midi_out_device, midi_id, (uintptr_t) m_event,
        		   0, CALLBACK_EVENT);
                if (hr != MMSYSERR_NOERROR) {
                        printf("midiOutOpen error - %08X\n",hr);
                        return;
                }
        }

        midiOutReset(midi_out_device);
}

void plat_midi_close()
{
        if (midi_out_device != NULL)
        {
                midiOutReset(midi_out_device);
                midiOutClose(midi_out_device);
                /* midi_out_device = NULL; */
		CloseHandle(m_event);
        }
}

int plat_midi_get_num_devs()
{
        return midiOutGetNumDevs();
}
void plat_midi_get_dev_name(int num, char *s)
{
        MIDIOUTCAPS caps;

        midiOutGetDevCaps(num, &caps, sizeof(caps));
        strcpy(s, caps.szPname);
}

void plat_midi_play_msg(uint8_t *msg)
{
	midiOutShortMsg(midi_out_device, *(uint32_t *) msg);
}

MIDIHDR m_hdr;

void plat_midi_play_sysex(uint8_t *sysex, unsigned int len)
{
	MMRESULT result;

	if (WaitForSingleObject(m_event, 2000) == WAIT_TIMEOUT)
	{
		pclog("Can't send MIDI message\n");
		return;
	}

	midiOutUnprepareHeader(midi_out_device, &m_hdr, sizeof(m_hdr));

	m_hdr.lpData = (char *) sysex;
	m_hdr.dwBufferLength = len;
	m_hdr.dwBytesRecorded = len;
	m_hdr.dwUser = 0;

	result = midiOutPrepareHeader(midi_out_device, &m_hdr, sizeof(m_hdr));

	if (result != MMSYSERR_NOERROR)  return;
	ResetEvent(m_event);
	result = midiOutLongMsg(midi_out_device, &m_hdr, sizeof(m_hdr));
	if (result != MMSYSERR_NOERROR)
	{
		SetEvent(m_event);
		return;
	}
}

int plat_midi_write(uint8_t val)
{
	return 0;
}
