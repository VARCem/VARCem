/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various Packard Bell machines.
 *
 * Version:	@(#)m_pbell.c	1.0.2	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
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
#include "../config.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/acc2168.h"
#include "../devices/chipsets/intel4x0.h"
#include "../devices/system/pci.h"
#include "../devices/system/memregs.h"
#include "../devices/system/intel.h"
#include "../devices/system/intel_flash.h"
#include "../devices/system/intel_sio.h"
#include "../devices/system/intel_piix.h"
#include "../devices/input/keyboard.h"
#include "../devices/sio/sio.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "machine.h"


static void *
common_init(const device_t *info, void *arg)
{
    /* Allocate machine device to system. */
    device_add_ex(info, arg);

    switch(info->local) {
	/* PB410A/PB430/ACC2168/ACC3221 */
	case 410:
		device_add(&acc2168_device);
		m_at_common_ide_init();
		device_add(&keyboard_ps2_device);
		memregs_init();
		device_add(&acc3221_device);
		if (config.video_card == VID_INTERNAL)
			device_add(&ht216_32_pb410a_device);	
		break;

	/* PB640: Packard Bell PB640/430FX/AMI/NS PC87306 */
	case 640:
		device_add(&i430fx_pb640_device);
		device_add(&piix_pb640_device);
		device_add(&intel_flash_bxt_ami_device);
		m_at_common_init();
		device_add(&keyboard_ps2_ami_pci_device);
		memregs_init();

		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x08, PCI_CARD_ONBOARD, 4, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 2, 1, 3, 4);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 3, 2, 1, 4);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		ide_enable_pio_override();
		device_add(&pc87306_device);

		if (config.video_card == VID_INTERNAL)
			device_add(&gd5440_onboard_pci_device);
		break;
    }

    return(arg);
}


static const machine_t pb640_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_pb640 = {
    "Packard Bell 640",
    DEVICE_ROOT,
    640,
    L"pbell/pb640",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &pb640_info,
    NULL		/* &gd5440_onboard_pci_device */
};


static const machine_t pb410a_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,
    0,
    1, 32, 1, 128, -1,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_pb410a = {
    "Packard Bell 410A",
    DEVICE_ROOT,
    410,
    L"pbell/pb410a",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &pb410a_info,
    NULL
};
