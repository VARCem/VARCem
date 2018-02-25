/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of the emulated machines.
 *
 * Version:	@(#)machine.c	1.0.2	2018/02/24
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


#define PATH_ROM_BIOS	"roms/machines"


int	romset;
int	machine;
int	AT, PCI;


void
machine_init(void)
{
    char temp[1024];
    int i;

    pclog("Initializing as \"%s\"\n", machine_getname());

    ide_set_bus_master(NULL, NULL, NULL);

    /* Set up the architecture flags. */
    AT = IS_ARCH(machine, MACHINE_AT);
    PCI = IS_ARCH(machine, MACHINE_PCI);

    /* Load the machine's ROM BIOS. */
#if 0
    strcpy(temp, PATH_ROM_BIOS);
    i = strlen(temp);
#ifdef _WIN32
    temp[i++] = '\\';
#else
    temp[i++] = '/';
#endif
    strcpy(&temp[i], machines[machine].bios_path);
    rom_load_bios(temp);
#else
    rom_load_bios(romset);
#endif

    /* Activate the ROM BIOS. */
    mem_add_bios();

    /* All good, boot the machine! */
    machines[machine].init(&machines[machine]);
}


void
machine_common_init(machine_t *model)
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
