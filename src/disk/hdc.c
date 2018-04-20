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
 * Version:	@(#)hdc.c	1.0.8	2018/04/14
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
#include <wchar.h>
#define HAVE_STDARG_H
#include "../emu.h"
#include "../machine/machine.h"
#include "../device.h"
#include "hdc.h"
#include "hdc_ide.h"


#ifdef ENABLE_HDC_LOG
int	hdc_do_log = ENABLE_HDC_LOG;
#endif


static void *
null_init(const device_t *info)
{
    return(NULL);
}


static void
null_close(void *priv)
{
}


static const device_t null_device = {
    "Null HDC", 0, 0,
    null_init, null_close, NULL,
    NULL, NULL, NULL, NULL, NULL
};


static void *
inthdc_init(const device_t *info)
{
    return(NULL);
}


static void
inthdc_close(void *priv)
{
}


static const device_t inthdc_device = {
    "Internal Controller", 0, 0,
    inthdc_init, inthdc_close, NULL,
    NULL, NULL, NULL, NULL, NULL
};


static const struct {
    const char		*name;
    const char		*internal_name;
    const device_t	*device;
    int			is_mfm;
} controllers[] = {
    { "Disabled",					"none",		
      &null_device,					0		},

    { "Internal Controller",				"internal",
      &inthdc_device,					0		},

    { "[ISA] [MFM] IBM PC Fixed Disk Adapter",		"mfm_xt",
      &mfm_xt_xebec_device,				1		},

    { "[ISA] [MFM] DTC-5150X Fixed Disk Adapter",	"mfm_dtc5150x",
      &mfm_xt_dtc5150x_device,				1		},

    { "[ISA] [MFM] IBM PC/AT Fixed Disk Adapter",	"mfm_at",
      &mfm_at_wd1003_device,				1		},

    { "[ISA] [ESDI] PC/AT ESDI Fixed Disk Adapter",	"esdi_at",
      &esdi_at_wd1007vse1_device,			0		},

#ifdef USE_XTA
    { "[ISA] [IDE] PC/XT IDE (XTA) Adapter",		"xta_isa",
      &xta_isa_device,					0,		},
#endif

    { "[ISA] [IDE] PC/XT XTIDE (ATA)",			"xtide",
      &xtide_device,					0		},

    { "[ISA] [IDE] PC/XT Acculogic XT IDE",		"xtide_acculogic",
      &xtide_acculogic_device,				0		},

    { "[ISA] [IDE] PC/AT IDE (ATA) Adapter",		"ide_isa",
      &ide_isa_device,					0		},

    { "[ISA] [IDE] PC/AT IDE Adapter (Dual-Channel)",	"ide_isa_2ch",
      &ide_isa_2ch_device,				0		},

    { "[ISA] [IDE] PC/AT XTIDE",			"xtide_at",
      &xtide_at_device,					0		},

    { "[ISA] [IDE] PS/2 AT XTIDE (1.1.5)",		"xtide_at_ps2",
      &xtide_at_ps2_device,				0		},

    { "[MCA] [ESDI] IBM PS/2 ESDI Fixed Disk Adapter","esdi_mca",
      &esdi_ps2_device,					1		},

    { "[PCI] [IDE] PCI IDE Adapter",			"ide_pci",
      &ide_pci_device,					0		},

    { "[PCI] [IDE] PCI IDE Adapter (Dual-Channel)",	"ide_pci_2ch",
      &ide_pci_2ch_device,				0		},

    { "[VLB] [IDE] PC/AT IDE Adapter",			"vlb_isa",
      &ide_vlb_device,					0		},

    { "[VLB] [IDE] PC/AT IDE Adapter (Dual-Channel)",	"vlb_isa_2ch",
      &ide_vlb_2ch_device,				0		},

    { NULL,						NULL, NULL, 0	}
};


void
hdc_log(const char *fmt, ...)
{
#ifdef ENABLE_IDE_LOG
   va_list ap;

   if (hdc_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
   }
#endif
}


/* Reset the HDC, whichever one that is. */
void
hdc_reset(void)
{
#ifdef ENABLE_HDC_LOG
    hdc_log("HDC: reset(current=%d, internal=%d)\n",
	hdc_type, (machines[machine].flags & MACHINE_HDC)?1:0);
#endif

    /* If we have a valid controller, add its device. */
    if (hdc_type > 1)
	device_add(controllers[hdc_type].device);

    /* Reconfire and reset the IDE layer. */
    ide_ter_disable();
    ide_qua_disable();
    if (ide_enable[2])
	ide_ter_init();
    if (ide_enable[3])
	ide_qua_init();
    ide_reset_hard();
}


const char *
hdc_get_name(int hdc)
{
    return(controllers[hdc].name);
}


const char *
hdc_get_internal_name(int hdc)
{
    return(controllers[hdc].internal_name);
}


const device_t *
hdc_get_device(int hdc)
{
    return(controllers[hdc].device);
}


int
hdc_get_flags(int hdc)
{
    return(controllers[hdc].device->flags);
}


int
hdc_available(int hdc)
{
    return(device_available(controllers[hdc].device));
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
