/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of a Sierra SC1502X RAMDAC.
 *
 *		Used by the TLIVESA1 driver for ET4000.
 *
 * Version:	@(#)vid_sc1502x_ramdac.c	1.0.2	2018/05/06
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
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_sc1502x_ramdac.h"


void sc1502x_ramdac_out(uint16_t addr, uint8_t val, sc1502x_ramdac_t *ramdac, svga_t *svga)
{
	int oldbpp = 0;
        switch (addr)
        {
                case 0x3C6:
                if (ramdac->state == 4)
                {
                        ramdac->state = 0;
			if (val == 0xFF)  break;
                        ramdac->ctrl = val;
			oldbpp = svga->bpp;
                        switch ((val&1)|((val&0xC0)>>5))
                        {
                                case 0:
                                svga->bpp = 8;
                                break;
                                case 2: case 3:
				switch (val & 0x20)
				{
					case 0x00: svga->bpp = 32; break;
	                                case 0x20: svga->bpp = 24; break;
				}
                                break;
                                case 4: case 5:
                                svga->bpp = 15;
                                break;
                                case 6:
                                svga->bpp = 16;
                                break;
                                case 7:
                                switch (val & 4)
                                {
                                        case 4:
				                switch (val & 0x20)
				                {
					                case 0x00: svga->bpp = 32; break;
	                                                case 0x20: svga->bpp = 24; break;
				                }
                                                break;
                                        case 0: default:
                                                svga->bpp = 16;
                                                break;
                                }
				case 1: default:
				break;
                        }
			if (oldbpp != svga->bpp)
			{
				svga_recalctimings(svga);
			}
                        return;
                }
                ramdac->state = 0;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                ramdac->state = 0;
                break;
        }
        svga_out(addr, val, svga);
}

uint8_t sc1502x_ramdac_in(uint16_t addr, sc1502x_ramdac_t *ramdac, svga_t *svga)
{
        switch (addr)
        {
                case 0x3C6:
                if (ramdac->state == 4)
                {
                        ramdac->state = 0;
                        return ramdac->ctrl;
                }
                ramdac->state++;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                ramdac->state = 0;
                break;
        }
        return svga_in(addr, svga);
}
