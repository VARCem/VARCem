/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the BT485 driver.
 *
 * Version:	@(#)vid_bt485_ramdac.h	1.0.1	2018/02/14
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
#ifndef VIDEO_BT485_RAMDAC_H
# define VIDEO_BT485_RAMDAC_H


typedef struct bt485_ramdac_t
{
        int magic_count;
        uint8_t command;
        int windex, rindex;
        uint16_t regs[256];
        int reg_ff;
        int rs2;
	int rs3;
} bt485_ramdac_t;

void bt485_ramdac_out(uint16_t addr, uint8_t val, bt485_ramdac_t *ramdac, svga_t *svga);
uint8_t bt485_ramdac_in(uint16_t addr, bt485_ramdac_t *ramdac, svga_t *svga);

float bt485_getclock(int clock, void *p);


#endif	/*VIDEO_BT485_RAMDAC_H*/
