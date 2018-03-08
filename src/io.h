/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the I/O handler.
 *
 * Version:	@(#)io.h	1.0.1	2018/02/14
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
#ifndef EMU_IO_H
# define EMU_IO_H


extern void	io_init(void);

extern void	io_sethandler(uint16_t base, int size,
			uint8_t (*inb)(uint16_t addr, void *priv),
			uint16_t (*inw)(uint16_t addr, void *priv),
			uint32_t (*inl)(uint16_t addr, void *priv),
			void (*outb)(uint16_t addr, uint8_t val, void *priv),
			void (*outw)(uint16_t addr, uint16_t val, void *priv),
			void (*outl)(uint16_t addr, uint32_t val, void *priv),
			void *priv);

extern void	io_removehandler(uint16_t base, int size,
			uint8_t (*inb)(uint16_t addr, void *priv),
			uint16_t (*inw)(uint16_t addr, void *priv),
			uint32_t (*inl)(uint16_t addr, void *priv),
			void (*outb)(uint16_t addr, uint8_t val, void *priv),
			void (*outw)(uint16_t addr, uint16_t val, void *priv),
			void (*outl)(uint16_t addr, uint32_t val, void *priv),
			void *priv);

#ifdef PC98
extern void	io_sethandler_interleaved(uint16_t base, int size,
			uint8_t (*inb)(uint16_t addr, void *priv),
			uint16_t (*inw)(uint16_t addr, void *priv),
			uint32_t (*inl)(uint16_t addr, void *priv),
			void (*outb)(uint16_t addr, uint8_t val, void *priv),
			void (*outw)(uint16_t addr, uint16_t val, void *priv),
			void (*outl)(uint16_t addr, uint32_t val, void *priv),
			void *priv);

extern void	io_removehandler_interleaved(uint16_t base, int size,
			uint8_t (*inb)(uint16_t addr, void *priv),
			uint16_t (*inw)(uint16_t addr, void *priv),
			uint32_t (*inl)(uint16_t addr, void *priv),
			void (*outb)(uint16_t addr, uint8_t val, void *priv),
			void (*outw)(uint16_t addr, uint16_t val, void *priv),
			void (*outl)(uint16_t addr, uint32_t val, void *priv),
			void *priv);
#endif

extern uint8_t	inb(uint16_t port);
extern void	outb(uint16_t port, uint8_t  val);
extern uint16_t	inw(uint16_t port);
extern void	outw(uint16_t port, uint16_t val);
extern uint32_t	inl(uint16_t port);
extern void	outl(uint16_t port, uint32_t val);


#endif	/*EMU_IO_H*/
