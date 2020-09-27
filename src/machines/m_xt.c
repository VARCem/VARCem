/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of standard IBM PC/XT class machine.
 *
 * Version:	@(#)m_xt.c	1.0.21	2020/09/26
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
#include "../mem.h"
#include "../device.h"
#include "../devices/system/nmi.h"
#include "../devices/system/dma.h"
#include "../devices/system/pit.h"
#include "../devices/input/keyboard.h"
#include "../devices/ports/parallel.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/video/video.h"
#ifdef USE_CASSETTE
# include <cassette.h>
#endif
#include "machine.h"


typedef struct {
    uint8_t	type;

    int8_t	basic;
    int8_t	floppy;
} pcxt_t;


static void
xt_close(priv_t priv)
{
    free(priv);
}


/* Generic PC/XT system board with just the basics. */
static priv_t
xt_common_init(const device_t *info, void *arg)
{
    pcxt_t *dev;

    /* Allocate private control block for machine. */
    dev = (pcxt_t *)mem_alloc(sizeof(pcxt_t));
    memset(dev, 0x00, sizeof(pcxt_t));
    dev->type = info->local;

    /* First of all, add the root device. */
    device_add_ex(info, (priv_t)dev);

    /* Check if we support a BASIC ROM. */
    dev->basic = machine_get_config_int("rom_basic");
    dev->floppy = machine_get_config_int("floppy");

    machine_common_init();

    nmi_init();

    /* Set up the DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_xt_refresh_timer);

    switch(dev->type) {
	case 0:		/* PC, 1981 */
	case 1:		/* PC, 1982 */
		if (dev->type == 1)
			device_add(&keyboard_pc82_device);
		else
			device_add(&keyboard_pc_device);
#ifdef USE_CASSETTE
		device_add(&cassette_device);
#endif
		break;

	case 10:	/* XT, 1982 */
	case 11:	/* XT, 1986 */
		if (dev->type == 11)
			device_add(&keyboard_xt86_device);
		else
			device_add(&keyboard_xt_device);
		break;

    case 106: /* TO16 */
        if (config.video_card == VID_INTERNAL)
            device_add(&colorplus_onboard_device); /* Real chip is Paradise PVC-2 */
        device_add(&keyboard_xt86_device);
        break;

	default:	/* clones */
		device_add(&keyboard_xt86_device);
    }

    /* Not entirely correct, they were optional. */
    if (dev->floppy)
	device_add(&fdc_xt_device);

    return((priv_t)dev);
}


static const device_config_t xt_config[] = {
    {
	"rom_basic", "ROM BASIC", CONFIG_SELECTION, "", 0,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled", 1
		},
		{
			NULL
		}
	}
    },
    {
	"floppy", "Floppy Controller", CONFIG_SELECTION, "", 1,
	{
		{
			"Not present", 0
		},
		{
			"Present", 1
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


static const machine_t pc81_info = {
    MACHINE_ISA,
    0,
    16, 256, 16, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_pc81 = {
    "IBM PC (1981)",
    DEVICE_ROOT,
    0,
    L"ibm/pc",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &pc81_info,
    xt_config
};


static const machine_t pc82_info = {
    MACHINE_ISA,
    0,
    64, 256, 32, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_pc82 = {
    "IBM PC (1982)",
    DEVICE_ROOT,
    1,
    L"ibm/pc82",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &pc82_info,
    xt_config
};


static const machine_t xt82_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt82 = {
    "PC/XT (1982)",
    DEVICE_ROOT,
    10,
    L"ibm/xt",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &xt82_info,
    xt_config
};


static const machine_t xt86_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt86 = {
    "PC/XT (1986)",
    DEVICE_ROOT,
    11,
    L"ibm/xt86",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &xt86_info,
    xt_config
};


static const machine_t ami_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_ami = {
    "XT (AMI, generic)",
    DEVICE_ROOT,
    100,
    L"generic/xt/ami",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &ami_info,
    xt_config
};


static const machine_t award_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_award = {
    "XT (Award, generic)",
    DEVICE_ROOT,
    101,
    L"generic/xt/award",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &award_info,
    xt_config
};


static const machine_t phoenix_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_phoenix = {
    "XT (generic, Phoenix)",
    DEVICE_ROOT,
    102,
    L"generic/xt/phoenix",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &phoenix_info,
    xt_config
};


static const machine_t openxt_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_openxt = {
    "XT (generic, OpenXT)",
    DEVICE_ROOT,
    103,
    L"generic/xt/open_xt",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &openxt_info,
    xt_config
};


static const machine_t dtk_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_dtk = {
    "DTK Data-1000 Turbo XT",
    DEVICE_ROOT,
    104,
    L"dtk/xt",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &dtk_info,
    xt_config
};


static const machine_t juko_info = {
    MACHINE_ISA,
    0,
    64, 640, 64, 0, 0,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xt_juko = {
    "Juko XT",
    DEVICE_ROOT,
    105,
    L"juko/pc",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &juko_info,
    xt_config
};

static const machine_t to16_info = {
    MACHINE_ISA | MACHINE_VIDEO,
    0,
    512, 640, 128, 16, 0,
    {{"Intel",cpus_8088}}
};

const device_t m_thom_to16 = {
    "Thomson TO16",
    DEVICE_ROOT,
    106,
    L"thomson/to16",
    xt_common_init, xt_close, NULL,
    NULL, NULL, NULL,
    &to16_info,
    xt_config
};


/* Start a DRAM refresh cycle by issuing a DMA read. */
void
m_xt_refresh_timer(int new_out, int old_out)
{
    if (new_out && !old_out)
        dma_channel_read(0);
}
