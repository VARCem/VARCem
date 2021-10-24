/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of video controllers for Tandy models.
 *
 * Version:	@(#)m_tandy1000_vid.c	1.0.8	2021/10/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2021 Sarah Walker.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include "../emu.h"
#include "../timer.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../nvr.h"
#include "../device.h"
#include "../devices/system/clk.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../devices/video/vid_cga_comp.h"
#include "../plat.h"
#include "machine.h"
#include "m_tandy1000.h"


typedef struct {
    int		type;
    int		composite;

    uint32_t	base;

    mem_map_t	mapping;
    mem_map_t	vram_mapping;

    uint8_t	crtc[32];
    int		crtcreg;

    int		array_index;
    uint8_t	array[256];
    int		memctrl;
    uint8_t	mode, col;
    uint8_t	stat;

    uint8_t	*vram, *b8000;
    uint32_t	b8000_mask;
    uint32_t	b8000_limit;
    uint8_t	planar_ctrl;

    int		linepos,
		displine;
    int		sc, vc;
    int		dispon;
    int		con, coff,
		cursoron,
		blink;
    int		vadj;
    uint16_t	ma, maback;

    tmrval_t	vsynctime,
		dispontime,
		dispofftime,
		vidtime;

    int		firstline,
		lastline;

    void	*cpriv;

    uint8_t	fontdat[256][8];
} t1kvid_t;


static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t crtcmask_sl[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const video_timings_t tandy_timing = { VID_BUS,1,2,4,1,2,4 };


static uint8_t	vid_in(uint16_t, priv_t);
static void	vid_out(uint16_t, uint8_t, priv_t);


static void
recalc_mapping(t1kvid_t *dev)
{
    mem_map_disable(&dev->mapping);
    io_removehandler(0x03d0, 16,
		     vid_in, NULL, NULL, vid_out, NULL, NULL, dev);

    if (dev->planar_ctrl & 4) {
	mem_map_enable(&dev->mapping);
	if (dev->array[5] & 1)
		mem_map_set_addr(&dev->mapping, 0xa0000, 0x10000);
	  else
		mem_map_set_addr(&dev->mapping, 0xb8000, 0x8000);
	io_sethandler(0x03d0, 16, vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    }
}


static void
recalc_timings(t1kvid_t *dev)
{
    double _dispontime, _dispofftime, disptime;

    if (dev->mode & 1) {
	disptime = dev->crtc[0] + 1;
	_dispontime = dev->crtc[1];
    } else {
	disptime = (dev->crtc[0] + 1) << 1;
	_dispontime = dev->crtc[1] << 1;
    }

    _dispofftime = disptime - _dispontime;
    _dispontime  *= CGACONST;
    _dispofftime *= CGACONST;

    dev->dispontime  = (tmrval_t)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
recalc_address(t1kvid_t *dev, int sl)
{
    if (sl) {
	dev->b8000_limit = 0x8000;

	if (dev->array[5] & 1) {
		dev->vram  = &ram[((dev->memctrl & 0x04) << 14) + dev->base];
		dev->b8000 = &ram[((dev->memctrl & 0x20) << 11) + dev->base];
	} else if ((dev->memctrl & 0xc0) == 0xc0) {
		dev->vram  = &ram[((dev->memctrl & 0x06) << 14) + dev->base];
		dev->b8000 = &ram[((dev->memctrl & 0x30) << 11) + dev->base];
	} else {
		dev->vram  = &ram[((dev->memctrl & 0x07) << 14) + dev->base];
		dev->b8000 = &ram[((dev->memctrl & 0x38) << 11) + dev->base];
		if ((dev->memctrl & 0x38) == 0x38)
			dev->b8000_limit = 0x4000;
	}
    } else {
	if ((dev->memctrl & 0xc0) == 0xc0) {
		dev->vram  = &ram[((dev->memctrl & 0x06) << 14) + dev->base];
		dev->b8000 = &ram[((dev->memctrl & 0x30) << 11) + dev->base];
		dev->b8000_mask = 0x7fff;
	} else {
		dev->vram  = &ram[((dev->memctrl & 0x07) << 14) + dev->base];
		dev->b8000 = &ram[((dev->memctrl & 0x38) << 11) + dev->base];
		dev->b8000_mask = 0x3fff;
	}
    }
}


static void
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;
    uint8_t old;

    if ((addr >= 0x3d0) && (addr <= 0x3d7))
	addr = (addr & 0xff9) | 0x004;
    switch (addr) {
	case 0x03d0: case 0x03d2:
	case 0x03d4: case 0x03d6:
		dev->crtcreg = val & 0x1f;
		break;

	case 0x03d1: case 0x03d3:
	case 0x03d5: case 0x03d7:
		old = dev->crtc[dev->crtcreg];
		if (dev->type == 2)
			dev->crtc[dev->crtcreg] = val & crtcmask_sl[dev->crtcreg];
		  else
			dev->crtc[dev->crtcreg] = val & crtcmask[dev->crtcreg];
		if (old != val) {
			if (dev->crtcreg < 0xe || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				recalc_timings(dev);
			}
		}
		break;

	case 0x03d8:
		dev->mode = val;
		if ((dev->type != 2) && (dev->cpriv != NULL))
			cga_comp_update(dev->cpriv, dev->mode);
		break;

	case 0x03d9:
		dev->col = val;
		break;

	case 0x03da:
		dev->array_index = val & 0x1f;
		break;

	case 0x03de:
		if (dev->array_index & 16) 
			val &= 0xf;
		dev->array[dev->array_index & 0x1f] = val;
		if (dev->type == 2) {
			if ((dev->array_index & 0x1f) == 5) {
				recalc_mapping(dev);
				recalc_address(dev, 1);
			}
		}
		break;

	case 0x03df:
		dev->memctrl = val;
		if (dev->type == 2)
			recalc_address(dev, 1);
		  else
			recalc_address(dev, 0);
		break;

	case 0x0065:
		if (val == 8) return;	/*Hack*/
		dev->planar_ctrl = val;
		recalc_mapping(dev);
		break;
    }
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;
    uint8_t ret = 0xff;

    if ((addr >= 0x3d0) && (addr <= 0x3d7))
	addr = (addr & 0xff9) | 0x004;

    switch (addr) {
	case 0x03d4:
		ret = dev->crtcreg;
		break;

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
vid_write(uint32_t addr, uint8_t val, priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;

    if (dev->memctrl == -1) return;

    if (dev->type == 2) {
	if (dev->array[5] & 1)
		dev->b8000[addr & 0xffff] = val;
	  else {
		if ((addr & 0x7fff) < dev->b8000_limit)
			dev->b8000[addr & 0x7fff] = val;
	}
    } else {
	dev->b8000[addr & dev->b8000_mask] = val;
    }
}


static uint8_t
vid_read(uint32_t addr, priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;
    uint8_t ret = 0xff;

    if (dev->memctrl == -1) return(ret);

    if (dev->type == 2) {
	if (dev->array[5] & 1)
		ret = dev->b8000[addr & 0xffff];
	if ((addr & 0x7fff) < dev->b8000_limit)
		ret = dev->b8000[addr & 0x7fff];
    } else
	ret = dev->b8000[addr & dev->b8000_mask];
 
    return(ret);
}


static void
vid_poll(priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint16_t dat;
    int cols[4];
    int col;
    int oldsc;

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
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;

		cols[0] = (dev->array[2] & 0xf) + 16;
		for (c = 0; c < 8; c++) {
			if (dev->array[3] & 4) {
				screen->line[dev->displine][c].pal = cols[0];
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = cols[0];
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = cols[0];
			} else if ((dev->mode & 0x12) == 0x12) {
				screen->line[dev->displine][c].pal = 0;
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = 0;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = 0;
			} else {
				screen->line[dev->displine][c].pal = (dev->col & 15) + 16;
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = (dev->col & 15) + 16;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = (dev->col & 15) + 16;
			}
		}

		if ((dev->array[3] & 0x10) && (dev->mode & 1)) { /*320x200x16*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000) + 1];
				dev->ma++;
				screen->line[dev->displine][(x << 3) + 8].pal  = 
					screen->line[dev->displine][(x << 3) + 9].pal  = dev->array[((dat >> 12) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 10].pal = 
					screen->line[dev->displine][(x << 3) + 11].pal = dev->array[((dat >>  8) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 12].pal = 
					screen->line[dev->displine][(x << 3) + 13].pal = dev->array[((dat >>  4) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 14].pal = 
					screen->line[dev->displine][(x << 3) + 15].pal = dev->array[(dat & dev->array[1]) + 16] + 16;
			}
		} else if (dev->array[3] & 0x10) { /*160x200x16*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000) + 1];
				dev->ma++;
				screen->line[dev->displine][(x << 4) + 8].pal  = 
				screen->line[dev->displine][(x << 4) + 9].pal  = 
				screen->line[dev->displine][(x << 4) + 10].pal =
				screen->line[dev->displine][(x << 4) + 11].pal = dev->array[((dat >> 12) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 12].pal = 
				screen->line[dev->displine][(x << 4) + 13].pal =
				screen->line[dev->displine][(x << 4) + 14].pal =
				screen->line[dev->displine][(x << 4) + 15].pal = dev->array[((dat >>  8) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 16].pal = 
				screen->line[dev->displine][(x << 4) + 17].pal =
				screen->line[dev->displine][(x << 4) + 18].pal =
				screen->line[dev->displine][(x << 4) + 19].pal = dev->array[((dat >>  4) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 20].pal = 
				screen->line[dev->displine][(x << 4) + 21].pal =
				screen->line[dev->displine][(x << 4) + 22].pal =
				screen->line[dev->displine][(x << 4) + 23].pal = dev->array[(dat & dev->array[1]) + 16] + 16;
			}
		} else if (dev->array[3] & 0x08) { /*640x200x4 - this implementation is a complete guess!*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000)] << 8) |
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					chr  =  (dat >>  6) & 2;
					chr |= ((dat >> 15) & 1);
					screen->line[dev->displine][(x << 3) + 8 + c].pal = dev->array[(chr & dev->array[1]) + 16] + 16;
					dat <<= 1;
				}
			}
		} else if (dev->mode & 1) {
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->vram[ (dev->ma << 1) & 0x3fff];
				attr = dev->vram[((dev->ma << 1) + 1) & 0x3fff];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->mode & 0x20) {
					cols[1] = dev->array[ ((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[(((attr >> 4) & 7) & dev->array[1]) + 16] + 16;
					if ((dev->blink & 16) && (attr & 0x80) && !drawcursor) 
						cols[1] = cols[0];
				} else {
					cols[1] = dev->array[((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[((attr >> 4) & dev->array[1]) + 16] + 16;
				}
				if (dev->sc & 8) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal = cols[0];
				} else {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
				if (drawcursor) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal ^= 15;
				}
				dev->ma++;
			}
		} else if (! (dev->mode & 2)) {
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->vram[ (dev->ma << 1)      & 0x3fff];
				attr = dev->vram[((dev->ma << 1) + 1) & 0x3fff];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->mode & 0x20) {
					cols[1] = dev->array[ ((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[(((attr >> 4) & 7) & dev->array[1]) + 16] + 16;
					if ((dev->blink & 16) && (attr & 0x80) && !drawcursor) 
						cols[1] = cols[0];
				} else {
					cols[1] = dev->array[((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[((attr >> 4) & dev->array[1]) + 16] + 16;
				}
				dev->ma++;
				if (dev->sc & 8) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
							screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[0];
				} else {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
							screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
				if (drawcursor) {
					for (c = 0; c < 16; c++)
						screen->line[dev->displine][(x << 4) + c + 8].pal ^= 15;
				}
			}
		} else if (! (dev->mode & 16)) {
			cols[0] = (dev->col & 15);
			col = (dev->col & 16) ? 8 : 0;
			if (dev->mode & 4) {
				cols[1] = col | 3;
				cols[2] = col | 4;
				cols[3] = col | 7;
			} else if (dev->col & 32) {
				cols[1] = col | 3;
				cols[2] = col | 5;
				cols[3] = col | 7;
			} else {
				cols[1] = col | 2;
				cols[2] = col | 4;
				cols[3] = col | 6;
			}
			cols[0] = dev->array[(cols[0] & dev->array[1]) + 16] + 16;
			cols[1] = dev->array[(cols[1] & dev->array[1]) + 16] + 16;
			cols[2] = dev->array[(cols[2] & dev->array[1]) + 16] + 16;
			cols[3] = dev->array[(cols[3] & dev->array[1]) + 16] + 16;
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal =
					screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[dat >> 14];
					dat <<= 2;
				}
			}
		} else {
			cols[0] = 0; 
			cols[1] = dev->array[(dev->col & dev->array[1]) + 16] + 16;
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) |
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 16; c++) {
					screen->line[dev->displine][(x << 4) + c + 8].pal = cols[dat >> 15];
					dat <<= 1;
				}
			}
		}
	} else {
		if (dev->array[3] & 4) {
			if (dev->mode & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, (dev->array[2] & 0xf) + 16);
			  else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, (dev->array[2] & 0xf) + 16);
		} else {
			cols[0] = ((dev->mode & 0x12) == 0x12) ? 0 : (dev->col & 0xf) + 16;
			if (dev->mode & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, cols[0]);
			  else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
		}
	}

	if (dev->mode & 1)
		x = (dev->crtc[1] << 3) + 16;
	  else
		x = (dev->crtc[1] << 4) + 16;
	if (dev->composite)
		cga_comp_process(dev->cpriv, dev->mode, 0, x >> 2, screen->line[dev->displine]);

	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 360) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) 
		dev->stat &= ~1;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->stat &= ~8;
	}
	if (dev->sc == (dev->crtc[11] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
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
			dev->vsynctime = 16;
			if (dev->crtc[7]) {
				if (dev->mode & 1)
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
				if (dev->composite) 
				   video_blit_start(0, 0, dev->firstline-4, 0, (dev->lastline - dev->firstline) + 8, xsize, (dev->lastline - dev->firstline) + 8);
				else	  
				   video_blit_start(1, 0, dev->firstline-4, 0, (dev->lastline - dev->firstline) + 8, xsize, (dev->lastline - dev->firstline) + 8);

				frames++;

				video_res_x = xsize - 16;
				video_res_y = ysize;
				if ((dev->array[3] & 0x10) && (dev->mode & 1)) { /*320x200x16*/
					video_res_x /= 2;
					video_bpp = 4;
				} else if (dev->array[3] & 0x10) { /*160x200x16*/
					video_res_x /= 4;
					video_bpp = 4;
				} else if (dev->array[3] & 0x08) { /*640x200x4 - this implementation is a complete guess!*/
					video_bpp = 2;
				} else if (dev->mode & 1) {
					video_res_x /= 8;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (dev->mode & 2)) {
					video_res_x /= 16;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (dev->mode & 16)) {
					video_res_x /= 2;
					video_bpp = 2;
				} else {
				   video_bpp = 1;
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
	if ((dev->sc == (dev->crtc[10] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1)))) 
		dev->con = 1;
    }
}


static void
vid_poll_sl(priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint16_t dat;
    int cols[4];
    int col;
    int oldsc;

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
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;
		cols[0] = (dev->array[2] & 0xf) + 16;
		for (c = 0; c < 8; c++) {
			if (dev->array[3] & 4) {
				screen->line[dev->displine][c].pal = cols[0];
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = cols[0];
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = cols[0];
			} else if ((dev->mode & 0x12) == 0x12) {
				screen->line[dev->displine][c].pal = 0;
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = 0;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = 0;
			} else {
				screen->line[dev->displine][c].pal = (dev->col & 15) + 16;
				if (dev->mode & 1)
					screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = (dev->col & 15) + 16;
				  else
					screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = (dev->col & 15) + 16;
			}
		}
		if (dev->array[5] & 1) { /*640x200x16*/
			for (x = 0; x < dev->crtc[1]*2; x++) {
				dat = (dev->vram[(dev->ma << 1) & 0xffff] << 8) | 
				       dev->vram[((dev->ma << 1) + 1) & 0xffff];
				dev->ma++;
				screen->line[dev->displine][(x << 2) + 8].pal  = dev->array[((dat >> 12) & 0xf)/*dev->array[1])*/ + 16] + 16;
				screen->line[dev->displine][(x << 2) + 9].pal  = dev->array[((dat >>  8) & 0xf)/*dev->array[1])*/ + 16] + 16;
				screen->line[dev->displine][(x << 2) + 10].pal = dev->array[((dat >>  4) & 0xf)/*dev->array[1])*/ + 16] + 16;
				screen->line[dev->displine][(x << 2) + 11].pal = dev->array[(dat & 0xf)/*dev->array[1])*/ + 16] + 16;
			}
		} else if ((dev->array[3] & 0x10) && (dev->mode & 1)) { /*320x200x16*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000) + 1];
				dev->ma++;
				screen->line[dev->displine][(x << 3) + 8].pal  = 
				screen->line[dev->displine][(x << 3) + 9].pal  = dev->array[((dat >> 12) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 10].pal = 
				screen->line[dev->displine][(x << 3) + 11].pal = dev->array[((dat >>  8) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 12].pal = 
				screen->line[dev->displine][(x << 3) + 13].pal = dev->array[((dat >>  4) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 3) + 14].pal = 
				screen->line[dev->displine][(x << 3) + 15].pal = dev->array[(dat & dev->array[1]) + 16] + 16;
			}
		} else if (dev->array[3] & 0x10) { /*160x200x16*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				screen->line[dev->displine][(x << 4) + 8].pal  = 
				screen->line[dev->displine][(x << 4) + 9].pal  = 
				screen->line[dev->displine][(x << 4) + 10].pal =
				screen->line[dev->displine][(x << 4) + 11].pal = dev->array[((dat >> 12) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 12].pal = 
				screen->line[dev->displine][(x << 4) + 13].pal =
				screen->line[dev->displine][(x << 4) + 14].pal =
				screen->line[dev->displine][(x << 4) + 15].pal = dev->array[((dat >>  8) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 16].pal = 
				screen->line[dev->displine][(x << 4) + 17].pal =
				screen->line[dev->displine][(x << 4) + 18].pal =
				screen->line[dev->displine][(x << 4) + 19].pal = dev->array[((dat >>  4) & dev->array[1]) + 16] + 16;
				screen->line[dev->displine][(x << 4) + 20].pal = 
				screen->line[dev->displine][(x << 4) + 21].pal =
				screen->line[dev->displine][(x << 4) + 22].pal =
				screen->line[dev->displine][(x << 4) + 23].pal = dev->array[(dat & dev->array[1]) + 16] + 16;
			}
		} else if (dev->array[3] & 0x08) { /*640x200x4 - this implementation is a complete guess!*/
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000)] << 8) |
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 3) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					chr  =  (dat >>  6) & 2;
					chr |= ((dat >> 15) & 1);
					screen->line[dev->displine][(x << 3) + 8 + c].pal = dev->array[(chr & dev->array[1]) + 16] + 16;
					dat <<= 1;
				}
			}
		} else if (dev->mode & 1) {
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->vram[ (dev->ma << 1)      & 0x3fff];
				attr = dev->vram[((dev->ma << 1) + 1) & 0x3fff];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->mode & 0x20) {
					cols[1] = dev->array[ ((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[(((attr >> 4) & 7) & dev->array[1]) + 16] + 16;
					if ((dev->blink & 16) && (attr & 0x80) && !drawcursor) 
						cols[1] = cols[0];
				} else {
					cols[1] = dev->array[((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[((attr >> 4) & dev->array[1]) + 16] + 16;
				}
				if (dev->sc & 8) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal = cols[0];
				} else {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
				if (drawcursor) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 3) + c + 8].pal ^= 15;
				}
				dev->ma++;
			}
		} else if (! (dev->mode & 2)) {
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->vram[ (dev->ma << 1)      & 0x3fff];
				attr = dev->vram[((dev->ma << 1) + 1) & 0x3fff];
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				if (dev->mode & 0x20) {
					cols[1] = dev->array[ ((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[(((attr >> 4) & 7) & dev->array[1]) + 16] + 16;
					if ((dev->blink & 16) && (attr & 0x80) && !drawcursor) 
						cols[1] = cols[0];
				} else {
					cols[1] = dev->array[((attr & 15) & dev->array[1]) + 16] + 16;
					cols[0] = dev->array[((attr >> 4) & dev->array[1]) + 16] + 16;
				}
				dev->ma++;
				if (dev->sc & 8) {
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
							screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[0];
				} else 	{
					for (c = 0; c < 8; c++)
						screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
							screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(dev->fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
				}
				if (drawcursor) {
					for (c = 0; c < 16; c++)
						screen->line[dev->displine][(x << 4) + c + 8].pal ^= 15;
				}
			}
		} else if (! (dev->mode & 16)) {
			cols[0] = (dev->col & 15) | 16;
			col = (dev->col & 16) ? 24 : 16;
			if (dev->mode & 4) {
				cols[1] = col | 3;
				cols[2] = col | 4;
				cols[3] = col | 7;
			} else if (dev->col & 32) {
				cols[1] = col | 3;
				cols[2] = col | 5;
				cols[3] = col | 7;
			} else {
				cols[1] = col | 2;
				cols[2] = col | 4;
				cols[3] = col | 6;
			}

			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal =
					screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[dat >> 14];
					dat <<= 2;
				}
			}
		} else {
			cols[0] = 0; 
			cols[1] = dev->array[(dev->col & dev->array[1]) + 16] + 16;
			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) |
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				dev->ma++;
				for (c = 0; c < 16; c++) {
					screen->line[dev->displine][(x << 4) + c + 8].pal = cols[dat >> 15];
					dat <<= 1;
				}
			}
		}
	} else {
		if (dev->array[3] & 4) {
			if (dev->mode & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, (dev->array[2] & 0xf) + 16);
			  else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, (dev->array[2] & 0xf) + 16);
		} else {
			cols[0] = ((dev->mode & 0x12) == 0x12) ? 0 : (dev->col & 0xf) + 16;
			if (dev->mode & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, cols[0]);
			  else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
		}
	}

	if (dev->mode & 1)
		x = (dev->crtc[1] << 3) + 16;
	  else
		x = (dev->crtc[1] << 4) + 16;
	dev->sc = oldsc;

	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 360) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) 
		dev->stat &= ~1;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->stat &= ~8;
	}
	if (dev->sc == (dev->crtc[11] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
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
			if (dev->array[5] & 1)
				dev->ma = dev->maback = dev->crtc[13] | (dev->crtc[12] << 8);
			  else
				dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			dev->sc = 0;
		}
	} else if (dev->sc == dev->crtc[9] || ((dev->crtc[8] & 3) == 3 && dev->sc == (dev->crtc[9] >> 1))) {
		dev->maback = dev->ma;
		dev->sc = 0;
		oldvc = dev->vc;
		dev->vc++;
		dev->vc &= 255;
		if (dev->vc == dev->crtc[6]) 
			dev->dispon = 0;
		if (oldvc == dev->crtc[4]) {
			dev->vc = 0;
			dev->vadj = dev->crtc[5];
			if (! dev->vadj) 
				dev->dispon = 1;
			if (! dev->vadj) {
				if (dev->array[5] & 1)
					dev->ma = dev->maback = dev->crtc[13] | (dev->crtc[12] << 8);
				  else
					dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			}
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			  else
				dev->cursoron = dev->blink & 16;
		}
		if (dev->vc == dev->crtc[7]) {
			dev->dispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;
			if (dev->crtc[7]) {
				if (dev->mode & 1)
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

				video_blit_start(1, 0, dev->firstline-4, 0, (dev->lastline - dev->firstline) + 8, xsize, (dev->lastline - dev->firstline) + 8);

				frames++;
				video_res_x = xsize - 16;
				video_res_y = ysize;
				if ((dev->array[3] & 0x10) && (dev->mode & 1)) { /*320x200x16*/
					video_res_x /= 2;
						video_bpp = 4;
				} else if (dev->array[3] & 0x10) { /*160x200x16*/
					video_res_x /= 4;
					video_bpp = 4;
				} else if (dev->array[3] & 0x08) { /*640x200x4 - this implementation is a complete guess!*/
				   video_bpp = 2;
				} else if (dev->mode & 1) {
					video_res_x /= 8;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (dev->mode & 2)) {
					video_res_x /= 16;
					video_res_y /= dev->crtc[9] + 1;
					video_bpp = 0;
				} else if (! (dev->mode & 16)) {
					video_res_x /= 2;
					video_bpp = 2;
				} else {
				   video_bpp = 1;
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
	if ((dev->sc == (dev->crtc[10] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1)))) 
		dev->con = 1;
    }
}


static void
speed_changed(priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;

    recalc_timings(dev);
}


static void
vid_close(priv_t priv)
{
    t1kvid_t *dev = (t1kvid_t *)priv;

    if (dev->cpriv != NULL) {
	cga_comp_close(dev->cpriv);
	dev->cpriv = NULL;
    }

    free(dev);
}


static const device_t tandy1k_video = {
    "Tandy 1000 Video",
    0, 0,
    NULL,
    NULL, vid_close, NULL,
    NULL,
    speed_changed,
    NULL, NULL,
    NULL
};


/* Load a font from its ROM source. */
static void
load_font(t1kvid_t *dev, const wchar_t *fn)
{
    FILE *fp;
    int c;

    fp = rom_fopen(fn, L"rb");
    if (fp == NULL) {
	ERRLOG("Tandy1000: cannot load font '%ls'\n", fn);
	return;
    }

    for (c = 0; c < 256; c++)
	(void)fread(&dev->fontdat[c][0], 1, 8, fp);
    (void)fclose(fp);
}


void
tandy1k_video_init(int type, int display_type, uint32_t base, const wchar_t *fn)
{
    t1kvid_t *dev;

    dev = (t1kvid_t *)mem_alloc(sizeof(t1kvid_t));
    memset(dev, 0x00, sizeof(t1kvid_t));
    dev->type = type;
    dev->base = base;
    dev->memctrl = -1;

    /* Add the video device. */
    device_add_ex(&tandy1k_video, dev);

    load_font(dev, fn);

    dev->composite = (display_type != 0);
    if (dev->composite)
	dev->cpriv = cga_comp_init(1);

    cga_palette = 0;
    video_palette_rebuild();

    if (dev->type == 2) {
	dev->b8000_limit = 0x8000;
	dev->planar_ctrl = 4;
	overscan_x = overscan_y = 16;

	io_sethandler(0x0065, 1, vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

	timer_add(vid_poll_sl, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);
    } else {
	dev->b8000_mask = 0x3fff;

	timer_add(vid_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);
    }

    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);

    io_sethandler(0x03d0, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

    video_inform(VID_TYPE_CGA, &tandy_timing);
}
