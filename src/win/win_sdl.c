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
 * Version:	@(#)win_sdl.c  	1.0.5	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Michael Drüing, <michael@drueing.de>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include "../version.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
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
static int		sdl_w, sdl_h;


/* Pointers to the real functions. */
static void 		(*sdl_GetVersion)(SDL_version *ver);
static char		*const (*sdl_GetError)(void);
static int 		(*sdl_Init)(Uint32 flags);
static void	 	(*sdl_Quit)(void);
static SDL_Window	*(*sdl_CreateWindowFrom)(const void *data);
static void	 	(*sdl_DestroyWindow)(SDL_Window *window);
static SDL_Renderer	*(*sdl_CreateRenderer)(SDL_Window *window,
						int index, Uint32 flags);
static void	 	(*sdl_DestroyRenderer)(SDL_Renderer *renderer);
static SDL_Texture	*(*sdl_CreateTexture)(SDL_Renderer *renderer,
						Uint32 format, int access,
						int w, int h);
static void	 	(*sdl_DestroyTexture)(SDL_Texture *texture);
static int 		(*sdl_LockTexture)(SDL_Texture *texture,
						const SDL_Rect *rect,
						void **pixels, int *pitch);
static void	 	(*sdl_UnlockTexture)(SDL_Texture *texture);
static int 		(*sdl_RenderCopy)(SDL_Renderer *renderer,
						SDL_Texture *texture,
						const SDL_Rect *srcrect,
						const SDL_Rect *dstrect);
static void	 	(*sdl_RenderPresent)(SDL_Renderer *renderer);


static const dllimp_t sdl_imports[] = {
  { "SDL_GetVersion",		&sdl_GetVersion		},
  { "SDL_GetError",		&sdl_GetError		},
  { "SDL_Init",			&sdl_Init		},
  { "SDL_Quit",			&sdl_Quit		},
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
  { NULL,			NULL			}
};


static void
sdl_blit(int x, int y, int y1, int y2, int w, int h)
{
    SDL_Rect r_src;
    void *pixeldata;
    int pitch;
    int yy;

    if ((y1 == y2) || (buffer32 == NULL)) {
	video_blit_complete();
	return;
    }

    /*
     * TODO:
     * SDL_UpdateTexture() might be better here, as it is
     * (reportedly) slightly faster.
     */
    sdl_LockTexture(sdl_tex, 0, &pixeldata, &pitch);

    for (yy = y1; yy < y2; yy++) {
        if ((y + yy) >= 0 && (y + yy) < buffer32->h)
#if 0
		if (video_grayscale || invert_display)
			video_transform_copy((uint32_t *) &(((uint8_t *)pixeldata)[yy * pitch]), &(((uint32_t *)buffer32->line[y + yy])[x]), w);
		  else
#endif
			memcpy((uint32_t *) &(((uint8_t *)pixeldata)[yy * pitch]), &(((uint32_t *)buffer32->line[y + yy])[x]), w * 4);
    }

    video_blit_complete();

    sdl_UnlockTexture(sdl_tex);

    r_src.x = 0;
    r_src.y = 0;
    r_src.w = w;
    r_src.h = h;

    sdl_RenderCopy(sdl_render, sdl_tex, &r_src, 0);

    sdl_RenderPresent(sdl_render);
}


static void
sdl_close(void)
{
    /* Unregister our renderer! */
    video_setblit(NULL);

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

#if 1
	ShowWindow(hwndRender, TRUE);
	SetFocus(hwndMain);
#endif

	DestroyWindow(sdl_hwnd);
	sdl_hwnd = NULL;
    }

    /* Quit and unload the DLL if possible. */
    if (sdl_handle != NULL) {
	sdl_Quit();

	dynld_close(sdl_handle);
	sdl_handle = NULL;
    }
}


static int
sdl_init(int fs)
{
    wchar_t temp[128];
    SDL_version ver;

    INFO("SDL: init (fs=%d)\n", fs);

    cgapal_rebuild();

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
        sdl_hwnd = CreateWindow(FS_CLASS_NAME,
				temp,
				WS_POPUP,
				0, 0, sdl_w, sdl_h,
				HWND_DESKTOP,
				NULL,
				hInstance,
				NULL);
	DEBUG("SDL: FS %dx%d window at %08lx\n", sdl_w, sdl_h, sdl_hwnd);

	/* Redirect RawInput to this new window. */
	plat_set_input(sdl_hwnd);

	/* Show the window, make it topmost, and give it focus. */
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
    video_setblit(sdl_blit);

    return(1);
}


static void
sdl_resize(int x, int y)
{
    DEBUG("SDL: resizing to %dx%d\n", x, y);
}


static void
sdl_screenshot(const wchar_t *fn)
{
#if 0
    int i, res, x, y, width = 0, height = 0;
    unsigned char* rgba = NULL;
    png_bytep *b_rgb = NULL;
    FILE *fp = NULL;

    sdl_GetWindowSize(sdl_win, &width, &height);

    /* Create file. */
    if ((fp = plat_fopen(fn, L"wb")) == NULL) {
	ERRLOG("SDL: screenshot: file %ls could not be opened for writing\n", fn);
	return;
    }

    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
	ERRLOG("SDL: screenshot: create_write_struct failed\n");
	fclose(fp);
	return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
	ERRLOG("SDL: screenshot: create_info_struct failed");
	fclose(fp);
	return;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, width, height,
	8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    pixels = (uint8_t *)mem_alloc(width * height * 4);
    if (pixels == NULL) {
	ERRLOG("SDL: screenshot: unable to allocate RGBA Bitmap memory\n");
	fclose(fp);
	return;
    }

    res = sdl_RenderReadPixels(sdl_render, NULL,
			       SDL_PIXELFORMAT_ABGR8888, pixels, width * 4);
    if (res != 0) {
	ERRLOG("SDL: screenshot: error reading render pixels\n");
	free(pixels);
	fclose(fp);
	return;
    }

    if ((b_rgb = (png_bytep *)mem_alloc(sizeof(png_bytep) * height)) == NULL) {
	ERRLOG("[sdl_take_screenshot] Unable to Allocate RGB Bitmap Memory");
	free(rgba);
	fclose(fp);
	return;
    }

    for (y = 0; y < height; ++y) {
	b_rgb[y] = (png_byte *)mem_alloc(png_get_rowbytes(png_ptr, info_ptr));
    	for (x = 0; x < width; ++x) {
		b_rgb[y][(x) * 3 + 0] = rgba[(y * width + x) * 4 + 0];
		b_rgb[y][(x) * 3 + 1] = rgba[(y * width + x) * 4 + 1];
		b_rgb[y][(x) * 3 + 2] = rgba[(y * width + x) * 4 + 2];
	}
    }

    png_write_info(png_ptr, info_ptr);

    png_write_image(png_ptr, b_rgb);

    png_write_end(png_ptr, NULL);

    /* cleanup heap allocation */
    for (i = 0; i < height; i++)
	if (b_rgb[i])  free(b_rgb[i]);

    if (b_rgb) free(b_rgb);

    if (rgba) free(rgba);

    if (fp) fclose(fp);
#endif
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


const vidapi_t sdl_vidapi = {
    "SDL",
    1,
    sdl_init,
    sdl_close,
    NULL,
    sdl_resize,
    NULL,
    sdl_screenshot,
    sdl_available
};
