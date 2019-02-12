/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Hercules emulation.
 *
 * Version:	@(#)vid_hercules.c	1.0.13	2019/02/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../io.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../system/pit.h"
#include "../ports/parallel.h"
#include "video.h"


typedef struct {
    mem_map_t	mapping;

    uint8_t	crtc[32];
    int		crtcreg;

    uint8_t	ctrl,
		ctrl2,
		stat;

    int64_t	dispontime,
		dispofftime;
    int64_t	vidtime;

    int		firstline,
		lastline;

    int		linepos,
		displine;
    int		vc,
		sc;
    uint16_t	ma,
		maback;
    int		con, coff,
		cursoron;
    int		dispon,
		blink;
    int		vadj;
    int		blend;
    int64_t	vsynctime;

    int		cols[256][2][2];

    uint8_t	*vram;
} hercules_t;


static const int	ws_array[16] = {3,4,5,6,7,8,4,5,6,7,8,4,5,6,7,8};


static void
recalc_timings(hercules_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    disptime = dev->crtc[0] + 1;
    _dispontime  = dev->crtc[1];
    _dispofftime = disptime - _dispontime;
    _dispontime  *= MDACONST;
    _dispofftime *= MDACONST;

    dev->dispontime  = (int64_t)(_dispontime  * (1LL << TIMER_SHIFT));
    dev->dispofftime = (int64_t)(_dispofftime * (1LL << TIMER_SHIFT));
}


static void
hercules_out(uint16_t addr, uint8_t val, void *priv)
{
    hercules_t *dev = (hercules_t *)priv;
    uint8_t old;

    switch (addr) {
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
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val;

		/*
		 * Fix for Generic Turbo XT BIOS, which
		 * sets up cursor registers wrong.
		 */
		if (dev->crtc[10] == 6 && dev->crtc[11] == 7) {
			dev->crtc[10] = 0xb;
			dev->crtc[11] = 0xc;
		}

		if (old ^ val)
			recalc_timings(dev);
		break;

	case 0x03b8:
		old = dev->ctrl;
		dev->ctrl = val;
		if (old ^ val)
			recalc_timings(dev);
		break;

	case 0x03bf:
		dev->ctrl2 = val;
		if (val & 0x02)
			mem_map_set_addr(&dev->mapping, 0xb0000, 0x10000);
		  else
			mem_map_set_addr(&dev->mapping, 0xb0000, 0x08000);
		break;

	default:
		break;
    }
}


static uint8_t
hercules_in(uint16_t addr, void *priv)
{
    hercules_t *dev = (hercules_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
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
		ret = 0x72;	/* Hercules ident */
		if (dev->stat & 0x08)
			ret |= 0x88;
		break;

	default:
		break;
    }

    return(ret);
}


static void
hercules_waitstates(void *p)
{
    int ws;

    ws = ws_array[cycles & 0xf];

    cycles -= ws;
}


static void
hercules_write(uint32_t addr, uint8_t val, void *priv)
{
    hercules_t *dev = (hercules_t *)priv;

    dev->vram[addr & 0xffff] = val;
    hercules_waitstates(dev);
}


static uint8_t
hercules_read(uint32_t addr, void *priv)
{
    hercules_t *dev = (hercules_t *)priv;

    hercules_waitstates(dev);

    return(dev->vram[addr & 0xffff]);
}


static void
hercules_poll(void *priv)
{
    hercules_t *dev = (hercules_t *)priv;
    uint8_t chr, attr;
    uint16_t ca, dat;
    int oldsc, blink;
    int x, c, oldvc;
    int drawcursor;

    ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;

	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = (dev->sc << 1) & 7;

	if (dev->dispon) {
		if (dev->displine < dev->firstline) {
				dev->firstline = dev->displine;
				video_wait_for_buffer();
		}
		dev->lastline = dev->displine;

		if ((dev->ctrl & 2) && (dev->ctrl2 & 1)) {
			ca = (dev->sc & 3) * 0x2000;
			if ((dev->ctrl & 0x80) && (dev->ctrl2 & 2)) 
				ca += 0x8000;

			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->ctrl & 8)
					dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ca] << 8) | dev->vram[((dev->ma << 1) & 0x1fff) + ca + 1];
				else
					dat = 0;
				dev->ma++;
				for (c = 0; c < 16; c++) {
				    buffer->line[dev->displine][(x << 4) + c] = (dat & (32768 >> c)) ? 7 : 0;

				}

				if (dev->blend) {
					for (c = 0; c < 16; c += 8)
						video_blend((x << 4) + c, dev->displine);
				}
			}
		} else {
			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->ctrl & 8) {
					chr  = dev->vram[(dev->ma << 1) & 0xfff];
					attr = dev->vram[((dev->ma << 1) + 1) & 0xfff];
				} else
					chr = attr = 0;
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				blink = ((dev->blink & 16) && (dev->ctrl & 0x20) && (attr & 0x80) && !drawcursor);

				if (dev->sc == 12 && ((attr & 7) == 1)) {
					for (c = 0; c < 9; c++)
					    buffer->line[dev->displine][(x * 9) + c] = dev->cols[attr][blink][1];
				} else {
					for (c = 0; c < 8; c++)
					    buffer->line[dev->displine][(x * 9) + c] = dev->cols[attr][blink][(fontdatm[chr][dev->sc] & (1 << (c ^ 7))) ? 1 : 0];

					if ((chr & ~0x1f) == 0xc0)
						buffer->line[dev->displine][(x * 9) + 8] = dev->cols[attr][blink][fontdatm[chr][dev->sc] & 1];
					  else
						buffer->line[dev->displine][(x * 9) + 8] = dev->cols[attr][blink][0];
				}
				dev->ma++;

				if (drawcursor) {
					for (c = 0; c < 9; c++)
					    buffer->line[dev->displine][(x * 9) + c] ^= dev->cols[attr][0][1];
				}
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

	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
				dev->stat &= ~8;
	}

	if (dev->sc == (dev->crtc[11] & 31) ||
	    ((dev->crtc[8] & 3)==3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) {
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
			if (! dev->vadj)
				dev->dispon = 1;
			if (! dev->vadj)
				dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			  else
				dev->cursoron = dev->blink & 16;
		}

		if (dev->vc == dev->crtc[7]) {
			dev->dispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;//(crtcm[3]>>4)+1;
			if (dev->crtc[7]) {
				if ((dev->ctrl & 2) && (dev->ctrl2 & 1))
					x = dev->crtc[1] << 4;
				else
					x = dev->crtc[1] * 9;

				dev->lastline++;
				if ((dev->ctrl & 8) &&
				    ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) || video_force_resize_get())) {
					xsize = x;
					ysize = dev->lastline - dev->firstline;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, ysize);

					if (video_force_resize_get())
						video_force_resize_set(0);
				}

				video_blit_memtoscreen_8(0, dev->firstline, 0, ysize, xsize, ysize);
				frames++;
				if ((dev->ctrl & 2) && (dev->ctrl2 & 1)) {
					video_res_x = dev->crtc[1] * 16;
					video_res_y = dev->crtc[6] * 4;
					video_bpp = 1;
				} else {
					video_res_x = dev->crtc[1];
					video_res_y = dev->crtc[6];
					video_bpp = 0;
				}
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

	if (dev->dispon)
		dev->stat &= ~1;

	if ((dev->sc == (dev->crtc[10] & 31) ||
	    ((dev->crtc[8] & 3)==3 && dev->sc == ((dev->crtc[10] & 31) >> 1))))
		dev->con = 1;
    }
}


static void *
hercules_init(const device_t *info)
{
    hercules_t *dev;
    int c;

    dev = (hercules_t *)mem_alloc(sizeof(hercules_t));
    memset(dev, 0x00, sizeof(hercules_t));

    dev->blend = device_get_config_int("blend");

    dev->vram = (uint8_t *)mem_alloc(0x10000);

    timer_add(hercules_poll, &dev->vidtime, TIMER_ALWAYS_ENABLED, dev);

    /*
     * Map in the memory, enable exec on it (for software like basich.com
     * that relocates code to it, and executes it from there..)
     */
    mem_map_add(&dev->mapping, 0xb0000, 0x08000,
		hercules_read,NULL,NULL, hercules_write,NULL,NULL,
		dev->vram, MEM_MAPPING_EXTERNAL, dev);

    io_sethandler(0x03b0, 16,
		  hercules_in,NULL,NULL, hercules_out,NULL,NULL, dev);

    for (c = 0; c < 256; c++) {
	dev->cols[c][0][0] = dev->cols[c][1][0] = dev->cols[c][1][1] = 16;

	if (c & 0x08)
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
    cgapal_rebuild();

    video_inform(VID_TYPE_MDA, info->vid_timing);

    /* Force the LPT3 port to be enabled. */
    parallel_enabled[2] = 1;
    parallel_setup(2, 0x03bc);

    return(dev);
}


static void
hercules_close(void *priv)
{
    hercules_t *dev = (hercules_t *)priv;

    if (! dev)
	return;

    if (dev->vram)
	free(dev->vram);

    free(dev);
}


static void
speed_changed(void *priv)
{
    hercules_t *dev = (hercules_t *)priv;
	
    recalc_timings(dev);
}


static const device_config_t hercules_config[] = {
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
			""
		}
	}
    },
    {
	"blend", "Blend", CONFIG_BINARY, "", 1
    },
    {
	"", "", -1
    }
};

static const video_timings_t hercules_timings = { VID_ISA,8,16,32,8,16,32 };

const device_t hercules_device = {
    "Hercules",
    DEVICE_ISA,
    0,
    hercules_init, hercules_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &hercules_timings,
    hercules_config
};
