/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ICD2061 clock generator emulation.
 *
 *		Used by ET4000w32/p (Diamond Stealth 32)
 *
 * Version:	@(#)vid_icd2061.c	1.0.2	2018/05/06
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
#include "vid_icd2061.h"


void icd2061_write(icd2061_t *icd2061, int val)
{
        int q, p, m, a;
        if ((val & 1) && !(icd2061->state & 1))
        {
                if (!icd2061->status)
                {
                        if (val & 2) 
                                icd2061->unlock++;
                        else         
                        {
                                if (icd2061->unlock >= 5)
                                {
                                        icd2061->status = 1;
                                        icd2061->pos = 0;
                                }
                                else
                                   icd2061->unlock = 0;
                        }
                }
                else if (val & 1)
                {
                        icd2061->data = (icd2061->data >> 1) | (((val & 2) ? 1 : 0) << 24);
                        icd2061->pos++;
                        if (icd2061->pos == 26)
                        {
                                a = (icd2061->data >> 21) & 0x7;
                                if (!(a & 4))
                                {
                                        q = (icd2061->data & 0x7f) - 2;
                                        m = 1 << ((icd2061->data >> 7) & 0x7);
                                        p = ((icd2061->data >> 10) & 0x7f) - 3;
                                        if (icd2061->ctrl & (1 << a))
                                           p <<= 1;
                                        icd2061->freq[a] = ((double)p / (double)q) * 2.0 * 14318184.0 / (double)m;
                                }
                                else if (a == 6)
                                {
                                        icd2061->ctrl = val;
                                }
                                icd2061->unlock = icd2061->data = 0;
                                icd2061->status = 0;
                        }
                }
        }
        icd2061->state = val;
}

double icd2061_getfreq(icd2061_t *icd2061, int i)
{
        return icd2061->freq[i];
}
