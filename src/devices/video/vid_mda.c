/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		MDA emulation.
 *
 * Version:	@(#)vid_mda.c	1.0.15	2019/05/03
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
#include "../../emu.h"
#include "../../config.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/pit.h"
#include "../ports/parallel.h"
#include "video.h"
#include "vid_mda.h"


static const video_timings_t mda_timings = { VID_ISA,8,16,32,8,16,32 };


void
mda_recalctimings(mda_t *dev)
{
    double _dispontime, _dispofftime, disptime;

    disptime = dev->crtc[0] + 1;
    _dispontime = dev->crtc[1];
    _dispofftime = disptime - _dispontime;
    _dispontime *= MDACONST;
    _dispofftime *= MDACONST;

    dev->dispontime = (int64_t)(_dispontime * (1 << TIMER_SHIFT));
    dev->dispofftime = (int64_t)(_dispofftime * (1 << TIMER_SHIFT));
}


void
mda_out(uint16_t port, uint8_t val, void *priv)
{
    mda_t *dev = (mda_t *)priv;

    switch (port) {
	case 0x03b0:
	case 0x03b2:
	case 0x03b4:
	case 0x03b6:
		dev->crtcreg = val & 31;
		break;

	case 0x03b1:
	case 0x03b3:
	case 0x03b5:
	case 0x03b7:
		dev->crtc[dev->crtcreg] = val;
		if (dev->crtc[10] == 6 && dev->crtc[11] == 7) {
			/*Fix for Generic Turbo XT BIOS,
			 * which sets up cursor registers wrong*/
			dev->crtc[10] = 0xb;
			dev->crtc[11] = 0xc;
		}
		mda_recalctimings(dev);
		break;

	case 0x03b8:
		dev->ctrl = val;
		break;
    }
}


uint8_t
mda_in(uint16_t port, void *priv)
{
    mda_t *dev = (mda_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x03b0:
	case 0x03b2:
	case 0x03b4:
	case 0x03b6:
		ret = dev->crtcreg;
		break;

	case 0x03b1:
	case 0x03b3:
	case 0x03b5:
	case 0x03b7:
		ret = dev->crtc[dev->crtcreg];
		break;

	case 0x03ba:
		ret = dev->stat | 0xF0;
		break;

	default:
		break;
    }

    return ret;
}


void
mda_write(uint32_t addr, uint8_t val, void *priv)
{
    mda_t *dev = (mda_t *)priv;

    dev->vram[addr & 0xfff] = val;
}


uint8_t
mda_read(uint32_t addr, void *priv)
{
    mda_t *dev = (mda_t *)priv;

    return dev->vram[addr & 0xfff];
}


void
mda_poll(void *priv)
{
    mda_t *dev = (mda_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    int oldsc;
    int blink;
    pel_t *pels;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;
	pels = screen->line[dev->displine];

	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = (dev->sc << 1) & 7;

	if (dev->dispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;

		for (x = 0; x < dev->crtc[1]; x++) {
			chr  = dev->vram[(dev->ma << 1) & 0xfff];
			attr = dev->vram[((dev->ma << 1) + 1) & 0xfff];
			drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
			blink = ((dev->blink & 16) && (dev->ctrl & 0x20) && (attr & 0x80) && !drawcursor);
			if (dev->sc == 12 && ((attr & 7) == 1)) {
				for (c = 0; c < 9; c++)
				    pels[(x * 9) + c].pal = dev->cols[attr][blink][1];
			} else {
				for (c = 0; c < 8; c++)
				    pels[(x * 9) + c].pal = dev->cols[attr][blink][(fontdatm[chr][dev->sc] & (1 << (c ^ 7))) ? 1 : 0];
				if ((chr & ~0x1f) == 0xc0)
					pels[(x * 9) + 8].pal = dev->cols[attr][blink][fontdatm[chr][dev->sc] & 1];
				else
					pels[(x * 9) + 8].pal = dev->cols[attr][blink][0];
			}
			dev->ma++;

			if (drawcursor) {
				for (c = 0; c < 9; c++)
				    pels[(x * 9) + c].pal ^= dev->cols[attr][0][1];
			}
		}
	}
	dev->sc = oldsc;

	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 500) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon)
		dev->stat &= ~1;
	dev->linepos = 0;

	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->stat &= ~8;
	}

	if (dev->sc == (dev->crtc[11] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
		dev->con = 0; 
		dev->coff = 1; 
	}

	if (dev->vadj) {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
		dev->vadj--;
		if (! dev->vadj) {
			dev->dispon = 1;
			dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			dev->sc = 0;
		}
	} else if (dev->sc == dev->crtc[9] || ((dev->crtc[8] & 3) == 3 && dev->sc == (dev->crtc[9] >> 1))) {
		dev->maback = dev->ma;
		dev->sc = 0;
		oldvc = dev->vc;
		dev->vc++;
		dev->vc &= 127;
		if (dev->vc == dev->crtc[6]) 
			dev->dispon = 0;
		if (oldvc == dev->crtc[4]) {
			dev->vc = 0;
			dev->vadj = dev->crtc[5];
			if (!dev->vadj) dev->dispon = 1;
			if (!dev->vadj) dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			else
				dev->cursoron = dev->blink & 16;
		}

		if (dev->vc == dev->crtc[7]) {
			dev->dispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;
			if (dev->crtc[7]) {
				x = dev->crtc[1] * 9;
				dev->lastline++;
				if ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) || video_force_resize_get()) {
					xsize = x;
					ysize = dev->lastline - dev->firstline;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, ysize);

					if (video_force_resize_get())
						video_force_resize_set(0);
				}

				video_blit_start(1, 0, dev->firstline, 0, ysize, xsize, ysize);
				frames++;

				video_res_x = dev->crtc[1];
				video_res_y = dev->crtc[6];
				video_bpp = 0;
			}

			dev->firstline = 1000;
			dev->lastline = 0;
			dev->blink++;
		}
	} else {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
	}

	if ((dev->sc == (dev->crtc[10] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1)))) {
		dev->con = 1;
	}
    }
}


void
mda_init(mda_t *dev)
{
    int c;

    for (c = 0; c < 256; c++) {
	dev->cols[c][0][0] = dev->cols[c][1][0] = dev->cols[c][1][1] = 16;
	if (c & 8)
		dev->cols[c][0][1] = 15 + 16;
	else
		dev->cols[c][0][1] =  7 + 16;
    }
    dev->cols[0x70][0][1] = 16;
    dev->cols[0x70][0][0] = dev->cols[0x70][1][0] =
		dev->cols[0x70][1][1] = 16 + 15;
    dev->cols[0xF0][0][1] = 16;
    dev->cols[0xF0][0][0] = dev->cols[0xF0][1][0] = 
		dev->cols[0xF0][1][1] = 16 + 15;
    dev->cols[0x78][0][1] = 16 + 7;
    dev->cols[0x78][0][0] = dev->cols[0x78][1][0] = 
		dev->cols[0x78][1][1] = 16 + 15;
    dev->cols[0xF8][0][1] = 16 + 7;
    dev->cols[0xF8][0][0] = dev->cols[0xF8][1][0] = 
		dev->cols[0xF8][1][1] = 16 + 15;
    dev->cols[0x00][0][1] = dev->cols[0x00][1][1] = 16;
    dev->cols[0x08][0][1] = dev->cols[0x08][1][1] = 16;
    dev->cols[0x80][0][1] = dev->cols[0x80][1][1] = 16;
    dev->cols[0x88][0][1] = dev->cols[0x88][1][1] = 16;

    overscan_x = overscan_y = 0;

    cga_palette = device_get_config_int("rgb_type") << 1;
    if (cga_palette > 6)
	cga_palette = 0;
    video_palette_rebuild();
}


static void *
mda_standalone_init(const device_t *info, UNUSED(void *parent))
{
    mda_t *dev;

    dev = (mda_t *)mem_alloc(sizeof(mda_t));
    memset(dev, 0x00, sizeof(mda_t));
    dev->type = info->local;

    dev->vram = (uint8_t *)mem_alloc(0x1000);

    mda_init(dev);

    mem_map_add(&dev->mapping, 0xb0000, 0x08000,
		mda_read,NULL,NULL, mda_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, dev);

    io_sethandler(0x03b0, 16,
		  mda_in,NULL,NULL, mda_out,NULL,NULL, dev);

    timer_add(mda_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)&mda_timings);

    /* Force the LPT3 port to be enabled. */
    config.parallel_enabled[2] = 1;
    parallel_setup(2, 0x03bc);

    return(dev);
}


static void
mda_close(void *priv)
{
    mda_t *dev = (mda_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(void *priv)
{
    mda_t *dev = (mda_t *)priv;

    mda_recalctimings(dev);
}


const device_config_t mda_config[] = {
    {
	"rgb_type", "Display type", CONFIG_SELECTION, "", 0,
	{
		{
			"Default", 0
		},
		{
			"Green", 1
		},
		{
			"Amber", 2
		},
		{
			"Gray", 3
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

const device_t mda_device = {
    "MDA",
    DEVICE_VIDEO(VID_TYPE_MDA) | DEVICE_ISA,
    0,
    NULL,
    mda_standalone_init, mda_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    mda_config
};


void
mda_setcol(mda_t *dev, int chr, int blink, int fg, uint8_t cga_ink)
{
    dev->cols[chr][blink][fg] = pal_lookup[cga_ink];
}
