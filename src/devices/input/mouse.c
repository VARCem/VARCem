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
 * Version:	@(#)mouse.c	1.0.21	2019/05/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog mouse_log
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../plat.h"
#include "mouse.h"


#ifdef ENABLE_MOUSE_LOG
int	mouse_do_log = ENABLE_MOUSE_LOG;
#endif
int	mouse_x,
	mouse_y,
	mouse_z,
	mouse_buttons;


static const struct {
    const char		*internal_name;
    const device_t	*device;
} devices[] = {
    { "none",		NULL				},
    { "internal",	NULL				},

    { "logibus",	&mouse_logibus_device		},
    { "msbus",		&mouse_msinport_device		},
    { "mssystems",	&mouse_mssystems_device		},
    { "msserial",	&mouse_msserial_device		},
    { "ltserial",	&mouse_ltserial_device		},
    { "mswhserial",	&mouse_mswhserial_device	},
    { "ps2",		&mouse_ps2_device		},

    { NULL,		NULL				}
};


static int	mouse_nbut;
static priv_t	mouse_priv;
static int	(*mouse_func)(int,int,int,int,priv_t);


#ifdef _LOGGING
void
mouse_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_MOUSE_LOG
    va_list ap;

    if (mouse_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
mouse_close(void)
{
    mouse_nbut = 0;

    mouse_x = mouse_y = mouse_z = 0;
    mouse_buttons = 0x00;

    mouse_priv = NULL;
    mouse_func = NULL;
}


void
mouse_reset(void)
{
    /* Nothing to do if no mouse, or a machine-internal one. */
    if (config.mouse_type <= MOUSE_INTERNAL) return;

    INFO("MOUSE: reset(type=%i, '%s')\n",
	 config.mouse_type, devices[config.mouse_type].device->name);

    /* Clear local data. */
    mouse_x = mouse_y = mouse_z = 0;
    mouse_buttons = 0x00;
    mouse_priv = NULL;
    mouse_func = NULL;

    /* Initialize the mouse device. */
    if (devices[config.mouse_type].device != NULL) {
	mouse_priv = device_add(devices[config.mouse_type].device);
	mouse_func = (int (*)(int,int,int,int,void *))
			devices[config.mouse_type].device->ms_poll;
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

    if ((mouse_priv == NULL) ||		/* no or no active device */
	(config.mouse_type == MOUSE_NONE)) return;

    if (--poll_delay) return;

    mouse_poll();

    if (mouse_func != NULL) {
    	if (! mouse_func(mouse_x,mouse_y,mouse_z,mouse_buttons, mouse_priv)) {
		/* Poll failed, maybe port closed? */
		mouse_close();

		ERRLOG("MOUSE: device closed, mouse disabled!\n");

		return;
	}

	/* Reset mouse deltas. */
	mouse_x = mouse_y = mouse_z = 0;
    }

    poll_delay = 2;
}


void
mouse_set_poll(int (*func)(int,int,int,int,priv_t), priv_t arg)
{
    if (config.mouse_type != MOUSE_INTERNAL) return;

    mouse_func = func;
    mouse_priv = arg;
}


const char *
mouse_get_name(int mouse)
{
    if (devices[mouse].device != NULL)
	return(devices[mouse].device->name);

    return(NULL);
}


const char *
mouse_get_internal_name(int mouse)
{
    return(devices[mouse].internal_name);
}


int
mouse_get_from_internal_name(const char *s)
{
    int c;

    for (c = 0; devices[c].internal_name != NULL; c++)
	if (! strcmp(devices[c].internal_name, s))
		return(c);

    /* Not found. */
    return(0);
}


int
mouse_has_config(int mouse)
{
    if (devices[mouse].device != NULL)
	return(devices[mouse].device->config ? 1 : 0);

    return(0);
}


const device_t *
mouse_get_device(int mouse)
{
    return(devices[mouse].device);
}


int
mouse_get_buttons(void)
{
    return(mouse_nbut);
}
