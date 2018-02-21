/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ICS2595 clock chip emulation.  Used by ATI Mach64.
 *
 * Version:	@(#)vid_ics2595.c	1.0.1	2018/02/14
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "vid_ics2595.h"


enum
{
        ICS2595_IDLE = 0,
        ICS2595_WRITE,
        ICS2595_READ
};


static int ics2595_div[4] = {8, 4, 2, 1};


void ics2595_write(ics2595_t *ics2595, int strobe, int dat)
{
        if (strobe)
        {
                if ((dat & 8) && !ics2595->oldfs3) /*Data clock*/
                {
                        switch (ics2595->state)
                        {
                                case ICS2595_IDLE:
                                ics2595->state = (dat & 4) ? ICS2595_WRITE : ICS2595_IDLE;
                                ics2595->pos = 0;
                                break;
                                case ICS2595_WRITE:
                                ics2595->dat = (ics2595->dat >> 1);
                                if (dat & 4)
                                        ics2595->dat |= (1 << 19);
                                ics2595->pos++;
                                if (ics2595->pos == 20)
                                {
                                        int d, n, l;
                                        l = (ics2595->dat >> 2) & 0xf;
                                        n = ((ics2595->dat >> 7) & 255) + 257;
                                        d = ics2595_div[(ics2595->dat >> 16) & 3];

                                        ics2595->clocks[l] = (14318181.8 * ((double)n / 46.0)) / (double)d;
                                        ics2595->state = ICS2595_IDLE;
                                }
                                break;                                                
                        }
                }
                        
                ics2595->oldfs2 = dat & 4;
                ics2595->oldfs3 = dat & 8;
        }
        ics2595->output_clock = ics2595->clocks[dat];
}
