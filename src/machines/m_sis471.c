/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the SiS 85c471 based machines.
 *
 * Version:	@(#)m_sis471.c	1.0.16	2020/06/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "../device.h"
#include "../devices/chipsets/sis471.h"
#include "../devices/system/memregs.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&sis_85c471_device);

    device_add(&memregs_device);

    switch(info->local) {
	default:
		m_at_ide_init();
		break;
    }

    device_add(&fdc_at_device);

    return((priv_t)arg);
}


static const machine_t ami_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 64, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_sis471_ami = {
    "AMI 486 (SiS 471)",
    DEVICE_ROOT,
    0,
    L"sis471/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &ami_info,
    NULL
};


static const machine_t dtk_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 64, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_dtk486 = {
    "DTK 486 (SiS 471)",
    DEVICE_ROOT,
    1,
    L"dtk/486",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &dtk_info,
    NULL
};
