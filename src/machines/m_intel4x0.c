/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Intel 430/440-based machines.
 *
 * Version:	@(#)m_intel4x0.c	1.0.11	2020/02/12
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
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/intel4x0.h"
#include "../devices/system/clk.h"
#include "../devices/system/pci.h"
#include "../devices/system/memregs.h"
#include "../devices/system/intel_flash.h"
#include "../devices/system/intel_sio.h"
#include "../devices/system/intel_piix.h"
#include "../devices/input/keyboard.h"
#include "../devices/sio/sio.h"
#include "../devices/disk/hdc.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"


typedef struct {
    uint8_t	port73,		/* board config reg, also 0x78 */
		port75;		/* board config reg, also 0x79 */

    uint16_t	timer_latch;
    tmrval_t	timer;
} batman_t;


static uint8_t
config_read(uint16_t port, priv_t priv)
{
    batman_t *dev = (batman_t *)priv;
    uint8_t ret = 0x00;

    switch (port) {
	case 0x0073:			/* board configuration register */
		ret = dev->port73;
		break;

	case 0x0075:			/* board configuration register */
		ret = dev->port75;
		break;
    }

    return(ret);
}


static void
timer_over(priv_t priv)
{
    batman_t *dev = (batman_t *)priv;

    dev->timer = 0;
}


static void
timer_write(uint16_t port, uint8_t val, priv_t priv)
{
    batman_t *dev = (batman_t *)priv;

    if (port & 1)
	dev->timer_latch = (dev->timer_latch & 0xff) | (val << 8);
    else
	dev->timer_latch = (dev->timer_latch & 0xff00) | val;

    dev->timer = dev->timer_latch * TIMER_USEC;
}


static uint8_t
timer_read(uint16_t port, priv_t priv)
{
    batman_t *dev = (batman_t *)priv;
    uint16_t latch;
    uint8_t ret;

    cycles -= (int)PITCONST;

    timer_clock();

    if (dev->timer < 0)
	return 0;

    latch = (uint16_t)(dev->timer / TIMER_USEC);

    if (port & 1)
	ret = latch >> 8;
    else
	ret = latch & 0xff;

    return(ret);
}


static void
batman_close(priv_t priv)
{
    batman_t *dev = (batman_t *)priv;

    free(dev);
}


static priv_t
batman_init(const device_t *info, UNUSED(void *parent))
{
    batman_t *dev;

    dev = (batman_t *)mem_alloc(sizeof(batman_t));
    memset(dev, 0x00, sizeof(batman_t));

    io_sethandler(0x0073, 1,
		  config_read,NULL,NULL, NULL,NULL,NULL, dev);
    io_sethandler(0x0075, 1,
		  config_read,NULL,NULL, NULL,NULL,NULL, dev);

    io_sethandler(0x0078, 2,
		  timer_read,NULL,NULL, timer_write,NULL,NULL, dev);

    timer_add(timer_over, dev, &dev->timer, &dev->timer);

    return((priv_t)dev);
}


static const device_t batman_device = {
    "Intel Batman support",
    0, 0, NULL,
    batman_init, batman_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static void
premiere_init(int nx)
{
    batman_t *dev;

    /* Create the board registers. */
    dev = (batman_t *)device_add(&batman_device);

    /* Add configuration bits for port 75. */
    dev->port73 = 0xff;		/* 1111 1111, probably wrong */
    dev->port75 = 0xc0;		/* 1100 0000, " */
    if (machine_get_config_int("setup_disable"))
	dev->port75 |= 0x20;	/* "disable Setup" */

    pci_init(PCI_CONFIG_TYPE_2);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x01, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x06, PCI_CARD_NORMAL, 3, 2, 1, 4);
    pci_register_slot(0x0E, PCI_CARD_NORMAL, 2, 1, 3, 4);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 3, 2, 4);
    pci_register_slot(0x02, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    if (nx)
	device_add(&i430nx_device);
    else
	device_add(&i430lx_device);

    device_add(&sio_device);
    device_add(&intel_flash_bxt_ami_device);
    device_add(&memregs_device);

    m_at_common_init();

    device_add(&keyboard_ps2_ami_pci_device);

    device_add(&ide_pci_2ch_device);

    device_add(&fdc37c665_device);
}


static priv_t
common_init(const device_t *info, void *arg)
{
    static video_timings_t endeavor_timing = {VID_BUS,3,2,4,25,25,40};

    /* Allocate machine device to system. */
    device_add_ex(info, arg);

    switch(info->local) {
	/* Revenge: Premiere/PCI I/430LX/AMI/SMC FDC37C665 */
	case 0:
		premiere_init(0);
		break;

	/* Plato: Premiere/PCI II/430NX/AMI/SMC FDC37C665 */
	case 1:
		premiere_init(1);
		break;

	/* Endeavor: Intel Advanced_EV/430FX/AMI/NS PC87306 */
	case 2:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x08, PCI_CARD_ONBOARD, 4, 0, 0, 0);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x10, PCI_CARD_NORMAL, 4, 1, 2, 3);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430fx_device);
		device_add(&piix_device);
		device_add(&intel_flash_bxt_ami_device);
		device_add(&memregs_device);

		m_at_common_init();

		device_add(&keyboard_ps2_ami_pci_device);
		device_add(&pc87306_device);

		if (config.video_card == VID_INTERNAL) {
			device_add(&s3_phoenix_trio64_onboard_pci_device);
			video_inform(VID_TYPE_SPEC, &endeavor_timing);
		}
		break;

	/* Zappa: Intel Advanced_ZP/430FX/AMI/NS PC87306 */
	case 3:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430fx_device);
		device_add(&piix_device);
		device_add(&intel_flash_bxt_ami_device);
		device_add(&memregs_device);

		m_at_common_init();

		device_add(&keyboard_ps2_ami_pci_device);
		device_add(&pc87306_device);
		break;

	/* Thor: Intel Advanced_ATX/430FX/AMI/NS PC87306 */
	/* MRthor: Intel Advanced_ATX/430FX/MR.BIOS/NS PC87306 */
	case 4:
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x08, PCI_CARD_ONBOARD, 4, 0, 0, 0);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0E, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 2, 1);
		pci_register_slot(0x10, PCI_CARD_NORMAL, 4, 3, 2, 1);
		pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

		device_add(&i430fx_device);
		device_add(&piix_device);
		device_add(&intel_flash_bxt_ami_device);
		device_add(&memregs_device);

		m_at_common_init();

		device_add(&keyboard_ps2_ami_pci_device);
		device_add(&pc87306_device);
		break;
    }

    return((priv_t)arg);
}


static const device_config_t batman_config[] = {
    {
	"setup_disable", "Disable SETUP", CONFIG_BINARY, "", 0
    },
    {
	NULL
    }
};


static const machine_t revenge_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    2, 128, 2, 128, -1,
    {{"Intel",cpus_Pentium5V}}
};

const device_t m_batman = {
    "Intel Batman",
    DEVICE_ROOT,
    0,
    L"intel/revenge",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &revenge_info,
    batman_config
};

const device_t m_ambradp60 = {
    "Ambra DP60PCI",
    DEVICE_ROOT,
    0,
    L"ambra/dp60",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &revenge_info,
    batman_config
};

static const machine_t plato_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    2, 128, 2, 128, -1,
    {{"Intel",cpus_PentiumS5},{"IDT",cpus_WinChip},CPU_AMD_K5}
};

const device_t m_plato = {
    "Intel Plato",
    DEVICE_ROOT,
    1,
    L"intel/plato",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &plato_info,
    batman_config
};

const device_t m_ambradp90 = {
    "Ambra DP90PCI",
    DEVICE_ROOT,
    1,
    L"ambra/dp90",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &plato_info,
    batman_config
};


static const machine_t endeavor_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_PentiumS5},{"IDT",cpus_WinChip},CPU_AMD_K56}
};

const device_t m_endeavor = {
    "Intel Endeavor",
    DEVICE_ROOT,
    2,
    L"intel/endeavor",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &endeavor_info,
    NULL		/* &s3_phoenix_trio64_onboard_pci_device */
};


static const machine_t zappa_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_PentiumS5},{"IDT",cpus_WinChip},CPU_AMD_K5}
};

const device_t m_zappa = {
    "Intel Zappa",
    DEVICE_ROOT,
    3,
    L"intel/zappa",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &zappa_info,
    NULL
};


static const machine_t thor_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_thor = {
    "Intel Thor",
    DEVICE_ROOT,
    4,
    L"intel/thor",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &thor_info,
    NULL
};


static const machine_t thor_mr_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC,
    0,
    8, 128, 8, 128, -1,
    {{"Intel",cpus_Pentium},{"IDT",cpus_WinChip},CPU_AMD_K56,{"Cyrix",cpus_6x86}}
};

const device_t m_thor_mr = {
    "Intel Thor (MR)",
    DEVICE_ROOT,
    4,
    L"intel/thor_mr",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &thor_mr_info,
    NULL
};
