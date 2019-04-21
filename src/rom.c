/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of ROM image files.
 *
 * Version:	@(#)rom.c	1.0.18	2019/04/11
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
#include "emu.h"
#include "cpu/cpu.h"
#include "mem.h"
#include "rom.h"
#include "plat.h"


mem_map_t	bios_mapping[8],
		bios_high_mapping[8];
uint8_t		*bios = NULL;
uint32_t	biosmask;
int		shadowbios = 0,
		shadowbios_write;


/* Read one byte from the BIOS ROM area. */
uint8_t
rom_bios_read(uint32_t addr, void *priv)
{
    return bios[addr & biosmask];
}


/* Read one word from the BIOS ROM area. */
uint16_t
rom_bios_readw(uint32_t addr, void *priv)
{
    return *(uint16_t *)&bios[addr & biosmask];
}


/* Read one doubleword from the BIOS ROM area. */
uint32_t
rom_bios_readl(uint32_t addr, void *priv)
{
    return *(uint32_t *)&bios[addr & biosmask];
}


/* Read a byte from some area in ROM. */
uint8_t
rom_read(uint32_t addr, void *priv)
{
    rom_t *ptr = (rom_t *)priv;

#ifdef ROM_TRACE
    if (ptr->mapping.base == ROM_TRACE)
	DEBUG("ROM: read byte from BIOS at %06lX\n", addr);
#endif

    return(ptr->rom[addr & ptr->mask]);
}


/* Read a word from some area in ROM. */
uint16_t
rom_readw(uint32_t addr, void *priv)
{
    rom_t *ptr = (rom_t *)priv;

#ifdef ROM_TRACE
    if (ptr->mapping.base == ROM_TRACE)
	DEBUG("ROM: read word from BIOS at %06lX\n", addr);
#endif

    return(*(uint16_t *)&ptr->rom[addr & ptr->mask]);
}


/* Read a double-word from some area in ROM. */
uint32_t
rom_readl(uint32_t addr, void *priv)
{
    rom_t *ptr = (rom_t *)priv;

#ifdef ROM_TRACE
    if (ptr->mapping.base == ROM_TRACE)
	DEBUG("ROM: read long from BIOS at %06lX\n", addr);
#endif

    return(*(uint32_t *)&ptr->rom[addr & ptr->mask]);
}


/* (Re-)allocate ROM area and reset size mask. */
void
rom_reset(void)
{
    uint32_t c;

    if (bios != NULL)
	free(bios);

    /* Allocate 128KB of BIOS space. */
    c = 1024UL * 128;
    bios = (uint8_t *)mem_alloc(c);
    memset(bios, 0xff, c);

    /* Set mask for a 64K (or smaller) ROM. */
    biosmask = 0x00ffff;
}


/* Return the base path for ROM images. */
wchar_t *
rom_path(const wchar_t *str)
{
    static wchar_t temp[1024];

    /* Get the full path in place. */
    wcscpy(temp, emu_path);
    wcscat(temp, ROMS_PATH);
    plat_append_slash(temp);
    wcscat(temp, str);

    /* Make sure path is clean. */
    pc_path(temp, sizeof_w(temp), NULL);

    return(temp);
}


/* Used by the available() functions to check if a file exists. */
int
rom_present(const wchar_t *fn)
{
    FILE *fp;

    fp = plat_fopen(rom_path(fn), L"rb");
    if (fp != NULL) {
	(void)fclose(fp);
	return(1);
    }

    ERRLOG("ROM: image for '%ls' not found!\n", fn);

    return(0);
}


/* Map in the E0000 ROM segments. */
void
rom_add_upper_bios(void)
{
    uint32_t base = 0x0e0000;
    uint32_t size = 0x4000;
    int i;

    for (i = 0; i < 4; i++) {
	mem_map_add(&bios_mapping[i], base, size,
			    rom_bios_read,rom_bios_readw,rom_bios_readl,
			    mem_write_null,mem_write_nullw,mem_write_nulll,
			    bios + ((0x10000 + (size * i)) & biosmask),
			    MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
	base += size;
    }
}


/* Map in the BIOS ROM segments. */
void
rom_add_bios(void)
{
    uint32_t hibase = (AT && cpu_16bitbus) ? 0xfe0000 : 0xfffe0000;
    uint32_t lobase = 0x0f0000;
    uint32_t size = 0x4000;
    int i;

    /* Add E0000-EFFFF mappings for AT+ systems. */
    if (AT)
	rom_add_upper_bios();

    /* Add F0000-FFFFF mappings. */
    for (i = 0; i < 4; i++) {
	mem_map_add(&bios_mapping[i + 4], lobase, size,
		    rom_bios_read,rom_bios_readw,rom_bios_readl,
		    mem_write_null,mem_write_nullw,mem_write_nulll,
		    bios + ((0x10000 + (size * i)) & biosmask),
		    MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
	lobase += size;
    }

    /* Add FE0000-FFFFFF mappings for 286+ systems. */
    for (i = 0; i < 8; i++) {
	mem_map_add(&bios_high_mapping[i], hibase, size,
		    rom_bios_read,rom_bios_readw,rom_bios_readl,
		    mem_write_null,mem_write_nullw,mem_write_nulll,
		    bios + ((size * i) & biosmask), MEM_MAPPING_ROM, 0);
	hibase += size;
    }
}


/* Load a ROM BIOS from its chips, linear mode. */
int
rom_load_linear(const wchar_t *fn, uint32_t addr, int sz, int off, uint8_t *ptr)
{
    FILE *fp;
 
    fp = plat_fopen(rom_path(fn), L"rb");
    if (fp == NULL) {
	ERRLOG("ROM: image '%ls' not found\n", fn);
	return(0);
    }

    /* Make sure we only look at the base-256K offset. */
    if (addr >= 0x40000)
	addr = 0;
      else
	addr &= 0x03ffff;

    (void)fseek(fp, off, SEEK_SET);
    (void)fread(ptr + addr, sz, 1, fp);
    (void)fclose(fp);

    return(1);
}


/* Load a ROM BIOS from its chips, interleaved mode. */
int
rom_load_interleaved(const wchar_t *fnl, const wchar_t *fnh, uint32_t addr, int sz, int off, uint8_t *ptr)
{
    FILE *fl = plat_fopen(rom_path(fnl), L"rb");
    FILE *fh = plat_fopen(rom_path(fnh), L"rb");
    int c;

    if (fl == NULL || fh == NULL) {
	if (fl == NULL) ERRLOG("ROM: image '%ls' not found\n", fnl);
	  else (void)fclose(fl);
	if (fh == NULL) ERRLOG("ROM: image '%ls' not found\n", fnh);
	  else (void)fclose(fh);

	return(0);
    }

    /* Make sure we only look at the base-256K offset. */
    if (addr >= 0x40000)
	addr = 0;
      else
	addr &= 0x03ffff;

    (void)fseek(fl, off, SEEK_SET);
    (void)fseek(fh, off, SEEK_SET);
    for (c = 0; c < sz; c += 2) {
	ptr[addr + c] = fgetc(fl);
	ptr[addr + c + 1] = fgetc(fh);
    }
    (void)fclose(fh);
    (void)fclose(fl);

    return(1);
}


/* Read and initialize an option ROM. */
int
rom_init(rom_t *ptr, const wchar_t *fn, uint32_t addr, int sz, int mask, int off, uint32_t fl)
{
    /* Allocate a buffer for the image. */
    ptr->rom = (uint8_t *)mem_alloc(sz);
    memset(ptr->rom, 0xff, sz);

    /* Load the image file into the buffer. */
    if (! rom_load_linear(fn, addr, sz, off, ptr->rom)) {
	/* Nope.. clean up. */
	free(ptr->rom);
	ptr->rom = NULL;
	return(0);
    }

    ptr->mask = mask;

    mem_map_add(&ptr->mapping, addr, sz,
		rom_read, rom_readw, rom_readl,
		mem_write_null, mem_write_nullw, mem_write_nulll,
		ptr->rom, fl | MEM_MAPPING_ROM, ptr);

    return(1);
}


int
rom_init_interleaved(rom_t *ptr, const wchar_t *fnl, const wchar_t *fnh, uint32_t addr, int sz, int mask, int off, uint32_t fl)
{
    /* Allocate a buffer for the image. */
    ptr->rom = (uint8_t *)mem_alloc(sz);
    memset(ptr->rom, 0xff, sz);

    /* Load the image file into the buffer. */
    if (! rom_load_interleaved(fnl, fnh, addr, sz, off, ptr->rom)) {
	/* Nope.. clean up. */
	free(ptr->rom);
	ptr->rom = NULL;
	return(0);
    }

    ptr->mask = mask;

    mem_map_add(&ptr->mapping, addr, sz,
		rom_read, rom_readw, rom_readl,
		mem_write_null, mem_write_nullw, mem_write_nulll,
		ptr->rom, fl | MEM_MAPPING_ROM, ptr);

    return(1);
}
