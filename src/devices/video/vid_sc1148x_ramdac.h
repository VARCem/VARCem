/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the SC1148X driver.
 *
 * Version:	@(#)vid_sc1148x_ramdac_h	1.0.0	2021/02/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *          Altheos
 *
 *		Copyright 2021 Fred N. van Kempen.
 *      Copyright 2021 Altheos.
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
#ifndef VIDEO_SC1148X_RAMDAC_H
# define VIDEO_SC1148X_RAMDAC_H


typedef struct
{
    int type;
    int state;
    uint8_t ctrl;
} sc1148x_ramdac_t;


extern const device_t sc11483_ramdac_device;
extern const device_t sc11487_ramdac_device;

extern void	sc1148x_ramdac_out(uint16_t addr, uint8_t val, sc1148x_ramdac_t *dev, svga_t *svga);
extern uint8_t	sc1148x_ramdac_in(uint16_t addr, sc1148x_ramdac_t *dev, svga_t *svga);


#endif	/*VIDEO_SC1148X_RAMDAC_H*/
