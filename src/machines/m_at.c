/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Standard PC/AT implementation.
 *
 * Version:	@(#)m_at.c	1.0.11	2019/02/16
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
#include "../mem.h"
#include "../device.h"
#include "../nvr.h"
#include "../devices/system/pic.h"
#include "../devices/system/pit.h"
#include "../devices/system/dma.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "machine.h"


void
m_at_common_init(const machine_t *model, void *arg)
{
    machine_common_init(model, arg);

    pit_set_out_func(&pit, 1, pit_refresh_timer_at);
    pic2_init();
    dma16_init();

    device_add(&at_nvr_device);
}


void
m_at_init(const machine_t *model, void *arg)
{
    m_at_common_init(model, arg);

    device_add(&keyboard_at_device);
}


void
m_at_ibm_init(const machine_t *model, void *arg)
{
    m_at_init(model, arg);

    mem_remap_top(384);

    device_add(&fdc_at_device);
}


void
m_at_ps2_init(const machine_t *model, void *arg)
{
    m_at_common_init(model, arg);

    device_add(&keyboard_ps2_device);
}


void
m_at_common_ide_init(const machine_t *model, void *arg)
{
    m_at_common_init(model, arg);

    device_add(&ide_isa_2ch_opt_device);
}


void
m_at_ide_init(const machine_t *model, void *arg)
{
    m_at_init(model, arg);

    device_add(&ide_isa_2ch_opt_device);
}


void
m_at_ps2_ide_init(const machine_t *model, void *arg)
{
    m_at_ps2_init(model, arg);

    device_add(&ide_isa_2ch_opt_device);
}
