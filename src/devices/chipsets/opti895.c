/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Opti 82C895/802G chipset.
 *
 * Version:	@(#)opti895.c	1.0.2	2021/06/26
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *		Tiseno100, ?
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020 Fred N. van Kempen.
 *		Copyright 2020 Miran Grca.
 *		Copyright 2020 Tiseno100.
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../floppy/fdd.h"
#include "../floppy/fdc.h"
#include "../system/port92.h"
#include "opti895.h"


typedef struct {
    int		type;

    uint8_t	indx,
		forced_green,
	    	regs[0x100];
#if 0
    uint8_t    scratch[2];
#endif
} opti895_t;


static void
shadow_recalc(opti895_t *dev)
{
    uint32_t base;
    uint32_t i, shflags = 0;

    shadowbios = 0;
    shadowbios_write = 0;

    if (dev->regs[0x22] & 0x80) {
	shadowbios = 1;
	shadowbios_write = 0;
	shflags = MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL;
    } else {
	shadowbios = 0;
	shadowbios_write = 1;
	shflags = MEM_READ_INTERNAL | MEM_WRITE_DISABLED;
    }

    mem_set_mem_state(0xf0000, 0x10000, shflags);

    for (i = 0; i < 8; i++) {
	base = 0xd0000 + (i << 14);

	if (dev->regs[0x23] & (1 << i)) {
	    shflags = MEM_READ_INTERNAL;
		    shflags |= (dev->regs[0x22] & ((base >= 0xe0000) ? 0x08 : 0x10)) ? MEM_WRITE_DISABLED : MEM_WRITE_INTERNAL;
	    } else {
		    if (dev->regs[0x26] & 0x40) {
			    shflags = MEM_READ_EXTERNAL;
			    shflags |= (dev->regs[0x22] & ((base >= 0xe0000) ? 0x08 : 0x10)) ? MEM_WRITE_DISABLED : MEM_WRITE_INTERNAL;
		    } else
			    shflags = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
	    }

	    mem_set_mem_state(base, 0x4000, shflags);
    }

    for (i = 0; i < 4; i++) {
	    base = 0xc0000 + (i << 14);

	    if (dev->regs[0x26] & (1 << i)) {
		    shflags = MEM_READ_INTERNAL;
		    shflags |= (dev->regs[0x26] & 0x20) ? MEM_WRITE_DISABLED : MEM_WRITE_INTERNAL;
	    } else {
		    if (dev->regs[0x26] & 0x40) {
			    shflags = MEM_READ_EXTERNAL;
			    shflags |= (dev->regs[0x26] & 0x20) ? MEM_WRITE_DISABLED : MEM_WRITE_INTERNAL;
		    } else
			shflags = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
	    }

	    mem_set_mem_state(base, 0x4000, shflags);
    }

    flushmmucache();
}


static uint8_t
opti895_in(uint16_t port, priv_t priv)
{
    opti895_t *dev = (opti895_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x23:
		if (dev->indx == 0x01)
			ret = dev->regs[dev->indx];
	    	break;

	case 0x24:
		if (((dev->indx >= 0x20) && (dev->indx <= 0x2f)) ||
		    ((dev->indx >= 0xe0) && (dev->indx <= 0xef))) {
			ret = dev->regs[dev->indx];

			if (dev->indx == 0xee)
				ret = (ret & 0x7f);
		}
		else {
			DEBUG("Out of Range 895 register read %X\n", dev->regs[dev->indx]);
			ret = dev->regs[dev->indx];
		}      
		break;
    }

    return(ret);
}


static void
opti895_out(uint16_t port, uint8_t val, priv_t priv)
{
    opti895_t *dev = (opti895_t *)priv;

    switch (port) {
	    case 0x22:
		    dev->indx = val;
		    break;
	case 0x23:
		if (dev->indx == 0x01)
			dev->regs[dev->indx] = val;
		break;

	case 0x24:
		if (((dev->indx >= 0x20) && (dev->indx <= 0x2f)) ||
		    ((dev->indx >= 0xe0) && (dev->indx <= 0xef))) {

			switch (dev->indx) {
				case 0x21:
					dev->regs[dev->indx] = val & 0xbf;
					cpu_cache_ext_enabled = !!(val & 0x10);
					cpu_update_waitstates();
					break;

				case 0x22: case 0x23: case 0x26:
					dev->regs[dev->indx] = val;
					shadow_recalc(dev);
					break;

				case 0x24:
					dev->regs[dev->indx] |= (val & 0x80);
					//smram_state_change(dev->smram, 0, !!(val & 0x80));
					break;

				case 0x27:
					dev->regs[dev->indx] = val & 0xfe;
					break;

				case 0x2c:
					return;

				case 0xe0:
					dev->regs[dev->indx] = val;
					if (!(val & 0x01))
						dev->forced_green = 0;
					break;

				case 0xe1:
					dev->regs[dev->indx] = val;
					if ((val & 0x08) && (dev->regs[0xe0] & 0x01))
						dev->forced_green = 1;
					break;

				default:
					dev->regs[dev->indx] = val;
					break;
			}
		}
		else {
			DEBUG("Out of Range 895 register write %X, %X\n", dev->regs[dev->indx], val);
			dev->regs[dev->indx] = val;
		}
		break;
    }
}


static void
opti895_close(priv_t priv)
{
    opti895_t *dev = (opti895_t *)priv;

    free(dev);
}


static priv_t
opti895_init(const device_t *info, UNUSED(void *parent))
{
    opti895_t *dev;

    dev = (opti895_t *)mem_alloc(sizeof(opti895_t));
    memset(dev, 0x00, sizeof(opti895_t));
    dev->type = info->local;

    device_add(&port92_device);

#if 0
    dev->scratch[0] = dev->scratch[1] = 0xff;
#endif

    dev->regs[0x22] = 0xe4;
    dev->regs[0x25] = 0x7c;
    dev->regs[0x26] = 0x10;
    dev->regs[0x27] = 0xde;
    dev->regs[0x28] = 0xf8;
    dev->regs[0x29] = 0x10;
    dev->regs[0x2a] = 0xe0;
    dev->regs[0x2b] = 0x10;
    dev->regs[0x2d] = 0xc0;
    dev->regs[0xe8] = 0x08;
    dev->regs[0xe9] = 0x08;
    dev->regs[0xeb] = 0xff;
    dev->regs[0xef] = 0x41;

    switch (mem_size >> 10) {
    	case 2:
		dev->regs[0x24] =  0x0;
		break;
	case 3:	case 4:
		dev->regs[0x24] =  0x02;
		break;	
	case 5:
		dev->regs[0x24] = 0x53;
		break;
	case 6:
		dev->regs[0x24] = 0x47;
		break;
	case 8 :
		dev->regs[0x24] = 0x04;
		break;
	case 10:
		dev->regs[0x24] = 0x06;
		break;
	case 12:
		dev->regs[0x24] = 0x15;
		break;
	case 16:
		dev->regs[0x24] = 0x11;
		break;	
	case 17:
		dev->regs[0x24] = 0x54;
		break;
	case 20:
		dev->regs[0x24] = 0x55;
		break;
	case 24:
		dev->regs[0x24] = 0x22;
		break;
	case 32:
		dev->regs[0x24] = 0x12;
		break;
	case 64:
		dev->regs[0x24] = 0x14;
		break;
	case 128:
		dev->regs[0x24] = 0x46;
		break;
	default:
		dev->regs[0x24] = 0;
};

    io_sethandler(0x0022, 3,
		  opti895_in,NULL,NULL, opti895_out,NULL,NULL, dev);
#if 0
    // Note : don't know if these registers are part of 895 */
    io_sethandler(0x00e1, 2,
		  opti895_in,NULL,NULL, opti895_out,NULL,NULL, dev);
#endif

    shadow_recalc(dev);

    return(dev);
}


const device_t opti895_device = {
    "Opti 82C895",
    0,
    0,
    NULL,
    opti895_init, opti895_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
