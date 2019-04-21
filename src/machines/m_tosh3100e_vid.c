/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Toshiba 3100e plasma display.
 *		This display has a fixed 640x400 resolution.
 *
 *		T3100e CRTC regs (from the ROM):
 *
 *		Selecting a character height of 3 seems to be sufficient to
 *		convert the 640x200 graphics mode to 640x400 (and, by
 *		analogy, 320x200 to 320x400).
 * 
 *		Horiz-----> Vert------>  I ch
 *		38 28 2D 0A 1F 06 19 1C 02 07 06 07   CO40
 *		71 50 5A 0A 1F 06 19 1C 02 07 06 07   CO80
 *		38 28 2D 0A 7F 06 64 70 02 01 06 07   Graphics
 *		61 50 52 0F 19 06 19 19 02 0D 0B 0C   MONO
 *		2D 28 22 0A 67 00 64 67 02 03 06 07   640x400
 *
 * Version:	@(#)m_t3100e_vid.c	1.0.10	2019/04/08
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../timer.h"
#include "../device.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../plat.h"
#include "m_tosh3100e.h"


#define FONT_ROM_PATH	L"machines/toshiba/t3100e/t3100e_font.bin"
#define T3100E_XSIZE	640
#define T3100E_YSIZE	400


/* Video options set by the motherboard; they will be picked up by the card
 * on the next poll.
 *
 * Bit  3:   Disable built-in video (for add-on card)
 * Bit  2:   Thin font
 * Bits 0,1: Font set (not currently implemented) 
 */
static uint8_t st_video_options;
static int8_t st_display_internal = -1;


typedef struct {
    mem_map_t	mapping;

    cga_t	cga;			/* The CGA is used for the external 
					 * display; most of its registers are
					 * ignored by the plasma display. */

    int		font;			/* current font, 0-3 */
    int		enabled;		/* hardware enabled, 0 or 1 */
    int		internal;		/* using internal display? */
    uint8_t	attrmap;		/* attribute mapping register */

    int64_t	dispontime,
		dispofftime,
		vsynctime;

    int		linepos, displine;
    int		vc;
    int		dispon;
    uint8_t	video_options;

    uint8_t	*vram;

    /* Mapping of attributes to colors. */
    uint32_t	amber,
		black;
    uint8_t	boldcols[256];		/* which attrs use the bold font */
    uint32_t	blinkcols[256][2];
    uint32_t	normcols[256][2];

    uint8_t	fontdat[2048][8];
    uint8_t	fontdatm[2048][16];
} vid_t;


static video_timings_t t3100e_timing = { VID_ISA, 8,16,32, 8,16,32 };


static void
recalc_timings(vid_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    if (! dev->internal) {
	cga_recalctimings(&dev->cga);
	return;
    }

    disptime = 651;
    _dispontime = 640;
    _dispofftime = disptime - _dispontime;

    dev->dispontime  = (int)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
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

    /* Set up colors. */
    dev->amber = makecol(0xf7, 0x7c, 0x34);
    dev->black = makecol(0x17, 0x0c, 0x00);

    /* Initialize the attribute mapping. Start by defaulting
     * everything to black on amber, and with bold set by bit 3.
     */
    for (n = 0; n < 256; n++) {
	dev->boldcols[n] = (n & 8) != 0;
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber; 
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
    }

    /* Colors 0x11-0xFF are controlled by bits 2 and 3 of the 
     * passed value. Exclude x0 and x8, which are always black
     * on amber.
     */
    for (n = 0x11; n <= 0xff; n++) {
	if ((n & 7) == 0) continue;

	if (dev->attrmap & 4) {		/* Inverse */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
	} else {			/* Normal */
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
	}
	if (dev->attrmap & 8)
		dev->boldcols[n] = 1;	/* Bold */
    }

    /* Set up the 01-0E range, controlled by bits 0 and 1 of the 
     * passed value. When blinking is enabled this also affects
     * 81-8E.
     */
    for (n = 0x01; n <= 0x0e; n++) {
	if (n == 7) continue;

	if (dev->attrmap & 1) {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->amber;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->black;
		dev->blinkcols[n+128][0] = dev->amber;
		dev->blinkcols[n+128][1] = dev->black;
	} else {
		dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
		dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
		dev->blinkcols[n+128][0] = dev->black;
		dev->blinkcols[n+128][1] = dev->amber;
	}
	if (dev->attrmap & 2)
		dev->boldcols[n] = 1;
    }

    /* Colors 07 and 0F are always amber on black. If
     * blinking is enabled so are 87 and 8F.
     */
    for (n = 0x07; n <= 0x0f; n += 8) {
	dev->blinkcols[n][0] = dev->normcols[n][0] = dev->black;
	dev->blinkcols[n][1] = dev->normcols[n][1] = dev->amber;
	dev->blinkcols[n+128][0] = dev->black;
	dev->blinkcols[n+128][1] = dev->amber;
    }

    /* When not blinking, colors 81-8F are always amber on black. */
    for (n = 0x81; n <= 0x8f; n ++) {
	dev->normcols[n][0] = dev->black;
	dev->normcols[n][1] = dev->amber;
	dev->boldcols[n] = (n & 0x08) != 0;
    }

    /* Finally do the ones which are solid black. These
     * differ between the normal and blinking mappings.
     */
    for (n = 0; n <= 0xff; n += 0x11)
	dev->normcols[n][0] = dev->normcols[n][1] = dev->black;

    /* In the blinking range, 00 11 22 .. 77 and 80 91 A2 .. F7 are black */
    for (n = 0; n <= 0x77; n += 0x11) {
	dev->blinkcols[n][0] = dev->blinkcols[n][1] = dev->black;
	dev->blinkcols[n+128][0] = dev->blinkcols[n+128][1] = dev->black;
    }
}


static void
vid_out(uint16_t port, uint8_t val, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    switch (port) {
	/* Emulated CRTC, register select */
	case 0x3d0: case 0x3d2: case 0x3d4: case 0x3d6:
		cga_out(port, val, &dev->cga);
		break;

	/* Emulated CRTC, value */
	case 0x3d1: case 0x3d3: case 0x3d5: case 0x3d7:
		/* Register 0x12 controls the attribute mappings for the
		 * plasma screen. */
		if (dev->cga.crtcreg == 0x12) {
			dev->attrmap = val;	
			recalc_attrs(dev);
			return;
		}	
		cga_out(port, val, &dev->cga);

		recalc_timings(dev);
		return;

	/* CGA control register */
	case 0x3d8:
		cga_out(port, val, &dev->cga);
	      	return;

	/* CGA color register */
	case 0x3d9:
		cga_out(port, val, &dev->cga);
	      	return;
    }
}


static uint8_t
vid_in(uint16_t port, void *priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t ret;

    switch (port) {
	case 0x3d1: case 0x3d3: case 0x3d5: case 0x3d7:
		if (dev->cga.crtcreg == 0x12) {
			ret = dev->attrmap & 0x0f;
			if (dev->internal)
				ret |= 0x30; /* Plasma / CRT */
			return ret;
		}
		break;
    }

    return cga_in(port, &dev->cga);
}


static void
vid_write(uint32_t addr, uint8_t val, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    dev->vram[addr & 0x7fff] = val;

    cycles -= 4;
}
	

static uint8_t
vid_read(uint32_t addr, void *priv)
{
    vid_t *dev = (vid_t *)priv;

    cycles -= 4;

    return dev->vram[addr & 0x7fff];
}


/* Draw a row of text in 80-column mode. */
static void
text_row80(vid_t *dev)
{
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x7fff;
    uint16_t ca = (dev->cga.crtc[15] | (dev->cga.crtc[14] << 8)) & 0x7fff;
    uint32_t cols[2];
    int x, c;
    uint8_t chr, attr;
    int drawcursor;
    int cursorline;
    int bold;
    int blink;
    uint16_t addr;
    uint8_t sc;

    sc = (dev->displine) & 15;
    addr = ((ma & ~1) + (dev->displine >> 4) * 80) * 2;
    ma += (dev->displine >> 4) * 80;

    if ((dev->cga.crtc[10] & 0x60) == 0x20) {
	cursorline = 0;
    } else {
	cursorline = ((dev->cga.crtc[10] & 0x0f)*2 <= sc) &&
		     ((dev->cga.crtc[11] & 0x0f)*2 >= sc);
    }

    for (x = 0; x < 80; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x7fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x7fff];
	drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	if (dev->video_options & 4)
		bold = dev->boldcols[attr] ? chr + 256 : chr;
	else
		bold = dev->boldcols[attr] ? chr : chr + 256;
	bold += 512 * (dev->video_options & 3);

	if (dev->cga.cgamode & 0x20) {	/* Blink */
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
			screen->line[dev->displine][(x << 3) + c].val = cols[(dev->fontdatm[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->amber ^ dev->black);
		}
	} else {
		for (c = 0; c < 8; c++)
			screen->line[dev->displine][(x << 3) + c].val = cols[(dev->fontdatm[bold][sc] & (1 << (c ^ 7))) ? 1 : 0];
	}
	++ma;
    }
}


/* Draw a row of text in 40-column mode. */
static void
text_row40(vid_t *dev)
{
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x7fff;
    uint16_t ca = (dev->cga.crtc[15] | (dev->cga.crtc[14] << 8)) & 0x7fff;
    uint32_t cols[2];
    int x, c;
    uint8_t chr, attr;
    int drawcursor;
    int cursorline;
    int bold;
    int blink;
    uint16_t addr;
    uint8_t sc;

    sc = (dev->displine) & 15;
    addr = ((ma & ~1) + (dev->displine >> 4) * 40) * 2;
    ma += (dev->displine >> 4) * 40;

    if ((dev->cga.crtc[10] & 0x60) == 0x20) {
	cursorline = 0;
    } else {
	cursorline = ((dev->cga.crtc[10] & 0x0f)*2 <= sc) &&
		     ((dev->cga.crtc[11] & 0x0f)*2 >= sc);
    }

    for (x = 0; x < 40; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x7fff];
	attr = dev->vram[(addr + 2 * x + 1) & 0x7fff];
	drawcursor = ((ma == ca) && cursorline &&
		(dev->cga.cgamode & 8) && (dev->cga.cgablink & 16));

	blink = ((dev->cga.cgablink & 16) && (dev->cga.cgamode & 0x20) &&
		(attr & 0x80) && !drawcursor);

	if (dev->video_options & 4)
		bold = dev->boldcols[attr] ? chr + 256 : chr;
	else
		bold = dev->boldcols[attr] ? chr : chr + 256;
	bold += 512 * (dev->video_options & 3);

	if (dev->cga.cgamode & 0x20) {	/* Blink */
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
			screen->line[dev->displine][(x << 4) + c*2].val = screen->line[dev->displine][(x << 4) + c*2 + 1].val = cols[(dev->fontdatm[bold][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ (dev->amber ^ dev->black);
		}
	} else {
		for (c = 0; c < 8; c++) {
			screen->line[dev->displine][(x << 4) + c*2].val = screen->line[dev->displine][(x << 4) + c*2+1].val = cols[(dev->fontdatm[bold][sc] & (1 << (c ^ 7))) ? 1 : 0];
		}
	}

	ma++;
    }
}


/* Draw a line in CGA 640x200 or T3100e 640x400 mode. */
static void
cga_line6(vid_t *dev)
{
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x7fff;
    uint32_t fg = (dev->cga.cgacol & 0x0f) ? dev->amber : dev->black;
    uint32_t bg = dev->black;
    uint8_t dat;
    uint32_t ink = 0;
    uint16_t addr;
    int x, c;

    if (dev->cga.crtc[9] == 3) {	/* 640*400 */
	addr = ((dev->displine) & 1) * 0x2000 +
	       ((dev->displine >> 1) & 1) * 0x4000 +
	       (dev->displine >> 2) * 80 +
	       ((ma & ~1) << 1);
    } else {
	addr = ((dev->displine >> 1) & 1) * 0x2000 +
	       (dev->displine >> 2) * 80 +
	       ((ma & ~1) << 1);
    }

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr & 0x7fff];
	addr++;

	for (c = 0; c < 8; c++) {
		ink = (dat & 0x80) ? fg : bg;
		if (!(dev->cga.cgamode & 8))
			ink = dev->black;
		screen->line[dev->displine][x*8+c].val = ink;
		dat = dat << 1;
	}
    }
}


/* Draw a line in CGA 320x200 mode. Here the CGA colors are
 * converted to dither patterns: color 1 to 25% grey, color
 * 2 to 50% grey.
 */
static void
cga_line4(vid_t *dev)
{
    uint16_t ma = (dev->cga.crtc[13] | (dev->cga.crtc[12] << 8)) & 0x7fff;
    uint8_t dat, pattern;
    uint32_t ink0 = 0, ink1 = 0;
    uint16_t addr;
    int x, c;

    if (dev->cga.crtc[9] == 3) {	/* 320*400 undocumented */
	addr = ((dev->displine) & 1) * 0x2000 +
	       ((dev->displine >> 1) & 1) * 0x4000 +
	       (dev->displine >> 2) * 80 +
	       ((ma & ~1) << 1);
    } else {		/* 320*200 */
	addr = ((dev->displine >> 1) & 1) * 0x2000 +
	       (dev->displine >> 2) * 80 +
	       ((ma & ~1) << 1);
    }

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr & 0x7fff];
	addr++;

	for (c = 0; c < 4; c++) {
		pattern = (dat & 0xc0) >> 6;
		if (!(dev->cga.cgamode & 8)) pattern = 0;

		switch (pattern & 3) {
			case 0:
				ink0 = ink1 = dev->black; break;

			case 1:
				if (dev->displine & 1) {
					ink0 = dev->black; ink1 = dev->black;
				} else {
					ink0 = dev->amber; ink1 = dev->black;
				}
				break;

			case 2:
				if (dev->displine & 1) {
					ink0 = dev->black; ink1 = dev->amber;
				} else {
					ink0 = dev->amber; ink1 = dev->black;
				}
				break;

			case 3:
				ink0 = ink1 = dev->amber; break;

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

    if (dev->video_options != st_video_options) {
	dev->video_options = st_video_options;

	if (dev->video_options & 8) /* Disable internal CGA */
		mem_map_disable(&dev->mapping);
	else
		mem_map_enable(&dev->mapping);

	/* Set the font used for the external display */
	dev->cga.fontbase = (512 * (dev->video_options & 3))
			     + ((dev->video_options & 4) ? 256 : 0);

    }

    /* Switch between internal plasma and external CRT display. */
    if (st_display_internal != -1 && st_display_internal != dev->internal) {
	dev->internal = st_display_internal;
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
		if (dev->displine == 0)
			video_blit_wait_buffer();

		/* Graphics */
		if (dev->cga.cgamode & 0x02)	{
			if (dev->cga.cgamode & 0x10)
				cga_line6(dev);
			else
				cga_line4(dev);
		} else	if (dev->cga.cgamode & 0x01) {	/* High-res text */
			text_row80(dev); 
		} else {
			text_row40(dev); 
		}
	}
	dev->displine++;

	/* Hardcode a fixed refresh rate and VSYNC timing */
	if (dev->displine == 400) {	/* Start of VSYNC */
		dev->cga.cgastat |= 8;
		dev->dispon = 0;
	}

	if (dev->displine == 416) {	/* End of VSYNC */
		dev->displine = 0;
		dev->cga.cgastat &= ~8;
		dev->dispon = 1;
	}
    } else {
	if (dev->dispon)
		dev->cga.cgastat &= ~1;
	dev->cga.vidtime += dev->dispontime;
	dev->linepos = 0;

	if (dev->displine == 400) {
		/* Hardcode 640x400 window size */
		if (T3100E_XSIZE != xsize || T3100E_YSIZE != ysize) {
			xsize = T3100E_XSIZE;
			ysize = T3100E_YSIZE;
			if (xsize < 64) xsize = 656;
			if (ysize < 32) ysize = 200;
			set_screen_size(xsize, ysize);
		}

		video_blit_start(0, 0, 0, 0, ysize, xsize, ysize);
		frames++;

		/* Fixed 640x400 resolution */
		video_res_x = T3100E_XSIZE;
		video_res_y = T3100E_YSIZE;

		if (dev->cga.cgamode & 0x02)	{
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


static int
load_font(vid_t *dev, const wchar_t *s)
{
    FILE *fp;
    int c, d;

    fp = plat_fopen(rom_path(s), L"rb");
    if (fp == NULL) {
	ERRLOG("T3100e: cannot load font '%ls'\n", s);
	return(0);
    }

    /* Fonts for 4 languages... */
    for (d = 0; d < 2048; d += 512) {
	for (c = d; c < d+256; c++)
		(void)fread(&dev->fontdatm[c][8], 1, 8, fp);
	for (c = d+256; c < d+512; c++)
		(void)fread(&dev->fontdatm[c][8], 1, 8, fp);
	for (c = d; c < d+256; c++)
		(void)fread(&dev->fontdatm[c][0], 1, 8, fp);
	for (c = d+256; c < d+512; c++)
		(void)fread(&dev->fontdatm[c][0], 1, 8, fp);

	/* Skip blank section. */
	(void)fseek(fp, 4096, SEEK_CUR);

	for (c = d; c < d+256; c++)
		(void)fread(&dev->fontdat[c][0], 1, 8, fp);
	for (c = d+256; c < d+512; c++)
		(void)fread(&dev->fontdat[c][0], 1, 8, fp);
    }

    (void)fclose(fp);

    return(1);
}


static void *
vid_init(const device_t *info, UNUSED(void *parent))
{
    vid_t *dev;

    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));

    if (! load_font(dev, FONT_ROM_PATH)) {
	free(dev);
	return(NULL);
    }

    cga_init(&dev->cga);

    dev->internal = 1;

    /* 32K video RAM */
    dev->vram = (uint8_t *)mem_alloc(0x8000);

    timer_add(vid_poll, &dev->cga.vidtime, TIMER_ALWAYS_ENABLED, dev);

    /* Occupy memory between 0xB8000 and 0xBFFFF */
    mem_map_add(&dev->mapping, 0xb8000, 0x8000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);

    /* Respond to CGA I/O ports */
    io_sethandler(0x03d0, 12, vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

    /* Default attribute mapping is 4 */
    dev->attrmap = 4;
    recalc_attrs(dev);

    /* Start off in 80x25 text mode */
    dev->cga.cgastat = 0xf4;
    dev->cga.vram = dev->vram;
    dev->enabled = 1;
    dev->video_options = 0xff;

    video_inform(VID_TYPE_CGA, &t3100e_timing);

    return(dev);
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


const device_t t3100e_vid_device = {
    "Toshiba T3100e Video",
    0, 0, NULL,
    vid_init, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};


void
t3100e_video_options_set(uint8_t options)
{
    st_video_options = options;
}


void
t3100e_display_set(uint8_t internal)
{
    st_display_internal = internal;
}


uint8_t
t3100e_display_get(void)
{
    return st_display_internal;
}
