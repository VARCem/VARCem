/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		IBM PC/AT RTC/NVRAM ("CMOS") emulation.
 *
 *		The original PC/AT series had DS12885 series modules; later
 *		versions and clones used the 12886 and/or 1288(C)7 series,
 *		or the MC146818 series, all with an external battery. Many
 *		of those batteries would create corrosion issues later on
 *		in mainboard life...
 *
 * Version:	@(#)nvr_at.c	1.0.2	2018/03/04
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Mahod,
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
#include <stdlib.h>
#include <wchar.h>
#include "cpu/cpu.h"
#include "io.h"
#include "device.h"
#include "machine/machine.h"
#include "mem.h"
#include "nmi.h"
#include "nvr.h"
#include "rom.h"


static nvr_t *nvrp;


static void
nvr_write(uint16_t addr, uint8_t val, void *priv)
{
    nvr_t *nvr = (nvr_t *)priv;

    cycles -= ISA_CYCLES(8);

    if (! (addr & 1)) {
	nvr->addr = (val & nvr->mask);
	if (!(machines[machine].flags & MACHINE_MCA) && (romset != ROM_IBMPS1_2133))
		nmi_mask = (~val & 0x80);

	return;
    }

    /* Write the chip's registers. */
    (*nvr->set)(nvr, nvr->addr, val);
}


static uint8_t
nvr_read(uint16_t addr, void *priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t ret;

    cycles -= ISA_CYCLES(8);

    if (addr & 1) {
	/* Read from the chip's registers. */
	ret = (*nvr->get)(nvr, nvr->addr);
    } else {
	ret = nvr->addr;
    }

    return(ret);
}


void
nvr_at_close(void)
{
    if (nvrp == NULL)
	return;

    if (nvrp->fname != NULL)
	free(nvrp->fname);

    free(nvrp);

    nvrp = NULL;
}


void
nvr_at_init(int64_t irq)
{
    nvr_t *nvr;

    /* Allocate an NVR for this machine. */
    nvr = (nvr_t *)malloc(sizeof(nvr_t));
    if (nvr == NULL) return;
    memset(nvr, 0x00, sizeof(nvr_t));

    /* This is machine specific. */
    nvr->mask = machines[machine].nvrmask;
    nvr->irq = irq;

    /* Set up any local handlers here. */

    /* Initialize the actual NVR. */
    nvr_init(nvr);

    /* Set up the PC/AT handler for this device. */
    io_sethandler(0x0070, 2,
		  nvr_read, NULL, NULL, nvr_write, NULL, NULL, nvr);

    /* Load the NVR into memory! */
    (void)nvr_load();

    nvrp = nvr;
}
