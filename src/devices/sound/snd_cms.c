/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Creative CMS/GameBlaster sound device.
 *
 * Version:	@(#)snd_cms.c	1.0.7	2018/10/16
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
#include <stdlib.h>
#include <wchar.h>
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "sound.h"


#define MASTER_CLOCK	7159090


typedef struct {
    int		addrs[2];
    uint8_t	regs[2][32];
    uint16_t	latch[2][6];
    int		freq[2][6];
    float	count[2][6];
    int		vol[2][6][2];
    int		stat[2][6];
    uint16_t	noise[2][2];
    uint16_t	noisefreq[2][2];
    int		noisecount[2][2];
    int		noisetype[2][2];

    uint8_t	latched_data;

    int		pos;
    int16_t buffer[SOUNDBUFLEN * 2];
} cms_t;


static void
cms_update(cms_t *dev)
{
    int16_t out_l = 0, out_r = 0;
    int c, d;

    for (; dev->pos < sound_pos_global; dev->pos++) {
	for (c = 0; c < 4; c++) {
		switch (dev->noisetype[c>>1][c&1]) {
			case 0:
				dev->noisefreq[c>>1][c&1] = MASTER_CLOCK/256;
				break;
			case 1:
				dev->noisefreq[c>>1][c&1] = MASTER_CLOCK/512;
				break;
			case 2:
				dev->noisefreq[c>>1][c&1] = MASTER_CLOCK/1024;
				break;
			case 3:
				dev->noisefreq[c>>1][c&1] = dev->freq[c>>1][(c&1) * 3];
				break;
		}
	}

	for (c = 0; c < 2; c ++) {
		if (dev->regs[c][0x1C] & 1) {
			for (d = 0; d < 6; d++) {
				if (dev->regs[c][0x14] & (1 << d)) {
					if (dev->stat[c][d])
						out_l += (dev->vol[c][d][0] * 90);
					if (dev->stat[c][d])
						out_r += (dev->vol[c][d][1] * 90);
					dev->count[c][d] += dev->freq[c][d];
					if (dev->count[c][d] >= 24000) {
						dev->count[c][d] -= 24000;
						dev->stat[c][d] ^= 1;
					}
				} else if (dev->regs[c][0x15] & (1 << d)) {
					if (dev->noise[c][d / 3] & 1)
						out_l += (dev->vol[c][d][0] * 90);
					if (dev->noise[c][d / 3] & 1)
						out_r += (dev->vol[c][d][0] * 90);
				}
			}

			for (d = 0; d < 2; d++) {
				dev->noisecount[c][d] += dev->noisefreq[c][d];
				while (dev->noisecount[c][d] >= 24000) {
					dev->noisecount[c][d] -= 24000;
					dev->noise[c][d] <<= 1;
					if (!(((dev->noise[c][d] & 0x4000) >> 8) ^ (dev->noise[c][d] & 0x40))) 
						dev->noise[c][d] |= 1;
				}
			}
		}
	}

	dev->buffer[(dev->pos << 1)] = out_l;
	dev->buffer[(dev->pos << 1) + 1] = out_r;
    }
}


static void
get_buffer(int32_t *buffer, int len, void *priv)
{
    cms_t *dev = (cms_t *)priv;
    int c;

    cms_update(dev);

    for (c = 0; c < len * 2; c++)
	buffer[c] += dev->buffer[c];

    dev->pos = 0;
}


static void
cms_write(uint16_t addr, uint8_t val, void *priv)
{
    cms_t *dev = (cms_t *)priv;
    int voice;
    int chip = (addr & 2) >> 1;

    switch (addr & 0xf) {
	case 1:
		dev->addrs[0] = val & 31;
		break;

	case 3:
		dev->addrs[1] = val & 31;
		break;

	case 0:
	case 2:
		cms_update(dev);
		dev->regs[chip][dev->addrs[chip] & 31] = val;

		switch (dev->addrs[chip] & 31) {
			case 0x00: case 0x01: case 0x02: /*Volume*/
			case 0x03: case 0x04: case 0x05:
				voice = dev->addrs[chip] & 7;
				dev->vol[chip][voice][0] = val & 0xf;
				dev->vol[chip][voice][1] = val >> 4;
				break;

			case 0x08: case 0x09: case 0x0A: /*Frequency*/
			case 0x0B: case 0x0C: case 0x0D:
				voice = dev->addrs[chip] & 7;
				dev->latch[chip][voice] = (dev->latch[chip][voice] & 0x700) | val;
				dev->freq[chip][voice] = (MASTER_CLOCK/512 << (dev->latch[chip][voice] >> 8)) / (511 - (dev->latch[chip][voice] & 255));
				break;

			case 0x10: case 0x11: case 0x12: /*Octave*/
				voice = (dev->addrs[chip] & 3) << 1;
				dev->latch[chip][voice] = (dev->latch[chip][voice] & 0xFF) | ((val & 7) << 8);
				dev->latch[chip][voice + 1] = (dev->latch[chip][voice + 1] & 0xFF) | ((val & 0x70) << 4);
				dev->freq[chip][voice] = (MASTER_CLOCK/512 << (dev->latch[chip][voice] >> 8)) / (511 - (dev->latch[chip][voice] & 255));
				dev->freq[chip][voice + 1] = (MASTER_CLOCK/512 << (dev->latch[chip][voice + 1] >> 8)) / (511 - (dev->latch[chip][voice + 1] & 255));
				break;

			case 0x16: /*Noise*/
				dev->noisetype[chip][0] = val & 3;
				dev->noisetype[chip][1] = (val >> 4) & 3;
				break;
		}
		break;

	case 0x6: case 0x7:
		dev->latched_data = val;
		break;
    }
}


static uint8_t
cms_read(uint16_t addr, void *priv)
{
    cms_t *dev = (cms_t *)priv;
    uint8_t ret = 0xff;

    switch (addr & 0xf) {
	case 0x1:
		ret = dev->addrs[0];
		break;

	case 0x3:
		ret = dev->addrs[1];
		break;

	case 0x4:
		ret = 0x7f;
		break;

	case 0xa:
	case 0xb:
		ret = dev->latched_data;
		break;

	default:
		break;
    }

    return(ret);
}


static void *
cms_init(const device_t *info)
{
    cms_t *dev;

    dev = (cms_t *)mem_alloc(sizeof(cms_t));
    memset(dev, 0x00, sizeof(cms_t));

    io_sethandler(0x0220, 16,
		  cms_read,NULL,NULL, cms_write,NULL,NULL, dev);

    sound_add_handler(get_buffer, dev);

    return(dev);
}


static void
cms_close(void *priv)
{
    cms_t *dev = (cms_t *)priv;

    free(dev);
}


const device_t cms_device = {
    "Creative Music System (Game Blaster)",
    DEVICE_ISA,
    0,
    cms_init, cms_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
