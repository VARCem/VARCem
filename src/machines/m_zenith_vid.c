/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the display system for the SupersPort
 *		machine, which is a CGA-ish design based around a V6355
 *		controller chip made by Yamaha.
 *
 *		The LCD "part" of this code is based on the plasma driver
 *		for the Toshiba T1000, written by John Elliott.
 *
 * **NOTES**	Although this code sort-of works, much more work has to be
 *		done on implementing other parts of the Yamaha V6355 chip
 *		that implements the video controller.
 *
 * Version:	@(#)m_zenith_vid.c	1.0.3	2019/04/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *              John Elliott, <jce@seasip.info>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *              Copyright 2017,2018 John Elliott.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../timer.h"
#include "../device.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../plat.h"
#include "m_zenith.h"


#define LCD_XSIZE	640
#define LCD_YSIZE	200


static video_timings_t timing_zenith = {VID_ISA, 8,16,32, 8,16,32};


typedef struct {
    int		internal,		/* using internal display? */
		st_internal;

    uint8_t	attrmap;

    mem_map_t	mapping;

    cga_t	cga;

    uint8_t	bank_indx;		/* V6355 extended bank */
    uint8_t	bank_data[0x6a];	/* V6355 extended bank */

    int64_t	dispontime,
		dispofftime,
 		vsynctime;

    int		linepos, displine;
    int		vc, dispon;

    uint8_t	*vram;

    /* Mapping of attributes to colors. */
    uint32_t	fgcol,
		bgcol;
    uint8_t	boldcols[256];	/* which attributes use the bold font */
    uint32_t	blinkcols[256][2];
    uint32_t	normcols[256][2];
} vid_t;


static void
recalc_timings(vid_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    if (dev->internal) {
	disptime = 651;
	_dispontime = 640;
	_dispofftime = disptime - _dispontime;

	dev->dispontime  = (int64_t)(_dispontime  * (1 << TIMER_SHIFT));
	dev->dispofftime = (int64_t)(_dispofftime * (1 << TIMER_SHIFT));
    } else
	cga_recalctimings(&dev->cga);
}


/* Draw a row of text in 80-column mode */
static void
vid_text_80(vid_t *dev)
{
    uint32_t cols[2];
    int x, c;
    uint8_t chr, attr;
    int drawcursor;
    int cursorline;
    int bold;
    int blink;
    uint16_t addr;
    uint8_t sc;
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;
    uint16_t ca = (dev->cga.crtc[15] | (dev->cga.crtc[14] << 8)) & 0x3fff;

    sc = (dev->displine) & 7;
    addr = ((ma & ~1) + (dev->displine >> 3) * 80) * 2;
    ma += (dev->displine >> 3) * 80;

    if ((dev->cga.crtc[10] & 0x60) == 0x20) {
	cursorline = 0;
    } else {
	cursorline = ((dev->cga.crtc[10] & 0x0f) <= sc) &&
		     ((dev->cga.crtc[11] & 0x0f) >= sc);
    }

    for (x = 0; x < 80; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x3fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x3fff];
	       drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	bold = dev->boldcols[attr] ? chr + 256 : chr;

	if (dev->cga.cgamode & 0x20) {
		/* Blink */
		cols[1] = dev->blinkcols[attr][1]; 		
		cols[0] = dev->blinkcols[attr][0]; 		
		if (blink)
			cols[1] = cols[0];
	} else {
		cols[1] = dev->normcols[attr][1];
		cols[0] = dev->normcols[attr][0];
	}

	if (drawcursor) {
	       	for (c = 0; c < 8; c++)
	       		screen->line[dev->displine][(x << 3) + c].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->fgcol ^ dev->bgcol);
	} else {
	       	for (c = 0; c < 8; c++)
			screen->line[dev->displine][(x << 3) + c].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0];
	}

	ma++;
    }
}


/* Draw a row of text in 40-column mode. */
static void
vid_text_40(vid_t *dev)
{
    uint32_t cols[2];
    int x, c;
    uint8_t chr, attr;
    int drawcursor;
    int cursorline;
    int bold;
    int blink;
    uint16_t addr;
    uint8_t sc;
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;
    uint16_t ca = (dev->cga.crtc[15] | (dev->cga.crtc[14] << 8)) & 0x3fff;

    sc = (dev->displine) & 7;
    addr = ((ma & ~1) + (dev->displine >> 3) * 40) * 2;
    ma += (dev->displine >> 3) * 40;

    if ((dev->cga.crtc[10] & 0x60) == 0x20) {
	cursorline = 0;
    } else {
	cursorline = ((dev->cga.crtc[10] & 0x0f) <= sc) &&
		     ((dev->cga.crtc[11] & 0x0f) >= sc);
    }

    for (x = 0; x < 40; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x3fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x3fff];
	drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	bold = dev->boldcols[attr] ? chr + 256 : chr;

	if (dev->cga.cgamode & 0x20) {
		/* Blink */
		cols[1] = dev->blinkcols[attr][1]; 		
		cols[0] = dev->blinkcols[attr][0]; 		
		if (blink)
			cols[1] = cols[0];
	} else {
		cols[1] = dev->normcols[attr][1];
		cols[0] = dev->normcols[attr][0];
	}

	if (drawcursor) {
		for (c = 0; c < 8; c++) {
			screen->line[dev->displine][(x << 4) + c*2].val = 
			  screen->line[dev->displine][(x << 4) + c*2 + 1].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->fgcol ^ dev->bgcol);
		}
	} else {
		for (c = 0; c < 8; c++) {
			screen->line[dev->displine][(x << 4) + c*2].val = 
			  screen->line[dev->displine][(x << 4) + c*2+1].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0];
		}
	}

	ma++;
    }
}


/* Draw a line in CGA 640x200 mode */
static void
vid_cgaline6(vid_t *dev)
{
    int x, c;
    uint8_t dat;
    uint32_t ink = 0;
    uint16_t addr;
    uint32_t fg = (dev->cga.cgacol & 0x0f) ? dev->fgcol : dev->bgcol;
    uint32_t bg = dev->bgcol;
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;

    addr = ((dev->displine) & 1) * 0x2000 +
	    (dev->displine >> 1) * 80 + ((ma & ~1) << 1);

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr & 0x3fff];
	addr++;

	for (c = 0; c < 8; c++) {
		ink = (dat & 0x80) ? fg : bg;
		if (! (dev->cga.cgamode & 8))
			ink = dev->bgcol;
		screen->line[dev->displine][x*8+c].val = ink;
		dat = dat << 1;
	}
    }
}


/* Draw a line in CGA 320x200 mode. Here the CGA colours are converted to
 * dither patterns: colour 1 to 25% grey, colour 2 to 50% grey */
static void
vid_cgaline4(vid_t *dev)
{
    int x, c;
    uint8_t dat, pattern;
    uint32_t ink0, ink1;
    uint16_t addr;
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;

    addr = ((dev->displine) & 1) * 0x2000 +
	    (dev->displine >> 1) * 80 + ((ma & ~1) << 1);

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr & 0x3fff];
	addr++;

	for (c = 0; c < 4; c++) {
		pattern = (dat & 0xc0) >> 6;
		if (! (dev->cga.cgamode & 8))
			pattern = 0;

		switch (pattern & 3) {
			default:
			case 0:
				ink0 = ink1 = dev->bgcol; break;

			case 1:
				if (dev->displine & 1) {
					ink0 = dev->bgcol; ink1 = dev->bgcol;
				} else {
					ink0 = dev->fgcol; ink1 = dev->bgcol;
				}
				break;

			case 2: if (dev->displine & 1) {
					ink0 = dev->bgcol; ink1 = dev->fgcol;
				} else {
					ink0 = dev->fgcol; ink1 = dev->bgcol;
				}
				break;

			case 3:
				ink0 = ink1 = dev->fgcol; break;
		}

		screen->line[dev->displine][x*8+2*c].val = ink0;
		screen->line[dev->displine][x*8+2*c+1].val = ink1;
		dat = dat << 2;
	}
    }
}


static void
vid_poll(void *priv)
{
    vid_t *dev = (vid_t *)priv;

    /* Switch between internal LCD and external CRT display. */
    if (dev->st_internal != -1 && dev->st_internal != dev->internal) {
	dev->internal = dev->st_internal;
	recalc_timings(dev);
    }

    if (! dev->internal) {
	cga_poll(&dev->cga);
	return;
    }

    if (! dev->linepos) {
	dev->cga.vidtime += dev->dispofftime;
	dev->cga.cgastat |= 1;
	dev->linepos = 1;

	if (dev->dispon) {
		if (dev->displine == 0) {
			video_blit_wait_buffer();
		}

		if (dev->cga.cgamode & 0x02) {
			/* Graphics */
			if (dev->cga.cgamode & 0x10)
				vid_cgaline6(dev);
			else
				vid_cgaline4(dev);
		} else if (dev->cga.cgamode & 0x01) {
			 /* High-res text */
			vid_text_80(dev); 
		} else
			vid_text_40(dev); 
	}

	dev->displine++;

	/* Hardcode a fixed refresh rate and VSYNC timing. */
	if (dev->displine == 200) {
		/* Start of VSYNC */
		dev->cga.cgastat |= 8;
		dev->dispon = 0;
	}

	if (dev->displine == 216) {
		/* End of VSYNC */
		dev->displine = 0;
		dev->cga.cgastat &= ~8;
		dev->dispon = 1;
	}
    } else {
	if (dev->dispon) {
		dev->cga.cgastat &= ~1;
	}
	dev->cga.vidtime += dev->dispontime;
	dev->linepos = 0;

	if (dev->displine == 200) {
		/* Hardcode 640x200 window size. */
		if (LCD_XSIZE != xsize || LCD_YSIZE != ysize) {
			xsize = LCD_XSIZE;
			ysize = LCD_YSIZE;
			if (xsize < 64)
				xsize = 656;
			if (ysize < 32)
				ysize = 200;
			set_screen_size(xsize, ysize);
		}

		video_blit_start(0, 0, 0, 0, ysize, xsize, ysize);
		frames++;

		/* Fixed 640x200 resolution. */
		video_res_x = LCD_XSIZE;
		video_res_y = LCD_YSIZE;

		if (dev->cga.cgamode & 0x02) {
			if (dev->cga.cgamode & 0x10)
				video_bpp = 1;
			else
				video_bpp = 2;
		} else
			video_bpp = 0;

		dev->cga.cgablink++;
	}
    }
}


/* val behaves as follows:
 *     Bit 0: Attributes 01-06, 08-0E are inverse video 
 *     Bit 1: Attributes 01-06, 08-0E are bold 
 *     Bit 2: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
 * 	      are inverse video 
 *     Bit 3: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
 * 	      are bold
 */
static void
recalc_attrs(vid_t *dev)
{
    int n;

    /* Set up colors */
    dev->fgcol = makecol(0x33, 0x33, 0xff);	/* king blue */
    dev->bgcol = makecol(0xe0, 0xe0, 0xe0);	/* light grey */

    /* Initialize the attribute mapping.
     * Start by defaulting everything to grey on blue,
     * and with bold set by bit 3.
     */
    for (n = 0; n < 256; n++) {
	dev->boldcols[n] = (n & 8) != 0;
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->fgcol; 
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->bgcol;
    }

    /* Colors 0x11-0xff are controlled by bits 2 and 3
     * of the passed value. Exclude x0 and x8, which are
     * always grey on blue.
     */
    for (n = 0x11; n <= 0xff; n++) {
	if ((n & 7) == 0) continue;

	if (dev->attrmap & 4) {
		/* Inverse */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->fgcol;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->bgcol;
	} else {
		/* Normal */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->bgcol;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->fgcol;
	}

	if (dev->attrmap & 8)
		dev->boldcols[n] = 1;	/* Bold */
    }

    /* Set up the 01-0E range, controlled by bits 0 and 1 of the 
     * passed value. When blinking is enabled this also affects 81-8e.
     */
    for (n = 0x01; n <= 0x0e; n++) {
	if (n == 7) continue;

	if (dev->attrmap & 1) {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->fgcol;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->bgcol;
		dev->blinkcols[n+128][0] = dev->fgcol;
		dev->blinkcols[n+128][1] = dev->bgcol;
	} else {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->bgcol;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->fgcol;
		dev->blinkcols[n+128][0] = dev->bgcol;
		dev->blinkcols[n+128][1] = dev->fgcol;
	}

	if (dev->attrmap & 2)
		dev->boldcols[n] = 1;
    }

    /* Colors 07 and 0f are always blue on grey.
     * If blinking is enabled so are 87 and 8f.
     */
    for (n = 0x07; n <= 0x0f; n += 8) {
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->bgcol;
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->fgcol;
	dev->blinkcols[n+128][0] = dev->bgcol;
	dev->blinkcols[n+128][1] = dev->fgcol;
    }

    /* When not blinking, colors 81-8f are always blue on grey. */
    for (n = 0x81; n <= 0x8f; n ++) {
	dev->normcols[n][0] = dev->bgcol;
	dev->normcols[n][1] = dev->fgcol;
	dev->boldcols[n] = (n & 0x08) != 0;
    }

    /* Finally do the ones which are solid grey.
     * These differ between the normal and blinking mappings.
     */
    for (n = 0; n <= 0xff; n += 0x11)
	dev->normcols[n][0] = dev->normcols[n][1] = dev->bgcol;

    /* In the blinking range, 00 11 22 .. 77 and 80 91 a2 .. f7 are grey. */
    for (n = 0; n <= 0x77; n += 0x11) {
	dev->blinkcols[n][0] = dev->blinkcols[n][1] = dev->bgcol;
	dev->blinkcols[n+128][0] = dev->blinkcols[n+128][1] = dev->bgcol;
    }
}


static void
vid_out(uint16_t addr, uint8_t val, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    switch (addr) {
	case 0x03d1:	/* emulated CRTC, value */
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		cga_out(addr, val, &dev->cga);
		recalc_timings(dev);
		return;

	case 0x03d8:	/* CGA control register */
INFO("Zenith: vid_out(%04x, %02x)\n", addr, val);
		if (val == 0xff) return;
	      	break;

	case 0x03dd:	/* V6355 Register Bank Index */
		dev->bank_indx = val;
INFO("Zenith: vid_out(%04x, %02x)\n", addr, val);
		return;

	case 0x03de:	/* V6355 Register Bank Data */
		if (dev->bank_indx < 0x6a)
			dev->bank_data[dev->bank_indx] = val;
INFO("Zenith: vid_out(%04x, %02x)\n", addr, val);
		return;
    }

    cga_out(addr, val, &dev->cga);
}


static uint8_t
vid_in(uint16_t addr, void *priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0x03dd:	/* V6355 Register Bank Index */
		ret = dev->bank_indx;
INFO("Zenith: vid_in(%04x) = %02x\n", addr, ret);
		return(ret);

	case 0x03de:	/* V6355 Register Bank Data */
		if (dev->bank_indx < 0x6a)
			ret = dev->bank_data[dev->bank_indx];
INFO("Zenith: vid_in(%04x) = %02x\n", addr, ret);
		return(ret);
    }
	
    ret = cga_in(addr, &dev->cga);

    return(ret);
}


static void
vid_write(uint32_t addr, uint8_t val, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    dev->vram[addr & 0x3fff] = val;

    cycles -= 4;
}


static uint8_t
vid_read(uint32_t addr, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    cycles -= 4;

    return(dev->vram[addr & 0x3fff]);
}


static void
vid_close(void *priv)
{
    vid_t *dev = (vid_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(void *priv)
{
    vid_t *dev = (vid_t *)priv;
	
    recalc_timings(dev);
}


static void *
vid_init(const device_t *info, UNUSED(void *parent))
{
    vid_t *dev;

    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));
    dev->internal = 1;
    dev->st_internal = -1;

    /* Default attribute mapping is 4 */
    dev->attrmap = 0;
    recalc_attrs(dev);

    /* Allocate 16K video RAM. */
    dev->vram = (uint8_t *)mem_alloc(0x4000);
    mem_map_add(&dev->mapping, 0xb8000, 0x8000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);

    dev->cga.vram = dev->vram;
    cga_init(&dev->cga);

    /* Start off in 80x25 text mode. */
    dev->cga.cgastat = 0xf4;

    /* Respond to CGA I/O ports. */
    io_sethandler(0x03d0, 0x000c,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

    timer_add(vid_poll, dev, &dev->cga.vidtime, TIMER_ALWAYS_ENABLED);

    /* Load the CGA Font ROM. */
    video_load_font(CGA_FONT_ROM_PATH, FONT_CGA_THICK);

    video_inform(VID_TYPE_CGA, &timing_zenith);

    return(dev);
}


const device_t zenith_ss_video_device = {
    "Zenith SupersPORT Video",
    0, 0, NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};


void
zenith_vid_set_internal(void *arg, int val)
{
    vid_t *dev = (vid_t *)arg;

    dev->st_internal = val;
}
