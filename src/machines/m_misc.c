/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various systems and mainboards.
 *
 * Version:	@(#)m_misc.c	1.0.5	2020/06/14
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
#include "../devices/chipsets/intel4x0.h"
#include "../devices/system/pci.h"
#include "../devices/system/memregs.h"
#include "../devices/system/intel_flash.h"
#include "../devices/system/intel_sio.h"
#include "../devices/system/intel_piix.h"
#include "../devices/input/keyboard.h"
#include "../devices/sio/sio.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/video/video.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Allocate machine device to system. */
    device_add_ex(info, (priv_t)arg);

    switch(info->local) {
	/* Shuttle HOT-557: Award 430VX PCI/430VX/Award/UMC UM8669F*/
	case 0:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430vx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);

		device_add(&memregs_eb_device);

		m_at_common_init();

		device_add(&um8669f_device);
		device_add(&keyboard_ps2_pci_device);
		break;

	/* MB500N: PC Partner MB500N/430FX/Award/SMC FDC37C665 */
	case 10:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430fx_device);
		device_add(&piix_device);
		device_add(&intel_flash_bxt_device);

		device_add(&memregs_eb_device);

		m_at_common_init();

		device_add(&fdc37c665_device);
		device_add(&keyboard_ps2_pci_device);
		break;

	/* President: Award 430FX PCI/430FX/Award/Unknown SIO */
	case 11:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x08, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x09, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&memregs_eb_device);

		device_add(&i430fx_device);
		device_add(&piix_device);
		device_add(&intel_flash_bxt_device);

		m_at_common_init();

		device_add(&w83877f_president_device);
		device_add(&keyboard_ps2_pci_device);
		break;

	/* P55VA: Epox P55-VA/430VX/Award/SMC FDC37C932FR*/
	case 12:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x08, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x09, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430vx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);

		device_add(&memregs_eb_device);

		m_at_common_init();

		device_add(&fdc37c932fr_device);
		device_add(&keyboard_ps2_pci_device);
		break;

	/* J656VXD: Jetway J656VXD/430VX/Award/SMC FDC37C669*/
	case 13:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430vx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);

		device_add(&memregs_eb_device);

		m_at_common_init();

		device_add(&fdc37c669_device);
		device_add(&keyboard_ps2_pci_device);
		break;

	/* P55T2S: Supermicro P/I-P55T2S/430HX/AMI/NS PC87306 */
	case 14:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&memregs_eb_device);

		device_add(&i430hx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);

		m_at_common_init();

		device_add(&pc87306_device);
		device_add(&keyboard_ps2_ami_pci_device);
		break;
    }

    return((priv_t)arg);
}


static const machine_t aw430vx_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_aw430vx = {
    "Shuttle HOT-557",
    DEVICE_ROOT,
    0,
    L"generic/430vx/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &aw430vx_info,
    NULL
};


static const machine_t mb500n_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_PentiumS5},{"IDT",cpus_WinChip},CPU_AMD_K5}
};

const device_t m_mb500n = {
    "MB500N",
    DEVICE_ROOT,
    10,
    L"pcpartner/mb500n",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &mb500n_info,
    NULL
};


static const machine_t president_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_PentiumS5},{"IDT",cpus_WinChip},CPU_AMD_K5}
};

const device_t m_president = {
    "President",
    DEVICE_ROOT,
    11,
    L"president/president",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &president_info,
    NULL
};


static const machine_t epox_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_p55va = {
    "Epox P55VA",
    DEVICE_ROOT,
    12,
    L"epox/p55va",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &epox_info,
    NULL
};


static const machine_t jetway_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_j656vxd = {
    "Jetway 656VXD",
    DEVICE_ROOT,
    13,
    L"jetway/j656vxd",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &jetway_info,
    NULL
};


static const machine_t t2s_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 768, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_p55t2s = {
    "ASUS P55T2S",
    DEVICE_ROOT,
    14,
    L"supermicro/p55t2s",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &t2s_info,
    NULL
};
