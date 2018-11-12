/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle SLiRP library processing.
 *
 * Version:	@(#)net_slirp.c	1.0.6	2018/11/12
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
#include <wchar.h>
#include <stdarg.h>
#ifdef USE_LIBSLIRP
# include <libslirp.h>
#else
typedef void slirp_t;				/* nicer than void.. */
#endif
#define HAVE_STDARG_H
#define netdbg network_log
#include "../../emu.h"
#include "../../device.h"
#include "../../plat.h"
#include "../../ui/ui.h"
#include "network.h"


#ifdef _WIN32
# define SLIRP_DLL_PATH	"libslirp.dll"
#else
# define SLIRP_DLL_PATH	"libslirp.so"
#endif


static volatile void		*slirp_handle;	/* handle to SLiRP DLL */
static slirp_t			*slirp;		/* SLiRP library handle */
static volatile thread_t	*poll_tid;
static event_t			*poll_state;


/* Pointers to the real functions. */
static void	(*SLIRP_debug)(void (*func)(slirp_t *, const char *fmt, va_list));
static int	(*SLIRP_version)(char *, int);
static slirp_t	*(*SLIRP_init)(void);
static void	(*SLIRP_close)(slirp_t *);
static void	(*SLIRP_poll)(slirp_t *);
static int	(*SLIRP_recv)(slirp_t *, uint8_t *);
static int	(*SLIRP_send)(slirp_t *, const uint8_t *, int);
static const dllimp_t slirp_imports[] = {
  { "slirp_debug",		&SLIRP_debug	},
  { "slirp_version",		&SLIRP_version	},
  { "slirp_init",		&SLIRP_init	},
  { "slirp_close",		&SLIRP_close	},
  { "slirp_poll",		&SLIRP_poll	},
  { "slirp_recv",		&SLIRP_recv	},
  { "slirp_send",		&SLIRP_send	},
  { NULL,			NULL		},
};


/* Forward module debugging into to our logfile. */
static void
handle_logging(UNUSED(slirp_t *slirp), const char *fmt, va_list ap)
{
    pclog_ex(fmt, ap);
}


/* Handle the receiving of frames. */
static void
poll_thread(void *arg)
{
    uint8_t pktbuff[2048];
    uint32_t mac_cmp32[2];
    uint16_t mac_cmp16[2];
    const uint8_t *mac = (const uint8_t *)arg;
    event_t *evt;
    int len;

    INFO("SLiRP: thread started.\n");
    thread_set_event(poll_state);

    /* Create a waitable event. */
    evt = thread_create_event();

    while (slirp != NULL) {
	/* Request ownership of the device. */
	network_wait(1);

	/* Wait for a poll request. */
	network_poll();

	/* See if there is any work. */
	SLIRP_poll(slirp);

	/* Our queue may have been nuked.. */
	if (slirp == NULL) break;

	/* Wait for the next packet to arrive. */
	len = SLIRP_recv(slirp, pktbuff);
	if (len > 0) {
		/* Received MAC. */
		mac_cmp32[0] = *(uint32_t *)(pktbuff+6);
		mac_cmp16[0] = *(uint16_t *)(pktbuff+10);

		/* Local MAC. */
		mac_cmp32[1] = *(uint32_t *)mac;
		mac_cmp16[1] = *(uint16_t *)(mac+4);
		if ((mac_cmp32[0] != mac_cmp32[1]) ||
		    (mac_cmp16[0] != mac_cmp16[1])) {
			DBGLOG(1, "SLiRP: got a %ibyte packet\n", len);

			network_rx(pktbuff, len); 
		} else {
			/* Mark as invalid packet. */
			len = 0;
		}
	} else {
		/* If we did not get anything, wait a while. */
		thread_wait_event(evt, 10);
	}

	/* Release ownership of the device. */
	network_wait(0);
    }

    /* No longer needed. */
    if (evt != NULL)
	thread_destroy_event(evt);
    thread_set_event(poll_state);

    INFO("SLiRP: thread stopped.\n");
}


#if 0
/* Needed by SLiRP library. */
static void
slirp_output(const uint8_t *pkt, int pkt_len)
{
    struct queuepacket *qp;

    if (slirpq != NULL) {
	qp = (struct queuepacket *)mem_alloc(sizeof(struct queuepacket));
	qp->len = pkt_len;
	memcpy(qp->data, pkt, pkt_len);
	QueueEnter(slirpq, qp);
    }
}


/* Needed by SLiRP library. */
static int
slirp_can_output(void)
{
    return((slirp != NULL)?1:0);
}
#endif


/*
 * Prepare the SLiRP module for use.
 *
 * This is called only once, during application init,
 * to check for availability of SLiRP, so the UI can
 * be properly initialized.
 */
static int
net_slirp_init(netdev_t *list)
{
    char temp[128];
    const char *fn = SLIRP_DLL_PATH;

    /* Try loading the DLL. */
    slirp_handle = dynld_module(fn, slirp_imports);
    if (slirp_handle == NULL) {
	/* Forward module name back to caller. */
	strcpy(list->description, fn);

        ERRLOG("SLIRP: unable to load '%s', SLiRP not available!\n", fn);
	return(-1);
    } else {
        INFO("SLiRP: module '%s' loaded.\n", fn);
    }

    /* Link the module to our logging. */
    SLIRP_debug(handle_logging);

    /* Get the library version. */
    if (SLIRP_version(temp, sizeof(temp)) <= 0) {
	ERRLOG("SLiRP could not get version!\n");
	return(-1);
    }
    INFO("SLiRP: initializing, version %s ..\n", temp);

    return(1);
}


/* Initialize SLiRP for use. */
static int
net_slirp_reset(uint8_t *mac)
{
    /* Make sure local variables are cleared. */
    poll_tid = NULL;
    poll_state = NULL;

    /* Get a handle to a SLIRP instance. */
    slirp = SLIRP_init();
    if (slirp == NULL) {
	ERRLOG("SLiRP could not be initialized!\n");
	return(-1);
    }

    poll_state = thread_create_event();
    poll_tid = thread_create(poll_thread, mac);
    thread_wait_event(poll_state, -1);

    return(0);
}


static void
net_slirp_close(void)
{
    slirp_t *sl;

    if (slirp == NULL) return;

    INFO("SLiRP: closing.\n");

    /* Tell the polling thread to shut down. */
    sl = slirp; slirp = NULL;

    /* Tell the thread to terminate. */
    if (poll_tid != NULL) {
	network_busy(0);

	/* Wait for the thread to finish. */
	INFO("SLiRP: waiting for thread to end...\n");
	thread_wait_event(poll_state, -1);
	INFO("SLiRP: thread ended\n");
	thread_destroy_event(poll_state);

	poll_tid = NULL;
	poll_state = NULL;
    }

    /* OK, now shut down SLiRP itself. */
    SLIRP_close(sl);

    INFO("SLiRP: closed.\n");
}


/* Are we available or not? */
static int
net_slirp_available(void)
{
    return((slirp_handle != NULL) ? 1 : 0);
}


/* Send a packet to the SLiRP interface. */
static void
net_slirp_send(uint8_t *pkt, int pkt_len)
{
    if (slirp != NULL) {
	network_busy(1);

	SLIRP_send(slirp, (const uint8_t *)pkt, pkt_len);

	network_busy(0);
    }
}


const network_t network_slirp = {
    "SLiRP",
    net_slirp_init,
    net_slirp_close,
    net_slirp_reset,
    net_slirp_available,
    net_slirp_send
};
