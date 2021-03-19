/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the MultiTech-Thomson TO-16 machine.
 *
 * Version:	@(#)m_thomson.c	1.0.1	2021/03/18
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2020,2021 Altheos.
 *		Copyright 2017-2021 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "../plat.h"
#include "machine.h"


typedef struct {
    uint8_t	type;

    int8_t	basic;
    int8_t	floppy;
} to16_t;


static void
to_close(priv_t priv)
{
    free(priv);
}


static priv_t
to_init(const device_t *info, UNUSED(void *arg))
{
    to16_t *dev;

    /* Allocate private control block for machine. */
    dev = (to16_t *)mem_alloc(sizeof(to16_t));
    memset(dev, 0x00, sizeof(to16_t));
    dev->type = info->local;

    /* First of all, add the root device. */
    device_add_ex(info, (priv_t)dev);

    dev->floppy = machine_get_config_int("floppy");

    machine_common_init();

    nmi_init();

    /* Set up the DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_xt_refresh_timer);

    switch (dev->type) {
	case 16:	/* TO16 */
		if (config.video_card == VID_INTERNAL) {
			video_load_font(L"video/ibm/mda/mda.rom", 0);

			/* Real chip is Paradise PVC-2 */
			device_add(&colorplus_onboard_device);
			cga_palette = 0;
			video_palette_rebuild();
		}
		device_add(&keyboard_xt86_device);
		break;
    }

    /* Not entirely correct, they were optional. */
    if (dev->floppy)
	device_add(&fdc_xt_device);

    return(dev);
}


static const device_config_t to_config[] = {
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
    to_init, to_close, NULL,
    NULL, NULL, NULL,
    &to16_info,
    to_config
};
