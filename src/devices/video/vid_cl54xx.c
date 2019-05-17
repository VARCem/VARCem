/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of select Cirrus Logic cards (CL-GD 5428,
 *		CL-GD 5429, 5430, 5434 and 5436 are supported).
 *
 * Version:	@(#)vid_cl54xx.c	1.0.30	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Barry Rodewald,
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2018 Barry Rodewald.
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
#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../cpu/cpu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"


#define BIOS_GD5420_PATH	L"video/cirruslogic/5420.vbi"
#define BIOS_GD5422_PATH	L"video/cirruslogic/cl5422.bin"
#define BIOS_GD5426_PATH	L"video/cirruslogic/diamond speedstar pro vlb v3.04.bin"
#define BIOS_GD5428_ISA_PATH	L"video/cirruslogic/gd5428.bin"
#define BIOS_GD5428_VLB_PATH	L"video/cirruslogic/vlbusjapan.bin"
#define BIOS_GD5429_PATH	L"video/cirruslogic/gd5429.vbi"
#define BIOS_GD5430_VLB_PATH	L"video/cirruslogic/diamondvlbus.bin"
#define BIOS_GD5430_PCI_PATH	L"video/cirruslogic/gd5430pci.bin"
#define BIOS_GD5434_PATH	L"video/cirruslogic/gd5434.bin"
#define BIOS_GD5436_PATH	L"video/cirruslogic/gd5436.vbi"
#define BIOS_GD5440_PATH	L"video/cirruslogic/gd5440.bin"
#define BIOS_GD5446_PATH	L"video/cirruslogic/gd5446bv.vbi"
#define BIOS_GD5446_STB_PATH	L"video/cirruslogic/stb_nitro64v.bin"
#define BIOS_GD5480_PATH	L"video/cirruslogic/gd5480.rom"


#define CIRRUS_ID_CLGD5402	  	0x89
#define CIRRUS_ID_CLGD5420	  	0x8a
#define CIRRUS_ID_CLGD5422  		0x8c
#define CIRRUS_ID_CLGD5424  		0x94
#define CIRRUS_ID_CLGD5426		0x90
#define CIRRUS_ID_CLGD5428		0x98
#define CIRRUS_ID_CLGD5429		0x9c
#define CIRRUS_ID_CLGD5430		0xa0
#define CIRRUS_ID_CLGD5434		0xa8
#define CIRRUS_ID_CLGD5436		0xac
#define CIRRUS_ID_CLGD5440		0xa0	/* yes, same ID as the 5430 */
#define CIRRUS_ID_CLGD5446		0xb8
#define CIRRUS_ID_CLGD5480		0xbc


/* sequencer 0x07 */
#define CIRRUS_SR7_BPP_VGA		0x00
#define CIRRUS_SR7_BPP_SVGA		0x01
#define CIRRUS_SR7_BPP_MASK		0x0e
#define CIRRUS_SR7_BPP_8		0x00
#define CIRRUS_SR7_BPP_16_DOUBLEVCLK	0x02
#define CIRRUS_SR7_BPP_24		0x04
#define CIRRUS_SR7_BPP_16		0x06
#define CIRRUS_SR7_BPP_32		0x08
#define CIRRUS_SR7_ISAADDR_MASK		0xe0

/* sequencer 0x12 */
#define CIRRUS_CURSOR_SHOW		0x01
#define CIRRUS_CURSOR_HIDDENPEL		0x02
#define CIRRUS_CURSOR_LARGE		0x04	/* 64x64 if set, else 32x32 */

/* sequencer 0x17 */
#define CIRRUS_BUSTYPE_VLBFAST		0x10
#define CIRRUS_BUSTYPE_PCI		0x20
#define CIRRUS_BUSTYPE_VLBSLOW		0x30
#define CIRRUS_BUSTYPE_ISA		0x38
#define CIRRUS_MMIO_ENABLE		0x04
#define CIRRUS_MMIO_USE_PCIADDR		0x40	/* 0xb8000 if cleared. */
#define CIRRUS_MEMSIZEEXT_DOUBLE	0x80

/* control 0x0b */
#define CIRRUS_BANKING_DUAL             0x01
#define CIRRUS_BANKING_GRANULARITY_16K  0x20	/* set:16k, clear:4k */

/* control 0x30 */
#define CIRRUS_BLTMODE_BACKWARDS	0x01
#define CIRRUS_BLTMODE_MEMSYSDEST	0x02
#define CIRRUS_BLTMODE_MEMSYSSRC	0x04
#define CIRRUS_BLTMODE_TRANSPARENTCOMP	0x08
#define CIRRUS_BLTMODE_PATTERNCOPY	0x40
#define CIRRUS_BLTMODE_COLOREXPAND	0x80
#define CIRRUS_BLTMODE_PIXELWIDTHMASK	0x30
#define CIRRUS_BLTMODE_PIXELWIDTH8	0x00
#define CIRRUS_BLTMODE_PIXELWIDTH16	0x10
#define CIRRUS_BLTMODE_PIXELWIDTH24	0x20
#define CIRRUS_BLTMODE_PIXELWIDTH32	0x30

/* control 0x31 */
#define CIRRUS_BLT_BUSY                 0x01
#define CIRRUS_BLT_START                0x02
#define CIRRUS_BLT_RESET                0x04
#define CIRRUS_BLT_FIFOUSED             0x10
#define CIRRUS_BLT_AUTOSTART            0x80

/* control 0x33 */
#define CIRRUS_BLTMODEEXT_SOLIDFILL        0x04
#define CIRRUS_BLTMODEEXT_COLOREXPINV      0x02
#define CIRRUS_BLTMODEEXT_DWORDGRANULARITY 0x01

#define CL_GD5429_SYSTEM_BUS_VESA	5
#define CL_GD5429_SYSTEM_BUS_ISA	7

#define CL_GD543X_SYSTEM_BUS_PCI	4
#define CL_GD543X_SYSTEM_BUS_VESA	6
#define CL_GD543X_SYSTEM_BUS_ISA	7


typedef struct {
    mem_map_t		mmio_mapping;
    mem_map_t	 	linear_mapping;

    svga_t		svga;

    int			has_bios,
			rev;

    rom_t		bios_rom;

    uint32_t		vram_size;
    uint32_t		vram_mask;

    uint8_t		vclk_n[4];
    uint8_t		vclk_d[4];        
    uint32_t		bank[2];

    struct {
	uint8_t	state;
	int	ctrl;
    }			ramdac;		

    struct {
	uint32_t	fg_col, bg_col;
	uint16_t	width, height;
	uint16_t	dst_pitch, src_pitch;               
	uint32_t	dst_addr, src_addr;
	uint8_t		mask, mode, rop;
	uint8_t		modeext;
	uint8_t		status;
	uint16_t	trans_col, trans_mask;

	uint32_t	dst_addr_backup, src_addr_backup;
	uint16_t	width_backup, height_internal;

	int		x_count, y_count;
	int		sys_tx;
	uint8_t		sys_cnt;
	uint32_t	sys_buf;
	uint16_t	pixel_cnt;
	uint16_t	scan_cnt;
    }			blt;

    int			pci, vlb;	 

    uint8_t		pci_regs[256];
    uint8_t		int_line;

    /* FIXME: move to SVGA?  --FvK */
    uint8_t		fc;			/* Feature Connector */

    uint8_t		clgd_latch[8];

    int			card;

    uint32_t		lfb_base;

    int			mmio_vram_overlap;

    uint32_t		extpallook[256];
    PALETTE		extpal;
} gd54xx_t;


static void	gd543x_mmio_write(uint32_t addr, uint8_t val, priv_t);
static void	gd543x_mmio_writew(uint32_t addr, uint16_t val, priv_t);
static void	gd543x_mmio_writel(uint32_t addr, uint32_t val, priv_t);
static uint8_t	gd543x_mmio_read(uint32_t addr, priv_t);
static uint16_t	gd543x_mmio_readw(uint32_t addr, priv_t);
static uint32_t	gd543x_mmio_readl(uint32_t addr, priv_t);
static void	gd54xx_start_blit(uint32_t cpu_dat, int count,
				  gd54xx_t *gd54xx, svga_t *svga);


/* Returns 1 if the card is a 5422+ */
static int
is_5422(svga_t *svga)
{
    if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5422)
	return 1;

    return 0;
}


/* Returns 1 if the card is a 5434, 5436/46, or 5480. */
static int
is_5434(svga_t *svga)
{
    if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5434)
	return 1;

    return 0;
}


static void
recalc_banking(gd54xx_t *dev)
{
    svga_t *svga = &dev->svga;

    if (! is_5422(svga)) {
	dev->bank[0] = (svga->gdcreg[0x09] & 0x7f) << 12;

	if (svga->gdcreg[0x0b] & CIRRUS_BANKING_DUAL)
		dev->bank[1] = (svga->gdcreg[0x0a] & 0x7f) << 12;
	else
		dev->bank[1] = dev->bank[0] + 0x8000;
    } else {
	if (svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K)
		dev->bank[0] = svga->gdcreg[0x09] << 14;
	else
		dev->bank[0] = svga->gdcreg[0x09] << 12;

	if (svga->gdcreg[0x0b] & CIRRUS_BANKING_DUAL) {
		if (svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K)
			dev->bank[1] = svga->gdcreg[0x0a] << 14;
		else
			dev->bank[1] = svga->gdcreg[0x0a] << 12;
	} else
		dev->bank[1] = dev->bank[0] + 0x8000;
    }
}


static void 
recalc_mapping(gd54xx_t *dev)
{
    svga_t *svga = &dev->svga;
    uint32_t base, size;
        
    if (! (dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM)) {
	mem_map_disable(&svga->mapping);
	mem_map_disable(&dev->linear_mapping);
	mem_map_disable(&dev->mmio_mapping);
	return;
    }

    dev->mmio_vram_overlap = 0;

    if (! (svga->seqregs[7] & 0xf0)) {
	mem_map_disable(&dev->linear_mapping);

	switch (svga->gdcreg[6] & 0x0c) {
		case 0x00:	/*128K at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x20000);
			svga->banked_mask = 0x1ffff;
			break;

		case 0x04:	/*64K at A0000*/
			mem_map_set_addr(&svga->mapping, 0xa0000, 0x10000);
			svga->banked_mask = 0xffff;
			break;

		case 0x08:	/*32K at B0000*/
			mem_map_set_addr(&svga->mapping, 0xb0000, 0x08000);
			svga->banked_mask = 0x7fff;
			break;

		case 0x0c:	/*32K at B8000*/
			mem_map_set_addr(&svga->mapping, 0xb8000, 0x08000);
			svga->banked_mask = 0x7fff;
			dev->mmio_vram_overlap = 1;
			break;
	}

	if (svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE)
		mem_map_set_addr(&dev->mmio_mapping, 0xb8000, 0x00100);
	else
		mem_map_disable(&dev->mmio_mapping);
    } else {
	if (svga->crtc[0x27] <= CIRRUS_ID_CLGD5429 ||
	    (!dev->pci && !dev->vlb)) {
		if (svga->gdcreg[0x0b] & CIRRUS_BANKING_GRANULARITY_16K) {
			base = (svga->seqregs[7] & 0xf0) << 16;
			size = 1 * 1024 * 1024;
		} else {
			base = (svga->seqregs[7] & 0xe0) << 16;
			size = 2 * 1024 * 1024;
		}
	} else if (dev->pci) {
		base = dev->lfb_base;

		if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
			size = 16 * 1024 * 1024;
		else
			size = 4 * 1024 * 1024;
	} else { /*VLB*/
		base = 128 * 1024 * 1024;
		if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
			size = 16 * 1024 * 1024;
		else
			size = 4 * 1024 * 1024;
	}

	mem_map_disable(&svga->mapping);
	mem_map_set_addr(&dev->linear_mapping, base, size);

	if (svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) {
		if (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR) {
			if (size >= (4 * 1024 * 1024))
				mem_map_disable(&dev->mmio_mapping); /* MMIO is handled in the linear read/write functions */
			else {
				mem_map_set_addr(&dev->linear_mapping, base, size - 256);
				mem_map_set_addr(&dev->mmio_mapping, base + size - 256, 0x00100);
			}
		} else
			mem_map_set_addr(&dev->mmio_mapping, 0xb8000, 0x00100);
	} else
		mem_map_disable(&dev->mmio_mapping);
    }    
}


static void
recalc_timings(svga_t *svga)
{
    gd54xx_t *dev = (gd54xx_t *)svga->p;	
    uint8_t clocksel;

    svga->rowoffset = (svga->crtc[0x13]) | ((svga->crtc[0x1b] & 0x10) << 4);

    svga->interlace = (svga->crtc[0x1a] & 0x01);

    if (svga->seqregs[7] & CIRRUS_SR7_BPP_SVGA)
	svga->render = svga_render_8bpp_highres;
    else if (svga->gdcreg[5] & 0x40)
	svga->render = svga_render_8bpp_lowres;

    svga->ma_latch |= ((svga->crtc[0x1b] & 0x01) << 16) | ((svga->crtc[0x1b] & 0xc) << 15);

    svga->bpp = 8;

    if (dev->ramdac.ctrl & 0x80)  { /* Extended Color Mode enabled */
	if (dev->ramdac.ctrl & 0x40) { /* Enable all Extended modes */
		switch (dev->ramdac.ctrl & 0xf) {
			case 0:
				if (dev->ramdac.ctrl & 0x10) { /* Mixed Mode */
					svga->render = svga_render_mixed_highres;
				} else {
					svga->bpp = 15;
					svga->render = svga_render_15bpp_highres;
				}
				break;

			case 1:
				svga->bpp = 16;
				svga->render = svga_render_16bpp_highres;
				break;

			case 5:
				if (is_5434(svga) && (svga->seqregs[7] & CIRRUS_SR7_BPP_32)) {
					svga->bpp = 32;
					svga->render = svga_render_32bpp_highres;
					if (svga->crtc[0x27] < CIRRUS_ID_CLGD5436)
						svga->rowoffset *= 2;
				} else {
					svga->bpp = 24;
					svga->render = svga_render_24bpp_highres;
				}
				break;

#if 0
			case 8:		/* Todo : Grayscale VGA rendering*/
				break;
			
			case 9:		/* Todo : 3-3-2 8bit RGB*/
				break;
#endif

			case 0xf:
				switch (svga->seqregs[7] & CIRRUS_SR7_BPP_MASK) {
					case CIRRUS_SR7_BPP_32:
						svga->bpp = 32;
						svga->render = svga_render_32bpp_highres;
						svga->rowoffset *= 2;
						break;

					case CIRRUS_SR7_BPP_24:
						svga->bpp = 24;
						svga->render = svga_render_24bpp_highres;
						break;

					case CIRRUS_SR7_BPP_16:
					case CIRRUS_SR7_BPP_16_DOUBLEVCLK:
						svga->bpp = 16;
						svga->render = svga_render_16bpp_highres;
						break;

					case CIRRUS_SR7_BPP_8:
						svga->bpp = 8;
						svga->render = svga_render_8bpp_highres;
						break;
				}
				break;
		}
	} else {
		/* Default to 5-5-5 Sierra Mode */
		svga->bpp = 15;
		svga->render = svga_render_15bpp_highres;
	}
    }

    clocksel = (svga->miscout >> 2) & 3;

    if (!dev->vclk_n[clocksel] || !dev->vclk_d[clocksel])
	svga->clock = cpuclock / ((svga->miscout & 0xc) ? 28322000.0 : 25175000.0);
    else {
	int n = dev->vclk_n[clocksel] & 0x7f;
	int d = (dev->vclk_d[clocksel] & 0x3e) >> 1;
	int m = dev->vclk_d[clocksel] & 0x01 ? 2 : 1;
	float freq = ((float)14318184.0 * ((float)n / ((float)d * m)));
	if (is_5422(svga)) {
		switch (svga->seqregs[7] & (is_5434(svga) ? 0xe : 6)) {
			case 2:
				freq /= 2.0;
				break;

			case 4:
				if (! is_5434(svga))
					freq /= 3.0; 
				break;
		}
	}

	svga->clock = cpuclock / freq;
    }

    if (dev->vram_size == (1 << 19)) /* Note : why 512KB VRAM cards does not wrap */
	svga->vram_display_mask = dev->vram_mask;
    else
	svga->vram_display_mask = (svga->crtc[0x1b] & 2) ? dev->vram_mask : 0x3ffff;
}


static void
gd54xx_out(uint16_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t o, indx, old;
    uint32_t o32;
    int c;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03ba:
	case 0x03da:
		dev->fc = val;		//FIXME: move to SVGA? --FvK
		break;

	case 0x03c0:
	case 0x03c1:
		if (! svga->attrff) {
			svga->attraddr = val & 0x3f;
			if ((val & 0x20) != svga->attr_palette_enable) {
				svga->fullchange = 3;
				svga->attr_palette_enable = val & 0x20;
				svga_recalctimings(svga);
			}
		} else {
			o = svga->attrregs[svga->attraddr & 0x3f];
			if ((svga->attraddr < 0x20) || (svga->attraddr >= 0x30))
				svga->attrregs[svga->attraddr & 0x3f] = val;
			if (svga->attraddr < 0x10) 
				svga->fullchange = changeframecount;
			if (svga->attraddr == 0x10 || svga->attraddr == 0x14 || svga->attraddr < 0x10) {
				for (c = 0; c < 16; c++) {
					if (svga->attrregs[0x10] & 0x80) svga->egapal[c] = (svga->attrregs[c] &  0xf) | ((svga->attrregs[0x14] & 0xf) << 4);
					else                             svga->egapal[c] = (svga->attrregs[c] & 0x3f) | ((svga->attrregs[0x14] & 0xc) << 4);
				}
			}

			/* Recalculate timings on change of attribute register 0x11 (overscan border color) too. */
			if (svga->attraddr == 0x10) {
				if (o != val)
					svga_recalctimings(svga);
			} else if (svga->attraddr == 0x11) {
				if (! (svga->seqregs[0x12] & 0x80)) {
					svga->overscan_color = svga->pallook[svga->attrregs[0x11]];
					if (o != val)  svga_recalctimings(svga);
				}
			} else if (svga->attraddr == 0x12) {
				if ((val & 0xf) != svga->plane_mask)
					svga->fullchange = changeframecount;
				svga->plane_mask = val & 0xf;
			}
		}
		svga->attrff ^= 1;
                return;

	case 0x03c4:
		svga->seqaddr = val;
		break;

	case 0x03c5:
		if (svga->seqaddr > 5) {
			o = svga->seqregs[svga->seqaddr & 0x1f];
			svga->seqregs[svga->seqaddr & 0x1f] = val;
			switch (svga->seqaddr) {
				case 0x06:
					val &= 0x17;
					if (val == 0x12)
						svga->seqregs[6] = 0x12;
					else
						svga->seqregs[6] = 0x0f;
					break;

				case 0x08:	// Todo EEPROM
					break;
					
				case 0x0a:
					if (is_5434(svga)) {
						svga->seqregs[0x0a] = val;
					} else {
						/* Hack to force memory size on some GD-542x BIOSes*/
						val &= 0xe7;
						switch (dev->vram_size) {
							case 0x080000:
								svga->seqregs[0x0a] = val | 0x08;
								break;

							case 0x100000:
								svga->seqregs[0x0a] = val | 0x10;
								break;

							case 0x200000:
								svga->seqregs[0x0a] = val | 0x18;
								break;
						}
					}
					break;

				case 0x0b: case 0x0c: case 0x0d: case 0x0e: /* VCLK stuff */
					dev->vclk_n[svga->seqaddr - 0x0b] = val;
					break;

				case 0x1b: case 0x1c: case 0x1d: case 0x1e: /* VCLK stuff */
					dev->vclk_d[svga->seqaddr-0x1b] = val;
					break;

				case 0x10: case 0x30: case 0x50: case 0x70:
				case 0x90: case 0xb0: case 0xd0: case 0xf0:
					svga->hwcursor.x = (val << 3) | (svga->seqaddr >> 5);
					break;

				case 0x11: case 0x31: case 0x51: case 0x71:
				case 0x91: case 0xb1: case 0xd1: case 0xf1:
					svga->hwcursor.y = (val << 3) | (svga->seqaddr >> 5);
					break;

				case 0x12:
					svga->ext_overscan = !!(val & 0x80);
					if (svga->ext_overscan)
						svga->overscan_color = dev->extpallook[2];
					else
						svga->overscan_color = svga->pallook[svga->attrregs[0x11]];
					svga_recalctimings(svga);
					svga->hwcursor.ena = val & CIRRUS_CURSOR_SHOW;
					svga->hwcursor.xsize = svga->hwcursor.ysize = (val & CIRRUS_CURSOR_LARGE) ? 64 : 32;
					svga->hwcursor.yoff = (svga->hwcursor.ysize == 32) ? 32 : 0;
					if (val & CIRRUS_CURSOR_LARGE)
						svga->hwcursor.addr = (((svga->vram_display_mask + 1) - 0x4000) + ((svga->seqregs[0x13] & 0x3c) * 256));
					else
						svga->hwcursor.addr = (((svga->vram_display_mask + 1) - 0x4000) + ((svga->seqregs[0x13] & 0x3f) * 256));
					break;

				case 0x13:
					if (svga->seqregs[0x12] & CIRRUS_CURSOR_LARGE)
						svga->hwcursor.addr = (((svga->vram_display_mask + 1) - 0x4000) + ((val & 0x3c) * 256));
					else
						svga->hwcursor.addr = (((svga->vram_display_mask + 1) - 0x4000) + ((val & 0x3f) * 256));
					break;

				case 0x15:
					if (is_5434(svga)) {
						/* Hack to force memory size on some GD-543x BIOSes*/
						val &= 0xf8;
						switch (dev->vram_size) {				
							case 0x100000:
								svga->seqregs[0x15] = val | 0x2;
								break;

							case 0x200000:
								svga->seqregs[0x15] = val | 0x3;
								break;

							case 0x400000:
								svga->seqregs[0x15] = val | 0x4;
								break;
						}
					} else
						return;
					break;

				case 0x07:
					if (! is_5422(svga)) {
						svga->seqregs[svga->seqaddr] &= 0x0f;
						recalc_mapping(dev);
					}
					if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5429)
						svga->set_reset_disabled = svga->seqregs[7] & 1;
					break;

				case 0x17:
					if (is_5422(svga))
						recalc_mapping(dev);
					else
						return;
					break;
			}
			return;
		}
		break;

	case 0x03c6:
		if (dev->ramdac.state == 4) {
			dev->ramdac.state = 0;
			dev->ramdac.ctrl = val;
			svga_recalctimings(svga);
			return;
		}
		dev->ramdac.state = 0;
		break;

	case 0x03c9:
		svga->dac_status = 0;
		svga->fullchange = changeframecount;
		switch (svga->dac_pos) {
			case 0:
				svga->dac_r = val;
				svga->dac_pos++; 
				break;

			case 1:
				svga->dac_g = val;
				svga->dac_pos++; 
				break;

                        case 2:
				indx = svga->dac_addr & 0xff;
				if (svga->seqregs[0x12] & 2) {
					indx &= 0x0f;
					dev->extpal[indx].r = svga->dac_r;
					dev->extpal[indx].g = svga->dac_g;
					dev->extpal[indx].b = val; 
					dev->extpallook[indx] = makecol32(video_6to8[dev->extpal[indx].r & 0x3f], video_6to8[dev->extpal[indx].g & 0x3f], video_6to8[dev->extpal[indx].b & 0x3f]);
					if (svga->ext_overscan && (indx == 2)) {
						o32 = svga->overscan_color;
						svga->overscan_color = dev->extpallook[2];
						if (o32 != svga->overscan_color)
							svga_recalctimings(svga);
					}
				} else {
					svga->vgapal[indx].r = svga->dac_r;
					svga->vgapal[indx].g = svga->dac_g;
					svga->vgapal[indx].b = val; 
					svga->pallook[indx] = makecol32(video_6to8[svga->vgapal[indx].r & 0x3f], video_6to8[svga->vgapal[indx].g & 0x3f], video_6to8[svga->vgapal[indx].b & 0x3f]);
				}
				svga->dac_addr = (svga->dac_addr + 1) & 255;
				svga->dac_pos = 0; 
                        	break;
                }
                return;

	case 0x03cf:
		if (svga->gdcaddr == 0)
			gd543x_mmio_write(0xb8000, val, dev);
		if (svga->gdcaddr == 1)
			gd543x_mmio_write(0xb8004, val, dev);
	
		if (svga->gdcaddr == 5) {
			svga->gdcreg[5] = val;
			if (svga->gdcreg[0xb] & 0x04)
				svga->writemode = svga->gdcreg[5] & 7;
			else
				svga->writemode = svga->gdcreg[5] & 3;
			svga->readmode = val & 8;
			svga->chain2_read = val & 0x10;
			return;
		}

		if (svga->gdcaddr == 6) {
			if ((svga->gdcreg[6] & 0x0c) != (val & 0x0c)) {
				svga->gdcreg[6] = val;
				recalc_mapping(dev);
			}
			svga->gdcreg[6] = val;
			return;
		}

		if (svga->gdcaddr > 8) {
			svga->gdcreg[svga->gdcaddr & 0x3f] = val;
			switch (svga->gdcaddr) {
				case 0x09: case 0x0a: case 0x0b:
					recalc_banking(dev);
					if (svga->gdcreg[0xb] & 0x04)
						svga->writemode = svga->gdcreg[5] & 7;
					else
						svga->writemode = svga->gdcreg[5] & 3;
					break;
					
				case 0x10:
					gd543x_mmio_write(0xb8001, val, dev);
					break;

				case 0x11:
					gd543x_mmio_write(0xb8005, val, dev);
					break;

				case 0x12:
					gd543x_mmio_write(0xb8002, val, dev);
					break;

				case 0x13:
					gd543x_mmio_write(0xb8006, val, dev);
					break;

				case 0x14:
					gd543x_mmio_write(0xb8003, val, dev);
					break;

				case 0x15:
					gd543x_mmio_write(0xb8007, val, dev);
					break;
					
				case 0x20:
					gd543x_mmio_write(0xb8008, val, dev);
					break;

				case 0x21:
					gd543x_mmio_write(0xb8009, val, dev);
					break;

				case 0x22:
					gd543x_mmio_write(0xb800a, val, dev);
					break;

				case 0x23:
					gd543x_mmio_write(0xb800b, val, dev);
					break;

				case 0x24:
					gd543x_mmio_write(0xb800c, val, dev);
					break;

				case 0x25:
					gd543x_mmio_write(0xb800d, val, dev);
					break;

				case 0x26:
					gd543x_mmio_write(0xb800e, val, dev);
					break;

				case 0x27:
					gd543x_mmio_write(0xb800f, val, dev);
					break;

				case 0x28:
					gd543x_mmio_write(0xb8010, val, dev);
					break;

				case 0x29:
					gd543x_mmio_write(0xb8011, val, dev);
					break;

				case 0x2a:
					gd543x_mmio_write(0xb8012, val, dev);
					break;

				case 0x2c:
					gd543x_mmio_write(0xb8014, val, dev);
					break;

				case 0x2d:
					gd543x_mmio_write(0xb8015, val, dev);
					break;

				case 0x2e:
					gd543x_mmio_write(0xb8016, val, dev);
					break;

				case 0x2f:
					gd543x_mmio_write(0xb8017, val, dev);
					break;

				case 0x30:
					gd543x_mmio_write(0xb8018, val, dev);
					break;
	
				case 0x32:
					gd543x_mmio_write(0xb801a, val, dev);
					break;

				case 0x33:
					gd543x_mmio_write(0xb801b, val, dev);
					break;
					
				case 0x31:
					gd543x_mmio_write(0xb8040, val, dev);
					break;
					
				case 0x34:
					gd543x_mmio_write(0xb801c, val, dev);
					break;
					
				case 0x35:
					gd543x_mmio_write(0xb801d, val, dev);
					break;

				case 0x38:
					gd543x_mmio_write(0xb8020, val, dev);
					break;

				case 0x39:
					gd543x_mmio_write(0xb8021, val, dev);
					break;
			}
			return;
		}
		break;

	case 0x03d4:
		svga->crtcreg = val & 0x3f;
		return;

	case 0x03d5:
		if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
			return;
		if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		if ((svga->crtcreg == 0x25) || (svga->crtcreg == 0x27))  
			return;

		old = svga->crtc[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;
		
		if (old != val) {
			if (svga->crtcreg < 0xe || svga->crtcreg > 0x10) {
				svga->fullchange = changeframecount;
				svga_recalctimings(svga);
			}
		}
		break;
    }

    svga_out(addr, val, svga);
}


static uint8_t
gd54xx_in(uint16_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
    uint8_t indx, temp;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3d0) &&
	!(svga->miscout & 1)) addr ^= 0x60;

    switch (addr) {
	case 0x03c4:
		if ((svga->seqregs[6] & 0x17) == 0x12) {
			temp = svga->seqaddr;
			if ((temp & 0x1e) == 0x10) {
				if (temp & 1)
					temp = ((svga->hwcursor.y & 7) << 5) | 0x11;
				else
					temp = ((svga->hwcursor.x & 7) << 5) | 0x10;
			}
			return temp;
		}
		return svga->seqaddr;
	
	case 0x03c5:
		if (svga->seqaddr > 5) {
			switch (svga->seqaddr) {
				case 0x06:
					return ((svga->seqregs[6] & 0x17) == 0x12) ? 0x12 : 0x0f;

				case 0x0b: case 0x0c: case 0x0d: case 0x0e:
					return dev->vclk_n[svga->seqaddr-0x0b];

				case 0x17:
					temp = svga->gdcreg[0x17] & ~(7 << 3);
					if (svga->crtc[0x27] <= CIRRUS_ID_CLGD5429) {
						if (dev->vlb)
							temp |= (CL_GD5429_SYSTEM_BUS_VESA << 3);
						else
							temp |= (CL_GD5429_SYSTEM_BUS_ISA << 3);
					} else {
						if (dev->pci)
							temp |= (CL_GD543X_SYSTEM_BUS_PCI << 3);
						else if (dev->vlb)
							temp |= (CL_GD543X_SYSTEM_BUS_VESA << 3);
						else
							temp |= (CL_GD543X_SYSTEM_BUS_ISA << 3);
					}
					return temp;

				case 0x1b: case 0x1c: case 0x1d: case 0x1e:
					return dev->vclk_d[svga->seqaddr-0x1b];
			}
			return svga->seqregs[svga->seqaddr & 0x3f];
		}
		break;

	case 0x03c6:
		if (dev->ramdac.state == 4) {
			dev->ramdac.state = 0;
			return dev->ramdac.ctrl;
		}
		dev->ramdac.state++;
		break;
		
	case 0x03ca:
		return dev->fc;		//FIXME: move to SVGA? --FvK
		
	case 0x03c9:
		svga->dac_status = 3;
		indx = (svga->dac_addr - 1) & 0xff;
		if (svga->seqregs[0x12] & 2)
			indx &= 0x0f;

		switch (svga->dac_pos) {
			case 0:
				svga->dac_pos++;
				if (svga->seqregs[0x12] & 2)
					return dev->extpal[indx].r & 0x3f;
				return svga->vgapal[indx].r & 0x3f;

			case 1: 
				svga->dac_pos++;
				if (svga->seqregs[0x12] & 2)
					return dev->extpal[indx].g & 0x3f;
				return svga->vgapal[indx].g & 0x3f;

                        case 2: 
				svga->dac_pos = 0;
				svga->dac_addr = (svga->dac_addr + 1) & 255;
				if (svga->seqregs[0x12] & 2)
                        		return dev->extpal[indx].b & 0x3f;
                        	return svga->vgapal[indx].b & 0x3f;
                }
                return 0xff;

	case 0x03cf:
		if (svga->gdcaddr > 8)
			return svga->gdcreg[svga->gdcaddr & 0x3f];
		break;

	case 0x03d4:
		return svga->crtcreg;

	case 0x03d5:
		switch (svga->crtcreg) {
			case 0x24: /*Attribute controller toggle readback (R)*/
				return svga->attrff << 7;

			case 0x26: /*Attribute controller index readback (R)*/
				return svga->attraddr & 0x3f;	

			case 0x27: /*ID*/
				return svga->crtc[0x27]; /*GD542x/GD543x*/

			case 0x28: /*Class ID*/
				if ((svga->crtc[0x27] == CIRRUS_ID_CLGD5430) || (svga->crtc[0x27] == CIRRUS_ID_CLGD5440))
					return 0xff; /*Standard CL-GD5430/40*/
				break;
		}
		return svga->crtc[svga->crtcreg];
    }

    return svga_in(addr, svga);
}


static void
hwcursor_draw(svga_t *svga, int displine)
{
    gd54xx_t *dev = (gd54xx_t *)svga->p;	
    int x, xx, comb, b0, b1;
    uint8_t dat[2];
    int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
    int y_add, x_add;
    int pitch = (svga->hwcursor.xsize == 64) ? 16 : 4;
    uint32_t bgcol = dev->extpallook[0x00];
    uint32_t fgcol = dev->extpallook[0x0f];

    y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
    x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;

    if (svga->interlace && svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;

    for (x = 0; x < svga->hwcursor.xsize; x += 8) {
	dat[0] = svga->vram[svga->hwcursor_latch.addr];
	if (svga->hwcursor.xsize == 64)
		dat[1] = svga->vram[svga->hwcursor_latch.addr + 0x08];
	else
		dat[1] = svga->vram[svga->hwcursor_latch.addr + 0x80];
	for (xx = 0; xx < 8; xx++) {
		b0 = (dat[0] >> (7 - xx)) & 1;
		b1 = (dat[1] >> (7 - xx)) & 1;
		comb = (b1 | (b0 << 1));

		if (offset >= svga->hwcursor_latch.x) {
			switch(comb) {
				case 0:
					/* The original screen pixel is shown (invisible cursor) */
					break;

				case 1:
					/* The pixel is shown in the cursor background color */
					screen->line[displine + y_add][offset + 32 + x_add].val = bgcol;
					break;

				case 2:
					/* The pixel is shown as the inverse of the original screen pixel
					   (XOR cursor) */
					screen->line[displine + y_add][offset + 32 + x_add].val ^= 0xffffff;
					break;

				case 3:
					/* The pixel is shown in the cursor foreground color */
					screen->line[displine + y_add][offset + 32 + x_add].val = fgcol;
					break;
			}
		}
		   
		offset++;
	}

	svga->hwcursor_latch.addr++;
    }

    if (svga->hwcursor.xsize == 64)
	svga->hwcursor_latch.addr += 8;

    if (svga->interlace && !svga->hwcursor_oddeven)
	svga->hwcursor_latch.addr += pitch;
}


static void
gd54xx_rop(gd54xx_t *dev, uint8_t *res, uint8_t *dst, const uint8_t *src)
{
    switch (dev->blt.rop) {
	case 0x00: *res =   0x00;          break;
	case 0x05: *res =   *src &  *dst;  break;
	case 0x06: *res =   *dst;          break;
	case 0x09: *res =   *src & ~*dst;  break;
	case 0x0b: *res =  ~*dst;          break;
	case 0x0d: *res =   *src;          break;
	case 0x0e: *res =   0xff;          break;
	case 0x50: *res =  ~*src &  *dst;  break;
	case 0x59: *res =   *src ^  *dst;  break;
	case 0x6d: *res =   *src |  *dst;  break;
	case 0x90: *res = ~(*src |  *dst); break;
	case 0x95: *res = ~(*src ^  *dst); break;
	case 0xad: *res =   *src | ~*dst;  break;
	case 0xd0: *res =  ~*src;          break;
	case 0xd6: *res =  ~*src |  *dst;  break;
	case 0xda: *res = ~(*src &  *dst); break;
    }
}


static void
memsrc_rop(gd54xx_t *dev, svga_t *svga, uint8_t src, uint8_t dst)
{
    uint8_t res = src;

    svga->changedvram[(dev->blt.dst_addr_backup & svga->vram_mask) >> 12] = changeframecount;

    gd54xx_rop(dev, &res, &dst, (const uint8_t *) &src);

    /* Handle transparency compare. */
    if (dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP) {
	/* TODO: 16-bit compare */
	/* if ROP result matches the transparency color, don't change the pixel */
	if ((res & (~dev->blt.trans_mask & 0xff)) == ((dev->blt.trans_col & 0xff) & (~dev->blt.trans_mask & 0xff)))
		return;
    }

    svga->vram[dev->blt.dst_addr_backup & svga->vram_mask] = res;	
}


/* non color-expanded BitBLTs from system memory must be doubleword sized, extra bytes are ignored */
static void 
blit_dword(gd54xx_t *dev, svga_t *svga)
{
    /* TODO: add support for reverse direction */
    uint8_t x, pixel;

    for (x = 0; x < 32; x += 8) {
	pixel = ((dev->blt.sys_buf & (0xff << x)) >> x);
	if (dev->blt.pixel_cnt <= dev->blt.width)
		memsrc_rop(dev, svga, pixel,
			  svga->vram[dev->blt.dst_addr_backup & svga->vram_mask]);
	dev->blt.dst_addr_backup++;
	dev->blt.pixel_cnt++;
    }

    if (dev->blt.pixel_cnt > dev->blt.width) {
	dev->blt.pixel_cnt = 0;
	dev->blt.scan_cnt++;
	dev->blt.dst_addr_backup = dev->blt.dst_addr + (dev->blt.dst_pitch*dev->blt.scan_cnt);
    }

    if (dev->blt.scan_cnt > dev->blt.height) {
	dev->blt.sys_tx = 0;  /*  BitBLT complete */
	recalc_mapping(dev);
    }
}


static void
blit_write_w(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    gd54xx_start_blit(val, 16, dev, &dev->svga);
}


static void
blit_write_l(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if ((dev->blt.mode & (CIRRUS_BLTMODE_MEMSYSSRC|CIRRUS_BLTMODE_COLOREXPAND)) == (CIRRUS_BLTMODE_MEMSYSSRC|CIRRUS_BLTMODE_COLOREXPAND)) {
	gd54xx_start_blit(val & 0xff, 8, dev, &dev->svga);
	gd54xx_start_blit((val>>8) & 0xff, 8, dev, &dev->svga);
	gd54xx_start_blit((val>>16) & 0xff, 8, dev, &dev->svga);
	gd54xx_start_blit((val>>24) & 0xff, 8, dev, &dev->svga);
    } else
	gd54xx_start_blit(val, 32, dev, &dev->svga);
}


static void
gd54xx_write(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;	

	if (svga->gdcreg[0x0b] & 1) {
		svga->write_bank = dev->bank[(addr >> 15) & 1];
		addr = addr & 0x7fff;
	}
	else
		svga->write_bank = dev->bank[0];

    if ((svga->seqregs[0x07] & 0x01) == 0) {
	svga_write(addr, val, svga);
	return;
    }

    if (dev->blt.sys_tx) {
	if (dev->blt.mode == CIRRUS_BLTMODE_MEMSYSSRC) {
		dev->blt.sys_buf &= ~(0xff << (dev->blt.sys_cnt * 8));
		dev->blt.sys_buf |= (val << (dev->blt.sys_cnt * 8));
		dev->blt.sys_cnt++;
		if (dev->blt.sys_cnt >= 4) {
			blit_dword(dev, svga);
			dev->blt.sys_cnt = 0;
		}
	}
	return;
    }

    addr &= svga->banked_mask;
    if (svga->banked_mask != 0x1ffff)
	addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    svga_write_linear(addr, val, svga);
}


static void 
gd54xx_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
	
    if (svga->gdcreg[0x0b] & 1) {
	svga->write_bank = dev->bank[(addr >> 15) & 1];
	addr = addr & 0x7fff;
    } else
	svga->write_bank = dev->bank[0];
	
    if ((svga->seqregs[0x07] & 0x01) == 0) {
	svga_writew(addr, val, svga);
	return;
    }

    if (dev->blt.sys_tx) {
	gd54xx_write(addr, val & 0xff, dev);
	gd54xx_write(addr+1, val >> 8, dev);
	return;
    }

    addr &= svga->banked_mask;
    if (svga->banked_mask != 0x1ffff)
	addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    if (svga->writemode < 4)	
    	svga_writew_linear(addr, val, svga);
    else {
	svga_write_linear(addr, val & 0xff, svga);
	svga_write_linear(addr + 1, val >> 8, svga);
    }
}


static void
gd54xx_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((svga->seqregs[0x07] & 0x01) == 0) {
	svga_writel(addr, val, svga);
	return;
    }

    if (dev->blt.sys_tx) {
	gd54xx_write(addr, val, dev);
	gd54xx_write(addr+1, val >> 8, dev);
	gd54xx_write(addr+2, val >> 16, dev);
	gd54xx_write(addr+3, val >> 24, dev);
	return;
    }

    addr &= svga->banked_mask;
    if (svga->banked_mask != 0x1ffff)
	addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    if (svga->writemode < 4)
	svga_writel_linear(addr, val, svga);
    else {
	svga_write_linear(addr, val & 0xff, svga);
	svga_write_linear(addr+1, val >> 8, svga);
	svga_write_linear(addr+2, val >> 16, svga);
	svga_write_linear(addr+3, val >> 24, svga);
    }
}


/* This adds write modes 4 and 5 to SVGA. */
static void
gd54xx_write_modes45(svga_t *svga, uint8_t val, uint32_t addr)
{
    uint32_t i, j;

    switch (svga->writemode) {
	case 4:
		if (svga->gdcreg[0xb] & 0x10) {
			addr <<= 2;

			for (i = 0; i < 8; i++) {
				if (val & svga->seqregs[2] & (0x80 >> i)) {
					svga->vram[addr + (i << 1)] = svga->gdcreg[1];
					svga->vram[addr + (i << 1) + 1] = svga->gdcreg[0x11];
				}
			}
		} else {
			addr <<= 1;

			for (i = 0; i < 8; i++) {
				if (val & svga->seqregs[2] & (0x80 >> i))
					svga->vram[addr + i] = svga->gdcreg[1];
			}
		}
		break;

	case 5:
		if (svga->gdcreg[0xb] & 0x10) {
			addr <<= 2;

			for (i = 0; i < 8; i++) {
				j = (0x80 >> i);
				if (svga->seqregs[2] & j) {
					svga->vram[addr + (i << 1)] = (val & j) ?
								      svga->gdcreg[1] : svga->gdcreg[0];
					svga->vram[addr + (i << 1) + 1] = (val & j) ?
									  svga->gdcreg[0x11] : svga->gdcreg[0x10];
				}
			}
		} else {
			addr <<= 1;

			for (i = 0; i < 8; i++) {
				j = (0x80 >> i);
				if (svga->seqregs[2] & j)
					svga->vram[addr + i] = (val & j) ? svga->gdcreg[1] : svga->gdcreg[0];
			}
		}
		break;
    }

    svga->changedvram[addr >> 12] = changeframecount;
}


static uint8_t
gd54xx_get_aperture(uint32_t addr)
{
    uint32_t ap = addr >> 22;

    return (uint8_t) (ap & 0x03);
}


static uint8_t
gd54xx_readb_linear(uint32_t addr, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    addr &= 0x003fffff;	/* 4 MB mask */

    switch (ap) {
	case 0:
	default:		
		break;

	case 1:
		/* 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2 */
		addr ^= 0x00000001;
		break;

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000003;
		break;

	case 3:
		return 0xff;
    }

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR))
		return gd543x_mmio_read(addr & 0x000000ff, dev);
    }

    return svga_read_linear(addr, priv);
}


static uint16_t
gd54xx_readw_linear(uint32_t addr, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint16_t temp, temp2;

    addr &= 0x003fffff;	/* 4 MB mask */

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		if (ap == 2)
			addr ^= 0x00000002;

		temp = gd543x_mmio_readw(addr & 0x000000ff, dev);

		switch(ap) {
			case 0:
			default:
				return temp;

			case 1:
			case 2:
				temp2 = temp >> 8;
				temp2 |= ((temp & 0xff) << 8);
				return temp;

			case 3:
				return 0xffff;
		}
	}
    }

    switch (ap) {
	case 0:
	default:		
		return svga_readw_linear(addr, priv);

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000002;

	case 1:
		temp = svga_readb_linear(addr + 1, priv);
		temp |= (svga_readb_linear(addr, priv) << 8);

		if (svga->fast)
		        cycles -= video_timing_read_w;

		return temp;

	case 3:
		return 0xffff;
    }
}


static uint32_t
gd54xx_readl_linear(uint32_t addr, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint32_t temp, temp2;

    addr &= 0x003fffff;	/* 4 MB mask */

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		temp = gd543x_mmio_readl(addr & 0x000000ff, dev);

		switch(ap) {
			case 0:
			default:
				return temp;

			case 1:
				temp2 = temp >> 24;
				temp2 |= ((temp >> 16) & 0xff) << 8;
				temp2 |= ((temp >> 8) & 0xff) << 16;
				temp2 |= (temp & 0xff) << 24;
				return temp2;

			case 2:
				temp2 = (temp >> 8) & 0xff;
				temp2 |= (temp & 0xff) << 8;
				temp2 = ((temp >> 24) & 0xff) << 16;
				temp2 = ((temp >> 16) & 0xff) << 24;
				return temp2;

			case 3:
				return 0xffffffff;
		}
	}
    }

    switch (ap) {
	case 0:
	default:		
		return svga_readw_linear(addr, priv);

	case 1:
		temp = svga_readb_linear(addr + 1, priv);
		temp |= (svga_readb_linear(addr, priv) << 8);
		temp |= (svga_readb_linear(addr + 3, priv) << 16);
		temp |= (svga_readb_linear(addr + 2, priv) << 24);

		if (svga->fast)
		        cycles -= video_timing_read_l;

		return temp;

	case 2:
		temp = svga_readb_linear(addr + 3, priv);
		temp |= (svga_readb_linear(addr + 2, priv) << 8);
		temp |= (svga_readb_linear(addr + 1, priv) << 16);
		temp |= (svga_readb_linear(addr, priv) << 24);

		if (svga->fast)
		        cycles -= video_timing_read_l;

		return temp;

	case 3:
		return 0xffffffff;
    }
}


static void
gd54xx_writeb_linear(uint32_t addr, uint8_t val, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    addr &= 0x003fffff;	/* 4 MB mask */

    switch (ap) {
	case 0:
	default:
		break;

	case 1:
		/* 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2 */
		addr ^= 0x00000001;
		break;

	case 2:
		/* 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0 */
		addr ^= 0x00000003;
		break;

	case 3:
		return;
    }

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR))
		gd543x_mmio_write(addr & 0x000000ff, val, dev);
    }

    if (dev->blt.sys_tx) {
	if (dev->blt.mode == CIRRUS_BLTMODE_MEMSYSSRC) {
		dev->blt.sys_buf &= ~(0xff << (dev->blt.sys_cnt * 8));
		dev->blt.sys_buf |= (val << (dev->blt.sys_cnt * 8));
		dev->blt.sys_cnt++;
		if (dev->blt.sys_cnt >= 4) {
			blit_dword(dev, svga);
			dev->blt.sys_cnt = 0;
		}
	}
	return;
    }
	
    svga_write_linear(addr, val, svga);
}


static void 
gd54xx_writew_linear(uint32_t addr, uint16_t val, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint16_t temp;

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		switch(ap) {
			case 0:
			default:
				gd543x_mmio_writew(addr & 0x000000ff, val, dev);
				return;

			case 2:
				addr ^= 0x00000002;
				/*FALLTHROUGH*/

			case 1:
				temp = (val >> 8);
				temp |= ((val & 0xff) << 8);
				gd543x_mmio_writew(addr & 0x000000ff, temp, dev);
				return;

			case 3:
				return;
		}
	}
    }

    if (dev->blt.sys_tx) {
	gd54xx_writeb_linear(addr, val & 0xff, svga);
	gd54xx_writeb_linear(addr+1, val >> 8, svga);
	return;
    }

    addr &= 0x003fffff;	/* 4 MB mask */

    if (svga->writemode < 4) {
	switch(ap) {
		case 0:
		default:
			svga_writew_linear(addr, val, svga);
			return;

		case 2:
			addr ^= 0x00000002;
			/*FALLTHROUGH*/

		case 1:
			svga_writeb_linear(addr + 1, val & 0xff, svga);
			svga_writeb_linear(addr, val >> 8, svga);

			if (svga->fast)
		        	cycles -= video_timing_write_w;
			return;

		case 3:
			return;
	}
    } else {
	switch(ap) {
		case 0:
		default:
			svga_write_linear(addr, val & 0xff, svga);
			svga_write_linear(addr + 1, val >> 8, svga);
			return;

		case 2:
			addr ^= 0x00000002;
			/*FALLTHROUGH*/

		case 1:
			svga_write_linear(addr + 1, val & 0xff, svga);
			svga_write_linear(addr, val >> 8, svga);
			return;

		case 3:
			return;
	}
    }
}


static void 
gd54xx_writel_linear(uint32_t addr, uint32_t val, priv_t priv)
{
    svga_t *svga = (svga_t *)priv;
    gd54xx_t *dev = (gd54xx_t *)svga->p;

    uint8_t ap = gd54xx_get_aperture(addr);
    uint32_t temp;

    if ((addr & 0x003fff00) == 0x003fff00) {
	if ((svga->seqregs[0x17] & CIRRUS_MMIO_ENABLE) && (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)) {
		switch(ap) {
			case 0:
			default:
				gd543x_mmio_writel(addr & 0x000000ff, val, dev);
				return;

			case 2:
				temp = (val >> 24);
				temp |= ((val >> 16) & 0xff) << 8;
				temp |= ((val >> 8) & 0xff) << 16;
				temp |= (val & 0xff) << 24;
				gd543x_mmio_writel(addr & 0x000000ff, temp, dev);
				return;

			case 1:
				temp = ((val >> 8) & 0xff);
				temp |= (val & 0xff) << 8;
				temp |= (val >> 24) << 16;
				temp |= ((val >> 16) & 0xff) << 24;
				gd543x_mmio_writel(addr & 0x000000ff, temp, dev);
				return;

			case 3:
				return;
		}
	}
    }

    if (dev->blt.sys_tx) {
	gd54xx_writeb_linear(addr, val, svga);
	gd54xx_writeb_linear(addr+1, val >> 8, svga);
	gd54xx_writeb_linear(addr+2, val >> 16, svga);
	gd54xx_writeb_linear(addr+3, val >> 24, svga);
	return;
    }

    addr &= 0x003fffff;	/* 4 MB mask */
	
    if (svga->writemode < 4) {
	switch(ap) {
		case 0:
		default:
			svga_writel_linear(addr, val, svga);
			return;

		case 1:
			svga_writeb_linear(addr + 1, val & 0xff, svga);
			svga_writeb_linear(addr, val >> 8, svga);
			svga_writeb_linear(addr + 3, val >> 16, svga);
			svga_writeb_linear(addr + 2, val >> 24, svga);
			return;

		case 2:
			svga_writeb_linear(addr + 3, val & 0xff, svga);
			svga_writeb_linear(addr + 2, val >> 8, svga);
			svga_writeb_linear(addr + 1, val >> 16, svga);
			svga_writeb_linear(addr, val >> 24, svga);
			return;

		case 3:
			return;
	}

	if (svga->fast)
        	cycles -= video_timing_write_l;
    } else {
	switch(ap) {
		case 0:
		default:
			svga_write_linear(addr, val & 0xff, svga);
			svga_write_linear(addr+1, val >> 8, svga);
			svga_write_linear(addr+2, val >> 16, svga);
			svga_write_linear(addr+3, val >> 24, svga);
			return;

		case 1:
			svga_write_linear(addr + 1, val & 0xff, svga);
			svga_write_linear(addr, val >> 8, svga);
			svga_write_linear(addr + 3, val >> 16, svga);
			svga_write_linear(addr + 2, val >> 24, svga);
			return;

		case 2:
			svga_write_linear(addr + 3, val & 0xff, svga);
			svga_write_linear(addr + 2, val >> 8, svga);
			svga_write_linear(addr + 1, val >> 16, svga);
			svga_write_linear(addr, val >> 24, svga);
			return;

		case 3:
			return;
	}
    }
}


static uint8_t
gd54xx_read(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((svga->seqregs[0x07] & 0x01) == 0)
	return svga_read(addr, svga);

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    addr &= svga->vram_mask;

    return svga_read_linear(addr, svga);
}


static uint16_t
gd54xx_readw(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((svga->seqregs[0x07] & 0x01) == 0)
	return svga_readw(addr, svga);

    addr &= svga->banked_mask;
    if (svga->banked_mask != 0x1ffff)
	addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    return svga_readw_linear(addr, svga);
}


static uint32_t
gd54xx_readl(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((svga->seqregs[0x07] & 0x01) == 0)
	return svga_readl(addr, svga);

    addr &= svga->banked_mask;
    addr = (addr & 0x7fff) + dev->bank[(addr >> 15) & 1];

    return svga_readl_linear(addr, svga);
}


static int
gd543x_do_mmio(svga_t *svga, uint32_t addr)
{
    if (svga->seqregs[0x17] & CIRRUS_MMIO_USE_PCIADDR)
	return 1;

    return ((addr & ~0xff) == 0xb8000);
}


static void
gd543x_mmio_write(uint32_t addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;
	
    if (gd543x_do_mmio(svga, addr)) {
	switch (addr & 0xff) {
		case 0x00:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xffffff00) | val;
			else
				dev->blt.bg_col = (dev->blt.bg_col & 0xff00) | val;
			break;

		case 0x01:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xffff00ff) | (val << 8);
			else
				dev->blt.bg_col = (dev->blt.bg_col & 0x00ff) | (val << 8);
			break;

		case 0x02:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0xff00ffff) | (val << 16);
			break;

		case 0x03:
			if (is_5434(svga))
				dev->blt.bg_col = (dev->blt.bg_col & 0x00ffffff) | (val << 24);
			break;

		case 0x04:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xffffff00) | val;
			else
				dev->blt.fg_col = (dev->blt.fg_col & 0xff00) | val;
			break;

		case 0x05:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xffff00ff) | (val << 8);
			else
				dev->blt.fg_col = (dev->blt.fg_col & 0x00ff) | (val << 8);
			break;

		case 0x06:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0xff00ffff) | (val << 16);
			break;

		case 0x07:
			if (is_5434(svga))
				dev->blt.fg_col = (dev->blt.fg_col & 0x00ffffff) | (val << 24);
			break;

		case 0x08:
			dev->blt.width = (dev->blt.width & 0xff00) | val;
			break;

		case 0x09:
			dev->blt.width = (dev->blt.width & 0x00ff) | (val << 8);
			if (is_5434(svga))
				dev->blt.width &= 0x1fff;
			else
				dev->blt.width &= 0x07ff;
			break;

		case 0x0a:
			dev->blt.height = (dev->blt.height & 0xff00) | val;
			break;

		case 0x0b:
			dev->blt.height = (dev->blt.height & 0x00ff) | (val << 8);
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)
				dev->blt.height &= 0x07ff;
			else
				dev->blt.height &= 0x03ff;
			break;

		case 0x0c:
			dev->blt.dst_pitch = (dev->blt.dst_pitch & 0xff00) | val;
			break;

		case 0x0d:
			dev->blt.dst_pitch = (dev->blt.dst_pitch & 0x00ff) | (val << 8);
			break;

		case 0x0e:
			dev->blt.src_pitch = (dev->blt.src_pitch & 0xff00) | val;
			break;

		case 0x0f:
			dev->blt.src_pitch = (dev->blt.src_pitch & 0x00ff) | (val << 8);
			break;

		case 0x10:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0xffff00) | val;
			break;
			case 0x11:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0xff00ff) | (val << 8);
			break;

		case 0x12:
			dev->blt.dst_addr = (dev->blt.dst_addr & 0x00ffff) | (val << 16);
			if (is_5434(svga))
				dev->blt.dst_addr &= 0x3fffff;
			else
				dev->blt.dst_addr &= 0x1fffff;

			if ((svga->crtc[0x27] >= CIRRUS_ID_CLGD5436) && (dev->blt.status & CIRRUS_BLT_AUTOSTART)) {
				if (dev->blt.mode == CIRRUS_BLTMODE_MEMSYSSRC) {
					dev->blt.sys_tx = 1;
					dev->blt.sys_cnt = 0;
					dev->blt.sys_buf = 0;
					dev->blt.pixel_cnt = dev->blt.scan_cnt = 0;
					dev->blt.src_addr_backup = dev->blt.src_addr;
					dev->blt.dst_addr_backup = dev->blt.dst_addr;						
				} else
					gd54xx_start_blit(0, -1, dev, svga);
			}
			break;

		case 0x14:
			dev->blt.src_addr = (dev->blt.src_addr & 0xffff00) | val;
			break;

		case 0x15:
			dev->blt.src_addr = (dev->blt.src_addr & 0xff00ff) | (val << 8);
			break;

		case 0x16:
			dev->blt.src_addr = (dev->blt.src_addr & 0x00ffff) | (val << 16);
			if (is_5434(svga))
				dev->blt.src_addr &= 0x3fffff;
			else
				dev->blt.src_addr &= 0x1fffff;
			break;

		case 0x17:
			dev->blt.mask = val;
			break;

		case 0x18:
			dev->blt.mode = val;
			break;

		case 0x1a:
			dev->blt.rop = val;
			break;

		case 0x1b:
			if (svga->crtc[0x27] >= CIRRUS_ID_CLGD5436)	
				dev->blt.modeext = val;
			break;		
		
		case 0x1c:
			dev->blt.trans_col = (dev->blt.trans_col & 0xff00) | val;
			break;
		
		case 0x1d:	
			dev->blt.trans_col = (dev->blt.trans_col & 0x00ff) | (val << 8);
			break;
		
		case 0x20:
			dev->blt.trans_mask = (dev->blt.trans_mask & 0xff00) | val;
			break;
		
		case 0x21:
			dev->blt.trans_mask = (dev->blt.trans_mask & 0x00ff) | (val << 8);
			break;
		
		case 0x40:
			dev->blt.status = val;
			if (dev->blt.status & CIRRUS_BLT_START) {
				if (dev->blt.mode == CIRRUS_BLTMODE_MEMSYSSRC) {
					dev->blt.sys_tx = 1;
					dev->blt.sys_cnt = 0;
					dev->blt.sys_buf = 0;
					dev->blt.pixel_cnt = dev->blt.scan_cnt = 0;
					dev->blt.src_addr_backup = dev->blt.src_addr;
					dev->blt.dst_addr_backup = dev->blt.dst_addr;						
				} else
					gd54xx_start_blit(0, -1, dev, svga);
			}
			break;
		}		
    } else if (dev->mmio_vram_overlap)
	gd54xx_write(addr, val, dev);
}


static void
gd543x_mmio_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr)) {
	gd543x_mmio_write(addr, val & 0xff, dev);
	gd543x_mmio_write(addr+1, val >> 8, dev);
    } else if (dev->mmio_vram_overlap) {
	gd54xx_write(addr, val & 0xff, dev);
	gd54xx_write(addr+1, val >> 8, dev);
    }
}


static void
gd543x_mmio_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr)) {
	gd543x_mmio_write(addr, val & 0xff, dev);
	gd543x_mmio_write(addr+1, val >> 8, dev);
	gd543x_mmio_write(addr+2, val >> 16, dev);
	gd543x_mmio_write(addr+3, val >> 24, dev);
    } else if (dev->mmio_vram_overlap) {
	gd54xx_write(addr, val, dev);
	gd54xx_write(addr+1, val >> 8, dev);
	gd54xx_write(addr+2, val >> 16, dev);
	gd54xx_write(addr+3, val >> 24, dev);
    }
}


static uint8_t
gd543x_mmio_read(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr)) {
	switch (addr & 0xff) {
		case 0x40: /*BLT status*/
			return 0;
	}
	return 0xff; /*All other registers read-only*/
    }
    else if (dev->mmio_vram_overlap)
	return gd54xx_read(addr, dev);

    return 0xff;
}


static uint16_t
gd543x_mmio_readw(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr))
	return gd543x_mmio_read(addr, dev) | (gd543x_mmio_read(addr+1, dev) << 8);
    else if (dev->mmio_vram_overlap)
	return gd54xx_read(addr, dev) | (gd54xx_read(addr+1, dev) << 8);

    return 0xffff;
}


static uint32_t
gd543x_mmio_readl(uint32_t addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if (gd543x_do_mmio(svga, addr))
	return gd543x_mmio_read(addr, dev) | (gd543x_mmio_read(addr+1, dev) << 8) | (gd543x_mmio_read(addr+2, dev) << 16) | (gd543x_mmio_read(addr+3, dev) << 24);
    else if (dev->mmio_vram_overlap)
	return gd54xx_read(addr, dev) | (gd54xx_read(addr+1, dev) << 8) | (gd54xx_read(addr+2, dev) << 16) | (gd54xx_read(addr+3, dev) << 24);

    return 0xffffffff;
}


static uint8_t
gd54xx_color_expand(gd54xx_t *dev, int mask, int shift)
{
    uint8_t ret;

    if (dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP)
	ret = dev->blt.fg_col >> (shift << 3);
    else
	ret = mask ? (dev->blt.fg_col >> (shift << 3))
		   : (dev->blt.bg_col >> (shift << 3));

    return ret;
}


static void 
gd54xx_start_blit(uint32_t cpu_dat, int count, gd54xx_t *dev, svga_t *svga)
{
    int blt_mask = 0;
    int x_max = 0;
    int shift = 0, last_x = 0;
    uint32_t src_addr = dev->blt.src_addr;

    switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
	case CIRRUS_BLTMODE_PIXELWIDTH8:
		blt_mask = dev->blt.mask & 7;
		x_max = 8;
		break;

	case CIRRUS_BLTMODE_PIXELWIDTH16:
		blt_mask = dev->blt.mask & 7;
		x_max = 16;
		blt_mask *= 2;
		break;

	case CIRRUS_BLTMODE_PIXELWIDTH24:
		blt_mask = (dev->blt.mask & 0x1f);
		x_max = 24;
		break;

	case CIRRUS_BLTMODE_PIXELWIDTH32:
		blt_mask = dev->blt.mask & 7;
		x_max = 32;
		blt_mask *= 4;
		break;
    }

    last_x = (x_max >> 3) - 1;

    if (count == -1) {
	dev->blt.dst_addr_backup = dev->blt.dst_addr;
	dev->blt.src_addr_backup = dev->blt.src_addr;
	dev->blt.width_backup    = dev->blt.width;
	dev->blt.height_internal = dev->blt.height;
	dev->blt.x_count = 0;
	if ((dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) == (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND))
		dev->blt.y_count = src_addr & 7;
	else
		dev->blt.y_count = 0;

	if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSSRC) {
		if (!(svga->seqregs[7] & 0xf0)) {
			mem_map_set_handler(&svga->mapping, NULL, NULL, NULL, NULL, blit_write_w, blit_write_l);
			mem_map_set_p(&svga->mapping, dev);
		} else {
			mem_map_set_handler(&dev->linear_mapping, NULL, NULL, NULL, NULL, blit_write_w, blit_write_l);
			mem_map_set_p(&dev->linear_mapping, dev);
		}
		recalc_mapping(dev);
		return;
	} else {
		if (!(svga->seqregs[7] & 0xf0)) {
			mem_map_set_handler(&svga->mapping, gd54xx_read, gd54xx_readw, gd54xx_readl, gd54xx_write, gd54xx_writew, gd54xx_writel);
			mem_map_set_p(&dev->svga.mapping, dev);			
		} else {
			mem_map_set_handler(&dev->linear_mapping, svga_readb_linear, svga_readw_linear, svga_readl_linear, gd54xx_writeb_linear, gd54xx_writew_linear, gd54xx_writel_linear);
			mem_map_set_p(&dev->linear_mapping, svga);
		}
		recalc_mapping(dev);
	}
    } else if (dev->blt.height_internal == 0xffff)
	return;

    while (count) {
	uint8_t src = 0, dst;
	int mask = 0;

	if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSSRC) {
		if (dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) {			
			if (dev->blt.modeext & CIRRUS_BLTMODEEXT_DWORDGRANULARITY)
				mask = !!(cpu_dat >> 31);
			else
				mask = !!(cpu_dat & 0x80);

			switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
				case CIRRUS_BLTMODE_PIXELWIDTH8:
					shift = 0;
					break;
				case CIRRUS_BLTMODE_PIXELWIDTH16:
					shift = (dev->blt.x_count & 1);
					break;
				case CIRRUS_BLTMODE_PIXELWIDTH24:
					shift = (dev->blt.x_count % 3);
					break;
				case CIRRUS_BLTMODE_PIXELWIDTH32:
					shift = (dev->blt.x_count & 3);
					break;
			}

			src = gd54xx_color_expand(dev, mask, shift);

			if (shift == last_x) {
				cpu_dat <<= 1;
				count--;
			}
		} else {
			/*This must stay for general purpose Cirrus drivers to render fine in WinNT 3.5x*/
			src = cpu_dat & 0xff;
			cpu_dat >>= 8;
			count -= 8;
			mask = 1;
		}
	} else {
		switch (dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) {
			case 0x00:
				src = svga->vram[dev->blt.src_addr & svga->vram_mask];
				src_addr += ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) ? -1 : 1);
				dev->blt.src_addr += ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) ? -1 : 1);
				mask = 1;
				break;
			case CIRRUS_BLTMODE_PATTERNCOPY:
				switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
					case CIRRUS_BLTMODE_PIXELWIDTH8:
						src = svga->vram[(src_addr & (svga->vram_mask & ~7)) + (dev->blt.y_count << 3) + (dev->blt.x_count & 7)];
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH16:
						src = svga->vram[(src_addr & (svga->vram_mask & ~15)) + (dev->blt.y_count << 4) + (dev->blt.x_count & 15)];
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH24:
						src = svga->vram[(src_addr & (svga->vram_mask & ~31)) + (dev->blt.y_count << 5) + (dev->blt.x_count % 24)];
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH32:
						src = svga->vram[(src_addr & (svga->vram_mask & ~31)) + (dev->blt.y_count << 5) + (dev->blt.x_count & 31)];
						break;
				}
				mask = 1;
				break;

			case CIRRUS_BLTMODE_COLOREXPAND:
				switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
					case CIRRUS_BLTMODE_PIXELWIDTH8:
						mask = svga->vram[src_addr & svga->vram_mask] & (0x80 >> dev->blt.x_count);
						shift = 0;
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH16:
						mask = svga->vram[src_addr & svga->vram_mask] & (0x80 >> (dev->blt.x_count >> 1));
						shift = (dev->blt.x_count & 1);
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH24:
						mask = svga->vram[src_addr & svga->vram_mask] & (0x80 >> (dev->blt.x_count / 3));
						shift = (dev->blt.x_count % 3);
						break;
					case CIRRUS_BLTMODE_PIXELWIDTH32:
						mask = svga->vram[src_addr & svga->vram_mask] & (0x80 >> (dev->blt.x_count >> 2));
						shift = (dev->blt.x_count & 3);
						break;
				}
				src = gd54xx_color_expand(dev, mask, shift);
				break;

			case CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND:
				if (dev->blt.modeext & CIRRUS_BLTMODEEXT_SOLIDFILL) {
					switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
						case CIRRUS_BLTMODE_PIXELWIDTH8:
							shift = 0;
							break;
						case CIRRUS_BLTMODE_PIXELWIDTH16:
							shift = (dev->blt.x_count & 1);
							break;
						case CIRRUS_BLTMODE_PIXELWIDTH24:
							shift = (dev->blt.x_count % 3);
							break;
						case CIRRUS_BLTMODE_PIXELWIDTH32:
							shift = (dev->blt.x_count & 3);
							break;
					}					
					src = (dev->blt.fg_col >> (shift << 3));
				} else {
					switch (dev->blt.mode & CIRRUS_BLTMODE_PIXELWIDTHMASK) {
						case CIRRUS_BLTMODE_PIXELWIDTH8:
							mask = svga->vram[(src_addr & svga->vram_mask & ~7) | dev->blt.y_count] & (0x80 >> dev->blt.x_count);
							shift = 0;
							break;
						case CIRRUS_BLTMODE_PIXELWIDTH16:
							mask = svga->vram[(src_addr & svga->vram_mask & ~7) | dev->blt.y_count] & (0x80 >> (dev->blt.x_count >> 1));
							shift = (dev->blt.x_count & 1);
							break;
						case CIRRUS_BLTMODE_PIXELWIDTH24:
							mask = svga->vram[(src_addr & svga->vram_mask & ~7) | dev->blt.y_count] & (0x80 >> (dev->blt.x_count / 3));
							shift = (dev->blt.x_count % 3);
							break;							
						case CIRRUS_BLTMODE_PIXELWIDTH32:
							mask = svga->vram[(src_addr & svga->vram_mask & ~7) | dev->blt.y_count] & (0x80 >> (dev->blt.x_count >> 2));
							shift = (dev->blt.x_count & 3);
							break;
					}
					src = gd54xx_color_expand(dev, mask, shift);
				}
				break;
		}
		count--;
	}
	dst = svga->vram[dev->blt.dst_addr & svga->vram_mask];
	svga->changedvram[(dev->blt.dst_addr & svga->vram_mask) >> 12] = changeframecount;

	gd54xx_rop(dev, (uint8_t *)&dst, (uint8_t *)&dst, (const uint8_t *) &src);

	if ((dev->blt.mode & CIRRUS_BLTMODE_COLOREXPAND) && (dev->blt.modeext & CIRRUS_BLTMODEEXT_COLOREXPINV))
		mask = !mask;

	if ((dev->blt.width_backup - dev->blt.width) >= blt_mask &&
		!((dev->blt.mode & CIRRUS_BLTMODE_TRANSPARENTCOMP) && !mask))
		svga->vram[dev->blt.dst_addr & svga->vram_mask] = dst;

	dev->blt.dst_addr += ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) ? -1 : 1);

	dev->blt.x_count++;
	if (dev->blt.x_count == x_max) {
		dev->blt.x_count = 0;
		if ((dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) == CIRRUS_BLTMODE_COLOREXPAND)
			src_addr++;
	}

	dev->blt.width--;
	if (dev->blt.width == 0xffff) {
		dev->blt.width = dev->blt.width_backup;

		dev->blt.dst_addr = dev->blt.dst_addr_backup = dev->blt.dst_addr_backup + ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) ? -dev->blt.dst_pitch : dev->blt.dst_pitch);

		switch (dev->blt.mode & (CIRRUS_BLTMODE_PATTERNCOPY|CIRRUS_BLTMODE_COLOREXPAND)) {
			case 0x00:
				src_addr = dev->blt.src_addr_backup = dev->blt.src_addr_backup + ((dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS) ? -dev->blt.src_pitch : dev->blt.src_pitch);
				break;

			case CIRRUS_BLTMODE_COLOREXPAND:
				if (dev->blt.x_count != 0)
					src_addr++;
				break;
		}

		dev->blt.x_count = 0;
		if (dev->blt.mode & CIRRUS_BLTMODE_BACKWARDS)
			dev->blt.y_count = (dev->blt.y_count - 1) & 7;
		else
			dev->blt.y_count = (dev->blt.y_count + 1) & 7;

		dev->blt.height_internal--;
		if (dev->blt.height_internal == 0xffff) {
			if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSSRC) {
				if (!(svga->seqregs[7] & 0xf0)) {
					mem_map_set_handler(&svga->mapping, gd54xx_read, gd54xx_readw, gd54xx_readl, gd54xx_write, gd54xx_writew, gd54xx_writel);
					mem_map_set_p(&svga->mapping, dev);					
				} else {
					mem_map_set_handler(&dev->linear_mapping, svga_readb_linear, svga_readw_linear, svga_readl_linear, gd54xx_writeb_linear, gd54xx_writew_linear, gd54xx_writel_linear);
					mem_map_set_p(&dev->linear_mapping, svga);					
				}
				recalc_mapping(dev);
			}
			return;
		}

		if (dev->blt.mode & CIRRUS_BLTMODE_MEMSYSSRC)
			return;
	}
    }
}


static uint8_t 
cl_pci_read(int func, int addr, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    svga_t *svga = &dev->svga;

    if ((addr >= 0x30) && (addr <= 0x33) && (!dev->has_bios))
	return 0;

    switch (addr) {
	case 0x00: return 0x13; /*Cirrus Logic*/
	case 0x01: return 0x10;

	case 0x02:
		return svga->crtc[0x27];
	case 0x03: return 0x00;
	
	case PCI_REG_COMMAND:
		return dev->pci_regs[PCI_REG_COMMAND]; /*Respond to IO and memory accesses*/

	// case 0x07: return 0 << 1; /*Fast DEVSEL timing*/
	case 0x07: return 0x02; /*Fast DEVSEL timing*/
        
	case 0x08: return dev->rev; /*Revision ID*/
	case 0x09: return 0x00; /*Programming interface*/
        
	case 0x0a: return 0x00; /*Supports VGA interface*/
	case 0x0b: return 0x03;

	case 0x10: return 0x08; /*Linear frame buffer address*/
	case 0x11: return 0x00;
	case 0x12: return 0x00;
	case 0x13: return dev->lfb_base >> 24;

	case 0x30: return (dev->pci_regs[0x30] & 0x01); /*BIOS ROM address*/
	case 0x31: return 0x00;
	case 0x32: return dev->pci_regs[0x32];
	case 0x33: return dev->pci_regs[0x33];

	case 0x3c: return dev->int_line;
	case 0x3d: return PCI_INTA;
    }

    return 0;
}


static void 
cl_pci_write(int func, int addr, uint8_t val, priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    if ((addr >= 0x30) && (addr <= 0x33) && (!dev->has_bios))
	return;

    switch (addr) {
	case PCI_REG_COMMAND:
		dev->pci_regs[PCI_REG_COMMAND] = val & 0x23;
		io_removehandler(0x03c0, 0x0020, gd54xx_in, NULL, NULL, gd54xx_out, NULL, NULL, dev);
		if (val & PCI_COMMAND_IO)
			io_sethandler(0x03c0, 0x0020, gd54xx_in, NULL, NULL, gd54xx_out, NULL, NULL, dev);
		recalc_mapping(dev);
		break;

	case 0x13: 
		dev->lfb_base = val << 24;
		recalc_mapping(dev); 
		break;                

	case 0x30: case 0x32: case 0x33:
		dev->pci_regs[addr] = val;
		if (dev->pci_regs[0x30] & 0x01) {
			uint32_t addr = (dev->pci_regs[0x32] << 16) | (dev->pci_regs[0x33] << 24);
			mem_map_set_addr(&dev->bios_rom.mapping, addr, 0x8000);
		} else
			mem_map_disable(&dev->bios_rom.mapping);
		return;

	case 0x3c:
		dev->int_line = val;
		return;
    }
}


static priv_t
gd54xx_init(const device_t *info, UNUSED(void *parent))
{
    gd54xx_t *dev = (gd54xx_t *)mem_alloc(sizeof(gd54xx_t));
    svga_t *svga = &dev->svga;
    int id = info->local & 0xff;
    const wchar_t *romfn = info->path;
    int vram;

    memset(dev, 0x00, sizeof(gd54xx_t));
    dev->pci = !!(info->flags & DEVICE_PCI);
    dev->vlb = !!(info->flags & DEVICE_VLB);
    dev->rev = 0;
    dev->has_bios = 1;

    switch (id) {
	case CIRRUS_ID_CLGD5402:
	case CIRRUS_ID_CLGD5420:
		break;

	case CIRRUS_ID_CLGD5422:
	case CIRRUS_ID_CLGD5424:
		break;

	case CIRRUS_ID_CLGD5426:
		break;

	case CIRRUS_ID_CLGD5428:
		if (dev->vlb)
			romfn = BIOS_GD5428_VLB_PATH;
		else
			romfn = BIOS_GD5428_ISA_PATH;
		break;

	case CIRRUS_ID_CLGD5429:
		break;

	case CIRRUS_ID_CLGD5434:
		break;
		
	case CIRRUS_ID_CLGD5436:
		break;

	case CIRRUS_ID_CLGD5430:
		if (info->local & 0x400) {
			/* CL-GD 5440 */
			dev->rev = 0x47;
			if (info->local & 0x200) {
				romfn = NULL;
				dev->has_bios = 0;
			} else
				romfn = BIOS_GD5440_PATH;
		} else {
			/* CL-GD 5430 */
			if (dev->pci)
				romfn = BIOS_GD5430_PCI_PATH;
			else
				romfn = BIOS_GD5430_VLB_PATH;
		}
		break;

	case CIRRUS_ID_CLGD5446:
		if (info->local & 0x100)
			romfn = BIOS_GD5446_STB_PATH;
		else
			romfn = BIOS_GD5446_PATH;
		break;

	case CIRRUS_ID_CLGD5480:
		break;
    }

    if (id >= CIRRUS_ID_CLGD5420)
	vram = device_get_config_int("memory");
    else
	vram = 0;

    if (vram)
	dev->vram_size = vram << 20;
    else
	dev->vram_size = 1 << 19;
    dev->vram_mask = dev->vram_size - 1;

    if (romfn)
	rom_init(&dev->bios_rom, romfn, 0xc0000, 0x8000, 0x7fff,
		 0, MEM_MAPPING_EXTERNAL);

    svga_init(&dev->svga, dev, dev->vram_size,
	      recalc_timings, gd54xx_in, gd54xx_out,
	      hwcursor_draw, NULL);
    svga->ven_write = gd54xx_write_modes45;

    mem_map_set_handler(&svga->mapping,
			gd54xx_read, gd54xx_readw, gd54xx_readl,
			gd54xx_write, gd54xx_writew, gd54xx_writel);
    mem_map_set_p(&svga->mapping, dev);

    mem_map_add(&dev->mmio_mapping, 0, 0,
		gd543x_mmio_read, gd543x_mmio_readw, gd543x_mmio_readl,
		gd543x_mmio_write, gd543x_mmio_writew, gd543x_mmio_writel,
		NULL, 0, dev);

    mem_map_add(&dev->linear_mapping, 0, 0,
		gd54xx_readb_linear, gd54xx_readw_linear, gd54xx_readl_linear,
		gd54xx_writeb_linear, gd54xx_writew_linear, gd54xx_writel_linear,
		NULL, 0, svga);

    io_sethandler(0x03c0, 32,
		  gd54xx_in,NULL,NULL, gd54xx_out,NULL,NULL, (priv_t)dev);

    svga->hwcursor.yoff = 32;
    svga->hwcursor.xoff = 0;

    if (id >= CIRRUS_ID_CLGD5420) {
	dev->vclk_n[0] = 0x4a;
	dev->vclk_d[0] = 0x2b;
	dev->vclk_n[1] = 0x5b;
	dev->vclk_d[1] = 0x2f;
	dev->vclk_n[2] = 0x45;
	dev->vclk_d[2] = 0x30;
	dev->vclk_n[3] = 0x7e;
	dev->vclk_d[3] = 0x33;
    } else {
	dev->vclk_n[0] = 0x66;
	dev->vclk_d[0] = 0x3b;
	dev->vclk_n[1] = 0x5b;
	dev->vclk_d[1] = 0x2f;
	dev->vclk_n[1] = 0x45;
	dev->vclk_d[1] = 0x2c;
	dev->vclk_n[1] = 0x7e;
	dev->vclk_d[1] = 0x33;
    }

    dev->bank[1] = 0x8000;

    if (dev->pci && id >= CIRRUS_ID_CLGD5430)
	pci_add_card(PCI_ADD_VIDEO, cl_pci_read, cl_pci_write, dev);

    dev->pci_regs[PCI_REG_COMMAND] = 7;

    dev->pci_regs[0x30] = 0x00;
    dev->pci_regs[0x32] = 0x0c;
    dev->pci_regs[0x33] = 0x00;
	
    svga->crtc[0x27] = id;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


static void
gd54xx_close(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
	
    svga_close(&dev->svga);
    
    free(dev);
}


static void
gd54xx_speed_changed(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;
    
    svga_recalctimings(&dev->svga);
}


static void
gd54xx_force_redraw(priv_t priv)
{
    gd54xx_t *dev = (gd54xx_t *)priv;

    dev->svga.fullchange = changeframecount;
}


static const device_config_t gd5422_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",1,
                {
                        {
                                "512 KB",0
                        },
                        {
                                "1 MB",1
                        },
                        {
                                NULL
                        }
                },
        },
        {
                NULL
        }
};

static const device_config_t gd5428_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",2,
                {
                        {
                                "1 MB",1
                        },
                        {
                                "2 MB",2
                        },
                        {
                                NULL
                        }
                },
        },
        {
                NULL
        }
};

static const device_config_t gd5440_onboard_config[] =
{
        {
                "memory","Video memory size",CONFIG_SELECTION,"",2,
                {
                        {
                                "1 MB",1
                        },
                        {
                                "2 MB",2
                        },
                        {
                                NULL
                        }
                },
        },
        {
                NULL
        }
};

static const device_config_t gd5434_config[] =
{
        {
                "memory","Memory size",CONFIG_SELECTION,"",4,
                {
                        {
                                "2 MB",2
                        },
                        {
                                "4 MB",4
                        },
                        {
                                NULL
                        }
                },
        },
        {
                NULL
        }
};

static const video_timings_t cl_gd_isa_timing = {VID_ISA,3,3,6,8,8,12};
static const video_timings_t cl_gd_vlb_timing = {VID_BUS,4,4,8,10,10,20};
static const video_timings_t cl_gd_pci_timing = {VID_BUS,4,4,8,10,10,20};

const device_t gd5402_isa_device = {
    "Cirrus Logic GD-5402 (ACUMOS AVGA2)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5402,
    BIOS_GD5420_PATH,		/* Common BIOS between 5402 and 5420 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    NULL,
};

const device_t gd5420_isa_device = {
    "Cirrus Logic GD-5420",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5420,
    BIOS_GD5420_PATH,		/* Common BIOS between 5402 and 5420 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5422_config,
};

const device_t gd5422_isa_device = {
    "Cirrus Logic GD-5422",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5422,
    BIOS_GD5422_PATH,		/* Common BIOS between 5422 and 5424 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5422_config,
};

const device_t gd5424_vlb_device = {
    "Cirrus Logic GD-5424",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5424,
    BIOS_GD5422_PATH,		/* Common BIOS between 5422 and 5424 */
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5422_config,
};

const device_t gd5426_vlb_device = {
    "Cirrus Logic GD-5426",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5426,
    BIOS_GD5426_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5428_isa_device = {
    "Cirrus Logic GD-5428",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5428,
    BIOS_GD5428_ISA_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5428_config
};

const device_t gd5428_vlb_device = {
    "Cirrus Logic GD-5428",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5428,
    BIOS_GD5428_VLB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5429_isa_device = {
    "Cirrus Logic GD-5429",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5429,
    BIOS_GD5429_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5428_config
};

const device_t gd5429_vlb_device = {
    "Cirrus Logic GD-5429",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5429,
    BIOS_GD5429_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5430_vlb_device = {
    "Cirrus Logic GD-5430",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5430,
    BIOS_GD5430_VLB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5428_config
};

const device_t gd5430_pci_device = {
    "Cirrus Logic GD-5430",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5430,
    BIOS_GD5430_PCI_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5428_config
};

const device_t gd5434_isa_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_AT | DEVICE_ISA,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_isa_timing,
    gd5434_config
};

const device_t gd5434_vlb_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_VLB,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_vlb_timing,
    gd5434_config
};

const device_t gd5434_pci_device = {
    "Cirrus Logic GD-5434",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5434,
    BIOS_GD5434_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5436_pci_device = {
    "Cirrus Logic GD-5436",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5436,
    BIOS_GD5436_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5440_pci_device = {
    "Cirrus Logic GD-5440",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5440 | 0x400,
    BIOS_GD5440_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5428_config
};

const device_t gd5440_onboard_pci_device = {
    "Onboard Cirrus Logic GD-5440",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5440 | 0x600,
    NULL,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5440_onboard_config
};

const device_t gd5446_pci_device = {
    "Cirrus Logic GD-5446",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5446,
    BIOS_GD5446_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5446_stb_pci_device = {
    "STB Nitro 64V",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5446 | 0x100,
    BIOS_GD5446_STB_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};

const device_t gd5480_pci_device = {
    "Cirrus Logic GD-5480",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_PCI,
    CIRRUS_ID_CLGD5480,
    BIOS_GD5480_PATH,
    gd54xx_init, gd54xx_close, NULL,
    NULL,
    gd54xx_speed_changed,
    gd54xx_force_redraw,
    &cl_gd_pci_timing,
    gd5434_config
};
