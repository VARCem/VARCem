/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle WinPcap library processing.
 *
 * Version:	@(#)net_pcap.c	1.0.11	2019/05/03
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
#include <wchar.h>
#if defined(_WIN32) && !defined(WIN32)
# define WIN32
#endif
#include <pcap/pcap.h>
#define dbglog network_log
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../plat.h"
#include "../../ui/ui.h"
#include "network.h"


#ifdef _WIN32
# define PCAP_DLL_PATH	"wpcap.dll"
#else
# define PCAP_DLL_PATH	"libpcap.so"
#endif


static volatile void		*pcap_handle;	/* handle to WinPcap DLL */
static volatile pcap_t		*pcap;		/* handle to WinPcap library */
static volatile thread_t	*poll_tid;
static event_t			*poll_state;


/* Pointers to the real functions. */
static char	*const (*PCAP_lib_version)(void);
static int	(*PCAP_findalldevs)(pcap_if_t **, char *);
static void	(*PCAP_freealldevs)(pcap_if_t *);
static pcap_t	*(*PCAP_open_live)(const char *, int, int, int, char *);
static int	(*PCAP_compile)(pcap_t *,struct bpf_program *,
				  const char *, int, bpf_u_int32);
static int	(*PCAP_setfilter)(pcap_t *, struct bpf_program *);
static uint8_t	*const (*PCAP_next)(pcap_t *, struct pcap_pkthdr *);
static int	(*PCAP_sendpacket)(pcap_t *, const uint8_t *, int);
static void	(*PCAP_close)(pcap_t *);

static const dllimp_t pcap_imports[] = {
  { "pcap_lib_version",	&PCAP_lib_version	},
  { "pcap_findalldevs",	&PCAP_findalldevs	},
  { "pcap_freealldevs",	&PCAP_freealldevs	},
  { "pcap_open_live",	&PCAP_open_live		},
  { "pcap_compile",	&PCAP_compile		},
  { "pcap_setfilter",	&PCAP_setfilter		},
  { "pcap_next",	&PCAP_next		},
  { "pcap_sendpacket",	&PCAP_sendpacket	},
  { "pcap_close",	&PCAP_close		},
  { NULL,		NULL			},
};


/* Handle the receiving of frames from the channel. */
static void
poll_thread(void *arg)
{
    uint8_t *mac = (uint8_t *)arg;
    uint8_t *data = NULL;
    struct pcap_pkthdr h;
    uint32_t mac_cmp32[2];
    uint16_t mac_cmp16[2];
    event_t *evt;

    INFO("PCAP: thread started.\n");
    thread_set_event(poll_state);

    /* Create a waitable event. */
    evt = thread_create_event();

    /* As long as the channel is open.. */
    while (pcap != NULL) {
	/* Request ownership of the device. */
	network_wait(1);

	/* Wait for a poll request. */
	network_poll();

	if (pcap == NULL) break;

	/* Wait for the next packet to arrive. */
	data = (uint8_t *)PCAP_next((pcap_t *)pcap, &h);
	if (data != NULL) {
		/* Received MAC. */
		mac_cmp32[0] = *(uint32_t *)(data+6);
		mac_cmp16[0] = *(uint16_t *)(data+10);

		/* Local MAC. */
		mac_cmp32[1] = *(uint32_t *)mac;
		mac_cmp16[1] = *(uint16_t *)(mac+4);
		if ((mac_cmp32[0] != mac_cmp32[1]) ||
		    (mac_cmp16[0] != mac_cmp16[1])) {

			network_rx(data, h.caplen);
		} else {
			/* Mark as invalid packet. */
			data = NULL;
		}
	}

	/* If we did not get anything, wait a while. */
	if (data == NULL)
		thread_wait_event(evt, 10);

	/* Release ownership of the device. */
	network_wait(0);
    }

    /* No longer needed. */
    if (evt != NULL)
	thread_destroy_event(evt);
    thread_set_event(poll_state);

    INFO("PCAP: thread stopped.\n");
}


/*
 * Prepare the (Win)Pcap module for use.
 *
 * This is called only once, during application init,
 * to check for availability of PCAP, and to retrieve
 * a list of (usable) intefaces for it.
 */
static int
do_init(netdev_t *list)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *devlist, *dev;
    char *str = PCAP_DLL_PATH;
    int i = 0;

    /* Local variables. */
    pcap = NULL;

    /* Try loading the DLL. */
    pcap_handle = dynld_module(str, pcap_imports);
    if (pcap_handle == NULL) {
	/* Forward module name back to caller. */
	strcpy(list->description, str);

        ERRLOG("PCAP: unable to load '%s', PCAP not available!\n", str);
        return(-1);
    } else {
        INFO("PCAP: module '%s' loaded.\n", str);
    }

    /* Get the PCAP library name and version. */
    strcpy(errbuf, PCAP_lib_version());
    str = strchr(errbuf, '(');
    if (str != NULL)
	*(str-1) = '\0';
    INFO("PCAP: initializing, %s\n", errbuf);

    /* Retrieve the device list from the local machine */
    if (PCAP_findalldevs(&devlist, errbuf) == -1) {
	ERRLOG("PCAP: error in pcap_findalldevs: %s\n", errbuf);
	return(-1);
    }

    /* Process the list and build our table. */
    for (dev = devlist; dev != NULL; dev=dev->next) {
	strcpy(list->device, dev->name);
	if (dev->description)
		strcpy(list->description, dev->description);
	  else
		memset(list->description, '\0', sizeof(list->description));
	list++; i++;
    }

    /* Release the memory. */
    PCAP_freealldevs(devlist);

    return(i);
}


/* Close up shop. */
static void
do_close(void)
{
    pcap_t *pc;

    if (pcap == NULL) return;

    INFO("PCAP: closing.\n");

    /* Tell the polling thread to shut down. */
    pc = (pcap_t *)pcap; pcap = NULL;

    /* Tell the thread to terminate. */
    if (poll_tid != NULL) {
	network_busy(0);

	/* Wait for the thread to finish. */
	INFO("PCAP: waiting for thread to end...\n");
	thread_wait_event(poll_state, -1);
	INFO("PCAP: thread ended\n");
	thread_destroy_event(poll_state);

	poll_tid = NULL;
	poll_state = NULL;
    }

    /* OK, now shut down Pcap itself. */
    PCAP_close(pc);
    pcap = NULL;

    INFO("PCAP: closed.\n");
}


/*
 * Reset (Win)Pcap and activate it.
 *
 * This is called on every 'cycle' of the emulator,
 * if and as long the NetworkType is set to PCAP,
 * and also as long as we have a NetCard defined.
 *
 * We already know we have PCAP available, as this
 * is called when the network activates itself and
 * tries to attach to the network module.
 */
static int
do_reset(uint8_t *mac)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    char filter_exp[255];
    struct bpf_program fp;

    /* Make sure local variables are cleared. */
    poll_tid = NULL;
    poll_state = NULL;

    /* Get the value of our capture interface. */
    if ((config.network_host[0] == '\0') ||
	!strcmp(config.network_host, "none")) {
	ERRLOG("PCAP: no interface configured!\n");
	return(-1);
    }

    /* Open a PCAP live channel. */
    if ((pcap = PCAP_open_live(config.network_host,	/* interface name */
			       1518,			/* max packet size */
			       1,			/* promiscuous mode? */
			       10,			/* timeout in msec */
			       errbuf)) == NULL) {	/* error buffer */
	ERRLOG(" Unable to open device: %s!\n", config.network_host);
	return(-1);
    }
    DEBUG("PCAP: interface: %s\n", config.network_host);

    /* Create a MAC address based packet filter. */
    DEBUG("PCAP: installing filter for MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(filter_exp,
	"( ((ether dst ff:ff:ff:ff:ff:ff) or (ether dst %02x:%02x:%02x:%02x:%02x:%02x)) and not (ether src %02x:%02x:%02x:%02x:%02x:%02x) )",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (PCAP_compile((pcap_t *)pcap, &fp, filter_exp, 0, 0xffffffff) != -1) {
	if (PCAP_setfilter((pcap_t *)pcap, &fp) != 0) {
		ERRLOG("PCAP: error installing filter (%s) !\n", filter_exp);
		PCAP_close((pcap_t *)pcap);
		return(-1);
	}
    } else {
	ERRLOG("PCAP: could not compile filter (%s) !\n", filter_exp);
	PCAP_close((pcap_t *)pcap);
	return(-1);
    }

    poll_state = thread_create_event();
    poll_tid = thread_create(poll_thread, mac);
    thread_wait_event(poll_state, -1);

    return(0);
}


/* Are we available or not? */
static int
do_available(void)
{
    return((pcap_handle != NULL) ? 1 : 0);
}


/* Send a packet to the Pcap interface. */
static void
do_send(uint8_t *bufp, int len)
{
    if (pcap == NULL) return;

    network_busy(1);

    PCAP_sendpacket((pcap_t *)pcap, bufp, len);

    network_busy(0);
}


const network_t network_pcap = {
#ifdef _WIN32
    "WinPCap",
#else
    "PCap",
#endif
    do_init, do_close, do_reset,
    do_available,
    do_send
};
