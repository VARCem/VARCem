/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of the emulated machines.
 *
 * Version:	@(#)machine.c	1.0.10	2018/03/21
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
#include "../emu.h"
#include "../device.h"
#include "../dma.h"
#include "../pic.h"
#include "../pit.h"
#include "../mem.h"
#include "../rom.h"
#include "../lpt.h"
#include "../serial.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../plat.h"
#include "machine.h"


int	romset;
int	machine;
int	AT, PCI;
int     romspresent[ROM_MAX];


void
machine_init(void)
{
    wchar_t temp[1024];
    romdef_t r;

    pclog("Initializing as \"%s\"\n", machine_getname());

    /* Set up the architecture flags. */
    AT = IS_ARCH(machine, MACHINE_AT);
    PCI = IS_ARCH(machine, MACHINE_PCI);

    /* Reset memory and the MMU. */
    mem_reset();

    /* Load the machine's ROM BIOS. */
    wcscpy(temp, MACHINES_PATH);
    plat_append_slash(temp);
    wcscat(temp, machines[machine].bios_path);
    if (! rom_load_bios(&r, temp, 0)) return;

    /* Activate the ROM BIOS. */
    mem_add_bios();

    /* Reset the IDE bus master pointers. */
    ide_set_bus_master(NULL, NULL, NULL);

    /* All good, boot the machine! */
    machines[machine].init(&machines[machine], &r);
}


/* Close down the machine (saving stuff, etc.) */
void
machine_close(void)
{
    if (machines[machine].close != NULL)
	machines[machine].close();
}


/* Check if the machine's ROM files are present. */
int
machine_available(int id)
{
    wchar_t temp[1024];
    romdef_t r;
    int i;

    wcscpy(temp, MACHINES_PATH);
    plat_append_slash(temp);
    wcscat(temp, machines[id].bios_path);
    i = rom_load_bios(&r, temp, 1);

    return(i);
}


/* Check for the availability of all the defined machines. */
int
machine_detect(void)
{
    int c, i;

    pclog("Scanning for ROM images:\n");

    c = 0;
    for (i=0; i<ROM_MAX; i++) {
	romspresent[i] = machine_available(i);
	c += romspresent[i];
    }

    if (c == 0) {
	/* No usable ROMs found, aborting. */
	pclog("No usable machine has been found!\n");
	return(0);
    }

    pclog("A total of %d machines are available.\n", c);

    return(c);
}


void
machine_common_init(const machine_t *model, UNUSED(void *arg))
{
    /* System devices first. */
    dma_init();
    pic_init();
    pit_init();

    if (lpt_enabled)
	lpt_init();

    if (serial_enabled[0])
	serial_setup(1, SERIAL1_ADDR, SERIAL1_IRQ);

    if (serial_enabled[1])
	serial_setup(2, SERIAL2_ADDR, SERIAL2_IRQ);
}
