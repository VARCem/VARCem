/*
		dev->genius_control |= 0x10;
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		MDSI Genius VHR emulation.
 *
 *		I'm at something of a disadvantage writing this emulation:
 *		I don't have an MDSI Genius card, nor do I have the BIOS
 *		extension (VHRBIOS.SYS) that came with it. What I do have
 *		are the GEM and Windows 1.04 drivers, plus a driver for a
 *		later MCA version of the card. The latter can be found at 
 *		<http://files.mpoli.fi/hardware/DISPLAY/GENIUS/> and is
 *		necessary if you want the Windows driver to work.
 *
 *		This emulation appears to work correctly with:
 *		  The MCA drivers GMC_ANSI.SYS and INS_ANSI.SYS
 *		  The GEM driver SDGEN9.VGA
 *		  The Windows 1.04 driver GENIUS.DRV
 *
 *		As far as I can see, the card uses a fixed resolution of
 *		728x1008 pixels. It has the following modes of operation:
 * 
 *		> MDA-compatible:      80x25 text, each character 9x15 pixels.
 *		> CGA-compatible:      640x200 mono graphics
 *		> Dual:                MDA text in the top half, CGA graphics
 *					in the bottom
 *		> Native text:         80x66 text, each character 9x15 pixels.
 *		> Native graphics:     728x1008 mono graphics.
 *
 *		Under the covers, this seems to translate to:
 *		> Text framebuffer.     At B000:0000, 16k. Displayed if
 *					enable bit is set in the MDA control
 *					register.
 *		> Graphics framebuffer. In native modes goes from A000:0000
 *					to A000:FFFF and B800:0000 to
 *					B800:FFFF. In CGA-compatible mode
 *					only the section at B800:0000 to
 *					B800:7FFF is visible. Displayed if
 *					enable bit is set in the CGA control
 *					register.
 * 
 *		Two card-specific registers control text and graphics display:
 * 
 *		03B0: Control register.
 *		  Bit 0: Map all graphics framebuffer into memory.
 *		  Bit 2: Unknown. Set by GMC /M; cleared by mode set or GMC /T.
 *		  Bit 4: Set for CGA-compatible graphics, clear for native graphics.
 *		  Bit 5: Set for black on white, clear for white on black.
 *
 *		03B1: Character height register.
 *		  Bits 0-1: Character cell height (0 => 15, 1 => 14, 2 => 13, 3 => 12)
 *		  Bit  4:   Set to double character cell height (scanlines are doubled)
 *		  Bit  7:   Unknown, seems to be set for all modes except 80x66
 *
 *		Not having the card also means I don't have its font.
 *		According to the card brochure the font is an 8x12 bitmap
 *		in a 9x15 character cell. I therefore generated it by taking
 *		the MDA font, increasing graphics to 16 pixels in height and
 *		reducing the height of characters so they fit in an 8x12 cell
 *		if necessary.
 *
 * Version:	@(#)vid_genius.c	1.0.17	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *              John Elliott, <jce@seasip.info>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2016-2019 John Elliott.
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
#include "../../timer.h"
#include "../../io.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"


#define FONT_ROM_PATH	L"video/mdsi/genius/8x12.bin"


#define GENIUS_XSIZE	728
#define GENIUS_YSIZE	1008


typedef struct {
    const char	*name;

    mem_map_t	mapping;

    uint8_t	mda_crtc[32];	/* The 'CRTC' as the host PC sees it */
    int		mda_crtcreg;	/* Current CRTC register */

    uint8_t	cga_crtc[32];	/* The 'CRTC' as the host PC sees it */
    int		cga_crtcreg;	/* Current CRTC register */

    uint8_t	genius_control;	/* Native control register 
				 * I think bit 0 enables the full 
				 * framebuffer. 
				 */
    uint8_t	genius_charh;	/* Native character height register: 
				 * 00h => chars are 15 pixels high 
				 * 81h => chars are 14 pixels high
				 * 83h => chars are 12 pixels high 
				 * 90h => chars are 30 pixels high [15 x 2]
				 * 93h => chars are 24 pixels high [12 x 2]
				 */
    uint8_t	genius_mode;	/* Current mode (see list at top of file) */

    uint8_t	cga_ctrl;	/* Emulated CGA control register */
    uint8_t	cga_color;	/* Emulated CGA color register (ignored) */
    uint8_t	cga_stat;	/* CGA status (IN 0x3DA) */

    uint8_t	mda_ctrl;	/* Emulated MDA control register */
    uint8_t	mda_stat;	/* MDA status (IN 0x3BA) */

    int		enabled;	/* Display enabled, 0 or 1 */
    int		font;		/* Current font, 0 or 1 */
    int		detach;		/* Detach cursor, 0 or 1 */

    tmrval_t	dispontime, dispofftime;
    tmrval_t	vidtime;
    tmrval_t	vsynctime;
	
    int		linepos, displine;
    int		vc;
    int		dispon, blink;

    uint8_t	pal[4];

    uint8_t	attr[256][2][2];	/* attr -> colors */

    uint8_t	fontdatm[256][16];
    uint8_t	fontdat[256][16];

    uint8_t	*vram;
} genius_t;


static const video_timings_t	genius_timings = { VID_ISA,8,16,32,8,16,32 };
static const int8_t		wsarray[16] = {3,4,5,6,7,8,4,5,6,7,8,4,5,6,7,8};


static void
recalc_timings(genius_t *dev)
{
    double _dispontime, _dispofftime;
    double disptime;

    disptime = 0x31;
    _dispontime = 0x28;
    _dispofftime = disptime - _dispontime;
    _dispontime  *= MDACONST;
    _dispofftime *= MDACONST;

    dev->dispontime  = (tmrval_t)(_dispontime  * (1LL << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1LL << TIMER_SHIFT));
}


static void
wait_states(void)
{
    int8_t ws;

    ws = wsarray[cycles & 0x0f];
    cycles -= ws;
}


static int
get_lines(genius_t *dev)
{
    int ret = 350;

    switch (dev->genius_charh & 0x13) {
	case 0x00:
		ret = 990;	/* 80x66 */
		break;

	case 0x01:
		ret = 980;	/* 80x70 */
		break;

	case 0x02:
		ret = 988;	/* guess: 80x76 */
		break;

	case 0x03:
		ret = 984;	/* 80x82 */
		break;

	case 0x10:
		ret = 375;	/* logic says 80x33 but it appears to be 80x25 */
		break;

	case 0x11:
		ret = 490;	/* guess: 80x35, fits the logic as well, half of 80x70 */
		break;

	case 0x12:
		ret = 494;	/* guess: 80x38 */
		break;

	case 0x13:
		ret = 492;	/* 80x41 */
		break;
    }

    return ret;
}


/* Draw a single line of the screen in either text mode */
static void
text_line(genius_t *dev, uint8_t bg, int mda, int cols80)
{
    int w  = 80;	/* 80 characters across */
    int cw = 9;		/* each character is 9 pixels wide */
    uint8_t chr, attr, sc, ctrl;
    uint8_t *crtc, bitmap[2];
    int x, blink, c, row, charh;
    int drawcursor, cursorline;
    uint16_t addr;
    uint16_t ma = (dev->mda_crtc[13] | (dev->mda_crtc[12] << 8)) & 0x3fff;
    uint16_t ca = (dev->mda_crtc[15] | (dev->mda_crtc[14] << 8)) & 0x3fff;
    uint8_t *framebuf = dev->vram + 0x010000;
    uint32_t dl = dev->displine;
    uint8_t col;

    if (mda) {
	/* Character height is 12-15. */
	if (dev->displine >= get_lines(dev))
		return;

	crtc = dev->mda_crtc;
	ctrl = dev->mda_ctrl;
	charh = 15 - (dev->genius_charh & 3);

#if 0
	if (dev->genius_charh & 0x10) {
		row = ((dl >> 1) / charh);	
		sc  = ((dl >> 1) % charh);	
	} else {
		row = (dl / charh);	
		sc  = (dl % charh);	
	}
#else
	row = (dl / charh);	
	sc  = (dl % charh);	
#endif
    } else {
	if ((dev->displine < 512)  || (dev->displine >= 912))
		return;

	crtc = dev->cga_crtc;
	ctrl = dev->cga_ctrl;
	framebuf += 0x08000;

	dl -= 512;
	w = crtc[1];
	cw = 8;
	charh = crtc[9] + 1;

	row = ((dl >> 1) / charh);	
	sc  = ((dl >> 1) % charh);	
    }

    ma = (crtc[13] | (crtc[12] << 8)) & 0x3fff;
    ca = (crtc[15] | (crtc[14] << 8)) & 0x3fff;

    addr = ((ma & ~1) + row * w) * 2;

    if (! mda)
	dl += 512;

    ma += (row * w);

    if ((crtc[10] & 0x60) == 0x20)
	cursorline = 0;
    else
	cursorline = ((crtc[10] & 0x1f) <= sc) && ((crtc[11] & 0x1f) >= sc);

    for (x = 0; x < w; x++) {
#if 0
	if ((dev->genius_charh & 0x10) && ((addr + 2 * x) > 0x0fff))
		chr = 0x00;
	if ((dev->genius_charh & 0x10) && ((addr + 2 * x + 1) > 0x0fff))
		attr = 0x00;
#endif
	chr  = framebuf[(addr + 2 * x) & 0x3fff];
	attr = framebuf[(addr + 2 * x + 1) & 0x3fff];

	drawcursor = ((ma == ca) && cursorline && dev->enabled && (ctrl & 8));

	switch (crtc[10] & 0x60) {
		case 0x00:
			drawcursor = drawcursor && (dev->blink & 16);
			break;

		case 0x60:
			drawcursor = drawcursor && (dev->blink & 32);
			break;
	}

	blink = ((dev->blink & 16) && (ctrl & 0x20) && (attr & 0x80) && !drawcursor);

	if (ctrl & 0x20)
		attr &= 0x7f;

	/* MDA underline. */
	if (mda && (sc == charh) && ((attr & 7) == 1)) {
		col = dev->attr[attr][blink][1];

		if (dev->genius_control & 0x20)
			col ^= 15;

		for (c = 0; c < cw; c++) {
			if (col != bg) {
				if (cols80)
					screen->line[dl][(x * cw) + c].pal = col;
				else {
					screen->line[dl][((x * cw) << 1) + (c << 1)].pal =
					screen->line[dl][((x * cw) << 1) + (c << 1) + 1].pal = col;
				}
			}
		}
	} else {
		/* Draw 8 pixels of character. */
		if (mda)
			bitmap[0] = dev->fontdatm[chr][sc];
		else
			bitmap[0] = dev->fontdat[chr][sc];

		for (c = 0; c < 8; c++) {
			col = dev->attr[attr][blink][(bitmap[0] & (1 << (c ^ 7))) ? 1 : 0];
			if (!dev->enabled || !(ctrl & 8))
				col = dev->attr[0][0][0];

			if (dev->genius_control & 0x20)
				col ^= 15;

			if (col != bg) {
				if (cols80)
					screen->line[dl][(x * cw) + c].pal = col;
				else {
					screen->line[dl][((x * cw) << 1) + (c << 1)].pal =
					screen->line[dl][((x * cw) << 1) + (c << 1) + 1].pal = col;
				}
			}
		}

		if (cw == 9) {
			/* The ninth pixel column... */
			if ((chr & ~0x1f) == 0xc0) {
				/* Echo column 8 for the graphics chars. */
				if (cols80) {
					col = screen->line[dl][(x * cw) + 7].pal;
					if (col != bg)
                	                        screen->line[dl][(x * cw) + 8].pal = col;
				} else {
					col = screen->line[dl][((x * cw) << 1) + 14].pal;
					if (col != bg) {
               	        	                screen->line[dl][((x * cw) << 1) + 16].pal =
               	                	        screen->line[dl][((x * cw) << 1) + 17].pal = col;
					}
				}
			} else {
				/* Otherwise fill with background. */
				col = dev->attr[attr][blink][0];
				if (dev->genius_control & 0x20)
					col ^= 15;
				if (col != bg) {
					if (cols80)
        	                                screen->line[dl][(x * cw) + 8].pal = col;
					else {
                        	                screen->line[dl][((x * cw) << 1) + 16].pal =
                                	        screen->line[dl][((x * cw) << 1) + 17].pal = col;
					}
				}
			}
		}

		if (drawcursor) {
			for (c = 0; c < cw; c++) {
				if (cols80)
                               		screen->line[dl][(x * cw) + c].pal ^= dev->attr[attr][0][1];
				else {
					screen->line[dl][((x * cw) << 1) + (c << 1)].pal ^= dev->attr[attr][0][1];
					screen->line[dl][((x * cw) << 1) + (c << 1) + 1].pal ^= dev->attr[attr][0][1];
				}
			}
		}

		ma++;
	}
    }
}


/* Draw a line in the CGA 640x200 mode. */
static void
cga_line(genius_t *dev)
{
    uint8_t ink_f, ink_b;
    uint32_t addr;
    uint8_t dat;
    int x, c;

    ink_f = (dev->genius_control & 0x20) ? dev->pal[0] : dev->pal[3];
    ink_b = (dev->genius_control & 0x20) ? dev->pal[3] : dev->pal[0];

    /* We draw the CGA at row 512 */
    if ((dev->displine < 512) || (dev->displine >= 912))
	return;

    addr = 0x018000 + 80 * ((dev->displine - 512) >> 2);
    if ((dev->displine - 512) & 2)
	addr += 0x002000;

    for (x = 0; x < 80; x++) {
	dat = dev->vram[addr];
	addr++;

	for (c = 0; c < 8; c++) {
		if (dat & 0x80)
			screen->line[dev->displine][(x << 3) + c].pal = ink_f;
		else
			screen->line[dev->displine][(x << 3) + c].pal = ink_b;

		dat = dat << 1;
	}
    }
}


/* Draw a line in the native high-resolution mode. */
static void
hires_line(genius_t *dev)
{
    uint8_t ink_f, ink_b;
    uint32_t addr;
    uint8_t dat;
    int x, c;

    ink_f = (dev->genius_control & 0x20) ? dev->pal[0] : dev->pal[3];
    ink_b = (dev->genius_control & 0x20) ? dev->pal[3] : dev->pal[0];

    if (dev->displine < 512)	/* first 512 lines live at A0000 */
	addr = 128 * dev->displine;
    else			/* second 496 live at B8000 */
	addr = 0x018000 + (128 * (dev->displine - 512));

    for (x = 0; x < 91; x++) {
	dat =  dev->vram[addr + x];

	for (c = 0; c < 8; c++) {
		if (dat & 0x80)
			screen->line[dev->displine][(x << 3) + c].pal = ink_f;
		else
			screen->line[dev->displine][(x << 3) + c].pal = ink_b;

		dat = dat << 1;
	}
    }
}


static void
genius_poll(priv_t priv)
{
    genius_t *dev = (genius_t *)priv;
    uint8_t bg;
    int x;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->cga_stat |= 1;
	dev->mda_stat |= 1;
	dev->linepos = 1;

	if (dev->dispon) {
		if (dev->genius_control & 0x20)
			bg = dev->pal[3];
		else
			bg = dev->pal[0];

		if (dev->displine == 0)
			video_blit_wait_buffer();

		/* Start off with a blank line. */
		for (x = 0; x < GENIUS_XSIZE; x++)
			screen->line[dev->displine][x].pal = bg;

		/* If graphics display enabled, draw graphics on top
		 * of the blanked line */
		if (dev->cga_ctrl & 8) {
			if (((dev->genius_control & 0x11) == 0x00) || (dev->genius_control & 0x08))
				cga_line(dev);
			else if ((dev->genius_control & 0x11) == 0x01)
				hires_line(dev);
			else {
				if (dev->cga_ctrl & 2)
					cga_line(dev);
				else {
					if (dev->cga_ctrl & 1)
						text_line(dev, bg, 0, 1);
					else
						text_line(dev, bg, 0, 0);
				}
			}
		}

		/* If MDA display is enabled, draw MDA text on top
		 * of the lot */
		if (dev->mda_ctrl & 8)
			text_line(dev, bg, 1, 1);
	}
	dev->displine++;

	/* Hardcode a fixed refresh rate and VSYNC timing */
	if (dev->displine == GENIUS_YSIZE) {
		/* Start of VSYNC */
		dev->cga_stat |= 8;
		dev->mda_stat |= 8;
		dev->dispon = 0;
	}

	if (dev->displine == 1040) {
		/* End of VSYNC */
		dev->displine = 0;
		dev->cga_stat &= ~8;
		dev->mda_stat &= ~8;
		dev->dispon = 1;
	}
    } else {
	if (dev->dispon) {
		dev->cga_stat &= ~1;
		dev->mda_stat &= ~1;
	}
	dev->vidtime += dev->dispontime;
	dev->linepos = 0;

	if (dev->displine == GENIUS_YSIZE) {
		/* Hardcode GENIUS_XSIZE * GENIUS_YSIZE window size */
		if (GENIUS_XSIZE != xsize || GENIUS_YSIZE != ysize) {
			xsize = GENIUS_XSIZE;
			ysize = GENIUS_YSIZE;
			if (xsize < 64) xsize = 656;
			if (ysize < 32) ysize = 200;
			set_screen_size(xsize, ysize);

			if (video_force_resize_get())
				video_force_resize_set(0);
		}

		video_blit_start(1, 0, 0, 0, ysize, xsize, ysize);
		frames++;

		/* Fixed 728x1008 resolution */
		video_res_x = GENIUS_XSIZE;
		video_res_y = GENIUS_YSIZE;
		video_bpp = 1;
		dev->blink++;
	}
    }
}


static void
genius_out(uint16_t port, uint8_t val, priv_t priv)
{
    genius_t *dev = (genius_t *)priv;

    DEBUG("GENIUS: out(%04x, %02x)\n", port, val);

    switch (port) {
	case 0x03b0: 	/* command / control register */
		dev->genius_control = val;
		if (val & 1)
			mem_map_set_addr(&dev->mapping, 0xa0000, 0x28000);
		else
			mem_map_set_addr(&dev->mapping, 0xb0000, 0x10000);
		break;

	case 0x03b1:
		dev->genius_charh = val;
		break;

	case 0x03b2:	/* emulated MDA CRTC, register select */
	case 0x03b4:
	case 0x03b6:
		dev->mda_crtcreg = val & 31;
		break;

	case 0x03b3:	/* emulated MDA CRTC, value */
	case 0x03b5:
	case 0x03b7:
		dev->mda_crtc[dev->mda_crtcreg] = val;
		recalc_timings(dev);
		break;

	case 0x03b8:	/* emulated MDA control register */
	     	dev->mda_ctrl = val;
	      	break;

	case 0x03d0:	/* emulated CGA CRTC, register select */
	case 0x03d2:
	case 0x03d4:
	case 0x03d6:
		dev->cga_crtcreg = val & 31;
		break;

	case 0x03d1:	/* emulated CGA CRTC, value */
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		dev->cga_crtc[dev->cga_crtcreg] = val;
		recalc_timings(dev);
		break;

	case 0x03d8:	/* emulated CGA control register */
	     	dev->cga_ctrl = val;
	      	break;

	case 0x03d9:	/* emulated CGA color register */
	       	dev->cga_color = val;
	      	break;
    }
}


static uint8_t
genius_in(uint16_t port, priv_t priv)
{
    genius_t *dev = (genius_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x03b0: case 0x03b2: case 0x03b4: case 0x03b6:
		ret = dev->mda_crtcreg;
		break;

	case 0x03b1: case 0x03b3: case 0x03b5: case 0x03b7:
		ret = dev->mda_crtc[dev->mda_crtcreg];
		break;

	case 0x03b8: 
		ret = dev->mda_ctrl;
		break;

	case 0x03ba: 
		ret = dev->mda_stat;
		break;

	case 0x03d0: case 0x03d2: case 0x03d4: case 0x03d6:
		ret = dev->cga_crtcreg;
		break;

	case 0x03d1: case 0x03d3: case 0x03d5: case 0x03d7:
		ret = dev->cga_crtc[dev->cga_crtcreg];
		break;

	case 0x03d8:
		ret = dev->cga_ctrl;
		break;

	case 0x03d9:
		ret = dev->cga_color;
		break;

	case 0x03da:
		ret = dev->cga_stat;
		break;

	default:
		break;
    }

    return(ret);
}


static void
genius_write(uint32_t addr, uint8_t val, priv_t priv)
{
    genius_t *dev = (genius_t *)priv;

    wait_states();

    if (dev->genius_control & 1) {
	if ((addr >= 0x0a0000) && (addr < 0x0b0000))
		addr = (addr - 0x0a0000) & 0x00ffff;
	else if ((addr >= 0x0b0000) && (addr < 0x0b8000))
		addr = ((addr - 0x0b0000) & 0x007fff) + 0x010000;
	else
		addr = ((addr - 0x0b8000) & 0x00ffff) + 0x018000;
    } else {
	/* If hi-res memory is disabled, only visible in the B000 segment */
	if (addr >= 0x0b8000)
		addr = (addr & 0x003fff) + 0x018000;
	else
		addr = (addr & 0x007fff) + 0x010000;
    }

    dev->vram[addr] = val;
}


static uint8_t
genius_read(uint32_t addr, priv_t priv)
{
    genius_t *dev = (genius_t *)priv;

    wait_states();

    if (dev->genius_control & 1) {
	if ((addr >= 0x0a0000) && (addr < 0x0b0000))
		addr = (addr - 0x0a0000) & 0x00ffff;
	else if ((addr >= 0x0b0000) && (addr < 0x0b8000))
		addr = ((addr - 0x0b0000) & 0x007fff) + 0x010000;
	else
		addr = ((addr - 0x0b8000) & 0x00ffff) + 0x018000;
    } else {
	/* If hi-res memory is disabled, only visible in the B000 segment */
	if (addr >= 0x0b8000)
		addr = (addr & 0x003fff) + 0x018000;
	else
		addr = (addr & 0x007fff) + 0x010000;
    }

    return(dev->vram[addr]);
}


static int
load_font(genius_t *dev, const wchar_t *fn)
{
    FILE *fp;
    int c;

    fp = rom_fopen(fn, L"rb");
    if (fp == NULL) {
	ERRLOG("%s: cannot load font '%ls'\n", dev->name, fn);
	return(0);
    }

    for (c = 0; c < 256; c++)
	(void)fread(&dev->fontdatm[c][0], 1, 16, fp);

    (void)fclose(fp);

    return(1);
}


static void
genius_close(priv_t priv)
{
    genius_t *dev = (genius_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    genius_t *dev = (genius_t *)priv;

    recalc_timings(dev);
}


static priv_t
genius_init(const device_t *info, UNUSED(void *parent))
{
    genius_t *dev;
    int c;

    dev = (genius_t *)mem_alloc(sizeof(genius_t));
    memset(dev, 0x00, sizeof(genius_t));
    dev->name = info->name;

    if (! load_font(dev, FONT_ROM_PATH)) {
	free(dev);
	return(NULL);
    }

    /* Allocate 160K video RAM. */
    dev->vram = (uint8_t *)mem_alloc(0x028000);

    timer_add(genius_poll, (priv_t)dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    /*
     * Occupy memory between 0xB0000 and 0xBFFFF.
     * (moves to 0xA0000 in high-resolution modes)
     */
    mem_map_add(&dev->mapping, 0x0b0000, 0x010000,
		genius_read,NULL,NULL, genius_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, (priv_t)dev);

    /* Respond to both MDA and CGA I/O ports. */
    io_sethandler(0x03b0, 12,
		  genius_in,NULL,NULL, genius_out,NULL,NULL, (priv_t)dev);
    io_sethandler(0x03d0, 16,
		  genius_in,NULL,NULL, genius_out,NULL,NULL, (priv_t)dev);

    dev->pal[0] =  0 + 16;	/* 0 */
    dev->pal[1] =  8 + 16;	/* 8 */
    dev->pal[2] =  7 + 16;	/* 7 */
    dev->pal[3] = 15 + 16;	/* F */

    /*
     * MDA attributes.
     * I don't know if the Genius's MDA emulation actually does 
     * emulate bright / non-bright. For the time being pretend
     * it does.
     */
    for (c = 0; c < 256; c++) {
	dev->attr[c][0][0] = dev->attr[c][1][0] =
		dev->attr[c][1][1] = dev->pal[0];
	if (c & 8)
		dev->attr[c][0][1] = dev->pal[3];
	else
		dev->attr[c][0][1] = dev->pal[2];
    }
    dev->attr[0x70][0][1] = dev->pal[0];
    dev->attr[0x70][0][0] = dev->attr[0x70][1][0] =
		dev->attr[0x70][1][1] = dev->pal[3];
    dev->attr[0xf0][0][1] = dev->pal[0];
    dev->attr[0xf0][0][0] = dev->attr[0xf0][1][0] =
		dev->attr[0xf0][1][1] = dev->pal[3];
    dev->attr[0x78][0][1] = dev->pal[2];
    dev->attr[0x78][0][0] = dev->attr[0x78][1][0] =
		dev->attr[0x78][1][1] = dev->pal[3];
    dev->attr[0xf8][0][1] = dev->pal[2];
    dev->attr[0xf8][0][0] = dev->attr[0xf8][1][0] =
		dev->attr[0xf8][1][1] = dev->pal[3];
    dev->attr[0x00][0][1] = dev->attr[0x00][1][1] = dev->pal[0];
    dev->attr[0x08][0][1] = dev->attr[0x08][1][1] = dev->pal[0];
    dev->attr[0x80][0][1] = dev->attr[0x80][1][1] = dev->pal[0];
    dev->attr[0x88][0][1] = dev->attr[0x88][1][1] = dev->pal[0];

    /* Set up for 80x25 text mode. */
    dev->enabled = 1;
    dev->cga_stat = 0xf4;
    dev->genius_mode = 2;
    dev->genius_control |= 0x10;
    dev->genius_charh = 0x90; /* native character height register */

    video_inform(DEVICE_VIDEO_GET(info->flags), &genius_timings);

    return((priv_t)dev);
}


const device_t genius_device = {
    "Genius VHR",
    DEVICE_VIDEO(VID_TYPE_MDA) | DEVICE_ISA,
    0,
    FONT_ROM_PATH,
    genius_init, genius_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};
