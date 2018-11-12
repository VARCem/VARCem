/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Intel 430xx 440xx PCISet chips.
 *
 * Version:	@(#)m_at_4x0.c	1.0.3	2018/11/08
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/system/pci.h"
#include "../devices/system/memregs.h"
#include "../devices/system/intel.h"
#include "../devices/system/intel_flash.h"
#include "../devices/system/intel_sio.h"
#include "../devices/system/intel_piix.h"
#include "../devices/input/keyboard.h"
#include "../devices/sio/sio.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "machine.h"


enum {
    INTEL_430LX,
    INTEL_430NX,
    INTEL_430FX,
    INTEL_430FX_PB640,
    INTEL_430HX,
    INTEL_430VX,
#if defined(DEV_BRANCH) && defined(USE_I686)
    INTEL_440FX
#endif
};

typedef struct {
    uint8_t	regs[256];
    int		type;
} i4x0_t;

typedef struct {
    int		index;
} acerm3a_t;


static void
i4x0_map(uint32_t addr, uint32_t size, int state)
{
    switch (state & 3) {
	case 0:
		mem_set_mem_state(addr, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 1:
		mem_set_mem_state(addr, size,
				  MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 2:
		mem_set_mem_state(addr, size,
				  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
		break;

	case 3:
		mem_set_mem_state(addr, size,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		break;
    }

    flushmmucache_nopc();
}


static void
i4x0_write(int func, int addr, uint8_t val, void *priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    if (func)
	return;

    if ((addr >= 0x10) && (addr < 0x4f))
	return;

    switch (addr) {
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x0c: case 0x0e:
		return;

	case 0x04: /*Command register*/
		if (dev->type >= INTEL_430FX) {
			if (dev->type == INTEL_430FX_PB640)
				val &= 0x06;
			else
				val &= 0x02;
		} else
			val &= 0x42;
		val |= 0x04;
		break;

	case 0x05:
		if (dev->type >= INTEL_430FX)
			val = 0;
		else
			val &= 0x01;
		break;

	case 0x06: /*Status*/
		val = 0;
		break;

	case 0x07:
		if (dev->type >= INTEL_430HX) {
			val &= 0x80;
			val |= 0x02;
		} else {
			val = 0x02;
			if (dev->type == INTEL_430FX_PB640)
				val |= 0x20;
		}
		break;

	case 0x59: /*PAM0*/
		if ((dev->regs[0x59] ^ val) & 0xf0) {
			i4x0_map(0xf0000, 0x10000, val >> 4);
			shadowbios = (val & 0x10);
		}
		break;

	case 0x5a: /*PAM1*/
		if ((dev->regs[0x5a] ^ val) & 0x0f)
			i4x0_map(0xc0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5a] ^ val) & 0xf0)
			i4x0_map(0xc4000, 0x04000, val >> 4);
		break;

	case 0x5b: /*PAM2*/
		if ((dev->regs[0x5b] ^ val) & 0x0f)
			i4x0_map(0xc8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5b] ^ val) & 0xf0)
			i4x0_map(0xcc000, 0x04000, val >> 4);
		break;

	case 0x5c: /*PAM3*/
		if ((dev->regs[0x5c] ^ val) & 0x0f)
			i4x0_map(0xd0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5c] ^ val) & 0xf0)
			i4x0_map(0xd4000, 0x04000, val >> 4);
		break;

	case 0x5d: /*PAM4*/
		if ((dev->regs[0x5d] ^ val) & 0x0f)
			i4x0_map(0xd8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5d] ^ val) & 0xf0)
			i4x0_map(0xdc000, 0x04000, val >> 4);
		break;

	case 0x5e: /*PAM5*/
		if ((dev->regs[0x5e] ^ val) & 0x0f)
			i4x0_map(0xe0000, 0x04000, val & 0xf);
		if ((dev->regs[0x5e] ^ val) & 0xf0)
			i4x0_map(0xe4000, 0x04000, val >> 4);
		break;

	case 0x5f: /*PAM6*/
		if ((dev->regs[0x5f] ^ val) & 0x0f)
			i4x0_map(0xe8000, 0x04000, val & 0xf);
		if ((dev->regs[0x5f] ^ val) & 0xf0)
			i4x0_map(0xec000, 0x04000, val >> 4);
		break;

	case 0x72: /*SMRAM*/
		if ((dev->type >= INTEL_430FX) &&
		    ((dev->regs[0x72] ^ val) & 0x48))
			i4x0_map(0xa0000, 0x20000, ((val & 0x48) == 0x48) ? 3 : 0);
		break;
    }

    dev->regs[addr] = val;
}


static uint8_t
i4x0_read(int func, int addr, void *priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    if (func)
	return 0xff;

    return dev->regs[addr];
}


static void
i4x0_reset(void *priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    i4x0_write(0, 0x59, 0x00, priv);
    if (dev->type >= INTEL_430FX)
	i4x0_write(0, 0x72, 0x02, priv);
}


static void
i4x0_close(void *priv)
{
    i4x0_t *dev = (i4x0_t *)priv;

    free(dev);
}


static void *
i4x0_init(const device_t *info)
{
    i4x0_t *dev = (i4x0_t *) mem_alloc(sizeof(i4x0_t));
    memset(dev, 0x00, sizeof(i4x0_t));
    dev->type = info->local;

    dev->regs[0x00] = 0x86; dev->regs[0x01] = 0x80; /*Intel*/

    switch(dev->type) {
	case INTEL_430LX:
		dev->regs[0x02] = 0xa3; dev->regs[0x03] = 0x04; /*82434LX/NX*/
		dev->regs[0x08] = 0x03; /*A3 stepping*/
		dev->regs[0x50] = 0x80;
		dev->regs[0x52] = 0x40; /*256kb PLB cache*/
		break;

	case INTEL_430NX:
		dev->regs[0x02] = 0xa3; dev->regs[0x03] = 0x04; /*82434LX/NX*/
		dev->regs[0x08] = 0x10; /*A0 stepping*/
		dev->regs[0x50] = 0xA0;
		dev->regs[0x52] = 0x44; /*256kb PLB cache*/
		dev->regs[0x66] = dev->regs[0x67] = 0x02;
		break;

	case INTEL_430FX:
	case INTEL_430FX_PB640:
		dev->regs[0x02] = 0x2d; dev->regs[0x03] = 0x12; /*SB82437FX-66*/
		if (dev->type == INTEL_430FX_PB640)
			dev->regs[0x08] = 0x02; /*???? stepping*/
		else
			dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x52] = 0x40; /*256kb PLB cache*/
		break;

	case INTEL_430HX:
		dev->regs[0x02] = 0x50; dev->regs[0x03] = 0x12; /*82439HX*/
		dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x51] = 0x20;
		dev->regs[0x52] = 0xB5; /*512kb cache*/
		dev->regs[0x56] = 0x52; /*DRAM control*/
		dev->regs[0x59] = 0x40;
		dev->regs[0x5A] = dev->regs[0x5B] = \
				  dev->regs[0x5C] = dev->regs[0x5D] = 0x44;
		dev->regs[0x5E] = dev->regs[0x5F] = 0x44;
		dev->regs[0x65] = dev->regs[0x66] = dev->regs[0x67] = 0x02;
		dev->regs[0x68] = 0x11;
		break;

	case INTEL_430VX:
		dev->regs[0x02] = 0x30; dev->regs[0x03] = 0x70; /*82437VX*/
		dev->regs[0x08] = 0x00; /*A0 stepping*/
		dev->regs[0x52] = 0x42; /*256kb PLB cache*/
		dev->regs[0x53] = 0x14;
		dev->regs[0x56] = 0x52; /*DRAM control*/
		dev->regs[0x67] = 0x11;
		dev->regs[0x69] = 0x03;
		dev->regs[0x70] = 0x20;
		dev->regs[0x74] = 0x0e;
		dev->regs[0x78] = 0x23;
		break;

#if defined(DEV_BRANCH) && defined(USE_I686)
	case INTEL_440FX:
		dev->regs[0x02] = 0x37; dev->regs[0x03] = 0x12; /*82441FX*/
		dev->regs[0x08] = 0x02; /*A0 stepping*/
		dev->regs[0x2c] = 0xf4;
		dev->regs[0x2d] = 0x1a;
		dev->regs[0x2f] = 0x11;
		dev->regs[0x51] = 0x01;
		dev->regs[0x53] = 0x80;
		dev->regs[0x58] = 0x10;
		dev->regs[0x5a] = dev->regs[0x5b] = \
				  dev->regs[0x5c] = dev->regs[0x5d] = 0x11;
		dev->regs[0x5e] = 0x11;
		dev->regs[0x5f] = 0x31;
		break;
#endif
    }

    dev->regs[0x04] = 0x06; dev->regs[0x05] = 0x00;
#if defined(DEV_BRANCH) && defined(USE_I686)
    if (dev->type == INTEL_440FX)
	dev->regs[0x06] = 0x80;
#endif

    if (dev->type == INTEL_430FX)
	dev->regs[0x07] = 0x82;
#if defined(DEV_BRANCH) && defined(USE_I686)
    else if (dev->type != INTEL_440FX)
#else
    else
#endif
	dev->regs[0x07] = 0x02;
    dev->regs[0x0b] = 0x06;
    if (dev->type >= INTEL_430FX)
	dev->regs[0x57] = 0x01;
    else
	dev->regs[0x57] = 0x31;
    dev->regs[0x60] = dev->regs[0x61] = dev->regs[0x62] = \
						dev->regs[0x63] = 0x02;
    dev->regs[0x64] = 0x02;
    if (dev->type >= INTEL_430FX)
	dev->regs[0x72] = 0x02;

    pci_add_card(0, i4x0_read, i4x0_write, dev);

    return dev;
}


static const device_t i430lx_device = {
    "Intel 82434LX",
    DEVICE_PCI,
    INTEL_430LX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

static const device_t i430nx_device = {
    "Intel 82434NX",
    DEVICE_PCI,
    INTEL_430NX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

static const device_t i430fx_device = {
    "Intel SB82437FX-66",
    DEVICE_PCI,
    INTEL_430FX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t i430fx_pb640_device = {
    "Intel SB82437FX-66 (PB640)",
    DEVICE_PCI,
    INTEL_430FX_PB640,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

static const device_t i430hx_device = {
    "Intel 82439HX",
    DEVICE_PCI,
    INTEL_430HX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

static const device_t i430vx_device = {
    "Intel 82437VX",
    DEVICE_PCI,
    INTEL_430VX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};

#if defined(DEV_BRANCH) && defined(USE_I686)
static const device_t i440fx_device = {
    "Intel 82441FX",
    DEVICE_PCI,
    INTEL_440FX,
    i4x0_init, i4x0_close, i4x0_reset,
    NULL, NULL, NULL, NULL,
    NULL
};
#endif


static void
machine_at_premiere_common_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    device_add(&ide_pci_2ch_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_2);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x01, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x06, PCI_CARD_NORMAL, 3, 2, 1, 4);
    pci_register_slot(0x0E, PCI_CARD_NORMAL, 2, 1, 3, 4);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 3, 2, 4);
    pci_register_slot(0x02, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&sio_device);
    device_add(&fdc37c665_device);
    device_add(&intel_batman_device);
    device_add(&intel_flash_bxt_ami_device);
}


/* Revenge: Intel Premiere/PCI I/430LX/AMI/SMC FDC37C665 */
void
machine_at_batman_init(const machine_t *model, void *arg)
{
    machine_at_premiere_common_init(model, arg);

    device_add(&i430lx_device);
}


/* Plato:   Intel Premiere/PCI II/430NX/AMI/SMC FDC37C665 */
void
machine_at_plato_init(const machine_t *model, void *arg)
{
    machine_at_premiere_common_init(model, arg);

    device_add(&i430nx_device);
}


/* P54TP4XE: ASUS P/I-P55TP4XE/430FX/Award/SMC FDC37C665 */
void
machine_at_p54tp4xe_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x09, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430fx_device);
    device_add(&piix_device);
    device_add(&fdc37c665_device);
    device_add(&intel_flash_bxt_device);
}


/* Endeavor: Intel Advanced_EV/430FX/AMI/NS PC87306 */
void
machine_at_endeavor_init(const machine_t *model, void *arg)
{
    static video_timings_t endeavor_timing = {VID_BUS,3,2,4,25,25,40};

    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

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
    device_add(&pc87306_device);
    device_add(&intel_flash_bxt_ami_device);

    if (video_card == VID_INTERNAL) {
	device_add(&s3_phoenix_trio64_onboard_pci_device);
	video_inform(VID_TYPE_SPEC, &endeavor_timing);
    }
}


/* Zappa: Intel Advanced_ZP/430FX/AMI/NS PC87306 */
void
machine_at_zappa_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0E, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x0F, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430fx_device);
    device_add(&piix_device);
    device_add(&pc87306_device);
    device_add(&intel_flash_bxt_ami_device);
}


/* MB500N: PC Partner MB500N/430FX/Award/SMC FDC37C665 */
void
machine_at_mb500n_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x14, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x12, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430fx_device);
    device_add(&piix_device);
    device_add(&fdc37c665_device);
    device_add(&intel_flash_bxt_device);
}


/* President: President Award 430FX PCI/430FX/Award/Unknown SIO */
void
machine_at_president_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x08, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x09, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430fx_device);
    device_add(&piix_device);
    device_add(&w83877f_president_device);
    device_add(&intel_flash_bxt_device);
}


/* Thor: Intel Advanced_ATX/430FX/AMI/NS PC87306 */
/* MRthor: Intel Advanced_ATX/430FX/MR.BIOS/NS PC87306 */
void
machine_at_thor_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

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
    device_add(&pc87306_device);
    device_add(&intel_flash_bxt_ami_device);
}


/* PB640: Packard Bell PB640/430FX/AMI/NS PC87306 */
void
machine_at_pb640_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x08, PCI_CARD_ONBOARD, 4, 0, 0, 0);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 2, 1, 3, 4);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 3, 2, 1, 4);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430fx_pb640_device);
    device_add(&piix_pb640_device);
    ide_enable_pio_override();
    device_add(&pc87306_device);
    device_add(&intel_flash_bxt_ami_device);

    if (video_card == VID_INTERNAL) {
	device_add(&gd5440_onboard_pci_device);
    }
}


/* AP53: AOpen AP53/430HX/AMI/SMC FDC37C665/669 */
void
machine_at_ap53_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

    powermate_memregs_init();

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
    device_add(&fdc37c669_device);
    device_add(&intel_flash_bxt_device);
}


/* P55T2P4: ASUS P/I-P55T2P4/430HX/Award/Winbond W8387F*/
void
machine_at_p55t2p4_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x09, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430hx_device);
    device_add(&piix3_device);
    device_add(&w83877f_device);
    device_add(&intel_flash_bxt_device);
}


/* P55T2S: ASUS P/I-P55T2S/430HX/AMI/NS PC87306 */
void
machine_at_p55t2s_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

    powermate_memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x12, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x14, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430hx_device);
    device_add(&piix3_device);
    device_add(&pc87306_device);
    device_add(&intel_flash_bxt_device);
}


/* P55TVP4: ASUS P/I-P55TVP4/430VX/Award/Winbond W8387F*/
void
machine_at_p55tvp4_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x09, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430vx_device);
    device_add(&piix3_device);
    device_add(&w83877f_device);
    device_add(&intel_flash_bxt_device);
}


/* 430VX: Award 430VX PCI/430VX/Award/UMC UM8669F*/
void
machine_at_i430vx_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x12, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x14, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430vx_device);
    device_add(&piix3_device);
    device_add(&um8669f_device);
    device_add(&intel_flash_bxt_device);
}


/* P55VA: Epox P55-VA/430VX/Award/SMC FDC37C932FR*/
void
machine_at_p55va_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x08, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x09, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x0B, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430vx_device);
    device_add(&piix3_device);
    device_add(&fdc37c932fr_device);
    device_add(&intel_flash_bxt_device);
}


/* J656VXD: Jetway J656VXD/430VX/Award/SMC FDC37C669*/
void
machine_at_j656vxd_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);

    device_add(&i430vx_device);
    device_add(&piix3_device);
    device_add(&fdc37c669_device);
    device_add(&intel_flash_bxt_device);
}


#if defined(DEV_BRANCH) && defined(USE_I686)
/* 440FX: Tyan Titan-Pro AT/440FX/Award BIOS/SMC FDC37C665 */
void
machine_at_i440fx_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    memregs_init();

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
    device_add(&fdc37c665_device);
    device_add(&intel_flash_bxt_device);
}


/* S1668: Tyan Titan-Pro ATX/440FX/AMI/SMC FDC37C669 */
void
machine_at_s1668_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_ami_pci_device);

    memregs_init();

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
    device_add(&fdc37c665_device);
    device_add(&intel_flash_bxt_device);
}
#endif


static void
acerm3a_out(uint16_t port, uint8_t val, void *priv)
{
    acerm3a_t *dev = (acerm3a_t *)priv;

    if (port == 0xea)
	dev->index = val;
}


static uint8_t
acerm3a_in(uint16_t port, void *priv)
{
    acerm3a_t *dev = (acerm3a_t *)priv;

    if (port == 0xeb) {
	switch (dev->index) {
		case 2:
			return 0xfd;
	}
    }

    return(0xff);
}


static void
acerm3a_close(void *priv)
{
    acerm3a_t *dev = (acerm3a_t *)priv;

    free(dev);
}


static void *
acerm3a_init(const device_t *info)
{
    acerm3a_t *dev = (acerm3a_t *)mem_alloc(sizeof(acerm3a_t));
    memset(dev, 0x00, sizeof(acerm3a_t));

    io_sethandler(0x00ea, 2,
		  acerm3a_in,NULL,NULL, acerm3a_out,NULL,NULL, dev);

    return(dev);
}


static const device_t acerm3a_device = {
    "Acer M3A Register",
    0,
    0,
    acerm3a_init, acerm3a_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


/* ACERM3A: Acer M3A/430HX/Acer/SMC FDC37C932FR */
void
machine_at_acerm3a_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    powermate_memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x0C, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x0E, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x1F, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x10, PCI_CARD_ONBOARD, 4, 0, 0, 0);

    device_add(&i430hx_device);
    device_add(&piix3_device);
    device_add(&fdc37c932fr_device);
    device_add(&acerm3a_device);
    device_add(&intel_flash_bxb_device);
}


/* ACERV35N: Acer V35N/430HX/Acer/SMC FDC37C932FR */
void
machine_at_acerv35n_init(const machine_t *model, void *arg)
{
    machine_at_common_init(model, arg);

    device_add(&keyboard_ps2_pci_device);

    powermate_memregs_init();

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x07, PCI_CARD_SPECIAL, 0, 0, 0, 0);
    pci_register_slot(0x11, PCI_CARD_NORMAL, 1, 2, 3, 4);
    pci_register_slot(0x12, PCI_CARD_NORMAL, 2, 3, 4, 1);
    pci_register_slot(0x13, PCI_CARD_NORMAL, 3, 4, 1, 2);
    pci_register_slot(0x14, PCI_CARD_NORMAL, 4, 1, 2, 3);
    pci_register_slot(0x0D, PCI_CARD_NORMAL, 1, 2, 3, 4);

    device_add(&i430hx_device);
    device_add(&piix3_device);
    device_add(&fdc37c932fr_device);
    device_add(&acerm3a_device);
    device_add(&intel_flash_bxb_device);
}
