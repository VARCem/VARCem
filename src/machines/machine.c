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
 * Version:	@(#)machine.c	1.0.22	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include "../config.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/system/dma.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/ports/game.h"
#include "../devices/ports/serial.h"
#include "../devices/ports/parallel.h"
#include "../plat.h"
#include "../ui/ui.h"
#include "machine.h"


static const machine_t	*machine = NULL;


/* Get the machine definition for the selected machine. */
const device_t *
machine_load(void)
{
    const device_t *dev;

    dev = machine_get_device_ex(config.machine_type);
    if (dev != NULL)
	machine = (machine_t *)dev->mach_info;

    return(dev);
}


void
machine_reset(void)
{
    wchar_t temp[1024];
    const device_t *dev;
    romdef_t roms;

    INFO("Initializing as \"%s\"\n", machine_get_name());

    /* Initialize the configured machine and CPU. */
    dev = machine_load();

    /* Set up the selected CPU at default speed. */
    cpu_set_type(machine->cpu[config.cpu_manuf].cpus,
		 config.cpu_manuf, config.cpu_type,
		 config.enable_ext_fpu, config.cpu_use_dynarec);

    /* Start with (max/turbo) speed. */
    pc_set_speed(1);

    /* Set up the architecture flags. */
    AT = IS_ARCH(MACHINE_AT);
    MCA = IS_ARCH(MACHINE_MCA);
    PCI = IS_ARCH(MACHINE_PCI);

    /* Reset memory and the MMU. */
    mem_reset();
    rom_reset();

    /* Disable shadowing for now. */
    shadowbios = 0;

    /* Load the machine's ROM BIOS. */
    plat_append_filename(temp, MACHINES_PATH, dev->path);
    if (! rom_load_bios(&roms, temp, 0)) {
	/*
	 * The machine's ROM BIOS could not be loaded.
	 *
	 * This is kinda fatal, inform the user and
	 * bail out, since continuing is pointless.
	 */
	ui_msgbox(MBX_ERROR|MBX_FATAL, get_string(IDS_ERR_NOBIOS));

	return;
    }

    /* Activate the ROM BIOS. */
    rom_add_bios();

    /* All good, boot the machine! */
    (void)dev->init(dev, &roms);
}


/* Return the current machine's flags. */
int
machine_get_flags(void)
{
    int ret = 0;

    if (machine != NULL)
	ret = machine->flags;

    return(ret);
}


/* Return the current machine's 'fixed devices' flags. */
int
machine_get_flags_fixed(void)
{
    int ret = 0;

    if (machine != NULL)
	ret = machine->flags_fixed;

    return(ret);
}


/* Return a machine's maximum memory size. */
int
machine_get_maxmem(void)
{
    int ret = 0;

    if (machine != NULL)
	ret = machine->max_ram;

    return(ret);
}


/* Return a machine's NVR size. */
int
machine_get_nvrsize(void)
{
    int ret = 0;

    if (machine != NULL)
	ret = machine->nvrsz;

    return(ret);
}


/* Return a machine's (adjusted) memory size. */
int
machine_get_memsize(int mem)
{
    int k = 0;

    if (machine != NULL) {
	k = machine->min_ram;
	if ((machine->flags & MACHINE_AT) && (machine->ram_granularity < 128))
		k *= 1024;
    }

    if (mem < k)
        mem = k;

    if (mem > 1048576)
        mem = 1048576;

    return(mem);
}


/* Return this machine's default or slow speed. */
uint32_t
machine_get_speed(int turbo)
{
    uint32_t k;
    int mhz;

    /* Get the current CPU's maximum speed. */
    k = cpu_get_speed();

    /*
     * If not on turbo, use the speed the mainboard
     * has set as its fallback ("slow") speed.
     */
    if (! turbo) {
	mhz = machine->slow_mhz;
	switch(mhz) {
		case -1:	/* machine has no slow/turbo switching */
			break;

		case 0:		/* use half of current max speed */
			k >>= 1;
			break;

		default:	/* use specified speed */
			k = (mhz << 20);
			break;
	}
    }

    return(k);
}


/* Check if the machine's ROM files are present. */
int
machine_available(int id)
{
    wchar_t temp[1024];
    const device_t *dev;
    romdef_t r;
    int i;

    dev = machine_get_device_ex(config.machine_type);

    wcscpy(temp, MACHINES_PATH);
    plat_append_slash(temp);
    wcscat(temp, dev->path);
    i = rom_load_bios(&r, temp, 1);

    return(i);
}


void
machine_common_init(void)
{
    /* System devices first. */
    pic_init();
    dma_init();
    pit_init();

    if (config.game_enabled)
	device_add(&game_device);

    if (config.parallel_enabled[0])
	device_add(&parallel_1_device);
    if (config.parallel_enabled[1])
	device_add(&parallel_2_device);
    if (config.parallel_enabled[2])
	device_add(&parallel_3_device);

    if (config.serial_enabled[0])
	device_add(&serial_1_device);
    if (config.serial_enabled[1])
	device_add(&serial_2_device);
}
