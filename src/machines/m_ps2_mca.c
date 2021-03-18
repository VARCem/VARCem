/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of MCA-based PS/2 machines.
 *
 *		The model 70 type 3/4 BIOS performs cache testing. Since we
 *		do not have proper cache emulation, it is faked a bit here.
 *
 *		Port E2 is used for cache diagnostics. Bit 7 seems to be set
 *		on a cache miss, toggling bit 2 seems to clear this. The BIOS
 *		performs at least the following tests:
 *
 *		- Disable RAM, access low 64kb (386) / 8kb (486), execute code
 *		  from cache to access low memory and verify that there are no
 *		  cache misses.
 *		- Write to low memory using DMA, read low memory and verify
 *		  that all accesses cause cache misses.
 *		- Read low memory, verify that first access is cache miss.
 *		  Read again and verify that second access is cache hit.
 *
 *		These tests are also performed on the 486 model 70, despite
 *		there being no external cache on that system. Port E2 seems to
 *		control the internal cache on these systems. Presumably this
 *		port is connected to KEN#/FLUSH# on the 486.
 *
 *		This behavior is required to pass the timer interrupt test on
 *		the 486 version - the BIOS uses a fixed length loop that will
 *		terminate too early on a 486/25 if it executes from internal
 *		cache.
 *
 *		To handle this, some basic heuristics are used:
 *
 *		- If cache is enabled but RAM is disabled, accesses to low
 *		  memory go directly to cache memory.
 *		- Reads to cache addresses not 'valid' will set the cache miss
 *		  flag, and mark that line as valid.
 *		- Cache flushes will clear the valid array.
 *		- DMA via the undocumented PS/2 command 0xb will clear the
 *		  valid array.
 *		- Disabling the cache will clear the valid array.
 *		- Disabling the cache will also mark shadowed ROM areas as
 *		  using ROM timings.
 *
 *		This works around the timing loop mentioned above.
 *
 * Version:	@(#)m_ps2_mca.c	1.0.31	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include "../cpu/x86.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/dma.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/port92.h"
#include "../devices/system/mca.h"
#include "../devices/system/nvr_ps2.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h"
#include "../plat.h"
#include "machine.h"


#define PS2_SETUP_IO	0x80
#define PS2_SETUP_VGA	0x20

#define PS2_CARD_SETUP	0x08


typedef struct _ps2_ {
    int		type;

    uint8_t	adapter_setup;
    uint8_t	option[4];
    uint8_t	pos_vga;
    uint8_t	setup;
    uint8_t	sys_ctrl_port_a;
    uint8_t	subaddr_lo,
		subaddr_hi;

    uint8_t	memory_bank[8];

    uint8_t	io_id;

    mem_map_t	shadow_mapping;
    mem_map_t	split_mapping;
    mem_map_t	exp_mapping;
    mem_map_t	cache_mapping;

    uint8_t	(*planar_read)(struct _ps2_ *, uint16_t port);
    void	(*planar_write)(struct _ps2_ *, uint16_t port, uint8_t val);

    uint8_t	mem_regs[3];

    uint32_t	split_addr, split_size;
    uint32_t	split_phys;

    uint8_t	mem_pos_regs[8];
    uint8_t	mem_2mb_pos_regs[8];

    int		pending_cache_miss;

    uint8_t	cache[65536];
    int		cache_valid[65536/8];
} ps2_t;


static ps2_t	*saved_dev = NULL;		/* needed for clear_cache() */


static uint8_t
cache_read(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    DBGLOG(1, "ps2_read_cache_ram: addr=%08x %i %04x:%04x\n",
	   addr, dev->cache_valid[addr >> 3], CS,cpu_state.pc);

    if (! dev->cache_valid[addr >> 3]) {
	dev->cache_valid[addr >> 3] = 1;
	dev->mem_regs[2] |= 0x80;
    } else
	dev->pending_cache_miss = 0;

    return dev->cache[addr];
}


static uint16_t
cache_readw(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    DBGLOG(1, "ps2_read_cache_ramw: addr=%08x %i %04x:%04x\n",
	   addr, dev->cache_valid[addr >> 3], CS,cpu_state.pc);

    if (! dev->cache_valid[addr >> 3]) {
	dev->cache_valid[addr >> 3] = 1;
	dev->mem_regs[2] |= 0x80;
    } else
	dev->pending_cache_miss = 0;

    return *((uint16_t *)(dev->cache+addr));
}


static uint32_t
cache_readl(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    DBGLOG(1, "ps2_read_cache_raml: addr=%08x %i %04x:%04x\n",
	   addr, dev->cache_valid[addr >> 3], CS,cpu_state.pc);

    if (! dev->cache_valid[addr >> 3]) {
	dev->cache_valid[addr >> 3] = 1;
	dev->mem_regs[2] |= 0x80;
    } else
	dev->pending_cache_miss = 0;

    return *((uint32_t *)dev->cache+addr);
}


static void
cache_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    DBGLOG(1, "ps2_write_cache_ram: addr=%08x val=%02x %04x:%04x %i\n",
	   addr, val, CS,cpu_state.pc, ins);

    dev->cache[addr] = val;
}


void
ps2_cache_clean(void)
{
    ps2_t *dev = saved_dev;

    if (dev)
	memset(dev->cache_valid, 0x00, sizeof(dev->cache_valid));
}


static uint8_t
shadow_read(uint32_t addr, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    return mem_read_ram(addr, priv);
}


static uint16_t
shadow_readw(uint32_t addr, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    return mem_read_ramw(addr, priv);
}


static uint32_t
shadow_readl(uint32_t addr, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    return mem_read_raml(addr, priv);
}


static void
shadow_write(uint32_t addr, uint8_t val, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    mem_write_ram(addr, val, priv);
}


static void
shadow_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    mem_write_ramw(addr, val, priv);
}


static void
shadow_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    addr = (addr & 0x1ffff) + 0xe0000;

    mem_write_raml(addr, val, priv);
}


static uint8_t
split_read(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    return mem_read_ram(addr, priv);
}


static uint16_t
split_readw(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    return mem_read_ramw(addr, priv);
}


static uint32_t
split_readl(uint32_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    return mem_read_raml(addr, priv);
}


static void
split_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    mem_write_ram(addr, val, priv);
}


static void
split_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    mem_write_ramw(addr, val, priv);
}


static void
split_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    addr = (addr % (dev->split_size << 10)) + dev->split_phys;

    mem_write_raml(addr, val, priv);
}


static uint8_t
exp_read(int port, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    return dev->mem_pos_regs[port & 7];
}


static void
exp_write(int port, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    if (port < 0x102 || port == 0x104)
	return;

    dev->mem_pos_regs[port & 7] = val;

    if (dev->mem_pos_regs[2] & 1)
	mem_map_enable(&dev->exp_mapping);
    else
	mem_map_disable(&dev->exp_mapping);
}


static uint8_t 
exp_feedb(priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    return(dev->mem_pos_regs[2] & 1);
}


static void
mem_fffc_init(ps2_t *dev, int start_mb)
{
    uint32_t planar_size, exp_start;

    if (start_mb == 2) {
	planar_size = 0x160000;
	exp_start = 0x260000;
    } else {
	planar_size = (start_mb - 1) << 20;
	exp_start = start_mb << 20;
    }

    mem_map_set_addr(&ram_high_mapping, 0x100000, planar_size);

    dev->mem_pos_regs[0] = 0xff;
    dev->mem_pos_regs[1] = 0xfc;

    switch ((mem_size / 1024) - start_mb) {
	case 1:
		dev->mem_pos_regs[4] = 0xfc;	/* 11 11 11 00 = 0 0 0 1 */
		break;

	case 2:
		dev->mem_pos_regs[4] = 0xfe;	/* 11 11 11 10 = 0 0 0 2 */
		break;

	case 3:
		dev->mem_pos_regs[4] = 0xf2;	/* 11 11 00 10 = 0 0 1 2 */
		break;

	case 4:
		dev->mem_pos_regs[4] = 0xfa;	/* 11 11 10 10 = 0 0 2 2 */
		break;

	case 5:
		dev->mem_pos_regs[4] = 0xca;	/* 11 00 10 10 = 0 1 2 2 */
		break;

	case 6:
		dev->mem_pos_regs[4] = 0xea;	/* 11 10 10 10 = 0 2 2 2 */
		break;

	case 7:
		dev->mem_pos_regs[4] = 0x2a;	/* 00 10 10 10 = 1 2 2 2 */
		break;

	case 8:
		dev->mem_pos_regs[4] = 0xaa;	/* 10 10 10 10 = 2 2 2 2 */
		break;
    }

    mca_add(exp_read, exp_write, exp_feedb, NULL, dev);

    mem_map_add(&dev->exp_mapping, exp_start, (mem_size - (start_mb << 10)) << 10,
		mem_read_ram, mem_read_ramw, mem_read_raml,
		mem_write_ram, mem_write_ramw, mem_write_raml,
		&ram[exp_start], MEM_MAPPING_INTERNAL, dev);
    mem_map_disable(&dev->exp_mapping);
}


static uint8_t
model_50_in(ps2_t *dev, uint16_t port)
{
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		ret = 0xfb;
		break;

	case 0x0102:
		ret = dev->option[0];
		break;

	case 0x0103:
		ret = dev->option[1];
		break;

	case 0x0104:
		ret = dev->option[2];
		break;

	case 0x0105:
		ret = dev->option[3];
		break;

	case 0x0106:
		ret = dev->subaddr_lo;
		break;

	case 0x0107:
		ret = dev->subaddr_hi;
		break;
    }

    return ret;
}


static void
model_50_out(ps2_t *dev, uint16_t port, uint8_t val)
{
    switch (port) {
	case 0x0100:
		dev->io_id = val;
		break;

	case 0x0101:
		break;

	case 0x0102:
#if 0
		serial_remove(0);
#endif
		if (val & 0x04) {
			if (val & 0x08)
				serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
			else
				serial_setup(0, SERIAL2_ADDR, SERIAL2_IRQ);
		}

		if (val & 0x10) switch ((val >> 5) & 3) {
			case 0:
				parallel_setup(0, 0x03bc);
				break;

			case 1:
				parallel_setup(0, 0x0378);
				break;

			case 2:
				parallel_setup(0, 0x0278);
				break;
		}
		dev->option[0] = val;
		break;

	case 0x0103:
		dev->option[1] = val;
		break;

	case 0x0104:
		dev->option[2] = val;
		break;

	case 0x0105:
		dev->option[3] = val;
		break;

	case 0x0106:
		dev->subaddr_lo = val;
		break;

	case 0x0107:
		dev->subaddr_hi = val;
		break;
    }
}


static void
model_50_init(ps2_t *dev)
{
    mem_remap_top(384);

    mca_init(4);

    dev->planar_read = model_50_in;
    dev->planar_write = model_50_out;

    if (mem_size > 2048) {
	/*
	 * The M50 planar only supports up to 2 MB.
	 * If we have more, create a memory expansion card for the rest.
	 */
	mem_fffc_init(dev, 2);
    }

    if (config.video_card == VID_INTERNAL)	
	device_add(&vga_ps1_device);
}


static uint8_t
model_55sx_in(ps2_t *dev, uint16_t port)
{
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		ret = 0xfb;
		break;

	case 0x0102:
		ret = dev->option[0];
		break;

	case 0x0103:
		ret = dev->option[1];
		break;

	case 0x0104:
		ret = dev->memory_bank[dev->option[3] & 7];
		break;

	case 0x0105:
		ret = dev->option[3];
		break;

	case 0x0106:
		ret = dev->subaddr_lo;
		break;

	case 0x0107:
		ret = dev->subaddr_hi;
		break;
    }

    return ret;
}


static void
model_55sx_out(ps2_t *dev, uint16_t port, uint8_t val)
{
    switch (port) {
	case 0x0100:
		dev->io_id = val;
		break;

	case 0x0101:
		break;

	case 0x0102:
#if 0
		serial_remove(0);
#endif
		if (val & 0x04) {
			if (val & 0x08)
				serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
			else
				serial_setup(0, SERIAL2_ADDR, SERIAL2_IRQ);
		}

		if (val & 0x10) switch ((val >> 5) & 3) {
			case 0:
				parallel_setup(0, 0x03bc);
				break;

			case 1:
				parallel_setup(0, 0x0378);
				break;

			case 2:
				parallel_setup(0, 0x0278);
				break;
		}
		dev->option[0] = val;
		break;

	case 0x0103:
		dev->option[1] = val;
		break;

	case 0x0104:
		dev->memory_bank[dev->option[3] & 7] &= ~0xf;
		dev->memory_bank[dev->option[3] & 7] |= (val & 0xf);

		DBGLOG(1, "Write memory bank %i %02x\n", dev->option[3] & 7, val);
		break;

	case 0x0105:
		DBGLOG(1, "Write POS3 %02x\n", val);
		dev->option[3] = val;
		shadowbios = !(val & 0x10);
		shadowbios_write = val & 0x10;

		if (shadowbios) {
			mem_set_mem_state(0xe0000, 0x20000,
				MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
			mem_map_disable(&dev->shadow_mapping);
		} else {
			mem_set_mem_state(0xe0000, 0x20000,
				MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
			mem_map_enable(&dev->shadow_mapping);
		}

		if ((dev->option[1] & 1) && !(dev->option[3] & 0x20))
			mem_set_mem_state(mem_size * 1024, 256 * 1024,
				MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		else
			mem_set_mem_state(mem_size * 1024, 256 * 1024,
				MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
		break;

	case 0x0106:
		dev->subaddr_lo = val;
		break;

	case 0x0107:
		dev->subaddr_hi = val;
		break;
    }
}


static void
model_55sx_init(ps2_t *dev)
{
    mem_map_add(&dev->shadow_mapping, (mem_size+256) * 1024, 128*1024,
		shadow_read,shadow_readw,shadow_readl,
		shadow_write, shadow_writew,shadow_writel,
		&ram[0xe0000], MEM_MAPPING_INTERNAL, dev);

    mem_remap_top(256);
    dev->option[3] = 0x10;

    memset(dev->memory_bank, 0xf0, 8);
    switch (mem_size >> 10) {
	case 1:
		dev->memory_bank[0] = 0x61;
		break;

	case 2:
		dev->memory_bank[0] = 0x51;
		break;

	case 3:
		dev->memory_bank[0] = 0x51;
		dev->memory_bank[1] = 0x61;
		break;

	case 4:
		dev->memory_bank[0] = 0x51;
		dev->memory_bank[1] = 0x51;
		break;

	case 5:
		dev->memory_bank[0] = 0x01;
		dev->memory_bank[1] = 0x61;
		break;

	case 6:
		dev->memory_bank[0] = 0x01;
		dev->memory_bank[1] = 0x51;
		break;

	case 7: /*Not supported*/
		dev->memory_bank[0] = 0x01;
		dev->memory_bank[1] = 0x51;
		break;

	case 8:
		dev->memory_bank[0] = 0x01;
		dev->memory_bank[1] = 0x01;
		break;
    }

    mca_init(4);

    dev->planar_read = model_55sx_in;
    dev->planar_write = model_55sx_out;

    if (config.video_card == VID_INTERNAL)	
	device_add(&vga_ps1_device);
}


static void
mem_encoding_update(ps2_t *dev)
{
    mem_map_disable(&dev->split_mapping);

    dev->split_addr = ((uint32_t) (dev->mem_regs[0] & 0xf)) << 20;

    if (dev->mem_regs[1] & 2) {
	mem_set_mem_state(0xe0000, 0x20000,
			  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
	DEBUG("PS/2 Model 80-111: ROM space enabled\n");
    } else {
	mem_set_mem_state(0xe0000, 0x20000,
			  MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
	DEBUG("PS/2 Model 80-111: ROM space disabled\n");
    }

    if (dev->mem_regs[1] & 4) {
	mem_map_set_addr(&ram_low_mapping, 0x00000, 0x80000);
	DEBUG("PS/2 Model 80-111: 00080000- 0009FFFF disabled\n");
    } else {
	mem_map_set_addr(&ram_low_mapping, 0x00000, 0xa0000);
	DEBUG("PS/2 Model 80-111: 00080000- 0009FFFF enabled\n");
    }

    if (! (dev->mem_regs[1] & 8)) {
	if (dev->mem_regs[1] & 4) {
		dev->split_size = 384;
		dev->split_phys = 0x80000;
	} else {
		dev->split_size = 256;
		dev->split_phys = 0xa0000;
	}

	mem_map_set_exec(&dev->split_mapping, &ram[dev->split_phys]);
	mem_map_set_addr(&dev->split_mapping, dev->split_addr, dev->split_size << 10);

	DEBUG("PS/2 Model 80-111: Split memory block enabled at %08X\n", dev->split_addr);
    } else {
	DEBUG("PS/2 Model 80-111: Split memory block disabled\n");
    }
}


static uint8_t
mem_encoding_read(uint16_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0xe0:
		ret = dev->mem_regs[0];
		break;

	case 0xe1:
		ret = dev->mem_regs[1];
		break;
    }

    return ret;
}


static void
mem_encoding_write(uint16_t addr, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    switch (addr) {
	case 0xe0:
		dev->mem_regs[0] = val;
		break;

	case 0xe1:
		dev->mem_regs[1] = val;
		break;
    }

    mem_encoding_update(dev);
}


static uint8_t
mem_encoding_read_cached(uint16_t addr, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;
    uint8_t ret = 0xff;

    switch (addr) {
	case 0xe0:
		ret = dev->mem_regs[0];
		break;

	case 0xe1:
		ret = dev->mem_regs[1];
		break;

	case 0xe2:
		ret = dev->mem_regs[2];
		break;
    }

    return ret;
}


static void
mem_encoding_write_cached(uint16_t addr, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;
    uint8_t old;

    switch (addr) {
	case 0xe0:
		dev->mem_regs[0] = val;
		break;

	case 0xe1:
		dev->mem_regs[1] = val;
		break;

	case 0xe2:
		old = dev->mem_regs[2];
		dev->mem_regs[2] = (dev->mem_regs[2] & 0x80) | (val & ~0x88);
		if (val & 2) {
			DBGLOG(1, "Clear latch - %i\n", dev->pending_cache_miss);
			if (dev->pending_cache_miss)
				dev->mem_regs[2] |=  0x80;
			else
				dev->mem_regs[2] &= ~0x80;
			dev->pending_cache_miss = 0;
		}

		if ((val & 0x21) == 0x20 && (old & 0x21) != 0x20)
			dev->pending_cache_miss = 1;
		if ((val & 0x21) == 0x01 && (old & 0x21) != 0x01)
			ps2_cache_clean();
#if 1
// FIXME: Look into this!!!
		if (val & 0x01)
			ram_mid_mapping.flags |= MEM_MAPPING_ROM;
		else
			ram_mid_mapping.flags &= ~MEM_MAPPING_ROM;
#endif
		break;
    }

    DBGLOG(1, "mem_encoding_write: addr=%02x val=%02x %04x:%04x  %02x %02x\n",
		addr, val, CS,cpu_state.pc, dev->mem_regs[1],dev->mem_regs[2]);
    mem_encoding_update(dev);

    if ((dev->mem_regs[1] & 0x10) && (dev->mem_regs[2] & 0x21) == 0x20) {
	mem_map_disable(&ram_low_mapping);
	mem_map_enable(&dev->cache_mapping);
    } else {
	mem_map_disable(&dev->cache_mapping);
	mem_map_enable(&ram_low_mapping);
    }

    flushmmucache();
}


static uint8_t
model_70_in(ps2_t *dev, uint16_t port)
{
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		ret = 0xf9;
		break;

	case 0x0102:
		ret = dev->option[0];
		break;

	case 0x0103:
		ret = dev->option[1];
		break;

	case 0x0104:
		ret = dev->option[2];
		break;

	case 0x0105:
		ret = dev->option[3];
		break;

	case 0x0106:
		ret = dev->subaddr_lo;
		break;

	case 0x0107:
		ret = dev->subaddr_hi;
		break;

    }

    return ret;
}


static void
model_70_out(ps2_t *dev, uint16_t port, uint8_t val)
{
    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		break;

	case 0x0102:
#if 0
		serial_remove(0);
#endif
		if (val & 0x04) {
			if (val & 0x08)
				serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
			else
				serial_setup(0, SERIAL2_ADDR, SERIAL2_IRQ);
		}

		if (val & 0x10) switch ((val >> 5) & 3) {
			case 0:
				parallel_setup(0, 0x03bc);
				break;

			case 1:
				parallel_setup(0, 0x0378);
				break;

			case 2:
				parallel_setup(0, 0x0278);
				break;
		}
		dev->option[0] = val;
		break;

	case 0x0105:
		dev->option[3] = val;
		break;

	case 0x0106:
		dev->subaddr_lo = val;
		break;

	case 0x0107:
		dev->subaddr_hi = val;
		break;
    }
}


static void
model_70_init(ps2_t *dev)
{
    int is_type4 = (dev->type == 74);

    dev->split_addr = mem_size * 1024;
    mca_init(4);

    dev->planar_read = model_70_in;
    dev->planar_write = model_70_out;

    device_add(&ps2_nvr_device);

    io_sethandler(0x00e0, 3,
		  mem_encoding_read_cached,NULL,NULL,
		  mem_encoding_write_cached,NULL,NULL, dev);

    dev->mem_regs[1] = 2;

    switch (mem_size >> 10) {
	case 2:
		dev->option[1] = 0xa6;
		dev->option[2] = 0x01;
		break;

	case 4:
		dev->option[1] = 0xaa;
		dev->option[2] = 0x01;
		break;

	case 6:
		dev->option[1] = 0xca;
		dev->option[2] = 0x01;
		break;

	case 8:
	default:
		dev->option[1] = 0xca;
		dev->option[2] = 0x02;
		break;
    }

    if (is_type4)
	dev->option[2] |= 0x04; /*486 CPU*/

    mem_map_add(&dev->split_mapping, (mem_size+256) * 1024, 256*1024,
		split_read,split_readw,split_readl,
		split_write,split_writew,split_writel,
		&ram[0xa0000], MEM_MAPPING_INTERNAL, dev);
    mem_map_disable(&dev->split_mapping);

    mem_map_add(&dev->cache_mapping, 0, is_type4 ? (8 * 1024) : (64 * 1024),
		cache_read,cache_readw,cache_readl,
		cache_write,NULL,NULL,
	        dev->cache, MEM_MAPPING_INTERNAL, dev);
    mem_map_disable(&dev->cache_mapping);

    if (mem_size > 8192) {
	/* Only 8 MB supported on planar, create a memory expansion card for the rest */
	mem_fffc_init(dev, 8);
    }

    if (config.video_card == VID_INTERNAL)	
	device_add(&vga_ps1_device);
}


static uint8_t
model_80_in(ps2_t *dev, uint16_t port)
{
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		ret = 0xfd;
		break;

	case 0x0102:
		ret = dev->option[0];
		break;

	case 0x0103:
		ret = dev->option[1];
		break;

	case 0x0104:
		ret = dev->option[2];
		break;

	case 0x0105:
		ret = dev->option[3];
		break;

	case 0x0106:
		ret = dev->subaddr_lo;
		break;

	case 0x0107:
		ret = dev->subaddr_hi;
		break;

    }

    return ret;
}


static void
model_80_out(ps2_t *dev, uint16_t port, uint8_t val)
{
    switch (port) {
	case 0x0100:
		break;

	case 0x0101:
		break;

	case 0x0102:
#if 0
		serial_remove(0);
#endif
		if (val & 0x04) {
			if (val & 0x08)
				serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
			else
				serial_setup(0, SERIAL2_ADDR, SERIAL2_IRQ);
		}

		if (val & 0x10) switch ((val >> 5) & 3) {
			case 0:
				parallel_setup(0, 0x03bc);
				break;

			case 1:
				parallel_setup(0, 0x0378);
				break;

			case 2:
				parallel_setup(0, 0x0278);
				break;
		}
		dev->option[0] = val;
		break;

	case 0x0103:
		dev->option[1] = (dev->option[1] & 0x0f) | (val & 0xf0);
		break;

	case 0x0104:
		dev->option[2] = val;
		break;

	case 0x0105:
		dev->option[3] = val;
		break;

	case 0x0106:
		dev->subaddr_lo = val;
		break;

	case 0x0107:
		dev->subaddr_hi = val;
		break;
    }
}


static void
model_80_init(ps2_t *dev)
{
    int is_486 = (dev->type > 80);

    dev->split_addr = mem_size * 1024;
    mca_init(8);

    dev->planar_read = model_80_in;
    dev->planar_write = model_80_out;

    device_add(&ps2_nvr_device);

    io_sethandler(0x00e0, 2,
		  mem_encoding_read,NULL,NULL,
		  mem_encoding_write,NULL,NULL, dev);

    dev->mem_regs[1] = 2;

    switch (mem_size >> 10) {
	case 1:
		dev->option[1] = 0x0e;		/* 11 10 = 0 2 */
		dev->mem_regs[1] = 0xd2;	/* 01 = 1 (first) */
		dev->mem_regs[0] = 0xf0;	/* 11 = invalid */
		break;

	case 2:
		dev->option[1] = 0x0e;		/* 11 10 = 0 2 */
		dev->mem_regs[1] = 0xc2;	/* 00 = 2 */
		dev->mem_regs[0] = 0xf0;	/* 11 = invalid */
		break;

	case 3:
		dev->option[1] = 0x0a;		/* 10 10 = 2 2 */
		dev->mem_regs[1] = 0xc2;	/* 00 = 2 */
		dev->mem_regs[0] = 0xd0;	/* 01 = 1 (first) */
		break;

	case 4:
	default:
		dev->option[1] = 0x0a;		/* 10 10 = 2 2 */
		dev->mem_regs[1] = 0xc2;	/* 00 = 2 */
		dev->mem_regs[0] = 0xc0;	/* 00 = 2 */
		break;
    }

    dev->mem_regs[0] |= ((mem_size / 1024) & 0x0f);

    mem_map_add(&dev->split_mapping, (mem_size+256) * 1024, 256*1024,
		split_read,split_readw,split_readl,
		split_write,split_writew,split_writel,
		&ram[0xa0000], MEM_MAPPING_INTERNAL, dev);
    mem_map_disable(&dev->split_mapping);

    if ((mem_size > 4096) && !is_486) {
	/* Only 4 MB supported on planar, create a memory expansion card for the rest */
	mem_fffc_init(dev, 4);
    }

    if (config.video_card == VID_INTERNAL)	
	device_add(&vga_ps1_device);
}


static uint8_t
ps2_mca_read(uint16_t port, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0091:
		/*NOTREACHED*/
		if (! (dev->setup & PS2_SETUP_IO))
			ret = 0x00;
		else if (! (dev->setup & PS2_SETUP_VGA))
			ret = 0x00;
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			ret = 0x00;
		else
			ret = !mca_feedb();
		ret |= 0xfe;
		break;

	case 0x0094:
		ret = dev->setup;
		break;

	case 0x0096:
		ret = dev->adapter_setup | 0x70;
		break;

	case 0x0100:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if (! (dev->setup & PS2_SETUP_VGA))
			ret = 0xfd;
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			ret = mca_read(port);
		break;

	case 0x0101:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if (! (dev->setup & PS2_SETUP_VGA))
			ret = 0xef;
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			ret = mca_read(port);
		break;

	case 0x0102:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if (! (dev->setup & PS2_SETUP_VGA))
			ret = dev->pos_vga;
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			ret = mca_read(port);
		break;

	case 0x0103:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			ret = mca_read(port);
		break;

	case 0x0104:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			ret = mca_read(port);
		break;

	case 0x0105:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			ret = mca_read(port);
		break;

	case 0x0106:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			ret = mca_read(port);
		break;

	case 0x0107:
		if (! (dev->setup & PS2_SETUP_IO))
			ret = dev->planar_read(dev, port);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			ret = mca_read(port);
		break;

	default:
		break;
    }

    DBGLOG(2, "ps2_read: port=%04x temp=%02x\n", port, ret);

    return ret;
}


static void
ps2_mca_write(uint16_t port, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    DBGLOG(2, "ps2_write: port=%04x val=%02x %04x:%04x\n",
	   port, val, CS,cpu_state.pc);

    switch (port) {
	case 0x0094:
		dev->setup = val;
		break;

	case 0x0096:
		if ((val & 0x80) && !(dev->adapter_setup & 0x80))
        	mca_reset();
		dev->adapter_setup = val;
		mca_set_index(val & 7);
		break;

	case 0x0100:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			mca_write(port, val);
		break;

	case 0x0101:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if ((dev->setup & PS2_SETUP_VGA) && (dev->adapter_setup & PS2_CARD_SETUP))
			mca_write(port, val);
		break;

	case 0x0102:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if ( !(dev->setup & PS2_SETUP_VGA))
			dev->pos_vga = val;
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;

	case 0x0103:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;

	case 0x0104:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;

	case 0x0105:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;

	case 0x0106:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;

	case 0x0107:
		if (! (dev->setup & PS2_SETUP_IO))
			dev->planar_write(dev, port, val);
		else if (dev->adapter_setup & PS2_CARD_SETUP)
			mca_write(port, val);
		break;
    }
}


static void
ps2_close(priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    saved_dev = NULL;

    free(dev);
}


static priv_t
ps2_init(const device_t *info, UNUSED(void *arg))
{
    ps2_t *dev;

    dev = (ps2_t *)mem_alloc(sizeof(ps2_t));
    memset(dev, 0x00, sizeof(ps2_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    machine_common_init();

    dma16_init();
    ps2_dma_init();
    pic2_init();
    pit_ps2_init();

    nmi_mask = 0x80;

    device_add(&ps_nvr_device);

    device_add(&fdc_at_device);

    parallel_setup(0, 0x03bc);

    device_add_parent(&port92_device, (priv_t)dev);

    io_sethandler(0x0091, 1,
		  ps2_mca_read,NULL,NULL, ps2_mca_write,NULL,NULL, dev);
    io_sethandler(0x0094, 1,
		  ps2_mca_read,NULL,NULL, ps2_mca_write,NULL,NULL, dev);
    io_sethandler(0x0096, 1,
		  ps2_mca_read,NULL,NULL, ps2_mca_write,NULL,NULL, dev);
    io_sethandler(0x0100, 8,
		  ps2_mca_read,NULL,NULL, ps2_mca_write,NULL,NULL, dev);

    dev->setup = 0xff;

    switch (dev->type) {
	case 50:	/* Model 50 */
    		device_add(&keyboard_ps2_mca_device);
		model_50_init(dev);
		break;

	case 55:	/* Model 55SX */
    		device_add(&keyboard_ps2_mca_device);
		model_55sx_init(dev);
		break;

	case 73:	/* Model 70, Type 3 */
	case 74:	/* Model 70, Type 4 */
    		device_add(&keyboard_ps2_mca_device);
		model_70_init(dev);
		break;

	case 80:	/* Model 80 */
    		device_add(&keyboard_ps2_mca_device);
		model_80_init(dev);
		break;
    }

    saved_dev = dev;

    return(dev);
}


static const CPU cpus_ps2_m50[] = {
    { "286/10", CPU_286, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1 },
    { "286/12", CPU_286, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2 },
    { NULL }
};

static const machine_t m50_info = {
    MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_FDC_PS2 | MACHINE_HDC_PS2,
    0,
    1, 10, 1, 64, -1,
    {{"",cpus_ps2_m50}}
};

const device_t m_ps2_m50 = {
    "IBM PS/2 M50 Type 1",
    DEVICE_ROOT,
    50,
    L"ibm/ps2/m50",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m50_info,
    NULL
};


static const machine_t m55_info = {
    MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_FDC_PS2 | MACHINE_HDC_PS2,
    0,
    1, 8, 1, 64, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_ps2_m55sx = {
    "IBM PS/2 M55SX",
    DEVICE_ROOT,
    55,
    L"ibm/ps2/m55sx",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m55_info,
    NULL
};


static const machine_t m70_3_info = {
    MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_FDC_PS2 | MACHINE_HDC_PS2,
    0,
    2, 16, 2, 64, -1,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_ps2_m70_3 = {
    "IBM PS/2 M70 Type 3",
    DEVICE_ROOT,
    73,
    L"ibm/ps2/m70_type3",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m70_3_info,
    NULL
};


static const machine_t m70_4_info = {
    MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_FDC_PS2 | MACHINE_HDC_PS2,
    MACHINE_VIDEO,
    2, 16, 2, 64, -1,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_ps2_m70_4 = {
    "IBM PS/2 M70 Type 4",
    DEVICE_ROOT,
    74,
    L"ibm/ps2/m70_type4",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m70_4_info,
    NULL
};


static const machine_t m80_info = {
    MACHINE_MCA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC_PS2,
    0,
    1, 12, 1, 64, -1,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_ps2_m80 = {
    "IBM PS/2 M80",
    DEVICE_ROOT,
    80,
    L"ibm/ps2/m80",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m80_info,
    NULL
};
