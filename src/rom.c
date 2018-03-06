/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of ROM image files.
 *
 * NOTES:	- pc2386 BIOS is corrupt (JMP at F000:FFF0 points to RAM)
 *		- pc2386 video BIOS is underdumped (16k instead of 24k)
 *		- c386sx16 BIOS fails checksum
 *		- the loadfont() calls should be done elsewhere
 *
 * Version:	@(#)rom.c	1.0.5	2018/03/04
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
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "emu.h"
#include "config.h"
#include "cpu/cpu.h"
#include "mem.h"
#include "rom.h"
#include "video/video.h"		/* for loadfont() */
#include "plat.h"


int	romspresent[ROM_MAX];


FILE *
rom_fopen(wchar_t *fn)
{
    wchar_t temp[1024];

    wcscpy(temp, emu_path);
    plat_put_backslash(temp);
    wcscat(temp, fn);

    return(plat_fopen(temp, L"rb"));
}


int
rom_getfile(wchar_t *fn, wchar_t *s, int size)
{
    FILE *f;

    wcscpy(s, emu_path);
    plat_put_backslash(s);
    wcscat(s, fn);

    f = plat_fopen(s, L"rb");
    if (f != NULL) {
	(void)fclose(f);
	return(1);
    }

    return(0);
}


int
rom_present(wchar_t *fn)
{
    FILE *f;

    f = rom_fopen(fn);
    if (f != NULL) {
	(void)fclose(f);
	return(1);
    }

    return(0);
}


/* Read a byte from some area in ROM. */
uint8_t
rom_read(uint32_t addr, void *priv)
{
    rom_t *rom = (rom_t *)priv;

#ifdef ROM_TRACE
    if (rom->mapping.base==ROM_TRACE)
	pclog("ROM: read byte from BIOS at %06lX\n", addr);
#endif

    return(rom->rom[addr & rom->mask]);
}


/* Read a word from some area in ROM. */
uint16_t
rom_readw(uint32_t addr, void *priv)
{
    rom_t *rom = (rom_t *)priv;

#ifdef ROM_TRACE
    if (rom->mapping.base==ROM_TRACE)
	pclog("ROM: read word from BIOS at %06lX\n", addr);
#endif

    return(*(uint16_t *)&rom->rom[addr & rom->mask]);
}


/* Read a double-word from some area in ROM. */
uint32_t
rom_readl(uint32_t addr, void *priv)
{
    rom_t *rom = (rom_t *)priv;

#ifdef ROM_TRACE
    if (rom->mapping.base==ROM_TRACE)
	pclog("ROM: read long from BIOS at %06lX\n", addr);
#endif

    return(*(uint32_t *)&rom->rom[addr & rom->mask]);
}


/* Load a ROM BIOS from its chips, interleaved mode. */
int
rom_load_linear(wchar_t *fn, uint32_t addr, int sz, int off, uint8_t *ptr)
{
    FILE *f = rom_fopen(fn);
        
    if (f == NULL) {
	pclog("ROM: image '%ls' not found\n", fn);
	return(0);
    }

    /* Make sure we only look at the base-256K offset. */
    if (addr >= 0x40000)
	addr = 0;
      else
	addr &= 0x03ffff;

    (void)fseek(f, off, SEEK_SET);
    (void)fread(ptr+addr, sz, 1, f);
    (void)fclose(f);

    return(1);
}


/* Load a ROM BIOS from its chips, linear mode with high bit flipped. */
int
rom_load_linear_inverted(wchar_t *fn, uint32_t addr, int sz, int off, uint8_t *ptr)
{
    FILE *f = rom_fopen(fn);

    if (f == NULL) {
	pclog("ROM: image '%ls' not found\n", fn);
	return(0);
    }

    /* Make sure we only look at the base-256K offset. */
    if (addr >= 0x40000)
	addr = 0;
      else
	addr &= 0x03ffff;

    (void)fseek(f, 0, SEEK_END);
    if (ftell(f) < sz) {
	(void)fclose(f);
	return(0);
    }

    (void)fseek(f, off, SEEK_SET);
    (void)fread(ptr+addr+0x10000, sz >> 1, 1, f);
    (void)fread(ptr+addr, sz >> 1, 1, f);
    (void)fclose(f);

    return(1);
}


/* Load a ROM BIOS from its chips, interleaved mode. */
int
rom_load_interleaved(wchar_t *fnl, wchar_t *fnh, uint32_t addr, int sz, int off, uint8_t *ptr)
{
    FILE *fl = rom_fopen(fnl);
    FILE *fh = rom_fopen(fnh);
    int c;

    if (fl == NULL || fh == NULL) {
	if (fl == NULL) pclog("ROM: image '%ls' not found\n", fnl);
	  else (void)fclose(fl);
	if (fh == NULL) pclog("ROM: image '%ls' not found\n", fnh);
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
    for (c=0; c<sz; c+=2) {
	ptr[addr+c] = fgetc(fl);
	ptr[addr+c+1] = fgetc(fh);
    }
    (void)fclose(fh);
    (void)fclose(fl);

    return(1);
}


/* Read and initialize an option ROM. */
int
rom_init(rom_t *rom, wchar_t *fn, uint32_t addr, int sz, int mask, int off, uint32_t flags)
{
    /* Allocate a buffer for the image. */
    rom->rom = malloc(sz);
    memset(rom->rom, 0xff, sz);

    /* Load the image file into the buffer. */
    if (! rom_load_linear(fn, addr, sz, off, rom->rom)) {
	/* Nope.. clean up. */
	free(rom->rom);
	rom->rom = NULL;
	return(-1);
    }

    rom->mask = mask;

    mem_mapping_add(&rom->mapping,
		    addr, sz,
		    rom_read, rom_readw, rom_readl,
		    mem_write_null, mem_write_nullw, mem_write_nulll,
		    rom->rom, flags, rom);

    return(0);
}


int
rom_init_interleaved(rom_t *rom, wchar_t *fnl, wchar_t *fnh, uint32_t addr, int sz, int mask, int off, uint32_t flags)
{
    /* Allocate a buffer for the image. */
    rom->rom = malloc(sz);
    memset(rom->rom, 0xff, sz);

    /* Load the image file into the buffer. */
    if (! rom_load_interleaved(fnl, fnh, addr, sz, off, rom->rom)) {
	/* Nope.. clean up. */
	free(rom->rom);
	rom->rom = NULL;
	return(-1);
    }

    rom->mask = mask;

    mem_mapping_add(&rom->mapping,
		    addr, sz,
		    rom_read, rom_readw, rom_readl,
		    mem_write_null, mem_write_nullw, mem_write_nulll,
		    rom->rom, flags, rom);

    return(0);
}
