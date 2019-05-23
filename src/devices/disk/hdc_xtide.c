/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		XT-IDE controller emulation.
 *
 *		The XT-IDE project is intended to allow 8-bit ("XT") systems
 *		to use regular IDE drives. IDE is a standard based on the
 *		16b PC/AT design, and so a special board (with its own BIOS)
 *		had to be created for this.
 *
 *		XT-IDE is *NOT* the same as XTA, or X-IDE, which is an older
 *		standard where the actual MFM/RLL controller for the PC/XT
 *		was placed on the hard drive (hard drives where its drive
 *		type would end in "X" or "XT", such as the 8425XT.) This was
 *		more or less the original IDE, but since those systems were
 *		already on their way out, the newer IDE standard based on the
 *		PC/AT controller and 16b design became the IDE we now know.
 *
 * FIXME:	Make sure this works with the new IDE stuff, the AT and PS/2
 *		controllers do not have dev->ide set to anything...
 *
 * Version:	@(#)hdc_xtide.c	1.0.14	2019/05/17
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
#include <stdlib.h>
#include <wchar.h>
#define dbglog hdc_log
#include "../../emu.h"
#include "../../timer.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "hdc.h"
#include "hdc_ide.h"
#include "hdd.h"


#define ROM_PATH_XT	L"disk/xtide/ide_xt.bin"
#define ROM_PATH_AT	L"disk/xtide/ide_at.bin"
#define ROM_PATH_PS2	L"disk/xtide/side1v12.bin"
#define ROM_PATH_PS2_AT	L"disk/xtide/ide_at_1_1_5.bin"


typedef struct {
    void	*ide_board;

    uint8_t	data_high;
    rom_t	bios_rom;
} hdc_t;


static void
hdc_write(uint16_t port, uint8_t val, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    switch (port & 0x0f) {
	case 0:
		ide_writew(0, val | (dev->data_high << 8), dev->ide_board);
		return;

	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		ide_writeb((port  & 0x0f), val, dev->ide_board);
		return;

	case 8:
		dev->data_high = val;
		return;

	case 14:
		ide_write_devctl(0, val, dev->ide_board);
		return;
    }
}


static uint8_t
hdc_read(uint16_t port, priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;
    uint16_t tempw = 0xffff;

    switch (port & 0x0f) {
	case 0:
		tempw = ide_readw(0, dev->ide_board);
		dev->data_high = tempw >> 8;
		break;

	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		tempw = ide_readb((port  & 0x0f), dev->ide_board);
		break;

	case 8:
		tempw = dev->data_high;
		break;

	case 14:
		tempw = ide_read_alt_status(0, dev->ide_board);
		break;

	default:
		break;
    }

    return(tempw & 0xff);
}


static void
xtide_close(priv_t priv)
{
    hdc_t *dev = (hdc_t *)priv;

    free(dev);

    ide_xtide_close();
}


static priv_t
xtide_init(const device_t *info, UNUSED(void *parent))
{
    int rom_sz = 0;
    int io = 0;
    hdc_t *dev;

    dev = (hdc_t *)mem_alloc(sizeof(hdc_t));
    memset(dev, 0x00, sizeof(hdc_t));

    switch(info->local) {
	case 0:
		rom_sz = 0x4000;
		io = 0x300;
		dev->ide_board = ide_xtide_init();
		break;

	case 1:
		rom_sz = 0x4000;
		io = 0x300;
		device_add(&ide_isa_2ch_device);
		break;

	case 2:
		rom_sz = 0x8000;		//FIXME: file is 8KB ?
		io = 0x360;
		dev->ide_board = ide_xtide_init();
		break;

	case 3:
		rom_sz = 0x4000;		//FIXME: no I/O address?
		device_add(&ide_isa_2ch_device);
		break;
    }

    rom_init(&dev->bios_rom, info->path,
	     0xc8000, rom_sz, rom_sz-1, 0, MEM_MAPPING_EXTERNAL);

    if (io != 0)
	io_sethandler(io, 16,
		      hdc_read,NULL,NULL, hdc_write,NULL,NULL, dev);

    return((priv_t)dev);
}


const device_t xtide_device = {
    "PC/XT XTIDE",
    DEVICE_ISA,
    (HDD_BUS_IDE << 8) | 0,
    ROM_PATH_XT,
    xtide_init, xtide_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t xtide_at_device = {
    "PC/AT XTIDE",
    DEVICE_ISA | DEVICE_AT,
    (HDD_BUS_IDE << 8) | 1,
    ROM_PATH_AT,
    xtide_init, xtide_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t xtide_acculogic_device = {
    "PC/XT XTIDE (Acculogic)",
    DEVICE_ISA,
    (HDD_BUS_IDE << 8) | 2,
    ROM_PATH_PS2,
    xtide_init, xtide_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t xtide_at_ps2_device = {
    "PS/2 AT XTIDE (1.1.5)",
    DEVICE_ISA,
    (HDD_BUS_IDE << 8) | 3,
    ROM_PATH_PS2_AT,
    xtide_init, xtide_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
