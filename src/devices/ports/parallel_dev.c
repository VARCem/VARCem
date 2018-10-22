/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the parallel-port-attached devices.
 *
 * Version:	@(#)parallel_dev.c	1.0.9	2018/10/06
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include "../../emu.h"
#include "../../io.h"
#include "parallel.h"
#include "parallel_dev.h"

#include "../../devices/sound/snd_lpt_dac.h"
#include "../../devices/sound/snd_lpt_dss.h"
#include "../../devices/printer/printer.h"


static const struct {
    const char	*internal_name;
    const char	*name;
    const lpt_device_t *device;
} devices[] = {
    { "none",		"None",		NULL				},

    { "dss",		NULL,		&dss_device			},
    { "lpt_dac",	NULL,		&lpt_dac_device			},
    { "lpt_dac_stereo",	NULL,		&lpt_dac_stereo_device		},

#ifdef USE_PRINTER
    /* Experimental, WIP --FvK */
    { "lpt_printer",	NULL,		&lpt_printer_device		},
#endif

    { "lpt_prt_text",	NULL,		&lpt_prt_text_device		},

    { "lpt_prt_escp",	NULL,		&lpt_prt_escp_device		},

    { NULL,		NULL,		NULL				}
};


const char *
parallel_device_get_name(int id)
{
    if (devices[id].name != NULL)
	return(devices[id].name);

    if (devices[id].device != NULL)
	return(devices[id].device->name);

    return(NULL);
}


const char *
parallel_device_get_internal_name(int id)
{
    return(devices[id].internal_name);
}


const lpt_device_t *
parallel_device_get_device(int id)
{
    return(devices[id].device);
}


int
parallel_device_get_from_internal_name(const char *s)
{
    int c = 0;

    while (devices[c].internal_name != NULL) {
	if (! strcmp(devices[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(0);
}
