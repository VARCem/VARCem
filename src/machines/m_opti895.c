/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Opti 82C495 based machines.
 *
 * Version:	@(#)m_opti8xx.c	1.0.0	2020/11/16
 *
 * Authors:	   Altheos
 *             Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *      Copyright 2020 Altheos
 *		Copyright 2020 Fred N. van Kempen.
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
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/sio/sio.h"
#include "../devices/system/memregs.h"
#include "../devices/chipsets/opti895.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"

static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&opti895_device);

    device_add(&fdc_at_device);
    
    switch(info->local) {
	case 0:		
		m_at_common_init();
		device_add(&keyboard_at_device);
		device_add(&memregs_device);
		break;
		
	case 1:		
		m_at_common_init();
		device_add(&keyboard_at_ami_device);
		device_add(&memregs_device);
		break;
    }

    return((priv_t)arg);
}

static const machine_t hot419_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 128, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_opti895_hot419 = {
    "Shuttle Hot-419",
    DEVICE_ROOT,
    1,
    L"opti8xx/shuttle",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &hot419_info,
    NULL
};

static const machine_t opti8xx_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 128, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_opti_dp4044 = {
    "Data Expert DP4044",
    DEVICE_ROOT,
    0,
    L"opti8xx/dataexpert",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &opti8xx_info,
    NULL
};
