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
 * Version:	@(#)machine.c	1.0.19	2019/02/16
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
#include "../devices/video/video.h"
#include "../plat.h"
#include "../ui/ui.h"
#include "machine.h"


int	AT, MCA, PCI;


void
machine_reset(void)
{
    wchar_t temp[1024];
    romdef_t r;

    INFO("Initializing as \"%s\"\n", machine_getname());

    /* Set up the architecture flags. */
    AT = IS_ARCH(machine, MACHINE_AT);
    PCI = IS_ARCH(machine, MACHINE_PCI);
    MCA = IS_ARCH(machine, MACHINE_MCA);

    /* Reset memory and the MMU. */
    mem_reset();

    /* Load the machine's ROM BIOS. */
    wcscpy(temp, MACHINES_PATH);
    plat_append_slash(temp);
    wcscat(temp, machines[machine].bios_path);
    if (! rom_load_bios(&r, temp, 0)) {
	/*
	 * The machine's ROM BIOS could not be loaded.
	 *
	 * Since this is kinda fatal, we inform the user
	 * and bail out, since continuing is pointless.
	 */
	ui_msgbox(MBX_ERROR|MBX_FATAL, get_string(IDS_ERR_NOBIOS));

	return;
    }

    /* Activate the ROM BIOS. */
    mem_add_bios();

    /*
     * If this is not a PCI or MCA machine, reset the video
     * card before initializing the machine, to please the
     * EuroPC and other systems that need this.
     */
    if (!PCI && !MCA)
	video_reset();

    /* All good, boot the machine! */
    machines[machine].init(&machines[machine], &r);

    /*
     * If this is a PCI or MCA machine, reset the video
     * card after initializing the machine, so the bus
     * slots work correctly.
     */
    if (PCI || MCA)
	video_reset();
}


/* Close down the machine (saving stuff, etc.) */
void
machine_close(void)
{
    if (machines[machine].close != NULL)
	machines[machine].close();
}


/* Return the machine type. */
int
machine_type(void)
{
    return (machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].cpu_type);
}


/* Return this machine's default or slow speed. */
uint32_t
machine_speed(int turbo)
{
    uint32_t k;
    int mhz;

    /* Get the current CPU's maximum speed. */
    k = machines[machine].cpu[cpu_manufacturer].cpus[cpu_effective].rspeed;

    /*
     * If not on turbo, use the speed the mainboard
     * has set as its fallback ("slow") speed.
     */
    if (! turbo) {
	mhz = machines[machine].slow_mhz;
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
    romdef_t r;
    int i;

    wcscpy(temp, MACHINES_PATH);
    plat_append_slash(temp);
    wcscat(temp, machines[id].bios_path);
    i = rom_load_bios(&r, temp, 1);

    return(i);
}


void
machine_common_init(const machine_t *model, UNUSED(void *arg))
{
    /* System devices first. */
    pic_init();
    dma_init();
    pit_init();

    cpu_set();

    /* Start with (max/turbo) speed. */
    pc_set_speed();

    if (game_enabled)
	device_add(&game_device);

    if (parallel_enabled[0])
	device_add(&parallel_1_device);
    if (parallel_enabled[1])
	device_add(&parallel_2_device);
    if (parallel_enabled[2])
	device_add(&parallel_3_device);

    if (serial_enabled[0])
	device_add(&serial_1_device);
    if (serial_enabled[1])
	device_add(&serial_2_device);
}
