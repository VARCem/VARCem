/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various A/Open mainboards.
 *
 * Version:	@(#)m_aopen.c	1.0.4	2019/05/28
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
	/* AP53: AOpen AP53/430HX/AMI/SMC FDC37C665/669 */
	case 0:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x06, PCI_CARD_ONBOARD, 1, 2, 3, 4);

		device_add(&i430hx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);
		device_add(&memregs_eb_device);

		m_at_common_init();

		device_add(&fdc37c669_device);
		device_add(&keyboard_ps2_ami_pci_device);

		break;
    }

    return((priv_t)arg);
}


static const machine_t ap53_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 512, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_ap53 = {
    "AOpen AP53",
    DEVICE_ROOT,
    0,
    L"aopen/ap53",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &ap53_info,
    NULL
};
