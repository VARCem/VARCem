/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the IBM PCjr.
 *
 * Version:	@(#)m_pcjr.c	1.0.24	2020/12/15
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
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/system/clk.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/ppi.h"
#include "../devices/ports/serial.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/sound/sound.h"
#include "../devices/sound/snd_speaker.h"
#include "../devices/sound/snd_sn76489.h"
#include "../devices/video/video.h"
#include "../devices/video/vid_cga.h"
#include "../devices/video/vid_cga_comp.h"
#include "../plat.h"
#include "machine.h"


#define PCJR_RGB	0
#define PCJR_COMPOSITE	1

#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01


typedef struct {
    int		display_type;
    int		composite;

    /* Video Controller stuff. */
    mem_map_t	mapping;
    uint8_t	crtc[32];
    int		crtcreg;
    int		array_index;
    uint8_t	array[32];
    int		array_ff;
    int		memctrl;
    uint8_t	stat;
    int		addr_mode;
    uint8_t	*vram,
		*b8000;
    int		linepos, displine;
    int		sc, vc;
    int		dispon;
    int		con, coff, cursoron, blink;
    int		vadj;
    uint16_t	ma, maback;

    tmrval_t	vsynctime,
		dispontime, dispofftime, vidtime;

    int		firstline, lastline;
    void	*cpriv;

    /* Keyboard Controller stuff. */
    int		latched;
    int		data;
    int		serial_data[44];
    int		serial_pos;
    uint8_t	pa;
    uint8_t	pb;
} pcjr_t;


static const uint8_t crtcmask[32] = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f,
    0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const video_timings_t pcjr_timings = { VID_BUS,0,0,0,0,0,0 };
static uint8_t	key_queue[16];
static int	key_queue_start = 0,
		key_queue_end = 0;


static void
recalc_address(pcjr_t *dev)
{
    if ((dev->memctrl & 0xc0) == 0xc0) {
	dev->vram  = &ram[(dev->memctrl & 0x06) << 14];
	dev->b8000 = &ram[(dev->memctrl & 0x30) << 11];
    } else {
	dev->vram  = &ram[(dev->memctrl & 0x07) << 14];
	dev->b8000 = &ram[(dev->memctrl & 0x38) << 11];
    }
}


static void
recalc_timings(pcjr_t *dev)
{
    double _dispontime, _dispofftime, disptime;

    if (dev->array[0] & 1) {
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
vid_out(uint16_t addr, uint8_t val, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;
    uint8_t old;

    switch (addr) {
	case 0x3d4:
		dev->crtcreg = val & 0x1f;
		return;

	case 0x3d5:
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val & crtcmask[dev->crtcreg];
		if (old != val) {
			if (dev->crtcreg < 0xe || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				recalc_timings(dev);
			}
		}
		return;

	case 0x3da:
		if (!dev->array_ff)
			dev->array_index = val & 0x1f;
		else {
			if (dev->array_index & 0x10)
				val &= 0x0f;
			dev->array[dev->array_index & 0x1f] = val;
			if (!(dev->array_index & 0x1f)) {/* Mode Control 1 register ? */
				//if (val & 0x10)
					cga_comp_update(dev->cpriv, val);
			}
		}
		dev->array_ff = !dev->array_ff;
		break;

	case 0x3df:
		dev->memctrl = val;
		dev->addr_mode = val >> 6;
		recalc_address(dev);
		break;
    }
}


static uint8_t
vid_in(uint16_t addr, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0x3d4:
		ret = dev->crtcreg;
		break;

	case 0x3d5:
		ret = dev->crtc[dev->crtcreg];
		break;

	case 0x3da:
		dev->array_ff = 0;
		dev->stat ^= 0x10;
		ret = dev->stat;
		break;
    }

    return(ret);
}


static void
vid_write(uint32_t addr, uint8_t val, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;

    if (dev->memctrl == -1) return;

    dev->b8000[addr & 0x3fff] = val;
}


static uint8_t
vid_read(uint32_t addr, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;

    if (dev->memctrl == -1) return(0xff);
		
    return(dev->b8000[addr & 0x3fff]);
}


static void
vid_poll(priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int drawcursor;
    int x, c;
    int oldvc;
    uint8_t chr, attr;
    uint16_t dat;
    int cols[4];
    int oldsc;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat &= ~1;
	dev->linepos = 1;
	oldsc = dev->sc;
	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = (dev->sc << 1) & 7;
	if (dev->dispon) {
		uint16_t offset = 0;
		uint16_t mask = 0x1fff;

		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;
		cols[0] = (dev->array[2] & 0xf) + 16;
		for (c = 0; c < 8; c++) {
			screen->line[dev->displine][c].pal = cols[0];
			if (dev->array[0] & 1)
				screen->line[dev->displine][c + (dev->crtc[1] << 3) + 8].pal = cols[0];
			  else
				screen->line[dev->displine][c + (dev->crtc[1] << 4) + 8].pal = cols[0];
		}

		switch (dev->addr_mode) {
			case 0: /*Alpha*/
				offset = 0;
				mask = 0x3fff;
				break;

			case 1: /*Low resolution graphics*/
				offset = (dev->sc & 1) * 0x2000;
				break;

			case 3: /*High resolution graphics*/
				offset = (dev->sc & 3) * 0x2000;
				break;
		}

		switch ((dev->array[0] & 0x13) | ((dev->array[3] & 0x08) << 5)) {
			case 0x13: /*320x200x16*/
				for (x = 0; x < dev->crtc[1]; x++) {
					dat = (dev->vram[((dev->ma << 1) & mask) + offset] << 8) | 
					       dev->vram[((dev->ma << 1) & mask) + offset + 1];
					dev->ma++;
					screen->line[dev->displine][(x << 3) + 8].pal  = 
					screen->line[dev->displine][(x << 3) + 9].pal  = dev->array[((dat >> 12) & dev->array[1]) + 16] + 16;
					screen->line[dev->displine][(x << 3) + 10].pal = 
					screen->line[dev->displine][(x << 3) + 11].pal = dev->array[((dat >>  8) & dev->array[1]) + 16] + 16;
					screen->line[dev->displine][(x << 3) + 12].pal = 
					screen->line[dev->displine][(x << 3) + 13].pal = dev->array[((dat >>  4) & dev->array[1]) + 16] + 16;
					screen->line[dev->displine][(x << 3) + 14].pal = 
					screen->line[dev->displine][(x << 3) + 15].pal = dev->array[(dat	 & dev->array[1]) + 16] + 16;
				}
				break;

			case 0x12: /*160x200x16*/
				for (x = 0; x < dev->crtc[1]; x++) {
					dat = (dev->vram[((dev->ma << 1) & mask) + offset] << 8) | 
					       dev->vram[((dev->ma << 1) & mask) + offset + 1];
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
					screen->line[dev->displine][(x << 4) + 23].pal = dev->array[(dat	 & dev->array[1]) + 16] + 16;
				}
				break;

			case 0x03: /*640x200x4*/
				for (x = 0; x < dev->crtc[1]; x++) {
					dat = (dev->vram[((dev->ma << 1) & mask) + offset] << 8) |
					       dev->vram[((dev->ma << 1) & mask) + offset + 1];
					dev->ma++;
					for (c = 0; c < 8; c++) {
						chr  =  (dat >>  7) & 1;
						chr |= ((dat >> 14) & 2);
						screen->line[dev->displine][(x << 3) + 8 + c].pal = dev->array[(chr & dev->array[1]) + 16] + 16;
						dat <<= 1;
					}
				}
				break;

			case 0x01: /*80 column text*/
				for (x = 0; x < dev->crtc[1]; x++) {
					chr  = dev->vram[((dev->ma << 1) & mask) + offset];
					attr = dev->vram[((dev->ma << 1) & mask) + offset + 1];
					drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
					if (dev->array[3] & 4) {
						cols[1] = dev->array[ ((attr & 15)      & dev->array[1]) + 16] + 16;
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
						    screen->line[dev->displine][(x << 3) + c + 8].pal = cols[(fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
					}
					if (drawcursor) {
						for (c = 0; c < 8; c++)
						    screen->line[dev->displine][(x << 3) + c + 8].pal ^= 15;
					}
					dev->ma++;
				}
				break;

			case 0x00: /*40 column text*/
				for (x = 0; x < dev->crtc[1]; x++) {
					chr  = dev->vram[((dev->ma << 1) & mask) + offset];
					attr = dev->vram[((dev->ma << 1) & mask) + offset + 1];
					drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);
					if (dev->array[3] & 4) {
						cols[1] = dev->array[ ((attr & 15)      & dev->array[1]) + 16] + 16;
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
						    screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[(fontdat[chr][dev->sc & 7] & (1 << (c ^ 7))) ? 1 : 0];
					}
					if (drawcursor) {
						for (c = 0; c < 16; c++)
						    screen->line[dev->displine][(x << 4) + c + 8].pal ^= 15;
					}
				}
				break;

			case 0x02: /*320x200x4*/
				cols[0] = dev->array[0 + 16] + 16;
				cols[1] = dev->array[1 + 16] + 16;
				cols[2] = dev->array[2 + 16] + 16;
				cols[3] = dev->array[3 + 16] + 16;
				for (x = 0; x < dev->crtc[1]; x++) {
					dat = (dev->vram[((dev->ma << 1) & mask) + offset] << 8) | 
					       dev->vram[((dev->ma << 1) & mask) + offset + 1];
					dev->ma++;
					for (c = 0; c < 8; c++) {
						screen->line[dev->displine][(x << 4) + (c << 1) + 8].pal =
						screen->line[dev->displine][(x << 4) + (c << 1) + 1 + 8].pal = cols[dat >> 14];
						dat <<= 2;
					}
				}
				break;

			case 0x102: /*640x200x2*/
				cols[0] = dev->array[0 + 16] + 16;
				cols[1] = dev->array[1 + 16] + 16;
				for (x = 0; x < dev->crtc[1]; x++) {
					dat = (dev->vram[((dev->ma << 1) & mask) + offset] << 8) |
					       dev->vram[((dev->ma << 1) & mask) + offset + 1];
					dev->ma++;
					for (c = 0; c < 16; c++) {
						screen->line[dev->displine][(x << 4) + c + 8].pal = cols[dat >> 15];
						dat <<= 1;
					}
				}
				break;
		}
	} else {
		if (dev->array[3] & 4) {
			if (dev->array[0] & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, (dev->array[2] & 0xf) + 16);
			else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, (dev->array[2] & 0xf) + 16);
		} else {
			cols[0] = dev->array[0 + 16] + 16;
			if (dev->array[0] & 1)
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 3) + 16, cols[0]);
			else
				cga_hline(screen, 0, dev->displine, (dev->crtc[1] << 4) + 16, cols[0]);
		}
	}
	if (dev->array[0] & 1)
		x = (dev->crtc[1] << 3) + 16;
	else
		x = (dev->crtc[1] << 4) + 16;

	if (dev->composite)
		cga_comp_process(dev->cpriv, dev->array[0], 0, x >> 2, screen->line[dev->displine]);

	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc) {
		dev->stat |= 8;
	}
	dev->displine++;
	if (dev->displine >= 360) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) 
		dev->stat |= 1;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (!dev->vsynctime) {
			dev->stat &= ~8;
		}
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
		if (!dev->vadj) {
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
			if (!dev->vadj) 
				dev->dispon = 1;
			if (!dev->vadj) 
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
			picint(1 << 5);
			if (dev->crtc[7]) {
				if (dev->array[0] & 1)
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
kbd_write(uint16_t port, uint8_t val, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;

    switch (port) {
	case 0x60:
		dev->pa = val;
		break;

	case 0x61:
		dev->pb = val;

		timer_process();
		timer_update_outstanding();

		speaker_update();
		speaker_gated = val & 1;
		speaker_enable = val & 2;
		if (speaker_enable) 
			speaker_was_enable = 1;
		pit_set_gate(&pit, 2, val & 1);
		sn76489_mute = speaker_mute = 1;
		switch (val & 0x60) {
			case 0x00:
				speaker_mute = 0;
				break;

			case 0x60:
				sn76489_mute = 0;
				break;
		}
		break;

	case 0xa0:
		nmi_mask = val & 0x80;
		pit_set_using_timer(&pit, 1, !(val & 0x20));
		break;
    }
}


static uint8_t
kbd_read(uint16_t port, priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x60:
		ret = dev->pa;
		break;
		
	case 0x61:
		ret = dev->pb;
		break;
		
	case 0x62:
		ret = (dev->latched ? 1 : 0);
		ret |= 0x02; /*Modem card not installed*/
		ret |= (ppispeakon ? 0x10 : 0);
		ret |= (ppispeakon ? 0x20 : 0);
		ret |= (dev->data ? 0x40: 0);
		if (dev->data)
			ret |= 0x40;
		break;
		
	case 0xa0:
		dev->latched = 0;
		ret = 0;
		break;
		
	default:
		ERRLOG("PCjr: bad keyboard read %04X\n", port);
    }

    return(ret);
}


static void
kbd_poll(priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;
    int c, p = 0, key;

    keyboard_delay += (220LL * TIMER_USEC);

    if (key_queue_start != key_queue_end &&
	!dev->serial_pos && !dev->latched) {
	key = key_queue[key_queue_start];

	key_queue_start = (key_queue_start + 1) & 0xf;

	dev->latched = 1;
	dev->serial_data[0] = 1; /*Start bit*/
	dev->serial_data[1] = 0;

	for (c = 0; c < 8; c++) {
		if (key & (1 << c)) {
			dev->serial_data[(c + 1) * 2] = 1;
			dev->serial_data[(c + 1) * 2 + 1] = 0;
			p++;
		} else {
			dev->serial_data[(c + 1) * 2] = 0;
			dev->serial_data[(c + 1) * 2 + 1] = 1;
		}
	}

	if (p & 1) { /*Parity*/
		dev->serial_data[9 * 2] = 1;
		dev->serial_data[9 * 2 + 1] = 0;
	} else {
		dev->serial_data[9 * 2] = 0;
		dev->serial_data[9 * 2 + 1] = 1;
	}

	for (c = 0; c < 11; c++) { /*11 stop bits*/
		dev->serial_data[(c + 10) * 2]     = 0;
		dev->serial_data[(c + 10) * 2 + 1] = 0;
	}

	dev->serial_pos++;
    }

    if (dev->serial_pos) {
	dev->data = dev->serial_data[dev->serial_pos - 1];
	nmi = dev->data;
	dev->serial_pos++;
	if (dev->serial_pos == 42+1)
		dev->serial_pos = 0;
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
    keyboard_adddata(val, kbd_adddata);
}


static void
irq0_timer(int new_out, int old_out)
{
    if (new_out && !old_out) {
	picint(1);
	pit_clock(&pit, 1);
    }

    if (! new_out)
	picintc(1);
}


static void
pcjr_close(priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;

    if (dev->composite && dev->cpriv)
	cga_comp_close(dev->cpriv);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    pcjr_t *dev = (pcjr_t *)priv;

    recalc_timings(dev);
}


static priv_t
pcjr_init(const device_t *info, UNUSED(void *arg))
{
    pcjr_t *dev;

    dev = (pcjr_t *)mem_alloc(sizeof(pcjr_t));
    memset(dev, 0x00, sizeof(pcjr_t));

    /* Add the machine device. */
    device_add_ex(info, (priv_t)dev);

    dev->memctrl = -1;
    dev->display_type = machine_get_config_int("display_type");
    dev->composite = (dev->display_type != PCJR_RGB);
    if (dev->composite)
		dev->cpriv = cga_comp_init(1);
	else
		dev->cpriv = cga_comp_init(0);

    pic_init();

    pit_init();
    pit_set_out_func(&pit, 0, irq0_timer);

    nmi_mask = 0x80;

    if (config.serial_enabled[0]) {
	serial_setup(1, 0x2f8, 3);
	device_add(&serial_1_pcjr_device);
    }

    /* Initialize the video controller. */
    mem_map_add(&dev->mapping, 0xb8000, 0x08000,
		vid_read, NULL, NULL,
		vid_write, NULL, NULL,  NULL, 0, dev);
    io_sethandler(0x03d0, 16,
		  vid_in,NULL,NULL, vid_out,NULL,NULL, dev);
    timer_add(vid_poll, dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);
    video_inform(VID_TYPE_CGA, &pcjr_timings);

    /* Initialize the keyboard. */
    keyboard_scan = 1;
    key_queue_start = key_queue_end = 0;
    io_sethandler(0x0060, 4,
		  kbd_read, NULL, NULL, kbd_write, NULL, NULL, dev);
    io_sethandler(0x00a0, 8,
		  kbd_read, NULL, NULL, kbd_write, NULL, NULL, dev);
    timer_add(kbd_poll, dev, &keyboard_delay, TIMER_ALWAYS_ENABLED);
    keyboard_set_table(scancode_xt);
    keyboard_send = kbd_adddata_ex;

    /* Add the built-in sound device. */
    device_add(&sn76489_device);

    /* Add the floppy controller. */
    device_add(&fdc_pcjr_device);

    return((priv_t)dev);
}


static const device_config_t pcjr_config[] = {
    {
	"display_type", "Display type", CONFIG_SELECTION, "", PCJR_RGB,
	{
		{
			"RGB", PCJR_RGB
		},
		{
			"Composite", PCJR_COMPOSITE
		},
		{
			NULL
		}
	}
    },
    {
	    NULL
    }
};


static const CPU cpus_pcjr[] = {
    { "8088/4.77", CPU_8088, 4772728, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1 },
    { NULL }
};

static const machine_t pcjr_info = {
    MACHINE_ISA | MACHINE_VIDEO,
    MACHINE_VIDEO,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_pcjr}}
};

const device_t m_pcjr = {
    "IBM PCjr",
    DEVICE_ROOT,
    0,
    L"ibm/pcjr",
    pcjr_init, pcjr_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &pcjr_info,
    pcjr_config
};
