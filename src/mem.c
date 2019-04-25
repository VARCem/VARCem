/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Memory handling and MMU.
 *
 * **NOTES**	The cpu-specific MMU code should be moved to cpu/mmu.c.
 *		The Port92 stuff should be moved to devices/system/memctl.c
 *		 as a standard device.
 *
 * Version:	@(#)mem.c	1.0.30	2019/04/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2008-2019 Sarah Walker.
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
#include "emu.h"
#include "cpu/cpu.h"
#include "cpu/x86_ops.h"
#include "cpu/x86.h"
#include "machines/machine.h"
#include "config.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#ifdef USE_DYNAREC
# include "cpu/codegen.h"
#else
# define PAGE_MASK_INDEX_MASK	3
# define PAGE_MASK_INDEX_SHIFT	10
# define PAGE_MASK_MASK		63
# define PAGE_MASK_SHIFT	4
#endif


uint8_t		*ram;				/* the virtual RAM */
uint32_t	rammask;

mem_map_t	base_mapping,
		ram_low_mapping,		/* 0..640K mapping */
		ram_mid_mapping,
		ram_remapped_mapping,		/* 640..1024K mapping */
		ram_high_mapping;		/* 1024K+ mapping */

page_t		*pages,				/* RAM page table */
		**page_lookup;			/* pagetable lookup */
uint32_t	pages_sz = 0;			/* #pages in table */

uint32_t	pccache;
uint8_t		*pccache2;

int		readlnext;
int		readlookup[256],
		readlookupp[256];
uintptr_t	*readlookup2;
int		writelnext;
int		writelookup[256],
		writelookupp[256];
uintptr_t	*writelookup2;

uint32_t	mem_logical_addr;

int		cachesize = 256;

uint32_t	ram_mapped_addr[64];

uint32_t	get_phys_virt,
		get_phys_phys;

int		mem_a20_key = 0,
		mem_a20_alt = 0,
		mem_a20_state = 0;

int		mmuflush = 0;
int		mmu_perm = 4;


static mem_map_t	*read_mapping[0x40000];
static mem_map_t	*write_mapping[0x40000];
static uint8_t		*_mem_exec[0x40000];
static int		_mem_state[0x40000];

static uint8_t		ff_pccache[4] = { 0xff, 0xff, 0xff, 0xff };

static uint8_t		port_92_reg = 0,
			port_92_mask = 0;


void
resetreadlookup(void)
{
    int c;

    /* Initialize the page lookup table. */
    memset(page_lookup, 0x00, (1<<20)*sizeof(page_t *));

    /* Initialize the tables for lower (<= 1024K) RAM. */
    for (c = 0; c < 256; c++) {
	readlookup[c] = 0xffffffff;
	writelookup[c] = 0xffffffff;
    }

    /* Initialize the tables for high (> 1024K) RAM. */
    memset(readlookup2, 0xff, (1<<20)*sizeof(uintptr_t));
    memset(writelookup2, 0xff, (1<<20)*sizeof(uintptr_t));

    readlnext = 0;
    writelnext = 0;
    pccache = 0xffffffff;
}


void
flushmmucache(void)
{
    int c;

    for (c = 0; c < 256; c++) {
	if (readlookup[c] != (int)0xffffffff) {
		readlookup2[readlookup[c]] = -1;
		readlookup[c] = 0xffffffff;
	}
	if (writelookup[c] != (int)0xffffffff) {
		page_lookup[writelookup[c]] = NULL;
		writelookup2[writelookup[c]] = -1;
		writelookup[c] = 0xffffffff;
	}
    }
    mmuflush++;

    pccache = (uint32_t)0xffffffff;
#ifdef _MSC_VER
    pccache2 = (uint8_t *)0xffffffffffffffff;
#else
    pccache2 = (uint8_t *)0xffffffff;
#endif

#ifdef USE_DYNAREC
    codegen_flush();
#endif
}


void
flushmmucache_nopc(void)
{
    int c;

    for (c = 0; c < 256; c++) {
	if (readlookup[c] != (int)0xffffffff) {
		readlookup2[readlookup[c]] = -1;
		readlookup[c] = 0xffffffff;
	}
	if (writelookup[c] != (int)0xffffffff) {
		page_lookup[writelookup[c]] = NULL;
		writelookup2[writelookup[c]] = -1;
		writelookup[c] = 0xffffffff;
	}
    }
}


void
flushmmucache_cr3(void)
{
    int c;

    for (c = 0; c < 256; c++) {
	if (readlookup[c] != (int)0xffffffff) {
		readlookup2[readlookup[c]] = -1;
		readlookup[c] = 0xffffffff;
	}
	if (writelookup[c] != (int)0xffffffff) {
		page_lookup[writelookup[c]] = NULL;
		writelookup2[writelookup[c]] = -1;
		writelookup[c] = 0xffffffff;
	}
    }
}


void
mem_flush_write_page(uint32_t addr, uint32_t virt)
{
    page_t *page_target = &pages[addr >> 12];
    int c;

    for (c = 0; c < 256; c++) {
	if (writelookup[c] != (int)0xffffffff) {
		uintptr_t target = (uintptr_t)&ram[(uintptr_t)(addr & ~0xfff) - (virt & ~0xfff)];

		if (writelookup2[writelookup[c]] == target || page_lookup[writelookup[c]] == page_target) {
			writelookup2[writelookup[c]] = -1;
			page_lookup[writelookup[c]] = NULL;
			writelookup[c] = 0xffffffff;
		}
	}
    }
}


#define mmutranslate_read(addr) mmutranslatereal(addr,0)
#define mmutranslate_write(addr) mmutranslatereal(addr,1)
#define rammap(x)	((uint32_t *)(_mem_exec[(x) >> 14]))[((x) >> 2) & 0xfff]

uint32_t
mmutranslatereal(uint32_t addr, int rw)
{
    uint32_t temp,temp2,temp3;
    uint32_t addr2;

    if (cpu_state.abrt) return -1;

    addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
    temp = temp2 = rammap(addr2);
    if (! (temp&1)) {
	cr2 = addr;
	temp &= 1;
	if (CPL == 3) temp |= 4;
	if (rw) temp |= 2;
	cpu_state.abrt = ABRT_PF;
	abrt_error = temp;
	return -1;
    }

    if ((temp & 0x80) && (cr4 & CR4_PSE)) {
	/*4MB page*/
	if ((CPL == 3 && !(temp & 4) && !cpl_override) || (rw && !(temp & 2) && ((CPL == 3 && !cpl_override) || cr0 & WP_FLAG))) {
		cr2 = addr;
		temp &= 1;
		if (CPL == 3)
			temp |= 4;
		if (rw)
			temp |= 2;
		cpu_state.abrt = ABRT_PF;
		abrt_error = temp;

		return -1;
	}

	mmu_perm = temp & 4;
	rammap(addr2) |= 0x20;

	return (temp & ~0x3fffff) + (addr & 0x3fffff);
    }

    temp = rammap((temp & ~0xfff) + ((addr >> 10) & 0xffc));
    temp3 = temp & temp2;
    if (!(temp&1) || (CPL==3 && !(temp3&4) && !cpl_override) || (rw && !(temp3&2) && ((CPL == 3 && !cpl_override) || cr0&WP_FLAG))) {
	cr2 = addr;
	temp &= 1;
	if (CPL == 3) temp |= 4;
	if (rw) temp |= 2;
	cpu_state.abrt = ABRT_PF;
	abrt_error = temp;
	return -1;
    }

    mmu_perm = temp & 4;
    rammap(addr2) |= 0x20;
    rammap((temp2 & ~0xfff) + ((addr >> 10) & 0xffc)) |= (rw?0x60:0x20);

    return (temp&~0xfff)+(addr&0xfff);
}


uint32_t
mmutranslate_noabrt(uint32_t addr, int rw)
{
    uint32_t temp,temp2,temp3;
    uint32_t addr2;

    if (cpu_state.abrt) 
	return -1;

    addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
    temp = temp2 = rammap(addr2);

    if (! (temp & 1))
	return -1;

    if ((temp & 0x80) && (cr4 & CR4_PSE)) {
	/*4MB page*/
	if ((CPL == 3 && !(temp & 4) && !cpl_override) || (rw && !(temp & 2) && (CPL == 3 || cr0 & WP_FLAG)))
		return -1;

	return (temp & ~0x3fffff) + (addr & 0x3fffff);
    }

    temp = rammap((temp & ~0xfff) + ((addr >> 10) & 0xffc));
    temp3 = temp & temp2;

    if (!(temp&1) || (CPL==3 && !(temp3&4) && !cpl_override) || (rw && !(temp3&2) && (CPL==3 || cr0&WP_FLAG)))
	return -1;

    return (temp & ~0xfff) + (addr & 0xfff);
}


void
mmu_invalidate(uint32_t addr)
{
    flushmmucache_cr3();
}


uint8_t
mem_addr_range_match(uint32_t addr, uint32_t start, uint32_t len)
{
    if (addr < start)
	return 0;
      else if (addr >= (start + len))
	return 0;
      else
	return 1;
}


uint32_t
mem_addr_translate(uint32_t addr, uint32_t chunk_start, uint32_t len)
{
    uint32_t mask = len - 1;

    return chunk_start + (addr & mask);
}


void
addreadlookup(uint32_t virt, uint32_t phys)
{
    if (virt == 0xffffffff) return;

    if (readlookup2[virt>>12] != (uintptr_t)-1) return;

    if (readlookup[readlnext] != (int)0xffffffff)
	readlookup2[readlookup[readlnext]] = -1;

    readlookup2[virt>>12] = (uintptr_t)&ram[(uintptr_t)(phys & ~0xFFF) - (uintptr_t)(virt & ~0xfff)];

    readlookupp[readlnext] = mmu_perm;
    readlookup[readlnext++] = virt >> 12;
    readlnext &= (cachesize-1);

    cycles -= 9;
}


void
addwritelookup(uint32_t virt, uint32_t phys)
{
    if (virt == 0xffffffff) return;

    if (page_lookup[virt >> 12]) return;

    if (writelookup[writelnext] != -1) {
	page_lookup[writelookup[writelnext]] = NULL;
	writelookup2[writelookup[writelnext]] = -1;
    }

#ifdef USE_DYNAREC
    if (pages[phys >> 12].block[0] || pages[phys >> 12].block[1] || pages[phys >> 12].block[2] || pages[phys >> 12].block[3] || (phys & ~0xfff) == recomp_page)
#else
    if (pages[phys >> 12].block[0] || pages[phys >> 12].block[1] || pages[phys >> 12].block[2] || pages[phys >> 12].block[3])
#endif
	page_lookup[virt >> 12] = &pages[phys >> 12];
      else
	writelookup2[virt>>12] = (uintptr_t)&ram[(uintptr_t)(phys & ~0xFFF) - (uintptr_t)(virt & ~0xfff)];

    writelookupp[writelnext] = mmu_perm;
    writelookup[writelnext++] = virt >> 12;
    writelnext &= (cachesize - 1);

    cycles -= 9;
}


uint8_t *
getpccache(uint32_t a)
{
    uint32_t a2;

    a2 = a;

    if (a2 < 0x100000 && ram_mapped_addr[a2 >> 14]) {
	a = (ram_mapped_addr[a2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? a2 : (ram_mapped_addr[a2 >> 14] & ~0x3FFF) + (a2 & 0x3FFF);
	return &ram[(uintptr_t)(a & 0xFFFFF000) - (uintptr_t)(a2 & ~0xFFF)];
    }

    a2 = a;

    if (cr0 >> 31) {
	a = mmutranslate_read(a);

	if (a == 0xffffffff) return ram;
    }
    a &= rammask;

    if (_mem_exec[a >> 14]) {
	if (is286) {
		if (read_mapping[a >> 14]->flags & MEM_MAPPING_ROM)
			cpu_prefetch_cycles = cpu_rom_prefetch_cycles;
		else
			cpu_prefetch_cycles = cpu_mem_prefetch_cycles;
	}
	
	return &_mem_exec[a >> 14][(uintptr_t)(a & 0x3000) - (uintptr_t)(a2 & ~0xfff)];
    }

    ERRLOG("Bad getpccache %08X\n", a);

    return (uint8_t *)&ff_pccache;
}


uint8_t
readmembl(uint32_t addr)
{
    mem_map_t *map;

    mem_logical_addr = addr;

    if (addr < 0x100000 && ram_mapped_addr[addr >> 14]) {
	addr = (ram_mapped_addr[addr >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr : (ram_mapped_addr[addr >> 14] & ~0x3fff) + (addr & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size)) return ram[addr];
	return 0xff;
    }

    if (cr0 >> 31) {
	addr = mmutranslate_read(addr);
	if (addr == 0xffffffff) return 0xff;
    }
    addr &= rammask;

    map = read_mapping[addr >> 14];
    if (map && map->read_b)
	return map->read_b(addr, map->p);

    return 0xff;
}


void
writemembl(uint32_t addr, uint8_t val)
{
    mem_map_t *map;

    mem_logical_addr = addr;

    if (addr < 0x100000 && ram_mapped_addr[addr >> 14]) {
	addr = (ram_mapped_addr[addr >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr : (ram_mapped_addr[addr >> 14] & ~0x3fff) + (addr & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		ram[addr] = val;
	return;
    }

    if (page_lookup[addr>>12]) {
	page_lookup[addr>>12]->write_b(addr, val, page_lookup[addr>>12]);

	return;
    }

    if (cr0 >> 31) {
	addr = mmutranslate_write(addr);
	if (addr == 0xffffffff) return;
    }
    addr &= rammask;

    map = write_mapping[addr >> 14];
    if (map && map->write_b)
	map->write_b(addr, val, map->p);
}


uint8_t
readmemb386l(uint32_t seg, uint32_t addr)
{
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);

	return 0xff;
    }

    mem_logical_addr = addr = addr + seg;
    if (addr < 0x100000 && ram_mapped_addr[addr >> 14]) {
	addr = (ram_mapped_addr[addr >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr : (ram_mapped_addr[addr >> 14] & ~0x3fff) + (addr & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		return ram[addr];
	return 0xff;
    }

    if (cr0 >> 31) {
	addr = mmutranslate_read(addr);
	if (addr == 0xffffffff)
		return 0xff;
    }

    addr &= rammask;

    map = read_mapping[addr >> 14];
    if (map && map->read_b)
	return map->read_b(addr, map->p);

    return 0xff;
}


void
writememb386l(uint32_t seg, uint32_t addr, uint8_t val)
{
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return;
    }

    mem_logical_addr = addr = addr + seg;
    if (addr < 0x100000 && ram_mapped_addr[addr >> 14]) {
	addr = (ram_mapped_addr[addr >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr : (ram_mapped_addr[addr >> 14] & ~0x3fff) + (addr & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		ram[addr] = val;
	return;
    }

    if (page_lookup[addr>>12]) {
	page_lookup[addr>>12]->write_b(addr, val, page_lookup[addr>>12]);

	return;
    }

    if (cr0 >> 31) {
	addr = mmutranslate_write(addr);
	if (addr == 0xffffffff) return;
    }

    addr &= rammask;

    map = write_mapping[addr >> 14];
    if (map && map->write_b)
	map->write_b(addr, val, map->p);
}


uint16_t
readmemwl(uint32_t seg, uint32_t addr)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return 0xffff;
    }

    if (addr2 & 1) {
	if (!cpu_cyrix_alignment || (addr2 & 7) == 7)
		cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xffe) {
		if (cr0 >> 31) {
			if (mmutranslate_read(addr2)   == 0xffffffff) return 0xffff;
			if (mmutranslate_read(addr2+1) == 0xffffffff) return 0xffff;
		}
		if (is386) return readmemb386l(seg,addr)|((uint16_t)(readmemb386l(seg,addr+1))<<8);
		else       return readmembl(seg+addr)|((uint16_t)(readmembl(seg+addr+1))<<8);
	}
	else if (readlookup2[addr2 >> 12] != (uintptr_t)-1)
		return *(uint16_t *)(readlookup2[addr2 >> 12] + addr2);
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		return *((uint16_t *)&ram[addr]);
	return 0xffff;
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_read(addr2);
	if (addr2 == 0xffffffff)
		return 0xffff;
    }

    addr2 &= rammask;

    map = read_mapping[addr2 >> 14];

    if (map && map->read_w)
	return map->read_w(addr2, map->p);

    if (map && map->read_b) {
	if (AT)
		return map->read_b(addr2, map->p) | ((uint16_t)(map->read_b(addr2 + 1, map->p)) << 8);
	else
		return map->read_b(addr2, map->p) | ((uint16_t)(map->read_b(seg + ((addr + 1) & 0xffff), map->p)) << 8);
    }

    return 0xffff;
}


void
writememwl(uint32_t seg, uint32_t addr, uint16_t val)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return;
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		*((uint16_t *)&ram[addr]) = val;
	return;
    }

    if (addr2 & 1) {
	if (!cpu_cyrix_alignment || (addr2 & 7) == 7)
		cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xffe) {
		if (cr0 >> 31) {
			if (mmutranslate_write(addr2)   == 0xffffffff) return;
			if (mmutranslate_write(addr2+1) == 0xffffffff) return;
		}
		if (is386) {
			writememb386l(seg,addr,(uint8_t)(val&0xff));
			writememb386l(seg,addr+1,(uint8_t)(val>>8));
		} else {
			writemembl(seg+addr,(uint8_t)(val&0xff));
			writemembl(seg+addr+1,(uint8_t)(val>>8));
		}
		return;
	} else if (writelookup2[addr2 >> 12] != (uintptr_t)-1) {
		*(uint16_t *)(writelookup2[addr2 >> 12] + addr2) = val;
		return;
	}
    }

    if (page_lookup[addr2 >> 12]) {
	page_lookup[addr2 >> 12]->write_w(addr2, val, page_lookup[addr2 >> 12]);
	return;
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_write(addr2);
	if (addr2 == 0xffffffff) return;
    }

    addr2 &= rammask;

    map = write_mapping[addr2 >> 14];
    if (map && map->write_w) {
	map->write_w(addr2, val, map->p);
	return;
    }

    if (map && map->write_b) {
	map->write_b(addr2, val, map->p);
	map->write_b(addr2 + 1, val >> 8, map->p);
    }
}


uint32_t
readmemll(uint32_t seg, uint32_t addr)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return -1;
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		return *((uint32_t *)&ram[addr]);
	return 0xffffffff;
    }

    if (addr2 & 3) {
	if (!cpu_cyrix_alignment || (addr2 & 7) > 4)
		cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xffc) {
		if (cr0 >> 31) {
			if (mmutranslate_read(addr2)   == 0xffffffff) return 0xffffffff;
			if (mmutranslate_read(addr2+3) == 0xffffffff) return 0xffffffff;
		}
		return readmemwl(seg,addr)|(readmemwl(seg,addr+2)<<16);
	} else if (readlookup2[addr2 >> 12] != (uintptr_t)-1)
		return *(uint32_t *)(readlookup2[addr2 >> 12] + addr2);
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_read(addr2);
	if (addr2 == 0xffffffff)
		return 0xffffffff;
    }

    addr2 &= rammask;

    map = read_mapping[addr2 >> 14];

    if (map && map->read_l)
	return map->read_l(addr2, map->p);

    if (map && map->read_w)
	return map->read_w(addr2, map->p) |
	       ((uint32_t) (map->read_w(addr2 + 2, map->p)) << 16);

    if (map && map->read_b)
	return map->read_b(addr2, map->p) |
	       ((uint32_t) (map->read_b(addr2 + 1, map->p)) << 8) |
	       ((uint32_t) (map->read_b(addr2 + 2, map->p)) << 16) |
	       ((uint32_t) (map->read_b(addr2 + 3, map->p)) << 24);

    return 0xffffffff;
}


void
writememll(uint32_t seg, uint32_t addr, uint32_t val)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return;
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		*((uint32_t *)&ram[addr]) = val;
	return;
    }

    if (addr2 & 3) {
	if (!cpu_cyrix_alignment || (addr2 & 7) > 4)
		cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xffc) {
		if (cr0 >> 31) {
			if (mmutranslate_write(addr2)   == 0xffffffff) return;
			if (mmutranslate_write(addr2+3) == 0xffffffff) return;
		}
		writememwl(seg,addr,val);
		writememwl(seg,addr+2,val>>16);
		return;
	} else if (writelookup2[addr2 >> 12] != (uintptr_t)-1) {
		*(uint32_t *)(writelookup2[addr2 >> 12] + addr2) = val;
		return;
	}
    }

    if (page_lookup[addr2 >> 12]) {
	page_lookup[addr2 >> 12]->write_l(addr2, val, page_lookup[addr2 >> 12]);
	return;
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_write(addr2);
	if (addr2 == 0xffffffff) return;
    }

    addr2 &= rammask;

    map = write_mapping[addr2 >> 14];

    if (map && map->write_l) {
	map->write_l(addr2, val, map->p);
	return;
    }

    if (map && map->write_w) {
	map->write_w(addr2,     val,       map->p);
	map->write_w(addr2 + 2, val >> 16, map->p);
	return;
    }

    if (map && map->write_b) {
	map->write_b(addr2,     val,       map->p);
	map->write_b(addr2 + 1, val >> 8,  map->p);
	map->write_b(addr2 + 2, val >> 16, map->p);
	map->write_b(addr2 + 3, val >> 24, map->p);
    }
}


uint64_t
readmemql(uint32_t seg, uint32_t addr)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return -1;
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		return *((uint64_t *)&ram[addr]);
	return -1;
    }

    if (addr2 & 7) {
	cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xff8) {
		if (cr0 >> 31) {
			if (mmutranslate_read(addr2)   == 0xffffffff) return 0xffffffff;
			if (mmutranslate_read(addr2+7) == 0xffffffff) return 0xffffffff;
		}
		return readmemll(seg,addr)|((uint64_t)readmemll(seg,addr+4)<<32);
	} else if (readlookup2[addr2 >> 12] != (uintptr_t)-1)
		return *(uint64_t *)(readlookup2[addr2 >> 12] + addr2);
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_read(addr2);
	if (addr2 == 0xffffffff)
		return -1;
    }

    addr2 &= rammask;

    map = read_mapping[addr2 >> 14];
    if (map && map->read_l)
	return map->read_l(addr2, map->p) | ((uint64_t)map->read_l(addr2 + 4, map->p) << 32);

    return readmemll(seg,addr) | ((uint64_t)readmemll(seg,addr+4)<<32);
}


void
writememql(uint32_t seg, uint32_t addr, uint64_t val)
{
    uint32_t addr2 = mem_logical_addr = seg + addr;
    mem_map_t *map;

    if (seg == (uint32_t)-1) {
	x86gpf("NULL segment", 0);
	return;
    }

    if (addr2 < 0x100000 && ram_mapped_addr[addr2 >> 14]) {
	addr = (ram_mapped_addr[addr2 >> 14] & MEM_MAP_TO_SHADOW_RAM_MASK) ? addr2 : (ram_mapped_addr[addr2 >> 14] & ~0x3fff) + (addr2 & 0x3fff);
	if (addr < (uint32_t)(1024UL * mem_size))
		*((uint64_t *)&ram[addr]) = val;
	return;
    }

    if (addr2 & 7) {
	cycles -= timing_misaligned;
	if ((addr2 & 0xfff) > 0xff8) {
		if (cr0 >> 31) {
			if (mmutranslate_write(addr2)   == 0xffffffff) return;
			if (mmutranslate_write(addr2+7) == 0xffffffff) return;
		}
		writememll(seg, addr, (uint32_t)(val&0xffffffff));
		writememll(seg, addr+4, (uint32_t)(val>>32));
		return;
	} else if (writelookup2[addr2 >> 12] != (uintptr_t)-1) {
		*(uint64_t *)(writelookup2[addr2 >> 12] + addr2) = val;
		return;
	}
    }

    if (page_lookup[addr2>>12]) {
	page_lookup[addr2>>12]->write_l(addr2, (uint32_t)(val&0xffffffff), page_lookup[addr2>>12]);
	page_lookup[addr2>>12]->write_l(addr2 + 4, (uint32_t)(val>>32), page_lookup[addr2>>12]);
	return;
    }

    if (cr0 >> 31) {
	addr2 = mmutranslate_write(addr2);
	if (addr2 == 0xffffffff) return;
    }

    addr2 &= rammask;

    map = write_mapping[addr2 >> 14];

    if (map && map->write_l) {
	map->write_l(addr2,   val,       map->p);
	map->write_l(addr2+4, val >> 32, map->p);
	return;
    }

    if (map && map->write_w) {
	map->write_w(addr2,     val,       map->p);
	map->write_w(addr2 + 2, val >> 16, map->p);
	map->write_w(addr2 + 4, val >> 32, map->p);
	map->write_w(addr2 + 6, val >> 48, map->p);
	return;
    }

    if (map && map->write_b) {
	map->write_b(addr2,     val,       map->p);
	map->write_b(addr2 + 1, val >> 8,  map->p);
	map->write_b(addr2 + 2, val >> 16, map->p);
	map->write_b(addr2 + 3, val >> 24, map->p);
	map->write_b(addr2 + 4, val >> 32, map->p);
	map->write_b(addr2 + 5, val >> 40, map->p);
	map->write_b(addr2 + 6, val >> 48, map->p);
	map->write_b(addr2 + 7, val >> 56, map->p);
    }
}


uint8_t
mem_readb_phys(uint32_t addr)
{
    mem_map_t *map = read_mapping[addr >> 14];

    if (_mem_exec[addr >> 14])
	return _mem_exec[addr >> 14][addr & 0x3fff];

    if (map && map->read_b)
       	return map->read_b(addr, map->p);

    return 0xff;
}


uint16_t
mem_readw_phys(uint32_t addr)
{
    mem_map_t *map = read_mapping[addr >> 14];
    uint16_t temp;

    if (_mem_exec[addr >> 14])
	return ((uint16_t *) _mem_exec[addr >> 14])[(addr >> 1) & 0x1fff];

    if (map && map->read_w)
       	return map->read_w(addr, map->p);

    temp = mem_readb_phys(addr + 1) << 8;
    temp |=  mem_readb_phys(addr);

    return temp;
}


void
mem_writeb_phys(uint32_t addr, uint8_t val)
{
    mem_map_t *map = write_mapping[addr >> 14];

    if (_mem_exec[addr >> 14])
	_mem_exec[addr >> 14][addr & 0x3fff] = val;
    else if (map && map->write_b)
       	map->write_b(addr, val, map->p);
}


uint8_t
mem_read_ram(uint32_t addr, void *priv)
{
    addreadlookup(mem_logical_addr, addr);

    return ram[addr];
}


uint16_t
mem_read_ramw(uint32_t addr, void *priv)
{
    addreadlookup(mem_logical_addr, addr);

    return *(uint16_t *)&ram[addr];
}


uint32_t
mem_read_raml(uint32_t addr, void *priv)
{
    addreadlookup(mem_logical_addr, addr);

    return *(uint32_t *)&ram[addr];
}


void
mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *p)
{
#ifdef USE_DYNAREC
    if (val != p->mem[addr & 0xfff] || codegen_in_recompile) {
#else
    if (val != p->mem[addr & 0xfff]) {
#endif
	uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
	p->dirty_mask[(addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] |= mask;
	p->mem[addr & 0xfff] = val;
    }
}


void
mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *p)
{
#ifdef USE_DYNAREC
    if (val != *(uint16_t *)&p->mem[addr & 0xfff] || codegen_in_recompile) {
#else
    if (val != *(uint16_t *)&p->mem[addr & 0xfff]) {
#endif
	uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
	if ((addr & 0xf) == 0xf)
		mask |= (mask << 1);
	p->dirty_mask[(addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] |= mask;
	*(uint16_t *)&p->mem[addr & 0xfff] = val;
    }
}


void
mem_write_raml_page(uint32_t addr, uint32_t val, page_t *p)
{
#ifdef USE_DYNAREC
    if (val != *(uint32_t *)&p->mem[addr & 0xfff] || codegen_in_recompile) {
#else
    if (val != *(uint32_t *)&p->mem[addr & 0xfff]) {
#endif
	uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
	if ((addr & 0xf) >= 0xd)
		mask |= (mask << 1);
	p->dirty_mask[(addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] |= mask;
	*(uint32_t *)&p->mem[addr & 0xfff] = val;
    }
}


void
mem_write_ram(uint32_t addr, uint8_t val, void *priv)
{
    addwritelookup(mem_logical_addr, addr);
    mem_write_ramb_page(addr, val, &pages[addr >> 12]);
}


void
mem_write_ramw(uint32_t addr, uint16_t val, void *priv)
{
    addwritelookup(mem_logical_addr, addr);
    mem_write_ramw_page(addr, val, &pages[addr >> 12]);
}


void
mem_write_raml(uint32_t addr, uint32_t val, void *priv)
{
    addwritelookup(mem_logical_addr, addr);
    mem_write_raml_page(addr, val, &pages[addr >> 12]);
}


static uint8_t
mem_read_remapped(uint32_t addr, void *priv)
{
    if (addr >= (1024UL * mem_size) && addr < (1024UL * (mem_size + 384)))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addreadlookup(mem_logical_addr, addr);

    return ram[addr];
}


static uint16_t
mem_read_remappedw(uint32_t addr, void *priv)
{
    if ((addr >= (1024UL * mem_size)) && (addr < (1024UL * (mem_size + 384))))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addreadlookup(mem_logical_addr, addr);

    return *(uint16_t *)&ram[addr];
}


static uint32_t
mem_read_remappedl(uint32_t addr, void *priv)
{
    if ((addr >= (1024UL * mem_size)) && (addr < (1024UL * (mem_size + 384))))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addreadlookup(mem_logical_addr, addr);

    return *(uint32_t *)&ram[addr];
}


static void
mem_write_remapped(uint32_t addr, uint8_t val, void *priv)
{
    uint32_t oldaddr = addr;

    if ((addr >= (1024UL * mem_size)) && (addr < (1024UL * (mem_size + 384))))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addwritelookup(mem_logical_addr, addr);

    mem_write_ramb_page(addr, val, &pages[oldaddr >> 12]);
}


static void
mem_write_remappedw(uint32_t addr, uint16_t val, void *priv)
{
    uint32_t oldaddr = addr;

    if ((addr >= (1024UL * mem_size)) && (addr < (1024UL * (mem_size + 384))))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addwritelookup(mem_logical_addr, addr);

    mem_write_ramw_page(addr, val, &pages[oldaddr >> 12]);
}


static void
mem_write_remappedl(uint32_t addr, uint32_t val, void *priv)
{
    uint32_t oldaddr = addr;

    if ((addr >= (1024UL * mem_size)) && (addr < (1024UL * (mem_size + 384))))
	addr = 0xA0000 + (addr - (mem_size * 1024));

    addwritelookup(mem_logical_addr, addr);

    mem_write_raml_page(addr, val, &pages[oldaddr >> 12]);
}


void
mem_write_null(uint32_t addr, uint8_t val, void *p)
{
}


void
mem_write_nullw(uint32_t addr, uint16_t val, void *p)
{
}


void
mem_write_nulll(uint32_t addr, uint32_t val, void *p)
{
}


void
mem_invalidate_range(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t cur_addr;

    start_addr &= ~PAGE_MASK_MASK;
    end_addr = (end_addr + PAGE_MASK_MASK) & ~PAGE_MASK_MASK;	

    for (; start_addr <= end_addr; start_addr += (1 << PAGE_MASK_SHIFT)) {
	uint64_t mask = (uint64_t)1 << ((start_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);

	/*
	 * Do nothing if the pages array is empty or DMA reads/writes
	 * to/from PCI device memory addresses may crash the emulator.
	 */
	cur_addr = (start_addr >> 12);
	if (cur_addr < pages_sz)
		pages[cur_addr].dirty_mask[(start_addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] |= mask;
    }
}


static __inline int
mem_map_read_allowed(uint32_t fl, int state)
{
    switch (state & MEM_READ_MASK) {
	case MEM_READ_ANY:
		return 1;

	case MEM_READ_EXTERNAL:
		return !(fl & MEM_MAPPING_INTERNAL);

	case MEM_READ_INTERNAL:
		return !(fl & MEM_MAPPING_EXTERNAL);

	default:
		fatal("mem_map_read_allowed : bad state %x\n", state);
    }

    return 0;
}


static __inline int
mem_map_write_allowed(uint32_t fl, int state)
{
    switch (state & MEM_WRITE_MASK) {
	case MEM_WRITE_DISABLED:
		return 0;
	case MEM_WRITE_ANY:
		return 1;
	case MEM_WRITE_EXTERNAL:
		return !(fl & MEM_MAPPING_INTERNAL);
	case MEM_WRITE_INTERNAL:
		return !(fl & MEM_MAPPING_EXTERNAL);
	default:
		fatal("mem_map_write_allowed : bad state %x\n", state);
    }

    return 0;
}


static void
mem_map_recalc(uint64_t base, uint64_t size)
{
    mem_map_t *map = base_mapping.next;
    uint64_t c;

    if (! size) return;

    /* Clear out old mappings. */
    for (c = base; c < base + size; c += 0x4000) {
	read_mapping[c >> 14] = NULL;
	write_mapping[c >> 14] = NULL;
    }

    /* Walk mapping list. */
    while (map != NULL) {
	/*In range?*/
	if (map->enable && (uint64_t)map->base < ((uint64_t)base + (uint64_t)size) && ((uint64_t)map->base + (uint64_t)map->size) > (uint64_t)base) {
		uint64_t start = (map->base < base) ? map->base : base;
		uint64_t end   = (((uint64_t)map->base + (uint64_t)map->size) < (base + size)) ? ((uint64_t)map->base + (uint64_t)map->size) : (base + size);
		if (start < map->base)
			start = map->base;

		for (c = start; c < end; c += 0x4000) {
			if ((map->read_b || map->read_w || map->read_l) &&
			     mem_map_read_allowed(map->flags, _mem_state[c >> 14])) {
				if (map->exec)
					_mem_exec[c >> 14] = map->exec + (c - map->base);
				else
					_mem_exec[c >> 14] = NULL;
				read_mapping[c >> 14] = map;
			}
			if ((map->write_b || map->write_w || map->write_l) &&
			     mem_map_write_allowed(map->flags, _mem_state[c >> 14]))
				write_mapping[c >> 14] = map;
		}
	}
	map = map->next;
    }

    flushmmucache_cr3();
}


void
mem_map_del(mem_map_t *map)
{
    mem_map_t *ptr;

    /* Disable the entry. */
    mem_map_disable(map);

    /* Zap it from the list. */
    for (ptr = &base_mapping; ptr->next != NULL; ptr = ptr->next) {
	if (ptr->next == map) {
		ptr->next = map->next;
		break;
	}
    }
}


void
mem_map_add(mem_map_t *map, uint32_t base, uint32_t size, 
	    uint8_t  (*read_b)(uint32_t addr, void *p),
	    uint16_t (*read_w)(uint32_t addr, void *p),
	    uint32_t (*read_l)(uint32_t addr, void *p),
	    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
	    void (*write_w)(uint32_t addr, uint16_t val, void *p),
	    void (*write_l)(uint32_t addr, uint32_t val, void *p),
	    uint8_t *exec, uint32_t fl, void *p)
{
    mem_map_t *dest = &base_mapping;

    /* Add mapping to the end of the list.*/
    while (dest->next)
	dest = dest->next;
    dest->next = map;
    map->prev = dest;

    if (size)
	map->enable  = 1;
      else
	map->enable  = 0;
    map->base    = base;
    map->size    = size;
    map->read_b  = read_b;
    map->read_w  = read_w;
    map->read_l  = read_l;
    map->write_b = write_b;
    map->write_w = write_w;
    map->write_l = write_l;
    map->exec    = exec;
    map->flags   = fl;
    map->p       = p;
    map->p2      = NULL;
    map->dev     = NULL;
    map->next    = NULL;

    mem_map_recalc(map->base, map->size);
}


void
mem_map_set_handler(mem_map_t *map,
		    uint8_t  (*read_b)(uint32_t addr, void *p),
		    uint16_t (*read_w)(uint32_t addr, void *p),
		    uint32_t (*read_l)(uint32_t addr, void *p),
		    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
		    void (*write_w)(uint32_t addr, uint16_t val, void *p),
		    void (*write_l)(uint32_t addr, uint32_t val, void *p))
{
    map->read_b  = read_b;
    map->read_w  = read_w;
    map->read_l  = read_l;
    map->write_b = write_b;
    map->write_w = write_w;
    map->write_l = write_l;

    mem_map_recalc(map->base, map->size);
}


void
mem_map_set_addr(mem_map_t *map, uint32_t base, uint32_t size)
{
    /* Remove old mapping. */
    map->enable = 0;
    mem_map_recalc(map->base, map->size);

    /* Set new mapping. */
    map->enable = 1;
    map->base = base;
    map->size = size;

    mem_map_recalc(map->base, map->size);
}


void
mem_map_set_exec(mem_map_t *map, uint8_t *exec)
{
    map->exec = exec;

    mem_map_recalc(map->base, map->size);
}


void
mem_map_set_p(mem_map_t *map, void *p)
{
    map->p = p;
}


void
mem_map_set_p2(mem_map_t *map, void *p)
{
    map->p2 = p;
}


void
mem_map_set_dev(mem_map_t *map, void *p)
{
    map->dev = p;
}


void
mem_map_disable(mem_map_t *map)
{
    map->enable = 0;

    mem_map_recalc(map->base, map->size);
}


void
mem_map_enable(mem_map_t *map)
{
    map->enable = 1;

    mem_map_recalc(map->base, map->size);
}


void
mem_set_mem_state(uint32_t base, uint32_t size, int state)
{
    uint32_t c;

    for (c = 0; c < size; c += 0x4000)
	_mem_state[(c + base) >> 14] = state;

    mem_map_recalc(base, size);
}


void
mem_a20_init(void)
{
    if (AT) {
	rammask = cpu_16bitbus ? 0xefffff : 0xffefffff;
	flushmmucache();
	mem_a20_state = mem_a20_key | mem_a20_alt;
    } else {
	rammask = 0xfffff;
	flushmmucache();
	mem_a20_key = mem_a20_alt = mem_a20_state = 0;
    }
}


/* Reset the memory state. */
void
mem_reset(void)
{
    uint32_t c, m;

    /*
     * Make sure the configured amount of RAM does not
     * exceed the physical limit of the machine to avoid
     * nasty crashes all over the place.
     */
    m = mem_size;
    c = machine->max_ram;
    if (AT)
	c <<= 10;	/* make KB */
    if (m > c) {
	INFO("MEM: %luKB exceeds machine limit (%luKB), adjusted!\n", m, c);
	mem_size = c;
    }

    m = 1024UL * mem_size;
    if (ram != NULL)
	free(ram);
    ram = (uint8_t *)mem_alloc(m);	/* allocate and clear the RAM block */
    memset(ram, 0x00, m);

    /*
     * Allocate the page table based on how much RAM we have.
     * We re-allocate the table on each (hard) reset, as the
     * memory amount could have changed.
     */
    if (AT) {
	if (cpu_16bitbus) {
		/* 80186/286; maximum address space is 16MB. */
		m = 4096;
	} else {
		/* 80386+; maximum address space is 4GB. */
		m = (mem_size + 384) >> 2;
		if ((m << 2) < ((uint32_t)mem_size + 384))
			m++;
		if (m < 4096)
			m = 4096;
	}
    } else {
	/* 8088/86; maximum address space is 1MB. */
	m = 256;
    }

    /*
     * Allocate and initialize the (new) page table.
     * We only do this if the size of the page table has changed.
     */
    if (pages_sz != m || pages_sz == 0) {
	pages_sz = m;
	free(pages);
	pages = (page_t *)mem_alloc(m * sizeof(page_t));
	memset(pages, 0x00, m * sizeof(page_t));
    }

    /* Initialize the page table. */
    for (c = 0; c < m; c++) {
	pages[c].mem = &ram[c << 12];
	pages[c].write_b = mem_write_ramb_page;
	pages[c].write_w = mem_write_ramw_page;
	pages[c].write_l = mem_write_raml_page;
    }

    /* Initialize the tables. */
    resetreadlookup();

    memset(_mem_exec,    0x00, sizeof(_mem_exec));

    memset(&base_mapping, 0x00, sizeof(base_mapping));

    memset(_mem_state, 0x00, sizeof(_mem_state));

    mem_set_mem_state(0x000000, (mem_size > 640) ? 0xa0000 : mem_size * 1024,
		      MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
    mem_set_mem_state(0x0c0000, 0x40000,
		      MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

    mem_map_add(&ram_low_mapping, 0x00000,
		(mem_size > 640) ? 0xa0000 : mem_size * 1024,
		mem_read_ram,mem_read_ramw,mem_read_raml,
		mem_write_ram,mem_write_ramw,mem_write_raml,
		ram, MEM_MAPPING_INTERNAL, NULL);

    if (mem_size > 1024) {
	if (cpu_16bitbus && mem_size > 16256) {
		mem_set_mem_state(0x100000, (16256 - 1024) * 1024,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		mem_map_add(&ram_high_mapping, 0x100000,
			    ((16256 - 1024) * 1024),
			    mem_read_ram,mem_read_ramw,mem_read_raml,
			    mem_write_ram,mem_write_ramw,mem_write_raml,
			    ram + 0x100000, MEM_MAPPING_INTERNAL, NULL);
	} else {
		mem_set_mem_state(0x100000, (mem_size - 1024) * 1024,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
		mem_map_add(&ram_high_mapping, 0x100000,
			    ((mem_size - 1024) * 1024),
			    mem_read_ram,mem_read_ramw,mem_read_raml,
			    mem_write_ram,mem_write_ramw,mem_write_raml,
			    ram + 0x100000, MEM_MAPPING_INTERNAL, NULL);
	}
    }

    if (mem_size > 768)
	mem_map_add(&ram_mid_mapping, 0xc0000, 0x40000,
		    mem_read_ram,mem_read_ramw,mem_read_raml,
		    mem_write_ram,mem_write_ramw,mem_write_raml,
		    ram + 0xc0000, MEM_MAPPING_INTERNAL, NULL);

    mem_map_add(&ram_remapped_mapping, mem_size * 1024, 256 * 1024,
	        mem_read_remapped,mem_read_remappedw,mem_read_remappedl,
	        mem_write_remapped,mem_write_remappedw,mem_write_remappedl,
	        ram + 0xa0000, MEM_MAPPING_INTERNAL, NULL);
    mem_map_disable(&ram_remapped_mapping);

    mem_a20_init();
}


/* Perform a one-time memory initialization. */
void
mem_init(void)
{
    /* Perform a one-time init. */
    ram = NULL;
    pages = NULL;
    pages_sz = 0;

    /* Allocate the lookup tables. */
    page_lookup = (page_t **)mem_alloc((1<<20)*sizeof(page_t *));

    readlookup2  = (uintptr_t *)mem_alloc((1<<20)*sizeof(uintptr_t));

    writelookup2 = (uintptr_t *)mem_alloc((1<<20)*sizeof(uintptr_t));

    memset(ram_mapped_addr, 0x00, 64 * sizeof(uint32_t));
}


void
mem_remap_top(int kb)
{
    uint32_t start = (mem_size >= 1024) ? mem_size : 1024;
    int size = mem_size - 640;
    uint32_t i;

    INFO("MEM: remapping top %iKB (mem=%i)\n", kb, mem_size);

    if (mem_size <= 640) return;

    if (kb == 0) {
	/* Called to disable the mapping. */
	mem_map_disable(&ram_remapped_mapping);

	return;
    }

    if (size > kb)
	size = kb;

    for (i = ((start * 1024) >> 12); i < (((start + size) * 1024) >> 12); i++) {
	pages[i].mem = &ram[0xA0000 + ((i - ((start * 1024) >> 12)) << 12)];
	pages[i].write_b = mem_write_ramb_page;
	pages[i].write_w = mem_write_ramw_page;
	pages[i].write_l = mem_write_raml_page;
    }

    mem_set_mem_state(start * 1024, size * 1024,
		      MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
    mem_map_set_addr(&ram_remapped_mapping, start * 1024, size * 1024);
    mem_map_set_exec(&ram_remapped_mapping, ram + (start * 1024));

    flushmmucache();
}


void
mem_reset_page_blocks(void)
{
    uint32_t c;

    if (pages == NULL) return;

    for (c = 0; c < pages_sz; c++) {
	pages[c].write_b = mem_write_ramb_page;
	pages[c].write_w = mem_write_ramw_page;
	pages[c].write_l = mem_write_raml_page;
	pages[c].block[0] = pages[c].block[1] =
			    pages[c].block[2] = pages[c].block[3] = NULL;
	pages[c].block_2[0] = pages[c].block_2[1] =
			      pages[c].block_2[2] = pages[c].block_2[3] = NULL;
    }
}


void
mem_a20_recalc(void)
{
    int state;

    if (! AT) {
	rammask = 0xfffff;
	flushmmucache();
	mem_a20_key = mem_a20_alt = mem_a20_state = 0;

	return;
    }

    state = mem_a20_key | mem_a20_alt;
    if (state && !mem_a20_state) {
	rammask = (AT && cpu_16bitbus) ? 0xffffff : 0xffffffff;
	flushmmucache();
    } else if (!state && mem_a20_state) {
	rammask = (AT && cpu_16bitbus) ? 0xefffff : 0xffefffff;
	flushmmucache();
    }

    mem_a20_state = state;
}


uint8_t
port_92_read(uint16_t port, void *priv)
{
    return port_92_reg | port_92_mask;
}


void
port_92_write(uint16_t port, uint8_t val, void *priv)
{
    if ((mem_a20_alt ^ val) & 2) {
	mem_a20_alt = (val & 2);
	mem_a20_recalc();
    }

    if ((~port_92_reg & val) & 1) {
	cpu_reset(0);
	cpu_set_edx();
    }

    port_92_reg = val | port_92_mask;
}


void
port_92_clear_reset(void)
{
    port_92_reg &= 2;
}


void
port_92_add(int inv)
{
    port_92_mask = (inv) ? 0xfc : 0x00;

    io_sethandler(0x0092, 1,
		  port_92_read,NULL,NULL, port_92_write, NULL,NULL,NULL);
}


void
port_92_remove(void)
{
    io_removehandler(0x0092, 1,
		     port_92_read,NULL,NULL, port_92_write,NULL,NULL, NULL);
}


void
port_92_reset(void)
{
    port_92_reg = 0;
    mem_a20_alt = 0;
    mem_a20_recalc();

    flushmmucache();
}
