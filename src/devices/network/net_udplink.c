/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement an Ethernet-over-UDP link tunnel.
 *
 * Version:	@(#)net_udplink.c	1.0.1	2021/03/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Bryan Biedenkapp, <gatekeep@gmail.com>
 *
 *		Copyright 2021 Fred N. van Kempen.
 *		Copyright 2018 Bryan Biedenkapp.
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
#include <udplink.h>
#include "../../emu.h"
#include "../../config.h"
#include "../../device.h"
#include "../../plat.h"
#include "../../ui/ui.h"
#include "network.h"


#define RX_BUF_SIZE	65535

#ifdef _WIN32
# define UL_DLL_PATH	"udplink.dll"
#else
# define UL_DLL_PATH	"udplink.so"
#endif

#if USE_UDPLINK != 2
# define FUNC(x)        ulink_ ## x
#else
# define FUNC(x)        ULINK_ ## x


/* Pointers to the real functions. */
static int	(*ULINK_version)(char *, int);
static void	(*ULINK_error)(char *, int);
static int	(*ULINK_open)(int);
static void	(*ULINK_close)(void);
static int	(*ULINK_connect)(const char *, int, const uint8_t *);
static void	(*ULINK_disconnect)(void);
static int	(*ULINK_recv)(uint8_t *, int);
static int	(*ULINK_send)(const uint8_t *, int);


static volatile void	*ulink_handle = NULL;	// handle to DLL

static const dllimp_t imports[] = {
  { "ulink_version",		&ULINK_version		},
  { "ulink_error",		&ULINK_error		},
  { "ulink_open",		&ULINK_open		},
  { "ulink_close",		&ULINK_close		},
  { "ulink_connect",		&ULINK_connect		},
  { "ulink_disconnect",		&ULINK_disconnect	},
  { "ulink_recv",		&ULINK_recv		},
  { "ulink_send",		&ULINK_send		},
  { NULL						}
};
#endif


static volatile thread_t
			*poll_tid;
static event_t		*poll_state;
static int		is_running;


/* Handle the receiving of frames from the channel. */
static void
poll_thread(UNUSED(void *arg))
{
    uint8_t *pkt_buf;
    int pkt_len;
    event_t *evt;

    INFO("UDPlink: polling started.\n");
    thread_set_event(poll_state);

    /* Create a waitable event. */
    evt = thread_create_event();

    /* Create a packet buffer. */
    pkt_buf = (uint8_t *)mem_alloc(RX_BUF_SIZE);

    /* As long as the channel is open.. */
    is_running = 1;
    while (is_running) {
	/* Request ownership of the device. */
	network_wait(1);

	/* Wait for a poll request. */
	network_poll();

	/* Wait for the next packet to arrive. */
	memset(pkt_buf, 0x00, RX_BUF_SIZE);

	pkt_len = FUNC(recv)(pkt_buf, RX_BUF_SIZE);
	if (pkt_len > 0) {
		network_rx(pkt_buf, pkt_len);
	} else {
		/* If we did not get anything, wait a while. */
		if (pkt_len == 0)
			thread_wait_event(evt, 10);
	}

	/* Release ownership of the device. */
	network_wait(0);
    }

    free(pkt_buf);

    /* No longer needed. */
    if (evt != NULL)
	thread_destroy_event(evt);

    INFO("UDPlink: polling stopped.\n");
    thread_set_event(poll_state);
}


/* Close up shop. */
static void
do_close(void)
{
    INFO("UDPlink: closing.\n");

    /* Tell the thread to terminate. */
    if (poll_tid != NULL) {
	network_busy(0);

	is_running = 0;

	/* Wait for the thread to finish. */
        INFO("UDPlink: waiting for thread to end...\n");
	thread_wait_event(poll_state, -1);

        INFO("UDPlink: thread ended\n");
	thread_destroy_event(poll_state);

	poll_tid = NULL;
	poll_state = NULL;
    }

    /* Disconnect from the peer. */
    FUNC(disconnect)();

    /* OK, now shut down UDPlink itself. */
    FUNC(close)();

#if 0	/* do not unload */
# if USE_UDPLINK == 2
    /* Unload the DLL if possible. */
    if (ulink_handle != NULL)
	dynld_close(ulink_handle);
# endif
    ulink_handle = NULL;
#endif
}


/*
 * Reset UDP and activate it.
 *
 * This is called on every 'cycle' of the emulator,
 * if and as long the NetworkType is set to UDP,
 * and also as long as we have a NetCard defined.
 */
static int
do_reset(uint8_t *mac)
{
    char temp[256];
    int port;

    /* Get the value of our server address. */
    if ((config.network_srv_addr[0] == '\0') ||
	!strcmp(config.network_srv_addr, "none")) {
        ERRLOG("UDPlink: no server address configured!\n");
        return(-1);
    }

    /* Get the value of our server port. */
    switch (config.network_srv_port) {
	case 0:		/* not set */
		ERRLOG("UDPlink: no server port configured!\n");
		return(-1);

	case -1:	/* default */
		port = UDPLINK_PORT;
		break;

	default:
		port = config.network_srv_port;
    }

    /* Tell the thread to terminate. */
    if (poll_tid != NULL)
	network_busy(0);

    /* Wait for the thread to finish. */
    INFO("UDPlink: waiting for thread to end...\n");
    thread_wait_event(poll_state, -1);

    INFO("UDPlink: thread ended\n");
    thread_destroy_event(poll_state);

    /* Make sure local variables are cleared. */
    poll_tid = NULL;
    poll_state = NULL;

    /* OK, now shut down UDP itself. */
    FUNC(close)();
    if (FUNC(open)(UDPLINK_PORT) <= 0) {
	FUNC(error)(temp, sizeof(temp));
	ERRLOG("UDPlink: %s\n", temp);
	return(0);
    }

    /* Tell the protocol to (virtually) connect to our peer. */
    if (FUNC(connect)(config.network_srv_addr, port, mac) <= 0) {
	FUNC(error)(temp, sizeof(temp));
	ERRLOG("UDPlink: %s\n", temp);
	return(0);
    }

    INFO("UDP: starting thread..\n");

    poll_state = thread_create_event();
    poll_tid = thread_create(poll_thread, mac);
    thread_wait_event(poll_state, -1);

    return(0);
}


/*
 * Initialize UDP-socket for use.
 *
 * This is called only once, during application init,
 * so the UI can be properly initialized.
 */
static int
do_init(netdev_t *list)
{
#if USE_UDPLINK == 2
    char temp[128];
    const char *fn = UL_DLL_PATH;

    /* Try loading the DLL. */
    ulink_handle = dynld_module(fn, imports);
    if (ulink_handle == NULL) {
	/* Forward module name back to caller. */
	strcpy(list->description, fn);

        ERRLOG("NETWORK: unable to load '%s', UDPlink not available!\n", fn);
	return(-1);
    } else {
	if (FUNC(version)(temp, sizeof(temp)) <= 0) {
		ERRLOG("SLiRP: could not get version!\n");
		return(-1);
	}
        INFO("NETWORK: module '%s' loaded, version %s.\n", fn, temp);
    }
#else
    ulink_handle = (void *)1;	/* just to indicate always therse */
#endif

    return(1);
}


/* Are we available or not? */
static int
do_available(void)
{
    return((ulink_handle != NULL) ? 1 : 0);
}


/* Send a packet to the UDP interface. */
static void
do_send(const uint8_t *bufp, int len)
{
    char temp[128];

    network_busy(1);

    if (FUNC(send)(bufp, len) <= 0) {
	FUNC(error)(temp, sizeof(temp));
        ERRLOG("UDPlink: %s\n", temp);
	network_busy(0);
	return;
    }

    network_busy(0);
}


const network_t network_udplink = {
    "UDPlink Tunnel",
    do_init, do_close, do_reset,
    do_available,
    do_send
};
