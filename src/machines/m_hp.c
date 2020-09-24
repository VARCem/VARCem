/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various HP machines.
 *
 *
 * Version:	@(#)m_hp.c	1.0.0	2020/09/20
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *			Altheos
 *
 *		Copyright 2020 Fred N. van Kempen.
 *		Copyright 2020 Altheos.
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
#include "../config.h"
#include "../timer.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/vl82c480.h"
#include "../devices/system/memregs.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/sio/sio.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/disk/hdc_ide.h"
#include "../devices/video/video.h"
#include "machine.h"


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Allocate machine device to system. */
    device_add_ex(info, (priv_t)arg);

	device_add(&vl82c480_device);
	m_at_common_ide_init(); /* This machine has only one IDE port */
	device_add(&keyboard_ps2_device);
	if (config.video_card == VID_INTERNAL)
			device_add(&gd5428_onboard_vlb_device);	
	if (config.mouse_type == MOUSE_INTERNAL)
			device_add(&mouse_ps2_device);	

    return((priv_t)arg);
}


static const machine_t hpv486_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT | MACHINE_PS2 | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE,
    0,
    1, 32, 1, 128, -1,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}}
};

const device_t m_pb640 = {
    "HP Vectra 486/33VL",
    DEVICE_ROOT,
    NULL,
    L"hp/vectra486",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &hpv486_info,
    NULL		/* &gd5428_onboard_vlb_device */
};
