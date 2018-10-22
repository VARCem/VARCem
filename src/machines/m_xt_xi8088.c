/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Xi8088 open-source machine.
 *
 * Version:	@(#)m_xt_xi8088.c	1.0.11	2018/09/19
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
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/dma.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/input/keyboard.h"
#include "../devices/ports/parallel.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "machine.h"
#include "m_xt_xi8088.h"


typedef struct {
    uint8_t turbo;

    int turbo_setting;
    int bios_128kb;
} xi8088_t;


static xi8088_t xi8088;


uint8_t
xi8088_turbo_get(void)
{
    return(xi8088.turbo);
}


void
xi8088_turbo_set(uint8_t value)
{
    int c;

    if (! xi8088.turbo_setting) return;

    xi8088.turbo = value;
    if (! value) {
	DEBUG("Xi8088 turbo off\n");
	c = cpu;
	cpu = 0;	/* 8088/4.77 */
	cpu_set();
	cpu = c;
    } else {
	DEBUG("Xi8088 turbo on\n");
	cpu_set();
    }
}


void
xi8088_bios_128kb_set(int val)
{
    xi8088.bios_128kb = val;
}


int
xi8088_bios_128kb(void)
{
    return(xi8088.bios_128kb);
}


static const device_config_t xi8088_config[] = {
    {
	"turbo_setting","Turbo",CONFIG_SELECTION,"",0,
	{
		{
			"Always at selected speed",0
		},
		{
			"Hotkeys (starts off)",1
		},
		{
			""
		}
	}
    },
    {
	"","",-1
    }
};


const device_t m_xi8088_device = {
    "Xi8088",
    0, 0,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    xi8088_config
};


void
machine_xt_xi8088_init(const machine_t *model, void *arg)
{
    /* Initialize local state. */
    memset(&xi8088, 0x00, sizeof(xi8088_t));

    if (biosmask < 0x010000)
	xi8088_bios_128kb_set(0);
     else
	xi8088_bios_128kb_set(1);

    if (xi8088_bios_128kb())
	mem_add_upper_bios();

    /*
     * Get the selected Turbo Speed setting.
     * Even though the bios by default turns the turbo off when
     * controlling by hotkeys, we always start at full speed.
     */
    xi8088.turbo = 1;
    xi8088.turbo_setting = device_get_config_int("turbo_setting");

    /*
     * TODO: set UMBs?
     * See if PCem always sets when we have > 640KB ram and avoids
     * conflicts when a peripheral uses the same memory space
     */
    machine_common_init(model, arg);

    nmi_init();
    pic2_init();

    device_add(&at_nvr_device);

    device_add(&keyboard_ps2_xi8088_device);

    device_add(&fdc_xt_device);
}
