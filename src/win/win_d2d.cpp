/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Rendering module for Microsoft Direct2D.
 *
 * Version:	@(#)win_d2d.cpp	1.0.6	2019/04/29
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		David Hrdlicka, <hrdlickadavid@outlook.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
 *		Copyright 2018 David Hrdlicka.
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../emu.h"
#include "../version.h"
#include "../device.h"
#include "../plat.h"
#ifdef USE_LIBPNG
# include "../png.h"
#endif
#ifdef _MSC_VER
# pragma warning(disable: 4200)
#endif
#include "../devices/video/video.h"
#include "win.h"
#include "win_d2d.h"


#define PATH_D2D_DLL	"d2d1.dll"


#if USE_D2D == 2
# define DLLFUNC(x)	D2D1_##x


/* Pointers to the real functions. */
typedef HRESULT (WINAPI *MyCreateFactory_t)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**);
static MyCreateFactory_t D2D1_CreateFactory;

static const dllimp_t d2d_imports[] = {
  { "D2D1CreateFactory",	&D2D1_CreateFactory		},
  { NULL,			NULL				}
};

static void			*d2d_handle = NULL;
#else
# define DLLFUNC(x)	D2D1##x
#endif


static HWND			d2d_hwnd, old_hwndMain;
static ID2D1Factory		*d2d_factory;
static ID2D1HwndRenderTarget	*d2d_hwndRT;
static ID2D1BitmapRenderTarget	*d2d_btmpRT;
static ID2D1Bitmap		*d2d_bitmap;
static int			d2d_width, d2d_height,
				d2d_screen_width, d2d_screen_height,
				d2d_fs;
static int			d2d_enabled;


static void
d2d_stretch(float *w, float *h, float *x, float *y)
{
    double dw, dh, dx, dy, temp, temp2, ratio_w, ratio_h, gsr, hsr;

    switch (vid_fullscreen_scale) {
	case FULLSCR_SCALE_FULL:
		*w = (float)d2d_screen_width;
		*h = (float)d2d_screen_height;
		*x = 0;
		*y = 0;
		break;

	case FULLSCR_SCALE_43:
		dw = (double)d2d_screen_width;
		dh = (double)d2d_screen_height;
		temp = (dh / 3.0) * 4.0;
		dx = (dw - temp) / 2.0;
		dw = temp;
		*w = (float)dw;
		*h = (float)dh;
		*x = (float)dx;
		*y = 0;
		break;

	case FULLSCR_SCALE_SQ:
		dw = (double)d2d_screen_width;
		dh = (double)d2d_screen_height;
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
		*w = (float)dw;
		*h = (float)dh;
		*x = (float)dx;
		*y = (float)dy;
		break;

	case FULLSCR_SCALE_INT:
		dw = (double)d2d_screen_width;
		dh = (double)d2d_screen_height;
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
		*w = (float)dw;
		*h = (float)dh;
		*x = (float)dx;
		*y = (float)dy;
		break;

	case FULLSCR_SCALE_KEEPRATIO:
		dw = (double)d2d_screen_width;
		dh = (double)d2d_screen_height;
		hsr = dw / dh;
		gsr = ((double) *w) / ((double) *h);
		if (gsr <= hsr) {
			temp = dh * gsr;
			dx = (dw - temp) / 2.0;
			dw = temp;
			*w = (float)dw;
			*h = (float)dh;
			*x = (float)dx;
			*y = 0;
		} else {
			temp = dw / gsr;
			dy = (dh - temp) / 2.0;
			dh = temp;
			*w = (float)dw;
			*h = (float)dh;
			*x = 0;
			*y = (float)dy;
		}
		break;
    }
}


static void
d2d_blit(bitmap_t *scr, int x, int y, int y1, int y2, int w, int h)
{
    ID2D1Bitmap *fs_bitmap;
    ID2D1RenderTarget *RT;
    D2D1_RECT_U rectU;
    HRESULT hr = S_OK;
    void *srcdata;
    int yy;	
    float fs_x = 0, fs_y = 0;
    float fs_w = (float)w;
    float fs_h = (float)h;

    DEBUG("D2D: blit(x=%d, y=%d, y1=%d, y2=%d, w=%d, h=%d)\n", x,y, y1,y2, w,h);

    if (! d2d_enabled) {
	video_blit_done();
	return;
    }

    // TODO: Detect double scanned mode and resize render target
    // appropriately for more clear picture
    if (w != d2d_width || h != d2d_height) {
	if (d2d_fs) {
		if (d2d_btmpRT) {
			d2d_btmpRT->Release();
			d2d_btmpRT = NULL;
		}
		hr = d2d_hwndRT->CreateCompatibleRenderTarget(
				D2D1::SizeF((float)w, (float)h),
				&d2d_btmpRT);
	} else {
		hr = d2d_hwndRT->Resize(D2D1::SizeU(w, h));
	}

	if (SUCCEEDED(hr)) {
		d2d_width = w;
		d2d_height = h;
	}
    }

    if ((y1 == y2) || (scr == NULL)) {
	video_blit_done();
	return;
    }

    // TODO: Copy data directly from screen to d2d_bitmap
    srcdata = mem_alloc(h * w * 4);
    for (yy = y1; yy < y2; yy++) {
	if ((y + yy) >= 0 && (y + yy) < scr->h) {
		if (vid_grayscale || invert_display)
			video_transform_copy(
				(uint32_t *)&(((uint8_t *)srcdata)[yy * w * 4]),
				&scr->line[y + yy][x], w);
		else
			memcpy(
				(uint32_t *)&(((uint8_t *)srcdata)[yy * w * 4]),
				&scr->line[y + yy][x], w * 4);
	}
    }
    video_blit_done();

    rectU = D2D1::RectU(0, 0, w, h);
    hr = d2d_bitmap->CopyFromMemory(&rectU, srcdata, w * 4);

    /*
     * In fullscreen mode we first draw offscreen to an intermediate
     * BitmapRenderTarget, which then gets rendered to the actual
     * HwndRenderTarget in order to implement different scaling modes.
     *
     * In windowed mode we draw directly to the HwndRenderTarget.
     */
    if (SUCCEEDED(hr)) {
	RT = d2d_fs ? (ID2D1RenderTarget *)d2d_btmpRT
		    : (ID2D1RenderTarget *)d2d_hwndRT;
	RT->BeginDraw();
	RT->DrawBitmap(d2d_bitmap,
		       D2D1::RectF(0, (float)y1, (float)w, (float)y2),
		       1.0f,
		       D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
		       D2D1::RectF(0, (float)y1, (float)w, (float)y2));
	hr = RT->EndDraw();
    }

    if (d2d_fs) {
	if (SUCCEEDED(hr))
		hr = d2d_btmpRT->GetBitmap(&fs_bitmap);

	if (SUCCEEDED(hr)) {
		d2d_stretch(&fs_w, &fs_h, &fs_x, &fs_y);

		d2d_hwndRT->BeginDraw();

		d2d_hwndRT->Clear(D2D1::ColorF(D2D1::ColorF::Black));

		d2d_hwndRT->DrawBitmap(fs_bitmap,
				       D2D1::RectF(fs_x, fs_y, fs_x + fs_w, fs_y + fs_h),
				       1.0f,
				       D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				       D2D1::RectF(0, 0, (float)w, (float)h));

		hr = d2d_hwndRT->EndDraw();
	}
    }

    if (FAILED(hr))
	ERRLOG("D2D: blit: error 0x%08lx\n", hr);

    /* Clean up. */
    free(srcdata);
}


static void
d2d_close(void)
{
    DEBUG("D2D: close()\n");

    video_blit_set(NULL);

    if (d2d_bitmap) {
	d2d_bitmap->Release();
	d2d_bitmap = NULL;
    }
    if (d2d_btmpRT) {
	d2d_btmpRT->Release();
	d2d_btmpRT = NULL;
    }
	
    if (d2d_hwndRT) {
	d2d_hwndRT->Release();
	d2d_hwndRT = NULL;
    }

    if (d2d_factory) {
	d2d_factory->Release();
	d2d_factory = NULL;
    }

    if (d2d_hwnd) {
	hwndMain = old_hwndMain;
	plat_set_input(hwndMain);
	DestroyWindow(d2d_hwnd);
	d2d_hwnd = NULL;
	old_hwndMain = NULL;
    }

#if USE_D2D == 2
    /* Quit and unload the DLL if possible. */
    if (d2d_handle != NULL) {
	dynld_close(d2d_handle);
	d2d_handle = NULL;
    }
#endif

    d2d_enabled = 0;
}


static int
d2d_init(int fs)
{
    WCHAR title[200];
    D2D1_HWND_RENDER_TARGET_PROPERTIES props;
    D2D1_FACTORY_OPTIONS options;
    HRESULT hr;

    INFO("D2D: init(fs=%d)\n", fs);

#if USE_D2D == 2
    /* Try loading the DLL. */
    if (d2d_handle == NULL)
	d2d_handle = dynld_module(PATH_D2D_DLL, d2d_imports);
    if (d2d_handle == NULL) {
	ERRLOG("D2D: unable to load '%s', D2D not available.\n", PATH_D2D_DLL);
	return(0);
    } else
	INFO("D2D: module '%s' loaded.\n", PATH_D2D_DLL);
#endif

    ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));
    if (FAILED(DLLFUNC(CreateFactory)(D2D1_FACTORY_TYPE_MULTI_THREADED,
				      __uuidof(ID2D1Factory),
				      &options,
			        reinterpret_cast <void **>(&d2d_factory)))) {
	ERRLOG("D2D: unable to load factory, D2D not available.\n");
	d2d_close();
	return(0);
    }

    if (fs) {
	/*
	 * Direct2D seems to lack any proper fullscreen mode,
	 * therefore we just create a full screen window and
	 * pass its handle to a HwndRenderTarget.
 	 */
	d2d_screen_width = GetSystemMetrics(SM_CXSCREEN);
	d2d_screen_height = GetSystemMetrics(SM_CYSCREEN);

	mbstowcs(title, emu_version, sizeof_w(title));
	d2d_hwnd = CreateWindow(FS_CLASS_NAME,
				title,
				WS_POPUP,
				0, 0, d2d_screen_width, d2d_screen_height,
				HWND_DESKTOP,
				NULL,
				hInstance,
				NULL);

	old_hwndMain = hwndMain;
	hwndMain = d2d_hwnd;

	plat_set_input(d2d_hwnd);
	SetFocus(d2d_hwnd);
	SetWindowPos(d2d_hwnd, HWND_TOPMOST,
		     0,0, d2d_screen_width,d2d_screen_height, SWP_SHOWWINDOW);	

	props = D2D1::HwndRenderTargetProperties(d2d_hwnd,
		D2D1::SizeU(d2d_screen_width, d2d_screen_height));
    } else {
	// HwndRenderTarget will get resized appropriately by d2d_blit,
	// so it's fine to let D2D imply size of 0x0 for now
	props = D2D1::HwndRenderTargetProperties(hwndRender);
    }

    hr = d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
					     props, &d2d_hwndRT);
    if (FAILED(hr)) {
	ERRLOG("D2D: unable to create target: error 0x%08lx\n", hr);
	d2d_close();
	return(0);
    }

    // Create a bitmap for storing intermediate data
    hr = d2d_hwndRT->CreateBitmap(D2D1::SizeU(2048, 2048),
				  D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),	
				  &d2d_bitmap);
    if (FAILED(hr)) {
	ERRLOG("D2D: unable to create bitmap: error 0x%08lx\n", hr);
	d2d_close();
	return(0);
    }	

    d2d_fs = fs;
    d2d_width = 0;
    d2d_height = 0;

    /* Make sure we get a clean exit. */
    atexit(d2d_close);

    /* Register our renderer! */
    video_blit_set(d2d_blit);

    d2d_enabled = 1;

    return(1);
}


static void
d2d_screenshot(const wchar_t *fn)
{
    // Saving a screenshot of a Direct2D render target is harder than
    // one would think. Keeping this stubbed for the moment
    //	-ryu
    INFO("D2D: screenshot(%ls)\n", fn);
}


/* See if this module is available or not. */
static int
d2d_available(void)
{
    void *handle;

    handle = dynld_module(PATH_D2D_DLL, NULL);
    if (handle != NULL) {
        dynld_close(handle);
        return(1);
    }

    return(0);
}


static void
d2d_enable(int yes)
{
    d2d_enabled = yes;
}


const vidapi_t d2d_vidapi = {
    "d2d",
    "Direct 2D",
    1,
    d2d_init, d2d_close, NULL,
    NULL,
    NULL,
    d2d_enable,
    d2d_screenshot,
    d2d_available
};
