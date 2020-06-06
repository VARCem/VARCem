/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the PC-Speaker device.
 *
 * Version:	@(#)snd_speaker.c	1.0.9	2020/01/31
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include <wchar.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../timer.h"
#include "../system/pit.h"
#include "../system/ppi.h"
#include "sound.h"
#include "snd_speaker.h"


int		speaker_mute = 0,
		speaker_gated = 0,
		speaker_enable = 0,
		speaker_was_enable = 0;

int		speaker_val,
		speaker_on;


static int32_t	speaker_buffer[SOUNDBUFLEN];
static int	speaker_pos;


static void
get_buffer(int32_t *buffer, int len, priv_t priv)
{
    int32_t val;
    int c;

    speaker_update();
        
    if (! speaker_mute) {
	for (c = 0; c < len * 2; c++) {
		val = speaker_buffer[c >> 1];
		buffer[c++] += val;
		buffer[c] += val;
	}
    }

    speaker_pos = 0;
}


void
speaker_update(void)
{
    int32_t val;
    double timer2_count, amplitude;
    
    timer2_count = pit.l[2] ? ((double) pit.l[2]) : 65536.0;
    amplitude = ((timer2_count / 64.0) * 10240.0) - 5120.0;
    
    if (amplitude > 5120.0)
	amplitude = 5120.0;

    if (speaker_pos >= sound_pos_global)
	return;

    for (; speaker_pos < sound_pos_global; speaker_pos++) {
	if (speaker_gated && speaker_was_enable) {
		if ((pit.m[2] == 0) || (pit.m[2] == 4))
			val = (int32_t) amplitude;
		else if (pit.l[2] < 0x40)
			val = 0x0a00;
		else 
			val = speaker_on ? 0x1400 : 0;
	} else {
		if (pit.m[2] == 1)
			val = speaker_was_enable ? (int32_t) amplitude : 0;
		else
			val = speaker_was_enable ? 0x1400 : 0;
	}

	if (! speaker_enable)
		speaker_was_enable = 0;

	speaker_buffer[speaker_pos] = val;
    }
}


void
speaker_timer(int new_out, int old_out)
{
    tmrval_t l;

    speaker_update();

    l = pit.l[2] ? pit.l[2] : (tmrval_t)0x10000;
    if (l < 25LL)
        speaker_on = 0;
      else
        speaker_on = new_out;

    ppispeakon = new_out;
}


void
speaker_reset(void)
{
    INFO("SPEAKER: reset\n");

    sound_add_handler(get_buffer, NULL);

    speaker_mute = speaker_gated = 0;
    speaker_enable = speaker_was_enable = 0;
    speaker_pos = 0;
}
