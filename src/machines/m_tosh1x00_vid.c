/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Toshiba T1000 plasma display, which
 *		has a fixed resolution of 640x200 pixels.
 *
 * Version:	@(#)m_tosh1x00_vid.c	1.0.13	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *              John Elliott, <jce@seasip.info>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2017,2018 Miran Grca.
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
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../plat.h"
#include "m_tosh1x00.h"


#define T1000_XSIZE 640
#define T1000_YSIZE 200


static video_timings_t timing_t1000 = {VID_ISA, 8,16,32, 8,16,32};


/* Video options set by the motherboard; they will be picked up by the card
 * on the next poll.
 *
 * Bit  1:   Danish
 * Bit  0:   Thin font
 */
static uint8_t	st_options;
static uint8_t	st_enabled = 1;
static int8_t	st_internal = -1;
static uint8_t  langid;


typedef struct {
    mem_map_t	mapping;

    cga_t	cga;		/* The CGA is used for the external 
				 * display; most of its registers are
				 * ignored by the plasma display. */

    int		font;		/* current font, 0-3 */
    int		enabled;	/* hardware enabled, 0 or 1 */
    int		internal;	/* using internal display? */
    uint8_t	attrmap;	/* attribute mapping register */

    tmrval_t	dispontime,
		dispofftime,
 		vsynctime;

    int		linepos, displine;
    int		vc, dispon;

    uint8_t	options;

    uint8_t	*vram;

    /* Mapping of attributes to colors. */
    uint32_t	blue, grey;
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

	dev->dispontime  = (tmrval_t)(_dispontime  * (1 << TIMER_SHIFT));
	dev->dispofftime = (tmrval_t)(_dispofftime * (1 << TIMER_SHIFT));
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
	cursorline = ((dev->cga.crtc[10] & 0x0F) <= sc) &&
		     ((dev->cga.crtc[11] & 0x0F) >= sc);
    }

    for (x = 0; x < 80; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x3fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x3fff];
	       drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	if (dev->options & 1)
		bold = dev->boldcols[attr] ? chr : chr + 256;
	else
		bold = dev->boldcols[attr] ? chr + 256 : chr;
	if (dev->options & 2)
		bold += 512;

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
	       		screen->line[dev->displine][(x << 3) + c].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->blue ^ dev->grey);
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
	cursorline = ((dev->cga.crtc[10] & 0x0F) <= sc) &&
		     ((dev->cga.crtc[11] & 0x0F) >= sc);
    }

    for (x = 0; x < 40; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x3fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x3fff];
	drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	if (dev->options & 1)
		bold = dev->boldcols[attr] ? chr : chr + 256;
	else
		bold = dev->boldcols[attr] ? chr + 256 : chr;
	if (dev->options & 2)
		bold += 512;

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
			  screen->line[dev->displine][(x << 4) + c*2 + 1].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->blue ^ dev->grey);
		}
	} else {
		for (c = 0; c < 8; c++) {
			screen->line[dev->displine][(x << 4) + c*2].val = 
			  screen->line[dev->displine][(x << 4) + c*2+1].val = cols[(fontdat[bold][sc] & (1 << (c ^ 7))) ? 1 : 0];
		}
	}

	++ma;
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
    uint32_t fg = (dev->cga.cgacol & 0x0f) ? dev->blue : dev->grey;
    uint32_t bg = dev->grey;
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x3fff;

    addr = ((dev->displine) & 1) * 0x2000 +
	    (dev->displine >> 1) * 80 + ((ma & ~1) << 1);

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr & 0x3FFF];
	addr++;

	for (c = 0; c < 8; c++) {
		ink = (dat & 0x80) ? fg : bg;
		if (! (dev->cga.cgamode & 8))
			ink = dev->grey;
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
	dat = dev->vram[addr & 0x3FFF];
	addr++;

	for (c = 0; c < 4; c++) {
		pattern = (dat & 0xC0) >> 6;
		if (! (dev->cga.cgamode & 8))
			pattern = 0;

		switch (pattern & 3) {
			default:
			case 0:
				ink0 = ink1 = dev->grey; break;

			case 1:
				if (dev->displine & 1) {
					ink0 = dev->grey; ink1 = dev->grey;
				} else {
					ink0 = dev->blue; ink1 = dev->grey;
				}
				break;

			case 2: if (dev->displine & 1) {
					ink0 = dev->grey; ink1 = dev->blue;
				} else {
					ink0 = dev->blue; ink1 = dev->grey;
				}
				break;

			case 3:
				ink0 = ink1 = dev->blue; break;
		}

		screen->line[dev->displine][x*8+2*c].val = ink0;
		screen->line[dev->displine][x*8+2*c+1].val = ink1;
		dat = dat << 2;
	}
    }
}


static void
vid_poll(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    if (dev->options != st_options || dev->enabled != st_enabled) {
	dev->options = st_options;
	dev->enabled = st_enabled;

	/* Set the font used for the external display */
	dev->cga.fontbase = ((dev->options & 3) * 256);

	/* Enable or disable internal chipset. */
	if (dev->enabled)
		mem_map_enable(&dev->mapping);
	else    
		mem_map_disable(&dev->mapping);
    }

    /* Switch between internal plasma and external CRT display. */
    if (st_internal != -1 && st_internal != dev->internal) {
	dev->internal = st_internal;
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
		if (T1000_XSIZE != xsize || T1000_YSIZE != ysize) {
			xsize = T1000_XSIZE;
			ysize = T1000_YSIZE;
			if (xsize < 64)
				xsize = 656;
			if (ysize < 32)
				ysize = 200;
			set_screen_size(xsize, ysize);
		}

		video_blit_start(0, 0, 0, 0, ysize, xsize, ysize);
		frames++;

		/* Fixed 640x200 resolution. */
		video_res_x = T1000_XSIZE;
		video_res_y = T1000_YSIZE;

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


static void
recalc_attrs(vid_t *dev)
{
    int n;

    /* val behaves as follows:
     *     Bit 0: Attributes 01-06, 08-0E are inverse video 
     *     Bit 1: Attributes 01-06, 08-0E are bold 
     *     Bit 2: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
     * 	      are inverse video 
     *     Bit 3: Attributes 11-16, 18-1F, 21-26, 28-2F ... F1-F6, F8-FF
     * 	      are bold
     */

    /* Set up colors */
    dev->blue = makecol(0x2d, 0x39, 0x5a);
    dev->grey = makecol(0x85, 0xa0, 0xd6);

    /* Initialize the attribute mapping.
     * Start by defaulting everything to grey on blue,
     * and with bold set by bit 3.
     */
    for (n = 0; n < 256; n++) {
	dev->boldcols[n] = (n & 8) != 0;
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->blue; 
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->grey;
    }

    /* Colors 0x11-0xff are controlled by bits 2 and 3
     * of the passed value. Exclude x0 and x8, which are
     * always grey on blue.
     */
    for (n = 0x11; n <= 0xff; n++) {
	if ((n & 7) == 0) continue;

	if (dev->attrmap & 4) {
		/* Inverse */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->blue;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->grey;
	} else {
		/* Normal */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->grey;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->blue;
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
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->blue;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->grey;
		dev->blinkcols[n+128][0] = dev->blue;
		dev->blinkcols[n+128][1] = dev->grey;
	} else {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->grey;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->blue;
		dev->blinkcols[n+128][0] = dev->grey;
		dev->blinkcols[n+128][1] = dev->blue;
	}

	if (dev->attrmap & 2)
		dev->boldcols[n] = 1;
    }

    /* Colors 07 and 0f are always blue on grey.
     * If blinking is enabled so are 87 and 8f.
     */
    for (n = 0x07; n <= 0x0f; n += 8) {
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->grey;
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->blue;
	dev->blinkcols[n+128][0] = dev->grey;
	dev->blinkcols[n+128][1] = dev->blue;
    }

    /* When not blinking, colors 81-8f are always blue on grey. */
    for (n = 0x81; n <= 0x8f; n ++) {
	dev->normcols[n][0] = dev->grey;
	dev->normcols[n][1] = dev->blue;
	dev->boldcols[n] = (n & 0x08) != 0;
    }

    /* Finally do the ones which are solid grey.
     * These differ between the normal and blinking mappings.
     */
    for (n = 0; n <= 0xff; n += 0x11)
	dev->normcols[n][0] = dev->normcols[n][1] = dev->grey;

    /* In the blinking range, 00 11 22 .. 77 and 80 91 a2 .. f7 are grey. */
    for (n = 0; n <= 0x77; n += 0x11) {
	dev->blinkcols[n][0] = dev->blinkcols[n][1] = dev->grey;
	dev->blinkcols[n+128][0] = dev->blinkcols[n+128][1] = dev->grey;
    }
}


static void
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    switch (addr) {
	case 0x03d0:	/* emulated CRTC, register select */
	case 0x03d2:
	case 0x03d4:
	case 0x03d6:
		break;

	case 0x03d1:	/* emulated CRTC, value */
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		if (dev->cga.crtcreg == 0x12) {
			/*
			 * Register 0x12 controls the attribute
			 * mappings for the LCD screen.
			 */
			dev->attrmap = val;	
			recalc_attrs(dev);
			return;
		}	
		cga_out(addr, val, &dev->cga);
		recalc_timings(dev);
		return;

	case 0x03d8:	/* CGA control register */
	      	break;

	case 0x03d9:	/* CGA color register */
	      	break;
    }

    cga_out(addr, val, &dev->cga);
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t ret;

    switch (addr) {
	case 0x03d1:
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		if (dev->cga.crtcreg == 0x12) {
			ret = dev->attrmap & 0x0F;
			if (dev->internal)
				ret |= 0x20; /* LCD / CRT */
			return(ret);
		}
		break;
    }
	
    return(cga_in(addr, &dev->cga));
}


static void
vid_write(uint32_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    dev->vram[addr & 0x3fff] = val;

    cycles -= 4;
}


static uint8_t
vid_read(uint32_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    cycles -= 4;

    return(dev->vram[addr & 0x3fff]);
}


static void
vid_close(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
	
    recalc_timings(dev);
}


static priv_t
vid_init(const device_t *info, UNUSED(void *parent))
{
    vid_t *dev;

    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));

    cga_init(&dev->cga);

    dev->internal = 1;
    dev->options = 0x01;
    dev->enabled = 1;

    /* Default attribute mapping is 4 */
    dev->attrmap = 4;
    recalc_attrs(dev);

    /* Start off in 80x25 text mode. */
    dev->cga.cgastat = 0xf4;
    dev->cga.vram = dev->vram;

//    langid = device_get_config_int("display_language") ? 2 : 0;
   langid = 0;

    /* Allocate 16K video RAM. */
    dev->vram = (uint8_t *)mem_alloc(0x4000);
    mem_map_add(&dev->mapping, 0xb8000, 0x8000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);

    /* Respond to CGA I/O ports. */
    io_sethandler(0x03d0, 0x000c,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

    timer_add(vid_poll, dev, &dev->cga.vidtime, TIMER_ALWAYS_ENABLED);

    video_inform(VID_TYPE_CGA, &timing_t1000);

    return((priv_t)dev);
}


const device_t t1000_video_device = {
    "Toshiba T1000 Video",
    0, 0,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};

const device_t t1200_video_device = {
    "Toshiba T1200 Video",
    0, 1,
    NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};


void
t1000_video_options_set(uint8_t options)
{
    st_options = options & 1;
    st_options |= langid;
}


void
t1000_video_enable(uint8_t enabled)
{
    st_enabled = enabled;
}


void
t1000_display_set(uint8_t internal)
{
    st_internal = (int8_t)internal;
}


uint8_t
t1000_display_get(void)
{
    return((uint8_t)st_internal);
}
