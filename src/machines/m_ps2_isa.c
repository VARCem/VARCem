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
 * Version:	@(#)m_ps2_isa.c	1.0.15	2019/02/16
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
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/dma.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h" 
#include "machine.h"


static uint8_t ps2_94, ps2_102, ps2_103, ps2_104, ps2_105, ps2_190;


static struct {
    uint8_t status, int_status;
    uint8_t attention, ctrl;
} ps2_hd;


static uint8_t
ps2_read(uint16_t port, void *priv)
{
    uint8_t ret = 0xff;

    switch (port) {
	case 0x0091:
		ret = 0;
		break;

	case 0x0094:
		ret = ps2_94;
		break;

	case 0x0102:
		ret = ps2_102 | 8;
		break;

	case 0x0103:
		ret = ps2_103;
		break;

	case 0x0104:
		ret = ps2_104;
		break;

	case 0x0105:
		ret = ps2_105;
		break;

	case 0x0190:
		ret = ps2_190;
		break;

	case 0x0322:
		ret = ps2_hd.status;
		break;

	case 0x0324:
		ret = ps2_hd.int_status;
		ps2_hd.int_status &= ~0x02;
		break;

	default:
		break;
    }

    return ret;
}


static void
ps2_write(uint16_t port, uint8_t val, void *priv)
{
    switch (port) {
	case 0x0094:
		ps2_94 = val;
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
		ps2_102 = val;
		break;

	case 0x0103:
		ps2_103 = val;
		break;

	case 0x0104:
		ps2_104 = val;
		break;

	case 0x0105:
		ps2_105 = val;
		break;

	case 0x0190:
		ps2_190 = val;
		break;

	case 0x0322:
		ps2_hd.ctrl = val;
		if (val & 0x80)
			ps2_hd.status |= 0x02;
		break;

	case 0x0324:
		ps2_hd.attention = val & 0xf0;
		if (ps2_hd.attention)
			ps2_hd.status = 0x14;
		break;
    }
}


static void
ps2_common_init(void)
{
    io_sethandler(0x0091, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);
    io_sethandler(0x0094, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);
    io_sethandler(0x0102, 4, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);
    io_sethandler(0x0190, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);

    io_sethandler(0x0320, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);
    io_sethandler(0x0322, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);
    io_sethandler(0x0324, 1, ps2_read,NULL,NULL, ps2_write,NULL,NULL, NULL);

    port_92_reset();
    port_92_add();

    ps2_190 = 0;

    parallel_setup(0, 0x03bc);

    memset(&ps2_hd, 0x00, sizeof(ps2_hd));
}


void
m_ps2_m30_286_init(const machine_t *model, void *arg)
{
    machine_common_init(model, arg);

    device_add(&fdc_at_ps1_device);

    pit_set_out_func(&pit, 1, pit_refresh_timer_at);
    dma16_init();
    pic2_init();

    ps2_common_init();

    device_add(&keyboard_ps2_device);
    device_add(&ps_nvr_device);
    device_add(&ps1vga_device);
}
