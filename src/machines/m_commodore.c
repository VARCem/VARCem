/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various Commodore systems.
 *
 * Version:	@(#)m_commodore.c	1.0.16	2020/09/28
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/scamp.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/ports/parallel.h"
#include "../devices/ports/serial.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "machine.h"


typedef struct {
    int		type;
} pc30_t;


static void
pc30_write(uint16_t port, uint8_t val, priv_t priv)
{
    switch (val & 0x03) {
	case 1:
		parallel_setup(0, 0x03bc);
		break;

	case 2:
       	parallel_setup(0, 0x0378);
		break;

	case 3:
		parallel_setup(0, 0x0278);
		break;
    }

    switch (val & 0x0c) {
	case 0x04:
		serial_setup(0, 0x02f8, 3);
		break;

	case 0x08:
		serial_setup(0, 0x03f8, 4);
		break;
    }
}


static void
pc30_close(priv_t priv)
{
    pc30_t *dev = (pc30_t *)priv;

    free(dev);
}

static priv_t
common_init(const device_t *info, void *arg)
{
    pc30_t *dev;

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    switch(info->local) {
        case 30: /* PC30 */
            dev = (pc30_t *)mem_alloc(sizeof(pc30_t));
            memset(dev, 0x00, sizeof(pc30_t));
            dev->type = info->local;
            m_at_ide_init();
            mem_remap_top(384);
            device_add(&fdc_at_device);
            io_sethandler(0x0230, 1,
		      NULL,NULL,NULL, pc30_write,NULL,NULL, NULL);
            break;
        case 53: /* SL386SX */
            device_add(&scamp_device);
            m_at_common_ide_init();
            device_add(&fdc_at_device);
            device_add(&keyboard_ps2_device);
            //device_add(&keyboard_at_device);
            if (config.mouse_type == MOUSE_INTERNAL)
			    device_add(&mouse_ps2_device);	
            if (config.video_card == VID_INTERNAL)
			    device_add(&gd5402_onboard_device);
            break;
    }

    return((priv_t)arg);
}


static const machine_t pc30_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    640, 16384, 128, 128, 8,
    {{"",cpus_286}}
};

const device_t m_cbm_pc30 = {
    "Commodore PC-30",
    DEVICE_ROOT,
    30,
    L"commodore/pc30",
    common_init, pc30_close, NULL,
    NULL, NULL, NULL,
    &pc30_info,
    NULL
};

static const machine_t sl386sx_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_PS2 | MACHINE_VIDEO | MACHINE_HDC | MACHINE_MOUSE,
    0,
    1024, 8192, 512, 128, 16,
        {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_cbm_sl386sx = {
    "Commodore SL-386SX",
    DEVICE_ROOT,
    53,
    L"commodore/sl386sx",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &sl386sx_info,
    NULL
};
