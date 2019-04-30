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
 * TODO:	Implement screenshots, and Audio Redirection.
 *
 * Version:	@(#)vnc.c	1.0.11	2019/04/29
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Based on raw code by RichardG, <richardg867@gmail.com>
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
//#define LIBVNCSERVER_HAVE_LIBPTHREAD
#include <rfb/rfb.h>
#include "emu.h"
#include "device.h"
#include "devices/video/video.h"
#include "devices/input/keyboard.h"
#include "devices/input/mouse.h"
#include "ui/ui.h"
#include "plat.h"
#if USE_LIBPNG
# include "png.h"
#endif
#include "vnc.h"


#ifdef _WIN32
# define PATH_VNC_DLL	"libvncserver.dll"
#else
# define PATH_VNC_DLL	"libvncserver.so"
#endif

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


#if USE_VNC == 2
# define DLLFUNC(x)		rfb_##x

static void	*vnc_handle = NULL;	/* handle to libVNCserver DLL */


/* Pointers to the real functions. */
static rfbScreenInfoPtr (*rfb_GetScreen)(int *,char **,int,int,int,int,int);
static void		(*rfb_InitServer)(rfbScreenInfoPtr);
static void		(*rfb_RunEventLoop)(rfbScreenInfoPtr, long, rfbBool);
static void		(*rfb_ScreenCleanup)(rfbScreenInfoPtr);
static rfbClientIteratorPtr (*rfb_GetClientIterator)(rfbScreenInfoPtr);
static rfbClientPtr	(*rfb_ClientIteratorNext)(rfbClientIteratorPtr);
static void		(*rfb_MarkRectAsModified)(rfbScreenInfoPtr,
						  int,int,int,int);
static void		(*rfb_DefaultPtrAddEvent)(int,int,int,rfbClientPtr);
static void		(*rfb_ClientLock)(rfbClientPtr);
static void		(*rfb_ClientUnlock)(rfbClientPtr);
static void		(*rfb_LogEnable)(int);
static void		(*rfb_Log)(const char *fmt, ...);
static void		(*rfb_Err)(const char *fmt, ...);


static const dllimp_t vnc_imports[] = {
  { "rfbGetScreen",			&rfb_GetScreen			},
  { "rfbInitServerWithPthreadsAndZRLE",	&rfb_InitServer			},
  { "rfbRunEventLoop",			&rfb_RunEventLoop		},
  { "rfbScreenCleanup",			&rfb_ScreenCleanup		},
  { "rfbGetClientIterator",		&rfb_GetClientIterator		},
  { "rfbClientIteratorNext",		&rfb_ClientIteratorNext		},
  { "rfbMarkRectAsModified",		&rfb_MarkRectAsModified		},
  { "rfbDefaultPtrAddEvent",		&rfb_DefaultPtrAddEvent		},
  { "rfbClientLock",			&rfb_ClientLock			},
  { "rfbClientUnlock",			&rfb_ClientUnlock		},
  { "rfbLogEnable",			&rfb_LogEnable			},
  { "rfbLog",				&rfb_Log			},
  { "rfbErr",				&rfb_Err			},
  { NULL,				NULL				}
};
#else
# define DLLFUNC(x)		rfb##x
#endif


/* Local handlers for VNCserver event logging. */
static void
vnc_dbglog(const char *fmt, ...)
{
    char temp[1024];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(temp, fmt, ap);
    va_end(ap);
    pclog(LOG_INFO, "VNC: %s", temp);
}


static void
vnc_errlog(const char *fmt, ...)
{
    char temp[1024];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(temp, fmt, ap);
    va_end(ap);
    pclog(LOG_ERR, "VNC: %s", temp);
}


static void
vnc_kbdevent(rfbBool down, rfbKeySym k, UNUSED(rfbClientPtr cl))
{
    /* Handle it through the lookup tables. */
    vnc_kbinput(down?1:0, (int)k);
}


static void
vnc_ptrevent(int but, int x, int y, rfbClientPtr cl)
{
   if (x >= 0 && x < allowedX && y >= 0 && y < allowedY) {
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

   DLLFUNC(DefaultPtrAddEvent)(but, x, y, cl);
}


static void
vnc_clientgone(rfbClientPtr cl)
{
    vnc_dbglog("client disconnected: %s\n", cl->host);

    if (clients > 0)
	clients--;
    if (clients == 0) {
	/* No more clients, pause the emulator. */
	vnc_dbglog("no more clients, pausing..\n");

	/* Disable the mouse. */
	plat_mouse_capture(0);

	pc_pause(1);
    }
}


static enum rfbNewClientAction
vnc_newclient(rfbClientPtr cl)
{
    /* Hook the ClientGone function so we know when they're gone. */
    cl->clientGoneHook = vnc_clientgone;

    vnc_dbglog("new client: %s\n", cl->host);
    if (++clients == 1) {
	/* Reset the mouse. */
	ptr_x = allowedX/2;
	ptr_y = allowedY/2;
	mouse_x = mouse_y = mouse_z = 0;
	mouse_buttons = 0x00;

	/* We now have clients, un-pause the emulator if needed. */
	vnc_dbglog("unpausing..\n");

	/* Enable the mouse. */
	plat_mouse_capture(1);

	pc_pause(0);
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
vnc_blit(bitmap_t *scr, int x, int y, int y1, int y2, int w, int h)
{
    uint32_t *p;
    int yy;

INFO("VNC: blit(%i,%i, %i,%i, %i,%i)\n", x,y, y1,y2, w,h);

    for (yy = y1; yy < y2; yy++) {
	p = (uint32_t *)&(((uint32_t *)rfb->frameBuffer)[yy*VNC_MAX_X]);

	if ((y+yy) >= 0 && (y+yy) < VNC_MAX_Y) {
		if (vid_grayscale || invert_display)
			video_transform_copy(p, &scr->line[y+yy][x], w);
		  else
			memcpy(p, &scr->line[y+yy][x], w*4);
	}
    }
 
    video_blit_done();

    if (! updatingSize)
	DLLFUNC(MarkRectAsModified)(rfb, 0,0, allowedX,allowedY);
}


static void
vnc_close(void)
{
    video_blit_set(NULL);

    if (rfb != NULL) {
	free(rfb->frameBuffer);

	DLLFUNC(ScreenCleanup)(rfb);

	rfb = NULL;
    }

#if USE_VNC == 2
    /* Unload the DLL if possible. */
    if (vnc_handle != NULL) {
	dynld_close(vnc_handle);
	vnc_handle = NULL;
    }
#endif
}


/* Initialize VNC for operation. */
static int
vnc_init(int fs)
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
#if USE_VNC == 2
    wchar_t temp[512];
    const char *fn = PATH_VNC_DLL;
#endif

    /* We do not support fullscreen, folks. */
    if (fs) {
	vnc_errlog("fullscreen mode is not supported!\n");
	return(0);
    }

#if USE_VNC == 2
    /* Try loading the DLL. */
    vnc_handle = dynld_module(fn, vnc_imports);
    if (vnc_handle == NULL) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_NOLIB), "VNCserver", fn);
        ui_msgbox(MBX_ERROR, temp);
	vnc_errlog("unable to load '%s', VNC not available.\n", fn);
	return(0);
    } else {
        INFO("VNC: module '%s' loaded.\n", fn);
    }
#endif

#ifdef _WIN32
    //FIXME: these are defined in Linux version, but not present.. --FvK
    /* Set up and enable VNC server logging. */
    DLLFUNC(Log) = vnc_dbglog;
    DLLFUNC(Err) = vnc_errlog;
    DLLFUNC(LogEnable)(1);
#endif

    if (rfb == NULL) {
	wcstombs(title, ui_window_title(NULL), sizeof(title));
	updatingSize = 0;
	allowedX = scrnsz_x;
	allowedY = scrnsz_y;
 
	rfb = DLLFUNC(GetScreen)(0, NULL, VNC_MAX_X, VNC_MAX_Y, 8, 3, 4);
	rfb->desktopName = title;
	rfb->frameBuffer = (char *)mem_alloc(VNC_MAX_X*VNC_MAX_Y*4);

	rfb->serverFormat = rpf;
	rfb->alwaysShared = TRUE;
	rfb->displayHook = vnc_display;
	rfb->ptrAddEvent = vnc_ptrevent;
	rfb->kbdAddEvent = vnc_kbdevent;
	rfb->newClientHook = vnc_newclient;
 
	/* Set up our current resolution. */
	rfb->width = allowedX;
	rfb->height = allowedY;
 
	DLLFUNC(InitServer)(rfb);

	DLLFUNC(RunEventLoop)(rfb, -1, TRUE);
    }
 
    /* Set up our BLIT handlers. */
    video_blit_set(vnc_blit);

    clients = 0;

    vnc_dbglog("init complete.\n");

    return(1);
}


static void
vnc_resize(int x, int y)
{
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl;

    if (rfb == NULL) return;

    /* TightVNC doesn't like certain sizes.. */
    if (x < VNC_MIN_X || x > VNC_MAX_X || y < VNC_MIN_Y || y > VNC_MAX_Y) {
	vnc_errlog("invalid resolution %ix%i requested!\n", x, y);
	return;
    }

    if ((x != rfb->width || y != rfb->height) && x > 160 && y > 0) {
	vnc_dbglog("updating resolution: %dx%d\n", x, y);
 
	allowedX = (rfb->width < x) ? rfb->width : x;
	allowedY = (rfb->width < y) ? rfb->width : y;
 
	rfb->width = x;
	rfb->height = y;
 
	iterator = DLLFUNC(GetClientIterator)(rfb);
	while ((cl = DLLFUNC(ClientIteratorNext)(iterator)) != NULL) {
		DLLFUNC(ClientLock)(cl);
		cl->newFBSizePending = 1;
		DLLFUNC(ClientUnlock)(cl);
	}
    }
}
 

/* Tell them to pause if we have no clients. */
static int
vnc_pause(void)
{
    return((clients > 0) ? 0 : 1);
}


static void
vnc_screenshot(const wchar_t *fn)
{
    vnc_dbglog("take_screenshot\n");
}


/* See if this module is available or not. */
static int
vnc_available(void)
{
#if USE_VNC == 2
    void *handle;

    handle = dynld_module(PATH_VNC_DLL, NULL);
    if (handle != NULL) {
	dynld_close(handle);
	return(1);
    }

    return(0);
#else
    return(1);
#endif
}


const vidapi_t vnc_vidapi = {
    "vnc",
    "VNC",
    0,
    vnc_init, vnc_close, NULL,
    vnc_resize,
    vnc_pause,
    NULL,
    vnc_screenshot,
    vnc_available
};
