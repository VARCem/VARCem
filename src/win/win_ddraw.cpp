/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Rendering module for Microsoft DirectDraw 9.
 *
 *		If configured with USE_LIBPNG, we try to load the external
 *		PNG library and use that if found. Otherwise, we fall back
 *		the original mode, which uses the Windows/DDraw built-in BMP
 *		format.
 *
 * Version:	@(#)win_ddraw.cpp	1.0.16	2018/06/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#define UNICODE
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#ifdef USE_LIBPNG
# define PNG_DEBUG 0
# include <png.h>
#endif
#include "../emu.h"
#include "../device.h"
#include "../ui/ui.h"
#include "../plat.h"
#ifdef _MSC_VER
# pragma warning(disable: 4200)
#endif
#include "../devices/video/video.h"
#include "win.h"


#ifdef USE_LIBPNG
# define PATH_PNG_DLL		"libpng16.dll"
#endif


static LPDIRECTDRAW4		lpdd4 = NULL;
static LPDIRECTDRAWSURFACE4	lpdds_pri = NULL,
				lpdds_back = NULL,
				lpdds_back2 = NULL;
static LPDIRECTDRAWCLIPPER	lpdd_clipper = NULL;
static HWND			ddraw_hwnd;
static int			ddraw_w, ddraw_h,
				xs, ys, ys2;
#ifdef USE_LIBPNG
static void			*png_handle = NULL;	/* handle to DLL */
# if USE_LIBPNG == 1
#  define PNGFUNC(x)		png_ ## x
# else
#  define PNGFUNC(x)		PNG_ ## x


/* Pointers to the real functions. */
extern "C" {
png_structp	(*PNG_create_write_struct)(png_const_charp user_png_ver,
						png_voidp error_ptr,
						png_error_ptr error_fn,
						png_error_ptr warn_fn);
void		(*PNG_destroy_write_struct)(png_structpp png_ptr_ptr,
					    png_infopp info_ptr_ptr);
png_infop	(*PNG_create_info_struct)(png_const_structrp png_ptr);
void		(*PNG_init_io)(png_structrp png_ptr, png_FILE_p fp);
void		(*PNG_set_IHDR)(png_const_structrp png_ptr,
				     png_inforp info_ptr, png_uint_32 width,
				     png_uint_32 height, int bit_depth,
				     int color_type, int interlace_method,
				     int compression_method,
				     int filter_method);
png_size_t	(*PNG_get_rowbytes)(png_const_structrp png_ptr,
				    png_const_inforp info_ptr);
void		(*PNG_write_info)(png_structrp png_ptr,
				       png_const_inforp info_ptr);
void		(*PNG_write_image)(png_structrp png_ptr,
					png_bytepp image);
void		(*PNG_write_end)(png_structrp png_ptr,
				      png_inforp info_ptr);
};


static const dllimp_t png_imports[] = {
  { "png_create_write_struct",	&PNG_create_write_struct	},
  { "png_destroy_write_struct",	&PNG_destroy_write_struct	},
  { "png_create_info_struct",	&PNG_create_info_struct		},
  { "png_init_io",		&PNG_init_io			},
  { "png_set_IHDR",		&PNG_set_IHDR			},
  { "png_get_rowbytes",		&PNG_get_rowbytes		},
  { "png_write_info",		&PNG_write_info			},
  { "png_write_image",		&PNG_write_image		},
  { "png_write_end",		&PNG_write_end			},
  { NULL,			NULL				}
};
# endif
#endif


static const char *
GetError(HRESULT hr)
{
    const char *err = "Unknown";

    switch(hr) {
	case DDERR_INCOMPATIBLEPRIMARY:
		err = "Incompatible Primary";
		break;

	case DDERR_INVALIDCAPS:
		err = "Invalid Caps";
		break;

	case DDERR_INVALIDOBJECT:
		err = "Invalid Object";
		break;

	case DDERR_INVALIDPARAMS:
		err = "Invalid Parameters";
		break;

	case DDERR_INVALIDPIXELFORMAT:
		err = "Invalid Pixel Format";
		break;

	case DDERR_NOALPHAHW:
		err = "Hardware does not support Alpha";
		break;

	case DDERR_NOCOOPERATIVELEVELSET:
		err = "No cooperative level set";
		break;

	case DDERR_NODIRECTDRAWHW:
		err = "Hardware does not support DirectDraw";
		break;

	case DDERR_NOEMULATION:
		err = "No emulation";
		break;

	case DDERR_NOEXCLUSIVEMODE:
		err = "No exclusive mode available";
		break;

	case DDERR_NOFLIPHW:
		err = "Hardware does not support flipping";
		break;

	case DDERR_NOMIPMAPHW:
		err = "Hardware does not support MipMap";
		break;

	case DDERR_NOOVERLAYHW:
		err = "Hardware does not support overlays";
		break;

	case DDERR_NOZBUFFERHW:
		err = "Hardware does not support Z buffers";
		break;

	case DDERR_OUTOFMEMORY:
		err = "Out of memory";
		break;

	case DDERR_OUTOFVIDEOMEMORY:
		err = "Out of video memory";
		break;

	case DDERR_PRIMARYSURFACEALREADYEXISTS:
		err = "Primary Surface already exists";
		break;

	case DDERR_UNSUPPORTEDMODE:
		err = "Mode not supported";
		break;

	default:
		break;
    }

    return(err);
}


static void
ddraw_fs_size_default(RECT w_rect, RECT *r_dest)
{
    r_dest->left   = 0;
    r_dest->top    = 0;
    r_dest->right  = (w_rect.right  - w_rect.left) - 1;
    r_dest->bottom = (w_rect.bottom - w_rect.top) - 1;
}


static void
ddraw_fs_size(RECT w_rect, RECT *r_dest, int w, int h)
{
    int ratio_w, ratio_h;
    double hsr, gsr, ra, d;

    switch (vid_fullscreen_scale) {
	case FULLSCR_SCALE_FULL:
		ddraw_fs_size_default(w_rect, r_dest);
		break;

	case FULLSCR_SCALE_43:
		r_dest->top    = 0;
		r_dest->bottom = (w_rect.bottom - w_rect.top) - 1;
		r_dest->left   = ((w_rect.right  - w_rect.left) / 2) - (((w_rect.bottom - w_rect.top) * 4) / (3 * 2));
		r_dest->right  = ((w_rect.right  - w_rect.left) / 2) + (((w_rect.bottom - w_rect.top) * 4) / (3 * 2)) - 1;
		if (r_dest->left < 0) {
			r_dest->left   = 0;
			r_dest->right  = (w_rect.right  - w_rect.left) - 1;
			r_dest->top    = ((w_rect.bottom - w_rect.top) / 2) - (((w_rect.right - w_rect.left) * 3) / (4 * 2));
			r_dest->bottom = ((w_rect.bottom - w_rect.top) / 2) + (((w_rect.right - w_rect.left) * 3) / (4 * 2)) - 1;
		}
		break;

	case FULLSCR_SCALE_SQ:
		r_dest->top    = 0;
		r_dest->bottom = (w_rect.bottom - w_rect.top) - 1;
		r_dest->left   = ((w_rect.right  - w_rect.left) / 2) - (((w_rect.bottom - w_rect.top) * w) / (h * 2));
		r_dest->right  = ((w_rect.right  - w_rect.left) / 2) + (((w_rect.bottom - w_rect.top) * w) / (h * 2)) - 1;
		if (r_dest->left < 0) {
			r_dest->left   = 0;
			r_dest->right  = (w_rect.right  - w_rect.left) - 1;
			r_dest->top    = ((w_rect.bottom - w_rect.top) / 2) - (((w_rect.right - w_rect.left) * h) / (w * 2));
			r_dest->bottom = ((w_rect.bottom - w_rect.top) / 2) + (((w_rect.right - w_rect.left) * h) / (w * 2)) - 1;
		}
		break;

	case FULLSCR_SCALE_INT:
		ratio_w = (w_rect.right  - w_rect.left) / w;
		ratio_h = (w_rect.bottom - w_rect.top)  / h;
		if (ratio_h < ratio_w)
			ratio_w = ratio_h;
		r_dest->left   = ((w_rect.right  - w_rect.left) / 2) - ((w * ratio_w) / 2);
		r_dest->right  = ((w_rect.right  - w_rect.left) / 2) + ((w * ratio_w) / 2) - 1;
		r_dest->top    = ((w_rect.bottom - w_rect.top)  / 2) - ((h * ratio_w) / 2);
		r_dest->bottom = ((w_rect.bottom - w_rect.top)  / 2) + ((h * ratio_w) / 2) - 1;
		break;

	case FULLSCR_SCALE_KEEPRATIO:
		hsr = ((double) (w_rect.right  - w_rect.left)) / ((double) (w_rect.bottom - w_rect.top));
		gsr = ((double) w) / ((double) h);

		if (hsr > gsr) {
			/* Host ratio is bigger than guest ratio. */
			ra = ((double) (w_rect.bottom - w_rect.top)) / ((double) h);

			d = ((double) w) * ra;
			d = (((double) (w_rect.right  - w_rect.left)) - d) / 2.0;

			r_dest->left   = ((int) d);
			r_dest->right  = (w_rect.right  - w_rect.left) - ((int) d) - 1;
			r_dest->top    = 0;
			r_dest->bottom = (w_rect.bottom - w_rect.top)  - 1;
		} else if (hsr < gsr) {
			/* Host ratio is smaller or rqual than guest ratio. */
			ra = ((double) (w_rect.right  - w_rect.left)) / ((double) w);

			d = ((double) h) * ra;
			d = (((double) (w_rect.bottom - w_rect.top)) - d) / 2.0;

			r_dest->left   = 0;
			r_dest->right  = (w_rect.right  - w_rect.left) - 1;
			r_dest->top    = ((int) d);
			r_dest->bottom = (w_rect.bottom - w_rect.top)  - ((int) d) - 1;
		} else {
			/* Host ratio is equal to guest ratio. */
			ddraw_fs_size_default(w_rect, r_dest);
		}
		break;
    }
}


static void
ddraw_blit_fs(int x, int y, int y1, int y2, int w, int h)
{
    DDSURFACEDESC2 ddsd;
    RECT r_src, r_dest, w_rect;
    DDBLTFX ddbltfx;
    HRESULT hr;
    int yy;

    if (lpdds_back == NULL) {
	video_blit_complete();
	return; /*Nothing to do*/
    }

    if ((y1 == y2) || (h <= 0)) {
	video_blit_complete();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = lpdds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back->Restore();
	lpdds_back->Lock(NULL, &ddsd,
			 DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
	device_force_redraw();
    }
    if (! ddsd.lpSurface) {
	video_blit_complete();
	return;
    }

    for (yy = y1; yy < y2; yy++)
	if (buffer32)  memcpy((void *)((uintptr_t)ddsd.lpSurface + (yy * ddsd.lPitch)), &(((uint32_t *)buffer32->line[y + yy])[x]), w * 4);
    video_blit_complete();
    lpdds_back->Unlock(NULL);

    w_rect.left = 0;
    w_rect.top = 0;
    w_rect.right = ddraw_w;
    w_rect.bottom = ddraw_h;
    ddraw_fs_size(w_rect, &r_dest, w, h);

    r_src.left   = 0;
    r_src.top    = 0;       
    r_src.right  = w;
    r_src.bottom = h;

    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwFillColor = 0;

    lpdds_back2->Blt(&w_rect, NULL, NULL,
		     DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);

    hr = lpdds_back2->Blt(&r_dest, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back2->Restore();
	lpdds_back2->Blt(&r_dest, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    }
	
    hr = lpdds_pri->Flip(NULL, DDFLIP_NOVSYNC);	
    if (hr == DDERR_SURFACELOST) {
	lpdds_pri->Restore();
	lpdds_pri->Flip(NULL, DDFLIP_NOVSYNC);
    }
}


static void
ddraw_blit(int x, int y, int y1, int y2, int w, int h)
{
    DDSURFACEDESC2 ddsd;
    RECT r_src, r_dest;
    HRESULT hr;
    POINT po;
    int yy;

    if (lpdds_back == NULL) {
	video_blit_complete();
	return; /*Nothing to do*/
    }

    if ((y1 == y2) || (h <= 0)) {
	video_blit_complete();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = lpdds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back->Restore();
	lpdds_back->Lock(NULL, &ddsd,
			 DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
	device_force_redraw();
    }

    if (! ddsd.lpSurface) {
	video_blit_complete();
	return;
    }

    for (yy = y1; yy < y2; yy++) {
	if (buffer32)
		if ((y + yy) >= 0 && (y + yy) < buffer32->h)
			memcpy((uint32_t *) &(((uint8_t *) ddsd.lpSurface)[yy * ddsd.lPitch]), &(((uint32_t *)buffer32->line[y + yy])[x]), w * 4);
    }
    video_blit_complete();
    lpdds_back->Unlock(NULL);

    po.x = po.y = 0;
	
    ClientToScreen(ddraw_hwnd, &po);
    GetClientRect(ddraw_hwnd, &r_dest);
    OffsetRect(&r_dest, po.x, po.y);	
	
    r_src.left   = 0;
    r_src.top    = 0;       
    r_src.right  = w;
    r_src.bottom = h;

    hr = lpdds_back2->Blt(&r_src, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_back2->Restore();
	lpdds_back2->Blt(&r_src, lpdds_back, &r_src, DDBLT_WAIT, NULL);
    }

    lpdds_back2->Unlock(NULL);
	
    hr = lpdds_pri->Blt(&r_dest, lpdds_back2, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	lpdds_pri->Restore();
	lpdds_pri->Blt(&r_dest, lpdds_back2, &r_src, DDBLT_WAIT, NULL);
    }
}


static void
ddraw_close(void)
{
    pclog("DDRAW: close\n");

    video_setblit(NULL);

    if (lpdds_back2 != NULL) {
	lpdds_back2->Release();
	lpdds_back2 = NULL;
    }
    if (lpdds_back != NULL) {
	lpdds_back->Release();
	lpdds_back = NULL;
    }
    if (lpdds_pri != NULL) {
	lpdds_pri->Release();
	lpdds_pri = NULL;
    }
    if (lpdd_clipper != NULL) {
	lpdd_clipper->Release();
	lpdd_clipper = NULL;
    }
    if (lpdd4 != NULL) {
	lpdd4->Release();
	lpdd4 = NULL;
    }

#if defined(USE_LIBPNG) && USE_LIBPNG == 2
    /* Unload the DLL if possible. */
    if (png_handle != NULL) {
	dynld_close(png_handle);
	png_handle = NULL;
    }
#endif
}


static int
ddraw_init(int fs)
{
    DDSURFACEDESC2 ddsd;
    LPDIRECTDRAW lpdd;
    HRESULT hr;
    DWORD dw;
    HWND h;

    pclog("DDraw: initializing (fs=%d)\n", fs);

    cgapal_rebuild();

    hr = DirectDrawCreate(NULL, &lpdd, NULL);
    if (FAILED(hr)) {
	pclog("DDRAW: cannot create an instance (%s)\n", GetError(hr));
	return(0);
    }
    hr = lpdd->QueryInterface(IID_IDirectDraw4, (LPVOID *)&lpdd4);
    if (FAILED(hr)) {
	pclog("DDRAW: no interfaces found (%s)\n", GetError(hr));
	return(0);
    }
    lpdd->Release();

    atexit(ddraw_close);

    if (fs) {
	dw = DDSCL_SETFOCUSWINDOW | DDSCL_CREATEDEVICEWINDOW | \
	     DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT;
	h = hwndMain;
    } else {
	dw = DDSCL_NORMAL;
	h = hwndRender;
    }
    hr = lpdd4->SetCooperativeLevel(h, dw);
    if (FAILED(hr)) {
	pclog("DDRAW: SetCooperativeLevel failed (%s)\n", GetError(hr));
	return(0);
    }

    if (fs) {
	ddraw_w = GetSystemMetrics(SM_CXSCREEN);
	ddraw_h = GetSystemMetrics(SM_CYSCREEN);
	hr = lpdd4->SetDisplayMode(ddraw_w, ddraw_h, 32, 0, 0);
	if (FAILED(hr)) {
		pclog("DDRAW: SetDisplayMode failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (fs) {
	ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps |= (DDSCAPS_COMPLEX | DDSCAPS_FLIP);
	ddsd.dwBackBufferCount = 1;
    }
    hr = lpdd4->CreateSurface(&ddsd, &lpdds_pri, NULL);
    if (FAILED(hr)) {
	pclog("DDRAW: CreateSurface failed (%s)\n", GetError(hr));
	return(0);
    }

    if (fs) {
	memset(&ddsd, 0x00, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = lpdds_pri->GetAttachedSurface(&ddsd.ddsCaps, &lpdds_back2);
	if (FAILED(hr)) {
		pclog("DDRAW: GetAttachedSurface failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
    if (FAILED(hr)) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
	if (FAILED(hr)) {
		pclog("DDRAW: CreateSurface(back) failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    if (! fs) {
	memset(&ddsd, 0x00, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
	if (FAILED(hr)) {
		ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.dwWidth = 2048;
		ddsd.dwHeight = 2048;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
		if (FAILED(hr)) {
			pclog("DDRAW: CreateSurface(back2) failed (%s)\n", GetError(hr));
			return(0);
		}
	}

	hr = lpdd4->CreateClipper(0, &lpdd_clipper, NULL);
	if (FAILED(hr)) {
		pclog("DDRAW: CreateClipper failed (%s)\n", GetError(hr));
		return(0);
	}

	hr = lpdd_clipper->SetHWnd(0, h);
	if (FAILED(hr)) {
		pclog("DDRAW: SetHWnd failed (%s)\n", GetError(hr));
		return(0);
	}

	hr = lpdds_pri->SetClipper(lpdd_clipper);
	if (FAILED(hr)) {
		pclog("DDRAW: SetClipper failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    ddraw_hwnd = h;

    if (fs)
	video_setblit(ddraw_blit_fs);
      else
	video_setblit(ddraw_blit);

#ifdef USE_LIBPNG
# if USE_LIBPNG == 2
    /* Try loading the DLL. */
    png_handle = dynld_module(PATH_PNG_DLL, png_imports);
    if (png_handle == NULL)
	pclog("DDraw: unable to load '%s', using BMP for screenshots.\n",
							PATH_PNG_DLL);
# else
    png_handle = (void *)1;	/* just to indicate always therse */
# endif
#endif

    return(1);
}


#ifdef USE_LIBPNG
static void
error_handler(png_structp arg, const char *str)
{
    pclog("PNG: stream 0x%08lx error '%s'\n", arg, str);
}


static void
warning_handler(png_structp arg, const char *str)
{
    pclog("PNG: stream 0x%08lx warning '%s'\n", arg, str);
}


static int
SavePNG(const wchar_t *fn, BITMAPINFO *bmi, uint8_t *pixels)
{
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytepp rows;
    uint8_t *r, *b;
    FILE *fp;
    int h, w;

    /* Create the image file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	pclog("[SavePNG] File %ls could not be opened for writing!\n", fn);
error:
	if (png != NULL)
		PNGFUNC(destroy_write_struct)(&png, &info);
	if (fp != NULL)
		(void)fclose(fp);
	return(0);
    }

    /* Initialize PNG stuff. */
    png = PNGFUNC(create_write_struct)(PNG_LIBPNG_VER_STRING, NULL,
				       error_handler, warning_handler);
    if (png == NULL) {
	pclog("[SavePNG] create_write_struct failed!\n");
	goto error;
    }

    info = PNGFUNC(create_info_struct)(png);
    if (info == NULL) {
	pclog("[SavePNG] create_info_struct failed!\n");
	goto error;
    }

    PNGFUNC(init_io)(png, fp);

    PNGFUNC(set_IHDR)(png, info,
		      bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 8,
		      PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    PNGFUNC(write_info)(png, info);

    /* Create a buffer for scanlines of pixels. */
    rows = (png_bytepp)malloc(sizeof(png_bytep) * bmi->bmiHeader.biHeight);
    for (h = 0; h < bmi->bmiHeader.biHeight; h++) {
	/* Create a buffer for this scanline. */
	rows[h] = (png_bytep)malloc(PNGFUNC(get_rowbytes)(png, info));
    }

    /*
     * Process all scanlines in the image.
     *
     * Since the bitmap is un bottom-up mode, we have to convert
     * all pixels to RGB mode, but also 'flip' the image to the
     * normal top-down mode.
     */
    for (h = 0; h < bmi->bmiHeader.biHeight; h++) {
	for (w = 0; w < bmi->bmiHeader.biWidth; w++) {
		/* Get pointer to pixel in bitmap data. */
                b = &pixels[((h * bmi->bmiHeader.biWidth) + w) * 4];

		/* Get pointer to png row data. */
		r = &rows[(bmi->bmiHeader.biHeight - 1) - h][w * 3];

                /* Copy the pixel data. */
                r[0] = b[2];
                r[1] = b[1];
                r[2] = b[0];
	}
    }

    /* Write image to the file. */
    PNGFUNC(write_image)(png, rows);

    /* No longer need the row buffers. */
    for (h = 0; h < bmi->bmiHeader.biHeight; h++)
	free(rows[h]);
    free(rows);

    PNGFUNC(write_end)(png, NULL);

    PNGFUNC(destroy_write_struct)(&png, &info);

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}
#endif


static int
SaveBMP(const wchar_t *fn, BITMAPINFO *bmi, uint8_t *pixels)
{
    BITMAPFILEHEADER bmpHdr; 
    FILE *fp;

    if ((fp = plat_fopen(fn, L"wb")) == NULL) {
	pclog("[SaveBMP] File %ls could not be opened for writing!\n", fn);
	return(0);
    } 

    memset(&bmpHdr, 0x00, sizeof(BITMAPFILEHEADER));
    bmpHdr.bfSize = sizeof(BITMAPFILEHEADER) + \
		    sizeof(BITMAPINFOHEADER) + bmi->bmiHeader.biSizeImage;
    bmpHdr.bfType = 0x4D42;
    bmpHdr.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); 

    (void)fwrite(&bmpHdr, sizeof(BITMAPFILEHEADER), 1, fp);
    (void)fwrite(&bmi->bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    (void)fwrite(pixels, bmi->bmiHeader.biSizeImage, 1, fp); 

    /* Clean up. */
    (void)fclose(fp);

    return(1);
}


static HBITMAP
CopySurface(IDirectDrawSurface4 *pDDSurface)
{ 
    HBITMAP hBmp, hOldBmp;
    DDSURFACEDESC2 ddsd;
    HDC hDC, hMemDC;

    pDDSurface->GetDC(&hDC);
    hMemDC = CreateCompatibleDC(hDC); 
    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    pDDSurface->GetSurfaceDesc(&ddsd);
    hBmp = CreateCompatibleBitmap(hDC, xs, ys);
    hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
    BitBlt(hMemDC, 0, 0, xs, ys, hDC, 0, 0, SRCCOPY);    
    SelectObject(hMemDC, hOldBmp); 
    DeleteDC(hMemDC);
    pDDSurface->ReleaseDC(hDC);

    return(hBmp);
}


static void
ddraw_screenshot(const wchar_t *fn)
{
    wchar_t path[512];
    wchar_t temp[512];
    BITMAPINFO bmi;
    uint8_t *pixels;
    uint8_t *pix2;
    HBITMAP hBmp;
    HDC hDC;
    int i;

#if 0
    xs = xsize;
    ys = ys2 = ysize;

    /* For EGA/(S)VGA, the size is NOT adjusted for overscan. */
    if ((overscan_y > 16) && enable_overscan) {
	xs += overscan_x;
	ys += overscan_y;
    }

    /* For CGA, the width is adjusted for overscan, but the height is not. */
    if (overscan_y == 16) {
	if (ys2 <= 250)
		ys += (overscan_y >> 1);
	  else
		ys += overscan_y;
    }
#endif

    xs = get_actual_size_x();
    ys = ys2 = get_actual_size_y();

    if (ysize <= 250) {
	ys >>= 1;
	ys2 >>= 1;
    }

    /* Create a surface copy and store it as a bitmap. */
    hBmp = CopySurface(lpdds_back);

    /* Create a compatible DC. */
    hDC = GetDC(NULL);

    /* Request the size info from the bitmap. */
    memset(&bmi, 0x00, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    GetDIBits(hDC, hBmp, 0, 0, NULL, &bmi, DIB_RGB_COLORS);
    if (bmi.bmiHeader.biSizeImage <= 0) {
	bmi.bmiHeader.biSizeImage = bmi.bmiHeader.biWidth * abs(bmi.bmiHeader.biHeight) * (bmi.bmiHeader.biBitCount + 7) / 8;
    }

    /* Allocate a buffer for the pixel data. */
    if ((pixels = (uint8_t *)malloc(bmi.bmiHeader.biSizeImage)) == NULL) {
	pclog("DDraw: unable to allocate bitmap memory!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	ReleaseDC(NULL, hDC); 
	return;
    }

    /* Now get the actual pixel data from the bitmap. */
    bmi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(hDC, hBmp, 0, bmi.bmiHeader.biHeight,
	      pixels, &bmi, DIB_RGB_COLORS);

    /* No longer need the DC. */
    ReleaseDC(NULL, hDC); 

    /*
     * For some CGA modes (320x200, 640x200 etc) we double-up
     * the image height so it looks a little better. We simply
     * copy each real scanline.
     */
    if (ys <= 250) {
	/* Save current buffer. */
	pix2 = pixels;

	/* Update bitmap image size. */
	bmi.bmiHeader.biHeight <<= 1;
	bmi.bmiHeader.biSizeImage <<= 1;

	/* Allocate new buffer, doubled-up. */
	pixels = (uint8_t *)malloc(bmi.bmiHeader.biSizeImage);

	/* Copy scanlines. */
	for (i = 0; i < ys; i++) {
		/* Copy original line. */
		memcpy(pixels + (i * xs * 8),
		       pix2 + (i * xs * 4), xs * 4);

		/* And copy it once more, doubling it. */
		memcpy(pixels + ((i * xs * 8) + (xs * 4)),
		       pix2 + (i * xs * 4), xs * 4);
	}

	/* No longer need original buffer. */
	free(pix2);
    }

    /* Save filename. */
    wcscpy(path, fn);

#ifdef USE_LIBPNG
    /* Save the screenshot, using PNG if available. */
    if (png_handle != NULL) {
	/* Use the PNG library. */
	i = SavePNG(path, &bmi, pixels);
    } else {
#endif
	/* Use BMP, so fix the file name. */
	path[wcslen(path)-3] = L'b';
	path[wcslen(path)-2] = L'm';
	path[wcslen(path)-1] = L'p';
	i = SaveBMP(path, &bmi, pixels);
#ifdef USE_LIBPNG
    }
#endif

    /* Release pixel buffer. */
    free(pixels);

    /* Show error message if needed. */
    if (i == 0) {
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), path);
	ui_msgbox(MBX_ERROR, temp);
    }
}


const vidapi_t ddraw_vidapi = {
    "DDraw",
    1,
    ddraw_init,
    ddraw_close,
    NULL,
    NULL,
    NULL,
    ddraw_screenshot,
    NULL
};
