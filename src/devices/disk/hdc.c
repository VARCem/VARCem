/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Common code to handle all sorts of disk controllers.
 *
 * Version:	@(#)hdc.c	1.0.14	2018/09/16
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog hdc_log
#include "../../emu.h"
#include "../../machines/machine.h"
#include "../../device.h"
#include "hdc.h"
#include "hdd.h"
#include "hdc_ide.h"


#ifdef ENABLE_HDC_LOG
int	hdc_do_log = ENABLE_HDC_LOG;
#endif


static const struct {
    const char		*internal_name;
    const device_t	*device;
} controllers[] = {
    { "none",			NULL				},
    { "internal",		NULL				},

    { "st506_xt",		&st506_xt_xebec_device		},
    { "st506_dtc5150x",		&st506_xt_dtc5150x_device	},

    { "xta_wdxt150",		&xta_wdxt150_device		},

    { "xtide",			&xtide_device			},
    { "xtide_acculogic",	&xtide_acculogic_device		},

    { "st506_at",		&st506_at_wd1003_device		},
    { "esdi_at",		&esdi_at_wd1007vse1_device	},

    { "ide_isa",		&ide_isa_device			},
    { "ide_isa_2ch",		&ide_isa_2ch_device		},

    { "xtide_at",		&xtide_at_device		},
    { "xtide_at_ps2",		&xtide_at_ps2_device		},

    { "esdi_mca",		&esdi_ps2_device		},

    { "ide_pci",		&ide_pci_device			},
    { "ide_pci_2ch",		&ide_pci_2ch_device		},

    { "vlb_isa",		&ide_vlb_device			},
    { "vlb_isa_2ch",		&ide_vlb_2ch_device		},

    { NULL,			NULL				}
};


void
hdc_log(int level, const char *fmt, ...)
{
#ifdef ENABLE_HDC_LOG
   va_list ap;

   if (hdc_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
   }
#endif
}


/* Initialize the HDC module. */
void
hdc_init(void)
{
    /* Zero all the hard disk image arrays. */
    hdd_image_init();
}


/* Reset the HDC, whichever one that is. */
void
hdc_reset(void)
{
    INFO("HDC: reset(current=%d, internal=%d)\n",
	 hdc_type, !!(machines[machine].flags & MACHINE_HDC));

    /* If we have a valid controller, add its device. */
    if (controllers[hdc_type].device != NULL)
	device_add(controllers[hdc_type].device);

    /* Now, add the tertiary and/or quaternary IDE controllers. */
    if (ide_ter_enabled)
	device_add(&ide_ter_device);
    if (ide_qua_enabled)
	device_add(&ide_qua_device);
}


const char *
hdc_get_name(int hdc)
{
    if (controllers[hdc].device != NULL)
	return(controllers[hdc].device->name);

    return(NULL);
}


const char *
hdc_get_internal_name(int hdc)
{
    return(controllers[hdc].internal_name);
}


int
hdc_get_from_internal_name(const char *s)
{
    int c = 0;

    while (controllers[c].internal_name != NULL) {
	if (! strcmp(controllers[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(0);
}


const device_t *
hdc_get_device(int hdc)
{
    return(controllers[hdc].device);
}


int
hdc_has_config(int hdc)
{
    const device_t *dev = hdc_get_device(hdc);

    if (dev == NULL) return(0);

    if (dev->config == NULL) return(0);

    return(1);
}


int
hdc_get_flags(int hdc)
{
    if (controllers[hdc].device != NULL)
	return(controllers[hdc].device->flags);

    return(0);
}


int
hdc_available(int hdc)
{
    return(device_available(controllers[hdc].device));
}
