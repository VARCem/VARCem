/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Olivetti M24 built-in video controller.
 *
 * Version:	@(#)m_olim24_vid.c	1.0.7	2020/01/24
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#define dbglog kbd_log
#include "../emu.h"
#include "../timer.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/system/clk.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "machine.h"
#include "m_olim24.h"


typedef struct {
    mem_map_t	mapping;
    uint8_t	crtc[32];
    int		crtcreg;
    uint8_t	monitor_type,
		port_23c6;
    uint8_t	*vram;
    uint8_t	charbuffer[256];
    uint8_t	ctrl;
    uint32_t	base;
    uint8_t	cgamode, cgacol;
    uint8_t	stat;
    int		linepos, displine;
    int		sc, vc;
    int		con, coff, cursoron, blink;
    int		vadj;
    int		lineff;
    uint16_t	ma, maback;
    int		dispon;

    tmrval_t	vsynctime,
		dispontime, dispofftime,
		vidtime;

    int		firstline, lastline;
} olivid_t;

static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static video_timings_t m24_timing = {VID_ISA, 8,16,32, 8,16,32};


static void
recalc_timings(olivid_t *dev)
{
    double _dispontime, _dispofftime, disptime;

    if (dev->cgamode & 1) {
	disptime    = dev->crtc[0] + 1;
	_dispontime = dev->crtc[1];
    } else {
	disptime    = (dev->crtc[0] + 1) << 1;
	_dispontime = dev->crtc[1] << 1;
    }

    _dispofftime = disptime - _dispontime;
    _dispontime  *= CGACONST / 2;
    _dispofftime *= CGACONST / 2;
    dev->dispontime  = (tmrval_t)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;
    uint8_t old;

    switch (addr) {
	case 0x03d4:
		dev->crtcreg = val & 31;
		break;

	case 0x03d5:
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val & crtcmask[dev->crtcreg];
		if (old != val) {
			if (dev->crtcreg < 0xe || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				recalc_timings(dev);
			}
		}
		break;

	case 0x03d8:
		dev->cgamode = val;
		break;

	case 0x03d9:
		dev->cgacol = val;
		break;

	case 0x03de:
		dev->ctrl = val;
		dev->base = (val & 0x08) ? 0x4000 : 0;
		break;

	case 0x13c6:
		dev->monitor_type = val;
		break;

	case 0x23c6:
		dev->port_23c6 = val;
		break;
    }
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;
    uint8_t ret = 0xff;

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

	case 0x13c6:
		ret = dev->monitor_type;
		break;

	case 0x23c6:
		ret = dev->port_23c6;
		break;
    }

    return(ret);
}


static void
vid_write(uint32_t addr, uint8_t val, priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;

    dev->vram[addr & 0x7fff] = val;
    dev->charbuffer[ ((int)(((dev->dispontime - dev->vidtime) * 2) / (CGACONST / 2))) & 0xfc] = val;
    dev->charbuffer[(((int)(((dev->dispontime - dev->vidtime) * 2) / (CGACONST / 2))) & 0xfc) | 1] = val;	
}


static uint8_t
vid_read(uint32_t addr, priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;

    return(dev->vram[addr & 0x7fff]);
}


static void
vid_poll(priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c, xs_temp, ys_temp;
    int oldvc;
    uint8_t chr, attr;
    uint16_t dat, dat2;
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
		for (c = 0; c < 8; c++) {
			if ((dev->cgamode & 0x12) == 0x12) {
				screen->line[dev->displine][c].pal = 0;
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
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->charbuffer[ x << 1];
				attr = dev->charbuffer[(x << 1) + 1];
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
					    screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0];
				}
				dev->ma++;
			}
		} else if (!(dev->cgamode & 2)) {
			for (x = 0; x < dev->crtc[1]; x++) {
				chr  = dev->vram[((dev->ma << 1) & 0x3fff) + dev->base];
				attr = dev->vram[(((dev->ma << 1) + 1) & 0x3fff) + dev->base];
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
						screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal = 
					    	screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0];
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
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + dev->base] << 8) | 
				       dev->vram[((dev->ma << 1) & 0x1fff) + ((dev->sc & 1) * 0x2000) + 1 + dev->base];
				dev->ma++;
				for (c = 0; c < 8; c++) {
					screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal =
					screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[dat >> 14];
					dat <<= 2;
				}
			}
		} else {
			if (dev->ctrl & 1 || ((dev->monitor_type & 8) && (dev->port_23c6 & 1))) {
				dat2 = ((dev->sc & 1) * 0x4000) | (dev->lineff * 0x2000);
				cols[0] = 0; cols[1] = /*(dev->cgacol & 15)*/15 + 16;
			} else {
				dat2 = (dev->sc & 1) * 0x2000;
				cols[0] = 0; cols[1] = (dev->cgacol & 15) + 16;
			}

			for (x = 0; x < dev->crtc[1]; x++) {
				dat = (dev->vram[((dev->ma << 1) & 0x1fff) + dat2] << 8) | dev->vram[((dev->ma << 1) & 0x1fff) + dat2 + 1];
				dev->ma++;
				for (c = 0; c < 16; c++) {
					screen->line[dev->displine][(x << 4) + c + 8].pal = cols[dat >> 15];
					dat <<= 1;
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

	if (dev->cgamode & 1)
		x = (dev->crtc[1] << 3) + 16;
	  else
		x = (dev->crtc[1] << 4) + 16;

	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 720)
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) dev->stat &= ~1;
	dev->linepos = 0;
	dev->lineff ^= 1;
	if (dev->lineff) {
		dev->ma = dev->maback;
	} else {
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
				dev->dispon=0;

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
				dev->vsynctime = (dev->crtc[3] >> 4) + 1;
				if (dev->crtc[7]) {
					if (dev->cgamode & 1)
						x = (dev->crtc[1] << 3) + 16;
					  else
						x = (dev->crtc[1] << 4) + 16;
					dev->lastline++;

					xs_temp = x;
					ys_temp = (dev->lastline - dev->firstline);
					
					if ((xs_temp > 0) && (ys_temp > 0)) {
						if (xsize < 64) xs_temp = 656;
						if (ysize < 32) ys_temp = 200;
						if (!enable_overscan)
							xs_temp -= 16;
						if ((xs_temp != xsize) || (ys_temp != ysize) || video_force_resize_get()) {
							xsize = xs_temp;
							ysize = ys_temp;
							set_screen_size(xsize, ysize + (enable_overscan ? 16:0));
						if (video_force_resize_get())
							video_force_resize_set(0);
						}
						
						if (enable_overscan) {
							video_blit_start(1, 0, dev->firstline - 8, 0, (dev->lastline - dev->firstline) + 16, xsize, (dev->lastline - dev->firstline) + 16);
						} else
							video_blit_start(1, 8, dev->firstline, 0, (dev->lastline - dev->firstline), xsize, (dev->lastline - dev->firstline));
					}

					frames++;

					video_res_x = xsize;
					video_res_y = ysize;
					if (dev->cgamode & 1) {
						video_res_x /= 8;
						video_res_y /= (dev->crtc[9] + 1) * 2;
						video_bpp = 0;
					} else if (! (dev->cgamode & 2)) {
						video_res_x /= 16;
						video_res_y /= (dev->crtc[9] + 1) * 2;
						video_bpp = 0;
					} else if (! (dev->cgamode & 16)) {
						video_res_x /= 2;
						video_res_y /= 2;
						video_bpp = 2;
					} else if (! (dev->ctrl & 1)) {
						video_res_y /= 2;
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
	if (dev->dispon && (dev->cgamode & 1)) {
		for (x = 0; x < (dev->crtc[1] << 1); x++)
		    dev->charbuffer[x] = dev->vram[(((dev->ma << 1) + x) & 0x3fff) + dev->base];
	}
    }
}


static void
speed_changed(priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;

    recalc_timings(dev);
}


static void
vid_close(priv_t priv)
{
    olivid_t *dev = (olivid_t *)priv;

    free(dev->vram);

    free(dev);
}


static const device_t video_device = {
    "Olivetti M24 Video",
    0, 0,
    NULL,
    NULL, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    NULL,
    NULL
};


void
m_olim24_vid_init(int type)
{
    olivid_t *dev;

    dev = (olivid_t *)mem_alloc(sizeof(olivid_t));
    memset(dev, 0x00, sizeof(olivid_t));

    dev->vram = (uint8_t *)mem_alloc(0x8000);
    overscan_x = overscan_y = 16;
    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);
    io_sethandler(0x03d0, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);

    switch(type) {
	case 0:		/* Olivetti M24 */
		break;

	case 1:		/* Compaq Portable3 */
		io_sethandler(0x13c6, 1,
			      vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
		io_sethandler(0x23c6, 1,
			      vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
		break;
    }

    timer_add(vid_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    device_add_ex(&video_device, dev);

    video_inform(VID_TYPE_CGA, &m24_timing);
    cga_palette = 0;
    video_palette_rebuild();
}
