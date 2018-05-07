/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the ATI 68860 driver.
 *
 * Version:	@(#)vid_ati68860.h	1.0.1	2018/02/14
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
#ifndef VIDEO_ATI68860_H
# define VIDEO_ATI68860_H


typedef struct ati68860_ramdac_t
{
        uint8_t regs[16];
        void (*render)(struct svga_t *svga);
        
        int dac_write, dac_pos;
        int dac_r, dac_g;
        PALETTE pal;
        uint32_t pallook[2];
        
        int ramdac_type;
} ati68860_ramdac_t;

void ati68860_ramdac_out(uint16_t addr, uint8_t val, ati68860_ramdac_t *ramdac, svga_t *svga);
uint8_t ati68860_ramdac_in(uint16_t addr, ati68860_ramdac_t *ramdac, svga_t *svga);
void ati68860_ramdac_init(ati68860_ramdac_t *ramdac);
void ati68860_set_ramdac_type(ati68860_ramdac_t *ramdac, int type);


#endif	/*VIDEO_ATI68860_H*/
