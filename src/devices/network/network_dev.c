/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the various network cards.
 *
 * NOTE		The definition of the netcard_t is currently not optimal;
 *		it should be malloc'ed and then linked to the NETCARD def.
 *		Will be done later.
 *
 * Version:	@(#)network_dev.c	1.0.2	2019/05/02
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog network_dev_log
#include "../../emu.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "network.h"
#include "net_ne2000.h"
#include "net_wd80x3.h"
#include "net_3com.h"


static const struct {
    const char          *internal_name;
    const device_t      *device;
} net_cards[] = {
    { "none",		NULL			},
    { "internal",	NULL			},

    /* ISA cards. */
    { "ne1k",		&ne1000_device		},
    { "ne2k",		&ne2000_device		},
#if 0
    { "3c501",		&el1_device		},
#endif
    { "3c503",		&el2_device		},
    { "ne2kpnp",	&rtl8019as_device	},
    { "wd8003e",	&wd8003e_device		},
    { "wd8013ebt",	&wd8013ebt_device	},

    /* MCA cards. */
    { "ne2",		&ne2_mca_device		},
    { "ne2_enext",	&ne2_enext_mca_device	},
    { "wd8013epa",	&wd8013epa_device	},

    /* PCI cards. */
    { "ne2kpci",	&rtl8029as_device	},

    { NULL,		NULL			}
};


/* Global variables. */
#ifdef ENABLE_NETWORK_DEV_LOG
int	network_card_do_log = ENABLE_NETWORK_DEV_LOG;
#endif


#ifdef _LOGGING
void
network_card_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_NETWORK_DEV_LOG
    va_list ap;

    if (network_card_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


/* UI */
int
network_card_get_from_internal_name(const char *s)
{
    int c;
	
    for (c = 0; net_cards[c].internal_name != NULL; c++)
	if (! strcmp(net_cards[c].internal_name, s))
		return(c);

    /* Not found. */
    return(0);
}


/* UI */
const char *
network_card_get_internal_name(int card)
{
    return(net_cards[card].internal_name);
}


/* UI */
const char *
network_card_getname(int card)
{
    if (net_cards[card].device != NULL)
	return(net_cards[card].device->name);

    return(NULL);
}


/* UI */
int
network_card_available(int card)
{
    if (net_cards[card].device != NULL)
	return(device_available(net_cards[card].device));

    return(1);
}


/* UI */
const device_t *
network_card_getdevice(int card)
{
    return(net_cards[card].device);
}


/* UI */
int
network_card_has_config(int card)
{
    if (net_cards[card].device != NULL)
	return(net_cards[card].device->config ? 1 : 0);

    return(0);
}
