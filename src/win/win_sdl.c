/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Rendering module for libSDL2
 *
 * NOTE:	Given all the problems reported with FULLSCREEN use of SDL,
 *		we will not use that, but, instead, use a new window which
 *		coverrs the entire desktop.
 *
 * Version:	@(#)win_sdl.c  	1.0.11	2019/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Michael Drüing, <michael@drueing.de>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2018 Michael Drüing.
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
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef _MSC_VER
# pragma warning(disable: 4005)
#endif
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../emu.h"
#include "../config.h"
#include "../version.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#if USE_LIBPNG
# include "../png.h"
#endif
#include "../devices/video/video.h"
#include "win.h"
#include "win_sdl.h"


#if USE_SDL != 1
# error This module can only be used dynamically.
#endif


#define PATH_SDL_DLL	"sdl2.dll"


static void		*sdl_handle = NULL;	/* handle to libSDL2 DLL */
static SDL_Window	*sdl_win = NULL;
static SDL_Renderer	*sdl_render = NULL;
static SDL_Texture	*sdl_tex = NULL;
static HWND		sdl_hwnd = NULL;
static HWND		sdl_parent_hwnd = NULL;
static int		sdl_w, sdl_h;
static int		cur_w, cur_h;
static int		sdl_fs;
static int		is_enabled;


/* Pointers to the real functions. */
static void 		(*sdl_GetVersion)(SDL_version *);
static char		*const (*sdl_GetError)(void);
static int 		(*sdl_Init)(Uint32);
static void	 	(*sdl_Quit)(void);
static void		(*sdl_GetWindowSize)(SDL_Window *, int *, int *);
static SDL_Window	*(*sdl_CreateWindowFrom)(const void *);
static void	 	(*sdl_DestroyWindow)(SDL_Window *);
static SDL_Renderer	*(*sdl_CreateRenderer)(SDL_Window *, int, Uint32);
static void	 	(*sdl_DestroyRenderer)(SDL_Renderer *);
static SDL_Texture	*(*sdl_CreateTexture)(SDL_Renderer *, Uint32, int, int, int);
static void	 	(*sdl_DestroyTexture)(SDL_Texture *);
static int 		(*sdl_LockTexture)(SDL_Texture *, const SDL_Rect *,
					   void **, int *);
static void	 	(*sdl_UnlockTexture)(SDL_Texture *);
static int 		(*sdl_RenderCopy)(SDL_Renderer *, SDL_Texture *,
					  const SDL_Rect *, const SDL_Rect *);
static void	 	(*sdl_RenderPresent)(SDL_Renderer *);
static int		(*sdl_RenderReadPixels)(SDL_Renderer *,
					        const SDL_Rect *,
                                                Uint32, void *, int);


static const dllimp_t sdl_imports[] = {
  { "SDL_GetVersion",		&sdl_GetVersion		},
  { "SDL_GetError",		&sdl_GetError		},
  { "SDL_Init",			&sdl_Init		},
  { "SDL_Quit",			&sdl_Quit		},
  { "SDL_GetWindowSize",	&sdl_GetWindowSize	},
  { "SDL_CreateWindowFrom",	&sdl_CreateWindowFrom	},
  { "SDL_DestroyWindow",	&sdl_DestroyWindow	},
  { "SDL_CreateRenderer",	&sdl_CreateRenderer	},
  { "SDL_DestroyRenderer",	&sdl_DestroyRenderer	},
  { "SDL_CreateTexture",	&sdl_CreateTexture	},
  { "SDL_DestroyTexture",	&sdl_DestroyTexture	},
  { "SDL_LockTexture",		&sdl_LockTexture	},
  { "SDL_UnlockTexture",	&sdl_UnlockTexture	},
  { "SDL_RenderCopy",		&sdl_RenderCopy		},
  { "SDL_RenderPresent",	&sdl_RenderPresent	},
  { "SDL_RenderReadPixels",	&sdl_RenderReadPixels	},
  { NULL,			NULL			}
};


static void
sdl_stretch(int *w, int *h, int *x, int *y)
{
    double dw, dh, dx, dy, temp, temp2, ratio_w, ratio_h, gsr, hsr;

    switch (config.vid_fullscreen_scale) {
	case FULLSCR_SCALE_FULL:
		*w = sdl_w;
		*h = sdl_h;
		*x = 0;
		*y = 0;
		break;

	case FULLSCR_SCALE_43:
		dw = (double) sdl_w;
		dh = (double) sdl_h;
		temp = (dh / 3.0) * 4.0;
		dx = (dw - temp) / 2.0;
		dw = temp;
		*w = (int) dw;
		*h = (int) dh;
		*x = (int) dx;
		*y = 0;
		break;

	case FULLSCR_SCALE_SQ:
		dw = (double) sdl_w;
		dh = (double) sdl_h;
		temp = ((double) *w);
		temp2 = ((double) *h);
		dx = (dw / 2.0) - ((dh * temp) / (temp2 * 2.0));
		dy = 0.0;
		if (dx < 0.0) {
			dx = 0.0;
			dy = (dw / 2.0) - ((dh * temp2) / (temp * 2.0));
		}
		dw -= (dx * 2.0);
		dh -= (dy * 2.0);
		*w = (int) dw;
		*h = (int) dh;
		*x = (int) dx;
		*y = (int) dy;
		break;

	case FULLSCR_SCALE_INT:
		dw = (double) sdl_w;
		dh = (double) sdl_h;
		temp = ((double) *w);
		temp2 = ((double) *h);
		ratio_w = dw / ((double) *w);
		ratio_h = dh / ((double) *h);
		if (ratio_h < ratio_w)
			ratio_w = ratio_h;
		dx = (dw / 2.0) - ((temp * ratio_w) / 2.0);
		dy = (dh / 2.0) - ((temp2 * ratio_h) / 2.0);
		dw -= (dx * 2.0);
		dh -= (dy * 2.0);
		*w = (int) dw;
		*h = (int) dh;
		*x = (int) dx;
		*y = (int) dy;
		break;

	case FULLSCR_SCALE_KEEPRATIO:
		dw = (double) sdl_w;
		dh = (double) sdl_h;
		hsr = dw / dh;
		gsr = ((double) *w) / ((double) *h);
		if (gsr <= hsr) {
			temp = dh * gsr;
			dx = (dw - temp) / 2.0;
			dw = temp;
			*w = (int) dw;
			*h = (int) dh;
			*x = (int) dx;
			*y = 0;
		} else {
			temp = dw / gsr;
			dy = (dh - temp) / 2.0;
			dh = temp;
			*w = (int) dw;
			*h = (int) dh;
			*x = 0;
			*y = (int) dy;
		}
		break;
    }
}


static void
sdl_resize(int x, int y)
{
    int ww = 0, wh = 0, wx = 0, wy = 0;

    DEBUG("SDL: resizing to %dx%d\n", x, y);

    if ((x == cur_w) && (y == cur_h)) return;

    DEBUG("sdl_resize(%i, %i)\n", x, y);
    ww = x;
    wh = y;
    sdl_stretch(&ww, &wh, &wx, &wy);

    MoveWindow(sdl_hwnd, wx, wy, ww, wh, TRUE);

    cur_w = x;
    cur_h = y;
}


static void
sdl_blit(bitmap_t *scr, int x, int y, int y1, int y2, int w, int h)
{
    SDL_Rect r_src;
    void *pixeldata;
    int xx, yy, ret;
    int pitch;

    if (! is_enabled) {
	video_blit_done();
	return;
    }

    if ((y1 == y2) || (scr == NULL)) {
	video_blit_done();
	return;
    }

    /*
     * TODO:
     * SDL_UpdateTexture() might be better here, as it is
     * (reportedly) slightly faster.
     */
    sdl_LockTexture(sdl_tex, 0, &pixeldata, &pitch);

    for (yy = y1; yy < y2; yy++) {
       	if ((y + yy) >= 0 && (y + yy) < scr->h) {
		if (config.vid_grayscale || config.invert_display)
			video_transform_copy((uint32_t *) &(((uint8_t *)pixeldata)[yy * pitch]), &scr->line[y + yy][x], w);
		else
			memcpy((uint32_t *) &(((uint8_t *)pixeldata)[yy * pitch]), &scr->line[y + yy][x], w * 4);
	}
    }

    video_blit_done();

    sdl_UnlockTexture(sdl_tex);

    if (sdl_fs) {
	get_screen_size_natural(&xx, &yy);
	DEBUG("sdl_blit(%i, %i, %i, %i, %i, %i) (%i, %i)\n", x, y, y1, y2, w, h, xx, yy);
	if (w == xx)
		sdl_resize(w, h);
	DEBUG("(%08X, %08X, %08X)\n", sdl_win, sdl_render, sdl_tex);
    }

    r_src.x = 0;
    r_src.y = 0;
    r_src.w = w;
    r_src.h = h;

    ret = sdl_RenderCopy(sdl_render, sdl_tex, &r_src, 0);
    if (ret)
	DEBUG("SDL: unable to copy texture to renderer (%s)\n", sdl_GetError());

    sdl_RenderPresent(sdl_render);
}


static void
sdl_close(void)
{
    /* Unregister our renderer! */
    video_blit_set(NULL);

    if (sdl_tex != NULL) {
	sdl_DestroyTexture(sdl_tex);
	sdl_tex = NULL;
    }

    if (sdl_render != NULL) {
	sdl_DestroyRenderer(sdl_render);
	sdl_render = NULL;
    }

    if (sdl_win != NULL) {
	sdl_DestroyWindow(sdl_win);
	sdl_win = NULL;
    }

    if (sdl_hwnd != NULL) {
	plat_set_input(hwndMain);

	ShowWindow(hwndRender, TRUE);
	SetFocus(hwndMain);

	DestroyWindow(sdl_hwnd);
	sdl_hwnd = NULL;
    }

    if (sdl_parent_hwnd != NULL) {
	DestroyWindow(sdl_parent_hwnd);
	sdl_parent_hwnd = NULL;
    }

    /* Quit and unload the DLL if possible. */
    if (sdl_handle != NULL) {
	sdl_Quit();

	dynld_close(sdl_handle);
	sdl_handle = NULL;
    }

    is_enabled = 0;
}


static int
sdl_init(int fs)
{
    wchar_t temp[128];
    SDL_version ver;
    int x, y;

    INFO("SDL: init (fs=%d)\n", fs);

    /* Try loading the DLL. */
    sdl_handle = dynld_module(PATH_SDL_DLL, sdl_imports);
    if (sdl_handle == NULL) {
	ERRLOG("SDL: unable to load '%s', SDL not available.\n", PATH_SDL_DLL);
	return(0);
    }

    /* Get and log the version of the DLL we are using. */
    sdl_GetVersion(&ver);
    INFO("SDL: version %d.%d.%d\n", ver.major, ver.minor, ver.patch);

    /* Initialize the SDL system. */
    if (sdl_Init(SDL_INIT_VIDEO) < 0) {
	ERRLOG("SDL: initialization failed (%s)\n", sdl_GetError());
	return(0);
    }

    if (fs) {
	/* Get the size of the (current) desktop. */
	sdl_w = GetSystemMetrics(SM_CXSCREEN);
	sdl_h = GetSystemMetrics(SM_CYSCREEN);

	/* Create the desktop-covering window. */
        swprintf(temp, sizeof_w(temp),
		 L"%s v%s Full-Screen", EMU_NAME, emu_version);
        sdl_parent_hwnd = CreateWindow(FS_CLASS_NAME,
				temp,
				WS_POPUP,
				0, 0, sdl_w, sdl_h,
				HWND_DESKTOP,
				NULL,
				hInstance,
				NULL);

	SetWindowPos(sdl_parent_hwnd, HWND_TOPMOST,
		     0, 0, sdl_w, sdl_h, SWP_SHOWWINDOW);

	/* Create the actual rendering window. */
	swprintf(temp, sizeof_w(temp), L"%s v%s", EMU_NAME, emu_version);
        sdl_hwnd = CreateWindow(FS_CLASS_NAME,
				temp,
				WS_POPUP,
				0, 0, sdl_w, sdl_h,
				sdl_parent_hwnd,
				NULL,
				hInstance,
				NULL);
	DEBUG("SDL: FS %dx%d window at %08lx\n", sdl_w, sdl_h, sdl_hwnd);

	/* Redirect RawInput to this new window. */
	plat_set_input(sdl_hwnd);

	/* Show the window, make it topmost, and give it focus. */
	get_screen_size_natural(&x, &y);
	sdl_stretch(&sdl_w, &sdl_h, &x, &y);
	SetWindowPos(sdl_hwnd, HWND_TOPMOST,
		     0, 0, sdl_w, sdl_h, SWP_SHOWWINDOW);

	/* Now create the SDL window from that. */
	sdl_win = sdl_CreateWindowFrom((void *)sdl_hwnd);
    } else {
	/* Create the SDL window from the render window. */
	sdl_win = sdl_CreateWindowFrom((void *)hwndRender);
    }

    if (sdl_win == NULL) {
	ERRLOG("SDL: unable to CreateWindowFrom (%s)\n", sdl_GetError());
	sdl_close();
	return(0);
    }
    sdl_fs = fs;

    /*
     * TODO:
     * SDL_RENDERER_SOFTWARE, because SDL tries to do funky stuff
     * otherwise (it turns off Win7 Aero and it looks like it's
     * trying to switch to fullscreen even though the window is
     * not a fullscreen window?)
     */
    sdl_render = sdl_CreateRenderer(sdl_win, -1, SDL_RENDERER_SOFTWARE);
    if (sdl_render == NULL) {
	ERRLOG("SDL: unable to create renderer (%s)\n", sdl_GetError());
	sdl_close();
        return(0);
    }

    /*
     * TODO:
     * Actually the source is (apparently) XRGB8888, but the alpha
     * channel seems to be set to 255 everywhere, so ARGB8888 works
     * just as well.
     */
    sdl_tex = sdl_CreateTexture(sdl_render, SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING, 2048, 2048);
    if (sdl_tex == NULL) {
	ERRLOG("SDL: unable to create texture (%s)\n", sdl_GetError());
	sdl_close();
        return(0);
    }

    /* Make sure we get a clean exit. */
    atexit(sdl_close);

    /* Register our renderer! */
    video_blit_set(sdl_blit);

    is_enabled = 1;

    return(1);
}


static void
sdl_screenshot(const wchar_t *fn)
{
    wchar_t temp[256];
    int width = 0, height = 0;
    uint8_t *pixels = NULL;
    int i = 0, res;

    sdl_GetWindowSize(sdl_win, &width, &height);

    pixels = (uint8_t *)mem_alloc(width * height * 4);
    if (pixels == NULL) {
	ERRLOG("SDL: screenshot: unable to allocate RGBA Bitmap memory\n");
	return;
    }

    res = sdl_RenderReadPixels(sdl_render, NULL,
			       SDL_PIXELFORMAT_ABGR8888, pixels, width * 4);
    if (res != 0) {
	ERRLOG("SDL: screenshot: error reading render pixels\n");
	free(pixels);
	return;
    }

#ifdef USE_LIBPNG
    /* Save the screenshot, using PNG. */
    i = png_write_rgb(fn, 0, pixels, (int16_t)width, (int16_t)height);
#endif

    if (pixels)
	free(pixels);

    /* Show error message if needed. */
    if (i == 0) {
	swprintf(temp, sizeof_w(temp),
		 get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
    }
}


/* See if this module is available or not. */
static int
sdl_available(void)
{
    void *handle;

    handle = dynld_module(PATH_SDL_DLL, NULL);
    if (handle != NULL) return(1);

    return(0);
}


static void
sdl_enable(int yes)
{
    is_enabled = yes;
}


const vidapi_t sdl_vidapi = {
    "sdl",
    "SDL2",
    1,
    sdl_init, sdl_close, NULL,
    sdl_resize,
    NULL,
    sdl_enable,
    sdl_screenshot,
    sdl_available
};
