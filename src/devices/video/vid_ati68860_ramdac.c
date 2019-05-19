/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ATI 68860 RAMDAC emulation (for Mach64)
 *
 *		ATI 68860/68880 Truecolor DACs:
 *		  REG08 (R/W):
 *		  bit 0-?  Always 2 ??
 *
 *		  REG0A (R/W):
 *		  bit 0-?  Always 1Dh ??
 *
 *		  REG0B (R/W):  (GMR ?)
 *		  bit 0-7  Mode. 82h: 4bpp, 83h: 8bpp,
 *				 A0h: 15bpp, A1h: 16bpp, C0h: 24bpp,
 *				 E3h: 32bpp  (80h for VGA modes ?)
 *		  
 *		  REG0C (R/W):  Device Setup Register A
 *		  bit   0  Controls 6/8bit DAC. 0: 8bit DAC/LUT, 1: 6bit DAC/LUT
 *		  2-3  Depends on Video memory (= VRAM width ?) .
 *			1: Less than 1Mb, 2: 1Mb, 3: > 1Mb
 *		  5-6  Always set ?
 *		    7  If set can remove "snow" in some cases
 *			(A860_Delay_L ?) ??
 *
 * Version:	@(#)vid_ati68860.c	1.0.7	2019/05/17
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
#include "../../timer.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_ati68860_ramdac.h"


void
ati68860_ramdac_out(uint16_t addr, uint8_t val, ati68860_ramdac_t *dev, svga_t *svga)
{
    switch (addr) {
	case 0: 
		svga_out(0x3c8, val, svga);
		break;

	case 1: 
		svga_out(0x3c9, val, svga); 
		break;

	case 2: 
		svga_out(0x3c6, val, svga); 
		break;

	case 3: 
		svga_out(0x3c7, val, svga); 
		break;

	default:
		dev->regs[addr & 0xf] = val;
		switch (addr & 0xf) {
			case 0x4:
				dev->dac_addr = val;
				dev->dac_pos = 0;
				break;

			case 0x5:
				switch (dev->dac_pos) {
					case 0:
						dev->dac_r = val;
						dev->dac_pos++; 
						break;

					case 1:
						dev->dac_g = val;
						dev->dac_pos++; 
						break;

					case 2:
						if (dev->dac_addr > 1)
							break;
						dev->pal[dev->dac_addr].r = dev->dac_r; 
						dev->pal[dev->dac_addr].g = dev->dac_g;
						dev->pal[dev->dac_addr].b = val; 
						if (dev->ramdac_type == RAMDAC_8BIT)
							dev->pallook[dev->dac_addr] = makecol32(dev->pal[dev->dac_addr].r,
												      dev->pal[dev->dac_addr].g,
												      dev->pal[dev->dac_addr].b);
						else
							dev->pallook[dev->dac_addr] = makecol32(video_6to8[dev->pal[dev->dac_addr].r & 0x3f],
												      video_6to8[dev->pal[dev->dac_addr].g & 0x3f],
												      video_6to8[dev->pal[dev->dac_addr].b & 0x3f]);
						dev->dac_pos = 0;
						dev->dac_addr = (dev->dac_addr + 1) & 255; 
						break;
				}
				break;

			case 0xb:
				switch (val) {
					case 0x82:
						dev->render = svga_render_4bpp_highres;
						break;

					case 0x83:
						dev->render = svga_render_8bpp_highres;
						break;

					case 0xa0: case 0xb0:
						dev->render = svga_render_15bpp_highres;
						break;

					case 0xa1: case 0xb1:
						dev->render = svga_render_16bpp_highres;
						break;

					case 0xc0: case 0xd0:
						dev->render = svga_render_24bpp_highres;
						break;

					case 0xe2: case 0xf7:
						dev->render = svga_render_32bpp_highres;
						break;

					case 0xe3:
						dev->render = svga_render_ABGR8888_highres;
						break;

					case 0xf2:
						dev->render = svga_render_RGBA8888_highres;
						break;

					default:
						dev->render = svga_render_8bpp_highres;
						break;
                        	}
				break;

			case 0xc:
				svga_set_ramdac_type(svga, (val & 1) ? RAMDAC_6BIT : RAMDAC_8BIT);
				break;
		}
		break;
    }
}


uint8_t
ati68860_ramdac_in(uint16_t addr, ati68860_ramdac_t *dev, svga_t *svga)
{
    uint8_t temp = 0;

    switch (addr) {
	case 0:
		temp = svga_in(0x3c8, svga);
		break;

	case 1:
		temp = svga_in(0x3c9, svga);
		break;

	case 2:
		temp = svga_in(0x3c6, svga);
		break;

	case 3:
		temp = svga_in(0x3c7, svga);
		break;

	case 4: case 8:
		temp = 2; 
		break;

	case 6: case 0xa:
		temp = 0x1d;
		break;

	case 0xf:
		temp = 0xd0;
		break;

	default:
		temp = dev->regs[addr & 0xf];
		break;
    }

    return temp;
}


void
ati68860_set_ramdac_type(ati68860_ramdac_t *dev, int type)
{
    int c;

    if (dev->ramdac_type != type) {
	dev->ramdac_type = type;
	for (c = 0; c < 2; c++) {
		if (dev->ramdac_type == RAMDAC_8BIT)
			dev->pallook[c] = makecol32(dev->pal[c].r,
						    dev->pal[c].g,
						    dev->pal[c].b);
		else
			dev->pallook[c] = makecol32(video_6to8[dev->pal[c].r & 0x3f],
						    video_6to8[dev->pal[c].g & 0x3f],
						    video_6to8[dev->pal[c].b & 0x3f]); 
	}
    }
}


static void *
ati68860_init(const device_t *info, UNUSED(void *parent))
{
    ati68860_ramdac_t *dev;

    dev = (ati68860_ramdac_t *)mem_alloc(sizeof(ati68860_ramdac_t));
    memset(dev, 0x00, sizeof(ati68860_ramdac_t));

    dev->render = svga_render_8bpp_highres;

    return dev;
}


static void
ati68860_close(void *priv)
{
    ati68860_ramdac_t *dev = (ati68860_ramdac_t *)priv;

    if (dev != NULL)
	free(dev);
}


const device_t ati68860_ramdac_device = {
    "ATi-68860 RAMDAC",
    0, 0, NULL,
    ati68860_init, ati68860_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
