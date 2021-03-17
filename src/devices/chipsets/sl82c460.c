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
 * Version:	@(#)sl82c460.c	1.0.3	2021/03/16
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *
 *		Copyright 2020 Altheos.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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


static void
sl82c460_shadow_set(uint32_t base, uint32_t size, int state)
{
    switch (state) {
	case 0:
		mem_set_mem_state(base, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 1:
		mem_set_mem_state(base, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
		break;

	case 2:
#if 0
		mem_set_mem_state(base, size,
				  MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
#endif
		mem_set_mem_state(base, size,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		break;

#if 0
	case 3: /* I never found this case true */
		mem_set_mem_state(base, size,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		break;
#endif
    }
}


static void 
shadow_recalc(sl82c460_t *dev)
{
    shadowbios = 0;
    shadowbios_write = 0;

    if (dev->regs[0x31] & 0x80) {
	shadowbios = 0;
	shadowbios_write = 1;
	mem_set_mem_state(0xf0000, 0X10000,
			  MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
    } else {
	shadowbios = 1;
	shadowbios_write = 0;
	mem_set_mem_state(0xf0000, 0X10000,
			  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
    }

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
sl82c460_out(uint16_t port, uint8_t val, priv_t priv)
{
    sl82c460_t *dev = (sl82c460_t *)priv;

    if (port & 1) {
	dev->regs[dev->reg_idx] = val;

	switch (dev->reg_idx) {
		case 0X2e: case 0x2f: case 0x30: case 0x31:
			shadow_recalc(dev);
			break;
	}
    } else
	dev->reg_idx = val;
}


static uint8_t 
sl82c460_in(uint16_t port, priv_t priv)
{
    sl82c460_t *dev = (sl82c460_t *)priv;

    if (port & 1)
	return(dev->regs[dev->reg_idx]);

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
	
    io_sethandler(0x00a8, 2,
		  sl82c460_in,NULL,NULL, sl82c460_out,NULL,NULL, dev);	

    return(dev);
}


const device_t sl82c460_device = {
    "SL 82C460",
    0,
    0,
    NULL,
    sl82c460_init, sl82c460_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
