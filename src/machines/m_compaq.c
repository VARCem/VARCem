/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of various Compaq PC's.
 *
 * **NOTE**	All Compaq machines are now in DevBranch, as none of them
 *		seem to be fully functional. The Portable, Portable/286
 *		and Portable II machines need the 'Compaq VDU' video card
 *		(which is in m_compaq_vid.c), the Portable 3 needs the
 *		Plasma driver, there are some ROM issues, etc.
 *
 * Version:	@(#)m_compaq.c	1.0.14	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/system/nmi.h"
#include "../devices/system/pit.h"
#include "../devices/input/keyboard.h"
#include "../devices/ports/parallel.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "machine.h"
#include "m_compaq.h"
#include "m_olim24.h"


typedef struct {
    int		type;

    mem_map_t	ram_mapping;		/* Deskpro 386 remaps RAM from
					 * 0x0a0000-0x0fffff to 0xfa0000 */
} cpq_t;


static uint8_t
read_ram(uint32_t addr, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addreadlookup(mem_logical_addr, addr);

    return(ram[addr]);
}


static uint16_t
read_ramw(uint32_t addr, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addreadlookup(mem_logical_addr, addr);

    return(*(uint16_t *)&ram[addr]);
}


static uint32_t
read_raml(uint32_t addr, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addreadlookup(mem_logical_addr, addr);

    return(*(uint32_t *)&ram[addr]);
}


static void
write_ram(uint32_t addr, uint8_t val, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addwritelookup(mem_logical_addr, addr);

    mem_write_ramb_page(addr, val, &pages[addr >> 12]);
}


static void
write_ramw(uint32_t addr, uint16_t val, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addwritelookup(mem_logical_addr, addr);

    mem_write_ramw_page(addr, val, &pages[addr >> 12]);
}


static void
write_raml(uint32_t addr, uint32_t val, priv_t priv)
{
//    cpq_t *dev = (cpq_t *)priv;

    addr = (addr & 0x7ffff) + 0x80000;
    addwritelookup(mem_logical_addr, addr);

    mem_write_raml_page(addr, val, &pages[addr >> 12]);
}


static void
cpq_close(priv_t priv)
{
    cpq_t *dev = (cpq_t *)priv;

    free(dev);
}


static priv_t
cpq_init(const device_t *info, void *arg)
{
    cpq_t *dev;

    dev = (cpq_t *)mem_alloc(sizeof(cpq_t));
    memset(dev, 0x00, sizeof(cpq_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    switch(dev->type) {
	case 0:		/* Portable */
		machine_common_init();
		nmi_init();

		/* Set up our DRAM refresh timer. */
		pit_set_out_func(&pit, 1, m_xt_refresh_timer);

		device_add(&keyboard_xt_device);
		parallel_setup(0, 0x03bc);

		if (config.video_card == VID_INTERNAL)
			device_add(&compaq_video_device);

		device_add(&fdc_xt_device);
		break;

	case 1:		/* Portable/286 */
		m_at_init();
		mem_remap_top(384);
		mem_map_add(&dev->ram_mapping, 0xfa0000, 0x60000,
          		    read_ram,read_ramw,read_raml,
			    write_ram,write_ramw,write_raml,
			    ram + 0xa0000, MEM_MAPPING_INTERNAL, dev);

		if (config.video_card == VID_INTERNAL)
			device_add(&compaq_video_device);

		device_add(&fdc_at_device);
		break;

	case 2:		/* Portable II */
		m_at_init();
		mem_remap_top(384);
		mem_map_add(&dev->ram_mapping, 0xfa0000, 0x60000,
          		    read_ram,read_ramw,read_raml,
			    write_ram,write_ramw,write_raml,
			    ram + 0xa0000, MEM_MAPPING_INTERNAL, dev);

		if (config.video_card == VID_INTERNAL)
			device_add(&compaq_video_device);

		device_add(&fdc_at_device);
		break;

	case 3:		/* Portable III */
		m_at_init();
		mem_remap_top(384);
		mem_map_add(&dev->ram_mapping, 0xfa0000, 0x60000,
          		    read_ram,read_ramw,read_raml,
			    write_ram,write_ramw,write_raml,
			    ram + 0xa0000, MEM_MAPPING_INTERNAL, dev);

		if (config.video_card == VID_INTERNAL)
			m_olim24_vid_init(1);

		device_add(&fdc_at_device);
		break;

	case 4:		/* Portable III/386 */
		m_at_init();
		mem_remap_top(384);
		mem_map_add(&dev->ram_mapping, 0xfa0000, 0x60000,
          		    read_ram,read_ramw,read_raml,
			    write_ram,write_ramw,write_raml,
			    ram + 0xa0000, MEM_MAPPING_INTERNAL, dev);

		if (config.video_card == VID_INTERNAL)
			m_olim24_vid_init(1);

		device_add(&fdc_at_device);

		if (config.hdc_type == HDC_INTERNAL)
			device_add(&ide_isa_device);
		break;

	case 5:		/* Deskpro 386 */
		m_at_init();
		mem_remap_top(384);

		device_add(&fdc_at_device);

		if (config.hdc_type == HDC_INTERNAL)
			device_add(&ide_isa_device);
		break;
    }

    return((priv_t)dev);
}


static const machine_t p1_info = {
    MACHINE_ISA | MACHINE_VIDEO,
    0,
    128, 640, 128, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_cpq_p1 = {
    "Compaq Portable",
    DEVICE_ROOT,
    0,
    L"compaq/portable",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &p1_info,
    NULL
};


static const machine_t p1_286_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    640, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_cpq_p1_286 = {
    "Compaq Portable/286",
    DEVICE_ROOT,
    1,
    L"compaq/portable286",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &p1_286_info,
    NULL
};


static const machine_t p2_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    640, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_cpq_p2 = {
    "Compaq Portable II",
    DEVICE_ROOT,
    2,
    L"compaq/portable2",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &p2_info,
    NULL
};


static const machine_t p3_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_VIDEO,
    MACHINE_VIDEO,
    640, 16384, 128, 128, 6,
    {{"",cpus_286}}
};

const device_t m_cpq_p3 = {
    "Compaq Portable III",
    DEVICE_ROOT,
    3,
    L"compaq/portable3",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &p3_info,
    NULL
};


static const machine_t p3_386_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,
    MACHINE_VIDEO,
    1, 14, 1, 128, -1,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_cpq_p3_386 = {
    "Compaq Portable III/386",
    DEVICE_ROOT,
    4,
    L"compaq/deskpro386",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &p3_386_info,
    NULL
};


static const machine_t dp_386_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,
    MACHINE_VIDEO,
    1, 14, 1, 128, -1,
    {{"Intel",cpus_i386DX},{"AMD",cpus_Am386DX},{"Cyrix",cpus_486DLC}}
};

const device_t m_cpq_dp_386 = {
    "Compaq Deskpro/386",
    DEVICE_ROOT,
    5,
    L"compaq/deskpro386",
    cpq_init, cpq_close, NULL,
    NULL, NULL, NULL,
    &dp_386_info,
    NULL
};
