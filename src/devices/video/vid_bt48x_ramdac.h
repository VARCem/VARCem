/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Bt48x RAMDAC series driver.
 *
 * Version:	@(#)vid_bt485_ramdac.h	1.0.5	2021/01/23
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
#ifndef VIDEO_BT48X_RAMDAC_H
# define VIDEO_BT48X_RAMDAC_H


typedef struct {
    PALETTE	extpal;
    uint32_t	extpallook[256];
    uint8_t	cursor32_data[256];
    uint8_t	cursor64_data[1024];
    int		hwc_y,
		hwc_x;
    uint8_t	cr0;
    uint8_t	cr1;
    uint8_t	cr2;
    uint8_t	cr3;
    uint8_t	cr4;
    uint8_t	status;
    uint8_t	type;
} bt48x_ramdac_t;


extern const device_t bt484_ramdac_device;
extern const device_t att20c504_ramdac_device;
extern const device_t bt485_ramdac_device;
extern const device_t att20c505_ramdac_device;
extern const device_t bt485a_ramdac_device;


extern void	bt48x_ramdac_out(uint16_t addr, int rs2, int rs3, uint8_t val,
				 bt48x_ramdac_t *dev, svga_t *svga);

extern uint8_t	bt48x_ramdac_in(uint16_t addr, int rs2, int rs3,
				bt48x_ramdac_t *dev, svga_t *svga);

extern void	bt48x_recalctimings(bt48x_ramdac_t *dev, svga_t *svga);

extern void	bt48x_hwcursor_draw(svga_t *svga, int displine);

#endif	/*VIDEO_BT48X_RAMDAC_H*/
