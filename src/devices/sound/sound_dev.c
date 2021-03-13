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
 * Version:	@(#)sound_dev.c	1.0.14	2020/12/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#define dbglog sound_card_log
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../plat.h"
#include "sound.h"


#ifdef ENABLE_SOUND_DEV_LOG
int	sound_card_do_log = ENABLE_SOUND_DEV_LOG;
#endif


/* Sound card devices. */
extern const device_t adlib_device;
extern const device_t adlib_mca_device;
extern const device_t adgold_device;
extern const device_t es1371_device;
extern const device_t sbpci128_device;
extern const device_t cms_device;
extern const device_t gus_device;
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
extern const device_t gusmax_device;
#endif
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
#if defined(USE_RESID)
extern const device_t ssi2001_device;
#endif
extern const device_t wss_device;
extern const device_t ncr_business_audio_device;


static const struct {
    const char		*internal_name;
    const device_t	*device;
} devices[] = {
    { "none",		NULL				},
    { "internal",	NULL				},

    { "adlib",		&adlib_device			},
    { "adlibgold",	&adgold_device			},
    { "cms",		&cms_device			},
    { "gus",		&gus_device			},
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
    { "gusmax",		&gusmax_device			},
#endif
#if defined(USE_RESID)
    { "ssi2001",	&ssi2001_device			},
#endif
    { "sb",		&sb_1_device			},
    { "sb1.5",		&sb_15_device			},
    { "sb2.0",		&sb_2_device			},
    { "sbprov1",	&sb_pro_v1_device		},
    { "sbprov2",	&sb_pro_v2_device		},
    { "sb16",		&sb_16_device			},
    { "sbawe32",	&sb_awe32_device		},
#if defined(DEV_BRANCH) && defined(USE_PAS16)
    { "pas16",		&pas16_device			},
#endif
    { "wss",		&wss_device			},

    { "adlib_mca",	&adlib_mca_device		},
    { "ncraudio",	&ncr_business_audio_device	},
    { "sbmcv",		&sb_mcv_device			},
    { "sbpromcv",	&sb_pro_mcv_device		},

    { "es1371",		&es1371_device			},
    { "sbpci128",	&sbpci128_device		},
    { NULL,		NULL				}
};


#ifdef _LOGGING
void
sound_card_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SOUND_DEV_LOG
    va_list ap;

    if (sound_card_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
sound_card_reset(void)
{
    if (devices[config.sound_card].device != NULL)
	device_add(devices[config.sound_card].device);
}


int
sound_card_available(int card)
{
    if (devices[card].device != NULL)
	return(device_available(devices[card].device));

    return(1);
}


const char *
sound_card_getname(int card)
{
    if (devices[card].device != NULL)
	return(devices[card].device->name);

    return(NULL);
}


const device_t *
sound_card_getdevice(int card)
{
    return(devices[card].device);
}


int
sound_card_has_config(int card)
{
    if (devices[card].device != NULL)
	return(devices[card].device->config ? 1 : 0);

    return(0);
}


const char *
sound_card_get_internal_name(int card)
{
    return(devices[card].internal_name);
}


int
sound_card_get_from_internal_name(const char *s)
{
    int c;

    for (c = 0; devices[c].internal_name != NULL; c++)
	if (! strcmp(devices[c].internal_name, s))
		return(c);

    /* Not found. */
    return(0);
}
