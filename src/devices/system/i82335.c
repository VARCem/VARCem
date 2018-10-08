/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Intel 82335 SX emulation, used by the Phoenix 386 clone.
 *
 * Version:	@(#)i82335.c	1.0.5	2018/10/07
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
#include "../../io.h"
#include "../../mem.h"
#include "../../plat.h"


typedef struct {
    uint8_t reg_22;
    uint8_t reg_23;
} i82335_t;


static i82335_t i82335;


static uint8_t
i82335_read(uint16_t addr, UNUSED(void *priv))
{
    DBGLOG(1, "i82335_read(%04X)\n", addr);
    if (addr == 0x22)
	return(i82335.reg_22);

    if (addr == 0x23)
	return(i82335.reg_23);

    return(0);
}


static void
i82335_write(uint16_t addr, uint8_t val, UNUSED(void *priv))
{
    int mem_write = 0;
    int i = 0;

    DBGLOG(1, "i82335_write(%04X, %02X)\n", addr, val);

    switch (addr) {
	case 0x22:
		if ((val ^ i82335.reg_22) & 1) {
			if (val & 1) {
				for (i = 0; i < 8; i++)
					mem_set_mem_state(0xe0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
				shadowbios = 1;
			} else {
				for (i = 0; i < 8; i++)
					mem_set_mem_state(0xe0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
				shadowbios = 0;
			}

			flushmmucache();
		}

		i82335.reg_22 = val | 0xd8;
		break;

	case 0x23:
		i82335.reg_23 = val;

		if ((val ^ i82335.reg_22) & 2) {
			if (val & 2) {
				for (i = 0; i < 8; i++)
					mem_set_mem_state(0xc0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
				shadowbios = 1;
			} else {
				for (i = 0; i < 8; i++)
					mem_set_mem_state(0xc0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
				shadowbios = 0;
			}
		}

		if ((val ^ i82335.reg_22) & 0xc) {
			if (val & 2) {
				for (i = 0; i < 8; i++) {
					mem_write = (val & 8) ? MEM_WRITE_DISABLED : MEM_WRITE_INTERNAL;
					mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | mem_write);
				}
				shadowbios = 1;
			} else {
				for (i = 0; i < 8; i++) {
					mem_write = (val & 8) ? MEM_WRITE_DISABLED : MEM_WRITE_EXTERNAL;
					mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | mem_write);
				}
				shadowbios = 0;
			}
		}

		if ((val ^ i82335.reg_22) & 0xe)
			flushmmucache();

		if (val & 0x80) {
			io_removehandler(0x0022, 1,
					 i82335_read,NULL,NULL,
					 i82335_write,NULL,NULL, NULL);
		}
		break;
    }
}


void
i82335_init(void)
{
    memset(&i82335, 0x00, sizeof(i82335_t));

    i82335.reg_22 = 0xd8;

    io_sethandler(0x0022, 20,
		  i82335_read,NULL,NULL, i82335_write,NULL,NULL, NULL);
}
