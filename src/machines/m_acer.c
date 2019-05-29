/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Intel 430xx-based Acer machines.
 *
 * Version:	@(#)m_acer.c	1.0.6	2019/05/28
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
#include <stdlib.h>
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


typedef struct {
    int		type;

    int		indx;		/* M3A */
} acer_t;


static void
m3a_out(uint16_t port, uint8_t val, priv_t priv)
{
    acer_t *dev = (acer_t *)priv;

    if (port == 0xea)
	dev->indx = val;
}


static uint8_t
m3a_in(uint16_t port, priv_t priv)
{
    acer_t *dev = (acer_t *)priv;

    if (port == 0xeb) {
	switch (dev->indx) {
		case 2:
			return 0xfd;
	}
    }

    return(0xff);
}


static void
acer_close(priv_t priv)
{
    acer_t *dev = (acer_t *)priv;

    free(dev);
}


static priv_t
acer_init(const device_t *info, void *arg)
{
    acer_t *dev;

    dev = (acer_t *)mem_alloc(sizeof(acer_t));
    memset(dev, 0x00, sizeof(acer_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    m_at_common_init();

    switch(dev->type) {
	case 0:		/* Acer M3A/430HX/Acer/SMC FDC37C932FR */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x1F, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x10, PCI_CARD_ONBOARD, 4, 0, 0, 0);

		device_add(&memregs_ed_device);

		device_add(&i430hx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxb_device);

		device_add(&fdc37c932fr_device);
		device_add(&keyboard_ps2_pci_device);

		io_sethandler(0x00ea, 2,
			      m3a_in,NULL,NULL, m3a_out,NULL,NULL, dev);
		break;

	case 1:		/* Acer V35N/430HX/Acer/SMC FDC37C932FR */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);

		device_add(&memregs_ed_device);

		device_add(&i430hx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxb_device);

		device_add(&fdc37c932fr_device);
		device_add(&keyboard_ps2_pci_device);

		io_sethandler(0x00ea, 2,
			      m3a_in,NULL,NULL, m3a_out,NULL,NULL, dev);
		break;

	case 2:		/* Acer V30/430FX/P54C P75-133/Acer/SMC FDC37C655 */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x14, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);

		device_add(&memregs_ed_device);

		device_add(&i430fx_device);
		device_add(&piix3_device);
		device_add(&intel_flash_bxb_device);

		device_add(&fdc37c665_device);
		device_add(&keyboard_ps2_acer_device);

		break;
    }

    return((priv_t)dev);
}


static const machine_t m3a_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 192, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_acer_m3a = {
    "Acer M3A",
    DEVICE_ROOT,
    0,
    L"acer/m3a",
    acer_init, acer_close, NULL,
    NULL, NULL, NULL,
    &m3a_info,
    NULL
};


static const machine_t v35n_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 192, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_acer_v35n = {
    "Acer V35N",
    DEVICE_ROOT,
    1,
    L"acer/v35n",
    acer_init, acer_close, NULL,
    NULL, NULL, NULL,
    &v35n_info,
    NULL
};


static const machine_t v30_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 192, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_acer_v30 = {
    "Acer V30",
    DEVICE_ROOT,
    2,
    L"acer/v30",
    acer_init, acer_close, NULL,
    NULL, NULL, NULL,
    &v30_info,
    NULL
};
