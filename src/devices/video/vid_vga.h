/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the VGA driver.
 *
 * Version:	@(#)vid_vga.h	1.0.0	2021/03/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *          Alheos
 *
 *		Copyright 2021 Fred N. van Kempen.
 *		Copyright 2021 Altheos.
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

#ifndef VIDEO_VGA_H
# define VIDEO_VGA_H

typedef struct {
    svga_t	svga;

    rom_t	bios_rom;
} vga_t;

//void vga_disable(priv_t priv);
//void vga_enable(priv_t priv);

struct svga_t;
extern struct svga_t *mb_vga;

#endif	/*VIDEO_VGA_H*/