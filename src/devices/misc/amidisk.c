/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the AMI DISK BIOS Extensions LBA card.
 *
 * Version:	@(#)amidisk.c	1.0.1	2021/02/15
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2021 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../rom.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "amidisk.h"


#define BIOS_ROM_PATH		L"misc/amidisk11.bin"


typedef struct {
    const char	*name;

    rom_t	bios;
    uint32_t	bios_addr,
		bios_size;
} ami_t;


static void
ami_close(priv_t priv)
{
    ami_t *dev = (ami_t *)priv;

    free(dev);
}


static priv_t
ami_init(const device_t *info, UNUSED(void *parent))
{
    ami_t *dev;

    dev = (ami_t *)mem_alloc(sizeof(ami_t));
    memset(dev, 0x00, sizeof(ami_t));
    dev->name = info->name;

    dev->bios_addr = device_get_config_hex20("bios_addr");

    if ((dev->bios_addr > 0) && (info->path != NULL)) {
	rom_init(&dev->bios, info->path,
		 dev->bios_addr, 0x4000, 0x3fff, 0, MEM_MAPPING_EXTERNAL);
    }

    INFO("%s: ", dev->name);
    if (dev->bios_addr != 0x000000)
	INFO("ADDR=%06x", dev->bios_addr);
      else
	INFO("DISABLED");
    INFO("\n");

    return(dev);
}


static const device_config_t ami_config[] = {
    {
	"bios_addr", "BIOS address", CONFIG_HEX20, "", 0x0d0000,
	{
		{
			"Disabled", 0
		},
		{
			"C800H", 0x0c8000
		},
		{
			"CC00H", 0x0cc000
		},
		{
			"D000H", 0x0d0000
		},
		{
			"D400H", 0x0d4000
		},
		{
			"D800H", 0x0d8000
		},
		{
			"DC00H", 0x0dc000
		},
		{
			NULL
		}
	}
    },
    {
	NULL,
    }
};


const device_t amidisk_device = {
    "AMI DISK IDE Extension Card",
    DEVICE_ISA,
    0,
    BIOS_ROM_PATH,
    ami_init, ami_close, NULL,
    NULL, NULL, NULL, NULL,
    ami_config
};
