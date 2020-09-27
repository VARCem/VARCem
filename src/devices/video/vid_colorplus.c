/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Plantronics ColorPlus emulation.
 *
 * Version:	@(#)vid_colorplus.c	1.0.17	2019/05/13
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
#include <math.h>
#include "../../emu.h"
#include "../../config.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../ports/parallel.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_cga_comp.h"


/* Bits in the colorplus control register: */
#define COLORPLUS_PLANE_SWAP	0x40	/* Swap planes at 0000h and 4000h */
#define COLORPLUS_640x200_MODE	0x20	/* 640x200x4 mode active */
#define COLORPLUS_320x200_MODE	0x10	/* 320x200x16 mode active */
#define COLORPLUS_EITHER_MODE   0x30	/* Either mode active */

/* Bits in the CGA graphics mode register */
#define CGA_GRAPHICS_MODE       0x02	/* CGA graphics mode selected? */

#define CGA_RGB		0
#define CGA_COMPOSITE	1

#define COMPOSITE_OLD	0
#define COMPOSITE_NEW	1


typedef struct {
    cga_t	cga;
    uint8_t	control;
} colorplus_t;


static const int cols16[16] = {
    0x10,0x12,0x14,0x16, 0x18,0x1a,0x1c,0x1e,
    0x11,0x13,0x15,0x17, 0x19,0x1b,0x1d,0x1f
};


static void
colorplus_out(uint16_t port, uint8_t val, priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    if (port == 0x3dd)
	dev->control = val & 0x70;
    else
	cga_out(port, val, &dev->cga);
}


static uint8_t
colorplus_in(uint16_t port, priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    return cga_in(port, &dev->cga);
}


static void
colorplus_write(uint32_t addr, uint8_t val, priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    if ((dev->control & COLORPLUS_PLANE_SWAP) &&
        (dev->control & COLORPLUS_EITHER_MODE) &&
	(dev->cga.cgamode & CGA_GRAPHICS_MODE)) {
	addr ^= 0x4000;
    } else if (!(dev->control & COLORPLUS_EITHER_MODE)) {
	addr &= 0x3fff;
    }
    dev->cga.vram[addr & 0x7fff] = val;

    if (dev->cga.snow_enabled) {
	dev->cga.charbuffer[((int)(((dev->cga.dispontime - dev->cga.vidtime) * 2) / CGACONST)) & 0xfc] = val;
	dev->cga.charbuffer[(((int)(((dev->cga.dispontime - dev->cga.vidtime) * 2) / CGACONST)) & 0xfc) | 1] = val;
    }

    cycles -= 4;
}


static uint8_t
colorplus_read(uint32_t addr, priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    if ((dev->control & COLORPLUS_PLANE_SWAP) &&
	(dev->control & COLORPLUS_EITHER_MODE) &&
	(dev->cga.cgamode & CGA_GRAPHICS_MODE)) {
	addr ^= 0x4000;
    }	else if (!(dev->control & COLORPLUS_EITHER_MODE)) {
	addr &= 0x3fff;
    }

    cycles -= 4;	

    if (dev->cga.snow_enabled) {
	dev->cga.charbuffer[ ((int)(((dev->cga.dispontime - dev->cga.vidtime) * 2) / CGACONST)) & 0xfc] = dev->cga.vram[addr & 0x7fff];
	dev->cga.charbuffer[(((int)(((dev->cga.dispontime - dev->cga.vidtime) * 2) / CGACONST)) & 0xfc) | 1] = dev->cga.vram[addr & 0x7fff];
    }

    return dev->cga.vram[addr & 0x7fff];
}


static void
colorplus_poll(priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;
    uint8_t *plane0 = dev->cga.vram;
    uint8_t *plane1 = dev->cga.vram + 0x4000;
    int x, c;
    int oldvc;
    uint16_t dat0, dat1;
    int cols[4];
    int col;
    int oldsc;

    /* If one of the extra modes is not selected,
     * fall back to the CGA drawing code.
     */
    if (!((dev->control & COLORPLUS_EITHER_MODE) &&
	      (dev->cga.cgamode & CGA_GRAPHICS_MODE))) {
	cga_poll(&dev->cga);
	return;
    }

    if (! dev->cga.linepos) {
	dev->cga.vidtime += dev->cga.dispofftime;
	dev->cga.cgastat |= 1;
	dev->cga.linepos = 1;
	oldsc = dev->cga.sc;

	if ((dev->cga.crtc[8] & 3) == 3) 
	   dev->cga.sc = ((dev->cga.sc << 1) + dev->cga.oddeven) & 7;

	if (dev->cga.cgadispon) {
		if (dev->cga.displine < dev->cga.firstline) {
			dev->cga.firstline = dev->cga.displine;
			video_blit_wait_buffer();
		}
		dev->cga.lastline = dev->cga.displine;

		/* Left / right border */
		for (c = 0; c < 8; c++) {
			screen->line[dev->cga.displine][c].pal = screen->line[dev->cga.displine][c + (dev->cga.crtc[1] << 4) + 8].pal = (dev->cga.cgacol & 15) + 16;
		}

		if (dev->control & COLORPLUS_320x200_MODE) {
			for (x = 0; x < dev->cga.crtc[1]; x++) {
				dat0 = (plane0[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000)] << 8) |
					plane0[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000) + 1];
				dat1 = (plane1[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000)] << 8) |
					plane1[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000) + 1];
				dev->cga.ma++;

				for (c = 0; c < 8; c++) {
					screen->line[dev->cga.displine][(x << 4) + (c << 1) + 8].pal = screen->line[dev->cga.displine][(x << 4) + (c << 1) + 1 + 8].pal = cols16[(dat0 >> 14) | ((dat1 >> 14) << 2)];
					dat0 <<= 2;
					dat1 <<= 2;
				}
			}
		} else if (dev->control & COLORPLUS_640x200_MODE) {
			cols[0] = (dev->cga.cgacol & 15) | 16;
			col = (dev->cga.cgacol & 16) ? 24 : 16;
			if (dev->cga.cgamode & 4) {
				cols[1] = col | 3;
				cols[2] = col | 4;
				cols[3] = col | 7;
			} else if (dev->cga.cgacol & 32) {
				cols[1] = col | 3;
				cols[2] = col | 5;
				cols[3] = col | 7;
			} else {
				cols[1] = col | 2;
				cols[2] = col | 4;
				cols[3] = col | 6;
			}

			for (x = 0; x < dev->cga.crtc[1]; x++) {
				dat0 = (plane0[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000)] << 8) |
					plane0[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000) + 1];
				dat1 = (plane1[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000)] << 8) |
					plane1[((dev->cga.ma << 1) & 0x1fff) + ((dev->cga.sc & 1) * 0x2000) + 1];
				dev->cga.ma++;

				for (c = 0; c < 16; c++) {
					screen->line[dev->cga.displine][(x << 4) + c + 8].pal = cols[(dat0 >> 15) | ((dat1 >> 15) << 1)];
					dat0 <<= 1;
					dat1 <<= 1;
				}
			}
		}
	} else {	/* Top / bottom border */
		cols[0] = (dev->cga.cgacol & 15) + 16;
		cga_hline(screen, 0, dev->cga.displine, (dev->cga.crtc[1] << 4) + 16, cols[0]);
	}

	x = (dev->cga.crtc[1] << 4) + 16;

	if (dev->cga.composite)
		cga_comp_process(dev->cga.cpriv, dev->cga.cgamode, 0, x >> 2, screen->line[dev->cga.displine]);

	dev->cga.sc = oldsc;
	if (dev->cga.vc == dev->cga.crtc[7] && !dev->cga.sc)
		dev->cga.cgastat |= 8;

	dev->cga.displine++;
	if (dev->cga.displine >= 360) 
		dev->cga.displine = 0;
    } else {
	dev->cga.vidtime += dev->cga.dispontime;
	dev->cga.linepos = 0;
	if (dev->cga.vsynctime) {
		dev->cga.vsynctime--;
		if (! dev->cga.vsynctime)
			dev->cga.cgastat &= ~8;
	}

	if (dev->cga.sc == (dev->cga.crtc[11] & 31) || ((dev->cga.crtc[8] & 3) == 3 && dev->cga.sc == ((dev->cga.crtc[11] & 31) >> 1))) { 
		dev->cga.con = 0; 
		dev->cga.coff = 1; 
	}

	if ((dev->cga.crtc[8] & 3) == 3 && dev->cga.sc == (dev->cga.crtc[9] >> 1))
		dev->cga.maback = dev->cga.ma;

	if (dev->cga.vadj) {
		dev->cga.sc++;
		dev->cga.sc &= 31;
		dev->cga.ma = dev->cga.maback;
		dev->cga.vadj--;
		if (! dev->cga.vadj) {
			dev->cga.cgadispon = 1;
			dev->cga.ma = dev->cga.maback = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;
			dev->cga.sc = 0;
		}
	} else if (dev->cga.sc == dev->cga.crtc[9]) {
		dev->cga.maback = dev->cga.ma;
		dev->cga.sc = 0;
		oldvc = dev->cga.vc;
		dev->cga.vc++;
		dev->cga.vc &= 127;

		if (dev->cga.vc == dev->cga.crtc[6]) 
			dev->cga.cgadispon = 0;

		if (oldvc == dev->cga.crtc[4]) {
			dev->cga.vc = 0;
			dev->cga.vadj = dev->cga.crtc[5];
			if (! dev->cga.vadj)
				dev->cga.cgadispon = 1;
			if (! dev->cga.vadj)
				dev->cga.ma = dev->cga.maback = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;
			if ((dev->cga.crtc[10] & 0x60) == 0x20)
				dev->cga.cursoron = 0;
			else
				dev->cga.cursoron = dev->cga.cgablink & 8;
		}

		if (dev->cga.vc == dev->cga.crtc[7]) {
			dev->cga.cgadispon = 0;
			dev->cga.displine = 0;
			dev->cga.vsynctime = 16;
			if (dev->cga.crtc[7]) {
				if (dev->cga.cgamode & 1)
					x = (dev->cga.crtc[1] << 3) + 16;
				else
					x = (dev->cga.crtc[1] << 4) + 16;
				dev->cga.lastline++;

				if (x != xsize || (dev->cga.lastline - dev->cga.firstline) != ysize) {
					xsize = x;
					ysize = dev->cga.lastline - dev->cga.firstline;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, (ysize << 1) + 16);
				}
					
				if (dev->cga.composite) 
					video_blit_start(0, 0, dev->cga.firstline - 4, 0, (dev->cga.lastline - dev->cga.firstline) + 8, xsize, (dev->cga.lastline - dev->cga.firstline) + 8);
				else	  
					video_blit_start(1, 0, dev->cga.firstline - 4, 0, (dev->cga.lastline - dev->cga.firstline) + 8, xsize, (dev->cga.lastline - dev->cga.firstline) + 8);
				frames++;

				video_res_x = xsize - 16;
				video_res_y = ysize;
				if (dev->cga.cgamode & 1) {
					video_res_x /= 8;
					video_res_y /= dev->cga.crtc[9] + 1;
					video_bpp = 0;
				} else if (!(dev->cga.cgamode & 2)) {
					video_res_x /= 16;
					video_res_y /= dev->cga.crtc[9] + 1;
					video_bpp = 0;
				} else if (!(dev->cga.cgamode & 16)) {
					video_res_x /= 2;
					video_bpp = 2;
				} else {
					video_bpp = 1;
				}
			}
			dev->cga.firstline = 1000;
			dev->cga.lastline = 0;
			dev->cga.cgablink++;
			dev->cga.oddeven ^= 1;
		}
	} else {
		dev->cga.sc++;
		dev->cga.sc &= 31;
		dev->cga.ma = dev->cga.maback;
	}

	if (dev->cga.cgadispon)
		dev->cga.cgastat &= ~1;
	if ((dev->cga.sc == (dev->cga.crtc[10] & 31) || ((dev->cga.crtc[8] & 3) == 3 && dev->cga.sc == ((dev->cga.crtc[10] & 31) >> 1)))) 
		dev->cga.con = 1;

	if (dev->cga.cgadispon && (dev->cga.cgamode & 1)) {
		for (x = 0; x < (dev->cga.crtc[1] << 1); x++)
			dev->cga.charbuffer[x] = dev->cga.vram[(((dev->cga.ma << 1) + x) & 0x3fff)];
	}
    }
}


static priv_t
colorplus_init(const device_t *info, UNUSED(void *parent))
{
    colorplus_t *dev;
    int display_type;

    dev = (colorplus_t *)mem_alloc(sizeof(colorplus_t));
    memset(dev, 0x00, sizeof(colorplus_t));

    display_type = device_get_config_int("display_type");
    dev->cga.composite = (display_type != CGA_RGB);
    dev->cga.revision = device_get_config_int("composite_type");
    dev->cga.snow_enabled = device_get_config_int("snow_enabled");

    dev->cga.vram = (uint8_t *)mem_alloc(0x8000);

    dev->cga.cpriv = cga_comp_init(1);

    timer_add(colorplus_poll, (priv_t)dev, &dev->cga.vidtime, TIMER_ALWAYS_ENABLED);

    mem_map_add(&dev->cga.mapping, 0xb8000, 0x08000,
	 	colorplus_read,NULL,NULL, colorplus_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, (priv_t)dev);

    io_sethandler(0x03d0, 16,
		  colorplus_in,NULL,NULL, colorplus_out,NULL,NULL, (priv_t)dev);

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    /* Force the LPT3 port to be enabled. */
    config.parallel_enabled[2] = 1;
    parallel_setup(2, 0x03bc);

    return (priv_t)dev;
}


static void
colorplus_close(priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    if (dev->cga.cpriv != NULL)
	cga_comp_close(dev->cga.cpriv);

    free(dev->cga.vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    colorplus_t *dev = (colorplus_t *)priv;

    cga_recalctimings(&dev->cga);
}


static const device_config_t colorplus_config[] = {
    {
	"display_type", "Display type", CONFIG_SELECTION, "", CGA_RGB,
	{
		{
			"RGB", CGA_RGB
		},
		{
			"Composite", CGA_COMPOSITE
		},
		{
			NULL
		}
	}
    },
    {
	"composite_type", "Composite type", CONFIG_SELECTION, "", COMPOSITE_OLD,
	{
		{
			"Old", COMPOSITE_OLD
		},
		{
			"New", COMPOSITE_NEW
		},
		{
			NULL
		}
	}
    },
    {
	"snow_enabled", "Snow emulation", CONFIG_BINARY, "", 1
    },
    {
	NULL
    }
};

static const video_timings_t colorplus_timings = { VID_ISA,8,16,32,8,16,32 };

const device_t colorplus_device = {
    "Plantronics ColorPlus",
    DEVICE_VIDEO(VID_TYPE_CGA) | DEVICE_ISA,
    0,
    NULL,
    colorplus_init, colorplus_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &colorplus_timings,
    colorplus_config
};

const device_t colorplus_onboard_device = {
    "Onboard Plantronics ColorPlus",
    DEVICE_VIDEO(VID_TYPE_CGA) | DEVICE_ISA,
    0,
    NULL,
    colorplus_init, colorplus_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &colorplus_timings,
    colorplus_config};
