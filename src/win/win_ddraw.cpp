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
 * Version:	@(#)win_ddraw.cpp	1.0.10	2018/05/13
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


static LPDIRECTDRAW		lpdd = NULL;
static LPDIRECTDRAW4		lpdd4 = NULL;
static LPDIRECTDRAWSURFACE4	dds_pri = NULL,
				dds_back = NULL,
				dds_back2 = NULL;
static LPDIRECTDRAWCLIPPER	lpdd_clipper = NULL;
static DDSURFACEDESC2		ddsd;
static HWND			ddraw_hwnd;
static HBITMAP			hbitmap;
static int			ddraw_w, ddraw_h,
				xs, ys, ys2;
#ifdef USE_LIBPNG
static png_structp		png_ptr;
static png_infop		png_info_ptr;
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
CopySurface(IDirectDrawSurface4 *pDDSurface)
{ 
    HDC hdc, hmemdc;
    HBITMAP hprevbitmap;
    DDSURFACEDESC2 ddsd2;

    pDDSurface->GetDC(&hdc);
    hmemdc = CreateCompatibleDC(hdc); 
    ZeroMemory(&ddsd2, sizeof(ddsd2));
    ddsd2.dwSize = sizeof(ddsd2);
    pDDSurface->GetSurfaceDesc(&ddsd2);
    hbitmap = CreateCompatibleBitmap(hdc, xs, ys);
    hprevbitmap = (HBITMAP)SelectObject(hmemdc, hbitmap);
    BitBlt(hmemdc, 0, 0, xs, ys, hdc, 0, 0, SRCCOPY);    
    SelectObject(hmemdc, hprevbitmap);
    DeleteDC(hmemdc);
    pDDSurface->ReleaseDC(hdc);
}


static void
DoubleLines(uint8_t *dst, uint8_t *src)
{
    int i = 0;

    for (i = 0; i < ys; i++) {
	memcpy(dst + (i * xs * 8), src + (i * xs * 4), xs * 4);
	memcpy(dst + ((i * xs * 8) + (xs * 4)), src + (i * xs * 4), xs * 4);
    }
}


#ifdef USE_LIBPNG
static void
bgra_to_rgb(png_bytep *b_rgb, uint8_t *bgra, int width, int height)
{
    uint8_t *r, *b;
    int i, j;

    for (i = 0; i < height; i++) {
	for (j = 0; j < width; j++) {
		r = &b_rgb[(height - 1) - i][j * 3];
		b = &bgra[((i * width) + j) * 4];
		r[0] = b[2];
		r[1] = b[1];
		r[2] = b[0];
	}
    }
}


static void
SavePNG(const wchar_t *fn, HBITMAP hBitmap)
{
    WCHAR temp[512];
    BITMAPINFO bmpInfo;
    HDC hdc;
    LPVOID pBuf = NULL;
    LPVOID pBuf2 = NULL;
    png_bytep *b_rgb = NULL;
    FILE *fp;
    int i;

    /* Create file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	pclog("[SavePNG] File %ls could not be opened for writing!\n", fn);
	_swprintf(temp, get_string(IDS_2088), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    /* Initialize PNG stuff. */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
	(void)fclose(fp);
	pclog("[SavePNG] png_create_write_struct failed!\n");
	_swprintf(temp, get_string(IDS_2088), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    png_info_ptr = png_create_info_struct(png_ptr);
    if (png_info_ptr == NULL) {
	(void)fclose(fp);
	pclog("[SavePNG] png_create_info_struct failed!\n");
	_swprintf(temp, get_string(IDS_2088), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    png_init_io(png_ptr, fp);

    hdc = GetDC(NULL);

    ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS);
    if (bmpInfo.bmiHeader.biSizeImage <= 0)
	bmpInfo.bmiHeader.biSizeImage =
		bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount+7)/8;

    if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
	(void)fclose(fp);
	pclog("[SavePNG] Unable to allocate bitmap memory!\n");
	_swprintf(temp, get_string(IDS_2088), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    if (ys2 <= 250) {
	bmpInfo.bmiHeader.biSizeImage <<= 1;

	if ((pBuf2 = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
		(void)fclose(fp);
		free(pBuf);
		pclog("[SavePNG] Unable to allocate secondary bitmap memory!\n");
		_swprintf(temp, get_string(IDS_2088), fn);
		ui_msgbox(MBX_ERROR, temp);
		return;
	}

	bmpInfo.bmiHeader.biHeight <<= 1;
    }

#if 0
    pclog("save png w=%i h=%i\n",
	bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);
#endif

    bmpInfo.bmiHeader.biCompression = BI_RGB;

    GetDIBits(hdc, hBitmap, 0,
	      bmpInfo.bmiHeader.biHeight, pBuf, &bmpInfo, DIB_RGB_COLORS);

    png_set_IHDR(png_ptr, png_info_ptr,
		 bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight,
		 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    b_rgb = (png_bytep *)malloc(sizeof(png_bytep)*bmpInfo.bmiHeader.biHeight);
    if (b_rgb == NULL) {
	(void)fclose(fp);
	free(pBuf);
	free(pBuf2);
	pclog("[SavePNG] Unable to allocate RGB bitmap memory!\n");
	_swprintf(temp, get_string(IDS_2088), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    for (i = 0; i < bmpInfo.bmiHeader.biHeight; i++)
	b_rgb[i] = (png_byte *)malloc(png_get_rowbytes(png_ptr, png_info_ptr));

    if (pBuf2) {
	DoubleLines((uint8_t *)pBuf2, (uint8_t *)pBuf);
	bgra_to_rgb(b_rgb, (uint8_t *)pBuf2,
		    bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);
    } else
	bgra_to_rgb(b_rgb, (uint8_t *)pBuf,
		    bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);

    png_write_info(png_ptr, png_info_ptr);

    png_write_image(png_ptr, b_rgb);

    png_write_end(png_ptr, NULL);

    /* Clean up. */
    if (hdc) ReleaseDC(NULL,hdc); 

    if (b_rgb) {
	for (i = 0; i < bmpInfo.bmiHeader.biHeight; i++)
		free(b_rgb[i]);
	free(b_rgb);
    }

    if (pBuf) free(pBuf); 
    if (pBuf2) free(pBuf2); 

    if (fp != NULL) fclose(fp);
}
#else
static void
SaveBMP(const wchar_t *fn, HBITMAP hBitmap)
{
    WCHAR temp[512];
    BITMAPFILEHEADER bmpFileHeader; 
    BITMAPINFO bmpInfo;
    HDC hdc;
    FILE *fp = NULL;
    LPVOID pBuf = NULL;
    LPVOID pBuf2 = NULL;

    do { 
	hdc = GetDC(NULL);

	ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
	bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS); 

	if (bmpInfo.bmiHeader.biSizeImage <= 0)
		bmpInfo.bmiHeader.biSizeImage =
			bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount+7)/8;

	if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
//		pclog("ERROR: Unable to Allocate Bitmap Memory");
		break;
	}

	if (ys2 <= 250)
		pBuf2 = malloc(bmpInfo.bmiHeader.biSizeImage * 2);

	bmpInfo.bmiHeader.biCompression = BI_RGB;

	GetDIBits(hdc, hBitmap, 0, bmpInfo.bmiHeader.biHeight, pBuf, &bmpInfo, DIB_RGB_COLORS);

	if ((fp = _wfopen(fn, L"wb")) == NULL) {
		pclog("[SaveBMP] File %ls could not be opened for writing!\n", fn);
		_swprintf(temp, get_string(IDS_2088), fn);
		ui_msgbox(MBX_ERROR, temp);
		break;
	} 

	bmpFileHeader.bfReserved1 = 0;
	bmpFileHeader.bfReserved2 = 0;
	if (pBuf2) {
		bmpInfo.bmiHeader.biSizeImage <<= 1;
		bmpInfo.bmiHeader.biHeight <<= 1;
	}
	bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) +
			       sizeof(BITMAPINFOHEADER) +
			       bmpInfo.bmiHeader.biSizeImage;
	bmpFileHeader.bfType = 0x4D42;
	bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) +
				  sizeof(BITMAPINFOHEADER); 

	(void)fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, fp);
	(void)fwrite(&bmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
	if (pBuf2) {
		DoubleLines((uint8_t *)pBuf2, (uint8_t *)pBuf);
		(void)fwrite(pBuf2, bmpInfo.bmiHeader.biSizeImage, 1, fp); 
	} else {
		(void)fwrite(pBuf, bmpInfo.bmiHeader.biSizeImage, 1, fp); 
	}
    } while(false); 

    if (hdc) ReleaseDC(NULL, hdc); 

    if (pBuf2) free(pBuf2); 

    if (pBuf) free(pBuf); 

    if (fp) fclose(fp);
}
#endif


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

    pclog("vid_fullscreen_scale = %i\n", vid_fullscreen_scale);

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
    RECT r_src, r_dest;
    RECT w_rect;
    DDBLTFX ddbltfx;
    HRESULT hr;
    int yy;

    if (dds_back == NULL) {
	video_blit_complete();
	return; /*Nothing to do*/
    }

    if ((y1 == y2) || (h <= 0)) {
	video_blit_complete();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = dds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	dds_back->Restore();
	dds_back->Lock(NULL, &ddsd,
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
    dds_back->Unlock(NULL);

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

    dds_back2->Blt(&w_rect, NULL, NULL,
		     DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);

    hr = dds_back2->Blt(&r_dest, dds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	dds_back2->Restore();
	dds_back2->Blt(&r_dest, dds_back, &r_src, DDBLT_WAIT, NULL);
    }
	
    hr = dds_pri->Flip(NULL, DDFLIP_NOVSYNC);	
    if (hr == DDERR_SURFACELOST) {
	dds_pri->Restore();
	dds_pri->Flip(NULL, DDFLIP_NOVSYNC);
    }
}


static void
ddraw_blit(int x, int y, int y1, int y2, int w, int h)
{
    RECT r_src, r_dest;
    POINT po;
    HRESULT hr;
    int yy;

    if (dds_back == NULL) {
	video_blit_complete();
	return; /*Nothing to do*/
    }

    if ((y1 == y2) || (h <= 0)) {
	video_blit_complete();
	return;
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    hr = dds_back->Lock(NULL, &ddsd,
			  DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	dds_back->Restore();
	dds_back->Lock(NULL, &ddsd,
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
    dds_back->Unlock(NULL);

    po.x = po.y = 0;
	
    ClientToScreen(ddraw_hwnd, &po);
    GetClientRect(ddraw_hwnd, &r_dest);
    OffsetRect(&r_dest, po.x, po.y);	
	
    r_src.left   = 0;
    r_src.top    = 0;       
    r_src.right  = w;
    r_src.bottom = h;

    hr = dds_back2->Blt(&r_src, dds_back, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	dds_back2->Restore();
	dds_back2->Blt(&r_src, dds_back, &r_src, DDBLT_WAIT, NULL);
    }

    dds_back2->Unlock(NULL);
	
    hr = dds_pri->Blt(&r_dest, dds_back2, &r_src, DDBLT_WAIT, NULL);
    if (hr == DDERR_SURFACELOST) {
	dds_pri->Restore();
	dds_pri->Blt(&r_dest, dds_back2, &r_src, DDBLT_WAIT, NULL);
    }
}


static void
ddraw_close(void)
{
    pclog("DDRAW: close (fs=%d)\n", (dds_back2 != NULL)?1:0);

    video_setblit(NULL);

    if (dds_back2 != NULL) {
	dds_back2->Release();
	dds_back2 = NULL;
    }
    if (dds_back != NULL) {
	dds_back->Release();
	dds_back = NULL;
    }
    if (dds_pri != NULL) {
	dds_pri->Release();
	dds_pri = NULL;
    }
    if (lpdd_clipper != NULL) {
	lpdd_clipper->Release();
	lpdd_clipper = NULL;
    }
    if (lpdd4 != NULL) {
	lpdd4->Release();
	lpdd4 = NULL;
    }
}


static int
ddraw_init(int fs)
{
    HRESULT hr;
    HWND h;
    DWORD dw;

    pclog("DDRAW: init (fs=%d)\n", fs);

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
    lpdd = NULL;

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
    hr = lpdd4->CreateSurface(&ddsd, &dds_pri, NULL);
    if (FAILED(hr)) {
	pclog("DDRAW: CreateSurface failed (%s)\n", GetError(hr));
	return(0);
    }

    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth  = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    hr = lpdd4->CreateSurface(&ddsd, &dds_back, NULL);
    if (FAILED(hr)) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth  = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	hr = lpdd4->CreateSurface(&ddsd, &dds_back, NULL);
	if (FAILED(hr)) {
		pclog("DDRAW: CreateSurface back failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    if (fs) {
	ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = dds_pri->GetAttachedSurface(&ddsd.ddsCaps, &dds_back2);
	if (FAILED(hr)) {
		pclog("DDRAW: GetAttachedSurface failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    memset(&ddsd, 0x00, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth  = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    if (fs)
	hr = lpdd4->CreateSurface(&ddsd, &dds_back, NULL);
      else
	hr = lpdd4->CreateSurface(&ddsd, &dds_back2, NULL);
    if (FAILED(hr)) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth  = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	if (fs)
		hr = lpdd4->CreateSurface(&ddsd, &dds_back, NULL);
	  else
		hr = lpdd4->CreateSurface(&ddsd, &dds_back2, NULL);
	if (FAILED(hr)) {
		pclog("DDRAW: CreateSurface(back) failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    if (! fs) {
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

	hr = dds_pri->SetClipper(lpdd_clipper);
	if (FAILED(hr)) {
		pclog("DDRAW: SetClipper failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    ddraw_hwnd = hwndRender;

    if (fs)
	video_setblit(ddraw_blit_fs);
      else
	video_setblit(ddraw_blit);

    return(1);
}


static void
ddraw_screenshot(const wchar_t *fn)
{
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

    CopySurface(dds_back2);

#ifdef USE_LIBPNG
    SavePNG(fn, hbitmap);
#else
    SaveBMP(fn, hbitmap);
#endif
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
