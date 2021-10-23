/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		ET4000/W32p emulation (Diamond Stealth 32)
 *
 * Known bugs:	Accelerator doesn't work in planar modes
 *
 * FIXME:	Note the madness on line 1163, fix that somehow?  --FvK
 *
 * Version:	@(#)vid_et4000w32.c	1.0.26	2021/10/21
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
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "../system/pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_icd2061.h"
#include "vid_stg_ramdac.h"


#define BIOS_ROM_PATH_DIAMOND	L"video/tseng/et4000w32/et4000w32.bin"
#define BIOS_ROM_PATH_CARDEX	L"video/tseng/et4000w32/cardex.vbi"


#define FIFO_SIZE 65536
#define FIFO_MASK (FIFO_SIZE - 1)
#define FIFO_ENTRY_SIZE (1 << 31)

#define FIFO_ENTRIES (dev->fifo_write_idx - dev->fifo_read_idx)
#define FIFO_FULL    ((dev->fifo_write_idx - dev->fifo_read_idx) >= (FIFO_SIZE-1))
#define FIFO_EMPTY   (dev->fifo_read_idx == dev->fifo_write_idx)

#define FIFO_TYPE 0xff000000
#define FIFO_ADDR 0x00ffffff

#define ACL_WRST 1
#define ACL_RDST 2
#define ACL_XYST 4
#define ACL_SSO  8

enum
{
    ET4000W32_CARDEX = 0,
    ET4000W32_DIAMOND
};

enum
{
    FIFO_INVALID    = (0x00 << 24),
    FIFO_WRITE_BYTE = (0x01 << 24),
    FIFO_WRITE_MMU  = (0x02 << 24)
};

typedef struct
{
    uint32_t addr_type;
    uint32_t val;
} fifo_entry_t;

typedef struct
{
    mem_map_t linear_mapping;
    mem_map_t mmu_mapping;
    
    rom_t bios_rom;
    
    svga_t svga;

    int index;
    int pci;
    uint8_t regs[256];
    uint32_t linearbase, linearbase_old;
    uint32_t vram_mask;

    uint8_t banking, banking2;

    uint8_t pci_regs[256];
    
    int interleaved;

    /*Accelerator*/
    struct {
	struct {
		uint32_t pattern_addr,source_addr,dest_addr,mix_addr;
		uint16_t pattern_off,source_off,dest_off,mix_off;
		uint8_t pixel_depth,xy_dir;
		uint8_t pattern_wrap,source_wrap;
		uint16_t count_x,count_y;
		uint8_t ctrl_routing,ctrl_reload;
		uint8_t rop_fg,rop_bg;
		uint16_t pos_x,pos_y;
		uint16_t error;
		uint16_t dmin,dmaj;
	} queued,internal;
	uint32_t pattern_addr,source_addr,dest_addr,mix_addr;
	uint32_t pattern_back,source_back,dest_back,mix_back;
	unsigned int pattern_x,source_x;
	unsigned int pattern_x_back,source_x_back;
	unsigned int pattern_y,source_y;
	uint8_t status;
	uint64_t cpu_dat;
	int cpu_dat_pos;
	int pix_pos;
    } acl;

    struct {
	uint32_t base[3];
	uint8_t ctrl;
    } mmu;

    fifo_entry_t fifo[FIFO_SIZE];
    volatile int fifo_read_idx, fifo_write_idx;

    thread_t *fifo_thread;
    event_t *wake_fifo_thread;
    event_t *fifo_not_full_event;
    
    int blitter_busy;
    uint64_t blitter_time;
    uint64_t status_time;
    int type;
} et4000w32p_t;


static uint8_t et4000w32p_mmu_read(uint32_t addr, priv_t);
static void et4000w32p_mmu_write(uint32_t addr, uint8_t val, priv_t);
static void et4000w32_blit_start(et4000w32p_t *et4000);
static void et4000w32_blit(int count, uint32_t mix, uint32_t sdat, int cpu_input, et4000w32p_t *et4000);


static void
et4000w32p_recalcmapping(et4000w32p_t *dev)
{
    svga_t *svga = &dev->svga;

    if (!(dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM)) {
	mem_map_disable(&svga->mapping);
	mem_map_disable(&dev->linear_mapping);
	mem_map_disable(&dev->mmu_mapping);
	return;
    }

    if (svga->crtc[0x36] & 0x10) /*Linear frame buffer*/ {
	mem_map_set_addr(&dev->linear_mapping, dev->linearbase, 0x200000);
	mem_map_disable(&svga->mapping);
	mem_map_disable(&dev->mmu_mapping);
    } else {
	int map = (svga->gdcreg[6] & 0xc) >> 2;
	if (svga->crtc[0x36] & 0x20) map |= 4;
	if (svga->crtc[0x36] & 0x08) map |= 8;
	switch (map) {
		case 0x0: case 0x4: case 0x8: case 0xc: /*128k at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x20000);
			mem_map_disable(&dev->mmu_mapping);
			svga->banked_mask = 0x1ffff;
			break;
		case 0x1: /*64k at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
			mem_map_disable(&dev->mmu_mapping);
			svga->banked_mask = 0xffff;
			break;
		case 0x2: /*32k at B0000*/
			mem_map_set_addr(&svga->mapping, 0xb0000, 0x08000);
			mem_map_disable(&dev->mmu_mapping);
			svga->banked_mask = 0x7fff;
			break;
		case 0x3: /*32k at B8000*/
			mem_map_set_addr(&svga->mapping, 0xb8000, 0x08000);
			mem_map_disable(&dev->mmu_mapping);
			svga->banked_mask = 0x7fff;
			break;
		case 0x5: case 0x9: case 0xd: /*64k at A0000, MMU at B8000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
			mem_map_set_addr(&dev->mmu_mapping, 0xb8000, 0x08000);
			svga->banked_mask = 0xffff;
			break;
		case 0x6: case 0xa: case 0xe: /*32k at B0000, MMU at A8000*/
			mem_map_set_addr(&svga->mapping, 0xb0000, 0x08000);
			mem_map_set_addr(&dev->mmu_mapping, 0xa8000, 0x08000);
			svga->banked_mask = 0x7fff;
			break;
		case 0x7: case 0xb: case 0xf: /*32k at B8000, MMU at A8000*/
			mem_map_set_addr(&svga->mapping, 0xb8000, 0x08000);
			mem_map_set_addr(&dev->mmu_mapping, 0xa8000, 0x08000);
			svga->banked_mask = 0x7fff;
			break;
		}

	mem_map_disable(&dev->linear_mapping);
    }
    dev->linearbase_old = dev->linearbase;
    
    if (!dev->interleaved && (dev->svga.crtc[0x32] & 0x80))
	mem_map_disable(&svga->mapping);
}


static void
et4000w32p_recalctimings(svga_t *svga)
{
    svga->ma_latch |= (svga->crtc[0x33] & 0x7) << 16;

    if (svga->crtc[0x35] & 0x01)     svga->vblankstart += 0x400;
    if (svga->crtc[0x35] & 0x02)     svga->vtotal      += 0x400;
    if (svga->crtc[0x35] & 0x04)     svga->dispend     += 0x400;
    if (svga->crtc[0x35] & 0x08)     svga->vsyncstart  += 0x400;
    if (svga->crtc[0x35] & 0x10)     svga->split       += 0x400;
    if (svga->crtc[0x3F] & 0x80)     svga->rowoffset   += 0x100;
    if (svga->crtc[0x3F] & 0x01)     svga->htotal      += 256;
    if (svga->attrregs[0x16] & 0x20) svga->hdisp <<= 1;
    
    svga->clock = cpu_clock / svga->getclock((svga->miscout >> 2) & 3, svga->clock_gen);

    switch (svga->bpp) {
	case 15: case 16:
		svga->hdisp >>= 1;
		break;
	case 24:
		svga->hdisp /= 3;
		break;
    }
}


static uint8_t
et4000w32p_in(uint16_t addr, priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;
    svga_t *svga = &dev->svga;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
	addr ^= 0x60;

    switch (addr) {
	case 0x3c5:
		if ((svga->seqaddr & 0xf) == 7) 
			return svga->seqregs[svga->seqaddr & 0xf] | 4;
		break;

	case 0x3c6: case 0x3c7: case 0x3c8: case 0x3c9:
		return stg_ramdac_in(addr, svga->ramdac, svga);

	case 0x3cb:
		return dev->banking2;
	case 0x3cd:
		return dev->banking;
	case 0x3d4:
		return svga->crtcreg;
	case 0x3d5:
		return svga->crtc[svga->crtcreg];

	case 0x210a: case 0x211a: case 0x212a: case 0x213a:
	case 0x214a: case 0x215a: case 0x216a: case 0x217a:
		return dev->index;

	case 0x210b: case 0x211b: case 0x212b: case 0x213b:
	case 0x214b: case 0x215b: case 0x216b: case 0x217b:
		if (dev->index==0xec) 
			return (dev->regs[0xec] & 0xf) | 0x60; /*ET4000/W32p rev D*/
		if (dev->index == 0xee) { /*Preliminary implementation*/
			if (svga->bpp == 8)
				return 3;
			else if (svga->bpp == 16)
				return 4;
			else
				break;
		}
		if (dev->index == 0xef) {
			if (dev->pci) return dev->regs[0xef] | 0xe0;       /*PCI*/
			else          return dev->regs[0xef] | 0x60;       /*VESA local bus*/
		}
		return dev->regs[dev->index];
	}
    return svga_in(addr, svga);
}


static void
et4000w32p_out(uint16_t addr, uint8_t val, priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t old;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
		addr ^= 0x60;

    switch (addr) {
	case 0x3c2:
		if (dev->type == ET4000W32_DIAMOND)
			icd2061_write(svga->clock_gen, (val >> 2) & 3);
		break;

	case 0x3c6: case 0x3c7: case 0x3c8: case 0x3c9:
		stg_ramdac_out(addr, val, svga->ramdac, svga);
		return;

	case 0x3cb: /*Banking extension*/
		if (!(svga->crtc[0x36] & 0x10) && !(svga->gdcreg[6] & 0x08)) {
			svga->write_bank = (svga->write_bank & 0xfffff) | ((val & 1) << 20);
			svga->read_bank  = (svga->read_bank  & 0xfffff) | ((val & 0x10) << 16);
		}
		dev->banking2 = val;
		return;

	case 0x3cd: /*Banking*/
		if (!(svga->crtc[0x36] & 0x10) && !(svga->gdcreg[6] & 0x08)) {
			svga->write_bank = (svga->write_bank & 0x100000) | ((val & 0xf) * 65536);
			svga->read_bank  = (svga->read_bank  & 0x100000) | (((val >> 4) & 0xf) * 65536);
		}
		dev->banking = val;
		return;

	case 0x3cf:
		switch (svga->gdcaddr & 15) {
			case 6:
				if (!(svga->crtc[0x36] & 0x10) && !(val & 0x08)) {
				svga->write_bank = ((dev->banking2 & 1) << 20) | ((dev->banking & 0xf) * 65536);
				svga->read_bank  = ((dev->banking2 & 0x10) << 16) | (((dev->banking >> 4) & 0xf) * 65536);
				} else
					svga->write_bank = svga->read_bank = 0;

				svga->gdcreg[svga->gdcaddr & 15] = val;
				et4000w32p_recalcmapping(dev);
				return;
		}
		break;
	case 0x3d4:
		svga->crtcreg = val & 63;
		return;
	case 0x3d5:
		if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 0x35) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		old = svga->crtc[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;
		if (svga->crtcreg == 0x36) {
			if (!(val & 0x10) && !(svga->gdcreg[6] & 0x08)) {
				svga->write_bank = ((dev->banking2 & 1) << 20) | ((dev->banking & 0xf) * 65536);
				svga->read_bank  = ((dev->banking2 & 0x10) << 16) | (((dev->banking >> 4) & 0xf) * 65536);
			} else
				svga->write_bank = svga->read_bank = 0;
		}
		if (old != val) {
			if (svga->crtcreg < 0xe || svga->crtcreg > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		if (svga->crtcreg == 0x30) {
			if (dev->pci) {
				dev->linearbase &= 0xc0000000;
				dev->linearbase = (val & 0xfc) << 22;
			} else {
				dev->linearbase = val << 22;
			}
			et4000w32p_recalcmapping(dev);
		}
		if (svga->crtcreg == 0x32 || svga->crtcreg == 0x36)
			et4000w32p_recalcmapping(dev);
		break;

	case 0x210a: case 0x211a: case 0x212a: case 0x213a:
	case 0x214a: case 0x215a: case 0x216a: case 0x217a:
		dev->index=val;
		return;

	case 0x210b: case 0x211b: case 0x212b: case 0x213b:
	case 0x214b: case 0x215b: case 0x216b: case 0x217b:
		dev->regs[dev->index] = val;
		svga->hwcursor.x     = dev->regs[0xe0] | ((dev->regs[0xe1] & 7) << 8);
		svga->hwcursor.y     = dev->regs[0xe4] | ((dev->regs[0xe5] & 7) << 8);
		svga->hwcursor.addr  = (dev->regs[0xe8] | (dev->regs[0xe9] << 8) | ((dev->regs[0xea] & 7) << 16)) << 2;
		svga->hwcursor.addr += (dev->regs[0xe6] & 63) * 16;
		svga->hwcursor.ena   = dev->regs[0xf7] & 0x80;
		svga->hwcursor.xoff  = dev->regs[0xe2] & 63;
		svga->hwcursor.yoff  = dev->regs[0xe6] & 63;
		return;
    }
    svga_out(addr, val, svga);
}

static void et4000w32p_accel_write_fifo(et4000w32p_t *dev, uint32_t addr, uint8_t val)
{
    switch (addr & 0x7fff) {
	case 0x7f80: dev->acl.queued.pattern_addr = (dev->acl.queued.pattern_addr & 0xFFFFFF00) | val;         break;
	case 0x7f81: dev->acl.queued.pattern_addr = (dev->acl.queued.pattern_addr & 0xFFFF00FF) | (val << 8);  break;
	case 0x7f82: dev->acl.queued.pattern_addr = (dev->acl.queued.pattern_addr & 0xFF00FFFF) | (val << 16); break;
	case 0x7f83: dev->acl.queued.pattern_addr = (dev->acl.queued.pattern_addr & 0x00FFFFFF) | (val << 24); break;
	case 0x7f84: dev->acl.queued.source_addr  = (dev->acl.queued.source_addr  & 0xFFFFFF00) | val;         break;
	case 0x7f85: dev->acl.queued.source_addr  = (dev->acl.queued.source_addr  & 0xFFFF00FF) | (val << 8);  break;
	case 0x7f86: dev->acl.queued.source_addr  = (dev->acl.queued.source_addr  & 0xFF00FFFF) | (val << 16); break;
	case 0x7f87: dev->acl.queued.source_addr  = (dev->acl.queued.source_addr  & 0x00FFFFFF) | (val << 24); break;
	case 0x7f88: dev->acl.queued.pattern_off  = (dev->acl.queued.pattern_off  & 0xFF00) | val;        break;
	case 0x7f89: dev->acl.queued.pattern_off  = (dev->acl.queued.pattern_off  & 0x00FF) | (val << 8); break;
	case 0x7f8a: dev->acl.queued.source_off   = (dev->acl.queued.source_off   & 0xFF00) | val;        break;
	case 0x7f8b: dev->acl.queued.source_off   = (dev->acl.queued.source_off   & 0x00FF) | (val << 8); break;
	case 0x7f8c: dev->acl.queued.dest_off     = (dev->acl.queued.dest_off     & 0xFF00) | val;        break;
	case 0x7f8d: dev->acl.queued.dest_off     = (dev->acl.queued.dest_off     & 0x00FF) | (val << 8); break;
	case 0x7f8e: dev->acl.queued.pixel_depth = val; break;
	case 0x7f8f: dev->acl.queued.xy_dir = val; break;
	case 0x7f90: dev->acl.queued.pattern_wrap = val; break;
	case 0x7f92: dev->acl.queued.source_wrap = val; break;
	case 0x7f98: dev->acl.queued.count_x    = (dev->acl.queued.count_x & 0xFF00) | val;        break;
	case 0x7f99: dev->acl.queued.count_x    = (dev->acl.queued.count_x & 0x00FF) | (val << 8); break;
	case 0x7f9a: dev->acl.queued.count_y    = (dev->acl.queued.count_y & 0xFF00) | val;        break;
	case 0x7f9b: dev->acl.queued.count_y    = (dev->acl.queued.count_y & 0x00FF) | (val << 8); break;
	case 0x7f9c: dev->acl.queued.ctrl_routing = val; break;
	case 0x7f9d: dev->acl.queued.ctrl_reload  = val; break;
	case 0x7f9e: dev->acl.queued.rop_bg       = val; break;
	case 0x7f9f: dev->acl.queued.rop_fg       = val; break;
	case 0x7fa0: dev->acl.queued.dest_addr = (dev->acl.queued.dest_addr & 0xFFFFFF00) | val;         break;
	case 0x7fa1: dev->acl.queued.dest_addr = (dev->acl.queued.dest_addr & 0xFFFF00FF) | (val << 8);  break;
	case 0x7fa2: dev->acl.queued.dest_addr = (dev->acl.queued.dest_addr & 0xFF00FFFF) | (val << 16); break;
	case 0x7fa3: dev->acl.queued.dest_addr = (dev->acl.queued.dest_addr & 0x00FFFFFF) | (val << 24);
		dev->acl.internal = dev->acl.queued;
		et4000w32_blit_start(dev);
		if (!(dev->acl.queued.ctrl_routing & 0x43)) {
			et4000w32_blit(0xffffff, ~0, 0, 0, dev);
		}
		if ((dev->acl.queued.ctrl_routing & 0x40) && !(dev->acl.internal.ctrl_routing & 3))
			et4000w32_blit(4, ~0, 0, 0, dev);
		break;
	case 0x7fa4: dev->acl.queued.mix_addr = (dev->acl.queued.mix_addr & 0xFFFFFF00) | val;         break;
	case 0x7fa5: dev->acl.queued.mix_addr = (dev->acl.queued.mix_addr & 0xFFFF00FF) | (val << 8);  break;
	case 0x7fa6: dev->acl.queued.mix_addr = (dev->acl.queued.mix_addr & 0xFF00FFFF) | (val << 16); break;
	case 0x7fa7: dev->acl.queued.mix_addr = (dev->acl.queued.mix_addr & 0x00FFFFFF) | (val << 24); break;
	case 0x7fa8: dev->acl.queued.mix_off = (dev->acl.queued.mix_off & 0xFF00) | val;        break;
	case 0x7fa9: dev->acl.queued.mix_off = (dev->acl.queued.mix_off & 0x00FF) | (val << 8); break;
	case 0x7faa: dev->acl.queued.error   = (dev->acl.queued.error   & 0xFF00) | val;        break;
	case 0x7fab: dev->acl.queued.error   = (dev->acl.queued.error   & 0x00FF) | (val << 8); break;
	case 0x7fac: dev->acl.queued.dmin    = (dev->acl.queued.dmin    & 0xFF00) | val;        break;
	case 0x7fad: dev->acl.queued.dmin    = (dev->acl.queued.dmin    & 0x00FF) | (val << 8); break;
	case 0x7fae: dev->acl.queued.dmaj    = (dev->acl.queued.dmaj    & 0xFF00) | val;        break;
	case 0x7faf: dev->acl.queued.dmaj    = (dev->acl.queued.dmaj    & 0x00FF) | (val << 8); break;
    }
}

static void et4000w32p_accel_write_mmu(et4000w32p_t *dev, uint32_t addr, uint8_t val)
{
    if (!(dev->acl.status & ACL_XYST)) return;
    if (dev->acl.internal.ctrl_routing & 3) {
	if ((dev->acl.internal.ctrl_routing & 3) == 2) {
		if (dev->acl.mix_addr & 7)
			et4000w32_blit(8 - (dev->acl.mix_addr & 7), val >> (dev->acl.mix_addr & 7), 0, 1, dev);
		else
			et4000w32_blit(8, val, 0, 1, dev);
	}
	else if ((dev->acl.internal.ctrl_routing & 3) == 1)
		et4000w32_blit(1, ~0, val, 2, dev);
    }
}

static void fifo_thread(void *param)
{
    et4000w32p_t *dev = (et4000w32p_t *)param;

    uint64_t start_time = 0;
    uint64_t end_time = 0;

    fifo_entry_t *fifo;
    
    while (1) {
	thread_set_event(dev->fifo_not_full_event);
	thread_wait_event(dev->wake_fifo_thread, -1);
	thread_reset_event(dev->wake_fifo_thread);
	dev->blitter_busy = 1;
	while (!FIFO_EMPTY) {
		start_time = plat_timer_read();
		fifo = &dev->fifo[dev->fifo_read_idx & FIFO_MASK];

		switch (fifo->addr_type & FIFO_TYPE) {
			case FIFO_WRITE_BYTE:
				et4000w32p_accel_write_fifo(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
				break;
			case FIFO_WRITE_MMU:
				et4000w32p_accel_write_mmu(dev, fifo->addr_type & FIFO_ADDR, fifo->val);
				break;
		}

		dev->fifo_read_idx++;
		fifo->addr_type = FIFO_INVALID;

		if (FIFO_ENTRIES > 0xe000)
			thread_set_event(dev->fifo_not_full_event);

		end_time = plat_timer_read();
		dev->blitter_time += end_time - start_time;
	}
    dev->blitter_busy = 0;
    }
}

static __inline void wake_fifo_thread(et4000w32p_t *dev)
{
        thread_set_event(dev->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

static void et4000w32p_wait_fifo_idle(et4000w32p_t *dev)
{
    while (!FIFO_EMPTY) {
	wake_fifo_thread(dev);
	thread_wait_event(dev->fifo_not_full_event, 1);
    }
}

static void et4000w32p_queue(et4000w32p_t *dev, uint32_t addr, uint32_t val, uint32_t type)
{
    fifo_entry_t *fifo = &dev->fifo[dev->fifo_write_idx & FIFO_MASK];

    if (FIFO_FULL) {
	thread_reset_event(dev->fifo_not_full_event);
	if (FIFO_FULL) {
		thread_wait_event(dev->fifo_not_full_event, -1); /*Wait for room in ringbuffer*/
	}
    }

    fifo->val = val;
    fifo->addr_type = (addr & FIFO_ADDR) | type;

    dev->fifo_write_idx++;

    if (FIFO_ENTRIES > 0xe000 || FIFO_ENTRIES < 8)
            wake_fifo_thread(dev);
}

static void et4000w32p_mmu_write(uint32_t addr, uint8_t val, priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;
    svga_t *svga = &dev->svga;
    int bank;
    switch (addr & 0x6000) {
	case 0x0000: /*MMU 0*/
	case 0x2000: /*MMU 1*/
	case 0x4000: /*MMU 2*/
		bank = (addr >> 13) & 3;
		if (dev->mmu.ctrl & (1 << bank)) {
			et4000w32p_queue(dev, addr & 0x7fff, val, FIFO_WRITE_MMU);
		} else {
			if ((addr&0x1fff) + dev->mmu.base[bank] < svga->vram_max) {
				svga->vram[(addr & 0x1fff) + dev->mmu.base[bank]] = val;
				svga->changedvram[((addr & 0x1fff) + dev->mmu.base[bank]) >> 12] = changeframecount;
			}
		}
		break;
	case 0x6000:
		if ((addr & 0x7fff) >= 0x7f80) {
			et4000w32p_queue(dev, addr & 0x7fff, val, FIFO_WRITE_BYTE);
		}
		else switch (addr & 0x7fff) {
			case 0x7f00: dev->mmu.base[0] = (dev->mmu.base[0] & 0xFFFFFF00) | val;         break;
			case 0x7f01: dev->mmu.base[0] = (dev->mmu.base[0] & 0xFFFF00FF) | (val << 8);  break;
			case 0x7f02: dev->mmu.base[0] = (dev->mmu.base[0] & 0xFF00FFFF) | (val << 16); break;
			case 0x7f03: dev->mmu.base[0] = (dev->mmu.base[0] & 0x00FFFFFF) | (val << 24); break;
			case 0x7f04: dev->mmu.base[1] = (dev->mmu.base[1] & 0xFFFFFF00) | val;         break;
			case 0x7f05: dev->mmu.base[1] = (dev->mmu.base[1] & 0xFFFF00FF) | (val << 8);  break;
			case 0x7f06: dev->mmu.base[1] = (dev->mmu.base[1] & 0xFF00FFFF) | (val << 16); break;
			case 0x7f07: dev->mmu.base[1] = (dev->mmu.base[1] & 0x00FFFFFF) | (val << 24); break;
			case 0x7f08: dev->mmu.base[2] = (dev->mmu.base[2] & 0xFFFFFF00) | val;         break;
			case 0x7f09: dev->mmu.base[2] = (dev->mmu.base[2] & 0xFFFF00FF) | (val << 8);  break;
			case 0x7f0a: dev->mmu.base[2] = (dev->mmu.base[2] & 0xFF00FFFF) | (val << 16); break;
			case 0x7f0d: dev->mmu.base[2] = (dev->mmu.base[2] & 0x00FFFFFF) | (val << 24); break;
			case 0x7f13: dev->mmu.ctrl=val; break;
		}
		break;
    }
}

static uint8_t et4000w32p_mmu_read(uint32_t addr, priv_t pric)
{
    et4000w32p_t *dev = (et4000w32p_t *)pric;
    svga_t *svga = &dev->svga;
    int bank;
    uint8_t temp;
    switch (addr & 0x6000) {
	case 0x0000: /*MMU 0*/
	case 0x2000: /*MMU 1*/
	case 0x4000: /*MMU 2*/
		bank = (addr >> 13) & 3;
		if (dev->mmu.ctrl & (1 << bank)) {
			et4000w32p_wait_fifo_idle(dev);
			temp = 0xff;
			if (dev->acl.cpu_dat_pos) {
				dev->acl.cpu_dat_pos--;
				temp = dev->acl.cpu_dat & 0xff;
				dev->acl.cpu_dat >>= 8;
				}
			if ((dev->acl.queued.ctrl_routing & 0x40) && !dev->acl.cpu_dat_pos && !(dev->acl.internal.ctrl_routing & 3))
				et4000w32_blit(4, ~0, 0, 0, dev);
				/*???*/
			return temp;
		}
		if ((addr&0x1fff) + dev->mmu.base[bank] >= svga->vram_max)
			return 0xff;
		return svga->vram[(addr&0x1fff) + dev->mmu.base[bank]];

	case 0x6000:
		if ((addr & 0x7fff) >= 0x7f80)
			et4000w32p_wait_fifo_idle(dev);
		switch (addr&0x7fff) {
			case 0x7f00: return dev->mmu.base[0];
			case 0x7f01: return dev->mmu.base[0] >> 8;
			case 0x7f02: return dev->mmu.base[0] >> 16;
			case 0x7f03: return dev->mmu.base[0] >> 24;
			case 0x7f04: return dev->mmu.base[1];
			case 0x7f05: return dev->mmu.base[1] >> 8;
			case 0x7f06: return dev->mmu.base[1] >> 16;
			case 0x7f07: return dev->mmu.base[1] >> 24;
			case 0x7f08: return dev->mmu.base[2];
			case 0x7f09: return dev->mmu.base[2] >> 8;
			case 0x7f0a: return dev->mmu.base[2] >> 16;
			case 0x7f0b: return dev->mmu.base[2] >> 24;
			case 0x7f13: return dev->mmu.ctrl;

			case 0x7f36:
				temp = dev->acl.status;
				temp &= ~0x03;
				if (!FIFO_EMPTY)
					temp |= 0x02;
				if (FIFO_FULL)
					temp |= 0x01;
				return temp;
			case 0x7f80: return dev->acl.internal.pattern_addr;
			case 0x7f81: return dev->acl.internal.pattern_addr >> 8;
			case 0x7f82: return dev->acl.internal.pattern_addr >> 16;
			case 0x7f83: return dev->acl.internal.pattern_addr >> 24;
			case 0x7f84: return dev->acl.internal.source_addr;
			case 0x7f85: return dev->acl.internal.source_addr >> 8;
			case 0x7f86: return dev->acl.internal.source_addr >> 16;
			case 0x7f87: return dev->acl.internal.source_addr >> 24;
			case 0x7f88: return dev->acl.internal.pattern_off & 0xff;
			case 0x7f89: return dev->acl.internal.pattern_off >> 8;
			case 0x7f8a: return dev->acl.internal.source_off & 0xff;
			case 0x7f8b: return dev->acl.internal.source_off >> 8;
			case 0x7f8c: return dev->acl.internal.dest_off & 0xff;
			case 0x7f8d: return dev->acl.internal.dest_off >> 8;
			case 0x7f8e: return dev->acl.internal.pixel_depth;
			case 0x7f8f: return dev->acl.internal.xy_dir;
			case 0x7f90: return dev->acl.internal.pattern_wrap;
			case 0x7f92: return dev->acl.internal.source_wrap;
			case 0x7f98: return dev->acl.internal.count_x & 0xff;
			case 0x7f99: return dev->acl.internal.count_x >> 8;
			case 0x7f9a: return dev->acl.internal.count_y & 0xff;
			case 0x7f9b: return dev->acl.internal.count_y >> 8;
			case 0x7f9c: return dev->acl.internal.ctrl_routing;
			case 0x7f9d: return dev->acl.internal.ctrl_reload;
			case 0x7f9e: return dev->acl.internal.rop_bg;
			case 0x7f9f: return dev->acl.internal.rop_fg;
			case 0x7fa0: return dev->acl.internal.dest_addr;
			case 0x7fa1: return dev->acl.internal.dest_addr >> 8;
			case 0x7fa2: return dev->acl.internal.dest_addr >> 16;
			case 0x7fa3: return dev->acl.internal.dest_addr >> 24;
		}
		return 0xff;
	}
    return 0xff;
}

static unsigned int et4000w32_max_x[8]={0,0,4,8,16,32,64,0x70000000};
static unsigned int et4000w32_wrap_x[8]={0,0,3,7,15,31,63,0xFFFFFFFF};
static unsigned int et4000w32_wrap_y[8]={1,2,4,8,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

/* int bltout=0; */
static void et4000w32_blit_start(et4000w32p_t *dev)
{
    if (!(dev->acl.queued.xy_dir & 0x20))
	dev->acl.internal.error = dev->acl.internal.dmaj / 2;
    dev->acl.pattern_addr= dev->acl.internal.pattern_addr;
    dev->acl.source_addr = dev->acl.internal.source_addr;
    dev->acl.mix_addr    = dev->acl.internal.mix_addr;
    dev->acl.mix_back    = dev->acl.mix_addr;
    dev->acl.dest_addr   = dev->acl.internal.dest_addr;
    dev->acl.dest_back   = dev->acl.dest_addr;
    dev->acl.internal.pos_x = dev->acl.internal.pos_y = 0;
    dev->acl.pattern_x = dev->acl.source_x = dev->acl.pattern_y = dev->acl.source_y = 0;
    dev->acl.status |= ACL_XYST;
    if ((!(dev->acl.internal.ctrl_routing & 7) || (dev->acl.internal.ctrl_routing & 4)) && !(dev->acl.internal.ctrl_routing & 0x40)) 
	dev->acl.status |= ACL_SSO;

    if (et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7]) {
	dev->acl.pattern_x = dev->acl.pattern_addr & et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7];
	dev->acl.pattern_addr &= ~et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7];
    }
    dev->acl.pattern_back = dev->acl.pattern_addr;
    if (!(dev->acl.internal.pattern_wrap & 0x40)) {
	dev->acl.pattern_y = (dev->acl.pattern_addr / (et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7] + 1)) & (et4000w32_wrap_y[(dev->acl.internal.pattern_wrap >> 4) & 7] - 1);
	dev->acl.pattern_back &= ~(((et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7] + 1) * et4000w32_wrap_y[(dev->acl.internal.pattern_wrap >> 4) & 7]) - 1);
    }
    dev->acl.pattern_x_back = dev->acl.pattern_x;
    
    if (et4000w32_wrap_x[dev->acl.internal.source_wrap & 7]) {
	dev->acl.source_x = dev->acl.source_addr & et4000w32_wrap_x[dev->acl.internal.source_wrap & 7];
	dev->acl.source_addr &= ~et4000w32_wrap_x[dev->acl.internal.source_wrap & 7];
    }
    dev->acl.source_back = dev->acl.source_addr;
    if (!(dev->acl.internal.source_wrap & 0x40)) {
	dev->acl.source_y = (dev->acl.source_addr / (et4000w32_wrap_x[dev->acl.internal.source_wrap & 7] + 1)) & (et4000w32_wrap_y[(dev->acl.internal.source_wrap >> 4) & 7] - 1);
	dev->acl.source_back &= ~(((et4000w32_wrap_x[dev->acl.internal.source_wrap & 7] + 1) * et4000w32_wrap_y[(dev->acl.internal.source_wrap >> 4) & 7]) - 1);
    }
    dev->acl.source_x_back = dev->acl.source_x;

        et4000w32_max_x[2] = ((dev->acl.internal.pixel_depth & 0x30) == 0x20) ? 3 : 4;
        
        dev->acl.internal.count_x += (dev->acl.internal.pixel_depth >> 4) & 3;
        dev->acl.cpu_dat_pos = 0;
        dev->acl.cpu_dat = 0;
        
        dev->acl.pix_pos = 0;
}

static void et4000w32_incx(int c, et4000w32p_t *dev)
{
    dev->acl.dest_addr += c;
    dev->acl.pattern_x += c;
    dev->acl.source_x  += c;
    dev->acl.mix_addr  += c;
    if (dev->acl.pattern_x >= et4000w32_max_x[dev->acl.internal.pattern_wrap & 7])
	dev->acl.pattern_x  -= et4000w32_max_x[dev->acl.internal.pattern_wrap & 7];
    if (dev->acl.source_x  >= et4000w32_max_x[dev->acl.internal.source_wrap  & 7])
	dev->acl.source_x   -= et4000w32_max_x[dev->acl.internal.source_wrap  & 7];
}
void et4000w32_decx(int c, et4000w32p_t *dev)
{
    dev->acl.dest_addr -= c;
    dev->acl.pattern_x -= c;
    dev->acl.source_x  -= c;
    dev->acl.mix_addr  -= c;
#if 0	/* pattern_x is unsigned */
        if (dev->acl.pattern_x < 0)
           dev->acl.pattern_x  += et4000w32_max_x[dev->acl.internal.pattern_wrap & 7];
#endif
#if 0	/* source_x is unsigned */
        if (dev->acl.source_x  < 0)
           dev->acl.source_x   += et4000w32_max_x[dev->acl.internal.source_wrap  & 7];
#endif
}
void et4000w32_incy(et4000w32p_t *dev)
{
    dev->acl.pattern_addr += dev->acl.internal.pattern_off + 1;
    dev->acl.source_addr  += dev->acl.internal.source_off  + 1;
    dev->acl.mix_addr     += dev->acl.internal.mix_off     + 1;
    dev->acl.dest_addr    += dev->acl.internal.dest_off    + 1;
    dev->acl.pattern_y++;
    if (dev->acl.pattern_y == et4000w32_wrap_y[(dev->acl.internal.pattern_wrap >> 4) & 7]) {
	dev->acl.pattern_y = 0;
	dev->acl.pattern_addr = dev->acl.pattern_back;
    }
    dev->acl.source_y++;
    if (dev->acl.source_y == et4000w32_wrap_y[(dev->acl.internal.source_wrap >> 4) & 7]) {
	dev->acl.source_y = 0;
	dev->acl.source_addr = dev->acl.source_back;
    }
}
void et4000w32_decy(et4000w32p_t *dev)
{
    dev->acl.pattern_addr -= dev->acl.internal.pattern_off + 1;
    dev->acl.source_addr  -= dev->acl.internal.source_off  + 1;
    dev->acl.mix_addr     -= dev->acl.internal.mix_off     + 1;
    dev->acl.dest_addr    -= dev->acl.internal.dest_off    + 1;
    dev->acl.pattern_y--;
#if 0	/* pattern_y is unsigned */
        if (dev->acl.pattern_y < 0 && !(dev->acl.internal.pattern_wrap & 0x40))
        {
                dev->acl.pattern_y = et4000w32_wrap_y[(dev->acl.internal.pattern_wrap >> 4) & 7] - 1;
                dev->acl.pattern_addr = dev->acl.pattern_back + (et4000w32_wrap_x[dev->acl.internal.pattern_wrap & 7] * (et4000w32_wrap_y[(dev->acl.internal.pattern_wrap >> 4) & 7] - 1));
        }
#endif
    dev->acl.source_y--;
#if 0	/* source_y is unsigned */
        if (dev->acl.source_y < 0 && !(dev->acl.internal.source_wrap & 0x40))
        {
                dev->acl.source_y = et4000w32_wrap_y[(dev->acl.internal.source_wrap >> 4) & 7] - 1;
                dev->acl.source_addr = dev->acl.source_back + (et4000w32_wrap_x[dev->acl.internal.source_wrap & 7] *(et4000w32_wrap_y[(dev->acl.internal.source_wrap >> 4) & 7] - 1));;
        }
#endif
}

static void et4000w32_blit(int count, uint32_t mix, uint32_t sdat, int cpu_input, et4000w32p_t *dev)
{
    svga_t *svga = &dev->svga;
    int c,d;
    uint8_t pattern, source, dest, out;
    uint8_t rop;
    int mixdat;

    if (!(dev->acl.status & ACL_XYST)) return;
    if (dev->acl.internal.xy_dir & 0x80) /*Line draw*/ {
	while (count--) {
		/* if (bltout) DEBUG("%i,%i : ", dev->acl.internal.pos_x, dev->acl.internal.pos_y); */
		pattern = svga->vram[(dev->acl.pattern_addr + dev->acl.pattern_x) & 0x1fffff];
		source  = svga->vram[(dev->acl.source_addr  + dev->acl.source_x)  & 0x1fffff];
		/* if (bltout) DEBUG("%06X %06X ", (dev->acl.pattern_addr + dev->acl.pattern_x) & 0x1fffff, (dev->acl.source_addr + dev->acl.source_x) & 0x1fffff); */
		if (cpu_input == 2) {
			source = sdat & 0xff;
			sdat >>= 8;
		}
		dest = svga->vram[dev->acl.dest_addr & 0x1fffff];
		out = 0;
		/* if (bltout) DEBUG("%06X   ", dev->acl.dest_addr); */
		if ((dev->acl.internal.ctrl_routing & 0xa) == 8) {
			mixdat = svga->vram[(dev->acl.mix_addr >> 3) & 0x1fffff] & (1 << (dev->acl.mix_addr & 7));
			/* if (bltout) DEBUG("%06X %02X  ", dev->acl.mix_addr, svga->vram[(dev->acl.mix_addr >> 3) & 0x1fffff]); */
		} else {
			mixdat = mix & 1;
			mix >>= 1; 
			mix |= 0x80000000;
		}
		dev->acl.mix_addr++;
		rop = mixdat ? dev->acl.internal.rop_fg : dev->acl.internal.rop_bg;
		for (c = 0; c < 8; c++) {
			d = (dest & (1 << c)) ? 1 : 0;
			if (source & (1 << c))  d |= 2;
			if (pattern & (1 << c)) d |= 4;
			if (rop & (1 << d)) out |= (1 << c);
		}
		/* if (bltout) DEBUG("%06X = %02X\n", dev->acl.dest_addr & 0x1fffff, out); */
		if (!(dev->acl.internal.ctrl_routing & 0x40)) {
			svga->vram[dev->acl.dest_addr & 0x1fffff] = out;
			svga->changedvram[(dev->acl.dest_addr & 0x1fffff) >> 12] = changeframecount;
		} else {
			dev->acl.cpu_dat |= ((uint64_t)out << (dev->acl.cpu_dat_pos * 8));
			dev->acl.cpu_dat_pos++;
		}
                
		dev->acl.pix_pos++;
		dev->acl.internal.pos_x++;
		if (dev->acl.pix_pos <= ((dev->acl.internal.pixel_depth >> 4) & 3)) {
			if (dev->acl.internal.xy_dir & 1) et4000w32_decx(1, dev);
			else                              et4000w32_incx(1, dev);
		} else {
			if (dev->acl.internal.xy_dir & 1) 
				et4000w32_incx((dev->acl.internal.pixel_depth >> 4) & 3, dev);
			else 
				et4000w32_decx((dev->acl.internal.pixel_depth >> 4) & 3, dev);
			dev->acl.pix_pos = 0;
			/*Next pixel*/
			switch (dev->acl.internal.xy_dir & 7) {
				case 0: case 1: /*Y+*/
					et4000w32_incy(dev);
					dev->acl.internal.pos_y++;
					dev->acl.internal.pos_x -= ((dev->acl.internal.pixel_depth >> 4) & 3) + 1;
					break;
				case 2: case 3: /*Y-*/
					et4000w32_decy(dev);
					dev->acl.internal.pos_y++;
					dev->acl.internal.pos_x -= ((dev->acl.internal.pixel_depth >> 4) & 3) + 1;
					break;
				case 4: case 6: /*X+*/
					et4000w32_incx(((dev->acl.internal.pixel_depth >> 4) & 3) + 1, dev);
					break;
				case 5: case 7: /*X-*/
					et4000w32_decx(((dev->acl.internal.pixel_depth >> 4) & 3) + 1, dev);
					break;
				}
			dev->acl.internal.error += dev->acl.internal.dmin;
			if (dev->acl.internal.error > dev->acl.internal.dmaj) {
				dev->acl.internal.error -= dev->acl.internal.dmaj;
				switch (dev->acl.internal.xy_dir & 7) {
					case 0: case 2: /*X+*/
						et4000w32_incx(((dev->acl.internal.pixel_depth >> 4) & 3) + 1, dev);
						dev->acl.internal.pos_x++;
						break;
					case 1: case 3: /*X-*/
						et4000w32_decx(((dev->acl.internal.pixel_depth >> 4) & 3) + 1, dev);
						dev->acl.internal.pos_x++;
						break;
					case 4: case 5: /*Y+*/
						et4000w32_incy(dev);
						dev->acl.internal.pos_y++;
						break;
					case 6: case 7: /*Y-*/
						et4000w32_decy(dev);
						dev->acl.internal.pos_y++;
						break;
				}
			}
			if (dev->acl.internal.pos_x > dev->acl.internal.count_x ||
			    dev->acl.internal.pos_y > dev->acl.internal.count_y) {
				dev->acl.status &= ~(ACL_XYST | ACL_SSO);
				return;
			}
		}
	}
    } else {
	while (count--) {
		/* if (bltout) DEBUG("%i,%i : ", dev->acl.internal.pos_x, dev->acl.internal.pos_y); */
                
			pattern = svga->vram[(dev->acl.pattern_addr + dev->acl.pattern_x) & 0x1fffff];
			source  = svga->vram[(dev->acl.source_addr  + dev->acl.source_x)  & 0x1fffff];
			/* if (bltout) DEBUG("%i %06X %06X %02X %02X  ", dev->acl.pattern_y, (dev->acl.pattern_addr + dev->acl.pattern_x) & 0x1fffff, (dev->acl.source_addr + dev->acl.source_x) & 0x1fffff, pattern, source); */

		if (cpu_input == 2) {
			source = sdat & 0xff;
			sdat >>= 8;
		}
		dest = svga->vram[dev->acl.dest_addr & 0x1fffff];
		out = 0;
		/* if (bltout) DEBUG("%06X %02X  %i %08X %08X  ", dest, dev->acl.dest_addr, mix & 1, mix, dev->acl.mix_addr); */
		if ((dev->acl.internal.ctrl_routing & 0xa) == 8) {
			mixdat = svga->vram[(dev->acl.mix_addr >> 3) & 0x1fffff] & (1 << (dev->acl.mix_addr & 7));
			/* if (bltout) DEBUG("%06X %02X  ", dev->acl.mix_addr, svga->vram[(dev->acl.mix_addr >> 3) & 0x1fffff]); */
		} else {
			mixdat = mix & 1;
			mix >>= 1; 
			mix |= 0x80000000;
		}

		rop = mixdat ? dev->acl.internal.rop_fg : dev->acl.internal.rop_bg;
		for (c = 0; c < 8; c++) {
			d = (dest & (1 << c)) ? 1 : 0;
			if (source & (1 << c))  d |= 2;
			if (pattern & (1 << c)) d |= 4;
			if (rop & (1 << d)) out |= (1 << c);
		}
		/* if (bltout) DEBUG("%06X = %02X\n", dev->acl.dest_addr & 0x1fffff, out); */
		if (!(dev->acl.internal.ctrl_routing & 0x40)) {
			svga->vram[dev->acl.dest_addr & 0x1fffff] = out;
			svga->changedvram[(dev->acl.dest_addr & 0x1fffff) >> 12] = changeframecount;
		} else {
			dev->acl.cpu_dat |= ((uint64_t)out << (dev->acl.cpu_dat_pos * 8));
			dev->acl.cpu_dat_pos++;
		}

		if (dev->acl.internal.xy_dir & 1) et4000w32_decx(1, dev);
		else                              et4000w32_incx(1, dev);

		dev->acl.internal.pos_x++;
		if (dev->acl.internal.pos_x > dev->acl.internal.count_x) {
			if (dev->acl.internal.xy_dir & 2) {
				et4000w32_decy(dev);
				dev->acl.mix_back  = dev->acl.mix_addr  = dev->acl.mix_back  - (dev->acl.internal.mix_off  + 1);
				dev->acl.dest_back = dev->acl.dest_addr = dev->acl.dest_back - (dev->acl.internal.dest_off + 1);
			} else {
				et4000w32_incy(dev);
				dev->acl.mix_back  = dev->acl.mix_addr  = dev->acl.mix_back  + dev->acl.internal.mix_off  + 1;
				dev->acl.dest_back = dev->acl.dest_addr = dev->acl.dest_back + dev->acl.internal.dest_off + 1;
			}

			dev->acl.pattern_x = dev->acl.pattern_x_back;
			dev->acl.source_x  = dev->acl.source_x_back;

			dev->acl.internal.pos_y++;
			dev->acl.internal.pos_x = 0;
			if (dev->acl.internal.pos_y > dev->acl.internal.count_y) {
				dev->acl.status &= ~(ACL_XYST | ACL_SSO);
				return;
			}
			if (cpu_input) return;
			if (dev->acl.internal.ctrl_routing & 0x40) {
				if (dev->acl.cpu_dat_pos & 3) 
					dev->acl.cpu_dat_pos += 4 - (dev->acl.cpu_dat_pos & 3);
			return;
			}
		}
	}
    }
}


static void et4000w32p_hwcursor_draw(svga_t *svga, int displine)
{
    int x, offset;
    uint8_t dat;
    int y_add, x_add;

    offset = svga->hwcursor_latch.xoff;

    y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
    x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;

    for (x = 0; x < 64 - svga->hwcursor_latch.xoff; x += 4) {
	dat = svga->vram[svga->hwcursor_latch.addr + (offset >> 2)];
	if (!(dat & 2))          screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 32].val  = (dat & 1) ? 0xFFFFFF : 0;
	else if ((dat & 3) == 3) screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 32].val ^= 0xFFFFFF;
	dat >>= 2;
	if (!(dat & 2))          screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 33 + x_add].val  = (dat & 1) ? 0xFFFFFF : 0;
	else if ((dat & 3) == 3) screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 33 + x_add].val ^= 0xFFFFFF;
	dat >>= 2;
	if (!(dat & 2))          screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 34].val  = (dat & 1) ? 0xFFFFFF : 0;
	else if ((dat & 3) == 3) screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 34].val ^= 0xFFFFFF;
	dat >>= 2;
	if (!(dat & 2))          screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 35].val  = (dat & 1) ? 0xFFFFFF : 0;
	else if ((dat & 3) == 3) screen->line[displine + y_add][svga->hwcursor_latch.x + x_add + x + 35].val ^= 0xFFFFFF;
	dat >>= 2;
	offset += 4;
    }
    svga->hwcursor_latch.addr += 16;
}

static void et4000w32p_io_remove(et4000w32p_t *et4000)
{

    io_removehandler(0x03c0, 0x0020, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);

    io_removehandler(0x210A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x211A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x212A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x213A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x214A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x215A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x216A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_removehandler(0x217A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
}

static void et4000w32p_io_set(et4000w32p_t *et4000)
{
    et4000w32p_io_remove(et4000);
    
    io_sethandler(0x03c0, 0x0020, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);

    io_sethandler(0x210A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x211A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x212A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x213A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x214A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x215A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x216A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
    io_sethandler(0x217A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, et4000);
}

static uint8_t et4000w32p_pci_read(int func, int addr, priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;

    addr &= 0xff;

    switch (addr) {
	case 0x00: return 0x0c; /*Tseng Labs*/
	case 0x01: return 0x10;
        
	case 0x02: return 0x06; /*ET4000W32p Rev D*/
	case 0x03: return 0x32;
        
	case PCI_REG_COMMAND:
	return dev->pci_regs[PCI_REG_COMMAND] | 0x80; /*Respond to IO and memory accesses*/

	case 0x07: return 1 << 1; /*Medium DEVSEL timing*/
        
	case 0x08: return 0; /*Revision ID*/
	case 0x09: return 0; /*Programming interface*/

	case 0x0a: return 0x00; /*Supports VGA interface, XGA compatible*/
	case 0x0b: return is_pentium ? 0x03 : 0x00;	/* This has to be done in order to make this card work with the two 486 PCI machines. */
        
	case 0x10: return 0x00; /*Linear frame buffer address*/
	case 0x11: return 0x00;
	case 0x12: return 0x00;
	case 0x13: return (dev->linearbase >> 24);

	case 0x30: return dev->pci_regs[0x30] & 0x01; /*BIOS ROM address*/
	case 0x31: return 0x00;
	case 0x32: return 0x00;
	case 0x33: return (dev->pci_regs[0x33]) & 0xf0;

    }
    return 0;
}

static void et4000w32p_pci_write(int func, int addr, uint8_t val, priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;
    svga_t *svga = &dev->svga;

    addr &= 0xff;

     switch (addr) {
	case PCI_REG_COMMAND:
		dev->pci_regs[PCI_REG_COMMAND] = (val & 0x23) | 0x80;
		if (val & PCI_COMMAND_IO)
			et4000w32p_io_set(dev);
		else
			et4000w32p_io_remove(dev);
		et4000w32p_recalcmapping(dev);
		break;

	case 0x13: 
		dev->linearbase &= 0x00c00000; 
		dev->linearbase = (dev->pci_regs[0x13] << 24);
		svga->crtc[0x30] &= 3;
		svga->crtc[0x30] = ((dev->linearbase & 0x3f000000) >> 22);
		et4000w32p_recalcmapping(dev); 
		break;

	case 0x30: case 0x31: case 0x32: case 0x33:
		dev->pci_regs[addr] = val;
		dev->pci_regs[0x30] = 1;
		dev->pci_regs[0x31] = 0;
		dev->pci_regs[0x32] = 0;
		dev->pci_regs[0x33] &= 0xf0;
		if (dev->pci_regs[0x30] & 0x01) {
			uint32_t a = (dev->pci_regs[0x33] << 24);
			if (!a) {
				a = 0xC0000;
			}
			/* DEBUG("ET4000 bios_rom enabled at %08x\n", a); */
			mem_map_set_addr(&dev->bios_rom.mapping, a, 0x8000);
		} else {
			/* DEBUG("ET4000 bios_rom disabled\n"); */
			mem_map_disable(&dev->bios_rom.mapping);
		}
		return;
    }
}


static priv_t
et4000w32p_init(const device_t *info, UNUSED(void *parent))
{
    int vram_size;
    et4000w32p_t *dev = (et4000w32p_t *)mem_alloc(sizeof(et4000w32p_t));
    memset(dev, 0, sizeof(et4000w32p_t));
    dev->type = info->local;
    dev->pci = !!(info->flags & DEVICE_PCI);

    vram_size = device_get_config_int("memory");

    dev->interleaved = (vram_size == 2) ? 1 : 0;

    svga_init(&dev->svga, (priv_t)dev, vram_size << 20,
              et4000w32p_recalctimings,
              et4000w32p_in, et4000w32p_out,
              et4000w32p_hwcursor_draw,
              NULL);

    dev->svga.hwcursor.ysize = 64;

    dev->svga.ramdac = device_add(&stg_ramdac_device);

    dev->vram_mask = (vram_size << 20) - 1;

    switch(dev->type) {
	case ET4000W32_CARDEX:
		dev->svga.clock_gen = dev->svga.ramdac;
		dev->svga.getclock = stg_getclock;
		break;

	case ET4000W32_DIAMOND:
		dev->svga.clock_gen = device_add(&icd2061_device);
		dev->svga.getclock = icd2061_getclock;
		break;
    }

    rom_init(&dev->bios_rom, info->path,
	     0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    if (info->flags & DEVICE_PCI)
	mem_map_disable(&dev->bios_rom.mapping);

    mem_map_add(&dev->linear_mapping, 0, 0,
		svga_read_linear, svga_readw_linear, svga_readl_linear,
		svga_write_linear, svga_writew_linear, svga_writel_linear,
		NULL, 0, &dev->svga);

    mem_map_add(&dev->mmu_mapping, 0, 0,
		et4000w32p_mmu_read, NULL, NULL,
		et4000w32p_mmu_write, NULL, NULL, NULL, 0, dev);

    et4000w32p_io_set(dev);

    if (info->flags & DEVICE_PCI)
        pci_add_card(PCI_ADD_VIDEO,
		     et4000w32p_pci_read, et4000w32p_pci_write, (priv_t)dev);

    /* Hardwired bits: 00000000 1xx0x0xx */
    /* R/W bits:                 xx xxxx */
    /* PCem bits:                    111 */
    dev->pci_regs[0x04] = 0x83;

    dev->pci_regs[0x10] = 0x00;
    dev->pci_regs[0x11] = 0x00;
    dev->pci_regs[0x12] = 0xff;
    dev->pci_regs[0x13] = 0xff;

    dev->pci_regs[0x30] = 0x00;
    dev->pci_regs[0x31] = 0x00;
    dev->pci_regs[0x32] = 0x00;
    dev->pci_regs[0x33] = 0xf0;

    dev->wake_fifo_thread = thread_create_event();
    dev->fifo_not_full_event = thread_create_event();
    dev->fifo_thread = thread_create(fifo_thread, dev);

    dev->svga.packed_chain4 = 1;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


static void
et4000w32p_close(priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;

    svga_close(&dev->svga);

    thread_kill(dev->fifo_thread);
    thread_destroy_event(dev->wake_fifo_thread);
    thread_destroy_event(dev->fifo_not_full_event);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;

    svga_recalctimings(&dev->svga);
}


static void
force_redraw(priv_t priv)
{
    et4000w32p_t *dev = (et4000w32p_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static const device_config_t et4000w32p_config[] = {
    {
	"memory", "Memory size", CONFIG_SELECTION, "", 2,
	{
		{
			"1 MB", 1
		},
		{
			"2 MB", 2
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};

static const video_timings_t et4000w32p_pci_timing = {VID_BUS,4,4,4,10,10,10};
static const video_timings_t et4000w32p_vlb_timing = {VID_BUS,4,4,4,10,10,10};


const device_t et4000w32p_cardex_vlb_device = {
    "Tseng Labs ET4000/w32p (Cardex)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    ET4000W32_CARDEX,
    BIOS_ROM_PATH_CARDEX,
    et4000w32p_init, et4000w32p_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &et4000w32p_vlb_timing,
    et4000w32p_config
};

const device_t et4000w32p_cardex_pci_device = {
    "Tseng Labs ET4000/w32p (Cardex)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    ET4000W32_CARDEX,
    BIOS_ROM_PATH_CARDEX,
    et4000w32p_init, et4000w32p_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &et4000w32p_pci_timing,
    et4000w32p_config
};

const device_t et4000w32p_vlb_device = {
    "Tseng Labs ET4000/w32p (Diamond)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    ET4000W32_DIAMOND,
    BIOS_ROM_PATH_DIAMOND,
    et4000w32p_init, et4000w32p_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &et4000w32p_vlb_timing,
    et4000w32p_config
};

const device_t et4000w32p_pci_device = {
    "Tseng Labs ET4000/w32p (Diamond)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    ET4000W32_DIAMOND,
    BIOS_ROM_PATH_DIAMOND,
    et4000w32p_init, et4000w32p_close, NULL,
    NULL,
    speed_changed,
    force_redraw,
    &et4000w32p_pci_timing,
    et4000w32p_config
};
