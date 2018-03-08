/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the CGA driver.
 *
 * Version:	@(#)vid_cga.h	1.0.1	2018/02/14
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
#ifndef VIDEO_CGA_H
# define VIDEO_CGA_H


typedef struct cga_t
{
        mem_mapping_t mapping;
        
        int crtcreg;
        uint8_t crtc[32];
        
        uint8_t cgastat;
        
        uint8_t cgamode, cgacol;

	int fontbase;
        int linepos, displine;
        int sc, vc;
        int cgadispon;
        int con, coff, cursoron, cgablink;
        int vsynctime, vadj;
        uint16_t ma, maback;
        int oddeven;

        int64_t dispontime, dispofftime;
        int64_t vidtime;
        
        int firstline, lastline;
        
        int drawcursor;
        
        uint8_t *vram;
        
        uint8_t charbuffer[256];

	int revision;
	int composite;
	int snow_enabled;
	int rgb_type;
} cga_t;

void    cga_init(cga_t *cga);
void    cga_out(uint16_t addr, uint8_t val, void *p);
uint8_t cga_in(uint16_t addr, void *p);
void    cga_write(uint32_t addr, uint8_t val, void *p);
uint8_t cga_read(uint32_t addr, void *p);
void    cga_recalctimings(cga_t *cga);
void    cga_poll(void *p);

extern device_config_t cga_config[];
extern device_t cga_device;


#endif	/*VIDEO_CGA_H*/
