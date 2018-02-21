/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the ColorPlus driver.
 *
 * Version:	@(#)vid_colorplus.h	1.0.1	2018/02/14
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
#ifndef VIDEO_COLORPLUS_H
# define VIDEO_COLORPLUS_H


typedef struct colorplus_t
{
	cga_t cga;
        uint8_t control;
} colorplus_t;

void    colorplus_init(colorplus_t *colorplus);
void    colorplus_out(uint16_t addr, uint8_t val, void *p);
uint8_t colorplus_in(uint16_t addr, void *p);
void    colorplus_write(uint32_t addr, uint8_t val, void *p);
uint8_t colorplus_read(uint32_t addr, void *p);
void    colorplus_recalctimings(colorplus_t *colorplus);
void    colorplus_poll(void *p);

extern device_t colorplus_device;


#endif	/*VIDEO_COLORPLUS_H*/
