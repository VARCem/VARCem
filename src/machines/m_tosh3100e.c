/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Toshiba T3100e.
 *
 *		The Toshiba 3100e is a 286-based portable.
 *
 *		To bring up the BIOS setup screen hold down the 'Fn' key
 *		on booting.
 *
 *		Memory management
 *		~~~~~~~~~~~~~~~~~
 * 
 *		Motherboard memory is divided into:
 *		- Conventional memory: Either 512k or 640k
 *		- Upper memory:        Either 512k or 384k, depending on
 *				       amount of conventional memory.
 *				       Upper memory can be used as EMS or XMS.
 *		- High memory:         0-4Mb, depending on RAM installed.
 *				       The BIOS setup screen allows some or
 *				       all of this to be used as EMS; the
 *				       remainder is XMS.
 * 
 *		Additional memory (either EMS or XMS) can also be provided
 *		by ISA expansion cards.
 *
 *		Under test in PCem, the BIOS will boot with up to 65368Kb
 *		of memory in total (16Mb less 16k). However it will give
 *		an error with RAM sizes above 8Mb, if any of the high
 *		memory is allocated as EMS, because the builtin EMS page
 *		registers can only access up to 8Mb.
 *
 *		Memory is controlled by writes to I/O port 8084h:
 *		  Bit 7: Always 0  }
 *		  Bit 6: Always 1  } These bits select which motherboard
 *		  Bit 5: Always 0  } function to access.
 *		  Bit 4: Set to treat upper RAM as XMS
 *		  Bit 3: Enable external RAM boards? 
 *		  Bit 2: Set for 640k conventional memory, clear for 512k
 *		  Bit 1: Enable RAM beyond 1Mb.
 *		  Bit 0: Enable EMS.
 *
 *		The last value written to this port is saved at 0040:0093h,
 *		and in CMOS memory at offset 0x37. If the top bit of the
 *		CMOS byte is set, then high memory is being provided by
 *		an add-on card rather than the mainboard; accordingly,
 *		the BIOS will not allow high memory to be used as EMS.
 *
 *		EMS is controlled by 16 page registers:
 *
 *		Page mapped at		0xD000	0xD400	0xD800	0xDC00
 *		------------------------------------------------------
 *		Pages 0x00-0x7F		 0x208	0x4208	0x8208	0xc208
 *		Pages 0x80-0xFF		 0x218	0x4218	0x8218	0xc218
 *		Pages 0x100-0x17F	 0x258	0x4258	0x8258	0xc258
 *		Pages 0x180-0x1FF	 0x268	0x4268	0x8268	0xc268
 *
 *		The value written has bit 7 set to enable EMS, reset to
 *		disable it.
 *
 *		So:
 *		OUT 0x208,  0x80  will page in the first 16k page at 0xD0000.
 *		OUT 0x208,  0x00  will page out EMS, leaving nothing at 0xD0000.
 *		OUT 0x4208, 0x80  will page in the first 16k page at 0xD4000.
 *		OUT 0x218,  0x80  will page in the 129th 16k page at 0xD0000.
 *		etc.
 * 
 *		The T3100e motherboard can (and does) dynamically reassign RAM
 *		between conventional, XMS and EMS. This translates to monkeying
 *		with the mappings.
 *
 *		To use EMS from DOS, you will need the Toshiba EMS driver
 *		(TOSHEMM.ZIP). This supports the above system, plus further
 *		ranges of ports at 0x_2A8, 0x_2B8, 0x_2C8.
 *
 *		Features not implemented:
 *		> Four video fonts.
 *		> BIOS-controlled mapping of serial ports to IRQs.
 *		> Custom keyboard controller. This has a number of extra
 *		  commands in the 0xB0-0xBC range, for such things as turbo
 *		  on/off, and switching the keyboard between AT and PS/2
 *		  modes. Currently I have only implemented command 0xBB,
 *		  so that self-test completes successfully. Commands include:
 *
 *		  0xB0:   Turbo on
 *		  0xB1:   Turbo off
 *		  0xB2:   Internal display on?
 *		  0xB3:   Internal display off?
 *		  0xB5:   Get settings byte (bottom bit is color/mono setting)
 *		  0xB6:   Set settings byte
 *		  0xB7:   Behave as 101-key PS/2 keyboard
 *		  0xB8:   Behave as 84-key AT keyboard
 *		  0xBB:   Return a byte, bit 2 is Fn key state, other bits unknown.
 *
 *		The other main I/O port needed to POST is:
 *		  0x8084: System control.
 *		  Top 3 bits give command, bottom 5 bits give parameters.
 *		  000 => set serial port IRQ / addresses
 *		  bit 4:    IRQ5 serial port base: 1 => 0x338, 0 => 0x3E8
 *		  bits 3, 2, 0 specify serial IRQs for COM1, COM2, COM3:
 *			    00 0 => 4, 3, 5
 *                          00 1 => 4, 5, 3
 *                          01 0 => 3, 4, 5
 *                          01 1 => 3, 5, 4
 *                          10 0 => 4, -, 3
 *                          10 1 => 3, -, 4 
 *		  010 => set memory mappings
 *			 bit 4 set if upper RAM is XMS
 *			 bit 3 enable add-on memory boards beyond 5Mb?
 *                       bit 2 set for 640k sysram, clear for 512k sysram
 *                       bit 1 enable mainboard XMS
 *                       bit 0 enable mainboard EMS
 *		  100 => set parallel mode / LCD settings
 *                       bit 4 set for bidirectional parallel port
 *                       bit 3 set to disable internal CGA
 *                       bit 2 set for single-pixel LCD font
 *                       bits 0,1 for display font
 *
 * Version:	@(#)m_t3100e.c	1.0.14	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		John Elliott, <jce@seasip.info>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2017-2018 John Elliott.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../device.h"
#include "../devices/video/video.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../plat.h"
#include "machine.h"
#include "m_tosh3100e.h"


static const uint16_t ems_page_reg[] = {
    0x0208, 0x4208, 0x8208, 0xc208,	/* The first four map the first 2MB */
					/* of RAM into the page frame */
    0x0218, 0x4218, 0x8218, 0xc218,	/* The next four map the next 2MB */
					/* of RAM */
    0x0258, 0x4258, 0x8258, 0xc258,	/* and so on. */
    0x0268, 0x4268, 0x8268, 0xc268,
};


typedef struct {
    uint8_t	page[16];
    mem_map_t	mapping[4];
    uint32_t	page_exec[4];	/* Physical location of memory pages */
    uint32_t	upper_base;	/* Start of upper RAM */
    uint8_t	upper_pages;	/* Pages of EMS available from upper RAM */
    uint8_t	upper_is_ems;	/* Upper RAM is EMS? */
    mem_map_t	upper_mapping;
    uint8_t	notify;		/* Notification from keyboard controller */
    uint8_t	turbo;		/* 0 for 6MHz, else full speed */
    uint8_t	mono;		/* Emulates PC/AT 'mono' motherboard switch */
				/* Bit 0 is 0 for colour, 1 for mono */
} t3100e_t;


static void	ems_out(uint16_t, uint8_t, priv_t);


/* Given a memory address (which ought to be in the page frame at 0xD0000), 
 * which page does it relate to? */
static int
addr_to_page(uint32_t addr)
{
    if ((addr & 0xf0000) == 0xd0000)
	return ((addr >> 14) & 3);

    return -1;
}


/* And vice versa: Given a page slot, which memory address does it 
 * correspond to? */
static uint32_t
page_to_addr(int pg)
{
    return 0xd0000 + ((pg & 3) * 16384);
}


/* Given an EMS page ID, return its physical address in RAM. */
static uint32_t
ems_addr(t3100e_t *dev, int pg, uint16_t val)
{
    uint32_t addr;

    if (!(val & 0x80)) return 0;	/* Bit 7 reset => not mapped */

    val &= 0x7f;
    val += (0x80 * (pg >> 2));		/* high bits of the register bank */
					/* are used to extend val to allow up */
					/* to 8MB of EMS to be accessed */

    /* Is it in the upper memory range? */
    if (dev->upper_is_ems) {
	if (val < dev->upper_pages) {
		addr = dev->upper_base + 0x4000 * val;
		return addr;
	}
	val -= dev->upper_pages;
    }

    /* Otherwise work down from the top of high RAM (so, the more EMS,
     * the less XMS) */
    if (((val * 0x4000) + 0x100000) >= (mem_size * 1024))
	return 0;	/* Not enough high RAM for this page */
 
    /* High RAM found */
    addr = (mem_size * 1024) - 0x4000 * (val + 1);

    return addr;
}


/* The registers governing the EMS ports are in rather a nonintuitive order */
static int
port_to_page(uint16_t addr)
{
    switch (addr) {
	case 0x208:  return  0; 
	case 0x4208: return  1; 
	case 0x8208: return  2; 
	case 0xc208: return  3; 
	case 0x218:  return  4; 
	case 0x4218: return  5; 
	case 0x8218: return  6; 
	case 0xc218: return  7; 
	case 0x258:  return  8;
	case 0x4258: return  9;
	case 0x8258: return 10;
	case 0xc258: return 11;
	case 0x268:  return 12;
	case 0x4268: return 13;
	case 0x8268: return 14;
	case 0xc268: return 15;
    }

    return -1;
}


#ifdef _DEBUG
/* Used to dump the memory mapping table, for debugging. */
static void
dump_mappings(t3100e_t *dev)
{
    mem_map_t *mm = base_mapping.next;

    while (mm) {
	const char *name = "";
	uint32_t offset = (uint32_t)(mm->exec - ram);

	if (mm == &ram_low_mapping ) name = "LOW ";
	if (mm == &ram_mid_mapping ) name = "MID ";
	if (mm == &ram_high_mapping) name = "HIGH";
	if (mm == &dev->upper_mapping) name = "UPPR";
	if (mm == &dev->mapping[0]) {
		name = "EMS0";
		offset = dev->page_exec[0];
	}
	if (mm == &dev->mapping[1]) {
		name = "EMS1";
		offset = dev->page_exec[1];
	}
	if (mm == &dev->mapping[2]) {
		name = "EMS2";
		offset = dev->page_exec[2];
	}
	if (mm == &dev->mapping[3]) {
		name = "EMS3";
		offset = dev->page_exec[3];
	}

	DEBUG("  %p | base=%05x size=%05x %c @ %06x %s\n", mm, 
		mm->base, mm->size, mm->enable ? 'Y' : 'N', 
		offset, name);

	mm = mm->next;
    }
}
#endif


static void
map_ram(t3100e_t *dev, uint8_t val)
{
    int32_t upper_len;
    int n;

#if 0	/*NOT_USED*/
    DEBUG("OUT 0x8084, %02x [ set memory mapping :", val | 0x40); 
    if (val & 1) DEBUG("ENABLE_EMS ");
    if (val & 2) DEBUG("ENABLE_XMS ");
    if (val & 4) DEBUG("640K ");
    if (val & 8) DEBUG("X8X ");
    if (val & 16) DEBUG("UPPER_IS_XMS ");
    DEBUG("\n");
#endif

    /* Bit 2 controls size of conventional memory */
    if (val & 4) {
	dev->upper_base  = 0xA0000;
	dev->upper_pages = 24;
    } else {
	dev->upper_base = 0x80000;
	dev->upper_pages = 32;
    }
    upper_len = dev->upper_pages * 16384;

    mem_map_set_addr(&ram_low_mapping, 0, dev->upper_base);

    /* Bit 0 set if upper RAM is EMS */
    dev->upper_is_ems = (val & 1);

    /* Bit 1 set if high RAM is enabled */
    if (val & 2)
	mem_map_enable(&ram_high_mapping);
      else
	mem_map_disable(&ram_high_mapping);

    /* Bit 4 set if upper RAM is mapped to high memory 
     * (and bit 1 set if XMS enabled) */
    if ((val & 0x12) == 0x12) {
	mem_map_set_addr(&dev->upper_mapping, mem_size * 1024, upper_len);
	mem_map_enable(&dev->upper_mapping);
	mem_map_set_exec(&dev->upper_mapping, ram + dev->upper_base);
    } else {
	mem_map_disable(&dev->upper_mapping);
    }

    /* Recalculate EMS mappings */
    for (n = 0; n < 4; n++)
	ems_out(ems_page_reg[n], dev->page[n], dev);

#ifdef _DEBUG
    dump_mappings(dev);
#endif
}


static uint8_t
sys_in(UNUSED(uint16_t addr), priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    /* The low 4 bits always seem to be 0x0C. The high 4 are a 
     * notification sent by the keyboard controller when it detects
     * an [Fn] key combination */
    DEBUG("IN 0x8084\n");

    return 0x0c | (dev->notify << 4);
}


/* Handle writes to the T3100e system control port at 0x8084 */
static void
sys_out(UNUSED(uint16_t addr), uint8_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    switch (val & 0xe0) {
	case 0x00:	/* Set serial port IRQs. Not implemented */
		DEBUG("OUT 0x8084, %02x [ set serial port IRQs]\n", val);
		break;

	case 0x40:	/* Set RAM mappings. */
		map_ram(dev, val & 0x1F);
		break;

	case 0x80:	/* Set video options. */
		t3100e_video_options_set(val & 0x1f);
		break;

	default:	/* Other options not implemented. */
		DEBUG("OUT 0x8084, %02x\n", val);
		break;
    }
}


/* Read EMS page register */
static uint8_t
ems_in(uint16_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    return dev->page[port_to_page(addr)];

}


/* Write EMS page register */
static void
ems_out(uint16_t addr, uint8_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = port_to_page(addr);

    dev->page_exec[pg & 3] = ems_addr(dev, pg, val);
    DEBUG("EMS: page %d %02x -> %02x [%06x]\n",
	  pg, dev->page[pg], val, dev->page_exec[pg & 3]);
    dev->page[pg] = val;

    /* Bit 7 set if page is enabled, reset if page is disabled */
    pg &= 3;
    if (dev->page_exec[pg]) {
	DEBUG("Enabling EMS RAM at %05x\n", page_to_addr(pg));
	mem_map_enable(&dev->mapping[pg]);
	mem_map_set_exec(&dev->mapping[pg], ram + dev->page_exec[pg]);
    } else {
	DEBUG("Disabling EMS RAM at %05x\n", page_to_addr(pg));
	mem_map_disable(&dev->mapping[pg]);
    }
}


/* Read RAM in the EMS page frame. */
static uint8_t
ems_read(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return 0xff;

    addr = dev->page_exec[pg] + (addr & 0x3fff);

    return ram[addr];	
}


static uint16_t
ems_readw(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return 0xffff;

    DBGLOG(1, "ems_readw(%05x) ", addr);
    addr = dev->page_exec[pg] + (addr & 0x3fff);
    DBGLOG(1, "-> %06x val=%04x\n", addr, *(uint16_t *)&ram[addr]);

    return *(uint16_t *)&ram[addr];
}


static uint32_t
ems_readl(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return 0xffffffff;

    addr = dev->page_exec[pg] + (addr & 0x3fff);

    return *(uint32_t *)&ram[addr];
}


/* Write RAM in the EMS page frame. */
static void
ems_write(uint32_t addr, uint8_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    addr = dev->page_exec[pg] + (addr & 0x3fff);
    ram[addr] = val;
}


static void
ems_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    DBGLOG(1, "ems_writew(%05x) ", addr);
    addr = dev->page_exec[pg] + (addr & 0x3fff);
    DBGLOG(1, "-> %06x val=%04x\n", addr, val);

    *(uint16_t *)&ram[addr] = val;
}


static void
ems_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    addr = dev->page_exec[pg] + (addr & 0x3fff);
    *(uint32_t *)&ram[addr] = val;
}


/* Read RAM in the upper area. This is basically what the 'remapped'
 * mapping in mem.c does, except that the upper area can move around */
static uint8_t
upper_read(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;

    return ram[addr];
}


static uint16_t
upper_readw(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;

    return *(uint16_t *)&ram[addr];
}


static uint32_t
upper_readl(uint32_t addr, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;

    return *(uint32_t *)&ram[addr];
}


static void
upper_write(uint32_t addr, uint8_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;
    ram[addr] = val;
}


static void
upper_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;
    *(uint16_t *)&ram[addr] = val;
}


static void
upper_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    addr = (addr - (1024 * mem_size)) + dev->upper_base;
    *(uint32_t *)&ram[addr] = val;
}


static uint8_t
kbd_get(priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    return(dev->turbo);
}


static void
kbd_set(priv_t priv, uint8_t value)
{
    t3100e_t *dev = (t3100e_t *)priv;

    dev->turbo = !!value;
}


static void
t3100e_close(priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    free(dev);
}


static priv_t
t3100e_init(const device_t *info, UNUSED(void *arg))
{
    t3100e_t *dev;
    void *kbd;
    int pg;

    dev = (t3100e_t *)mem_alloc(sizeof(t3100e_t));
    memset(dev, 0x00, sizeof(t3100e_t));

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    m_at_common_ide_init();

    /* Tell keyboard driver we want to handle some stuff here. */
    kbd = device_add(&keyboard_at_toshiba_device);
    keyboard_at_set_funcs(kbd, kbd_get, kbd_set, dev);

    device_add(&fdc_at_device);

    /* Hook up system control port. */
    io_sethandler(0x8084, 1,
		  sys_in,NULL,NULL, sys_out,NULL,NULL, dev);

    /* Start monitoring all 16 EMS registers */
    for (pg = 0; pg < 16; pg++) {
	io_sethandler(ems_page_reg[pg], 1, 
		      ems_in,NULL,NULL, ems_out,NULL,NULL, dev); 
    }

    /* Map the EMS page frame. */
    for (pg = 0; pg < 4; pg++) {
	DBGLOG(1, "Adding memory map at %x for page %d\n",page_to_addr(pg),pg);
	mem_map_add(&dev->mapping[pg], page_to_addr(pg), 16384, 
		    ems_read,ems_readw,ems_readl,
		    ems_write,ems_writew,ems_writel,
		    NULL, MEM_MAPPING_EXTERNAL, dev);

	/* Start them all off disabled */
	mem_map_disable(&dev->mapping[pg]);
    }

    /* Mapping for upper RAM when in use as XMS*/
    mem_map_add(&dev->upper_mapping, mem_size * 1024, 384 * 1024,
		upper_read,upper_readw,upper_readl,
		upper_write,upper_writew,upper_writel,
		NULL, MEM_MAPPING_INTERNAL, dev);

    mem_map_disable(&dev->upper_mapping);

    device_add(&t3100e_vid_device);

    return((priv_t)dev);
}


static const machine_t t3100e_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_VIDEO | MACHINE_HDC,
    MACHINE_VIDEO,
    1024, 5120, 256,  64, 8,
    {{"",cpus_286}}
};

const device_t m_tosh_3100e = {
    "Toshiba 3100e",
    DEVICE_ROOT,
    0,
    L"toshiba/t3100e",
    t3100e_init, t3100e_close, NULL,
    NULL, NULL, NULL,
    &t3100e_info,
    NULL
};


/* The byte returned:
 *  Bit 7: Set if internal plasma display enabled
 *  Bit 6: Set if running at 6MHz, clear at full speed
 *  Bit 5: Always 1?
 *  Bit 4: Set if the FD2MB jumper is present (internal floppy is ?tri-mode)
 *  Bit 3: Clear if the FD2 jumper is present (two internal floppies)
 *  Bit 2: Set if the internal drive is A:, clear if B:
 *  Bit 1: Set if the parallel port is configured as a floppy connector
 *         for the second drive.
 *  Bit 0: Set if the F2HD jumper is present (internal floppy is 720k)
 */
uint8_t
t3100e_config_get(priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;
    uint8_t ret = 0x28;			/* start with bits 5 and 3 set */

    int type_a = fdd_get_type(0);
    int type_b = fdd_get_type(1);
    int prt_switch;	/* External drive type: 0=> none, 1=>A, 2=>B */

    /* Get display setting */
    if (t3100e_display_get())
	ret |= 0x80;
    if (! dev->turbo)
	ret |= 0x40;

    /* Try to determine the floppy types.*/
    prt_switch = (type_b ? 2 : 0);
    switch(type_a) {
	/*
	 * Since a T3100e cannot have an internal 5.25" drive,
	 * mark 5.25" A: drive as  being external, and set the
	 * internal type based on type_b.
	 */
	case 1:			/* 360k */
	case 2:			/* 1.2Mb */
	case 3:			/* 1.2Mb RPMx2*/
		prt_switch = 1;	/* External drive is A: */
		switch (type_b) {
			case 1:				/* 360k */
			case 4: ret |= 1;    break;	/* 720k */
			case 6: ret |= 0x10; break;	/* Tri-mode */
			/* All others will be treated as 1.4M */
		}
		break;

	case 4:
		ret |= 0x01;	/* 720k */
		if (type_a == type_b) {
			ret &= (~8);	/* Two internal drives */
			prt_switch = 0;	/* No external drive */
		}
		break;

	case 5:	/* 1.4M */
	case 7:	/* 2.8M */
		if (type_a == type_b) {
			ret &= (~8);	/* Two internal drives */
			prt_switch = 0;	/* No external drive */
		}
		break;

	case 6:	/* 3-mode */
		ret |= 0x10;
		if (type_a == type_b) {
			ret &= (~8);	/* Two internal drives */
			prt_switch = 0;	/* No external drive */
		}
		break;
    }

    switch (prt_switch) {
	case 0:	ret |= 4; break;	/* No external floppy */
	case 1:	ret |= 2; break;	/* External floppy is A: */
	case 2:	ret |= 6; break;	/* External floppy is B: */
    }

    return ret;
}


void
t3100e_notify_set(priv_t priv, uint8_t val)
{
    t3100e_t *dev = (t3100e_t *)priv;

    dev->notify = val;
}


void
t3100e_mono_set(priv_t priv, uint8_t val)
{
    t3100e_t *dev = (t3100e_t *)priv;

    dev->mono = val;
}


uint8_t
t3100e_mono_get(priv_t priv)
{
    t3100e_t *dev = (t3100e_t *)priv;

    return dev->mono;
}


void
t3100e_turbo_set(priv_t priv, uint8_t val)
{
    t3100e_t *dev = (t3100e_t *)priv;

    dev->turbo = val;

    pc_set_speed(dev->turbo);
}
