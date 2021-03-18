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
 * Version:	@(#)m_opti495.c	1.0.14	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include "../devices/system/memregs.h"
#include "../devices/chipsets/opti495.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&opti495_device);

    m_at_common_ide_init();

    device_add(&fdc_at_device);

    switch(info->local) {
	case 0:		/* Generic with AMI BIOS */
		device_add(&keyboard_at_ami_device);
		break;

	case 1:		/* Generic with Award BIOS */
		device_add(&memregs_device);
		device_add(&keyboard_at_device);
		break;

	case 2:		/* Generic with MR BIOS */
		device_add(&keyboard_at_ami_device);
		break;
    }

    return((priv_t)arg);
}


static const machine_t o386sx_ami_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 16, 1, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_opti495_386sx_ami = {
    "Opti495 386SX (AMI)",
    DEVICE_ROOT,
    0,
    L"opti/opti495/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386sx_ami_info,
    NULL
};


static const machine_t o386sx_award_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 16, 1, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_opti495_386sx_award = {
    "Opti495 386SX (Award)",
    DEVICE_ROOT,
    1,
    L"opti/opti495/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386sx_award_info,
    NULL
};


static const machine_t o386sx_mr_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 16, 1, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_opti495_386sx_mr = {
    "Opti495 386SX (MR)",
    DEVICE_ROOT,
    2,
    L"opti/opti495/mr",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386sx_mr_info,
    NULL
};


static const machine_t o386dx_ami_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_opti495_386dx_ami = {
    "Opti495 386DX (AMI)",
    DEVICE_ROOT,
    0,
    L"opti/opti495/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386dx_ami_info,
    NULL
};


static const machine_t o386dx_award_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_opti495_386dx_award = {
    "Opti495 386DX (Award)",
    DEVICE_ROOT,
    1,
    L"opti/opti495/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386dx_award_info,
    NULL
};


static const machine_t o386dx_mr_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_opti495_386dx_mr = {
    "Opti495 386DX (MR)",
    DEVICE_ROOT,
    2,
    L"opti/opti495/mr",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o386dx_mr_info,
    NULL
};


static const machine_t o486_ami_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_opti495_486_ami = {
    "Opti495 486 (AMI)",
    DEVICE_ROOT,
    0,
    L"opti/opti495/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o486_ami_info,
    NULL
};


static const machine_t o486_award_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_opti495_486_award = {
    "Opti495 486 (Award)",
    DEVICE_ROOT,
    1,
    L"opti/opti495/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o486_award_info,
    NULL
};


static const machine_t o486_mr_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    1, 32, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_opti495_486_mr = {
    "Opti495 486 (MR)",
    DEVICE_ROOT,
    2,
    L"opti/opti495/mr",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &o486_mr_info,
    NULL
};
