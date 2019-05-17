/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the IBM CGA composite filter.
 *
 * Version:	@(#)vid_cga_comp.h	1.0.4	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Andrew Jenner (reenigne), <andrew@reenigne.org>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#ifndef VIDEO_CGA_COMP_H
# define VIDEO_CGA_COMP_H


extern priv_t	cga_comp_init(int revision);
extern void	cga_comp_close(priv_t);

extern void	cga_comp_update(priv_t, uint8_t cgamode);
extern void	cga_comp_process(priv_t, uint8_t cgamode, uint8_t border,
				 uint32_t blocks, /*int8_t doublewidth,*/
				 pel_t *pels);


#endif	/*VIDEO_CGA_COMP_H*/
