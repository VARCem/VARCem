/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the C&T 82C235 ("SCAT") based machines.
 *
 * Version:	@(#)m_scat.c	1.0.16	2019/04/08
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original by GreatPsycho for PCem.
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include "../device.h"
#include "../cpu/cpu.h"
#include "../cpu/x86.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/scat.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "machine.h"


static void *
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, arg);

    switch(info->local) {
	case 0:		/* Generic Award SCAT */
		device_add(&scat_device);
		m_at_init();
		break;

	case 1:		/* Hyundai Super286TR */
		device_add(&scat_device);
		m_at_init();
		break;


	case 2:		/* GEAR GW286CT */
		device_add(&scat_4_device);
		m_at_init();
		break;

	case 3:		/* Samsung SPC4200P / SPC4216P */
		device_add(&scat_4_device);
		m_at_init();
		break;

	case 11:	/* KMX-C-02 (SCATsx) */
		device_add(&scat_sx_device);
		m_at_common_init();
		device_add(&keyboard_at_ami_device);
		break;
    }

    device_add(&fdc_at_device);

    return(arg);
}


static const machine_t award_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_scat_award = {
    "Award 286 (SCAT)",
    DEVICE_ROOT,
    0,
    L"generic/at/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &award_info,
    NULL
};


static const machine_t super_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_hs286tr = {
    "Hyundai Super286TR",
    DEVICE_ROOT,
    1,
    L"hyundai/super286tr",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &super_info,
    NULL
};


static const machine_t gear_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_gw286ct = {
    "Gear GW286CT",
    DEVICE_ROOT,
    2,
    L"unknown/gw286ct",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &gear_info,
    NULL
};


static const machine_t spc4200_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2,
    0,
    512, 2048, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_spc4200p = {
    "Samsung SPC4200P",
    DEVICE_ROOT,
    3,
    L"samsung/spc4200p",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &spc4200_info,
    NULL
};


static const machine_t spc4216_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2,
    0,
    1, 5, 1, 128, 8,
    {{"",cpus_286}}
};

const device_t m_spc4216p = {
    "Samsung SPC4216P",
    DEVICE_ROOT,
    3,
    L"samsung/spc4216p",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &spc4216_info,
    NULL
};


static const machine_t kmx_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 16384, 512, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_kmxc02 = {
    "KMX-C-02 (SCATsx)",
    DEVICE_ROOT,
    11,
    L"unknown/kmxc02",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &kmx_info,
    NULL
};
