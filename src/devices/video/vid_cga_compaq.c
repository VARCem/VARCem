/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of CGA used by Compaq PC's.
 *
 * Version:	@(#)vid_cga_compaq.c	1.0.5	2019/02/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../system/pit.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_cga_comp.h"


#define CGA_RGB		0
#define CGA_COMPOSITE	1


typedef struct {
    cga_t	cga;
    uint32_t	flags;

    uint8_t	attr[256][2][2];
} compaq_cga_t;


static void
recalc_timings(compaq_cga_t *dev)
{
    double _dispontime, _dispofftime, disptime;
    disptime = dev->cga.crtc[0] + 1;

    _dispontime = dev->cga.crtc[1];
    _dispofftime = disptime - _dispontime;
    _dispontime *= MDACONST;
    _dispofftime *= MDACONST;

    dev->cga.dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
    dev->cga.dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
compaq_poll(void *priv)
{
    compaq_cga_t *dev = (compaq_cga_t *)priv;
    uint16_t ca = (dev->cga.crtc[15] | (dev->cga.crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint32_t cols[4];
    int oldsc;
    int underline = 0;
    int blink = 0;

    /* If in graphics mode or character height is not 13, behave as CGA */
    if ((dev->cga.cgamode & 0x12) || (dev->cga.crtc[9] != 13)) {
	overscan_x = overscan_y = 16;
	cga_poll(&dev->cga);
	return;
    } else
	overscan_x = overscan_y = 0;

    /* We are in Compaq 350-line CGA territory */
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
			video_wait_for_buffer();
		}
		dev->cga.lastline = dev->cga.displine;
		       
		cols[0] = (dev->cga.cgacol & 15);

		for (c = 0; c < 8; c++) {
			((uint32_t *)buffer32->line[dev->cga.displine])[c] = cols[0];
			if (dev->cga.cgamode & 1)
				((uint32_t *)buffer32->line[dev->cga.displine])[c + (dev->cga.crtc[1] << 3) + 8] = cols[0];
			else
				((uint32_t *)buffer32->line[dev->cga.displine])[c + (dev->cga.crtc[1] << 4) + 8] = cols[0];
		}

		if (dev->cga.cgamode & 1) {
			for (x = 0; x < dev->cga.crtc[1]; x++) {
				chr = dev->cga.charbuffer[x << 1];
				attr = dev->cga.charbuffer[(x << 1) + 1];
				drawcursor = ((dev->cga.ma == ca) && dev->cga.con && dev->cga.cursoron);
				if (dev->flags) {
					underline = 0;
					blink = ((dev->cga.cgablink & 8) && (dev->cga.cgamode & 0x20) && (attr & 0x80) && !drawcursor);
				}

				if (dev->flags && (dev->cga.cgamode & 0x80)) {
					cols[0] = dev->attr[attr][blink][0];
					cols[1] = dev->attr[attr][blink][1];
					if (dev->cga.sc == 12 && (attr & 7) == 1) underline = 1;
				} else if (dev->cga.cgamode & 0x20) {
					cols[1] = (attr & 15) + 16;
					cols[0] = ((attr >> 4) & 7) + 16;
					if (dev->flags) {
						if (blink)
							cols[1] = cols[0];
					} else {
						if ((dev->cga.cgablink & 8) && (attr & 0x80) && !dev->cga.drawcursor) 
							cols[1] = cols[0];
					}
				} else {
					cols[1] = (attr & 15) + 16;
					cols[0] = (attr >> 4) + 16;
				}

				if (dev->flags && underline) {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 3) + c + 8] = dev->attr[attr][blink][1];
				} else if (drawcursor) {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 3) + c + 8] = cols[(fontdatm[chr + dev->cga.fontbase][dev->cga.sc & 15] & (1 << (c ^ 7))) ? 1 : 0] ^ 0xffffff;
				} else {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 3) + c + 8] = cols[(fontdatm[chr + dev->cga.fontbase][dev->cga.sc & 15] & (1 << (c ^ 7))) ? 1 : 0];
				}
				dev->cga.ma++;
			}
		} else {
			for (x = 0; x < dev->cga.crtc[1]; x++) {
				chr  = dev->cga.vram[((dev->cga.ma << 1) & 0x3fff)];
				attr = dev->cga.vram[(((dev->cga.ma << 1) + 1) & 0x3fff)];
				drawcursor = ((dev->cga.ma == ca) && dev->cga.con && dev->cga.cursoron);
				if (dev->flags) {
					underline = 0;
					blink = ((dev->cga.cgablink & 8) && (dev->cga.cgamode & 0x20) && (attr & 0x80) && !drawcursor);
				}

				if (dev->flags && (dev->cga.cgamode & 0x80)) {
					cols[0] = dev->attr[attr][blink][0];
					cols[1] = dev->attr[attr][blink][1];
					if (dev->cga.sc == 12 && (attr & 7) == 1) underline = 1;
				} else if (dev->cga.cgamode & 0x20) {
					cols[1] = (attr & 15) + 16;
					cols[0] = ((attr >> 4) & 7) + 16;
					if (dev->flags) {
						if (blink)
							cols[1] = cols[0];
					} else {
						if ((dev->cga.cgablink & 8) && (attr & 0x80) && !dev->cga.drawcursor) 
							cols[1] = cols[0];
					}
				} else {
					cols[1] = (attr & 15) + 16;
					cols[0] = (attr >> 4) + 16;
				}
				dev->cga.ma++;

				if (dev->flags && underline) {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4)+(c << 1) + 8] = ((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4)+(c << 1) + 9] = dev->attr[attr][blink][1];
				} else if (drawcursor) {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4)+(c << 1) + 8] = ((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4) + (c << 1) + 1 + 8] = cols[(fontdatm[chr + dev->cga.fontbase][dev->cga.sc & 15] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
						((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4) + (c << 1) + 8] = ((uint32_t *)buffer32->line[dev->cga.displine])[(x << 4) + (c << 1) + 1 + 8] = cols[(fontdatm[chr + dev->cga.fontbase][dev->cga.sc & 15] & (1 << (c ^ 7))) ? 1 : 0];
				}
			}
		}
	} else {
		cols[0] = (dev->cga.cgacol & 15) + 16;
		if (dev->cga.cgamode & 1)
			cga_hline(buffer32, 0, dev->cga.displine, (dev->cga.crtc[1] << 3) + 16, cols[0]);
		else
			cga_hline(buffer32, 0, dev->cga.displine, (dev->cga.crtc[1] << 4) + 16, cols[0]);
		}

		if (dev->cga.cgamode & 1)
			x = (dev->cga.crtc[1] << 3) + 16;
		else
			x = (dev->cga.crtc[1] << 4) + 16;

		if (dev->cga.composite) {
			for (c = 0; c < x; c++)
				buffer32->line[dev->cga.displine][c] = ((uint32_t *)buffer32->line[dev->cga.displine])[c] & 0xf;

		if (dev->flags)
			Composite_Process(dev->cga.cgamode & 0x7F, 0, x >> 2, buffer32->line[dev->cga.displine]);
		else
			Composite_Process(dev->cga.cgamode, 0, x >> 2, buffer32->line[dev->cga.displine]);
	} else {
		for (c = 0; c < x; c++)
			buffer->line[dev->cga.displine][c] = ((uint32_t *)buffer32->line[dev->cga.displine])[c];
	}			

	dev->cga.sc = oldsc;
	if (dev->cga.vc == dev->cga.crtc[7] && !dev->cga.sc)
		dev->cga.cgastat |= 8;
	dev->cga.displine++;
	if (dev->cga.displine >= 500) 
		dev->cga.displine = 0;
    } else {
	dev->cga.vidtime += dev->cga.dispontime;
	dev->cga.linepos = 0;
	if (dev->cga.vsynctime) {
		dev->cga.vsynctime--;
		if (!dev->cga.vsynctime)
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
					x = (dev->cga.crtc[1] << 3);
				else
					x = (dev->cga.crtc[1] << 4);
				dev->cga.lastline++;

				if (x != xsize || (dev->cga.lastline - dev->cga.firstline) != ysize) {
					xsize= x;
					ysize= dev->cga.lastline - dev->cga.firstline;

					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, ysize);
				}
					
				if (dev->cga.composite) 
					video_blit_memtoscreen(8, dev->cga.firstline, 0, ysize, xsize, ysize);
				else	  
					video_blit_memtoscreen_8(8, dev->cga.firstline, 0, ysize, xsize, ysize);
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


static void *
compaq_cga_init(const device_t *info)
{
    compaq_cga_t *dev;
    int c, display_type;

    dev = (compaq_cga_t *)mem_alloc(sizeof(compaq_cga_t));
    memset(dev, 0x00, sizeof(compaq_cga_t));

    display_type = device_get_config_int("display_type");
    dev->cga.composite = (display_type != CGA_RGB);
    dev->cga.revision = device_get_config_int("composite_type");
    dev->cga.snow_enabled = device_get_config_int("snow_enabled");
    dev->cga.rgb_type = device_get_config_int("rgb_type");

    dev->cga.vram = (uint8_t *)mem_alloc(0x4000);

    cga_comp_init(dev->cga.revision);

    timer_add(compaq_poll, &dev->cga.vidtime, TIMER_ALWAYS_ENABLED, dev);

    mem_map_add(&dev->cga.mapping, 0xb8000, 0x08000,
		cga_read,NULL,NULL, cga_write,NULL,NULL,
		dev->cga.vram, MEM_MAPPING_EXTERNAL, dev);

    io_sethandler(0x03d0, 16,
		  cga_in,NULL,NULL, cga_out,NULL,NULL, dev);

    if (info->local) {
	for (c = 0; c < 256; c++) {
		dev->attr[c][0][0] = dev->attr[c][1][0] = dev->attr[c][1][1] = 16;
		if (c & 8)
			dev->attr[c][0][1] = 15 + 16;
		else
			dev->attr[c][0][1] =  7 + 16;
	}
	dev->attr[0x70][0][1] = 16;
	dev->attr[0x70][0][0] = dev->attr[0x70][1][0] = dev->attr[0x70][1][1] = 16 + 15;
	dev->attr[0xF0][0][1] = 16;
	dev->attr[0xF0][0][0] = dev->attr[0xF0][1][0] = dev->attr[0xF0][1][1] = 16 + 15;
	dev->attr[0x78][0][1] = 16 + 7;
	dev->attr[0x78][0][0] = dev->attr[0x78][1][0] = dev->attr[0x78][1][1] = 16 + 15;
	dev->attr[0xF8][0][1] = 16 + 7;
	dev->attr[0xF8][0][0] = dev->attr[0xF8][1][0] = dev->attr[0xF8][1][1] = 16 + 15;
	dev->attr[0x00][0][1] = dev->attr[0x00][1][1] = 16;
	dev->attr[0x08][0][1] = dev->attr[0x08][1][1] = 16;
	dev->attr[0x80][0][1] = dev->attr[0x80][1][1] = 16;
	dev->attr[0x88][0][1] = dev->attr[0x88][1][1] = 16;
    }

    dev->flags = info->local;

    overscan_x = overscan_y = 16;

    cga_palette = (dev->cga.rgb_type << 1);
    cgapal_rebuild();

    video_inform(VID_TYPE_CGA, info->vid_timing);

    return dev;
}


static void
compaq_cga_close(void *priv)
{
    compaq_cga_t *dev = (compaq_cga_t *)priv;

    free(dev->cga.vram);
    free(dev);
}


static void
speed_changed(void *priv)
{
    compaq_cga_t *dev = (compaq_cga_t *)priv;
 
    if (dev->cga.crtc[9] == 13)		/* Character height */
	recalc_timings(dev);
      else
	cga_recalctimings(&dev->cga);
}


extern const device_config_t cga_config[];
static const video_timings_t cga_timings = { VID_ISA,8,16,32,8,16,32 };

const device_t cga_compaq_device = {
    "Compaq CGA",
    DEVICE_ISA, 0,
    compaq_cga_init, compaq_cga_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &cga_timings,
    cga_config
};

const device_t cga_compaq2_device = {
    "Compaq CGA 2",
    DEVICE_ISA, 1,
    compaq_cga_init, compaq_cga_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &cga_timings,
    cga_config
};
