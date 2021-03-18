/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the APM handler.
 *
 * Version:	@(#)apm.c	1.0.3	2021/03/18
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2019-2020 Miran Grca.
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
#include <string.h>
#include <wchar.h>
#include "../../cpu/cpu.h"
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "../../plat.h"
#include "apm.h"


typedef struct {
    uint8_t cmd,
	    stat;
} apm_t;


static uint8_t
apm_in(uint16_t port, priv_t priv)
{
    apm_t *dev = (apm_t *)priv;

    port &= 0x0001;

    if (port == 0x0000)
	return dev->cmd;
 
    return dev->stat;
}


static void
apm_out(uint16_t port, uint8_t val, priv_t priv)
{
    apm_t *dev = (apm_t *)priv;

    DEBUG("[%04X:%08X] APM write: %04X = %02X (BX = %04X, CX = %04X)\n", CS, cpu_state.pc, port, val, BX, CX);

    port &= 0x0001;

    if (port == 0x0000) {
	dev->cmd = val;

	switch (dev->cmd) {
		case 0x07:			/* Set Power State */
			if (CH != 0x00)
				break;
			switch (CX) {
				case 0x0000:
					INFO("APM: Set Power State: APM Enabled\n");
					break;

				case 0x0001:
					INFO("APM: Set Power State: Standby\n");
					break;

				case 0x0002:
					INFO("APM: Set Power State: Suspend\n");
					break;

				case 0x0003:	/* Off */
					INFO("APM: Set Power State: Off\n");
					break;
			}
			break;
	}
    } else
	dev->stat = val;
}


static void
apm_close(priv_t priv)
{
    apm_t *dev = (apm_t *)priv;

    free(dev);
}


static priv_t
apm_init(UNUSED(const device_t *info), UNUSED(void *parent))
{
    apm_t *dev;
    
    dev = (apm_t *)mem_alloc(sizeof(apm_t));
    memset(dev, 0, sizeof(apm_t));
    
    io_sethandler(0x00b2, 2,
		  apm_in,NULL,NULL, apm_out,NULL,NULL, dev);

    return(dev);
}


const device_t apm_device = {
    "Advanced Power Management",
    0,
    0,
    NULL,
    apm_init,  apm_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
