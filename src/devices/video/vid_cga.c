/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the old and new IBM CGA graphics cards.
 *
 * Version:	@(#)vid_cga.c	1.0.20	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2008-2019 Sarah Walker.
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
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_cga_comp.h"


#define CGA_RGB 0
#define CGA_COMPOSITE 1

#define COMPOSITE_OLD 0
#define COMPOSITE_NEW 1


static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const int ws_array[16] = {3,4,5,6,7,8,4,5,6,7,8,4,5,6,7,8};
static const video_timings_t cga_timing = { VID_ISA,8,16,32,8,16,32 };


#if defined(_DEBUG) && defined(CGA_DEBUG)
void
cga_print(cga_t *dev)
{
    int x_res = dev->crtc[0x02];
    int hchar = 3;
    char ncol = '4';

    if (! (dev->cgamode & 0x08))
	return;

#if 0
    if (dev->cgamode & 1)
	x_res <<= 3;
    else if (! (dev->cgamode & 2))
	x_res <<= 4;
    else if (! (dev->cgamode & 16))
	x_res <<= 3;
    else
	x_res <<= 4;
#else
    if ((dev->cgamode & 0x12) == 0x02)
	hchar++;
    else if ((dev->cgamode & 0x03) == 0x00)
	hchar++;
    x_res <<= hchar;
#endif

    if ((dev->cgamode & 0x12) == 0x12)
	ncol = '8';
    else if ((dev->cgamode & 0x03) == 0x01)
	ncol = '8';
    if ((dev->cgamode & 0x12) == 0x10)
	ncol = 't';
    else if ((dev->cgamode & 0x03) == 0x03)
	ncol = 'g';

#if 0
    INFO("CGA: HSync Pos: %i chars (%i p), VSync Pos: %i chars (%i p)\n",
	dev->crtc[0x02], x_res, dev->crtc[0x07], dev->crtc[0x07] * (dev->crtc[0x09] + 1));
#else
    INFO("CGA: [%c%c%i] HSync Pos: %i chars (%i p), VSync Pos: %i chars (%i p)\n",
       (dev->cgamode & 2) ? 'G' : 'T', ncol, hchar,
       dev->crtc[0x02], x_res,
       dev->crtc[0x07], dev->crtc[0x07] * (dev->crtc[0x09] + 1));
#endif
}
#endif


void
cga_recalctimings(cga_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    if (dev->cgamode & 1) {
	disptime = (double) (dev->crtc[0] + 1);
	_dispontime = (double) dev->crtc[1];
    } else {
	disptime = (double) ((dev->crtc[0] + 1) << 1);
	_dispontime = (double) (dev->crtc[1] << 1);
    }
    _dispofftime = disptime - _dispontime;
    _dispontime = _dispontime * CGACONST;
    _dispofftime = _dispofftime * CGACONST;

    dev->dispontime = (tmrval_t)(_dispontime * (1LL << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1LL << TIMER_SHIFT));
}


void
cga_out(uint16_t port, uint8_t val, priv_t priv)
{
    cga_t *dev = (cga_t *)priv;
    uint8_t old;

    switch (port) {
	case 0x03d4:
		dev->crtcreg = val & 31;
		return;

	case 0x03d5:
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val & crtcmask[dev->crtcreg];
		if (old != val) {
			if ((dev->crtcreg < 0x0e) || (dev->crtcreg > 0x10)) {
				fullchange = changeframecount;
				cga_recalctimings(dev);
			}
		}
		return;

	case 0x03d8:
		old = dev->cgamode;
		dev->cgamode = val;
		if (old ^ val) {
			if (((old ^ val) & 0x05) && dev->cpriv)
				cga_comp_update(dev->cpriv, val);
			cga_recalctimings(dev);
		}
		return;

	case 0x03d9:
		old = dev->cgacol;
		dev->cgacol = val;
		if (old ^ val)
			cga_recalctimings(dev);
		return;
    }
}


uint8_t
cga_in(uint16_t port, priv_t priv)
{
    cga_t *dev = (cga_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x03d4:
		ret = dev->crtcreg;
		break;

	case 0x03d5:
		ret = dev->crtc[dev->crtcreg];
		break;

	case 0x03da:
		ret = dev->cgastat;
		break;

	default:
		break;
    }

    return ret;
}


void
cga_waitstates(priv_t priv)
{
    int ws;

    ws = ws_array[cycles & 0x0f];
    cycles -= ws;
}


void
cga_write(uint32_t addr, uint8_t val, priv_t priv)
{
    cga_t *dev = (cga_t *)priv;

    dev->vram[addr & 0x3fff] = val;

    if (dev->snow_enabled) {
	dev->charbuffer[ ((int)(((dev->dispontime - dev->vidtime) * 2) / CGACONST)) & 0xfc] = val;
	dev->charbuffer[(((int)(((dev->dispontime - dev->vidtime) * 2) / CGACONST)) & 0xfc) | 1] = val;
    }

    cga_waitstates(dev);
}


uint8_t
cga_read(uint32_t addr, priv_t priv)
{
    cga_t *dev = (cga_t *)priv;

    cga_waitstates(dev);

    if (dev->snow_enabled) {
	dev->charbuffer[ ((int)(((dev->dispontime - dev->vidtime) * 2) / CGACONST)) & 0xfc] = dev->vram[addr & 0x3fff];
	dev->charbuffer[(((int)(((dev->dispontime - dev->vidtime) * 2) / CGACONST)) & 0xfc) | 1] = dev->vram[addr & 0x3fff];
    }

    return dev->vram[addr & 0x3fff];
}


void
cga_hline(bitmap_t *b, int x1, int y, int x2, uint8_t col)
{
    int c;

    if (y < 0 || y >= b->h)
	   return;

    for (c = 0; c < x2 - x1; c++)
	b->line[y][x1].pal = col;
}


void
cga_poll(priv_t priv)
{
    cga_t *dev = (cga_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint8_t border;
    uint16_t dat;
    int cols[4];
    int col;
    int oldsc;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->cgastat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;

	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = ((dev->sc << 1) + dev->oddeven) & 7;

	if (dev->cgadispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;

		for (c = 0; c < 8; c++) {
			if ((dev->cgamode & 0x12) == 0x12) {
				screen->line[(dev->displine << 1)][c].pal =
				screen->line[(dev->displine << 1) + 1][c].pal = 0;
				if (dev->cgamode & 1) {
					screen->line[(dev->displine << 1)][c + (dev->crtc[1] << 3) + 8].pal =
					screen->line[(dev->displine << 1) + 1][c + (dev->crtc[1] << 3) + 8].pal = 0;
				} else {
					screen->line[(dev->displine << 1)][c + (dev->crtc[1] << 4) + 8].pal =
					screen->line[(dev->displine << 1) + 1][c + (dev->crtc[1] << 4) + 8].pal = 0;
				}
			} else {
				screen->line[(dev->displine << 1)][c].pal =
				screen->line[(dev->displine << 1) + 1][c].pal = (dev->cgacol & 15) + 16;
				if (dev->cgamode & 1) {
					screen->line[(dev->displine << 1)][c + (dev->crtc[1] << 3) + 8].pal =
					screen->line[(dev->displine << 1) + 1][c + (dev->crtc[1] << 3) + 8].pal = (dev->cgacol & 15) + 16;
				} else {
					screen->line[(dev->displine << 1)][c + (dev->crtc[1] << 4) + 8].pal =
					screen->line[(dev->displine << 1) + 1][c + (dev->crtc[1] << 4) + 8].pal = (dev->cgacol & 15) + 16;
				}
			}
		}

		if (dev->cgamode & 1) {
			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->cgamode & 8) {	
					chr = dev->charbuffer[x << 1];
					attr = dev->charbuffer[(x << 1) + 1];
				} else
					chr = attr = 0;
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				cols[1] = (attr & 15) + 16;
				if (dev->cgamode & 0x20) {
					cols[0] = ((attr >> 4) & 7) + 16;
					if ((dev->cgablink & 8) && (attr & 0x80) && !dev->drawcursor) 
						cols[1] = cols[0];
				} else
					cols[0] = (attr >> 4) + 16;
				if (drawcursor) {
					for (c = 0; c < 8; c++) {
						screen->line[(dev->displine << 1)][(x << 3) + c + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 3) + c + 8].pal =
							cols[(fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
					}
				} else {
					for (c = 0; c < 8; c++) {
						screen->line[(dev->displine << 1)][(x << 3) + c + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 3) + c + 8].pal =
							cols[(fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
					}
				}
				dev->ma++;
			}
		} else if (! (dev->cgamode & 2)) {
			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->cgamode & 8) {
					chr  = dev->vram[((dev->ma << 1) & 0x3fff)];
					attr = dev->vram[(((dev->ma << 1) + 1) & 0x3fff)];
				} else
					chr = attr = 0;
				drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
				cols[1] = (attr & 15) + 16;
				if (dev->cgamode & 0x20) {
					cols[0] = ((attr >> 4) & 7) + 16;
					if ((dev->cgablink & 8) && (attr & 0x80))
						cols[1] = cols[0];
				} else
					cols[0] = (attr >> 4) + 16;
				dev->ma++;
				if (drawcursor) {
					for (c = 0; c < 8; c++) {
						screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 8].pal =
						screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 1 + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 1 + 8].pal =
							cols[(fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
					}
				} else {
					for (c = 0; c < 8; c++) {
						screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 8].pal =
						screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 1 + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 8].pal =
						screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 1 + 8].pal =
						cols[(fontdat[chr + dev->fontbase][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
					}
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

			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->cgamode & 8)
					dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				else
					dat = 0;
				dev->ma++;
				for (c = 0; c < 8; c++) {
					screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 8].pal =
					screen->line[(dev->displine << 1)][(x << 4) + (c << 1) + 1 + 8].pal =
					screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 8].pal =
					screen->line[(dev->displine << 1) + 1][(x << 4) + (c << 1) + 1 + 8].pal =
						cols[dat >> 14];
					dat <<= 2;
				}
			}
		} else {
			cols[0] = 0; cols[1] = (dev->cgacol & 15) + 16;
			for (x = 0; x < dev->crtc[1]; x++) {
				if (dev->cgamode & 8)	
					dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000)] << 8) | dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1];
				else
					dat = 0;
				dev->ma++;
				for (c = 0; c < 16; c++) {
					screen->line[(dev->displine << 1)][(x << 4) + c + 8].pal =
					screen->line[(dev->displine << 1) + 1][(x << 4) + c + 8].pal =
						cols[dat >> 15];
					dat <<= 1;
				}
			}
		}
	} else {
		cols[0] = ((dev->cgamode & 0x12) == 0x12) ? 0 : (dev->cgacol & 15) + 16;
		if (dev->cgamode & 1) {
			cga_hline(screen, 0, (dev->displine << 1), (dev->crtc[1] << 3) + 16, cols[0]);
			cga_hline(screen, 0, (dev->displine << 1) + 1, (dev->crtc[1] << 3) + 16, cols[0]);
		} else {
			cga_hline(screen, 0, (dev->displine << 1), (dev->crtc[1] << 4) + 16, cols[0]);
			cga_hline(screen, 0, (dev->displine << 1) + 1, (dev->crtc[1] << 4) + 16, cols[0]);
		}
	}

	if (dev->cgamode & 1)
		x = (dev->crtc[1] << 3) + 16;
	else
		x = (dev->crtc[1] << 4) + 16;

	if (dev->composite) {
		if (dev->cgamode & 0x10)
			border = 0x00;
		else
			border = dev->cgacol & 0x0f;

		cga_comp_process(dev->cpriv, dev->cgamode, border, x >> 2, screen->line[(dev->displine << 1)]);
		cga_comp_process(dev->cpriv, dev->cgamode, border, x >> 2, screen->line[(dev->displine << 1) + 1]);
	}

	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->cgastat |= 8;
	dev->displine++;
	if (dev->displine >= 360) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->cgastat &= ~8;
	}

	if (dev->sc == (dev->crtc[11] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
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
		if (! dev->vadj) {
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
			if (! dev->vadj) {
				dev->cgadispon = 1;
				dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			}

			switch (dev->crtc[10] & 0x60) {
				case 0x20:
					dev->cursoron = 0;
					break;

				case 0x60:
					dev->cursoron = dev->cgablink & 0x10;
					break;

				default:
					dev->cursoron = dev->cgablink & 8;
					break;
			}
		}

		if (dev->vc == dev->crtc[7]) {
			dev->cgadispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;

			if (dev->crtc[7]) {
				if (dev->cgamode & 1)
					x = (dev->crtc[1] << 3) + 16;
				else
					x = (dev->crtc[1] << 4) + 16;
				dev->lastline++;
#if defined(_DEBUG) && defined(CGA_DEBUG)
				cga_print(dev);
#endif
				if ((dev->cgamode & 8) && x && (dev->lastline - dev->firstline) &&
				    ((x != xsize) || (((dev->lastline - dev->firstline) << 1) != ysize) ||
				    video_force_resize_get())) {
					xsize = x;
					ysize = (dev->lastline - dev->firstline) << 1;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 400;
					set_screen_size(xsize, ysize + 16);

					if (video_force_resize_get())
						video_force_resize_set(0);
				}

				if (dev->composite) 
					video_blit_start(0, 0, (dev->firstline - 4) << 1, 0, ((dev->lastline - dev->firstline) + 8) << 1,
							       xsize, ((dev->lastline - dev->firstline) + 8) << 1);
				else
					video_blit_start(1, 0, (dev->firstline - 4) << 1, 0, ((dev->lastline - dev->firstline) + 8) << 1,
								 xsize, ((dev->lastline - dev->firstline) + 8) << 1);
				frames++;

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
				} else
					video_bpp = 1;
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
		dev->cgastat &= ~1;

	if ((dev->sc == (dev->crtc[10] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1)))) 
		dev->con = 1;

	if (dev->cgadispon && (dev->cgamode & 1)) {
		for (x = 0; x < (dev->crtc[1] << 1); x++)
			dev->charbuffer[x] = dev->vram[(((dev->ma << 1) + x) & 0x3fff)];
	}
    }
}


void
cga_init(cga_t *dev)
{
    dev->composite = 0;

    if (dev->cpriv != NULL) {
	cga_comp_close(dev->cpriv);
	dev->cpriv = NULL;
    }
}


void
cga_close(priv_t priv)
{
    cga_t *dev = (cga_t *)priv;

    free(dev->vram);

    if (dev->cpriv != NULL)
	cga_comp_close(dev->cpriv);

    free(dev);
}


void
cga_speed_changed(priv_t priv)
{
    cga_t *dev = (cga_t *)priv;

    cga_recalctimings(dev);
}


static priv_t
cga_standalone_init(const device_t *info, UNUSED(void *parent))
{
    int display_type;
    cga_t *dev;

    dev = (cga_t *)mem_alloc(sizeof(cga_t));
    memset(dev, 0x00, sizeof(cga_t));

    cga_init(dev);

    display_type = device_get_config_int("display_type");
    dev->composite = (display_type != CGA_RGB);
    dev->revision = device_get_config_int("composite_type");
    dev->snow_enabled = device_get_config_int("snow_enabled");
    dev->font_type = device_get_config_int("font_type");
    dev->rgb_type = device_get_config_int("rgb_type");

    dev->vram = (uint8_t *)mem_alloc(0x4000);

    if (dev->composite)
	dev->cpriv = cga_comp_init(dev->revision);

    timer_add(cga_poll, (priv_t)dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		cga_read,NULL,NULL, cga_write,NULL,NULL,
		dev->vram, MEM_MAPPING_EXTERNAL, (priv_t)dev);

    io_sethandler(0x03d0, 16,
		  cga_in,NULL,NULL, cga_out,NULL,NULL, (priv_t)dev);

    overscan_x = overscan_y = 16;

    cga_palette = (dev->rgb_type << 1);
    video_palette_rebuild();

    video_load_font(CGA_FONT_ROM_PATH,
		    (dev->font_type) ? FONT_CGA_THICK : FONT_CGA_THIN);

    video_inform(DEVICE_VIDEO_GET(info->flags),
	         (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


const device_config_t cga_config[] = {
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
			NULL
		}
	}
    },
    {
	"font_type", "Font Selection", CONFIG_SELECTION, "", 1,
	{
		{
			"Thin", 0
		},
		{
			"Thick", 1
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


const device_t cga_device = {
    "CGA",
    DEVICE_VIDEO(VID_TYPE_CGA) | DEVICE_ISA,
    0,
    NULL,
    cga_standalone_init, cga_close, NULL,
    NULL,
    cga_speed_changed,
    NULL,
    &cga_timing,
    cga_config
};
