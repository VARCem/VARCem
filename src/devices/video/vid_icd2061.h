/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the ICD 2061/9161 driver.
 *
 * Version:	@(#)vid_icd2061.h	1.0.2	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#ifndef VIDEO_ICD2061_H
# define VIDEO_ICD2061_H


typedef struct {
    float	freq[3];

    int		count,
		bit_count;
    int		unlocked,
		state;
    uint32_t	data,
		ctrl;
} icd2061_t;


extern const device_t icd2061_device;
extern const device_t ics9161_device;


extern void	icd2061_write(icd2061_t *icd2061, int val);
extern float	icd2061_getclock(int clock, void *priv);


#define ics9161_write	icd2061_write
#define ics9161_getclock icd2061_getclock


#endif	/*VIDEO_ICD2061_H*/
