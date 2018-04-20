/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the ROM image handler.
 *
 * Version:	@(#)rom.h	1.0.11	2018/04/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EMU_ROM_H
# define EMU_ROM_H


#define ROMDEF_NFILES	8		/* max number of rom image files */


typedef struct {
    uint8_t		*rom;
    uint32_t		mask;
    mem_mapping_t	mapping;
} rom_t;

typedef struct {
    int8_t	mode;
    int8_t	fontnum;
    int16_t	nfiles;
    uint32_t	vidsz;
    uint32_t	offset;
    uint32_t	total;
    wchar_t	fontfn[1024];
    wchar_t	vidfn[1024];
    struct romfile {
	wchar_t		path[1024];
	int		mode;
	uint32_t	offset;
	uint32_t	skip;
	uint32_t	size;
    }		files[ROMDEF_NFILES];
} romdef_t;


extern uint8_t	rom_read(uint32_t addr, void *p);
extern uint16_t	rom_readw(uint32_t addr, void *p);
extern uint32_t	rom_readl(uint32_t addr, void *p);

extern wchar_t	*rom_path(const wchar_t *fn);
extern int	rom_present(const wchar_t *fn);

extern int	rom_load_linear(const wchar_t *fn, uint32_t addr, int sz,
				int off, uint8_t *ptr);
extern int	rom_load_interleaved(const wchar_t *fnl, const wchar_t *fnh,
				     uint32_t addr, int sz, int off,
				     uint8_t *ptr);

extern int	rom_init(rom_t *rom, const wchar_t *fn, uint32_t address,
			 int size, int mask, int file_offset, uint32_t flags);
extern int	rom_init_interleaved(rom_t *rom, const wchar_t *fn_low,
				     const wchar_t *fn_high, uint32_t address,
				     int size, int mask, int file_offset,
				     uint32_t flags);

extern int	rom_load_bios(romdef_t *r, const wchar_t *fn, int test_only);


#endif	/*EMU_ROM_H*/
