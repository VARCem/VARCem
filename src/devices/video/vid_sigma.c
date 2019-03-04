/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Sigma Color 400 video card.
 *
 *		The Sigma Designs Color 400 is a video card from 1985,
 *		presumably intended as an EGA competitor.
 * 
 *		The hardware seems to have gone through various iterations;
 *		I've seen pictures of full-length and half-length versions.
 *		TH99 describes the jumpers / switches: 
 *
 *                    <http://arvutimuuseum.ee/th99/v/S-T/52579.htm>
 *
 *		The card is CGA-compatible at BIOS level, but to improve
 *		compatibility attempts to write to the CGA I/O ports at
 *		0x3D0-0x3DF trigger an NMI. The card's BIOS handles the NMI
 *		and translates the CGA writes into commands to its own
 *		hardware at 0x2D0-0x2DF. (DIP switches on the card allow the
 *		base address to be changed, but since the BIOS dump I have
 *		doesn't support this I haven't emulated it. Likewise the
 *		BIOS ROM address can be changed, but I'm going with the
 *		default of 0xC0000).
 *
 *		The BIOS still functions if the NMI system isn't operational.
 *		There doesn't seem to be a jumper or DIP switch to lock it
 *		out, but at startup the BIOS tests for its presence and
 *		configures itself to work or not as required. I've therefore
 *		added a configuration option to handle this.
 *
 *		The card's real CRTC at 0x2D0/0x2D1 appears to be a 6845.
 *		One oddity is that all its horizontal counts are halved
 *		compared to what a real CGA uses; 40-column modes have a width
 *		of 20, and 80-column modes have a width of 40. This means that
 *		the CRTC cursor position (registers 14/15) can only address
 *		even-numbered columns, so the top bit of the control register
 *		at 0x2D9 is used to adjust the position.
 *
 * Version:	@(#)vid_sigma.c	1.0.3	2019/03/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *              John Elliott, <jce@seasip.info>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2018 Miran Grca.
 *		Copyright 2018 John Elliott.
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/nmi.h"
#include "../system/pit.h"
#include "video.h"
#include "vid_cga.h"


#define FONT_ROM_PATH	L"video/sigma/sigma400_font.rom"
#define BIOS_ROM_PATH	L"video/sigma/sigma400_bios.rom"


/* 0x2D8: Mode register. Values written by the card BIOS are:
 *		Text 40x25:       0xA8
 *		Text 80x25:       0xB9
 *		Text 80x30:       0xB9
 *		Text 80x50:       0x79
 *		Graphics 320x200: 0x0F
 *		Graphics 640x200: 0x1F
 *		Graphics 640x400: 0x7F
 *
 * I have assumed this is a bitmap with the following meaning:
 */
#define MODE_80COLS   0x01	/* For text modes, 80 columns across */
#define MODE_GRAPHICS 0x02	/* Graphics mode */
#define MODE_NOBLINK  0x04	/* Disable blink? */
#define MODE_ENABLE   0x08	/* Enable display */
#define MODE_HRGFX    0x10	/* For graphics modes, 640 pixels across */
#define MODE_640x400  0x40	/* 400-line graphics mode */
#define MODE_FONT16   0x80	/* Use 16-pixel high font */

/*
 * 0x2D9: Control register, with the following bits:
 */
#define CTL_CURSOR	0x80	/* Low bit of cursor position */
#define CTL_NMI  	0x20	/* Writes to 0x3D0-0x3DF trigger NMI */
#define CTL_CLEAR_LPEN	0x08	/* Strobe 0 to clear lightpen latch */
#define CTL_SET_LPEN	0x04	/* Strobe 0 to set lightpen latch */
#define CTL_PALETTE	0x01	/* 0x2DE writes to palette (1) or plane (0) */

/*
 * The card BIOS seems to support two variants of the hardware: One where 
 * bits 2 and 3 are normally 1 and are set to 0 to set/clear the latch, and
 * one where they are normally 0 and are set to 1. Behaviour is selected by
 * whether the byte at C000:17FFh is greater than 2Fh.
 *
 * 0x2DA: Status register.
 */
#define STATUS_CURSOR	0x80	/* Last value written to bit 7 of 0x2D9 */
#define STATUS_NMI	0x20	/* Last value written to bit 5 of 0x2D9 */
#define STATUS_RETR_V	0x10	/* Vertical retrace */
#define STATUS_LPEN_T	0x04	/* Lightpen switch is off */
#define STATUS_LPEN_A	0x02	/* Edge from lightpen has set trigger */
#define STATUS_RETR_H	0x01	/* Horizontal retrace */

/*
 * 0x2DB: On read: Byte written to the card that triggered NMI 
 * 0x2DB: On write: Resets the 'card raised NMI' flag.
 * 0x2DC: On read: Bit 7 set if the card raised NMI. If so, bits 0-3 
 *                 give the low 4 bits of the I/O port address.
 * 0x2DC: On write: Resets the NMI.
 * 0x2DD: Memory paging. The memory from 0xC1800 to 0xC1FFF can be either:
 *
 *	> ROM: A 128 character 8x16 font for use in graphics modes
 *	> RAM: Use by the video BIOS to hold its settings.
 *
 * Reading port 2DD switches to ROM. Bit 7 of the value read gives the
 * previous paging state: bit 7 set if ROM was paged, clear if RAM was
 * paged.
 *
 * Writing port 2DD switches to RAM.
 *
 * 0x2DE: Meaning depends on bottom bit of value written to port 0x2D9.
 *        Bit 0 set: Write to palette. High 4 bits of value = register, 
 *                   low 4 bits = RGBI values (active low)
 *        Bit 0 clear: Write to plane select. Low 2 bits of value select
 *                    plane 0-3 
 */
 

#define COMPOSITE_OLD 0
#define COMPOSITE_NEW 1


typedef struct {
    const char	*name;

    mem_map_t	mapping,
		bios_ram;

    rom_t	bios_rom; 

    uint8_t	crtc[32];	/* CRTC: Real values */

    uint8_t	lastport;	/* Last I/O port written */
    uint8_t	lastwrite;	/* Value written to that port */
    uint8_t	sigma_ctl;	/* Controls register:
				 * Bit 7 is low bit of cursor position 
				 * Bit 5 set if writes to CGA ports trigger NMI
				 * Bit 3 clears lightpen latch
				 * Bit 2 sets lightpen latch
				 * Bit 1 controls meaning of port 2DE 
				 */
    uint8_t	enable_nmi;	/* Enable the NMI mechanism for CGA emulation?*/
    uint8_t	rom_paged;	/* Is ROM paged in at 0xC1800? */

    uint8_t	crtc_value;	/* Value to return from a CRTC register read */

    uint8_t	sigmastat;	/* Status register [0x2DA] */

    uint8_t	sigmamode;	/* Mode control register [0x2D8] */

    uint16_t	ma, maback;

    int		crtcreg;	/* CRTC: Real selected register */        

    int		linepos, displine;
    int		sc, vc;
    int		cgadispon;
    int		con, coff, cursoron, cgablink;
    int		vsynctime, vadj;
    int		oddeven;

    int64_t	dispontime, dispofftime;

    int		firstline, lastline;

    int		drawcursor;

    int		plane;
    int		revision;

    int64_t	vidtime;

    uint8_t	*vram;
    uint8_t	fontdat[256][8];		/* 8x8 font */
    uint8_t	fontdat16[256][16];		/* 8x16 font */
    uint8_t	bram[2048];       
    uint8_t	palette[16];
} sigma_t;


static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const video_timings_t sigma_timing = {VID_ISA,8,16,32,8,16,32};


static void
recalc_timings(sigma_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    if (dev->sigmamode & MODE_80COLS) {
	disptime = (dev->crtc[0] + 1) << 1;
	_dispontime = (dev->crtc[1]) << 1;
    } else {
	disptime = (dev->crtc[0] + 1) << 2;
	_dispontime = dev->crtc[1] << 2;
    }

    _dispofftime = disptime - _dispontime;
    _dispontime *= CGACONST;
    _dispofftime *= CGACONST;

    dev->dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
    dev->dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
sigma_out(uint16_t port, uint8_t val, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;
    uint8_t old;

    if (port >= 0x3d0 && port < 0x3e0) {
	dev->lastport = port & 0x0f;
	dev->lastwrite = val;

	/* If set to NMI on video I/O... */
	if (dev->enable_nmi && (dev->sigma_ctl & CTL_NMI)) {
		dev->lastport |= 0x80; 	/* Card raised NMI */
		nmi = 1;
	}

	/* For CRTC emulation, the card BIOS sets the value to be
	 * read from port 0x3D1 like this */
	if (port == 0x3d1)
		dev->crtc_value = val;

	return;
    }

    switch (port) {
	case 0x2d0:
	case 0x2d2:
	case 0x2d4:
	case 0x2d6:
		dev->crtcreg = val & 31;
		break;

	case 0x2d1:
	case 0x2d3:
	case 0x2d5:
	case 0x2d7:
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val & crtcmask[dev->crtcreg];
		if (old != val) {
			if (dev->crtcreg < 0xe || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				recalc_timings(dev);
			} 
		}
		break;

	case 0x2d8:
		dev->sigmamode = val;
		break;

	case 0x2d9:
		dev->sigma_ctl = val;
		break;

	case 0x2db:
		dev->lastport &= 0x7F;
		break;

	case 0x2dc:	/* Reset NMI */
		nmi = 0;
		dev->lastport &= 0x7F;
		break;

	case 0x2dd:	/* Page in RAM at 0xC1800 */
		if (dev->rom_paged != 0)
			mmu_invalidate(0xc0000);
		dev->rom_paged = 0;
		break;

	case 0x2de:
		if (dev->sigma_ctl & CTL_PALETTE)
			dev->palette[val >> 4] = (val & 0x0f) ^ 0x0f;
		else
			dev->plane = val & 3;
		break;
    }
}


static uint8_t
sigma_in(uint16_t port, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x2d0:
	case 0x2d2:
	case 0x2d4:
	case 0x2d6:
		ret = dev->crtcreg;
		break;

	case 0x2d1:
	case 0x2d3:
	case 0x2d5:
	case 0x2d7:
		ret = dev->crtc[dev->crtcreg & 0x1f];
		break;

	case 0x2da:
		ret = (dev->sigma_ctl & 0xe0) | (dev->sigmastat & 0x1f);
		break;

	case 0x2db: 
		ret = dev->lastwrite;		/* Value that triggered NMI */
		break;

	case 0x2dc: 
		ret = dev->lastport;		/* Port that triggered NMI */
		break;

	case 0x2dd:				/* Page in ROM at 0xC1800 */
		ret = (dev->rom_paged ? 0x80 : 0);
		if (dev->rom_paged != 0x80)
			mmu_invalidate(0xc0000);
		dev->rom_paged = 0x80;
		break;

	case 0x3d1:
	case 0x3d3:
	case 0x3d5:
	case 0x3d7:
		ret = dev->crtc_value;
		break;

	/*
	 * For CGA compatibility we have to return something palatable
	 * on this port. On a real card this functionality can be turned
	 * on or off with SW1/6.
	 */
	case 0x3da:
		ret = dev->sigmastat & 7;
		if (dev->sigmastat & STATUS_RETR_V) ret |= 8;
		break;
    }

    return ret;
}


static void
sigma_write(uint32_t addr, uint8_t val, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;

    cycles -= 4;

    dev->vram[dev->plane * 0x8000 + (addr & 0x7fff)] = val;
}


static uint8_t
sigma_read(uint32_t addr, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;

    cycles -= 4;        

    return dev->vram[dev->plane * 0x8000 + (addr & 0x7fff)];
}


static void
sigma_bwrite(uint32_t addr, uint8_t val, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;

    addr &= 0x3fff;
#if 0
    if ((addr >= 0x1800) && !dev->rom_paged && (addr < 0x2000))
#endif
    if ((addr < 0x1800) || dev->rom_paged || (addr >= 0x2000))
	;
    else
	dev->bram[addr & 0x7ff] = val;
}


static uint8_t
sigma_bread(uint32_t addr, void *priv)
{
    sigma_t *dev = (sigma_t *)priv;
    uint8_t ret;

    addr &= 0x3fff;
    if (addr >= 0x2000)
	return 0xff;

    if (addr < 0x1800 || dev->rom_paged)
	ret = dev->bios_rom.rom[addr & 0x1fff];
    else
	ret = dev->bram[addr & 0x7ff];

    return ret;
}


/* Render a line in 80-column text mode */
static void
sigma_text80(sigma_t *dev)
{
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8));
    uint16_t ma = ((dev->ma & 0x3fff) << 1);
    uint8_t *vram = dev->vram + (ma << 1);
    int drawcursor, x, c;
    uint8_t chr, attr;
    uint32_t cols[4];

    ca = ca << 1;
    if (dev->sigma_ctl & CTL_CURSOR)
	ca++;
    ca &= 0x3fff;

    /* The Sigma 400 seems to use screen widths stated in words
       (40 for 80-column, 20 for 40-column) */
    for (x = 0; x < (dev->crtc[1] << 1); x++) {
	chr  = vram[x << 1];
	attr = vram[(x << 1) + 1];
	drawcursor = ((ma == ca) && dev->con && dev->cursoron);

	if (! (dev->sigmamode & MODE_NOBLINK)) {
		cols[1] = (attr & 15) | 16;
		cols[0] = ((attr >> 4) & 7) | 16;
		if ((dev->cgablink & 8) && (attr & 0x80) && !dev->drawcursor) 
			cols[1] = cols[0];
	} else {	/* No blink */
		cols[1] = (attr & 15) | 16;
		cols[0] = (attr >> 4) | 16;
	}

	if (drawcursor) {
		for (c = 0; c < 8; c++) {
			if (dev->sigmamode & MODE_FONT16)
				buffer->line[dev->displine][(x << 3) + c + 8] = cols[(dev->fontdat16[chr][dev->sc & 15] & (1 << (c ^ 7))) ? 1 : 0] ^ 0x0f;
			else
				buffer->line[dev->displine][(x << 3) + c + 8] = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0] ^ 0x0f;
		}
	} else {
		for (c = 0; c < 8; c++) {
			if (dev->sigmamode & MODE_FONT16)
				buffer->line[dev->displine][(x << 3) + c + 8] = cols[(dev->fontdat16[chr][dev->sc & 15] & (1 << (c ^ 7))) ? 1 : 0];
			else
				buffer->line[dev->displine][(x << 3) + c + 8] = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
		}
	}
	++ma;
    }

    dev->ma += dev->crtc[1];
}


/* Render a line in 40-column text mode */
static void
sigma_text40(sigma_t *dev)
{
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8));
    uint16_t ma = ((dev->ma & 0x3FFF) << 1);
    uint8_t *vram = dev->vram + ((ma << 1) & 0x3FFF);
    int drawcursor, x, c;
    uint8_t chr, attr;
    uint32_t cols[4];

    ca = ca << 1;
    if (dev->sigma_ctl & CTL_CURSOR)
	++ca;
    ca &= 0x3fff;

    /* The Sigma 400 seems to use screen widths stated in words
       (40 for 80-column, 20 for 40-column) */
    for (x = 0; x < (dev->crtc[1] << 1); x++) {
	chr  = vram[x << 1];
	attr = vram[(x << 1) + 1];
	drawcursor = ((ma == ca) && dev->con && dev->cursoron);

	if (!(dev->sigmamode & MODE_NOBLINK)) {
		cols[1] = (attr & 15) | 16;
		cols[0] = ((attr >> 4) & 7) | 16;
		if ((dev->cgablink & 8) && (attr & 0x80) && !dev->drawcursor) 
			cols[1] = cols[0];
	} else {	/* No blink */
		cols[1] = (attr & 15) | 16;
		cols[0] = (attr >> 4) | 16;
	}

	if (drawcursor) {
		for (c = 0; c < 8; c++) { 
			buffer->line[dev->displine][(x << 4) + 2*c + 8] = 
			buffer->line[dev->displine][(x << 4) + 2*c + 9] = cols[(dev->fontdat16[chr][dev->sc & 15] & (1 << (c ^ 7))) ? 1 : 0] ^ 0x0f;
		}
	} else {
		for (c = 0; c < 8; c++) {
			buffer->line[dev->displine][(x << 4) + 2*c + 8] = 
			buffer->line[dev->displine][(x << 4) + 2*c + 9] = cols[(dev->fontdat16[chr][dev->sc & 15] & (1 << (c ^ 7))) ? 1 : 0];
		}	
	}
	ma++;
    }

    dev->ma += dev->crtc[1];
}


/* Draw a line in the 640x400 graphics mode */
static void
sigma_gfx400(sigma_t *dev)
{
    uint8_t *vram = &dev->vram[((dev->ma << 1) & 0x1fff) + (dev->sc & 3) * 0x2000];
    uint8_t mask, col, c;	
    uint8_t plane[4];
    int x;

    for (x = 0; x < (dev->crtc[1] << 1); x++) {
	plane[0] = vram[x];	
	plane[1] = vram[0x8000 + x];	
	plane[2] = vram[0x10000 + x];	
	plane[3] = vram[0x18000 + x];	

	for (c = 0, mask = 0x80; c < 8; c++, mask >>= 1) {
		col = ((plane[3] & mask) ? 8 : 0) | 
		      ((plane[2] & mask) ? 4 : 0) | 
		      ((plane[1] & mask) ? 2 : 0) | 
		      ((plane[0] & mask) ? 1 : 0);
		col |= 16;
		buffer->line[dev->displine][(x << 3) + c + 8] = col;
	}

	if (x & 1) dev->ma++;
    }
}


/* Draw a line in the 640x200 graphics mode.
 * This is actually a 640x200x16 mode; on startup, the BIOS selects plane 2,
 * blanks the other planes, and sets palette ink 4 to white. Pixels plotted
 * in plane 2 come out in white, others black; but by programming the palette
 * and plane registers manually you can get the full resolution. */
static void
sigma_gfx200(sigma_t *dev)
{
    uint8_t *vram = &dev->vram[((dev->ma << 1) & 0x1fff) + (dev->sc & 2) * 0x1000];
    uint8_t mask, col, c;	
    uint8_t plane[4];
    int x;

    for (x = 0; x < (dev->crtc[1] << 1); x++) {
	plane[0] = vram[x];	
	plane[1] = vram[0x8000 + x];	
	plane[2] = vram[0x10000 + x];	
	plane[3] = vram[0x18000 + x];	

	for (c = 0, mask = 0x80; c < 8; c++, mask >>= 1) {
		col = ((plane[3] & mask) ? 8 : 0) | 
		      ((plane[2] & mask) ? 4 : 0) | 
		      ((plane[1] & mask) ? 2 : 0) | 
		      ((plane[0] & mask) ? 1 : 0);
		col |= 16;
		buffer->line[dev->displine][(x << 3) + c + 8] = col;
	}

	if (x & 1) dev->ma++;
    }
}


/* Draw a line in the 320x200 graphics mode */
static void
sigma_gfx4col(sigma_t *dev)
{
    uint8_t *vram = &dev->vram[((dev->ma << 1) & 0x1fff) + (dev->sc & 2) * 0x1000];
    uint8_t mask, col, c;	
    uint8_t plane[4];
    int x;

    for (x = 0; x < (dev->crtc[1] << 1); x++) {
	plane[0] = vram[x];	
	plane[1] = vram[0x8000 + x];	
	plane[2] = vram[0x10000 + x];	
	plane[3] = vram[0x18000 + x];	

	mask = 0x80;
	for (c = 0; c < 4; c++) {
		col = ((plane[3] & mask) ? 2 : 0) | 
		      ((plane[2] & mask) ? 1 : 0);
		mask = mask >> 1;
		col |= ((plane[3] & mask) ? 8 : 0) | 
		       ((plane[2] & mask) ? 4 : 0);
		col |= 16;
		mask = mask >> 1;

		buffer->line[dev->displine][(x << 3) + (c << 1) + 8] = 
		buffer->line[dev->displine][(x << 3) + (c << 1) + 9] = col;
	}

	if (x & 1) dev->ma++;
    }
}


static void
sigma_poll(void *priv)
{
    sigma_t *dev = (sigma_t *)priv;
    uint32_t cols[4];
    int oldsc, oldvc;
    int x, c;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->sigmastat |= STATUS_RETR_H;
	dev->linepos = 1;
	oldsc = dev->sc;
	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = ((dev->sc << 1) + dev->oddeven) & 7;
	if (dev->cgadispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_wait_for_buffer();
		}
		dev->lastline = dev->displine;

		cols[0] = 16; 

		/* Left overscan */
		for (c = 0; c < 8; c++) {
			buffer->line[dev->displine][c] = cols[0];
			if (dev->sigmamode & MODE_80COLS)
				buffer->line[dev->displine][c + (dev->crtc[1] << 4) + 8] = cols[0];
			else
				buffer->line[dev->displine][c + (dev->crtc[1] << 5) + 8] = cols[0];
		}

		if (dev->sigmamode & MODE_GRAPHICS) {
			if (dev->sigmamode & MODE_640x400)
				sigma_gfx400(dev);
			else if (dev->sigmamode & MODE_HRGFX)
				sigma_gfx200(dev);
			else
				sigma_gfx4col(dev);
		} else {	/* Text modes */
			if (dev->sigmamode & MODE_80COLS)
				sigma_text80(dev);
			else
				sigma_text40(dev);
		}
	} else {
		cols[0] = 16;
		if (dev->sigmamode & MODE_80COLS) 
			cga_hline(buffer, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
		else
			cga_hline(buffer, 0, dev->displine, (dev->crtc[1] << 5) + 16, cols[0]);
	}

	if (dev->sigmamode & MODE_80COLS) 
		x = (dev->crtc[1] << 4) + 16;
	else
		x = (dev->crtc[1] << 5) + 16;

	for (c = 0; c < x; c++)
		buffer->line[dev->displine][c] = dev->palette[buffer->line[dev->displine][c] & 0xf] | 16;

	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->sigmastat |= STATUS_RETR_V;
	dev->displine++;
	if (dev->displine >= 560) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (!dev->vsynctime)
			dev->sigmastat &= ~STATUS_RETR_V;
	}
	if (dev->sc == (dev->crtc[11] & 31) ||
	    ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
		dev->con = 0; 
		dev->coff = 1; 
	}
	if ((dev->crtc[8] & 3) == 3 && dev->sc == (dev->crtc[9] >> 1))
		dev->maback = dev->ma;
	if (dev->vadj) {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
		dev->vadj--;
		if (!dev->vadj) {
			dev->cgadispon = 1;
			dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			dev->sc = 0;
		}
	} else if (dev->sc == dev->crtc[9]) {
		dev->maback = dev->ma;
		dev->sc = 0;
		oldvc = dev->vc;
		dev->vc++;
		dev->vc &= 127;

		if (dev->vc == dev->crtc[6]) 
			dev->cgadispon = 0;

		if (oldvc == dev->crtc[4]) {
			dev->vc = 0;
			dev->vadj = dev->crtc[5];
			if (!dev->vadj) dev->cgadispon = 1;
			if (!dev->vadj) dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			else
				dev->cursoron = dev->cgablink & 8;
		}

		if (dev->vc == dev->crtc[7]) {
			dev->cgadispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;
			if (dev->crtc[7]) {
				if (dev->sigmamode & MODE_80COLS) 
					x = (dev->crtc[1] << 4) + 16;
				else
					x = (dev->crtc[1] << 5) + 16;
				dev->lastline++;
				if ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) ||
				    video_force_resize_get()) {
					xsize = x;
					ysize = dev->lastline - dev->firstline;
					if (xsize < 64)
						xsize = 656;
					if (ysize < 32)
						ysize = 200;

					if (ysize <= 250)
						set_screen_size(xsize, (ysize << 1) + 16);
					else
						set_screen_size(xsize, ysize + 8);

					if (video_force_resize_get())
						video_force_resize_set(0);
				}

				video_blit_memtoscreen_8(0, dev->firstline - 4, 0, (dev->lastline - dev->firstline) + 8, xsize, (dev->lastline - dev->firstline) + 8);
				frames++;

				video_res_x = xsize - 16;
				video_res_y = ysize;
				if (dev->sigmamode & MODE_GRAPHICS) {
					if (dev->sigmamode & (MODE_HRGFX | MODE_640x400))
						video_bpp = 1;
					else {
						video_res_x /= 2;
						video_bpp = 2;
					}
				} else if (dev->sigmamode & MODE_80COLS) {
					/* 80-column text */
					video_res_x /= 8;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				} else {
					/* 40-column text */
					video_res_x /= 16;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				}
			}
			dev->firstline = 1000;
			dev->lastline = 0;
			dev->cgablink++;
			dev->oddeven ^= 1;
		}
	} else {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
	}
	if (dev->cgadispon)
		dev->sigmastat &= ~STATUS_RETR_H;
	if ((dev->sc == (dev->crtc[10] & 31) ||
	    ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1)))) 
		dev->con = 1;
    }
}


static int
load_font(sigma_t *dev, const wchar_t *s)
{
    FILE *fp;
    int c;

    fp = plat_fopen(rom_path(s), L"rb");
    if (fp == NULL) {
	ERRLOG("%s: cannot load font '%ls'\n", dev->name, s);
	return(0);
    }

    /* The first 4K of the character ROM holds an 8x8 font. */
    for (c = 0; c < 256; c++) {
	(void)fread(&dev->fontdat[c][0], 1, 8, fp);
	(void)fseek(fp, 8, SEEK_CUR);
    }

    /* The second 4K holds an 8x16 font. */
    for (c = 0; c < 256; c++)
	(void)fread(&dev->fontdat16[c][0], 1, 16, fp);

    (void)fclose(fp);

    return(1);
}


static void *
sigma_init(const device_t *info)
{
    sigma_t *dev;

    dev = (sigma_t *)mem_alloc(sizeof(sigma_t));
    memset(dev, 0x00, sizeof(sigma_t));
    dev->name = info->name;

    if (! load_font(dev, FONT_ROM_PATH)) {
	free(dev);
	return(NULL);
    }

    dev->enable_nmi = device_get_config_int("enable_nmi");

    cga_palette = device_get_config_int("rgb_type") << 1;
    if (cga_palette > 6)
	cga_palette = 0;
    cgapal_rebuild();

    dev->vram = malloc(0x8000 * 4);

    mem_map_add(&dev->mapping, 0xb8000, 0x08000, 
		sigma_read,NULL,NULL, sigma_write,NULL,NULL,  
		NULL, MEM_MAPPING_EXTERNAL, dev);

    rom_init(&dev->bios_rom, BIOS_ROM_PATH,
	     0xc0000, 0x2000, 0x1fff, 0, MEM_MAPPING_EXTERNAL);

    /*
     * The BIOS ROM is overlaid by RAM, so remove its default
     * mapping and access it through sigma_bread/sigma_bwrite.
     */
    mem_map_disable(&dev->bios_rom.mapping);
    memcpy(dev->bram, &dev->bios_rom.rom[0x1800], 0x0800);

    mem_map_add(&dev->bios_ram, 0xc1800, 0x0800,
		sigma_bread,NULL,NULL, sigma_bwrite,NULL,NULL,  
		dev->bios_rom.rom, MEM_MAPPING_EXTERNAL, dev);

    io_sethandler(0x03d0, 16, 
		  sigma_in,NULL,NULL, sigma_out,NULL,NULL, dev);
    io_sethandler(0x02d0, 16, 
		  sigma_in,NULL,NULL, sigma_out,NULL,NULL, dev);

    timer_add(sigma_poll, &dev->vidtime, TIMER_ALWAYS_ENABLED, dev);

    /* Start with ROM paged in, BIOS RAM paged out */
    dev->rom_paged = 0x80;

    if (dev->enable_nmi)
	dev->sigmastat = STATUS_LPEN_T;

    video_inform(VID_TYPE_CGA, &sigma_timing);

    return dev;
}


static void
sigma_close(void *priv)
{
    sigma_t *dev = (sigma_t *)priv;

    free(dev->vram);

    free(dev);
}


void
speed_changed(void *priv)
{
    sigma_t *dev = (sigma_t *)priv;

    recalc_timings(dev);
}


static int
sigma_available(void)
{
    return((rom_present(FONT_ROM_PATH) && rom_present(BIOS_ROM_PATH)));
}


static const device_config_t sigma_config[] = {
    {
	"rgb_type", "RGB type", CONFIG_SELECTION, "", 0,
	{
		{
			"Color", 0
		},
		{
			"Green Monochrome", 1
		},
		{
			"Amber Monochrome", 2
		},
		{
			"Gray Monochrome", 3
		},
		{
			"Color (no brown)", 4
		},
		{
			""
		}
	}
    },
    {
	"enable_nmi", "Enable NMI for CGA emulation", CONFIG_BINARY, "", 1
    },
    {
	"", "", -1
    }
};


const device_t sigma_device = {
    "Sigma Color 400",
    DEVICE_ISA,
    0,
    sigma_init, sigma_close, NULL,
    sigma_available,
    speed_changed,
    NULL,
    NULL,
    sigma_config
};
