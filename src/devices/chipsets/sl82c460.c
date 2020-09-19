/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Symphony 'Haydn II' chipset (SL82C460).
 *
 * Version:	@(#)sl82c460.c	1.0.1	2020/09/20
 *
 * Authors:
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *      Altheos
 *
 *		Copyright 2020 Sarah Walker, Altheos.
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "sl82c460.h"


typedef struct {
    int		reg_idx;
    uint8_t	regs[256];
} sl82c460_t;

#ifdef _LOGGING
void
sl82c460_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SL_LOG
    va_list ap;

    //if (sl_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    //}
# endif
}
#endif

static void sl82c460_shadow_set(uint32_t base, uint32_t size, int state)
{
        switch (state)
        {
                case 0:
                mem_set_mem_state(base, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 1:
                mem_set_mem_state(base, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 2:
                //mem_set_mem_state(base, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                mem_set_mem_state(base, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
                //case 3: /* I never found this case true */
                //mem_set_mem_state(base, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                //break;
        }

}

static void 
shadow_recalc(sl82c460_t *dev)
{
    uint32_t base;
    uint32_t i, shflags = 0;

    shadowbios = 0;
    shadowbios_write = 0;

    if (dev->regs[0x31] & 0x80) {
        shadowbios = 0;
        shadowbios_write = 1;
        shflags = MEM_READ_INTERNAL | MEM_WRITE_DISABLED;
    } else {
        shadowbios = 1;
        shadowbios_write = 0;
        shflags = MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL;
    }
    mem_set_mem_state(0xf0000, 0X10000, shflags);

    sl82c460_shadow_set(0xc0000, 0x4000, dev->regs[0x2e] & 3);
    sl82c460_shadow_set(0xc4000, 0x4000, ((dev->regs[0x2e] >> 2) & 3));
    sl82c460_shadow_set(0xc8000, 0x4000, ((dev->regs[0x2e] >> 4) & 3));
    sl82c460_shadow_set(0xcc000, 0x4000, ((dev->regs[0x2e] >> 6) & 3));
    sl82c460_shadow_set(0xd0000, 0x4000, dev->regs[0x2f] & 3);
    sl82c460_shadow_set(0xd4000, 0x4000, ((dev->regs[0x2f] >> 2) & 3));
    sl82c460_shadow_set(0xd8000, 0x4000, ((dev->regs[0x2f] >> 4) & 3));
    sl82c460_shadow_set(0xdc000, 0x4000, ((dev->regs[0x2f] >> 6) & 3));
    sl82c460_shadow_set(0xe0000, 0x10000, ((dev->regs[0x30] >> 6) & 3));
}


static void 
sl82c460_write(uint16_t addr, uint8_t val, priv_t priv)
{
    sl82c460_t *dev = (sl82c460_t *)priv;

    if (addr & 1) {
	    dev->regs[dev->reg_idx] = val;
        sl82c460_log(0, "Register %x write %x\n", dev->reg_idx, val);

	    switch (dev->reg_idx) {
	    	case 0X2e: case 0x2f: case 0x30: case 0x31:
		    	shadow_recalc(dev);
		    	break;
	    }
    } else
	    dev->reg_idx = val;

}


static uint8_t 
sl82c460_read(uint16_t addr, priv_t priv)
{
    sl82c460_t *dev = (sl82c460_t *)priv;

  if (addr & 1)
    {
	    sl82c460_log(0, "Register %x read %x\n", dev->reg_idx, dev->regs[dev->reg_idx]);
        return(dev->regs[dev->reg_idx]);
    }

    return(dev->reg_idx);
}


static void
sl82c460_close(priv_t priv)
{
    sl82c460_t *dev = (sl82c460_t *)priv;

    free(dev);
}


static priv_t
sl82c460_init(const device_t *info, UNUSED(void *parent))
{
    sl82c460_t *dev;

    dev = (sl82c460_t *)mem_alloc(sizeof(sl82c460_t));
    memset(dev, 0x00, sizeof(sl82c460_t));
	
    io_sethandler(0x00a8, 0x0002,
		  sl82c460_read,NULL,NULL, sl82c460_write,NULL,NULL, dev);	

    return((priv_t)dev);
}


const device_t sl82c460_device = {
    "SL 82C460",
    0, 0, NULL,
    sl82c460_init, sl82c460_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
