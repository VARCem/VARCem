/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Wyse-700 emulation.
 *
 *		The Wyse 700 is an unusual video card. Though it has an MC6845
 *		CRTC, this is not exposed directly to the host PC. Instead, the
 *		CRTC is controlled by an MC68705P3 microcontroller. 
 *
 *		Rather than emulate the real CRTC, I'm writing this as more
 *		or less a fixed-frequency card with a 1280x800 display, and
 *		scaling its selection of modes to that window.
 *
 *		By default, the card responds to both the CGA and MDA I/O and
 *		memory ranges. Either range can be disabled by means of
 *		jumpers; this allows the Wy700 to coexist with a CGA or MDA. 
 * 
 *		dev->wy700_mode indicates which of the supported video
 *		modes is in use:
 *
 *		  0x00:   40x 25   text     (CGA compatible) [32x32 char cell]
 *		  0x02:   80x 25   text     (CGA/MDA compatible)  [16x32 cell]
 *		  0x04:  320x200x4 graphics (CGA compatible)
 *		  0x06:  640x200x2 graphics (CGA compatible)
 *		  0x80:  640x400x2 graphics
 *		  0x90:  320x400x4 graphics
 *		  0xA0: 1280x400x2 graphics
 *		  0xB0:  640x400x4 graphics
 *		  0xC0: 1280x800x2 graphics (interleaved)
 *		  0xD0:  640x800x4 graphics (interleaved)
 *
 *		In hi-res graphics modes, bit 3 of the mode byte is the
 *		enable flag.
 *
 *		What works (or appears to):
 *		  MDA/CGA 80x25 text mode
 *		  CGA 40x25 text mode        
 *		  CGA 640x200 graphics mode 
 *		  CGA 320x200 graphics mode 
 *
 *		  Hi-res graphics modes
 *		  Font selection
 *		  Display enable / disable
 *		    -- via Wy700 mode register      (in hi-res modes)
 *		    -- via Wy700 command register   (in text & CGA modes)
 *		    -- via CGA/MDA control register (in text & CGA modes)
 *
 *		What doesn't work, is untested or not well understood:
 *		  - Cursor detach (commands 4 and 5)
 *
 * Version:	@(#)vid_wy700.c	1.0.12	2019/05/05
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
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"


#define WY700_XSIZE	1280
#define WY700_YSIZE	800

#define FONT_ROM_PATH	L"video/wyse/wyse700/wy700.rom"


/* The microcontroller sets up the real CRTC with one of five fixed mode
 * definitions. As written, this is a fairly simplistic emulation that 
 * doesn't attempt to closely follow the actual working of the CRTC; but I've 
 * included the definitions here for information.
 */
static const uint8_t mode_1280x800[] = {
    0x31,	/* Horizontal total */
    0x28,	/* Horizontal displayed */
    0x29,	/* Horizontal sync position */
    0x06,	/* Horizontal sync width */
    0x1b,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x19,	/* Vertical displayed */
    0x1a,	/* Vsync position */
    0x03,	/* Interlace and skew */
    0x0f	/* Maximum raster address */
};

static const uint8_t mode_1280x400[] = {
    0x31,	/* Horizontal total */
    0x28,	/* Horizontal displayed */
    0x29,	/* Horizontal sync position */
    0x06,	/* Horizontal sync width */
    0x1b,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x19,	/* Vertical displayed */
    0x1a,	/* Vsync position */
    0x01,	/* Interlace and skew */
    0x0f	/* Maximum raster address */
};

static const uint8_t mode_640x400[] = {
    0x18,	/* Horizontal total */
    0x14,	/* Horizontal displayed */
    0x14,	/* Horizontal sync position */
    0x03,	/* Horizontal sync width */
    0x1b,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x19,	/* Vertical displayed */
    0x1a,	/* Vsync position */
    0x01,	/* Interlace and skew */
    0x0f	/* Maximum raster address */
};

static const uint8_t mode_640x200[] = {
    0x18,	/* Horizontal total */
    0x14,	/* Horizontal displayed */
    0x14,	/* Horizontal sync position */
    0xff,	/* Horizontal sync width */
    0x37,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x32,	/* Vertical displayed */
    0x34,	/* Vsync position */
    0x03,	/* Interlace and skew */
    0x07	/* Maximum raster address */
};

static const uint8_t mode_80x24[] = {
    0x31,	/* Horizontal total */
    0x28,	/* Horizontal displayed */
    0x2A,	/* Horizontal sync position */
    0xff,	/* Horizontal sync width */
    0x1b,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x19,	/* Vertical displayed */
    0x1a,	/* Vsync position */
    0x01,	/* Interlace and skew */
    0x0f	/* Maximum raster address */
};

static const uint8_t mode_40x24[] = {
    0x18,	/* Horizontal total */
    0x14,	/* Horizontal displayed */
    0x15,	/* Horizontal sync position */
    0xff,	/* Horizontal sync width */
    0x1b,	/* Vertical total */
    0x00,	/* Vertical total adjust */
    0x19,	/* Vertical displayed */
    0x1a,	/* Vsync position */
    0x01,	/* Interlace and skew */
    0x0f	/* Maximum raster address */
};


typedef struct {
    const char	*name;

    mem_map_t	mapping;

    /* The microcontroller works by watching four ports: 
     * 0x3D8 / 0x3B8 (mode control register) 
     * 0x3DD	 (top scanline address)
     * 0x3DF	 (Wy700 control register)
     * CRTC reg 14   (cursor location high)
     * 
     * It will do nothing until one of these registers is touched. When
     * one is, it then reconfigures the internal 6845 based on what it
     * sees.
     */ 
    uint8_t	last_03D8;	/* Copies of values written to the listed */
    uint8_t	last_03DD;	/* I/O ports */
    uint8_t	last_03DF;
    uint8_t	last_crtc_0E;

    uint8_t	cga_crtc[32];	/* The 'CRTC' as the host PC sees it */
    uint8_t	real_crtc[32];	/* The internal CRTC as the microcontroller */
				/* sees it */
    int		cga_crtcreg;	/* Current CRTC register */
    uint16_t	wy700_base;	/* Framebuffer base address (native modes) */
    uint8_t	wy700_control;	/* Native control / command register */
    uint8_t	wy700_mode;	/* Current mode (see list at top of file) */
    uint8_t	cga_ctrl;	/* Emulated MDA/CGA control register */
    uint8_t	cga_color;	/* Emulated CGA color register (ignored) */

    uint8_t	mda_stat;	/* MDA status (IN 0x3BA) */
    uint8_t	cga_stat;	/* CGA status (IN 0x3DA) */

    int		font;		/* Current font, 0 or 1 */
    int		enabled;	/* Display enabled, 0 or 1 */
    int		detach;		/* Detach cursor, 0 or 1 */

    int64_t	dispontime, dispofftime;
    int64_t	vidtime;
	
    int		linepos, displine;
    int		vc;
    int		dispon, blink;
    int64_t	vsynctime;

    /* Mapping of attributes to colors, in CGA emulation... */
    uint8_t	cgacols[256][2][2];

    /* ... and MDA emulation. */
    uint8_t	mdacols[256][2][2];

    /* Font ROM: Two fonts, each containing 256 characters, 16x16 pixels */
    uint8_t	fontdat[512][32];

    uint8_t	*vram;
} wy700_t;


static void
recalc_timings(wy700_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    disptime = dev->real_crtc[0] + 1;
    _dispontime = dev->real_crtc[1];
    _dispofftime = disptime - _dispontime;
    _dispontime  *= MDACONST;
    _dispofftime *= MDACONST;

    dev->dispontime  = (int64_t)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (int64_t)(_dispofftime * (1 << TIMER_SHIFT));
}


/* Check if any of the four key registers has changed. If so, check for a 
 * mode change or cursor size change */
static void
check_changes(wy700_t *dev)
{
    uint8_t curstart, curend;

    if (dev->last_03D8    == dev->cga_ctrl &&
	dev->last_03DD    == (dev->wy700_base & 0xFF) &&
	dev->last_03DF    == dev->wy700_control &&
	dev->last_crtc_0E == dev->cga_crtc[0x0E]) {
		return;	/* Nothing changed */
    }

    /* Check for control register changes */
    if (dev->last_03DF != dev->wy700_control) {
	dev->last_03DF = dev->wy700_control;

	/* Values 1-7 are commands. */
	switch (dev->wy700_control) {
		case 1:	/* Reset */
			dev->font    = 0;
			dev->enabled = 1;
			dev->detach  = 0;
			break;
			
		case 2:	/* Font 1 */
			dev->font    = 0;
			break;
			
		case 3:	/* Font 2 */
			dev->font    = 1;
			break;
			
/* Even with the microprogram from an original card, I can't really work out 
 * what commands 4 and 5 (which I've called 'cursor detach' / 'cursor attach')
 * do. Command 4 sets a flag in microcontroller RAM, and command 5 clears 
 * it. When the flag is set, the real cursor doesn't track the cursor in the
 * emulated CRTC, and its blink rate increases. Possibly it's a self-test
 * function of some kind.
 *
 * The card documentation doesn't cover these commands.
 */

		case 4:	/* Detach cursor */
			dev->detach  = 1;
			break;
			
		case 5:	/* Attach cursor */
			dev->detach  = 0;
			break;
			
		case 6:	/* Disable display */
			dev->enabled = 0;
			break;
			
		case 7:	/* Enable display */
			dev->enabled = 1;
			break;
	}

	/* A control write with the top bit set selects graphics mode */
	if (dev->wy700_control & 0x80)	{
		/* Select hi-res graphics mode; map framebuffer at A0000 */
		mem_map_set_addr(&dev->mapping, 0xa0000, 0x20000);
		dev->wy700_mode = dev->wy700_control;

		/* Select appropriate preset timings */
		if (dev->wy700_mode & 0x40) {
			memcpy(dev->real_crtc, mode_1280x800, 
				sizeof(mode_1280x800));
		} else if (dev->wy700_mode & 0x20) {
			memcpy(dev->real_crtc, mode_1280x400, 
				sizeof(mode_1280x400));
		} else {
			memcpy(dev->real_crtc, mode_640x400, 
				sizeof(mode_640x400));
		}
	}
    }

    /* An attempt to program the CGA / MDA selects low-res mode */
    else if (dev->last_03D8 != dev->cga_ctrl) {
	dev->last_03D8 = dev->cga_ctrl;
	/* Set lo-res text or graphics mode. 
	 * (Strictly speaking, when not in hi-res mode the card 
	 *  should be mapped at B0000-B3FFF and B8000-BBFFF, leaving 
	 * a 16k hole between the two ranges) */
	mem_map_set_addr(&dev->mapping, 0xb0000, 0x0C000);
	if (dev->cga_ctrl & 2)	/* Graphics mode */ {
		dev->wy700_mode = (dev->cga_ctrl & 0x10) ? 6 : 4;
		memcpy(dev->real_crtc, mode_640x200, 
			sizeof(mode_640x200));
	} else if (dev->cga_ctrl & 1) /* Text mode 80x24 */ {
		dev->wy700_mode = 2;
		memcpy(dev->real_crtc, mode_80x24, sizeof(mode_80x24));
	} else	/* Text mode 40x24 */ {
		dev->wy700_mode = 0;
		memcpy(dev->real_crtc, mode_40x24, sizeof(mode_40x24));
	}
    }

    /* Convert the cursor sizes from the ones used by the CGA or MDA to native */
    if (dev->cga_crtc[9] == 13)	/* MDA scaling */ {
	curstart = dev->cga_crtc[10] & 0x1F;
	dev->real_crtc[10] = ((curstart + 5) >> 3) + curstart;
	if (dev->real_crtc[10] > 31) dev->real_crtc[10] = 31;
	/* And bring 'cursor disabled' flag across */
	if ((dev->cga_crtc[10] & 0x60) == 0x20) {
		dev->real_crtc[10] |= 0x20;
	}
	curend = dev->cga_crtc[11] & 0x1F;
	dev->real_crtc[11] = ((curend + 5) >> 3) + curend;
	if (dev->real_crtc[11] > 31) dev->real_crtc[11] = 31;
    } else	/* CGA scaling */ {
	curstart = dev->cga_crtc[10] & 0x1F;
	dev->real_crtc[10] = curstart << 1;
	if (dev->real_crtc[10] > 31) dev->real_crtc[10] = 31;
	/* And bring 'cursor disabled' flag across */
	if ((dev->cga_crtc[10] & 0x60) == 0x20) {
		dev->real_crtc[10] |= 0x20;
	}
	curend = dev->cga_crtc[11] & 0x1F;
	dev->real_crtc[11] = curend << 1;
	if (dev->real_crtc[11] > 31) dev->real_crtc[11] = 31;
    }
}


static void
wy700_out(uint16_t port, uint8_t val, void *priv)
{
    wy700_t *dev = (wy700_t *)priv;

    switch (port) {
	/* These three registers are only mapped in the 3Dx range, 
	 * not the 3Bx range. */
	case 0x3dd:	/* Base address (low) */
		dev->wy700_base &= 0xFF00;
		dev->wy700_base |= val;
		check_changes(dev);
		break;

	case 0x3de:	/* Base address (high) */
		dev->wy700_base &= 0xFF;
		dev->wy700_base |= ((uint16_t)val) << 8;
		check_changes(dev);
		break;

	case 0x3df:	/* Command / control register */
		dev->wy700_control = val;
		check_changes(dev);
		break;
				
	/* Emulated CRTC, register select */
	case 0x3b0: case 0x3b2: case 0x3b4: case 0x3b6:
	case 0x3d0: case 0x3d2: case 0x3d4: case 0x3d6:
		dev->cga_crtcreg = val & 31;
		break;

	/* Emulated CRTC, value */
	case 0x3b1: case 0x3b3: case 0x3b5: case 0x3b7:
	case 0x3d1: case 0x3d3: case 0x3d5: case 0x3d7:
		dev->cga_crtc[dev->cga_crtcreg] = val;
		check_changes(dev);
		recalc_timings(dev);
		return;

	/* Emulated MDA / CGA control register */
	case 0x3b8:
	case 0x3d8:
	       	dev->cga_ctrl = val;
		check_changes(dev);
	      	return;

	/* Emulated CGA color register */
	case 0x3d9:
	       	dev->cga_color = val;
	      	return;
    }
}


static uint8_t
wy700_in(uint16_t port, void *priv)
{
    wy700_t *dev = (wy700_t *)priv;

    switch (port) {
	case 0x3b0: case 0x3b2: case 0x3b4: case 0x3b6:
	case 0x3d0: case 0x3d2: case 0x3d4: case 0x3d6:
		return dev->cga_crtcreg;

	case 0x3b1: case 0x3b3: case 0x3b5: case 0x3b7:
	case 0x3d1: case 0x3d3: case 0x3d5: case 0x3d7:
		return dev->cga_crtc[dev->cga_crtcreg];

	case 0x3b8: case 0x3d8:
		return dev->cga_ctrl;

	case 0x3d9:
		return dev->cga_color;

	case 0x3ba: 
		return dev->mda_stat;

	case 0x3da:
		return dev->cga_stat;
    }

    return 0xff;
}


static void
wy700_write(uint32_t addr, uint8_t val, void *priv)
{
    wy700_t *dev = (wy700_t *)priv;

    if (dev->wy700_mode & 0x80) {	/* High-res mode. */
	addr &= 0xffff;

	/* In 800-line modes, bit 1 of the control register sets
	 * the high bit of the write address.
	 */
	if ((dev->wy700_mode & 0x42) == 0x42)	{
		addr |= 0x10000;	
	}	
	dev->vram[addr] = val;
    } else {
	dev->vram[addr & 0x3fff] = val;
    }
}


static uint8_t
wy700_read(uint32_t addr, void *priv)
{
    wy700_t *dev = (wy700_t *)priv;

    if (dev->wy700_mode & 0x80) {	/* High-res mode. */
	addr &= 0xffff;

	/* In 800-line modes, bit 0 of the control register sets
	 * the high bit of the read address.
	 */
	if ((dev->wy700_mode & 0x41) == 0x41)	{
		addr |= 0x10000;	
	}	
	return dev->vram[addr];
    } else {
	return dev->vram[addr & 0x3fff];
    }
}


/* Draw a single line of the screen in either text mode */
static void
text_line(wy700_t *dev)
{
    uint16_t ma = (dev->cga_crtc[13] | (dev->cga_crtc[12] << 8)) & 0x3fff;
    uint16_t ca = (dev->cga_crtc[15] | (dev->cga_crtc[14] << 8)) & 0x3fff;
    int x;
    int w  = (dev->wy700_mode == 0) ? 40 : 80;
    int cw = (dev->wy700_mode == 0) ? 32 : 16;
    uint8_t chr, attr;
    uint8_t bitmap[2];
    uint8_t *fontbase = &dev->fontdat[0][0];
    int blink, c;
    int drawcursor, cursorline;
    int mda = 0;
    uint16_t addr;
    uint8_t sc;

    /* The fake CRTC character height register selects whether MDA or CGA 
     * attributes are used */
    if (dev->cga_crtc[9] == 0 || dev->cga_crtc[9] == 13) {
	mda = 1;	
    }

    if (dev->font) {
	fontbase += 256*32;
    }
    addr = ((ma & ~1) + (dev->displine >> 5) * w) * 2;
    sc   = (dev->displine >> 1) & 15;

    ma += ((dev->displine >> 5) * w);

    if ((dev->real_crtc[10] & 0x60) == 0x20) {
	cursorline = 0;
    } else {
	cursorline = ((dev->real_crtc[10] & 0x1F) <= sc) &&
		     ((dev->real_crtc[11] & 0x1F) >= sc);
    }

    for (x = 0; x < w; x++) {
	chr  = dev->vram[(addr + 2 * x) & 0x3FFF];
	attr = dev->vram[(addr + 2 * x + 1) & 0x3FFF];
	drawcursor = ((ma == ca) && cursorline && dev->enabled &&
		(dev->cga_ctrl & 8) && (dev->blink & 16));
	blink = ((dev->blink & 16) && 
		(dev->cga_ctrl & 0x20) && 
		(attr & 0x80) && !drawcursor);

	if (dev->cga_ctrl & 0x20) attr &= 0x7F;

	/* MDA underline */
	if (sc == 14 && mda && ((attr & 7) == 1)) {
		for (c = 0; c < cw; c++)
			screen->line[dev->displine][(x * cw) + c].pal = dev->mdacols[attr][blink][1];
	} else {	/* Draw 16 pixels of character */
		bitmap[0] = fontbase[chr * 32 + 2 * sc];
		bitmap[1] = fontbase[chr * 32 + 2 * sc + 1];
		for (c = 0; c < 16; c++) {
			uint8_t col;

			if (c < 8)
				col = (mda ? dev->mdacols : dev->cgacols)[attr][blink][(bitmap[0] & (1 << (c ^ 7))) ? 1 : 0];
			else    col = (mda ? dev->mdacols : dev->cgacols)[attr][blink][(bitmap[1] & (1 << ((c & 7) ^ 7))) ? 1 : 0];
			if (!(dev->enabled) || !(dev->cga_ctrl & 8))
				col = dev->mdacols[0][0][0];

			if (w == 40) {
				screen->line[dev->displine][(x * cw) + 2*c].pal = col;
				screen->line[dev->displine][(x * cw) + 2*c + 1].pal = col;
			} else
				screen->line[dev->displine][(x * cw) + c].pal = col;
		}

		if (drawcursor) {
			for (c = 0; c < cw; c++)
				screen->line[dev->displine][(x * cw) + c].pal ^= (mda ? dev->mdacols : dev->cgacols)[attr][0][1];
		}
		++ma;
	}
    }
}


/* Draw a line in either of the CGA graphics modes (320x200 or 640x200) */
static void
cga_line(wy700_t *dev)
{
    int x, c;
    uint32_t dat;
    uint8_t ink = 0;
    uint16_t addr;
    uint16_t ma = (dev->cga_crtc[13] | (dev->cga_crtc[12] << 8)) & 0x3fff;

    addr = ((dev->displine >> 2) & 1) * 0x2000 +
	    (dev->displine >> 3) * 80 + ((ma & ~1) << 1);

    /* The fixed mode setting here programs the real CRTC with a screen 
     * width to 20, so draw in 20 fixed chunks of 4 bytes each */
    for (x = 0; x < 20; x++) {
	dat =  ((dev->vram[addr     & 0x3FFF] << 24) | 
		(dev->vram[(addr+1) & 0x3FFF] << 16) |
		(dev->vram[(addr+2) & 0x3FFF] << 8)  |
		(dev->vram[(addr+3) & 0x3FFF]));
	addr += 4;

	if (dev->wy700_mode == 6) {
		for (c = 0; c < 32; c++) {
			ink = (dat & 0x80000000) ? 16 + 15: 16 + 0;
			if (!(dev->enabled) || !(dev->cga_ctrl & 8))
				ink = 16;
			screen->line[dev->displine][x*64 + 2*c].pal = screen->line[dev->displine][x*64 + 2*c+1].pal = ink;
			dat = dat << 1;
		}
	} else {
		for (c = 0; c < 16; c++) {
			switch ((dat >> 30) & 3) {
				case 0: ink = 16 + 0; break;
				case 1: ink = 16 + 8; break;
				case 2: ink = 16 + 7; break;
				case 3: ink = 16 + 15; break;
			}
			if (!(dev->enabled) || !(dev->cga_ctrl & 8))
				ink = 16;
			screen->line[dev->displine][x*64 + 4*c].pal = screen->line[dev->displine][x*64 + 4*c+1].pal = screen->line[dev->displine][x*64 + 4*c+2].pal = screen->line[dev->displine][x*64 + 4*c+3].pal = ink;
			dat = dat << 2;
		}
	}
    }
}


/* Draw a line in the medium-resolution graphics modes (640x400 or 320x400) */
static void
medres_line(wy700_t *dev)
{
    int x, c;
    uint32_t dat;
    uint8_t ink = 0;
    uint32_t addr;

    addr = (dev->displine >> 1) * 80 + 4 * dev->wy700_base;

    for (x = 0; x < 20; x++) {
	dat =  ((dev->vram[addr     & 0x1FFFF] << 24) | 
		(dev->vram[(addr+1) & 0x1FFFF] << 16) |
		(dev->vram[(addr+2) & 0x1FFFF] << 8)  |
		(dev->vram[(addr+3) & 0x1FFFF]));
	addr += 4;

	if (dev->wy700_mode & 0x10) {
		for (c = 0; c < 16; c++) {
			switch ((dat >> 30) & 3) {
				case 0: ink = 16 + 0; break;
				case 1: ink = 16 + 8; break;
				case 2: ink = 16 + 7; break;
				case 3: ink = 16 + 15; break;
			}

			/* Display disabled? */
			if (!(dev->wy700_mode & 8)) ink = 16;
			screen->line[dev->displine][x*64 + 4*c].pal = screen->line[dev->displine][x*64 + 4*c+1].pal = screen->line[dev->displine][x*64 + 4*c+2].pal = screen->line[dev->displine][x*64 + 4*c+3].pal = ink;
			dat = dat << 2;
		}
	} else {
		for (c = 0; c < 32; c++) {
			ink = (dat & 0x80000000) ? 16 + 15: 16 + 0;

			/* Display disabled? */
			if (!(dev->wy700_mode & 8)) ink = 16;
			screen->line[dev->displine][x*64 + 2*c].pal = screen->line[dev->displine][x*64 + 2*c+1].pal = ink;
			dat = dat << 1;
		}
	}
    }
}


/* Draw a line in one of the high-resolution modes */
static void
hires_line(wy700_t *dev)
{
    int x, c;
    uint32_t dat;
    uint8_t ink = 0;
    uint32_t addr;

    addr = (dev->displine >> 1) * 160 + 4 * dev->wy700_base;

    if (dev->wy700_mode & 0x40) {	/* 800-line interleaved modes */
	if (dev->displine & 1) addr += 0x10000;
    }

    for (x = 0; x < 40; x++) {
	dat =  ((dev->vram[addr     & 0x1FFFF] << 24) | 
		(dev->vram[(addr+1) & 0x1FFFF] << 16) |
		(dev->vram[(addr+2) & 0x1FFFF] << 8)  |
		(dev->vram[(addr+3) & 0x1FFFF]));
	addr += 4;

	if (dev->wy700_mode & 0x10) {
		for (c = 0; c < 16; c++) {
			switch ((dat >> 30) & 3) {
				case 0: ink = 16 + 0; break;
				case 1: ink = 16 + 8; break;
				case 2: ink = 16 + 7; break;
				case 3: ink = 16 + 15; break;
			}

			/* Display disabled? */
			if (!(dev->wy700_mode & 8)) ink = 16;
			screen->line[dev->displine][x*32 + 2*c].pal = screen->line[dev->displine][x*32 + 2*c+1].pal = ink;
			dat = dat << 2;
		}
	} else {
		for (c = 0; c < 32; c++) {
			ink = (dat & 0x80000000) ? 16 + 15: 16 + 0;

			/* Display disabled? */
			if (!(dev->wy700_mode & 8)) ink = 16;
			screen->line[dev->displine][x*32 + c].pal = ink;
			dat = dat << 1;
		}
	}
    }
}


static void
wy700_poll(void *priv)
{
    wy700_t *dev = (wy700_t *)priv;
    int mode;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->cga_stat |= 1;
	dev->mda_stat |= 1;
	dev->linepos = 1;
	if (dev->dispon) {
		if (dev->displine == 0)
			video_blit_wait_buffer();
	
		if (dev->wy700_mode & 0x80) 
			mode = dev->wy700_mode & 0xF0;
		else
			mode = dev->wy700_mode & 0x0F;

		switch (mode) {
			default:
			case 0x00:
			case 0x02:
				text_line(dev);
				break;

			case 0x04:
			case 0x06:
				cga_line(dev);
				break;

			case 0x80:
			case 0x90:
				medres_line(dev);
				break;

			case 0xA0:
			case 0xB0:
			case 0xC0:
			case 0xD0:
			case 0xE0:
			case 0xF0:
				hires_line(dev);
				break;
		}
	}
	dev->displine++;

	/* Hardcode a fixed refresh rate and VSYNC timing */
	if (dev->displine == 800){
		 /* Start of VSYNC */
		dev->cga_stat |= 8;
		dev->dispon = 0;
	}
	if (dev->displine == 832) {
		/* End of VSYNC */
		dev->displine = 0;
		dev->cga_stat &= ~8;
		dev->dispon = 1;
	}
    } else {
	if (dev->dispon) {
		dev->cga_stat &= ~1;
		dev->mda_stat &= ~1;
	}
	dev->vidtime += dev->dispontime;
	dev->linepos = 0;

	if (dev->displine == 800) {
		/* Hardcode 1280x800 window size */
		if ((WY700_XSIZE != xsize) || (WY700_YSIZE != ysize) || video_force_resize_get()) {
			xsize = WY700_XSIZE;
			ysize = WY700_YSIZE;
			if (xsize < 64) xsize = 656;
			if (ysize < 32) ysize = 200;
			set_screen_size(xsize, ysize);

			if (video_force_resize_get())
				video_force_resize_set(0);
		}

		video_blit_start(1, 0, 0, 0, ysize, xsize, ysize);
		frames++;

		/* Fixed 1280x800 resolution */
		video_res_x = WY700_XSIZE;
		video_res_y = WY700_YSIZE;
		if (dev->wy700_mode & 0x80) 

			mode = dev->wy700_mode & 0xF0;
		else
			mode = dev->wy700_mode & 0x0F;

		switch(mode) {
			case 0x00: 
			case 0x02: video_bpp = 0; break;
			case 0x04: 
			case 0x90: 
			case 0xB0:
			case 0xD0:
			case 0xF0: video_bpp = 2; break;
			default:   video_bpp = 1; break;	
		}
		dev->blink++;
	}
    }
}


static int
load_font(wy700_t *dev, const wchar_t *s)
{
    FILE *fp;
    int c;

    fp = rom_fopen(s, L"rb");
    if (fp == NULL) {
	ERRLOG("%s: cannot load font '%ls'\n", dev->name, s);
	return(0);
    }

    for (c = 0; c < 512; c++)
	(void)fread(&dev->fontdat[c][0], 1, 32, fp);

    (void)fclose(fp);

    return(1);
}


static void *
wy700_init(const device_t *info, UNUSED(void *parent))
{
    wy700_t *dev;
    int c;

    dev = (wy700_t *)mem_alloc(sizeof(wy700_t));
    memset(dev, 0x00, sizeof(wy700_t));
    dev->name = info->name;

    if (! load_font(dev, info->path)) {
	free(dev);
	return(NULL);
    }

    /* 128K video RAM */
    dev->vram = (uint8_t *)mem_alloc(0x20000);

    timer_add(wy700_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    /* Occupy memory between 0xB0000 and 0xBFFFF (moves to 0xA0000 in
     * high-resolution modes)
     */
    mem_map_add(&dev->mapping, 0xb0000, 0x10000,
		wy700_read,NULL,NULL, wy700_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, dev);

    /* Respond to both MDA and CGA I/O ports */
    io_sethandler(0x03b0, 12,
		  wy700_in,NULL,NULL, wy700_out,NULL,NULL, dev);
    io_sethandler(0x03d0, 16,
		  wy700_in,NULL,NULL, wy700_out,NULL,NULL, dev);

    /* Set up the emulated attributes. 
     * CGA is done in four groups: 00-0F, 10-7F, 80-8F, 90-FF */
    for (c = 0; c < 0x10; c++) {
	dev->cgacols[c][0][0] = dev->cgacols[c][1][0] = dev->cgacols[c][1][1] = 16;
	if (c & 8)
		dev->cgacols[c][0][1] = 15 + 16;
	else
		dev->cgacols[c][0][1] =  7 + 16;
    }
    for (c = 0x10; c < 0x80; c++) {
	dev->cgacols[c][0][0] = dev->cgacols[c][1][0] = dev->cgacols[c][1][1] = 16 + 7;
	if (c & 8)
		dev->cgacols[c][0][1] = 15 + 16;
	else
		dev->cgacols[c][0][1] =  0 + 16;
	if ((c & 0x0F) == 8)
		dev->cgacols[c][0][1] = 8 + 16;
    }

    /* With special cases for 00, 11, 22, ... 77 */
    dev->cgacols[0x00][0][1] = dev->cgacols[0x00][1][1] = 16;
    for (c = 0x11; c <= 0x77; c += 0x11) {
	dev->cgacols[c][0][1] = dev->cgacols[c][1][1] = 16 + 7;
    }
    for (c = 0x80; c < 0x90; c++) {
	dev->cgacols[c][0][0] = 16 + 8;
	if (c & 8)
		dev->cgacols[c][0][1] = 15 + 16;
	else
		dev->cgacols[c][0][1] =  7 + 16;
	dev->cgacols[c][1][0] = dev->cgacols[c][1][1] = dev->cgacols[c-0x80][0][0];
    }
    for (c = 0x90; c < 0x100; c++) {
	dev->cgacols[c][0][0] = 16 + 15;
	if (c & 8)
		dev->cgacols[c][0][1] =  8 + 16;
	else
		dev->cgacols[c][0][1] =  7 + 16;
	if ((c & 0x0F) == 0)
		dev->cgacols[c][0][1] = 16;
	dev->cgacols[c][1][0] = dev->cgacols[c][1][1] = dev->cgacols[c-0x80][0][0];
    }
    /* Also special cases for 99, AA, ..., FF */
    for (c = 0x99; c <= 0xFF; c += 0x11)
	dev->cgacols[c][0][1] = 16 + 15;
    /* Special cases for 08, 80 and 88 */
    dev->cgacols[0x08][0][1] = 16 + 8;
    dev->cgacols[0x80][0][1] = 16;
    dev->cgacols[0x88][0][1] = 16 + 8;

    /* MDA attributes */
    for (c = 0; c < 256; c++) {
	dev->mdacols[c][0][0] = dev->mdacols[c][1][0] = dev->mdacols[c][1][1] = 16;
	if (c & 8)
		dev->mdacols[c][0][1] = 15 + 16;
	else
		dev->mdacols[c][0][1] =  7 + 16;
    }
    dev->mdacols[0x70][0][1] = 16;
    dev->mdacols[0x70][0][0] = dev->mdacols[0x70][1][0] = dev->mdacols[0x70][1][1] = 16 + 15;
    dev->mdacols[0xF0][0][1] = 16;
    dev->mdacols[0xF0][0][0] = dev->mdacols[0xF0][1][0] = dev->mdacols[0xF0][1][1] = 16 + 15;
    dev->mdacols[0x78][0][1] = 16 + 7;
    dev->mdacols[0x78][0][0] = dev->mdacols[0x78][1][0] = dev->mdacols[0x78][1][1] = 16 + 15;
    dev->mdacols[0xF8][0][1] = 16 + 7;
    dev->mdacols[0xF8][0][0] = dev->mdacols[0xF8][1][0] = dev->mdacols[0xF8][1][1] = 16 + 15;
    dev->mdacols[0x00][0][1] = dev->mdacols[0x00][1][1] = 16;
    dev->mdacols[0x08][0][1] = dev->mdacols[0x08][1][1] = 16;
    dev->mdacols[0x80][0][1] = dev->mdacols[0x80][1][1] = 16;
    dev->mdacols[0x88][0][1] = dev->mdacols[0x88][1][1] = 16;

    /* Start off in 80x25 text mode */
    dev->cga_stat   = 0xF4;
    dev->wy700_mode = 2;
    dev->enabled    = 1;
    memcpy(dev->real_crtc, mode_80x24, sizeof(mode_80x24));

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return(dev);
}


static void
wy700_close(void *priv)
{
    wy700_t *dev = (wy700_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(void *priv)
{
    wy700_t *dev = (wy700_t *)priv;
	
    recalc_timings(dev);
}


static const video_timings_t wy700_timings = { VID_ISA,8,16,32,8,16,32 };

const device_t wy700_device = {
    "Wyse 700",
    DEVICE_VIDEO(VID_TYPE_CGA) | DEVICE_ISA,
    0,
    FONT_ROM_PATH,
    wy700_init, wy700_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &wy700_timings,
    NULL
};
