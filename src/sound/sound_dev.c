/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Sound devices support module.
 *
 * Version:	@(#)sound_dev.c	1.0.3	2018/04/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include "../emu.h"
#include "../device.h"
#include "../plat.h"
#include "sound.h"


typedef struct {
    const char		*name;
    const char		*internal_name;
    const device_t	*device;
} sound_t;


#ifdef ENABLE_SOUND_DEV_LOG
int		sound_dev_do_log = ENABLE_SOUND_DEV_LOG;
#endif
int		sound_card_current = 0;


/* Sound card devices. */
extern const device_t adlib_device;
extern const device_t adlib_mca_device;
extern const device_t adgold_device;
extern const device_t es1371_device;
extern const device_t cms_device;
extern const device_t gus_device;
#if defined(DEV_BRANCH) && defined(USE_PAS16)
extern const device_t pas16_device;
#endif
extern const device_t sb_1_device;
extern const device_t sb_15_device;
extern const device_t sb_mcv_device;
extern const device_t sb_2_device;
extern const device_t sb_pro_v1_device;
extern const device_t sb_pro_v2_device;
extern const device_t sb_pro_mcv_device;
extern const device_t sb_16_device;
extern const device_t sb_awe32_device;
extern const device_t ssi2001_device;
extern const device_t wss_device;


static const sound_t	sound_cards[] = {
    {"Disabled",			"none",		NULL		},
    {"[ISA] Adlib",			"adlib",	&adlib_device	},
    {"[ISA] Adlib Gold",		"adlibgold",	&adgold_device	},
    {"[ISA] Creative Music System",	"cms",		&cms_device	},
    {"[ISA] Gravis Ultra Sound",	"gus",		&gus_device	},
    {"[ISA] Innovation SSI-2001",	"ssi2001",	&ssi2001_device	},
    {"[ISA] Sound Blaster 1.0",		"sb",		&sb_1_device	},
    {"[ISA] Sound Blaster 1.5",		"sb1.5",	&sb_15_device	},
    {"[ISA] Sound Blaster 2.0",		"sb2.0",	&sb_2_device	},
    {"[ISA] Sound Blaster Pro v1",	"sbprov1",	&sb_pro_v1_device},
    {"[ISA] Sound Blaster Pro v2",	"sbprov2",	&sb_pro_v2_device},
    {"[ISA] Sound Blaster 16",		"sb16",		&sb_16_device	},
    {"[ISA] Sound Blaster AWE32",	"sbawe32",	&sb_awe32_device},
#if defined(DEV_BRANCH) && defined(USE_PAS16)
    {"[ISA] Pro Audio Spectrum 16",	"pas16",	&pas16_device	},
#endif
    {"[ISA] Windows Sound System",	"wss",		&wss_device	},
    {"[MCA] Adlib",			"adlib_mca",	&adlib_mca_device},
    {"[MCA] Sound Blaster MCV",		"sbmcv",	&sb_mcv_device	},
    {"[MCA] Sound Blaster Pro MCV",	"sbpromcv",	&sb_pro_mcv_device},
    {"[PCI] Ensoniq AudioPCI (ES1371)",	"es1371",	&es1371_device	},
    {"[PCI] Sound Blaster PCI 128",	"sbpci128",	&es1371_device	},
    {NULL,				NULL,		NULL		}
};


void
snddev_log(const char *fmt, ...)
{
#ifdef ENABLE_SOUND_DEV_LOG
    va_list ap;

    if (sound_dev_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


void
snddev_reset(void)
{
    if (sound_cards[sound_card_current].device != NULL)
	device_add(sound_cards[sound_card_current].device);
}


int
sound_card_available(int card)
{
    if (sound_cards[card].device != NULL)
	return(device_available(sound_cards[card].device));

    return(1);
}


const char *
sound_card_getname(int card)
{
    return(sound_cards[card].name);
}


const device_t *
sound_card_getdevice(int card)
{
    return(sound_cards[card].device);
}


int
sound_card_has_config(int card)
{
    if (sound_cards[card].device == NULL) return(0);

    return(sound_cards[card].device->config ? 1 : 0);
}


const char *
sound_card_get_internal_name(int card)
{
    return(sound_cards[card].internal_name);
}


int
sound_card_get_from_internal_name(const char *s)
{
    int c = 0;

    while (sound_cards[c].internal_name != NULL) {
	if (! strcmp(sound_cards[c].internal_name, s))
		return(c);
	c++;
    }

    /* Not found. */
    return(-1);
}
