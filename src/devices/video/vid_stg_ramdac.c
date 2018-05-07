/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		STG1702 true color RAMDAC emulation.
 *
 * Version:	@(#)vid_stg_ramdac.c	1.0.3	2018/05/06
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
#include "../../emu.h"
#include "../../mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_stg_ramdac.h"


static int stg_state_read[2][8] = {{1,2,3,4,0,0,0,0}, {1,2,3,4,5,6,7,7}};
static int stg_state_write[8] = {0,0,0,0,0,6,7,7};


void stg_ramdac_set_bpp(svga_t *svga, stg_ramdac_t *ramdac)
{
	if (ramdac->command & 0x8)
	{
	        switch (ramdac->regs[3])
        	{
	                case 0: case 5: case 7: svga->bpp = 8;  break;
        	        case 1: case 2: case 8: svga->bpp = 15; break;
	                case 3: case 6:         svga->bpp = 16; break;
        	        case 4: case 9:         svga->bpp = 24; break;
	                default:                svga->bpp = 8; break;
	        }
	}
	else
	{
                switch (ramdac->command >> 5)
                {
                        case 0:  svga->bpp =  8; break;
                        case 5:  svga->bpp = 15; break;
                        case 6:  svga->bpp = 16; break;
                        case 7:  svga->bpp = 24; break;
                        default: svga->bpp =  8; break;
                }
	}
	svga_recalctimings(svga);
}

void stg_ramdac_out(uint16_t addr, uint8_t val, stg_ramdac_t *ramdac, svga_t *svga)
{
        int didwrite, old;
        switch (addr)
        {
                case 0x3c6:
                switch (ramdac->magic_count)
                {
			/* 0 = PEL mask register */
                        case 0: case 1: case 2: case 3: 
                        break;
                        case 4:		/* REG06 */
			old = ramdac->command;
                        ramdac->command = val; 
			if ((old ^ val) & 8)
			{
				stg_ramdac_set_bpp(svga, ramdac);
			}
			else
			{
				if ((old ^ val) & 0xE0)
				{
					stg_ramdac_set_bpp(svga, ramdac);
				}
			}
                        break;
                        case 5: 
                        ramdac->index = (ramdac->index & 0xff00) | val; 
                        break;
                        case 6: 
                        ramdac->index = (ramdac->index & 0xff) | (val << 8); 
                        break;
                        case 7:
                        if (ramdac->index < 0x100)
			{ 
                                ramdac->regs[ramdac->index] = val;
			}
			if ((ramdac->index == 3) && (ramdac->command & 8))  stg_ramdac_set_bpp(svga, ramdac);
                        ramdac->index++;
                        break;
                }
                didwrite = (ramdac->magic_count >= 4);
                ramdac->magic_count = stg_state_write[ramdac->magic_count & 7];
                if (didwrite) return;
                break;
                case 0x3c7: case 0x3c8: case 0x3c9:
                ramdac->magic_count=0;
                break;
        }
        svga_out(addr, val, svga);
}

uint8_t stg_ramdac_in(uint16_t addr, stg_ramdac_t *ramdac, svga_t *svga)
{
        uint8_t temp = 0xff;
        switch (addr)
        {
                case 0x3c6:
                switch (ramdac->magic_count)
                {
                        case 0: case 1: case 2: case 3: 
                        temp = 0xff;
                        break;
                        case 4: 
                        temp = ramdac->command; 
                        break;
                        case 5: 
                        temp = ramdac->index & 0xff; 
                        break;
                        case 6: 
                        temp = ramdac->index >> 8; 
                        break;
                        case 7:
                        switch (ramdac->index)
                        {
                                case 0: 
                                temp = 0x44; 
                                break;
                                case 1: 
                                temp = 0x03; 
                                break;
				case 7:
				temp = 0x88;
				break;
                                default:
                                if (ramdac->index < 0x100) temp = ramdac->regs[ramdac->index];
                                else                       temp = 0xff;
                                break;
                        }
                        ramdac->index++;
                        break;
                }
                ramdac->magic_count = stg_state_read[(ramdac->command & 0x10) ? 1 : 0][ramdac->magic_count & 7];
                return temp;
                case 0x3c7: case 0x3c8: case 0x3c9:
                ramdac->magic_count=0;
                break;
        }
        return svga_in(addr, svga);
}

float stg_getclock(int clock, void *p)
{
        stg_ramdac_t *ramdac = (stg_ramdac_t *)p;
        float t;
        int m, n, n2;
	float b, n1, d;
	uint16_t *c;
        if (clock == 0) return 25175000.0;
        if (clock == 1) return 28322000.0;
        clock ^= 1; /*Clocks 2 and 3 seem to be reversed*/
	c = (uint16_t *) &ramdac->regs[0x20 + (clock << 1)];
        m  =  (*c & 0xff) + 2;		/* B+2 */
        n  = ((*c >>  8) & 0x1f) + 2;	/* N1+2 */
        n2 = ((*c >> 13) & 0x07);	/* D */
	b  = (float) m;
	n1 = (float) n;
	d = (float) (1 << n2);
	t = (14318184.0f * b / d) / n1;
        return t;
}
