/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of ISA-based PS/2 machines.
 *
 * Version:	@(#)m_ps2_isa.c	1.0.23	2021/03/16
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
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/dma.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/port92.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h" 
#include "machine.h"


typedef struct {
    uint8_t	type;

    uint8_t	reg_94,
		reg_102,
		reg_103,
		reg_104,
		reg_105,
		reg_190;

    uint8_t	hd_status,
		hd_istat;
    uint8_t	hd_attn,
		hd_ctrl;
} ps2_t;


static uint8_t
ps2_in(uint16_t port, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0091:
		ret = 0;
		break;

	case 0x0094:
		ret = dev->reg_94;
		break;

	case 0x0102:
		ret = dev->reg_102 | 8;
		break;

	case 0x0103:
		ret = dev->reg_103;
		break;

	case 0x0104:
		ret = dev->reg_104;
		break;

	case 0x0105:
		ret = dev->reg_105;
		break;

	case 0x0190:
		ret = dev->reg_190;
		break;

	case 0x0322:
		ret = dev->hd_status;
		break;

	case 0x0324:
		ret = dev->hd_istat;
		dev->hd_istat &= ~0x02;
		break;

	default:
		break;
    }

    return(ret);
}


static void
ps2_out(uint16_t port, uint8_t val, priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    switch (port) {
	case 0x0094:
		dev->reg_94 = val;
		break;

	case 0x0102:
		if (val & 0x04)
			serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);
#if 0
		else
			serial_remove(0);
#endif
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
		dev->reg_102 = val;
		break;

	case 0x0103:
		dev->reg_103 = val;
		break;

	case 0x0104:
		dev->reg_104 = val;
		break;

	case 0x0105:
		dev->reg_105 = val;
		break;

	case 0x0190:
		dev->reg_190 = val;
		break;

	case 0x0322:
		dev->hd_ctrl = val;
		if (val & 0x80)
			dev->hd_status |= 0x02;
		break;

	case 0x0324:
		dev->hd_attn = val & 0xf0;
		if (dev->hd_attn)
			dev->hd_status = 0x14;
		break;
    }
}


static void
common_init(ps2_t *dev)
{
    io_sethandler(0x0091, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);
    io_sethandler(0x0094, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);
    io_sethandler(0x0102, 4, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);
    io_sethandler(0x0190, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);

    io_sethandler(0x0320, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);
    io_sethandler(0x0322, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);
    io_sethandler(0x0324, 1, ps2_in,NULL,NULL, ps2_out,NULL,NULL, dev);

    device_add_parent(&port92_device, (priv_t)dev);

    dev->reg_190 = 0;

    parallel_setup(0, 0x03bc);
}


static void
ps2_close(priv_t priv)
{
    ps2_t *dev = (ps2_t *)priv;

    free(dev);
}


static priv_t
ps2_init(const device_t *info, void *arg)
{
    ps2_t *dev;

    dev = (ps2_t *)mem_alloc(sizeof(ps2_t));
    memset(dev, 0x00, sizeof(ps2_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    machine_common_init();

    /* Set up our DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_at_refresh_timer);

    dma16_init();
    pic2_init();

    device_add(&ps_nvr_device);

    device_add(&keyboard_ps2_ps2_device);

    device_add(&fdc_at_ps1_device);

    switch (dev->type) {
	case 0:		/* Model 30/286 */
		device_add(&vga_ps1_device);
		break;
    }

    common_init(dev);

    return(dev);
}


static const CPU cpus_ps2_m30_286[] = {
    { "286/10", CPU_286, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1 },
    { "286/12", CPU_286, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2 },
    { "286/16", CPU_286, 16000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2 },
    { "286/20", CPU_286, 20000000, 1, 0, 0, 0, 0, 0, 4,4,4,4, 3 },
    { NULL }
};

static const machine_t m30_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_FDC_PS2 | MACHINE_HDC_PS2,
    MACHINE_VIDEO,
    1, 16, 1, 64, -1,
    {{"",cpus_ps2_m30_286}}
};

const device_t m_ps2_m30_286 = {
    "IBM PS/2 M30/286",
    DEVICE_ROOT,
    0,
    L"ibm/ps2_m30_286",
    ps2_init, ps2_close, NULL,
    NULL, NULL, NULL,
    &m30_info,
    NULL
};
