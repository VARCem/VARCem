/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ALi-based machines.
 *
 * Version:	@(#)m_ali.c	1.0.11	2020/01/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/ali1429.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Allocate machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&ali1429_device);

    switch(info->local) {
	case 0:		/* AMI 486 (ALi-1429) */
		/*FALLTHROUGH*/

	case 1:		/* AMI Win486 (ALi-1429) */
		m_at_common_ide_init();
		device_add(&fdc_at_device);
		device_add(&keyboard_at_ami_device);
		break;
    }

    return((priv_t)arg);
}


static const machine_t ami486_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 64, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_ali1429_ami = {
    "Olystar LIL1429 (ALi-1429)",
    DEVICE_ROOT,
    0,
    L"ali1429/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &ami486_info,
    NULL
};


static const machine_t win486_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 64, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_ali1429_win = {
    "AMI Win486 (ALi-1429)",
    DEVICE_ROOT,
    1,
    L"ali1429/ami_win",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &win486_info,
    NULL
};
