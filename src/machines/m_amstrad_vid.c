/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Amstrad video controllers.
 *
 * PC1512:	The PC1512 extends CGA with a bit-planar 640x200x16 mode.
 *		Most CRTC registers are fixed.
 *
 *		The Technical Reference Manual lists the video waitstate
 *		time as between 12 and 46 cycles. We currently always use
 *		the lower number.
 *
 * PC1640:	Mostly standard EGA, but with CGA & Hercules emulation.
 *
 * PC200:	CGA with some NMI stuff.
 *
 * **NOTES**	This module is not complete yet:
 *  PC1612:	IGA not complete; Hercules and InColor modes not implemented.
 *  PC200:	TV mode does not seem to work; results in CGA being selected
 *		 by the ROS.
 *  PPC:	MDA Monitor results in half-screen, half-cell-height display??
 *
 * Version:	@(#)m_amstrad_vid.c	1.0.7	2020/02/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		John Elliott, <jce@seasip.info>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2017-2019 John Elliott.
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
#define dbglog video_card_log
#include "../emu.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/system/clk.h"
#include "../devices/system/nmi.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../devices/video/vid_mda.h"
#include "../devices/video/vid_ega.h"
#include "../plat.h"
#include "machine.h"
#include "m_amstrad.h"


typedef struct {
    int		type;

    rom_t	bios_rom;		/* 1640 */

    cga_t	cga;			/* 1640/200 */
    mem_map_t	cga_mapping;

    mda_t	mda;
    mem_map_t	mda_mapping;

    ega_t	ega;			/* 1640 */

    uint8_t	crtc[32];		/* 1512 */
    uint8_t	crtcreg;		/* 1512 */

    uint8_t	cga_enabled;		/* 1640 */

    int		fontbase;		/* 1512/IDA */

    uint8_t	ida_enabled;		/* IDA */
    uint8_t	ida_mode;		/* Which display are we emulating? */
    uint8_t	switches;		/* DIP switches 1-3 */
    uint8_t	crtc_index;		/* CRTC index readback
					 *  bit7: CGA control port written
					 *  bit6: Operation control port written
					 *  bit5: CRTC register written 
					 *  Bits0-4: Last CRTC reg selected
					 */

    uint8_t	opctrl;			/* 200 */
    uint8_t	reg_3df;

    uint8_t	cgacol,
		cgamode,
		stat;
    uint8_t	plane_read,		/* 1512 */
		plane_write,		/* 1512 */
		border;			/* 1512/200 */
    int		linepos,
		displine;
    int		sc, vc;
    int		cgadispon;
    int		con, coff,
		cursoron,
		cgablink;
    int		vadj;
    uint16_t	ma, maback;
    int		dispon;
    int		blink;

    tmrval_t	vsynctime;
    tmrval_t	dispontime,		/* 1512/1640 */
		dispofftime;		/* 1512/1640 */
    tmrval_t	vidtime;		/* 1512/1640 */

    int		firstline,
		lastline;
    uint8_t	*vram;

    uint32_t	blue, green;		/* IDA for LCD */
    uint32_t	lcdcols[256][2][2];

    /* Note that IDA mda.c/cga.c code also needs fonts.. */
    uint8_t	fontdat[2048][8];	/* 1512/IDA, 2048 characters */
    uint8_t	fontdatm[2048][16];	/* 1512/IDA, 2048 characters */
} vid_t;


static const uint8_t mapping1[256] = {
    /*		0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
    /*00*/	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*10*/	2, 0, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1,
    /*20*/	2, 2, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1,
    /*30*/	2, 2, 2, 0, 2, 2, 1, 1, 2, 2, 2, 1, 2, 2, 1, 1,
    /*40*/	2, 2, 1, 1, 0, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1,
    /*50*/	2, 2, 1, 1, 2, 0, 1, 1, 2, 2, 1, 1, 2, 1, 1, 1,
    /*60*/	2, 2, 2, 2, 2, 2, 0, 1, 2, 2, 2, 2, 2, 2, 1, 1,
    /*70*/	2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 1,
    /*80*/	2, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    /*90*/	2, 2, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 1, 1, 1, 1,
    /*a0*/	2, 2, 2, 1, 2, 2, 1, 1, 2, 2, 0, 1, 2, 2, 1, 1,
    /*b0*/	2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 0, 2, 2, 1, 1,
    /*c0*/	2, 2, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1, 0, 1, 1, 1,
    /*d0*/	2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 0, 1, 1,
    /*e0*/	2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 0, 1,
    /*f0*/	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0
};
static const uint8_t mapping2[256] = {
    /*		0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
    /*00*/	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*10*/	1, 3, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2,
    /*20*/	1, 1, 3, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2,
    /*30*/	1, 1, 1, 3, 1, 1, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
    /*40*/	1, 1, 2, 2, 3, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2,
    /*50*/	1, 1, 2, 2, 1, 3, 2, 2, 1, 1, 2, 2, 1, 2, 2, 2,
    /*60*/	1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 1, 1, 1, 1, 2, 2,
    /*70*/	1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 2,
    /*80*/	2, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    /*90*/	1, 1, 2, 2, 2, 2, 2, 2, 1, 3, 2, 2, 2, 2, 2, 2,
    /*a0*/	1, 1, 1, 2, 1, 1, 2, 2, 1, 1, 3, 2, 1, 1, 2, 2,
    /*b0*/	1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 3, 1, 1, 2, 2,
    /*c0*/	1, 1, 2, 2, 1, 2, 2, 2, 1, 1, 2, 2, 3, 2, 2, 2,
    /*d0*/	1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 3, 2, 2,
    /*e0*/	1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 2,
    /*f0*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3
};
static const uint8_t crtc_mask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const video_timings_t pc1512_timing = {VID_BUS,0,0,0,0,0,0};
static const video_timings_t pc1640_timing = {VID_ISA,8,16,32,8,16,32};
static const video_timings_t ida_timing = {VID_ISA,8,16,32,8,16,32};


static void
pc1512_recalc_timings(vid_t *dev)
{
    double _dispontime, _dispofftime, disptime;

    disptime = 114; /*Fixed on PC1512*/

    _dispontime = 80;
    _dispofftime = disptime - _dispontime;
    _dispontime  *= CGACONST;
    _dispofftime *= CGACONST;

    dev->dispontime  = (tmrval_t)(_dispontime * (1 << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
pc1512_out(uint16_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t old;

    switch (addr) {
	case 0x03d4:
		dev->crtcreg = val & 31;
		return;

	case 0x03d5:
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val & crtc_mask[dev->crtcreg];
		if (old != val) {
			if (dev->crtcreg < 0xe || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				pc1512_recalc_timings(dev);
			}
		}
		return;

	case 0x03d8:
		if ((val & 0x12) == 0x12 && (dev->cgamode & 0x12) != 0x12) {
			dev->plane_write = 0xf;
			dev->plane_read  = 0;
		}
		dev->cgamode = val;
		return;

	case 0x03d9:
		dev->cgacol = val;
		return;

	case 0x03dd:
		dev->plane_write = val;
		return;

	case 0x03de:
		dev->plane_read = val & 3;
		return;

	case 0x03df:
		dev->border = val;
		return;
    }
}


static uint8_t
pc1512_in(uint16_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0x03d4:
		ret = dev->crtcreg;

	case 0x03d5:
		ret = dev->crtc[dev->crtcreg];
		break;

	case 0x03da:
		ret = dev->stat;
		break;
    }

    return(ret);
}


static void
pc1512_write(uint32_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    cycles -= 12;
    addr &= 0x3fff;

    if ((dev->cgamode & 0x12) == 0x12) {
	if (dev->plane_write & 0x01)
		dev->vram[addr] = val;
	if (dev->plane_write & 0x02)
		dev->vram[addr | 0x4000] = val;
	if (dev->plane_write & 0x04)
		dev->vram[addr | 0x8000] = val;
	if (dev->plane_write & 0x08)
		dev->vram[addr | 0xc000] = val;
    } else
	dev->vram[addr] = val;
}


static uint8_t
pc1512_read(uint32_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    cycles -= 12;
    addr &= 0x3fff;

    if ((dev->cgamode & 0x12) == 0x12)
	return(dev->vram[addr | (dev->plane_read << 14)]);

    return(dev->vram[addr]);
}


static void
pc1512_poll(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    uint8_t chr, attr;
    uint16_t dat, dat2, dat3, dat4;
    int cols[4];
    int col;
    int oldsc;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;
	if (dev->dispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;

		for (c = 0; c < 8; c++) {
			if ((dev->cgamode & 0x12) == 0x12) {
				screen->line[dev->displine][c].pal = (dev->border & 15) + 16;
				if (dev->cgamode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = 0;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = 0;
			} else {
				screen->line[dev->displine][c].pal = (dev->cgacol & 15) + 16;
				if (dev->cgamode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = (dev->cgacol & 15) + 16;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = (dev->cgacol & 15) + 16;
			}
		}
		if (dev->cgamode & 1) {
			for (x = 0; x < 80; x++) {
				chr  = dev->vram[ ((dev->ma << 1) & 0x3fff)];
				attr = dev->vram[(((dev->ma << 1) + 1) & 0x3fff)];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->cgamode & 0x20) {
					cols[1] = (attr & 15) + 16;
					cols[0] = ((attr >> 4) & 7) + 16;
					if ((dev->blink & 16) && (attr & 0x80) && !drawcursor) 
						cols[1] = cols[0];
				} else {
					cols[1] = (attr & 15) + 16;
					cols[0] = (attr >> 4) + 16;
				}
				if (drawcursor) {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(dev->fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(dev->fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
				dev->ma++;
			}
		} else if (! (dev->cgamode & 2)) {
			for (x = 0; x < 40; x++) {
				chr = dev->vram[((dev->ma << 1) & 0x3fff)];
				attr = dev->vram[(((dev->ma << 1) + 1) & 0x3fff)];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->cgamode & 0x20) {
					cols[1] = (attr & 15) + 16;
					cols[0] = ((attr >> 4) & 7) + 16;
					if ((dev->blink & 16) && (attr & 0x80)) 
						cols[1] = cols[0];
				} else {
					cols[1] = (attr & 15) + 16;
					cols[0] = (attr >> 4) + 16;
				}
				dev->ma++;
				if (drawcursor) {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
					    	screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(dev->fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
					    	screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(dev->fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
			}
		} else if (! (dev->cgamode & 16)) {
			cols[0] = (dev->cgacol & 15) | 16;
			col = (dev->cgacol & 16) ? 24 : 16;
			if (dev->cgamode & 4) {
				cols[1] = col | 3;
				cols[2] = col | 4;
				cols[3] = col | 7;
			} else if (dev->cgacol & 32) {
				cols[1] = col | 3;
				cols[2] = col | 5;
				cols[3] = col | 7;
			} else {
				cols[1] = col | 2;
				cols[2] = col | 4;
				cols[3] = col | 6;
			}
			for (x = 0; x < 40; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal =
						screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[dat >> 14];
					dat <<= 2;
				}
			}
		} else {
			for (x = 0; x < 40; x++) {
				ca = ((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000);
				dat  = (dev->vram[ca] << 8) | dev->vram[ca + 1];
				dat2 = (dev->vram[ca + 0x4000] << 8) | dev->vram[ca + 0x4001];
				dat3 = (dev->vram[ca + 0x8000] << 8) | dev->vram[ca + 0x8001];
				dat4 = (dev->vram[ca + 0xc000] << 8) | dev->vram[ca + 0xc001];

				dev->ma++;
				for (c = 0; c < 16; c++) {
					screen->line[dev->displine][(x << 4) + c + 8].pal = (((dat >> 15) | ((dat2 >> 15) << 1) | ((dat3 >> 15) << 2) | ((dat4 >> 15) << 3)) & (dev->cgacol & 15)) + 16;
					dat  <<= 1;
					dat2 <<= 1;
					dat3 <<= 1;
					dat4 <<= 1;
				}
			}
		}
	} else {
		cols[0] = ((dev->cgamode & 0x12) == 0x12) ? 0 : (dev->cgacol & 15) + 16;
		if (dev->cgamode & 1)
			cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, cols[0]);
		else
			cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
	}

	dev->sc = oldsc;
	if (dev->vsynctime)
	   dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 360) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if ((dev->lastline - dev->firstline) == 199) 
		dev->dispon = 0; /*Amstrad PC1512 always displays 200 lines, regardless of CRTC settings*/
	if (dev->dispon) 
		dev->stat &= ~1;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->stat &= ~8;
	}
	if (dev->sc == (dev->crtc[11] & 31)) { 
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
	} else if (dev->sc == dev->crtc[9]) {
		dev->maback = dev->ma;
		dev->sc = 0;
		dev->vc++;
		dev->vc &= 127;

		if (dev->displine == 32) {
			dev->vc = 0;
			dev->vadj = 6;
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			  else
				dev->cursoron = dev->blink & 16;
		}

		if (dev->displine >= 262) {
			dev->dispon = 0;
			dev->displine = 0;
			dev->vsynctime = 46;

			if (dev->cgamode&1)
				x = (dev->crtc[1] << 3) + 16;
			  else
				x = (dev->crtc[1] << 4) + 16;
			dev->lastline++;

			if ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) || video_force_resize_get()) {
				xsize = x;
				ysize = dev->lastline - dev->firstline;
				if (xsize < 64) xsize = 656;
				if (ysize < 32) ysize = 200;
				set_screen_size(xsize, (ysize << 1) + 16);

				if (video_force_resize_get())
					video_force_resize_set(0);
			}

			video_blit_start(1, 0, dev->firstline - 4, 0, (dev->lastline - dev->firstline) + 8, xsize, (dev->lastline - dev->firstline) + 8);

			video_res_x = xsize - 16;
			video_res_y = ysize;
			if (dev->cgamode & 1) {
				video_res_x /= 8;
				video_res_y /= dev->crtc[9] + 1;
				video_bpp = 0;
			} else if (! (dev->cgamode & 2)) {
				video_res_x /= 16;
				video_res_y /= dev->crtc[9] + 1;
				video_bpp = 0;
			} else if (! (dev->cgamode & 16)) {
				video_res_x /= 2;
				video_bpp = 2;
			} else {
				video_bpp = 4;
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
	if (dev->sc == (dev->crtc[10] & 31))
		dev->con = 1;
    }
}


static void
pc1512_close(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
pc1512_speed_change(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    pc1512_recalc_timings(dev);
}


static const device_t pc1512_video_device = {
    "Amstrad PC1512 Video",
    0, 0, NULL,
    NULL, pc1512_close, NULL,
    NULL,
    pc1512_speed_change,
    NULL,
    NULL,
    NULL
};


/* Initialize the PC1512 display controller, return the correct DDM bits. */
priv_t
m_amstrad_1512_vid_init(const wchar_t *fn, int fnt, int cp)
{
    vid_t *dev;
    FILE *fp;
    int c, d;

    /* Allocate a video controller block. */
    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));

    /* Process the options. */
    dev->fontbase = (cp & 0x03) * 256;

    /* Load the PC1512 CGA Character Set ROM. */
    fp = rom_fopen(fn, L"rb");
    if (fp != NULL) {
	if (fnt == FONT_CGA_THICK) {
		/* Use the second ("thick") font in the ROM. */
		(void)fseek(fp, 2048, SEEK_SET);
	}

	/* The ROM has 4 fonts. */
	for (d = 0; d < 4; d++) {
		/* 8x8 CGA. */
		for (c = 0; c < 256; c++)
			fread(&dev->fontdat[256 * d + c], 1, 8, fp);
	}

	(void)fclose(fp);
    } else {
	ERRLOG("AMSTRAD: cannot load fonts from '%ls'\n", fn);
	return(NULL);
    }

    /* Add the device to the system. */
    device_add_ex(&pc1512_video_device, dev);

    dev->vram = (uint8_t *)mem_alloc(0x10000);
    dev->cgacol = 7;
    dev->cgamode = 0x12;

    timer_add(pc1512_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);
    mem_map_add(&dev->cga.mapping, 0xb8000, 0x08000,
		pc1512_read,NULL,NULL, pc1512_write,NULL,NULL, NULL, 0, dev);
    io_sethandler(0x03d0, 16, pc1512_in,NULL,NULL, pc1512_out,NULL,NULL, dev);

    overscan_x = overscan_y = 16;

    cga_palette = 0;
    
    video_palette_rebuild();

    video_inform(VID_TYPE_CGA, &pc1512_timing);

    return((priv_t)dev);
}


static void
pc1640_recalc_timings(vid_t *dev)
{
    cga_recalctimings(&dev->cga);

    ega_recalctimings(&dev->ega);

    if (dev->cga_enabled) {
	overscan_x = overscan_y = 16;

	dev->dispontime  = dev->cga.dispontime;
	dev->dispofftime = dev->cga.dispofftime;
    } else {
	overscan_x = 16; overscan_y = 28;

	dev->dispontime  = dev->ega.dispontime;
	dev->dispofftime = dev->ega.dispofftime;
    }
}


static void
pc1640_out(uint16_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    switch (addr) {
	case 0x03bb:	/* IGA mode control */
	case 0x03db:	/* IGA mode control */
		dev->cga_enabled = val & 0x40;
		if (dev->cga_enabled) {
			/* Disable EGA. */
			mem_map_disable(&dev->ega.mapping);

			/* Enable CGA. */
			mem_map_enable(&dev->cga.mapping);
			video_inform(VID_TYPE_CGA, &pc1640_timing);
		} else {
			/* Disable CGA. */
			mem_map_disable(&dev->cga.mapping);

			/* Enable EGA. */
			switch (dev->ega.gdcreg[6] & 0xc) {
				case 0x00:	/* 128K at A0000 */
					mem_map_set_addr(&dev->ega.mapping,
							 0xa0000, 0x20000);
					break;

				case 0x04:	/* 64K at A0000 */
					mem_map_set_addr(&dev->ega.mapping,
							 0xa0000, 0x10000);
					break;

				case 0x08:	/* 32K at B0000 */
					mem_map_set_addr(&dev->ega.mapping,
							 0xb0000, 0x08000);
					break;

				case 0x0c:	/* 32K at B8000 */
					mem_map_set_addr(&dev->ega.mapping,
							 0xb8000, 0x08000);
					break;
			}
			video_inform(VID_TYPE_SPEC, &pc1640_timing);
		}
		return;

	case 0x03bf:	/* IGA Hercules control register */
		return;

	case 0x03dd:	/* IGA Plantronics InColor control register */
		return;
    }

    if (dev->cga_enabled)
	cga_out(addr, val, &dev->cga);
      else
	ega_out(addr, val, &dev->ega);
}


static uint8_t
pc1640_in(uint16_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

#if 0
    switch (addr) {
    }
#endif

    if (dev->cga_enabled)
	return(cga_in(addr, &dev->cga));
      else
	return(ega_in(addr, &dev->ega));
}


static void
pc1640_poll(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    if (dev->cga_enabled) {
	overscan_x = overscan_y = 16;

	dev->cga.vidtime = dev->vidtime;
	cga_poll(&dev->cga);
	dev->vidtime = dev->cga.vidtime;
    } else {
	overscan_x = 16; overscan_y = 28;

	dev->ega.vidtime = dev->vidtime;
	ega_poll(&dev->ega);
	dev->vidtime = dev->ega.vidtime;
    }
}


static void
pc1640_close(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    free(dev->ega.vram);

    free(dev);
}


static void
pc1640_speed_changed(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    pc1640_recalc_timings(dev);
}


static const device_t pc1640_video_device = {
    "Amstrad PC1640 Video",
    0, 0, NULL,
    NULL, pc1640_close, NULL,
    NULL,
    pc1640_speed_changed,
    NULL,
    NULL,
    NULL
};


/* Initialize the PC1640 display controller, return the correct DDM bits. */
priv_t
m_amstrad_1640_vid_init(const wchar_t *fn, int sz)
{
    vid_t *dev;

    /* Allocate a video controller block. */
    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));

    device_add_ex(&pc1640_video_device, dev);

    /* Load the EGA BIOS and initialize the EGA. */
    rom_init(&dev->bios_rom, fn, 0xc0000, sz, sz - 1, 0, 0);
    ega_init(&dev->ega, 9, 0);
    mem_map_add(&dev->ega.mapping, 0, 0,
		ega_read,NULL,NULL, ega_write,NULL,NULL, NULL, 0, &dev->ega);

    /* Set up and initialize the CGA. */
    dev->cga.vram = dev->ega.vram;
    dev->cga_enabled = 1;
    cga_init(&dev->cga);
    mem_map_add(&dev->cga.mapping, 0xb8000, 0x08000,
		cga_read,NULL,NULL, cga_write,NULL,NULL, NULL, 0, &dev->cga);
    video_load_font(CGA_FONT_ROM_PATH, FONT_CGA_THICK);

    /* We currently do not implement the MDA/Hercules mode regs at 3B0H */
    io_sethandler(0x03a0, 64, pc1640_in,NULL,NULL, pc1640_out,NULL,NULL, dev);

    timer_add(pc1640_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    overscan_x = overscan_y = 16;

    video_inform(VID_TYPE_CGA, &pc1640_timing);

    return((priv_t)dev);
}


static void
set_vid_time(vid_t *dev)
{
    switch (dev->ida_mode) {
	case IDA_CGA:
	case IDA_TV:
	case IDA_LCD_C:
		dev->vidtime = dev->cga.vidtime;
		break;

	case IDA_MDA:
	case IDA_LCD_M:
		dev->vidtime = dev->mda.vidtime;
		break;
    }
}


/*
 * LCD color mappings:
 * 
 *  0 => solid green
 *  1 => blue on green
 *  2 => green on blue
 *  3 => solid blue
 */
static void
set_lcd_cols(vid_t *dev, uint8_t mode_reg)
{
    const uint8_t *mapping = (mode_reg & 0x80) ? mapping2 : mapping1;
    int c;

    for (c = 0; c < 256; c++) switch (mapping[c]) {
	case 0:
		dev->lcdcols[c][0][0] = dev->lcdcols[c][1][0] = dev->green;
		dev->lcdcols[c][0][1] = dev->lcdcols[c][1][1] = dev->green;
		break;

	case 1:
		dev->lcdcols[c][0][0] = dev->lcdcols[c][1][0] =
					dev->lcdcols[c][1][1] = dev->green;
		dev->lcdcols[c][0][1] = dev->blue;
		break;

	case 2:
		dev->lcdcols[c][0][0] = dev->lcdcols[c][1][0] =
					dev->lcdcols[c][1][1] = dev->blue;
		dev->lcdcols[c][0][1] = dev->green;
		break;

	case 3:
		dev->lcdcols[c][0][0] = dev->lcdcols[c][1][0] = dev->blue;
		dev->lcdcols[c][0][1] = dev->lcdcols[c][1][1] = dev->blue;
		break;
    }
}


static void
lcd_draw_char_80(vid_t *dev, pel_t *pel, uint8_t chr, uint8_t attr,
		 int drawcursor, int blink, int sc, int mode160,
		 uint8_t control)
{
    uint8_t bits = fontdat[chr + dev->cga.fontbase][sc];
    uint8_t bright = 0;
    uint16_t mask;
    int c;

    if (attr & 8) {
	/* bright */
	/* The brightness algorithm appears to be: replace any bit
	 * sequence 011 with 001 (assuming an extra 0 to the left
	 * of the byte). 
	 */
	bright = bits;
	for (c = 0, mask = 0x100; c < 7; c++, mask >>= 1) {
		if (((bits & mask) == 0) &&
		    ((bits & (mask >> 1)) != 0) &&
		    ((bits & (mask >> 2)) != 0)) {
			bright &= ~(mask >> 1);
		}
	}
	bits = bright;
    }

    if (drawcursor)
	bits ^= 0xff;

    for (c = 0, mask = 0x80; c < 8; c++, mask >>= 1) {
	if (mode160)
		pel[c].val = (attr & mask) ? dev->blue : dev->green;
	else {
		if (control & 0x20) /* blinking */
			attr &= 0x7f;
		pel[c].val = dev->lcdcols[attr][blink][(bits & mask) ? 1 : 0];
	}
    }
}


static void
lcd_draw_char_40(vid_t *dev, pel_t *pel, uint8_t chr, uint8_t attr,
		 int drawcursor, int blink, int sc, uint8_t control)
{
    uint8_t bits = fontdat[chr + dev->cga.fontbase][sc];
    uint8_t mask = 0x80;
    int c;

    if (attr & 8) {
	/* bright */
	bits = bits & (bits >> 1);
    }

    if (drawcursor)
	bits ^= 0xff;

    for (c = 0; c < 8; c++, mask >>= 1) {
	if (control & 0x20) {
		pel[c * 2].val = pel[c * 2 + 1].val = 
			dev->lcdcols[attr & 0x7f][blink][(bits & mask) ? 1 : 0];
	} else {
		pel[c * 2].val = pel[c * 2 + 1].val = 
			dev->lcdcols[attr][blink][(bits & mask) ? 1 : 0];
	}
    }
}


static void
lcd_poll_mda(vid_t *dev)
{
    mda_t *mda = &dev->mda;
    uint16_t ca = (mda->crtc[15] | (mda->crtc[14] << 8)) & 0x3fff;
    uint8_t chr, attr;
    int drawcursor;
    int oldvc, oldsc;
    int x, blink;

    if (! mda->linepos) {
	dev->vidtime += mda->dispofftime;
	mda->stat |= 1;
	mda->linepos = 1;
	oldsc = mda->sc;

	if ((mda->crtc[8] & 3) == 3) 
		mda->sc = (mda->sc << 1) & 7;

	if (mda->dispon) {
		if (mda->displine < mda->firstline) {
			mda->firstline = mda->displine;
			video_blit_wait_buffer();
		}
		mda->lastline = mda->displine;

		for (x = 0; x < mda->crtc[1]; x++) {
			chr  = mda->vram[(mda->ma << 1) & 0xfff];
			attr = mda->vram[((mda->ma << 1) + 1) & 0xfff];
			drawcursor = ((mda->ma == ca) && mda->con && mda->cursoron);
			blink = ((mda->blink & 16) && (mda->ctrl & 0x20) && (attr & 0x80) && !drawcursor);

			lcd_draw_char_80(dev, &screen->line[mda->displine][x * 8], chr, attr, drawcursor, blink, mda->sc, 0, mda->ctrl);
			mda->ma++;
		}
	}

	mda->sc = oldsc;
	if (mda->vc == mda->crtc[7] && !mda->sc)
		mda->stat |= 8;

	mda->displine++;
	if (mda->displine >= 500) 
		mda->displine = 0;
    } else {
	dev->vidtime += mda->dispontime;

	if (mda->dispon)
		mda->stat &= ~1;
	mda->linepos = 0;

	if (mda->vsynctime) {
		mda->vsynctime--;
		if (! mda->vsynctime)
			mda->stat &= ~8;
	}

	if (mda->sc == (mda->crtc[11] & 31) || ((mda->crtc[8] & 3) == 3 && mda->sc == ((mda->crtc[11] & 31) >> 1))) { 
		mda->con = 0; 
		mda->coff = 1; 
	}

	if (mda->vadj) {
		mda->sc++;
		mda->sc &= 31;
		mda->ma = mda->maback;

		mda->vadj--;
		if (! mda->vadj) {
			mda->dispon = 1;
			mda->ma = mda->maback = (mda->crtc[13] | (mda->crtc[12] << 8)) & 0x3fff;
			mda->sc = 0;
		}
	} else if (mda->sc == mda->crtc[9] || ((mda->crtc[8] & 3) == 3 && mda->sc == (mda->crtc[9] >> 1))) {
		mda->maback = mda->ma;
		mda->sc = 0;

		oldvc = mda->vc;
		mda->vc++;
		mda->vc &= 127;
		if (mda->vc == mda->crtc[6]) 
			mda->dispon = 0;
		if (oldvc == mda->crtc[4]) {
			mda->vc = 0;

			mda->vadj = mda->crtc[5];
			if (! mda->vadj)
				mda->dispon = 1;
			if (! mda->vadj)
				mda->ma = mda->maback = (mda->crtc[13] | (mda->crtc[12] << 8)) & 0x3fff;
			if ((mda->crtc[10] & 0x60) == 0x20)
				mda->cursoron = 0;
			else
				mda->cursoron = mda->blink & 16;
		}

		if (mda->vc == mda->crtc[7]) {
			mda->dispon = 0;
			mda->displine = 0;
			mda->vsynctime = 16;

			if (mda->crtc[7]) {
				x = mda->crtc[1] * 8;
				mda->lastline++;

				if ((x != xsize) || ((mda->lastline - mda->firstline) != ysize) || video_force_resize_get()) {
					xsize = x;
					ysize = mda->lastline - mda->firstline;
					if (xsize < 64)
						xsize = 656;
					if (ysize < 32)
						ysize = 200;
					set_screen_size(xsize, ysize);

					if (video_force_resize_get())
						video_force_resize_set(0);
                                }

				video_blit_start(0, 0, mda->firstline, 0, ysize, xsize, ysize);
				frames++;

				video_res_x = mda->crtc[1];
				video_res_y = mda->crtc[6];
				video_bpp = 0;
			}

			mda->firstline = 1000;
			mda->lastline = 0;
			mda->blink++;
		}
	} else {
		mda->sc++;
		mda->sc &= 31;
		mda->ma = mda->maback;
	}

	if ((mda->sc == (mda->crtc[10] & 31) || ((mda->crtc[8] & 3) == 3 && mda->sc == ((mda->crtc[10] & 31) >> 1))))
		mda->con = 1;
    }
}


static void
lcd_poll_cga(vid_t *dev)
{
    cga_t *cga = &dev->cga;
    uint8_t chr, attr;
    uint16_t ca, dat;
    int drawcursor;
    int oldvc, oldsc;
    int x, c, blink;

    ca = (cga->crtc[15] | (cga->crtc[14] << 8)) & 0x3fff;

    if (! cga->linepos) {
	dev->vidtime += cga->dispofftime;
	cga->cgastat |= 1;
	cga->linepos = 1;
	oldsc = cga->sc;

	if ((cga->crtc[8] & 3) == 3) 
		cga->sc = ((cga->sc << 1) + cga->oddeven) & 7;

	if (cga->cgadispon) {
		if (cga->displine < cga->firstline) {
			cga->firstline = cga->displine;
			video_blit_wait_buffer();
		}
		cga->lastline = cga->displine;

		if (cga->cgamode & 1) {
			for (x = 0; x < cga->crtc[1]; x++) {
				chr = cga->charbuffer[x << 1];
				attr = cga->charbuffer[(x << 1) + 1];
				drawcursor = ((cga->ma == ca) && cga->con && cga->cursoron);
				blink = ((cga->cgablink & 16) && (cga->cgamode & 0x20) && (attr & 0x80) && !drawcursor);

				lcd_draw_char_80(dev, &screen->line[cga->displine][x * 8], chr, attr, drawcursor, blink, cga->sc, cga->cgamode & 0x40, cga->cgamode);
				cga->ma++;
			}
		} else if (! (cga->cgamode & 2)) {
			for (x = 0; x < cga->crtc[1]; x++) {
				chr  = cga->vram[((cga->ma << 1) & 0x3fff)];
				attr = cga->vram[(((cga->ma << 1) + 1) & 0x3fff)];
				drawcursor = ((cga->ma == ca) && cga->con && cga->cursoron);
				blink = ((cga->cgablink & 16) && (cga->cgamode & 0x20) && (attr & 0x80) && !drawcursor);

				lcd_draw_char_40(dev, &screen->line[cga->displine][x * 16], chr, attr, drawcursor, blink, cga->sc, cga->cgamode);
cga->ma++;
			}
		} else {
			/* Graphics mode */
			for (x = 0; x < cga->crtc[1]; x++) {
				dat = (cga->vram[((cga->ma << 1) & 0x1fff) + ((cga->sc & 1) * 0x2000)] << 8) | cga->vram[((cga->ma << 1) & 0x1fff) + ((cga->sc & 1) * 0x2000) + 1];
				cga->ma++;

				for (c = 0; c < 16; c++) {
					screen->line[cga->displine][(x << 4) + c].val = (dat & 0x8000) ? dev->blue : dev->green;
					dat <<= 1;
				}
			}
		}
	} else {
		if (cga->cgamode & 1)
			cga_hline(screen, 0, cga->displine, (cga->crtc[1] << 3), dev->green);
		else
			cga_hline(screen, 0, cga->displine, (cga->crtc[1] << 4), dev->green);
	}

	if (cga->cgamode & 1)
		x = (cga->crtc[1] << 3);
	else
		x = (cga->crtc[1] << 4);

	cga->sc = oldsc;
	if (cga->vc == cga->crtc[7] && !cga->sc)
		cga->cgastat |= 8;

	cga->displine++;
	if (cga->displine >= 360) 
		cga->displine = 0;
    } else {
	dev->vidtime += cga->dispontime;
	cga->linepos = 0;
	if (cga->vsynctime) {
		cga->vsynctime--;
		if (! cga->vsynctime)
			cga->cgastat &= ~8;
	}

	if (cga->sc == (cga->crtc[11] & 31) || ((cga->crtc[8] & 3) == 3 && cga->sc == ((cga->crtc[11] & 31) >> 1))) { 
		cga->con = 0; 
		cga->coff = 1; 
	}

	if ((cga->crtc[8] & 3) == 3 && cga->sc == (cga->crtc[9] >> 1))
		cga->maback = cga->ma;

	if (cga->vadj) {
		cga->sc++;
		cga->sc &= 31;
		cga->ma = cga->maback;

		cga->vadj--;
		if (! cga->vadj) {
			cga->cgadispon = 1;
			cga->ma = cga->maback = (cga->crtc[13] | (cga->crtc[12] << 8)) & 0x3fff;
			cga->sc = 0;
		}
	} else if (cga->sc == cga->crtc[9]) {
		cga->maback = cga->ma;
		cga->sc = 0;
		oldvc = cga->vc;
		cga->vc++;
		cga->vc &= 127;

		if (cga->vc == cga->crtc[6]) 
			cga->cgadispon = 0;

		if (oldvc == cga->crtc[4]) {
			cga->vc = 0;
			cga->vadj = cga->crtc[5];

			if (! cga->vadj)
				cga->cgadispon = 1;
			if (! cga->vadj)
				cga->ma = cga->maback = (cga->crtc[13] | (cga->crtc[12] << 8)) & 0x3fff;
			if ((cga->crtc[10] & 0x60) == 0x20)
				cga->cursoron = 0;
			else
				cga->cursoron = cga->cgablink & 8;
		}

		if (cga->vc == cga->crtc[7]) {
			cga->cgadispon = 0;
			cga->displine = 0;
			cga->vsynctime = 16;

			if (cga->crtc[7]) {
				if (cga->cgamode & 1)
					x = (cga->crtc[1] << 3);
				else
					x = (cga->crtc[1] << 4);

				cga->lastline++;
				if ((x != xsize) || ((cga->lastline - cga->firstline) != ysize) || video_force_resize_get()) {
					xsize = x;
					ysize = cga->lastline - cga->firstline;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, (ysize << 1));

					if (video_force_resize_get())
						video_force_resize_set(0);
                                }

				video_blit_start(0, 0, cga->firstline, 0, (cga->lastline - cga->firstline), xsize, (cga->lastline - cga->firstline));
				frames++;

				video_res_x = xsize - 16;
				video_res_y = ysize;
				if (cga->cgamode & 1) {
					video_res_x /= 8;
					video_res_y /= cga->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (cga->cgamode & 2)) {
					video_res_x /= 16;
					video_res_y /= cga->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (cga->cgamode & 16)) {
					video_res_x /= 2;
					video_bpp = 2;
				} else {
					video_bpp = 1;
				}
			}

			cga->firstline = 1000;
			cga->lastline = 0;
			cga->cgablink++;
			cga->oddeven ^= 1;
		}
	} else {
		cga->sc++;
		cga->sc &= 31;
		cga->ma = cga->maback;
	}

	if (cga->cgadispon)
		cga->cgastat &= ~1;

	if ((cga->sc == (cga->crtc[10] & 31) || ((cga->crtc[8] & 3) == 3 && cga->sc == ((cga->crtc[10] & 31) >> 1)))) 
		cga->con = 1;

	if (cga->cgadispon && (cga->cgamode & 1)) {
		for (x = 0; x < (cga->crtc[1] << 1); x++)
			cga->charbuffer[x] = cga->vram[(((cga->ma << 1) + x) & 0x3fff)];
	}
    }
}


static void
ida_poll(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    switch (dev->ida_mode) {
	case IDA_CGA:
	case IDA_TV:
		dev->cga.vidtime = dev->vidtime;
		cga_poll(&dev->cga);
		dev->vidtime = dev->cga.vidtime;
		return;

	case IDA_MDA:
		dev->mda.vidtime = dev->vidtime;
		mda_poll(&dev->mda);
		dev->vidtime = dev->mda.vidtime;
		return;

	case IDA_LCD_C:	
		lcd_poll_cga(dev);
		return;

	case IDA_LCD_M:
		lcd_poll_mda(dev);
		return;
    }
}
 
 
static void
ida_out(uint16_t addr, uint8_t val, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    cga_t *cga = &dev->cga;
    mda_t *mda = &dev->mda;
    uint8_t old;

    /* Only allow writes (to non-internal registers) if IDA is enabled. */
    if (! (dev->opctrl & 0x04)) switch (addr) {
	case 0x03b1:
	case 0x03b3:
	case 0x03b5:
	case 0x03b7:
		/* Writes banned to CRTC registers 0-11? */
		if (!(dev->opctrl & 0x40) && mda->crtcreg <= 11) {
			dev->crtc_index = 0x20 | (mda->crtcreg & 0x1f);
			if (dev->opctrl & 0x80) { 
				DEBUG("IDA: NMI (CRTC access: reg=0x%02x val=0x%02x) nmi_mask=%i\n", mda->crtcreg & 0x1f, val, nmi_mask);
				nmi = 1;
			}        
			dev->reg_3df = val;
			return;
		}
		old = mda->crtc[mda->crtcreg];
		mda->crtc[mda->crtcreg] = val & crtc_mask[mda->crtcreg];
		if (old != val) {
			if (mda->crtcreg < 0xe || mda->crtcreg > 0x10) {
				fullchange = changeframecount;
				mda_recalctimings(mda);
				set_vid_time(dev);
			}
		}
		return;

	case 0x03b8:
		old = mda->ctrl;
		mda->ctrl = val;
		if ((mda->ctrl ^ old) & 3) {
			mda_recalctimings(mda);
			set_vid_time(dev);
		}
		dev->crtc_index &= 0x1f;
		dev->crtc_index |= 0x80;
		if (dev->opctrl & 0x80) {
			DEBUG("IDA: NMI (mode control access: val=0x%02x) nmi_mask=%i\n", val, nmi_mask);
			nmi = 1;
		}
		return;

	case 0x03d1:
	case 0x03d3:
	case 0x03d5:
	case 0x03d7:
		/* Writes banned to CRTC registers 0-11? */
		if (!(dev->opctrl & 0x40) && cga->crtcreg <= 11) {
			dev->crtc_index = 0x20 | (cga->crtcreg & 0x1f);
                        if (dev->opctrl & 0x80) { 
				DEBUG("IDA: NMI (CRTC access: reg=0x%02x val=0x%02x) nmi_mask=%i\n", cga->crtcreg & 0x1F, val, nmi_mask);
				nmi = 1;
			}        
			dev->reg_3df = val;
			return;
		}

		old = cga->crtc[cga->crtcreg];
		cga->crtc[cga->crtcreg] = val & crtc_mask[cga->crtcreg];
		if (old != val) {
			if (cga->crtcreg < 0xe || cga->crtcreg > 0x10) {
				fullchange = changeframecount;
				cga_recalctimings(cga);
				set_vid_time(dev);
			}
		}
		return;

	case 0x03d8:
		old = cga->cgamode;
		cga->cgamode = val;
		if ((cga->cgamode ^ old) & 3) {
			cga_recalctimings(cga);
			set_vid_time(dev);
		}
		dev->crtc_index &= 0x1f;
		dev->crtc_index |= 0x80;
		if (dev->opctrl & 0x80) {
			DEBUG("IDA: NMI (mode ctrl access: val=0x%02x) nmi_mask=%i\n", val, nmi_mask);
			nmi = 1;
		} else
			set_lcd_cols(dev, val);
		return;
    }

    /* Writes to internal registers are always allowed. */
    switch (addr) {
	case 0x03de:
		dev->crtc_index = 0x1f;

		/*
		 * NMI only seems to be triggered if the value being written
		 * has the high bit set (enable NMI). So it only protects
		 * writes to this port if you let it?
		 */
		if (val & 0x80) {
			DEBUG("IDA: NMI (opctrl access: val=0x%02x) nmi_mask=%i\n", val, nmi_mask);
			dev->opctrl = val;
			dev->crtc_index |= 0x40;
			nmi = 1;
			return;
		}
		dev->opctrl = val;

		/* Bits 0 and 1 control emulation and output mode. */
		old = dev->ida_mode;
		if (val & 1) {
			/* Monitor */
			dev->ida_mode = (val & 2) ? IDA_MDA : IDA_CGA;
		} else {
			if (dev->type == 1) {
				/* LCD */
				dev->ida_mode = (val & 2) ? IDA_LCD_M:IDA_LCD_C;
			} else {
				/* TV */
				dev->ida_mode = IDA_TV;
			}
		}
		if (old != dev->ida_mode)
			INFO("IDA: mode from %i to %i\n", old, dev->ida_mode);

		old = !(val & 0x04);
		if (dev->ida_enabled ^ old) {
			dev->ida_enabled = old;
			INFO("IDA: %sabled\n",
				dev->ida_enabled ? "en" : "dis");
		}

#if 0
		if (dev->ida_enabled) {
			/*
			 * IDA is enabled.
			 *
			 * Enable the appropriate memory ranges
			 * depending whether the IDA is configured
			 * as MDA or CGA.
			 */
			if (dev->ida_mode == IDA_MDA ||
			    dev->ida_mode == IDA_LCD_M) {
				mem_map_disable(&dev->cga_mapping);
				mem_map_enable(&dev->mda_mapping);
			} else {
				mem_map_disable(&dev->mda_mapping);
				mem_map_enable(&dev->cga_mapping);
			}
		} else {
			/* IDA is disabled. */
			mem_map_disable(&dev->cga_mapping);
			mem_map_disable(&dev->mda_mapping);
		}
#endif

		return;
    }

    if (addr >= 0x03d0 && addr <= 0x03df)
       	cga_out(addr, val, cga);

    if (addr >= 0x03b0 && addr <= 0x03bb)
	mda_out(addr, val, mda);
}


static uint8_t
ida_in(uint16_t addr, priv_t priv)
{
    vid_t *dev = (vid_t *)priv;
    cga_t *cga = &dev->cga;
    mda_t *mda = &dev->mda;
    uint8_t temp;

    /* Only allow reads if IDA is enabled. */
    if (! (dev->opctrl & 0x04)) switch (addr) {
	case 0x03b8:	/* MDA control register */
		return(mda->ctrl);

	case 0x03d8:	/* CGA mode register */
		return(cga->cgamode);

	case 0x03dd:
		temp = dev->crtc_index;		/* read NMI reason */
		dev->crtc_index &= 0x1f;	/* reset NMI reason */
		nmi = 0;			/* and reset NMI flag */
		return(temp);

	case 0x03de:
		temp = (dev->opctrl & 0xc7) | dev->switches;
		DEBUG("IDA: read(opctrl) = %02x (switches=%02x)\n", temp, dev->switches);
		return(temp);

	case 0x03df:
		return(dev->reg_3df);
    }

    if (addr >= 0x03d0 && addr <= 0x03df)
       	return cga_in(addr, cga);

    if (addr >= 0x03b0 && addr <= 0x03bb)
	return mda_in(addr, mda);

    return(0xff);
}


static void
ida_close(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    free(dev->cga.vram);

    free(dev);
}


static void
ida_speed_changed(priv_t priv)
{
    vid_t *dev = (vid_t *)priv;

    cga_recalctimings(&dev->cga);
    mda_recalctimings(&dev->mda);

    set_vid_time(dev);
}


static const device_t ida_video_device = {
    "Amstrad IDA Video",
    0, 0, NULL,
    NULL, ida_close, NULL,
    NULL,
    ida_speed_changed,
    NULL,
    NULL,
    NULL
};


/* Initialize the PC200/PPC display controller, return the correct DDM bits. */
priv_t
m_amstrad_ida_init(int type, const wchar_t *fn, int cp, int em, int dt)
{
    vid_t *dev;
    FILE *fp;
    int c, d;

    /* Allocate a video controller block. */
    dev = (vid_t *)mem_alloc(sizeof(vid_t));
    memset(dev, 0x00, sizeof(vid_t));
    dev->type = type;

    /* Add the device to the system. */
    device_add_ex(&ida_video_device, dev);

    dev->fontbase = (cp & 0x03) * 256;

    dev->ida_enabled = 1;
    dev->ida_mode = em;

    /* Load the Character Set ROM. */
    fp = rom_fopen(fn, L"rb");
    if (fp != NULL) {
	/* The ROM has 4 fonts. */
	for (d = 0; d < 4; d++) {
		/* 8x14 MDA in 8x16 cell. */
		for (c = 0; c < 256; c++)
			fread(&fontdatm[256 * d + c], 1, 16, fp);

		/* 8x8 CGA in 8x16 cell. */
		for (c = 0; c < 256; c++) {
			fread(&fontdat[256 * d + c], 1, 8, fp);
			fseek(fp, 8, SEEK_CUR);
		}
	}

	(void)fclose(fp);
    } else {
	ERRLOG("IDA: cannot load fonts from '%ls'\n", fn);
	return(NULL);
    }

    /*
     * DIP switch settings.
     *
     * For PC-200:
     *   Switch 3 ON is MDA (CGA)
     *   Switch 2 ON is CRT (TV)
     *   Switch 1 ON is 'swap floppy drives' (not implemented).
     *
     * For PPC:
     *   Switch 3 ON disables IDA.
     *   Switch 2 ON is MDA (CGA)
     *   Switch 1 ON is CRT (LCD)
     *
     * Positive logic, ON=1.
     */
    switch (dev->ida_mode) {
	case IDA_CGA:
		if (dev->type == 1)
			dev->switches = 0x08;	/* PPC */
		else
			dev->switches = 0x10;	/* PC */
		break;

	case IDA_MDA:
		if (dev->type == 1)
			dev->switches = 0x18;	/* PPC */
		else
			dev->switches = 0x30;	/* PC */
		break;

	case IDA_TV:
		if (dev->type == 0)
			dev->switches = 0x00;	/* PC */
		break;

	case IDA_LCD_C:
		if (dev->type == 1)
			dev->switches = 0x00;	/* PPC */
		break;

	case IDA_LCD_M:
		if (dev->type == 1)
			dev->switches = 0x10;	/* PPC */
		break;
    }
    INFO("%s: IDA mode %i, switches %02x\n",
	(dev->type==1)?"PPC":"PC200", dev->ida_mode, dev->switches);

    /* Set up the CGA emulation. */
    dev->cga.vram = (uint8_t *)mem_alloc(0x8000);
    dev->cga.fontbase = dev->fontbase;
    cga_init(&dev->cga);
    mem_map_add(&dev->cga_mapping, 0xb8000, 0x08000,
		cga_read,NULL,NULL, cga_write,NULL,NULL, NULL, 0, &dev->cga);
    io_sethandler(0x03d0, 16, ida_in,NULL,NULL, ida_out,NULL,NULL, dev);

    /* Set up the MDA emulation (sharing the video RAM.) */
    dev->mda.vram = dev->cga.vram;
    mda_init(&dev->mda);
    mem_map_add(&dev->mda_mapping, 0xb0000, 0x08000,
		mda_read,NULL,NULL, mda_write,NULL,NULL, NULL, 0, &dev->mda);
    io_sethandler(0x03b0, 12, ida_in,NULL,NULL, ida_out,NULL,NULL, dev);

    mda_setcol(&dev->mda, 0x08, 0, 1, 15);	/* white on black */
    mda_setcol(&dev->mda, 0x88, 0, 1, 15);
    mda_setcol(&dev->mda, 0x40, 0, 1, 0);	/* black on black */
    mda_setcol(&dev->mda, 0xc0, 0, 1, 0);

    /* Set up colors for the LCD panel. */
    dev->green = makecol(0x1c, 0x71, 0x31);
    dev->blue = makecol(0x0f, 0x21, 0x3f);
    set_lcd_cols(dev, 0);

    timer_add(ida_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    overscan_x = overscan_y = 16;

    video_palette_rebuild();

    /*
     * Flag whether the card is behaving like a CGA or an MDA,
     * so that DIP switches are reported correctly.
     */
    if ((dev->ida_mode == IDA_MDA) || (dev->ida_mode == IDA_LCD_M))
	video_inform(VID_TYPE_MDA, &ida_timing);
    else
	video_inform(VID_TYPE_CGA, &ida_timing);

    return((priv_t)dev);
}


uint8_t
m_amstrad_ida_ddm(priv_t arg)
{
    vid_t *dev = (vid_t *)arg;
    uint8_t ret = 0x00;

    switch (dev->ida_mode) {
	case IDA_CGA:			/* CGA, Color80 */
	case IDA_LCD_C:
		ret = 0x02;
		break;

	case IDA_TV:			/* CGA, Color40 */
		ret = 0x01;
		break;

	case IDA_MDA:			/* MDA */
	case IDA_LCD_M:
		ret = 0x03;
		break;
    }

    return(ret);
}
