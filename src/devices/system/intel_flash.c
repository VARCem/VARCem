/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Intel 28F001BX (2Mbit,8-bit) flash devices.
 *
 * Version:	@(#)intel_flash.c	1.0.13	2019/05/13
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
#include "../../emu.h"
#include "../../timer.h"
#include "../../device.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../nvr.h"
#include "../../plat.h"
#include "../../machines/machine.h"


#define FLASH_IS_BXB	2
#define FLASH_INVERT	1

#define BLOCK_MAIN	0
#define BLOCK_DATA1	1
#define BLOCK_DATA2	2
#define BLOCK_BOOT	3


enum {
    CMD_READ_ARRAY = 0xff,
    CMD_IID = 0x90,
    CMD_READ_STATUS = 0x70,
    CMD_CLEAR_STATUS = 0x50,
    CMD_ERASE_SETUP = 0x20,
    CMD_ERASE_CONFIRM = 0xd0,
    CMD_ERASE_SUSPEND = 0xb0,
    CMD_PROGRAM_SETUP = 0x40,
    CMD_PROGRAM_SETUP_ALT = 0x10
};


typedef struct {
    uint8_t	id;
    int8_t	invert_high_pin;

    uint8_t	command,
		status;

    mem_map_t	mapping[8],
		mapping_h[8];

    uint32_t	block_start[4],
		block_end[4],
		block_len[4];

    wchar_t	path[1024];

    uint8_t	array[131072];
} flash_t;


static uint8_t
flash_read(uint32_t addr, priv_t priv)
{
    flash_t *dev = (flash_t *)priv;
    uint8_t ret = 0xff;

    if (dev->invert_high_pin)
	addr ^= 0x10000;
    addr &= 0x1ffff;

    switch (dev->command) {
        case CMD_IID:
                if (addr & 1)
                        ret = dev->id;
                else
			ret = 0x89;
		break;

        case CMD_READ_STATUS:
                ret = dev->status;
		break;

	case CMD_READ_ARRAY:
		/*FALLTHROUGH*/

	default:
                ret = dev->array[addr];
		break;
    }

    return(ret);
}


static uint16_t
flash_readw(uint32_t addr, priv_t priv)
{
    flash_t *dev = (flash_t *)priv;
    uint16_t *q;

    addr &= 0x1ffff;
    if (dev->invert_high_pin)
	addr ^= 0x10000;

    q = (uint16_t *)&dev->array[addr];

    return(*q);
}


static uint32_t
flash_readl(uint32_t addr, priv_t priv)
{
    flash_t *dev = (flash_t *)priv;
    uint32_t *q;

    addr &= 0x1ffff;
    if (dev->invert_high_pin)
	addr ^= 0x10000;

    q = (uint32_t *)&dev->array[addr];

    return(*q);
}


static void
flash_write(uint32_t addr, uint8_t val, priv_t priv)
{
    flash_t *dev = (flash_t *)priv;
    int i;

    if (dev->invert_high_pin)
	addr ^= 0x10000;
    addr &= 0x1ffff;

    switch (dev->command) {
        case CMD_ERASE_SETUP:
                if (val == CMD_ERASE_CONFIRM) {
			for (i = 0; i < 3; i++) {
                        	if ((addr >= dev->block_start[i]) && (addr <= dev->block_end[i]))
                                	memset(&dev->array[dev->block_start[i]], 0xff, dev->block_len[i]);
			}

                        dev->status = 0x80;
                }
                dev->command = CMD_READ_STATUS;
                break;

        case CMD_PROGRAM_SETUP:
        case CMD_PROGRAM_SETUP_ALT:
                if ((addr & 0x1e000) != (dev->block_start[3] & 0x1e000))
       	                dev->array[addr] = val;
                dev->command = CMD_READ_STATUS;
                dev->status = 0x80;
                break;

        default:
                dev->command = val;
                switch (val) {
                        case CMD_CLEAR_STATUS:
                        	dev->status = 0;
                        	break;
                }
    }
}


static void
add_mappings(flash_t *dev, int inverted)
{
    uint32_t addr;
    int i;

    for (i = 0; i < 8; i++) {
	/*
	 * Some boards invert the high pin - the flash->array pointers
	 * need to pointer invertedly in order for INTERNAL writes to
	 * go to the right part of the array.
	 */
	addr =  (inverted) ? ((i << 14) ^ 0x10000) : (i << 14);

	mem_map_add(&dev->mapping[i], 0xe0000 + (i << 14), 0x04000,
		    flash_read,flash_readw,flash_readl,
		    flash_write,mem_write_nullw,mem_write_nulll,
		    dev->array + (addr & 0x1ffff), MEM_MAPPING_EXTERNAL, dev);

	mem_map_add(&dev->mapping_h[i], 0xfffe0000 + (i << 14), 0x04000,
		    flash_read,flash_readw,flash_readl,
		    flash_write,mem_write_nullw,mem_write_nulll,
		    dev->array + (addr & 0x1ffff), 0, dev);
    }
}


static void
flash_close(priv_t priv)
{
    flash_t *dev = (flash_t *)priv;
    FILE *fp;

    fp = plat_fopen(nvr_path(dev->path), L"wb");
    if (fp != NULL) {
	fwrite(&dev->array[dev->block_start[BLOCK_MAIN]],
	       dev->block_len[BLOCK_MAIN], 1, fp);
	fwrite(&dev->array[dev->block_start[BLOCK_DATA1]],
	       dev->block_len[BLOCK_DATA1], 1, fp);
	fwrite(&dev->array[dev->block_start[BLOCK_DATA2]],
	       dev->block_len[BLOCK_DATA2], 1, fp);
	fclose(fp);
    }

    free(dev);
}


static priv_t
flash_init(const device_t *info, UNUSED(void *parent))
{
    wchar_t temp[128];
    flash_t *dev;
    int i, type = 0;
    FILE *fp;

    dev = (flash_t *)mem_alloc(sizeof(flash_t));
    memset(dev, 0x00, sizeof(flash_t));

    mbstowcs(temp, machine_get_internal_name(), sizeof_w(temp));
    swprintf(dev->path, sizeof_w(dev->path), L"%ls.bin", temp);

    switch(info->local) {
	case 0:		/* Intel BXT */
		break;

	case 1:		/* Intel BXB */
		type = FLASH_IS_BXB;
		break;

	case 10:	/* Intel BXT, Inverted */
		type = FLASH_INVERT;
		break;

	case 11:	/* Intel BXB, Inverted */
		type = FLASH_IS_BXB | FLASH_INVERT;
		break;
    }

    dev->id = (type & FLASH_IS_BXB) ? 0x95 : 0x94;
    dev->invert_high_pin = !!(type & FLASH_INVERT);

    /* The block lengths are the same both flash types. */
    dev->block_len[BLOCK_MAIN] = 0x1c000;
    dev->block_len[BLOCK_DATA1] = 0x01000;
    dev->block_len[BLOCK_DATA2] = 0x01000;
    dev->block_len[BLOCK_BOOT] = 0x02000;

    if (type & FLASH_IS_BXB) {			/* 28F001BX-B */
	dev->block_start[BLOCK_MAIN] = 0x04000;	/* MAIN BLOCK */
	dev->block_end[BLOCK_MAIN] = 0x1ffff;
	dev->block_start[BLOCK_DATA1] = 0x03000;/* DATA AREA 1 BLOCK */
	dev->block_end[BLOCK_DATA1] = 0x03fff;
	dev->block_start[BLOCK_DATA2] = 0x04000;/* DATA AREA 2 BLOCK */
	dev->block_end[BLOCK_DATA2] = 0x04fff;
	dev->block_start[BLOCK_BOOT] = 0x00000;	/* BOOT BLOCK */
	dev->block_end[BLOCK_BOOT] = 0x01fff;
    } else {					/* 28F001BX-T */
	dev->block_start[BLOCK_MAIN] = 0x00000;	/* MAIN BLOCK */
	dev->block_end[BLOCK_MAIN] = 0x1bfff;
	dev->block_start[BLOCK_DATA1] = 0x1c000;/* DATA AREA 1 BLOCK */
	dev->block_end[BLOCK_DATA1] = 0x1cfff;
	dev->block_start[BLOCK_DATA2] = 0x1d000;/* DATA AREA 2 BLOCK */
	dev->block_end[BLOCK_DATA2] = 0x1dfff;
	dev->block_start[BLOCK_BOOT] = 0x1e000;	/* BOOT BLOCK */
	dev->block_end[BLOCK_BOOT] = 0x1ffff;
    }

    for (i = 0; i < 8; i++) {
	mem_map_disable(&bios_mapping[i]);
	mem_map_disable(&bios_high_mapping[i]);
    }

    if (dev->invert_high_pin) {
	memcpy(dev->array, bios + 65536, 65536);
	memcpy(dev->array + 65536, bios, 65536);
    } else
	memcpy(dev->array, bios, 131072);

    add_mappings(dev, dev->invert_high_pin);

    dev->command = CMD_READ_ARRAY;
    dev->status = 0;

    fp = plat_fopen(nvr_path(dev->path), L"rb");
    if (fp != NULL) {
	fread(&dev->array[dev->block_start[BLOCK_MAIN]],
	      dev->block_len[BLOCK_MAIN], 1, fp);
	fread(&dev->array[dev->block_start[BLOCK_DATA1]],
	      dev->block_len[BLOCK_DATA1], 1, fp);
	fread(&dev->array[dev->block_start[BLOCK_DATA2]],
	      dev->block_len[BLOCK_DATA2], 1, fp);
	fclose(fp);
    }

    return((priv_t)dev);
}


const device_t intel_flash_bxt_device = {
    "Intel 28F001BXT Flash",
    0,
    0,
    NULL,
    flash_init, flash_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t intel_flash_bxb_device = {
    "Intel 28F001BXB Flash",
    0,
    1,
    NULL,
    flash_init, flash_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t intel_flash_bxt_ami_device = {
    "Intel 28F001BXT Flash (Inverted)",
    0,
    10,
    NULL,
    flash_init, flash_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t intel_flash_bxb_ami_device = {
    "Intel 28F001BXB Flash (Inverted)",
    0,
    11,
    NULL,
    flash_init, flash_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
