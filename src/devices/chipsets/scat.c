/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the C&T 82C235 ("SCAT") chipset.
 *
 * Version:	@(#)scat.c	1.0.21	2021/03/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original by GreatPsycho for PCem.
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../config.h"
#include "../../cpu/cpu.h"
#include "../../cpu/x86.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/nmi.h"
#include "scat.h"


#define SCAT_DMA_WAIT_STATE_CONTROL 0x01
#define SCAT_VERSION                0x40
#define SCAT_CLOCK_CONTROL          0x41
#define SCAT_PERIPHERAL_CONTROL     0x44
#define SCAT_MISCELLANEOUS_STATUS   0x45
#define SCAT_POWER_MANAGEMENT       0x46
#define SCAT_ROM_ENABLE             0x48
#define SCAT_RAM_WRITE_PROTECT      0x49
#define SCAT_SHADOW_RAM_ENABLE_1    0x4A
#define SCAT_SHADOW_RAM_ENABLE_2    0x4B
#define SCAT_SHADOW_RAM_ENABLE_3    0x4C
#define SCAT_DRAM_CONFIGURATION     0x4D
#define SCAT_EXTENDED_BOUNDARY      0x4E
#define SCAT_EMS_CONTROL            0x4F

#define SCATSX_LAPTOP_FEATURES          0x60
#define SCATSX_FAST_VIDEO_CONTROL       0x61
#define SCATSX_FAST_VIDEORAM_ENABLE     0x62
#define SCATSX_HIGH_PERFORMANCE_REFRESH 0x63
#define SCATSX_CAS_TIMING_FOR_DMA       0x64


typedef struct {
    uint8_t	regs_2x8;
    uint8_t	regs_2x9;
} ems_page_t;

typedef struct {
    int		type;

    int		indx;
    uint8_t	regs[256];
    uint8_t	reg_2xA;
    uint8_t	port_92;

    uint32_t	xms_bound;

    int		external_is_RAS;

    ems_page_t	page[32];

    mem_map_t	low_mapping[32];
    mem_map_t	high_mapping[16];
    mem_map_t	remap_mapping[6];
    mem_map_t	efff_mapping[44];
    mem_map_t	romcs_mapping[8];
    mem_map_t	ems_mapping[32];
} scat_t;


static const uint8_t max_map[32] = {
    0, 1,  1,  1,  2,  3,  4,  8,
    4, 8, 12, 16, 20, 24, 28, 32,
    0, 5,  9, 13,  6, 10,  0,  0,
    0, 0,  0,  0,  0,  0,  0,  0
};
static const uint8_t max_map_sx[32] = {
    0,  1,  2,  1,  3,  4,  6, 10,
    5,  9, 13,  4,  8, 12, 16, 14,
   18, 22, 26, 20, 24, 28, 32, 18,
   20, 32,  0,  0,  0,  0,  0,  0
};
static const uint8_t scatsx_external_is_RAS[33] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    0
};


static uint8_t	scat_in(uint16_t port, priv_t priv);
static void	scat_out(uint16_t port, uint8_t val, priv_t priv);


static void
romcs_state_update(scat_t *dev, uint8_t val)
{
    int i;

    for (i = 0; i < 4; i++) {
	if (val & 1) {
		mem_map_enable(&dev->romcs_mapping[i << 1]);
		mem_map_enable(&dev->romcs_mapping[(i << 1) + 1]);
	} else {
		mem_map_disable(&dev->romcs_mapping[i << 1]);
		mem_map_disable(&dev->romcs_mapping[(i << 1) + 1]);
	}
	val >>= 1;
    }

    for (i = 0; i < 4; i++) {
	if (val & 1) {
		mem_map_enable(&bios_mapping[i << 1]);
		mem_map_enable(&bios_mapping[(i << 1) + 1]);
	} else {
		mem_map_disable(&bios_mapping[i << 1]);
		mem_map_disable(&bios_mapping[(i << 1) + 1]);
	}
	val >>= 1;
    }
}


static void
shadow_state_update(scat_t *dev)
{
    int i, val;

    if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0xF) < 4) {
	/* Less than 1MB low memory, no shadow RAM available. */
        for (i = 0; i < 24; i++)
            mem_set_mem_state((i + 40) << 14, 0x4000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    } else {
	for (i = 0; i < 24; i++) {
		val = ((dev->regs[SCAT_SHADOW_RAM_ENABLE_1 + (i >> 3)] >> (i & 7)) & 1) ? MEM_READ_INTERNAL | MEM_WRITE_INTERNAL : MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
		mem_set_mem_state((i + 40) << 14, 0x4000, val);
	}
    }

    flushmmucache();
}


static void
set_xms_bound(scat_t *dev, uint8_t val)
{
    uint32_t xms_max = ((dev->regs[SCAT_VERSION] & 0xf0) != 0 && ((val & 0x10) != 0)) || (dev->regs[SCAT_VERSION] >= 4) ? 0xfe0000 : 0xfc0000;
    int i;

    switch (val & 0x0f) {
	case 1:
		dev->xms_bound = 0x100000;
		break;

	case 2:
		dev->xms_bound = 0x140000;
		break;

	case 3:
		dev->xms_bound = 0x180000;
		break;

	case 4:
		dev->xms_bound = 0x200000;
		break;

	case 5:
		dev->xms_bound = 0x300000;
		break;

	case 6:
		dev->xms_bound = 0x400000;
		break;

	case 7:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0x600000 : 0x500000;
		break;

	case 8:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0x800000 : 0x700000;
		break;

	case 9:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0xa00000 : 0x800000;
		break;

	case 10:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0xc00000 : 0x900000;
		break;

	case 11:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0xe00000 : 0xa00000;
		break;

	case 12:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? xms_max : 0xb00000;
		break;

	case 13:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? xms_max : 0xc00000;
		break;

	case 14:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? xms_max : 0xd00000;
		break;

	case 15:
		dev->xms_bound = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? xms_max : 0xf00000;
		break;

	default:
		dev->xms_bound = xms_max;
		break;
    }

    if ((((dev->regs[SCAT_VERSION] & 0xf0) == 0) && (val & 0x40) == 0 && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) == 3) ||
        (((dev->regs[SCAT_VERSION] & 0xf0) != 0) && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 3)) {
	if ((val & 0x0f) == 0 || dev->xms_bound > 0x160000)
		dev->xms_bound = 0x160000;

	if (dev->xms_bound > 0x100000)
		mem_set_mem_state(0x100000, dev->xms_bound - 0x100000,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);

	if (dev->xms_bound < 0x160000)
		mem_set_mem_state(dev->xms_bound, 0x160000 - dev->xms_bound,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    } else {
	if (dev->xms_bound > xms_max)
		dev->xms_bound = xms_max;

	if (dev->xms_bound > 0x100000)
		mem_set_mem_state(0x100000, dev->xms_bound - 0x100000,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);

	if (dev->xms_bound < ((uint32_t)mem_size << 10))
		mem_set_mem_state(dev->xms_bound, (mem_size << 10) - dev->xms_bound,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    }

    mem_map_set_addr(&dev->low_mapping[31], 0xf80000,
		     ((dev->regs[SCAT_VERSION] & 0xf0) != 0 && ((val & 0x10) != 0)) ||
		     (dev->regs[SCAT_VERSION] >= 4) ? 0x60000 : 0x40000);
    if (dev->regs[SCAT_VERSION] & 0xf0) {
	for (i = 0; i < 8; i++) {
		if (val & 0x10)
			mem_map_disable(&dev->high_mapping[i]);
		else
			mem_map_enable(&dev->high_mapping[i]);
	}
    }
}


static uint32_t
get_addr(scat_t *dev, uint32_t addr, ems_page_t *p)
{
    int nbanks_2048k, nbanks_512k;
    uint32_t addr2;
    int nbank;

    if (p && (dev->regs[SCAT_EMS_CONTROL] & 0x80) && (p->regs_2x9 & 0x80))
	addr = (addr & 0x3fff) | (((p->regs_2x9 & 3) << 8) | p->regs_2x8) << 14;

    if ((dev->regs[SCAT_VERSION] & 0xf0) == 0) {
	switch((dev->regs[SCAT_EXTENDED_BOUNDARY] & ((dev->regs[SCAT_VERSION] & 0x0f) > 3 ? 0x40 : 0)) | (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f)) {
		case 0x41:
			nbank = addr >> 19;
			if (nbank < 4)
				nbank = 1;
			else if (nbank == 4)
				nbank = 0;
			else
				nbank -= 3;
			break;

		case 0x42:
			nbank = addr >> 19;
			if (nbank < 8)
				nbank = 1 + (nbank >> 2);
			else if (nbank == 8)
				nbank = 0;
			else
				nbank -= 6;
			break;

		case 0x43:
			nbank = addr >> 19;
			if (nbank < 12)
				nbank = 1 + (nbank >> 2);
			else if (nbank == 12)
				nbank = 0;
			else
				nbank -= 9;
			break;

		case 0x44:
			nbank = addr >> 19;
			if (nbank < 4)
				nbank = 2;
			else if (nbank < 6)
				nbank -= 4;
			else
				nbank -= 3;
			break;

		case 0x45:
			nbank = addr >> 19;
			if (nbank < 8)
				nbank = 2 + (nbank >> 2);
			else if (nbank < 10)
				nbank -= 8;
			else
				nbank -= 6;
			break;

		default:
			nbank = addr >> (((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) < 8 && (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0) ? 19 : 21);
			break;
	}

	nbank &= (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) ? 7 : 3;

	if ((dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0 &&
	    (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) == 3 &&
	    nbank == 2 && (addr & 0x7ffff) < 0x60000 && mem_size > 640) {
		nbank = 1;
		addr ^= 0x70000;
	}

	if (dev->external_is_RAS && (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) == 0) {
		if (nbank == 3)
			nbank = 7;
		else
			return 0xffffffff;
	} else if (!dev->external_is_RAS && dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) {
		switch(nbank) {
			case 7:
				nbank = 3;
				break;

			/* Note - In the following cases, the chipset accesses multiple memory banks
				  at the same time, so it's impossible to predict which memory bank
				  is actually accessed. */
			case 5:
			case 1:
				nbank = 1;
				break;

			case 3:
				nbank = 2;
				break;

			default:
				nbank = 0;
				break;
		}
	}

	if ((dev->regs[SCAT_VERSION] & 0x0f) > 3 && (mem_size > 2048) && (mem_size & 1536)) {
		if ((mem_size & 1536) == 512) {
			if (nbank == 0)
				addr &= 0x7ffff;
			else
				addr = 0x80000 + ((addr & 0x1fffff) | ((nbank - 1) << 21));
		} else {
			if (nbank < 2)
				addr = (addr & 0x7ffff) | (nbank << 19);
			else
				addr = 0x100000 + ((addr & 0x1fffff) | ((nbank - 2) << 21));
		}
	} else {
		if (mem_size <= ((dev->regs[SCAT_VERSION] & 0x0f) > 3 ? 2048 : 4096) && (((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) < 8) || dev->external_is_RAS)) {
			nbanks_2048k = 0;
			nbanks_512k = mem_size >> 9;
		} else { 
			nbanks_2048k = mem_size >> 11;
			nbanks_512k = (mem_size & 1536) >> 9;
		}

		if (nbank < nbanks_2048k || (nbanks_2048k > 0 && nbank >= nbanks_2048k + nbanks_512k + ((mem_size & 511) >> 7))) {
			addr &= 0x1fffff;
			addr |= (nbank << 21);
		} else if (nbank < nbanks_2048k + nbanks_512k || nbank >= nbanks_2048k + nbanks_512k + ((mem_size & 511) >> 7)) {
			addr &= 0x7ffff;
			addr |= (nbanks_2048k << 21) | ((nbank - nbanks_2048k) << 19);
		} else {
			addr &= 0x1ffff;
			addr |= (nbanks_2048k << 21) | (nbanks_512k << 19) | ((nbank - nbanks_2048k - nbanks_512k) << 17);
		}
	}
    } else {
	switch(dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) {
		case 0x02:
		case 0x04:
			nbank = addr >> 19;
			if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else
				addr2 = addr >> 10;
			break;

		case 0x03:
			nbank = addr >> 19;
			if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) == 2 && (addr & 0x7ffff) < 0x60000) {
				addr ^= 0x1f0000;
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else
				addr2 = addr >> 10;
			break;

		case 0x05:
			nbank = addr >> 19;
			if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 4) {
				nbank = (addr >> 10) & 3;
				addr2 = addr >> 12;
			} else
				addr2 = addr >> 10;
			break;

		case 0x06:
			nbank = addr >> 19;
			if (nbank < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else {
				nbank = 2 + ((addr - 0x100000) >> 21);
				addr2 = (addr - 0x100000) >> 11;
			}
			break;

		case 0x07:
		case 0x0f:
			nbank = addr >> 19;
			if (nbank < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else if (nbank < 10) {
				nbank = 2 + (((addr - 0x100000) >> 11) & 1);
				addr2 = (addr - 0x100000) >> 12;
			} else {
				nbank = 4 + ((addr - 0x500000) >> 21);
				addr2 = (addr - 0x500000) >> 11;
			}
			break;

		case 0x08:
			nbank = addr >> 19;
			if (nbank < 4) {
				nbank = 1;
				addr2 = addr >> 11;
			} else if (nbank == 4) {
				nbank = 0;
				addr2 = addr >> 10;
			} else {
				nbank -= 3;
				addr2 = addr >> 10;
			}
			break;

		case 0x09:
			nbank = addr >> 19;
			if (nbank < 8) {
				nbank = 1 + ((addr >> 11) & 1);
				addr2 = addr >> 12;
			} else if (nbank == 8) {
				nbank = 0;
				addr2 = addr >> 10;
			} else {
				nbank -= 6;
				addr2 = addr >> 10;
			}
			break;

		case 0x0a:
			nbank = addr >> 19;
			if (nbank < 8) {
				nbank = 1 + ((addr >> 11) & 1);
				addr2 = addr >> 12;
			} else if (nbank < 12) {
				nbank = 3;
				addr2 = addr >> 11;
			} else if (nbank == 12) {
				nbank = 0;
				addr2 = addr >> 10;
			} else {
				nbank -= 9;
				addr2 = addr >> 10;
			}
			break;

		case 0x0b:
			nbank = addr >> 21;
			addr2 = addr >> 11;
			break;

		case 0x0c:
		case 0x0d:
			nbank = addr >> 21;
			if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2) {
				nbank = (addr >> 11) & 1;
				addr2 = addr >> 12;
			} else
				addr2 = addr >> 11;
			break;

		case 0x0e:
		case 0x13:
			nbank = addr >> 21;
			if ((nbank & (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 4) {
				nbank = (addr >> 11) & 3;
				addr2 = addr >> 13;
			} else
				addr2 = addr >> 11;
			break;

		case 0x10:
		case 0x11:
			nbank = addr >> 19;
			if (nbank < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else if (nbank < 10) {
				nbank = 2 + (((addr - 0x100000) >> 11) & 1);
				addr2 = (addr - 0x100000) >> 12;
			} else if (nbank < 18) {
				nbank = 4 + (((addr - 0x500000) >> 11) & 1);
				addr2 = (addr - 0x500000) >> 12;
			} else {
				nbank = 6 + ((addr - 0x900000) >> 21);
				addr2 = (addr - 0x900000) >> 11;
			}
			break;

		case 0x12:
			nbank = addr >> 19;
			if (nbank < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else if (nbank < 10) {
				nbank = 2 + (((addr - 0x100000) >> 11) & 1);
				addr2 = (addr - 0x100000) >> 12;
			} else {
				nbank = 4 + (((addr - 0x500000) >> 11) & 3);
				addr2 = (addr - 0x500000) >> 13;
			}
			break;

		case 0x14:
		case 0x15:
			nbank = addr >> 21;
			if ((nbank & 7) < 4) {
				nbank = (addr >> 11) & 3;
				addr2 = addr >> 13;
			} else if ((nbank & 7) < 6) {
				nbank = 4 + (((addr - 0x800000) >> 11) & 1);
				addr2 = (addr - 0x800000) >> 12;
			} else {
				nbank = 6 + (((addr - 0xc00000) >> 11) & 3);
				addr2 = (addr - 0xc00000) >> 13;
			}
			break;

		case 0x16:
			nbank = ((addr >> 21) & 4) | ((addr >> 11) & 3);
			addr2 = addr >> 13;
			break;

		case 0x17:
			if (dev->external_is_RAS && (addr & 0x800) == 0)
				return 0xffffffff;
			nbank = addr >> 19;
			if (nbank < 2) {
				nbank = (addr >> 10) & 1;
				addr2 = addr >> 11;
			} else {
				nbank = 2 + ((addr - 0x100000) >> 23);
				addr2 = (addr - 0x100000) >> 12;
			}
			break;

		case 0x18:
			if (dev->external_is_RAS && (addr & 0x800) == 0)
				return 0xffffffff;
			nbank = addr >> 21;
			if (nbank < 4) {
				nbank = 1;
				addr2 = addr >> 12;
			} else if (nbank == 4) {
				nbank = 0;
				addr2 = addr >> 11;
			} else {
				nbank -= 3;
				addr2 = addr >> 11;
			}
			break;

		case 0x19:
			if (dev->external_is_RAS && (addr & 0x800) == 0)
				return 0xffffffff;
			nbank = addr >> 23;
			if ((nbank & 3) < 2) {
				nbank = (addr >> 12) & 1;
				addr2 = addr >> 13;
			} else
				addr2 = addr >> 12;
			break;

		default:
			if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) < 6) {
				nbank = addr >> 19;
				addr2 = (addr >> 10) & 0x1ff;
			} else if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) < 0x17) {
				nbank = addr >> 21;
				addr2 = (addr >> 11) & 0x3ff;
			} else {
				nbank = addr >> 23;
				addr2 = (addr >> 12) & 0x7ff;
			}
			break;
	}

	nbank &= (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) ? 7 : 3;

	if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) > 0x16 && nbank == 3)
		return 0xffffffff;

	if (dev->external_is_RAS && (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) == 0) {
		if (nbank == 3)
			nbank = 7;
		else
			return 0xffffffff;
	} else if (!dev->external_is_RAS && dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x80) {
		switch(nbank) {
			case 7:
				nbank = 3;
				break;

			/* Note - In the following cases, the chipset accesses multiple memory banks
			  	at the same time, so it's impossible to predict which memory bank
			  	is actually accessed. */
			case 5:
			case 1:
				nbank = 1;
				break;

			case 3:
				nbank = 2;
				break;

			default:
				nbank = 0;
				break;
		}
	}

	switch(mem_size & ~511) {
		case 1024:
		case 1536:
			addr &= 0x3ff;
			if (nbank < 2)
				addr |= (nbank << 10) | ((addr2 & 0x1ff) << 11);
			else
				addr |= ((addr2 & 0x1ff) << 10) | (nbank << 19);
			break;

		case 2048:
			if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 5) {
				addr &= 0x3ff;
				if (nbank < 4)
					addr |= (nbank << 10) | ((addr2 & 0x1ff) << 12);
				else
					addr |= ((addr2 & 0x1ff) << 10) | (nbank << 19);
			} else {
				addr &= 0x7ff;
				addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			}
			break;

		case 2560:
			if (nbank == 0)
				addr = (addr & 0x3ff) | ((addr2 & 0x1ff) << 10);
			else {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr = addr + 0x80000 + ((addr2 << 11) | ((nbank - 1) << 21));
			}
			break;

		case 3072:
			if (nbank < 2)
				addr = (addr & 0x3ff) | (nbank << 10) | ((addr2 & 0x1ff) << 11);
			else
				addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 11) | ((nbank - 2) << 21));
			break;

		case 4096:
		case 6144:
			addr &= 0x7ff;
			if (nbank < 2)
				addr |= (nbank << 11) | ((addr2 & 0x3ff) << 12);
			else
				addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			break;

		case 4608:
			if (((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) >= 8 && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) <= 0x0a) || ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 0x18)) {
				if (nbank == 0)
					addr = (addr & 0x3ff) | ((addr2 & 0x1ff) << 10);
				else if (nbank < 3)
					addr = 0x80000 + ((addr & 0x7ff) | ((nbank - 1) << 11) | ((addr2 & 0x3ff) << 12));
				else
					addr = 0x480000 + ((addr & 0x3ff) | ((addr2 & 0x1ff) << 10) | ((nbank - 3) << 19));
			} else if (nbank == 0)
				addr = (addr & 0x3ff) | ((addr2 & 0x1ff) << 10);
			else {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr = addr + 0x80000 + ((addr2 << 11) | ((nbank - 1) << 21));
			}
			break;

		case 5120:
		case 7168:
			if (nbank < 2)
				addr = (addr & 0x3ff) | (nbank << 10) | ((addr2 & 0x1ff) << 11);
			else if (nbank < 4)
				addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 12) | ((nbank & 1) << 11));
			else
				addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 11) | ((nbank - 2) << 21));
			break;

		case 6656:
			if (((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) >= 8 && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) <= 0x0a) || ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 0x18)) {
				if (nbank == 0)
					addr = (addr & 0x3ff) | ((addr2 & 0x1ff) << 10);
				else if (nbank < 3)
					addr = 0x80000 + ((addr & 0x7ff) | ((nbank - 1) << 11) | ((addr2 & 0x3ff) << 12));
				else if (nbank == 3)
					addr = 0x480000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 11));
				else
					addr = 0x680000 + ((addr & 0x3ff) | ((addr2 & 0x1ff) << 10) | ((nbank - 4) << 19));
			} else if (nbank == 0)
				addr = (addr & 0x3ff) | ((addr2 & 0x1ff) << 10);
			else if (nbank == 1) {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr = addr + 0x80000 + (addr2 << 11);
			} else {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr = addr + 0x280000 + ((addr2 << 12) | ((nbank & 1) << 11) | (((nbank - 2) & 6) << 21));
			}
			break;

		case 8192:
			addr &= 0x7ff;
			if (nbank < 4)
				addr |= (nbank << 11) | ((addr2 & 0x3ff) << 13);
			else
				addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			break;

		case 9216:
			if (nbank < 2)
				addr = (addr & 0x3ff) | (nbank << 10) | ((addr2 & 0x1ff) << 11);
			else if (dev->external_is_RAS) {
				if (nbank < 6)
					addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 12) | ((nbank & 1) << 11));
				else
					addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 11) | ((nbank - 2) << 21));
			} else
				addr = 0x100000 + ((addr & 0xfff) | ((addr2 & 0x7ff) << 12) | ((nbank - 2) << 23));
			break;

		case 10240:
			if (dev->external_is_RAS) {
				addr &= 0x7ff;
				if (nbank < 4)
					addr |= (nbank << 11) | ((addr2 & 0x3ff) << 13);
				else
					addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			} else if (nbank == 0)
				addr = (addr & 0x7ff) | ((addr2 & 0x3ff) << 11);
			else {
				addr &= 0xfff;
				addr2 &= 0x7ff;
				addr = addr + 0x200000 + ((addr2 << 12) | ((nbank - 1) << 23));
			}
			break;

		case 11264:
			if (nbank < 2)
				addr = (addr & 0x3ff) | (nbank << 10) | ((addr2 & 0x1ff) << 11);
			else if (nbank < 6)
				addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 12) | ((nbank & 1) << 11));
			else
				addr = 0x100000 + ((addr & 0x7ff) | ((addr2 & 0x3ff) << 11) | ((nbank - 2) << 21));
			break;

		case 12288:
			if (dev->external_is_RAS) {
				addr &= 0x7ff;
				if (nbank < 4)
					addr |= (nbank << 11) | ((addr2 & 0x3ff) << 13);
				else if (nbank < 6)
					addr |= ((nbank & 1) << 11) | ((addr2 & 0x3ff) << 12) | ((nbank & 4) << 21);
				else
					addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			} else {
				if (nbank < 2)
					addr = (addr & 0x7ff) | (nbank << 11) | ((addr2 & 0x3ff) << 12);
				else
					addr = 0x400000 + ((addr & 0xfff) | ((addr2 & 0x7ff) << 12) | ((nbank - 2) << 23));
			}
			break;

		case 13312:
			if (nbank < 2)
				addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
			else if (nbank < 4)
				addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 12) | ((nbank & 1) << 11));
			else
				addr = 0x500000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 13) | ((nbank & 3) << 11));
			break;

		case 14336:
			addr &= 0x7ff;
			if (nbank < 4)
				addr |= (nbank << 11) | ((addr2 & 0x3ff) << 13);
			else if (nbank < 6)
				addr |= ((nbank & 1) << 11) | ((addr2 & 0x3ff) << 12) | ((nbank & 4) << 21);
			else
				addr |= ((addr2 & 0x3ff) << 11) | (nbank << 21);
			break;

		case 16384:
			if (dev->external_is_RAS) {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr |= ((nbank & 3) << 11) | (addr2 << 13) | ((nbank & 4) << 21);
			} else {
				addr &= 0xfff;
				addr2 &= 0x7ff;
				if (nbank < 2)
					addr |= (addr2 << 13) | (nbank << 12);
				else
					addr |= (addr2 << 12) | (nbank << 23);
			}
			break;

		default:
			if (mem_size < 2048 || ((mem_size & 1536) == 512) || (mem_size == 2048 && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) < 6)) {
				addr &= 0x3ff;
				addr2 &= 0x1ff;
				addr |= (addr2 << 10) | (nbank << 19);
			} else if (mem_size < 8192 || (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) < 0x17) {
				addr &= 0x7ff;
				addr2 &= 0x3ff;
				addr |= (addr2 << 11) | (nbank << 21);
			} else {
				addr &= 0xfff;
				addr2 &= 0x7ff;
				addr |= (addr2 << 12) | (nbank << 23);
			}
			break;
	}
    }

    return(addr);
}


static void
set_global_EMS_state(scat_t *dev, int state)
{
    uint32_t base_addr, virt_addr;
    int i, conf;

    for (i = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0 : 24; i < 32; i++) {
	base_addr = (i + 16) << 14;

	if (i >= 24)
		base_addr += 0x30000;

	if (state && (dev->page[i].regs_2x9 & 0x80)) {
		virt_addr = get_addr(dev, base_addr, &dev->page[i]);
		if (i < 24)
			mem_map_disable(&dev->efff_mapping[i]);
		else
			mem_map_disable(&dev->efff_mapping[i + 12]);
		mem_map_enable(&dev->ems_mapping[i]);

		if (virt_addr < ((uint32_t)mem_size << 10))
			mem_map_set_exec(&dev->ems_mapping[i], ram + virt_addr);
		else
			mem_map_set_exec(&dev->ems_mapping[i], NULL);
	} else {
		mem_map_set_exec(&dev->ems_mapping[i], ram + base_addr);
		mem_map_disable(&dev->ems_mapping[i]);

		conf = (dev->regs[SCAT_VERSION] & 0xf0) ? (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f)
							: (dev->regs[SCAT_DRAM_CONFIGURATION] & 0xf) | ((dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40) >> 2);
		if (i < 24) {
			if (conf > 1 || (conf == 1 && i < 16))
				mem_map_enable(&dev->efff_mapping[i]);
			else
				mem_map_disable(&dev->efff_mapping[i]);
		} else if (conf > 3 || ((dev->regs[SCAT_VERSION] & 0xf0) != 0 && conf == 2))
			mem_map_enable(&dev->efff_mapping[i + 12]);
		else
			mem_map_disable(&dev->efff_mapping[i + 12]);
	}
    }

    flushmmucache();
}


static void
memmap_state_update(scat_t *dev)
{
    uint32_t addr;
    int i;

    for (i = (((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0 : 16); i < 44; i++) {
	addr = get_addr(dev, 0x40000 + (i << 14), NULL);
	mem_map_set_exec(&dev->efff_mapping[i],
			 addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);
    }

    addr = get_addr(dev, 0, NULL);
    mem_map_set_exec(&dev->low_mapping[0],
		     addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);

    addr = get_addr(dev, 0xf0000, NULL);
    mem_map_set_exec(&dev->low_mapping[1],
		     addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);

    for (i = 2; i < 32; i++) {
	addr = get_addr(dev, i << 19, NULL);
	mem_map_set_exec(&dev->low_mapping[i],
			 addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);
    }

    if ((dev->regs[SCAT_VERSION] & 0xf0) == 0) {
	for (i = 0; i < max_map[(dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) | ((dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40) >> 2)]; i++)
		mem_map_enable(&dev->low_mapping[i]);

	for (; i < 32; i++)
		mem_map_disable(&dev->low_mapping[i]);

	for (i = 24; i < 36; i++) {
		if (((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) | (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40)) < 4)
			mem_map_disable(&dev->efff_mapping[i]);
		else
			mem_map_enable(&dev->efff_mapping[i]);
	}
    } else {
	for (i = 0; i < max_map_sx[dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f]; i++)
		mem_map_enable(&dev->low_mapping[i]);

	for (; i < 32; i++)
		mem_map_disable(&dev->low_mapping[i]);

	for(i = 24; i < 36; i++) {
		if ((dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) < 2 || (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 3)
			mem_map_disable(&dev->efff_mapping[i]);
		else
			mem_map_enable(&dev->efff_mapping[i]);
	}
    }

    if ((((dev->regs[SCAT_VERSION] & 0xf0) == 0) &&
	  (dev->regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0) || ((dev->regs[SCAT_VERSION] & 0xf0) != 0)) {
	if ((((dev->regs[SCAT_VERSION] & 0xf0) == 0) &&
	      (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) == 3) ||
	      (((dev->regs[SCAT_VERSION] & 0xf0) != 0) && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) == 3)) {
		mem_map_disable(&dev->low_mapping[2]);

		for (i = 0; i < 6; i++) {
			addr = get_addr(dev, 0x100000 + (i << 16), NULL);
			mem_map_set_exec(&dev->remap_mapping[i],
					 addr < ((uint32_t)mem_size << 10) ? ram + addr : NULL);
			mem_map_enable(&dev->remap_mapping[i]);
		}
	} else {
		for (i = 0; i < 6; i++)
			mem_map_disable(&dev->remap_mapping[i]);

		if ((((dev->regs[SCAT_VERSION] & 0xf0) == 0) &&
		      (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x0f) > 4) || (((dev->regs[SCAT_VERSION] & 0xf0) != 0) && (dev->regs[SCAT_DRAM_CONFIGURATION] & 0x1f) > 3))
			mem_map_enable(&dev->low_mapping[2]);
	}
    } else {
	for (i = 0; i < 6; i++)
		mem_map_disable(&dev->remap_mapping[i]);

	mem_map_enable(&dev->low_mapping[2]);
    }

    set_global_EMS_state(dev, dev->regs[SCAT_EMS_CONTROL] & 0x80);
}


static uint8_t
scat_in(uint16_t port, priv_t priv)
{
    scat_t *dev = (scat_t *)priv;
    uint8_t ret = 0xff, indx;

    switch (port) {
	case 0x23:
		switch (dev->indx) {
			case SCAT_MISCELLANEOUS_STATUS:
				ret = (dev->regs[dev->indx] & 0x3f) | (~nmi_mask & 0x80) | ((mem_a20_key & 2) << 5);
				break;

			case SCAT_DRAM_CONFIGURATION:
				if ((dev->regs[SCAT_VERSION] & 0xf0) == 0)
					ret = (dev->regs[dev->indx] & 0x8f) | (config.cpu_waitstates == 1 ? 0 : 0x10);
				else
					ret = dev->regs[dev->indx];
				break;

			case SCAT_EXTENDED_BOUNDARY:
				ret = dev->regs[dev->indx];
				if ((dev->regs[SCAT_VERSION] & 0xf0) == 0) {
					if ((dev->regs[SCAT_VERSION] & 0x0f) >= 4)
						ret |= 0x80;
					else
						ret &= 0xaf;
				}
				break;

			default:
				ret = dev->regs[dev->indx];
				break;
		}
		break;

	case 0x92:
		ret = dev->port_92;
		break;

	case 0x208:
	case 0x218:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4))) {
			if ((dev->regs[SCAT_VERSION] & 0xf0) == 0)
				indx = dev->reg_2xA & 0x1f;
			else
				indx = ((dev->reg_2xA & 0x40) >> 4) + (dev->reg_2xA & 0x3) + 24;
			ret = dev->page[indx].regs_2x8;
		}
		break;

	case 0x209:
	case 0x219:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4))) {
			if ((dev->regs[SCAT_VERSION] & 0xf0) == 0)
				indx = dev->reg_2xA & 0x1f;
			else
				indx = ((dev->reg_2xA & 0x40) >> 4) + (dev->reg_2xA & 0x3) + 24;
			ret = dev->page[indx].regs_2x9;
		}
		break;

	case 0x20a:
	case 0x21a:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
			ret = dev->reg_2xA;
		break;
    }

    return(ret);
}


static void
scat_out(uint16_t port, uint8_t val, priv_t priv)
{
    scat_t *dev = (scat_t *)priv;
    uint8_t reg_valid = 0,
	    shadow_update = 0,
	    map_update = 0,
	    indx;
    uint32_t base_addr, virt_addr;

    switch (port) {
	case 0x22:
		dev->indx = val;
		break;

	case 0x23:
		switch (dev->indx) {
			case SCAT_DMA_WAIT_STATE_CONTROL:
			case SCAT_CLOCK_CONTROL:
			case SCAT_PERIPHERAL_CONTROL:
				reg_valid = 1;
				break;

			case SCAT_EMS_CONTROL:
				io_removehandler(0x0208, 3, scat_in,NULL,NULL, scat_out,NULL,NULL, dev);
				io_removehandler(0x0218, 3, scat_in,NULL,NULL, scat_out,NULL,NULL, dev);
				if (val & 0x40) {
					if (val & 1) 
						io_sethandler(0x0218, 3, scat_in,NULL,NULL, scat_out,NULL,NULL, dev);
					else 
						io_sethandler(0x0208, 3, scat_in,NULL,NULL, scat_out,NULL,NULL, dev);
				}
				set_global_EMS_state(dev, val & 0x80);
				reg_valid = 1;
				break;

			case SCAT_POWER_MANAGEMENT:
				/* TODO - Only use AUX parity disable bit for this version.
					  Other bits should be implemented later. */
				val &= (dev->regs[SCAT_VERSION] & 0xf0) == 0 ? 0x40 : 0x60;
				reg_valid = 1;
				break;

			case SCAT_DRAM_CONFIGURATION:
				map_update = 1;

				if ((dev->regs[SCAT_VERSION] & 0xf0) == 0) {
					config.cpu_waitstates = (val & 0x70) == 0 ? 1 : 2;
					cpu_update_waitstates();
				}

				reg_valid = 1;
				shadow_update = 1;
				break;

			case SCAT_EXTENDED_BOUNDARY:
				if ((dev->regs[SCAT_VERSION] & 0xf0) == 0) {
					if (dev->regs[SCAT_VERSION] < 4) {
						val &= 0xbf;
						set_xms_bound(dev, val & 0x0f);
					} else {
						val = (val & 0x7f) | 0x80;
						set_xms_bound(dev, val & 0x4f);
					}
				} else
					set_xms_bound(dev, val & 0x1f);

				mem_set_mem_state(0x40000, 0x60000, (val & 0x20) ? MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL : MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
				if ((val ^ dev->regs[SCAT_EXTENDED_BOUNDARY]) & 0xc0)
					map_update = 1;
				reg_valid = 1;
				break;

			case SCAT_ROM_ENABLE:
				romcs_state_update(dev, val);
				reg_valid = 1;
				break;

			case SCAT_RAM_WRITE_PROTECT:
				reg_valid = 1;
				flushmmucache_cr3();
				break;

			case SCAT_SHADOW_RAM_ENABLE_1:
			case SCAT_SHADOW_RAM_ENABLE_2:
			case SCAT_SHADOW_RAM_ENABLE_3:
				reg_valid = 1;
				shadow_update = 1;
				break;

			case SCATSX_LAPTOP_FEATURES:
				if ((dev->regs[SCAT_VERSION] & 0xf0) != 0) {
					val = (val & ~8) | (dev->regs[SCATSX_LAPTOP_FEATURES] & 8);
					reg_valid = 1;
				}
				break;

			case SCATSX_FAST_VIDEO_CONTROL:
			case SCATSX_FAST_VIDEORAM_ENABLE:
			case SCATSX_HIGH_PERFORMANCE_REFRESH:
			case SCATSX_CAS_TIMING_FOR_DMA:
				if ((dev->regs[SCAT_VERSION] & 0xf0) != 0)
					reg_valid = 1;
				break;

			default:
				break;
		}

		if (reg_valid)
			dev->regs[dev->indx] = val;

		if (shadow_update)
			shadow_state_update(dev);

		if (map_update)
			memmap_state_update(dev);
		break;

	case 0x92:
		if ((mem_a20_alt ^ val) & 2) {
			mem_a20_alt = val & 2;
			mem_a20_recalc();
		}
		if ((~dev->port_92 & val) & 1) {
			cpu_reset(0);
			cpu_set_edx();
		}
		dev->port_92 = val;
		break;

	case 0x208:
	case 0x218:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4))) {
			if ((dev->regs[SCAT_VERSION] & 0xf0) == 0)
				indx = dev->reg_2xA & 0x1f;
			else
				indx = ((dev->reg_2xA & 0x40) >> 4) + (dev->reg_2xA & 0x3) + 24;
			dev->page[indx].regs_2x8 = val;
			base_addr = (indx + 16) << 14;
			if (indx >= 24)
				base_addr += 0x30000;

			if ((dev->regs[SCAT_EMS_CONTROL] & 0x80) && (dev->page[indx].regs_2x9 & 0x80)) {
				virt_addr = get_addr(dev, base_addr, &dev->page[indx]);
				if (virt_addr < ((uint32_t)mem_size << 10))
					mem_map_set_exec(&dev->ems_mapping[indx], ram + virt_addr);
				else
					mem_map_set_exec(&dev->ems_mapping[indx], NULL);
				flushmmucache();
			}
		}
		break;

	case 0x209:
	case 0x219:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4))) {
			if ((dev->regs[SCAT_VERSION] & 0xf0) == 0)
				indx = dev->reg_2xA & 0x1f;
			else
				indx = ((dev->reg_2xA & 0x40) >> 4) + (dev->reg_2xA & 0x3) + 24;
			dev->page[indx].regs_2x9 = val;
			base_addr = (indx + 16) << 14;
			if (indx >= 24)
				base_addr += 0x30000;

			if (dev->regs[SCAT_EMS_CONTROL] & 0x80) {
				if (val & 0x80) {
					virt_addr = get_addr(dev, base_addr, &dev->page[indx]);
					if (indx < 24)
						mem_map_disable(&dev->efff_mapping[indx]);
					else
						mem_map_disable(&dev->efff_mapping[indx + 12]);
					if (virt_addr < ((uint32_t)mem_size << 10))
						mem_map_set_exec(&dev->ems_mapping[indx], ram + virt_addr);
					else
						mem_map_set_exec(&dev->ems_mapping[indx], NULL);
					mem_map_enable(&dev->ems_mapping[indx]);
				} else {
					mem_map_set_exec(&dev->ems_mapping[indx], ram + base_addr);
					mem_map_disable(&dev->ems_mapping[indx]);
					if (indx < 24)
						mem_map_enable(&dev->efff_mapping[indx]);
					else
						mem_map_enable(&dev->efff_mapping[indx + 12]);
				}

				flushmmucache();
			}

			if (dev->reg_2xA & 0x80)
				dev->reg_2xA = (dev->reg_2xA & 0xe0) | ((dev->reg_2xA + 1) & (((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? 0x1f : 3));
		}
		break;

	case 0x20a:
	case 0x21a:
		if ((dev->regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
			dev->reg_2xA = ((dev->regs[SCAT_VERSION] & 0xf0) == 0) ? val : val & 0xc3;
		break;
    }
}


static uint8_t
scat_readb(uint32_t addr, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    scat_t *dev = (scat_t *)map->dev;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint8_t val = 0xff;

    addr = get_addr(dev, addr, page);
    if (addr < ((uint32_t)mem_size << 10))
	val = ram[addr];

    return(val);
}


static uint16_t
scat_readw(uint32_t addr, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    scat_t *dev = (scat_t *)map->dev;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint16_t val = 0xffff;

    addr = get_addr(dev, addr, page);
    if (addr < ((uint32_t)mem_size << 10))
	val = *(uint16_t *)&ram[addr];

    return(val);
}


static uint32_t
scat_readl(uint32_t addr, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    scat_t *dev = (scat_t *)map->dev;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint32_t val = 0xffffffff;

    addr = get_addr(dev, addr, page);
    if (addr < ((uint32_t)mem_size << 10))
	val = *(uint32_t *)&ram[addr];

    return(val);
}


static void
scat_writeb(uint32_t addr, uint8_t val, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint32_t oldaddr = addr, chkaddr;
    scat_t *dev;

    if (map == NULL)
	dev = NULL;
    else
	dev = (scat_t *)map->dev;
	
    if (dev == NULL)
	chkaddr = oldaddr;
    else {
	addr = get_addr(dev, addr, page);
	chkaddr = addr;
    }
    if (chkaddr >= 0xc0000 && chkaddr < 0x100000) {
	if (dev == NULL || (dev->regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xc0000) >> 15)))) 
		return;
    }

    if (addr < ((uint32_t)mem_size << 10))
	ram[addr] = val;
}


static void
scat_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint32_t oldaddr = addr, chkaddr;
    scat_t *dev;

    if (map == NULL)
	dev = NULL;
    else
	dev = (scat_t *)map->dev;
	
    if (dev == NULL)
	chkaddr = oldaddr;
    else {
	addr = get_addr(dev, addr, page);
	chkaddr = addr;
    }
   
    if (chkaddr >= 0xc0000 && chkaddr < 0x100000) {
	if (dev != NULL && (dev->regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xc0000) >> 15))))
		return;
    }

    if (addr < ((uint32_t)mem_size << 10))
	*(uint16_t *)&ram[addr] = val;
}


static void
scat_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    mem_map_t *map = (mem_map_t *)priv;
    ems_page_t *page = (ems_page_t *)map->p2;
    uint32_t oldaddr = addr, chkaddr;
    scat_t *dev;

    if (map == NULL)
	dev = NULL;
    else
	dev = (scat_t *)map->dev;
	
    if (dev == NULL)
	chkaddr = oldaddr;
    else {
	addr = get_addr(dev, addr, page);
	chkaddr = addr;
    }
    
    if (chkaddr >= 0xc0000 && chkaddr < 0x100000) {
	if (dev != NULL && (dev->regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xc0000) >> 15)))) 
		return;
    }
    
    if (addr < ((uint32_t)mem_size << 10))
	*(uint32_t *)&ram[addr] = val;
}


static void
scat_close(priv_t priv)
{
    scat_t *dev = (scat_t *)priv;

    free(dev);
}


static priv_t
scat_init(const device_t *info, UNUSED(void *parent))
{
    scat_t *dev;
    uint32_t k;
    int i, sx;

    dev = (scat_t *)mem_alloc(sizeof(scat_t));
    memset(dev, 0x00, sizeof(scat_t));
    dev->type = info->local;

    sx = (dev->type == 32) ? 1 : 0;

    for (i = 0; i < sizeof(dev->regs); i++)
	dev->regs[i] = 0xff;

    if (sx) {
	dev->regs[SCAT_VERSION] = 0x13;
	dev->regs[SCAT_CLOCK_CONTROL] = 6;
	dev->regs[SCAT_PERIPHERAL_CONTROL] = 0;
	dev->regs[SCAT_DRAM_CONFIGURATION] = 1;
	dev->regs[SCATSX_LAPTOP_FEATURES] = 0;
	dev->regs[SCATSX_FAST_VIDEO_CONTROL] = 0;
	dev->regs[SCATSX_FAST_VIDEORAM_ENABLE] = 0;
	dev->regs[SCATSX_HIGH_PERFORMANCE_REFRESH] = 8;
	dev->regs[SCATSX_CAS_TIMING_FOR_DMA] = 3;
    } else {
	switch(dev->type) {
		case 4:
			dev->regs[SCAT_VERSION] = 4;
			break;

		default:
			dev->regs[SCAT_VERSION] = 1;
			break;
	}
	dev->regs[SCAT_CLOCK_CONTROL] = 2;
	dev->regs[SCAT_PERIPHERAL_CONTROL] = 0x80;
	dev->regs[SCAT_DRAM_CONFIGURATION] = config.cpu_waitstates == 1 ? 2 : 0x12;
    }
    dev->regs[SCAT_DMA_WAIT_STATE_CONTROL] = 0;
    dev->regs[SCAT_MISCELLANEOUS_STATUS] = 0x37;
    dev->regs[SCAT_ROM_ENABLE] = 0xc0;
    dev->regs[SCAT_RAM_WRITE_PROTECT] = 0;
    dev->regs[SCAT_POWER_MANAGEMENT] = 0;
    dev->regs[SCAT_SHADOW_RAM_ENABLE_1] = 0;
    dev->regs[SCAT_SHADOW_RAM_ENABLE_2] = 0;
    dev->regs[SCAT_SHADOW_RAM_ENABLE_3] = 0;
    dev->regs[SCAT_EXTENDED_BOUNDARY] = 0;
    dev->regs[SCAT_EMS_CONTROL] = 0;
    dev->port_92 = 0;

    /* Disable all system mappings, we will override them. */
    mem_map_disable(&ram_low_mapping);
    if (! sx)
	mem_map_disable(&ram_mid_mapping);
    mem_map_disable(&ram_high_mapping);
    for (i = 0; i < 4; i++)
	mem_map_disable(&bios_mapping[i]);

    k = (sx) ? 0x80000 : 0x40000;
    mem_map_add(&dev->low_mapping[0], 0, k,
		scat_readb, scat_readw, scat_readl,
		scat_writeb, scat_writew, scat_writel,
		ram, MEM_MAPPING_INTERNAL, &dev->low_mapping[0]);
    mem_map_set_dev(&dev->low_mapping[0], dev);

    mem_map_add(&dev->low_mapping[1], 0xf0000, 0x10000,
		scat_readb, scat_readw, scat_readl,
		scat_writeb, scat_writew, scat_writel,
		ram + 0xf0000, MEM_MAPPING_INTERNAL, &dev->low_mapping[1]);
    mem_map_set_dev(&dev->low_mapping[1], dev);

    for (i = 2; i < 32; i++) {
	mem_map_add(&dev->low_mapping[i], (i << 19), 0x80000,
		    scat_readb, scat_readw, scat_readl,
		    scat_writeb, scat_writew, scat_writel,
		    ram + (i<<19), MEM_MAPPING_INTERNAL, &dev->low_mapping[i]);
	mem_map_set_dev(&dev->low_mapping[i], dev);
    }

    if (sx) {
	i = 16;
	k = 0x40000;
    } else {
	i = 0;
	k = (dev->regs[SCAT_VERSION] < 4) ? 0x40000 : 0x60000;
    }
    mem_map_set_addr(&dev->low_mapping[31], 0xf80000, k);

    for (; i < 44; i++) {
	mem_map_add(&dev->efff_mapping[i], 0x40000 + (i << 14), 0x4000,
		    scat_readb, scat_readw, scat_readl,
		    scat_writeb, scat_writew, scat_writel,
		    mem_size > (256 + (i << 4)) ? ram + 0x40000 + (i << 14) : NULL,
		    MEM_MAPPING_INTERNAL, &dev->efff_mapping[i]);
	mem_map_set_dev(&dev->efff_mapping[i], dev);

	if (sx)
		mem_map_enable(&dev->efff_mapping[i]);
    }

    for (i = 0; i < 8; i++) {
	mem_map_add(&dev->romcs_mapping[i], 0xc0000 + (i << 14), 0x4000,
		    rom_bios_read,rom_bios_readw,rom_bios_readl,
		    mem_write_null,mem_write_nullw,mem_write_nulll,
		    bios + ((i << 14) & biosmask),
		    MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, NULL);
	mem_map_disable(&dev->romcs_mapping[i]);
    }

    if (sx) {
	for (i = 24; i < 32; i++) {
		dev->page[i].regs_2x8 = 0xff;
		dev->page[i].regs_2x9 = 0x03;
		mem_map_add(&dev->ems_mapping[i], (i + 28) << 14, 0x04000,
			    scat_readb, scat_readw, scat_readl,
			    scat_writeb, scat_writew, scat_writel,
			    ram + ((i + 28) << 14), 0, &dev->ems_mapping[i]);
		mem_map_set_dev(&dev->ems_mapping[i], dev);
		mem_map_set_p2(&dev->ems_mapping[i], &dev->page[i]);
		mem_map_disable(&dev->ems_mapping[i]);
	}

	for (i = 0; i < 16; i++) {
		mem_map_add(&dev->high_mapping[i], (i << 14) + 0xfc0000, 0x04000,
			    rom_bios_read,rom_bios_readw,rom_bios_readl,
			    mem_write_null,mem_write_nullw,mem_write_nulll,
			    bios + ((i << 14) & biosmask), 0, NULL);
		mem_map_enable(&dev->high_mapping[i]);
	}
    } else {
	for (i = 0; i < 32; i++) {
		dev->page[i].regs_2x8 = 0xff;
		dev->page[i].regs_2x9 = 0x03;
		mem_map_add(&dev->ems_mapping[i], (i + (i >= 24 ? 28 : 16)) << 14, 0x04000,
			    scat_readb, scat_readw, scat_readl,
			    scat_writeb, scat_writew, scat_writel,
			    ram + ((i + (i >= 24 ? 28 : 16)) << 14),
			    0, &dev->ems_mapping[i]);
		mem_map_set_dev(&dev->ems_mapping[i], dev);
		mem_map_set_p2(&dev->ems_mapping[i], &dev->page[i]);
	}

	for (i = (dev->regs[SCAT_VERSION] < 4 ? 0 : 8); i < 16; i++) {
		mem_map_add(&dev->high_mapping[i], (i << 14) + 0xfc0000, 0x04000,
			    rom_bios_read,rom_bios_readw,rom_bios_readl,
			    mem_write_null,mem_write_nullw,mem_write_nulll,
			    bios + ((i << 14) & biosmask), 0, NULL);
		mem_map_enable(&dev->high_mapping[i]);
	}
    }

    for (i = 0; i < 6; i++) {
	mem_map_add(&dev->remap_mapping[i], 0x100000 + (i << 16), 0x10000,
		    scat_readb, scat_readw, scat_readl,
		    scat_writeb, scat_writew, scat_writel,
		    mem_size >= 1024 ? ram + get_addr(dev, 0x100000 + (i << 16), NULL) : NULL,
		    MEM_MAPPING_INTERNAL, &dev->remap_mapping[i]);
	mem_map_set_dev(&dev->remap_mapping[i], dev);
    }

    if (sx) {
	dev->external_is_RAS = scatsx_external_is_RAS[mem_size >> 9];
    } else {
	dev->external_is_RAS = (dev->regs[SCAT_VERSION] > 3) || (((mem_size & ~2047) >> 11) + ((mem_size & 1536) >> 9) + ((mem_size & 511) >> 7)) > 4;
    }

    set_xms_bound(dev, 0);
    memmap_state_update(dev);
    shadow_state_update(dev);

    io_sethandler(0x0022, 2,
		  scat_in,NULL,NULL, scat_out,NULL,NULL, dev);
    io_sethandler(0x0092, 1,
		  scat_in,NULL,NULL, scat_out,NULL,NULL, dev);

    return(dev);
}


const device_t scat_device = {
    "C&T SCAT (v1)",
    0,
    0,
    NULL,
    scat_init, scat_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t scat_4_device = {
    "C&T SCAT (v4)",
    0,
    4,
    NULL,
    scat_init, scat_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t scat_sx_device = {
    "C&T SCATsx",
    0,
    32,
    NULL,
    scat_init, scat_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
