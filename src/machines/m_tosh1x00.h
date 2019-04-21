/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Toshiba T1000/T1200 machines.
 *
 * Version:	@(#)m_tosh1x00.h	1.0.6	2019/03/18
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#ifndef MACHINE_TOSH1X00_H
# define MACHINE_TOSH1X00_H


#ifdef EMU_DEVICE_H
extern const device_t	m_tosh1000;
extern const device_t	m_tosh1200;

extern const device_t	t1000_video_device;
extern const device_t	t1200_video_device;
#endif


extern void	t1000_video_options_set(uint8_t options);
extern void	t1000_video_enable(uint8_t enabled);
extern void	t1000_display_set(uint8_t internal);

extern void	t1000_syskey(uint8_t amask, uint8_t omask, uint8_t xmask);

//extern void	t1000_configsys_load(void);
//extern void	t1000_configsys_save(void);

//extern void	t1000_emsboard_load(void);
//extern void	t1000_emsboard_save(void);


#endif	/*MACHINE_TOSH1X00_H*/
