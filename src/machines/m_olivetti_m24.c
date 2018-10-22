/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the Olivetti M24.
 *
 * Version:	@(#)m_olivetti_m24.c	1.0.12	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../io.h"
#include "../mem.h"
#include "../timer.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/ppi.h"
#include "../devices/system/nmi.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_speaker.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "machine.h"


#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01


typedef struct {
    /* Video stuff. */
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
    int64_t	vsynctime;
    int		vadj;
    int		lineff;
    uint16_t	ma, maback;
    int		dispon;
    int64_t	dispontime, dispofftime;
    int64_t	vidtime;
    int		firstline, lastline;

    /* Keyboard stuff. */
    int		wantirq;
    uint8_t	command;
    uint8_t	status;
    uint8_t	out;
    uint8_t	output_port;
    int		param,
		param_total;
    uint8_t	params[16];
    uint8_t	scan[7];

    /* Mouse stuff. */
    int		mouse_mode;
    int		x, y, b;
} olim24_t;


static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t	key_queue[16];
static int	key_queue_start = 0,
		key_queue_end = 0;


static void
recalc_timings(olim24_t *dev)
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
    dev->dispontime  = (int64_t)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (int64_t)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
vid_out(uint16_t addr, uint8_t val, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;
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
vid_in(uint16_t addr, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;
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
vid_write(uint32_t addr, uint8_t val, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    dev->vram[addr & 0x7fff] = val;
    dev->charbuffer[ ((int)(((dev->dispontime - dev->vidtime) * 2) / (CGACONST / 2))) & 0xfc] = val;
    dev->charbuffer[(((int)(((dev->dispontime - dev->vidtime) * 2) / (CGACONST / 2))) & 0xfc) | 1] = val;	
}


static uint8_t
vid_read(uint32_t addr, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    return(dev->vram[addr & 0x7fff]);
}


static void
vid_poll(void *priv)
{
    olim24_t *dev = (olim24_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint16_t dat, dat2;
    int cols[4];
    int col;
    int oldsc;

    if (!dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;
	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = (dev->sc << 1) & 7;
	if (dev->dispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
		}
		dev->lastline = dev->displine;
		for (c = 0; c < 8; c++) {
			if ((dev->cgamode & 0x12) == 0x12) {
				buffer->line[dev->displine][c] = 0;
				if (dev->cgamode & 1)
					buffer->line[dev->displine][c + (dev->crtc[1] << 3) + 8] = 0;
				  else
					buffer->line[dev->displine][c + (dev->crtc[1] << 4) + 8] = 0;
			} else {
				buffer->line[dev->displine][c] = (dev->cgacol & 15) + 16;
				if (dev->cgamode & 1)
					buffer->line[dev->displine][c + (dev->crtc[1] << 3) + 8] = (dev->cgacol & 15) + 16;
				else
					buffer->line[dev->displine][c + (dev->crtc[1] << 4) + 8] = (dev->cgacol & 15) + 16;
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
					    buffer->line[dev->displine][(x << 3) + c + 8] = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    buffer->line[dev->displine][(x << 3) + c + 8] = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0];
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
					    buffer->line[dev->displine][(x << 4) + (c << 1) + 8] = 
					    buffer->line[dev->displine][(x << 4) + (c << 1) + 1 + 8] = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0] ^ 15;
				} else {
					for (c = 0; c < 8; c++)
					    buffer->line[dev->displine][(x << 4) + (c << 1) + 8] = 
					    buffer->line[dev->displine][(x << 4) + (c << 1) + 1 + 8] = cols[(fontdatm[chr][((dev->sc & 7) << 1) | dev->lineff] & (1 << (c ^ 7))) ? 1 : 0];
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
					buffer->line[dev->displine][(x << 4) + (c << 1) + 8] =
					buffer->line[dev->displine][(x << 4) + (c << 1) + 1 + 8] = cols[dat >> 14];
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
					buffer->line[dev->displine][(x << 4) + c + 8] = cols[dat >> 15];
					dat <<= 1;
				}
			}
		}
	} else {
		cols[0] = ((dev->cgamode & 0x12) == 0x12) ? 0 : (dev->cgacol & 15) + 16;
		if (dev->cgamode & 1)
			cga_hline(buffer, 0, dev->displine, (dev->crtc[1] << 3) + 16, cols[0]);
		else
			cga_hline(buffer, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
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
					if ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) || video_force_resize_get()) {
						xsize = x;
						ysize = dev->lastline - dev->firstline;
						if (xsize < 64) xsize = 656;
						if (ysize < 32) ysize = 200;
						set_screen_size(xsize, ysize + 16);

						if (video_force_resize_get())
							video_force_resize_set(0);
					}

					video_blit_memtoscreen_8(0, dev->firstline - 8, 0, (dev->lastline - dev->firstline) + 16, xsize, (dev->lastline - dev->firstline) + 16);
					frames++;

					video_res_x = xsize - 16;
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
speed_changed(void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    recalc_timings(dev);
}


static void
kbd_poll(void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    keyboard_delay += (1000LL * TIMER_USEC);
    if (dev->wantirq) {
	dev->wantirq = 0;
	picint(2);
	DBGLOG(1, "M24: take IRQ\n");
    }

    if (!(dev->status & STAT_OFULL) && key_queue_start != key_queue_end) {
	DBGLOG(1, "M24: reading %02X from the key queue at %i\n",
				dev->out, key_queue_start);
	dev->out = key_queue[key_queue_start];
	key_queue_start = (key_queue_start + 1) & 0xf;
	dev->status |=  STAT_OFULL;
	dev->status &= ~STAT_IFULL;
	dev->wantirq = 1;
    }
}


static void
kbd_adddata(uint16_t val)
{
    key_queue[key_queue_end] = (uint8_t)(val&0xff);
    key_queue_end = (key_queue_end + 1) & 0xf;
}


static void
kbd_adddata_ex(uint16_t val)
{
    kbd_adddata_process(val, kbd_adddata);
}


static void
kbd_write(uint16_t port, uint8_t val, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    DBGLOG(2, "M24: write %04X %02X\n", port, val);

    switch (port) {
	case 0x60:
		if (dev->param != dev->param_total) {
			dev->params[dev->param++] = val;
			if (dev->param == dev->param_total) {
				switch (dev->command) {
					case 0x11:
						dev->mouse_mode = 0;
						dev->scan[0] = dev->params[0];
						dev->scan[1] = dev->params[1];
						dev->scan[2] = dev->params[2];
						dev->scan[3] = dev->params[3];
						dev->scan[4] = dev->params[4];
						dev->scan[5] = dev->params[5];
						dev->scan[6] = dev->params[6];
						break;

					case 0x12:
						dev->mouse_mode = 1;
						dev->scan[0] = dev->params[0];
						dev->scan[1] = dev->params[1];
						dev->scan[2] = dev->params[2];
						break;
					
					default:
						DEBUG("M24: bad keyboard command complete %02X\n", dev->command);
						break;
				}
			}
		} else {
			dev->command = val;
			switch (val) {
				case 0x01: /*Self-test*/
					break;

				case 0x05: /*Read ID*/
					kbd_adddata(0x00);
					break;

				case 0x11:
					dev->param = 0;
					dev->param_total = 9;
					break;

				case 0x12:
					dev->param = 0;
					dev->param_total = 4;
					break;

				default:
					ERRLOG("M24: bad keyboard command %02X\n", val);
					break;
			}
		}
		break;

	case 0x61:
		ppi.pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();
		speaker_gated = val & 1;
		speaker_enable = val & 2;
		if (speaker_enable) 
			was_speaker_enable = 1;
		pit_set_gate(&pit, 2, val & 1);
		break;
    }
}


static uint8_t
kbd_read(uint16_t port, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x60:
		ret = dev->out;
		if (key_queue_start == key_queue_end) {
			dev->status &= ~STAT_OFULL;
			dev->wantirq = 0;
		} else {
			dev->out = key_queue[key_queue_start];
			key_queue_start = (key_queue_start + 1) & 0xf;
			dev->status |= STAT_OFULL;
			dev->status &= ~STAT_IFULL;
			dev->wantirq = 1;	
		}
		break;

	case 0x61:
		ret = ppi.pb;
		break;

	case 0x64:
		ret = dev->status;
		dev->status &= ~(STAT_RTIMEOUT | STAT_TTIMEOUT);
		break;

	default:
		ERRLOG("\nBad M24 keyboard read %04X\n", port);
		break;
    }

    return(ret);
}


static int
mse_poll(int x, int y, int z, int b, void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    dev->x += x;
    dev->y += y;

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 1) && !(dev->b & 1))
	kbd_adddata(dev->scan[0]);
    if (!(b & 1) && (dev->b & 1))
	kbd_adddata(dev->scan[0] | 0x80);
    dev->b = (dev->b & ~1) | (b & 1);

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 2) && !(dev->b & 2))
	kbd_adddata(dev->scan[2]);
    if (!(b & 2) && (dev->b & 2))
	kbd_adddata(dev->scan[2] | 0x80);
    dev->b = (dev->b & ~2) | (b & 2);

    if (((key_queue_end - key_queue_start) & 0xf) > 14) return(0xff);

    if ((b & 4) && !(dev->b & 4))
	kbd_adddata(dev->scan[1]);
    if (!(b & 4) && (dev->b & 4))
	kbd_adddata(dev->scan[1] | 0x80);
    dev->b = (dev->b & ~4) | (b & 4);

    if (dev->mouse_mode) {
	if (((key_queue_end - key_queue_start) & 0xf) > 12) return(0xff);

	if (!dev->x && !dev->y) return(0xff);

	dev->y = -dev->y;

	if (dev->x < -127) dev->x = -127;
	if (dev->x >  127) dev->x =  127;
	if (dev->x < -127) dev->x = 0x80 | ((-dev->x) & 0x7f);

	if (dev->y < -127) dev->y = -127;
	if (dev->y >  127) dev->y =  127;
	if (dev->y < -127) dev->y = 0x80 | ((-dev->y) & 0x7f);

	kbd_adddata(0xfe);
	kbd_adddata(dev->x);
	kbd_adddata(dev->y);

	dev->x = dev->y = 0;
    } else {
	while (dev->x < -4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0xff);
		dev->x += 4;
		kbd_adddata(dev->scan[3]);
	}
	while (dev->x > 4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0xff);
		dev->x -= 4;
		kbd_adddata(dev->scan[4]);
	}
	while (dev->y < -4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0xff);
		dev->y += 4;
		kbd_adddata(dev->scan[5]);
	}
	while (dev->y > 4) {
		if (((key_queue_end - key_queue_start) & 0xf) > 14)
							return(0xff);
		dev->y -= 4;
		kbd_adddata(dev->scan[6]);
	}
    }

    return(0);
}


static uint8_t
m24_read(uint16_t port, void *priv)
{
    switch (port) {
	case 0x66:
		return(0x00);

	case 0x67:
		return(0x20 | 0x40 | 0x0c);
    }

    return(0xff);
}


static void
vid_close(void *priv)
{
    olim24_t *dev = (olim24_t *)priv;

    free(dev->vram);
    free(dev);
}


static video_timings_t m24_timing = {VID_ISA, 8,16,32, 8,16,32};

const device_t m24_device = {
    "Olivetti M24 Video",
    0, 0,
    NULL, vid_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &m24_timing,
    NULL
};


void
machine_olim24_init(const machine_t *model, void *arg)
{
    olim24_t *dev;

    dev = (olim24_t *)mem_alloc(sizeof(olim24_t));
    memset(dev, 0x00, sizeof(olim24_t));

    machine_common_init(model, arg);

    io_sethandler(0x0066, 2,
		  m24_read,NULL,NULL, NULL,NULL,NULL, dev);

    /* Initialize the video adapter. */
    dev->vram = (uint8_t *)mem_alloc(0x8000);
    overscan_x = overscan_y = 16;
    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);
    io_sethandler(0x03d0, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    timer_add(vid_poll, &dev->vidtime, TIMER_ALWAYS_ENABLED, dev);
    device_add_ex(&m24_device, dev);
    video_inform(VID_TYPE_CGA,
		 (const video_timings_t *)&m24_device.vid_timing);

    /* Initialize the keyboard. */
    dev->status = STAT_LOCK | STAT_CD;
    dev->scan[0] = 0x1c;
    dev->scan[1] = 0x53;
    dev->scan[2] = 0x01;
    dev->scan[3] = 0x4b;
    dev->scan[4] = 0x4d;
    dev->scan[5] = 0x48;
    dev->scan[6] = 0x50;
    io_sethandler(0x0060, 2,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    io_sethandler(0x0064, 1,
		  kbd_read,NULL,NULL, kbd_write,NULL,NULL, dev);
    keyboard_set_table(scancode_xt);
    keyboard_send = kbd_adddata_ex;
    keyboard_scan = 1;
    timer_add(kbd_poll, &keyboard_delay, TIMER_ALWAYS_ENABLED, dev);

    /* Tell mouse driver about our internal mouse. */
    mouse_reset();
    mouse_set_poll(mse_poll, dev);

    /* FIXME: make sure this is correct?? */
    device_add(&at_nvr_device);

    device_add(&fdc_xt_device);

    nmi_init();
}


#if defined(DEV_BRANCH) && defined(USE_PORTABLE3)
/* Compaq Portable III also seems to use this. */
void
machine_olim24_video_init(void)
{
    olim24_t *dev;

    dev = (olim24_t *)mem_alloc(sizeof(olim24_t));
    memset(dev, 0x00, sizeof(olim24_t));

    /* Initialize the video adapter. */
    dev->vram = (uint8_t *)mem_alloc(0x8000);
    overscan_x = overscan_y = 16;
    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		vid_read,NULL,NULL, vid_write,NULL,NULL, NULL, 0, dev);
    io_sethandler(0x03d0, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    io_sethandler(0x13c6, 1,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    io_sethandler(0x23c6, 1,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    timer_add(vid_poll, &dev->vidtime, TIMER_ALWAYS_ENABLED, dev);
    device_add_ex(&m24_device, dev);
    video_inform(VID_TYPE_CGA,
		 (const video_timings_t *)&m24_device.vid_timing);
}
#endif
