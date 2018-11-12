/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the network module.
 *
 * Version:	@(#)network.c	1.0.17	2018/11/06
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#ifdef _DEBUG
# include <ctype.h>
#endif
#define HAVE_STDARG_H
#define dbglog network_log
#include "../../emu.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "network.h"


typedef struct {
    mutex_t	*mutex;

    void	*priv;				/* card priv data */
    int		(*poll)(void *);		/* card poll function */
    NETRXCB	rx;				/* card RX function */
    uint8_t	*mac;				/* card MAC address */

    volatile int poll_busy,			/* polling thread data */
		queue_in_use;
    event_t	*poll_wake,
		*poll_complete,
		*queue_not_in_use;
} netdata_t;


/* Global variables. */
#ifdef ENABLE_NETWORK_LOG
int		network_do_log = ENABLE_NETWORK_LOG;
#endif
int		network_host_ndev;
netdev_t	network_host_devs[32];


static netdata_t	netdata;		/* operational data per card */
static const struct {
    const char		*internal_name;
    const network_t	*net;
} networks[] = {
    { "none",	NULL		},
    { "slirp",	&network_slirp	},
    { "pcap",	&network_pcap	},
#ifdef USE_VNS
    { "vns",	&network_vns	},
#endif
    { NULL,	NULL		}
};


/* UI */
int
network_get_from_internal_name(const char *s)
{
    int c = 0;
	
    while (networks[c].internal_name != NULL) {
	if (! strcmp(networks[c].internal_name, s))
			return(c);
	c++;
    }

    /* Not found. */
    return(0);
}


/* UI */
const char *
network_get_internal_name(int net)
{
    return(networks[net].internal_name);
}


/* UI */
const char *
network_getname(int net)
{
    if (networks[net].net != NULL)
	return(networks[net].net->name);

    return(NULL);
}


/* UI */
int
network_available(int net)
{
#if 0
    if (networks[net].net != NULL)
	return(networks[net].net->available());
#endif

    return(1);
}


#ifdef _DEBUG
/* Dump a buffer in hex to output buffer. */
void
hexdump_p(char *ptr, uint8_t *bufp, int len)
{
# define is_print(c)	(isalnum((int)(c)) || ((c) == ' '))
    char asci[20];
    uint8_t c;
    int addr;

    addr = 0;
    while (len-- > 0) {
	c = bufp[addr];
	if ((addr % 16) == 0) {
		sprintf(ptr, "%06X  %02X", addr, c);
	} else {
		sprintf(ptr, " %02X", c);
	}
	ptr += strlen(ptr);
	asci[(addr & 15)] = (char)((is_print(c) ? c : '.') & 0xff);
	if ((++addr % 16) == 0) {
		asci[16] = '\0';
		sprintf(ptr, "  | %s |\n", asci);
		ptr += strlen(ptr);
	}
    }

    if (addr % 16) {
	while (addr % 16) {
		sprintf(ptr, "   ");
		ptr += strlen(ptr);
		asci[(addr & 15)] = ' ';
		addr++;
	}
	asci[16] = '\0';
	sprintf(ptr, "  | %s |\n", asci);
	ptr += strlen(ptr);
    }
}
#endif


#ifdef _LOGGING
void
network_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_NETWORK_LOG
    va_list ap;

    if (network_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
network_wait(int8_t do_wait)
{
    if (do_wait)
	thread_wait_mutex(netdata.mutex);
      else
	thread_release_mutex(netdata.mutex);
}


void
network_poll(void)
{
    while (netdata.poll_busy)
	thread_wait_event(netdata.poll_wake, -1);

    thread_reset_event(netdata.poll_wake);
}


void
network_busy(int8_t set)
{
    netdata.poll_busy = !!set;

    if (! set)
	thread_set_event(netdata.poll_wake);
}


void
network_end(void)
{
    thread_set_event(netdata.poll_complete);
}


/*
 * Initialize the configured network cards.
 *
 * This function gets called only once, from the System
 * Platform initialization code (currently in pc.c) to
 * set our local stuff to a known state.
 */
void
network_init(void)
{
    int i, k;

    /* Clear the local data. */
    memset(&netdata, 0x00, sizeof(netdata_t));

    /* Initialize to a known state. */
    network_type = 0;
    network_card = 0;

    /* Create a first device entry that's always there, as needed by UI. */
    strcpy(network_host_devs[0].device, "none");
    strcpy(network_host_devs[0].description, "None");
    network_host_ndev = 1;

    /* Initialize the network provider modules, if present. */
    i = 0;
    while (networks[i].internal_name != NULL) {
	if (networks[i].net != NULL) {
		k = networks[i].net->init(&network_host_devs[network_host_ndev]);
		if (k > 0)
			network_host_ndev += k;
	}
	i++;
    }
}


/*
 * Attach a network card to the system.
 *
 * This function is called by a hardware driver ("card") after it has
 * finished initializing itself, to link itself to the platform support
 * modules.
 */
void
network_attach(void *dev, uint8_t *mac, NETRXCB rx)
{
    wchar_t temp[256];

    if (network_card == 0) return;

    /* Save the card's info. */
    netdata.priv = dev;
    netdata.rx = rx;
    netdata.mac = mac;

    /* Reset the network provider module. */
    if (networks[network_type].net->reset(mac) < 0) {
	/* Tell user we can't do this (at the moment.) */
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_PCAP), networks[network_type].net->name);
	ui_msgbox(MBX_ERROR, temp);

	// FIXME: we should ask in the dialog if they want to
	//	  reconfigure or quit, and throw them into the
	//	  Settings dialog if yes.

	/* Disable network. */
	network_type = 0;

	return;
    }

    /* Create the network events. */
    netdata.poll_wake = thread_create_event();
    netdata.poll_complete = thread_create_event();
}


/* Stop any network activity. */
void
network_close(void)
{
    int i = 0;

    /* If already closed, do nothing. */
    if (netdata.mutex == NULL) return;

    /* Force-close the network provider modules. */
    while (networks[i].internal_name != NULL) {
	if (networks[i].net)
		networks[i].net->close();
	i++;
    }

    /* Close the network events. */
    if (netdata.poll_wake != NULL) {
	thread_destroy_event(netdata.poll_wake);
	netdata.poll_wake = NULL;
    }
    if (netdata.poll_complete != NULL) {
	thread_destroy_event(netdata.poll_complete);
	netdata.poll_complete = NULL;
    }

    /* Close the network thread mutex. */
    thread_close_mutex(netdata.mutex);
    netdata.mutex = NULL;

    INFO("NETWORK: closed.\n");
}


/*
 * Reset the network card(s).
 *
 * This function is called each time the system is reset,
 * either a hard reset (including power-up) or a soft reset
 * including C-A-D reset.)  It is responsible for connecting
 * everything together.
 */
void
network_reset(void)
{
    const device_t *dev;

#ifdef ENABLE_NETWORK_LOG
    INFO("NETWORK: reset (type=%d, card=%d) debug=%d\n",
		network_type, network_card, network_do_log);
#else
    INFO("NETWORK: reset (type=%d, card=%d)\n",
			network_type, network_card);
#endif
    ui_sb_icon_update(SB_NETWORK, 0);

    /* Just in case.. */
    network_close();

    /* If no active card, we're done. */
    if ((network_type == 0) || (network_card == 0)) return;

    netdata.mutex = thread_create_mutex(L"VARCem.NetMutex");

    INFO("NETWORK: set up for %s, card='%s'\n",
	network_getname(network_type), network_card_getname(network_card));

    /* Add the selected card to the I/O system. */
    dev = network_card_getdevice(network_card);
    if (dev != NULL)
	device_add(dev);
}


/* Transmit a packet to one of the network providers. */
void
network_tx(uint8_t *bufp, int len)
{
    ui_sb_icon_update(SB_NETWORK, 1);

#ifdef ENABLE_NETWORK_DUMP
{
    char temp[8192];
    hexdump_p(temp, bufp, len);
    DBGLOG(2, "NETWORK: >> len=%d\n%s\n", len, temp);
}
#endif

    networks[network_type].net->send(bufp, len);

    ui_sb_icon_update(SB_NETWORK, 0);
}


/* Process a packet received from one of the network providers. */
void
network_rx(uint8_t *bufp, int len)
{
    ui_sb_icon_update(SB_NETWORK, 1);

#ifdef ENABLE_NETWORK_DUMP
{
    char temp[8192];
    hexdump_p(temp, bufp, len);
    DBGLOG(2, "NETWORK: << len=%d\n%s\n", len, temp);
}
#endif

    if (netdata.rx && netdata.priv)
	netdata.rx(netdata.priv, bufp, len);

    ui_sb_icon_update(SB_NETWORK, 0);
}


/* Get name of host-based network interface. */
int
network_card_to_id(const char *devname)
{
    int i = 0;

    for (i = 0; i < network_host_ndev; i++) {
	if (! strcmp(network_host_devs[i].device, devname)) {
		return(i);
	}
    }

    /* Not found. */
    return(0);
}
