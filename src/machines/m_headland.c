/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the HEADLAND AT286 chipset.
 *
 * **NOTE**	We need 'device continuation' implemented for this machine,
 *		so we can add configuration dialog for the onboard video
 *		controller for the AMA machine.
 *
 * Version:	@(#)m_headland.c	1.0.12	2019/04/08
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Original by GreatPsycho for PCem.
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2017,2018 Miran Grca.
 *		Copyright 2017,2018 GreatPsycho.
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
#include "../cpu/cpu.h"
#include "../cpu/x86.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/chipsets/headland.h"
#include "../devices/input/keyboard.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/video/video.h"
#include "machine.h"


typedef struct {
    int		type;

    rom_t	vid_bios;
} headland_t;


static void
headland_close(void *priv)
{
    headland_t *dev = (headland_t *)priv;

    free(dev);
}


static void *
headland_init(const device_t *info, void *arg)
{
    romdef_t *roms = (romdef_t *)arg;
    headland_t *dev;

    dev = (headland_t *)mem_alloc(sizeof(headland_t));
    memset(dev, 0x00, sizeof(headland_t));
    dev->type = info->local;

    /* Add machine device to system. */
    device_add_ex(info, dev);

    m_at_common_init();

    device_add(&keyboard_at_ami_device);

    device_add(&fdc_at_device);

    device_add(&ide_isa_device);

    switch(dev->type) {
	case 0:		/* Trigem 286M */
		device_add(&headland_device);
		break;

	case 11:	/* Headland 386SX */
		device_add(&headland_386_device);
		break;

	case 12:	/* Arche Technologies AMA-932J-25 */
		device_add(&headland_386_device);
		if (video_card == VID_INTERNAL) {
			/* Load the BIOS. */
			rom_init(&dev->vid_bios, roms->vidfn,
				 0xc0000, roms->vidsz, roms->vidsz - 1, 0, 0);

			/* Initialize the on-board controller. */
			device_add(&oti067_onboard_device);
		}
		break;
    }

    return(dev);
}


static const machine_t trigem_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    512, 8192, 128, 128, 8,
    {{"",cpus_286}}
};
	
const device_t m_tg286m = {
    "Trigem 286M",
    DEVICE_ROOT,
    1,
    L"trigem/tg286m",
    headland_init, headland_close, NULL,
    NULL, NULL, NULL,
    &trigem_info,
    NULL
};


static const machine_t ami_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    512, 16384, 128, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_headland_386_ami = {
    "AMI 386SX (Headland)",
    DEVICE_ROOT,
    11,
    L"headland/386sx/ami",
    headland_init, headland_close, NULL,
    NULL, NULL, NULL,
    &ami_info,
    NULL
};


static const machine_t ama_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO,
    0,
    512, 8192, 128, 128, 16,
    {{"Intel",cpus_i386SX},{"AMD",cpus_Am386SX},{"Cyrix",cpus_486SLC}}
};

const device_t m_ama932j = {
    "Arche Technologies AMA-932J-25",
    DEVICE_ROOT,
    12,
    L"arche/ama932j",
    headland_init, headland_close, NULL,
    NULL, NULL, NULL,
    &ama_info,
    NULL
};
