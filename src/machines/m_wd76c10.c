/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the WD76C10 based machines.
 *
 * Version:	@(#)m_wd76c10.c	1.0.13	2019/05/13
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
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/chipsets/wd76c10.h"
#include "../devices/input/keyboard.h"
#include "../devices/video/video.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&wd76c10_device);

    m_at_common_ide_init();

    switch(info->local) {
	case 1:		/* Amstrad MegaPC */
		device_add(&keyboard_ps2_quadtel_device);
		device_add(&paradise_wd90c11_megapc_device);
		break;
    }

    return((priv_t)arg);
}


static const machine_t sx_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC,
    0,
    1, 16, 1, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_amstrad_mega_sx = {
    "Amstrad MegaPC (WD76c10)",
    DEVICE_ROOT,
    1,
    L"amstrad/megapc",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &sx_info,
    NULL
};


static const machine_t dx_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_amstrad_mega_dx = {
    "Amstrad MegaPC (WD76c10)",
    DEVICE_ROOT,
    1,
    L"amstrad/megapc",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &dx_info,
    NULL
};
