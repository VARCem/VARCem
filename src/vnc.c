/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the VNC remote renderer with LibVNCServer.
 *
 * Version:	@(#)vnc.c	1.0.1	2018/02/14
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Based on raw code by RichardG, <richardg867@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <rfb/rfb.h>
#include "emu.h"
#include "device.h"
#include "video/video.h"
#include "keyboard.h"
#include "mouse.h"
#include "plat.h"
#include "ui.h"
#include "vnc.h"


#define VNC_MIN_X	320
#define VNC_MAX_X	2048
#define VNC_MIN_Y	200
#define VNC_MAX_Y	2048


static rfbScreenInfoPtr	rfb = NULL;
static int	clients;
static int	updatingSize;
static int	allowedX,
		allowedY;
static int	ptr_x, ptr_y, ptr_but;


static void
vnc_kbdevent(rfbBool down, rfbKeySym k, rfbClientPtr cl)
{
    (void)cl;

    /* Handle it through the lookup tables. */
    vnc_kbinput(down?1:0, (int)k);
}


static void
vnc_ptrevent(int but, int x, int y, rfbClientPtr cl)
{
   if (x>=0 && x<allowedX && y>=0 && y<allowedY) {
	/* VNC uses absolute positions within the window, no deltas. */
	if (x != ptr_x || y != ptr_y) {
		mouse_x += (x - ptr_x);
		mouse_y += (y - ptr_y);
		ptr_x = x; ptr_y = y;
	}

	if (but != ptr_but) {
		mouse_buttons = 0;
		if (but & 0x01)
			mouse_buttons |= 0x01;
		if (but & 0x02)
			mouse_buttons |= 0x04;
		if (but & 0x04)
			mouse_buttons |= 0x02;
		ptr_but = but;
	}
   }

   rfbDefaultPtrAddEvent(but, x, y, cl);
}


static void
vnc_clientgone(rfbClientPtr cl)
{
    pclog("VNC: client disconnected: %s\n", cl->host);

    if (clients > 0)
	clients--;
    if (clients == 0) {
	/* No more clients, pause the emulator. */
	pclog("VNC: no clients, pausing..\n");

	/* Disable the mouse. */
	plat_mouse_capture(0);

	plat_pause(1);
    }
}


static enum rfbNewClientAction
vnc_newclient(rfbClientPtr cl)
{
    /* Hook the ClientGone function so we know when they're gone. */
    cl->clientGoneHook = vnc_clientgone;

    pclog("VNC: new client: %s\n", cl->host);
    if (++clients == 1) {
	/* Reset the mouse. */
	ptr_x = allowedX/2;
	ptr_y = allowedY/2;
	mouse_x = mouse_y = mouse_z = 0;
	mouse_buttons = 0x00;

	/* We now have clients, un-pause the emulator if needed. */
	pclog("VNC: unpausing..\n");

	/* Enable the mouse. */
	plat_mouse_capture(1);

	plat_pause(0);
    }

    /* For now, we always accept clients. */
    return(RFB_CLIENT_ACCEPT);
}


static void
vnc_display(rfbClientPtr cl)
{
    /* Avoid race condition between resize and update. */
    if (!updatingSize && cl->newFBSizePending) {
	updatingSize = 1;
    } else if (updatingSize && !cl->newFBSizePending) {
	updatingSize = 0;

	allowedX = rfb->width;
	allowedY = rfb->height;
    }
}


static void
vnc_blit(int x, int y, int y1, int y2, int w, int h)
{
    uint32_t *p;
    int yy;

    for (yy=y1; yy<y2; yy++) {
	p = (uint32_t *)&(((uint32_t *)rfb->frameBuffer)[yy*VNC_MAX_X]);

	if ((y+yy) >= 0 && (y+yy) < VNC_MAX_Y)
		memcpy(p, &(((uint32_t *)buffer32->line[y+yy])[x]), w*4);
    }
 
    video_blit_complete();

    if (! updatingSize)
	rfbMarkRectAsModified(rfb, 0,0, allowedX,allowedY);
}


/* Initialize VNC for operation. */
int
vnc_init(UNUSED(void *arg))
{
    static char title[128];
    rfbPixelFormat rpf = {
	/*
	 * Screen format:
	 *	32bpp; 32 depth;
	 *	little endian;
	 *	true color;
	 *	max 255 R/G/B;
	 *	red shift 16; green shift 8; blue shift 0;
	 *	padding
	 */
	32, 32, 0, 1, 255,255,255, 16, 8, 0, 0, 0
    };

    cgapal_rebuild();

    if (rfb == NULL) {
	wcstombs(title, ui_window_title(NULL), sizeof(title));
	updatingSize = 0;
	allowedX = scrnsz_x;
	allowedY = scrnsz_y;
 
	rfb = rfbGetScreen(0, NULL, VNC_MAX_X, VNC_MAX_Y, 8, 3, 4);
	rfb->desktopName = title;
	rfb->frameBuffer = (char *)malloc(VNC_MAX_X*VNC_MAX_Y*4);

	rfb->serverFormat = rpf;
	rfb->alwaysShared = TRUE;
	rfb->displayHook = vnc_display;
	rfb->ptrAddEvent = vnc_ptrevent;
	rfb->kbdAddEvent = vnc_kbdevent;
	rfb->newClientHook = vnc_newclient;
 
	/* Set up our current resolution. */
	rfb->width = allowedX;
	rfb->height = allowedY;
 
	rfbInitServer(rfb);

	rfbRunEventLoop(rfb, -1, TRUE);
    }
 
    /* Set up our BLIT handlers. */
    video_setblit(vnc_blit);

    clients = 0;

    pclog("VNC: init complete.\n");

    return(1);
}


void
vnc_close(void)
{
    video_setblit(NULL);

    if (rfb != NULL) {
	free(rfb->frameBuffer);

	rfbScreenCleanup(rfb);

	rfb = NULL;
    }
}


void
vnc_resize(int x, int y)
{
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl;

    if (rfb == NULL) return;

    /* TightVNC doesn't like certain sizes.. */
    if (x < VNC_MIN_X || x > VNC_MAX_X || y < VNC_MIN_Y || y > VNC_MAX_Y) {
	pclog("VNC: invalid resoltion %dx%d requested!\n", x, y);
	return;
    }

    if ((x != rfb->width || y != rfb->height) && x > 160 && y > 0) {
	pclog("VNC: updating resolution: %dx%d\n", x, y);
 
	allowedX = (rfb->width < x) ? rfb->width : x;
	allowedY = (rfb->width < y) ? rfb->width : y;
 
	rfb->width = x;
	rfb->height = y;
 
	iterator = rfbGetClientIterator(rfb);
	while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
		LOCK(cl->updateMutex);
		cl->newFBSizePending = 1;
		UNLOCK(cl->updateMutex);
	}
    }
}
 

/* Tell them to pause if we have no clients. */
int
vnc_pause(void)
{
    return((clients > 0) ? 0 : 1);
}


void
vnc_take_screenshot(wchar_t *fn)
{
    pclog("VNC: take_screenshot\n");
}
