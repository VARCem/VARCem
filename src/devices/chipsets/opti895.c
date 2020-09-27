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
 *
 * Version:	@(#)opti895.c	1.0.0	2020/09/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *          Altheos
 *          Tiseno100
 *		    Miran Grca, <mgrca8@gmail.com>
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

    uint8_t		indx, forced_green,
            	regs[0x100];
    //uint8_t    scratch[2];
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

    mem_set_mem_state(0xf0000, 0X10000, shflags);

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

static void
opti895_write(uint16_t addr, uint8_t val, priv_t priv)
{
    opti895_t *dev = (opti895_t *)priv;

    switch (addr) {
	    case 0x22:
		    dev->indx = val;
		    break;
        case 0x23:
            if (dev->indx == 0x01)
                dev->regs[dev->indx] = val;
            break;
	    case 0x24:
		    if (((dev->indx >= 0x20) && (dev->indx <= 0x2C)) ||
                ((dev->indx >= 0xe0) && (dev->indx <= 0xef))) {
		    	    dev->regs[dev->indx] = val;
			    
                switch (dev->indx) {
                    case 0x21:
			    	    cpu_cache_ext_enabled = !!(val & 0x10);
		    		    cpu_update_waitstates();
                        break;
                    case 0x22: case 0x23: case 0x26:
                        shadow_recalc(dev);
                        break;
                    
                    case 0xe0:
                        if (!(val & 0x01))
                            dev->forced_green = 0;
                        break;

                    case 0xe1:
                        if ((val & 0x08) && (dev->regs[0xe0] & 0x01)) {
                            dev->forced_green = 1;
                        }
                        break;
			    }
		    }
		    break;
    }
}


static uint8_t
opti895_read(uint16_t addr, priv_t priv)
{
    opti895_t *dev = (opti895_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
        case 0x23:
            if (dev->indx == 0x01)
                ret = dev->regs[dev->indx];
            break;
	    case 0x24:
		    if (((dev->indx >= 0x20) && (dev->indx <= 0x2c)) ||
                ((dev->indx >= 0xe0) && (dev->indx <= 0xef))) {                 
			    ret = dev->regs[dev->indx];
                if (dev->indx == 0xee)
                    ret = (ret & 0x7f);
            }      
		    break;
    }

    return(ret);
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

    //dev->scratch[0] = dev->scratch[1] = 0xff;

    dev->regs[0x22] = 0xE4;
	dev->regs[0x25] = 0x7C;
	dev->regs[0x26] = 0x10;
	dev->regs[0x27] = 0xDE;
	dev->regs[0x28] = 0xF8;
	dev->regs[0x29] = 0x10;
	dev->regs[0x2A] = 0xE0;
	dev->regs[0x2B] = 0x10;
	dev->regs[0x2D] = 0xC0;
	dev->regs[0xe8] = 0x08;
	dev->regs[0xe9] = 0x08;
	dev->regs[0xef] = 0xFF;
	dev->regs[0xef] = 0x41;

    io_sethandler(0x0022, 0x003,
		  opti895_read,NULL,NULL, opti895_write,NULL,NULL, dev);
    /* io_sethandler(0x00e1, 0x002,
		  opti895_read,NULL,NULL, opti895_write,NULL,NULL, dev); // Note : don't know if these registers are part of 895 */

    shadow_recalc(dev);

    return((priv_t)dev);
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
