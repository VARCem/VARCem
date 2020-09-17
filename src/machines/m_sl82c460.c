/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SL 82c460 based machines.
 *
 * Version:	@(#)m_sl82c460.c	1.0.0	2020/09/15
 *
 * Authors:	Altheos
 *
 *		Copyright 2020 Altheos.
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
#include "../devices/system/memregs.h"
#include "../devices/chipsets/sl82c460.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&sl82c460_device);

    m_at_common_ide_init();

    device_add(&fdc_at_device);

	device_add(&keyboard_at_ami_device);

    return((priv_t)arg);
}

static const machine_t sl386dx_ami_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_sl82c460_386dx_ami = {
    "SL460 386DX (AMI)",
    DEVICE_ROOT,
    0,
    L"generic/ami/386sl",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &sl386dx_ami_info,
    NULL
};
