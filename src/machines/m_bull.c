/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of various machines made by (Groupe) Bull.
 *
 * **NOTE**	This is not finished yet, and, therefore, still in Dev:
 *
 *		 - proper handling of the SW-1 and and SW-2 switches and
 *		   their readout. Fabien is looking into that.
 *		 - figure out how the NCR 83C80 chip is mapped in.
 *		 - figure out why the InPort is detected, but does not work.
 *		 - at some point, hopefully add the Paradise PGA2A chip.
 *
 *		Other than the above, the machine works as expected.
 *
 * Version:	@(#)m_bull.c	1.0.1	2019/04/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Idea from a patch for PCem by DNS2KV2, but fully rewritten.
 *		Many thanks to Fabien Neck for technical info, BIOS and tools.
 *
 *		Copyright 2019 Fred N. van Kempen.
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
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "../cpu/cpu.h"
#include "../io.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../devices/input/mouse.h"
#include "../devices/floppy/fdd.h"
#include "../devices/floppy/fdc.h"
#include "../devices/disk/hdc.h"
#include "../devices/scsi/scsi.h"
#include "../devices/scsi/scsi_ncr5380.h"
#include "../devices/video/video.h"
#include "machine.h"


static void *
common_init(const device_t *info, void *arg)
{
    int ega, mouse, scsi;
    void *priv;
    int irq;

    /* Allocate machine device to system. */
    device_add_ex(info, arg);

    ega = machine_get_config_int("pega");
    mouse = machine_get_config_int("mouse");
    scsi = machine_get_config_int("scsi");

    switch(info->local) {
	/* MICRAL45: Bull Micral 45/286@12 */
	case 45:
		m_at_init();

		/* Mainboard switch. */
		mem_remap_top(384);

		if (hdc_type == HDC_INTERNAL)
			device_add(&st506_at_wd1003_device);

		if (video_card == VID_INTERNAL && ega) {
			/* Paradise PGA2A, really! */
			device_add(&ega_onboard_device);
		}

		if (mouse_type == MOUSE_INTERNAL && mouse) {
			priv = device_add(&mouse_msinport_onboard_device);
			mouse_bus_set_irq(priv, mouse);
		}

		if (scsi_card == SCSI_INTERNAL && scsi) {
			priv = device_add(&scsi_ncr53c80_onboard_device);
			irq = machine_get_config_int("scsi_irq");
			scsi_ncr5380_set_info(priv, scsi, irq);
		}

		device_add(&fdc_at_device);

		break;
    }

    return(arg);
}


/*
 * Micral 45 motherboard jumpers and switches.
 *
 * SWD1	DIPswitch on back, 4-pos.
 * JD01	Jumper on mainboard.
 *
 *  Display Mode	SWD-1	SWD-2	SWD-3	SWD-4	JD01
 *  Mono 80		OFF	OFF	OFF	OFF	2-3
 *  Color 80		ON	OFF	OFF	OFF	2-3
 *  (Color 40)		OFF	ON	OFF	OFF	2-3
 *  EGA			OFF	ON	ON	OFF	1-2
 *
 * These switches and the jumper drive the Paradise PGA2A EGA
 * chip. More modes like Hercules and Plantronics are possible.
 *
 * SW1	DIPswitch on mainboard.
 *
 * SW1-2   SW1-1	Memory mode
 *  OFF    OFF		Total 1152 KB
 *  OFF    ON		Total 640K
 *  ON     OFF		Extended Memory enabled
 *  ON     ON		Bank2 = 2MB
 *
 * SW1-3   Reserved
 *
 * SW1-5   SW1-4	Video controller
 *  OFF     X		Enable internal EGA controller
 *  ON      OFF		Enable external CGA
 *  ON      ON		Enable external MDA
 *
 * SW1-6   Reserved
 * SW1-7   Reserved
 * SW1-8   Reserved
 *
 * SW2  DIPswitch on mainboard.
 *
 *  SW2-3  SW2-2  SW2-1	Mouse mode
 *   X      X      ON	Disable mouse
 *   ON     OFF    OFF  Enable mouse, IRQ3 *
 *   OFF    ON     OFF  Enable mouse, IRQ5
 *   
 * SW2-4  Various
 *  ON    CP8 (SmartCard reader) at 370H, serial at 2F8H, parallel at 278H
 *
 * SW2-5  SCSI controller I/O
 *  OFF    controller at 320H *
 *  ON     controller at 328H
 *
 * SW2-7 SW2-6  SCSI controller IRQ
 *  ON    OFF    SCSI uses IRQ7 *
 *  OFF   ON     SCSI uses IRQ15
 *
 * SW2-9 SW2-8  Parallel port IRQ
 *  ON    OFF   Enable parallel port IRQ5 *
 *  OFF   ON    Enable parallel port IRQ7
 *
 * SW2-10 CP8 IRQ
 *  ON     Enable CP8 IRQ3 *
 *
 * These are somewhat configuring, and need to be verified.
 */
static const device_config_t m45_config[] = {
    {
	"ega", "Internal EGA", CONFIG_SELECTION, "", 0,
	{	/* This is SW1-5 on mainboard. */
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
	"mouse", "Internal Mouse", CONFIG_SELECTION, "", 3,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled, using IRQ3", 3
		},
		{
			"Enabled, using IRQ5", 5
		},
		{
			NULL
		}
	}
    },
    {
	"scsi", "Internal SCSI", CONFIG_SELECTION, "", 0,
	{
		{
			"Disabled", 0
		},
		{
			"320H", 0x320
		},
		{
			"328H", 0x328
		},
		{
			NULL
		}
	}
    },
    {
	"scsi_irq", "Internal SCSI IRQ", CONFIG_SELECTION, "", 7,
	{
		{
			"Disabled", 0
		},
		{
			"IRQ7", 7
		},
		{
			"IRQ15", 15
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

static const machine_t m45_info = {
    MACHINE_ISA | MACHINE_AT | MACHINE_HDC | MACHINE_VIDEO | MACHINE_MOUSE | MACHINE_SCSI,
    MACHINE_HDC,
    1024, 6144, 512, 128, -1,
    {{"Intel",cpus_286}}
};

const device_t m_bull_micral45 = {
    "Bull Micral 45",
    DEVICE_ROOT,
    45,
    L"bull/micral45",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &m45_info,
    m45_config
};
