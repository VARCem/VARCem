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
 * Version:	@(#)apm.c	1.0.0	2020/12/15
 *
 * Authors:	
 *		Miran Grca, <mgrca8@gmail.com>
 *      Altheos
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
#include "../../plat.h"
#include "../../cpu/cpu.h"
#include "../../emu.h"
#include "../../io.h"
#include "../../device.h"
#include "apm.h"

#ifdef ENABLE_APM_LOG
int apm_do_log = ENABLE_APM_LOG;
#endif

typedef struct
{
    uint8_t cmd,
	    stat;
} apm_t;


static void
apm_out(uint16_t port, uint8_t val, void *p)
{
    apm_t *apm = (apm_t *) p;

    DEBUG("[%04X:%08X] APM write: %04X = %02X (BX = %04X, CX = %04X)\n", CS, cpu_state.pc, port, val, BX, CX);

    port &= 0x0001;

    if (port == 0x0000) {
	    apm->cmd = val;

	        switch (apm->cmd) {
		        case 0x07:			/* Set Power State */
			        if (CH == 0x00)
                        switch (CX) {
		        		    case 0x0000:
#ifdef ENABLE_APM_LOG
				        	    DEBUG("APM Set Power State: APM Enabled\n");
#endif
					            break;
				            case 0x0001:
#ifdef ENABLE_APM_LOG
    					        DEBUG("APM Set Power State: Standby\n");
#endif
					            break;
				            case 0x0002:
#ifdef ENABLE_APM_LOG
					            DEBUG("APM Set Power State: Suspend\n");
#endif
	        				    break;
			        	    case 0x0003:	/* Off */
#ifdef ENABLE_APM_LOG
					            DEBUG("APM Set Power State: Off\n");
#endif
				        	    exit(-1);
					            break;
			            }
			        break;
	        }
    } else
	apm->stat = val;
}


static uint8_t
apm_in(uint16_t port, void *p)
{
    apm_t *apm = (apm_t *) p;

    //apm_log("[%04X:%08X] APM read: %04X = FF\n", CS, cpu_state.pc, port);

    port &= 0x0001;

    if (port == 0x0000)
	    return apm->cmd;
    else
	    return apm->stat;
}


static void
apm_close(void *p)
{
    apm_t *dev = (apm_t *)p;

    free(dev);
}


static priv_t
apm_init(const device_t *info, UNUSED(void *parent))
{
    apm_t *dev;
    
    dev = (apm_t *) malloc(sizeof(apm_t));
    memset(dev, 0, sizeof(apm_t));

    io_sethandler(0x00b2, 0x0002, apm_in, NULL, NULL, apm_out, NULL, NULL, dev);

    return((priv_t)dev);
}


const device_t apm_device =
{
    "Advanced Power Management",
    0,
    0,
    NULL,
    apm_init,  apm_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
