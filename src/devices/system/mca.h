/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the MCA bus handlers.
 *
 * FIXME:	add typedefs for the handler functions so we can shorten
 *		the various function calls etc. Also, move the PS/2 stuff
 *		into PS/2-land somewhere.
 *
 * Version:	@(#)mca.h	1.0.5	2021/03/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#ifndef EMU_MCA_H
# define EMU_MCA_H


extern void	mca_init(int nr_cards);


extern void	mca_add(uint8_t (*read)(int, priv_t), void (*write)(int, uint8_t, priv_t), uint8_t (*feedb)(priv_t), void(*reset)(priv_t), priv_t priv);
extern void	mca_set_index(int index);
extern uint8_t	mca_read(uint16_t port);
extern void	mca_write(uint16_t port, uint8_t val);
extern uint8_t	mca_feedb(void);
extern void	mca_reset(void);

extern void	ps2_cache_clean(void);		//FIXME: move to PS/2


#endif	/*EMU_MCA_H*/
