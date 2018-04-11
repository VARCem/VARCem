/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Common driver module for MOUSE devices.
 *
 * TODO:	Add the Genius bus- and serial mouse.
 *		Remove the '3-button' flag from mouse types.
 *
 * Version:	@(#)mouse.c	1.0.6	2018/04/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "emu.h"
#include "device.h"
#include "mouse.h"


typedef struct {
    const char		*internal_name;
    const device_t	*device;
} mouse_t;


int	mouse_type = 0;
int	mouse_x,
	mouse_y,
	mouse_z,
	mouse_buttons;


static const device_t mouse_none_device = {
    "Disabled",
    0, MOUSE_TYPE_NONE,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
static const device_t mouse_internal_device = {
    "Internal Mouse",
    0, MOUSE_TYPE_INTERNAL,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};


static const mouse_t mouse_devices[] = {
    { "none",		&mouse_none_device	},
    { "internal",	&mouse_internal_device	},
    { "logibus",	&mouse_logibus_device	},
    { "msbus",		&mouse_msinport_device	},
#if 0
    { "genibus",	&mouse_genibus_device	},
#endif
    { "mssystems",	&mouse_mssystems_device	},
    { "msserial",	&mouse_msserial_device	},
    { "ps2",		&mouse_ps2_device	},
    { NULL,		NULL			}
};


static void	*mouse_priv;
static int	mouse_nbut;
static device_t	mouse_dev;


/* Initialize the mouse module. */
void
mouse_init(void)
{
    /* Initialize local data. */
    mouse_x = mouse_y = mouse_z = 0;
    mouse_buttons = 0x00;

    mouse_type = MOUSE_TYPE_NONE;
    mouse_priv = NULL;
    mouse_nbut = 0;
}


void
mouse_close(void)
{
    mouse_priv = NULL;
    mouse_nbut = 0;
}


void
mouse_reset(void)
{
    /* Nothing to do if no mouse, or a machine-internal one. */
    if (mouse_type <= MOUSE_TYPE_INTERNAL) return;

    pclog("MOUSE: reset(type=%d, '%s')\n",
	mouse_type, mouse_devices[mouse_type].device->name);

    /* Clear local data. */
    mouse_x = mouse_y = mouse_z = 0;
    mouse_buttons = 0x00;

    /* Copy the (R/O) mouse info. */
    if (mouse_devices[mouse_type].device != NULL) {
	memcpy(&mouse_dev, mouse_devices[mouse_type].device,
					sizeof(device_t));
	mouse_priv = device_add(&mouse_dev);
    }
}


/* Callback from the hardware driver. */
void
mouse_set_buttons(int buttons)
{
    mouse_nbut = buttons;
}


void
mouse_process(void)
{
    static int poll_delay = 2;

    if (mouse_type == MOUSE_TYPE_NONE) return;

    if (--poll_delay) return;

    mouse_poll();

    if (mouse_dev.available != NULL) {
    	mouse_dev.available(mouse_x,mouse_y,mouse_z,mouse_buttons, mouse_priv);

	/* Reset mouse deltas. */
	mouse_x = mouse_y = mouse_z = 0;
    }

    poll_delay = 2;
}


void
mouse_set_poll(int (*func)(int,int,int,int,void *), void *arg)
{
    if (mouse_type != MOUSE_TYPE_INTERNAL) return;

    mouse_dev.available = func;
    mouse_priv = arg;
}


const char *
mouse_get_name(int mouse)
{
    return(mouse_devices[mouse].device->name);
}


const char *
mouse_get_internal_name(int mouse)
{
    return(mouse_devices[mouse].internal_name);
}


int
mouse_get_from_internal_name(const char *s)
{
    int c = 0;

    while (mouse_devices[c].internal_name != NULL) {
	if (! strcmp(mouse_devices[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(0);
}


int
mouse_has_config(int mouse)
{
    if (mouse_devices[mouse].device == NULL) return(0);

    return(mouse_devices[mouse].device->config ? 1 : 0);
}


const device_t *
mouse_get_device(int mouse)
{
    return(mouse_devices[mouse].device);
}


int
mouse_get_buttons(void)
{
    return(mouse_nbut);
}


/* Return number of MOUSE types we know about. */
int
mouse_get_ndev(void)
{
    return((sizeof(mouse_devices)/sizeof(mouse_t)) - 1);
}
