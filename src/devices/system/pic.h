/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Intel 8259 module.
 *
 * Version:	@(#)pic.h	1.0.2	2018/09/05
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
#ifndef EMU_PIC_H
# define EMU_PIC_H


typedef struct PIC {
    uint8_t	icw1,
		icw3,
		icw4,
		mask,
		ins,
		pend,
		mask2;
    uint8_t	vector;
    int		icw;
    int		read;
} PIC;


extern PIC	pic,
		pic2;
extern int	pic_intpending;


extern void	pic_init(void);
extern void	pic2_init(void);
extern void	pic_reset(void);

extern void	picint(uint16_t num);
extern void	picintlevel(uint16_t num);
extern void	picintc(uint16_t num);
extern uint8_t	picinterrupt(void);
extern void	picclear(int num);
extern void	dumppic(void);


#endif	/*EMU_PIC_H*/
