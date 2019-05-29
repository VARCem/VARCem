/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of some of the Tyan mainboards.
 *
 * **NOTE**	Although these machines have been tested with DOS, Windows
 *		(XP SP3) and Linux (Debian), they are known to still have
 *		some issues:
 *
 *		- the 440FX chipset apparently wants ACPI, which we do not
 *		  yet have
 *		- PentiumPro+ support has not been thoroughly tested for
 *		  proper operation (but adding the machine back hopefully
 *		  means we will get more testing done..)
 *		- the Tyan boards apparently include a PCI EIDE controller,
 *		  but it is unknown which one. Spotted was an FDC37C665 on
 *		  an S1662 board, and the manual for the S1668 board shows
 *		  that those can also have an FDC37C669. That said, neither
 *		  of those does EIDE/UDMA, so until we know which chip is
 *		  actually being used, UDMA will not work.
 *
 *		As stated above, it is hoped that by re-adding these, more
 *		testing will get done so they can be 'completed' sometime.
 *
 * Version:	@(#)m_tyan.c	1.0.4	2019/05/28
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
#include "../devices/disk/hdc.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Allocate machine device to system. */
    device_add_ex(info, (priv_t)arg);

    switch(info->local) {
	/* S1662: Tyan Titan-Pro AT/440FX/Award BIOS/SMC FDC37C665 */
	case 0:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0C, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x0A, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i440fx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);
		device_add(&memregs_eb_ffff_device);
		m_at_common_init();
		device_add(&keyboard_ps2_pci_device);

		device_add(&fdc37c669_device);
		break;

	/* S1668: Tyan Titan-Pro ATX/440FX/AMI/SMC FDC37C669 */
	case 1:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0C, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x0A, PCI_CARD_NORMAL, 1, 2, 3, 4);

		device_add(&i440fx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxt_device);
		device_add(&memregs_eb_ffff_device);
		m_at_common_init();
		device_add(&keyboard_ps2_ami_pci_device);

		device_add(&fdc37c665_device);
		break;
    }

    return((priv_t)arg);
}


static const machine_t s1662_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 1024, 8, 128, -1,
    {{"Intel",cpus_PentiumPro}}
};

const device_t m_tyan_1662_ami = {
    "Tyan Titan-Pro AT (S1662)",
    DEVICE_ROOT,
    0,
    L"tyan/s1662/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &s1662_info,
    NULL
};

const device_t m_tyan_1662_award = {
    "Tyan Titan-Pro AT (S1662)",
    DEVICE_ROOT,
    0,
    L"tyan/s1662/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &s1662_info,
    NULL
};


static const machine_t s1668_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 1024, 8, 128, -1,
    {{"Intel",cpus_PentiumPro}}
};

const device_t m_tyan_1668_ami = {
    "Tyan Ttan-Pro ATX (S1668)",
    DEVICE_ROOT,
    1,
    L"tyan/s1668/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &s1668_info,
    NULL
};

const device_t m_tyan_1668_award = {
    "Tyan Ttan-Pro ATX (S1668)",
    DEVICE_ROOT,
    1,
    L"tyan/s1668/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &s1668_info,
    NULL
};
