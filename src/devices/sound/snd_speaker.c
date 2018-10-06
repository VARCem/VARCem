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
 * Version:	@(#)snd_speaker.c	1.0.3	2018/09/22
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#define dbglog sound_dev_log
#include "../../emu.h"
#include "../system/pit.h"
#include "sound.h"
#include "snd_speaker.h"


int		speaker_mute,
		speaker_gated,
		speaker_enable,
		was_speaker_enable;

int		gated,
		speakval,
		speakon;


static int16_t	speaker_buffer[SOUNDBUFLEN];
static int	speaker_pos;


void
speaker_update(void)
{
    int16_t val;

    for (; speaker_pos < sound_pos_global; speaker_pos++) {
	if (speaker_gated && was_speaker_enable) {
		if (!pit.m[2] || pit.m[2]==4)
			val = speakval;
		else if (pit.l[2] < 0x40)
			val = 0x0a00;
		else 
			val = speakon ? 0x1400 : 0;
	} else
		val = was_speaker_enable ? 0x1400 : 0;

	if (! speaker_enable)
		was_speaker_enable = 0;

	speaker_buffer[speaker_pos] = val;
    }
}


static void
get_buffer(int32_t *buffer, int len, void *p)
{
    int c;

    speaker_update();
        
    if (! speaker_mute) {
	for (c = 0; c < len * 2; c++)
		buffer[c] += speaker_buffer[c >> 1];
    }

    speaker_pos = 0;
}


void
speaker_reset(void)
{
    INFO("SPEAKER: reset\n");

    sound_add_handler(get_buffer, NULL);

    speaker_mute = speaker_gated = 0;
    speaker_enable = was_speaker_enable = 0;
    speaker_pos = 0;
}
