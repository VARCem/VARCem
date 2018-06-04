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
 * NOTES:	This code should be re-merged into a single init() with a
 *		'fullscreen' argument, indicating FS mode is requested.
 *
 *		If configured with USE_LIBPNG, we try to load the external
 *		PNG library and use that if found. Otherwise, we fall back
 *		the original mode, which uses the Windows/DDraw built-in BMP
 *		format.
 *
 * Version:	@(#)win_ddraw.cpp	1.0.13	2018/05/24
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


static HBITMAP
CopySurface(IDirectDrawSurface4 *pDDSurface)
{ 
    HBITMAP hbmp, hbmprev;
    DDSURFACEDESC2 ddsd;
    HDC hdc, hmemdc;

    pDDSurface->GetDC(&hdc);
    hmemdc = CreateCompatibleDC(hdc); 
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    pDDSurface->GetSurfaceDesc(&ddsd);
    hbmp = CreateCompatibleBitmap(hdc, xs, ys);
    hbmprev = (HBITMAP)SelectObject(hmemdc, hbmp);
    BitBlt(hmemdc, 0, 0, xs, ys, hdc, 0, 0, SRCCOPY);    
    SelectObject(hmemdc, hbmprev); 
    DeleteDC(hmemdc);
    pDDSurface->ReleaseDC(hdc);

    return(hbmp);
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
    int i, j;
    uint8_t *r, *b;

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


/* Not strictly needed, but hey.. */
static void
png_error_handler(UNUSED(png_structp arg), const char *str)
{
    pclog("DDraw: PNG error '%s'\n", str);
}


/* Not strictly needed, but hey.. */
static void
png_warning_handler(UNUSED(png_structp arg), const char *str)
{
    pclog("DDraw: PNG warning '%s'\n", str);
}


static void
SavePNG(const wchar_t *fn, HBITMAP hBitmap)
{
    wchar_t temp[512];
    BITMAPINFO bmpInfo;
    HDC hdc;
    LPVOID pBuf = NULL;
    LPVOID pBuf2 = NULL;
    png_bytep *b_rgb = NULL;
    png_infop info_ptr;
    png_structp png_ptr;
    FILE *fp;
    int i;

    /* Create file. */
    fp = plat_fopen(fn, L"wb");
    if (fp == NULL) {
	pclog("[SavePNG] File %ls could not be opened for writing!\n", fn);
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    /* Initialize PNG stuff. */
    png_ptr = PNGFUNC(create_write_struct)(PNG_LIBPNG_VER_STRING, NULL,
				      png_error_handler, png_warning_handler);
    if (png_ptr == NULL) {
	(void)fclose(fp);
	pclog("[SavePNG] png_create_write_struct failed!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    info_ptr = PNGFUNC(create_info_struct)(png_ptr);
    if (info_ptr == NULL) {
//	PNGFUNC(destroy_write_struct)(&png_ptr, NULL, NULL);
	(void)fclose(fp);
	pclog("[SavePNG] png_create_info_struct failed!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    PNGFUNC(init_io)(png_ptr, fp);

    hdc = GetDC(NULL);

    ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS);
    if (bmpInfo.bmiHeader.biSizeImage <= 0)
	bmpInfo.bmiHeader.biSizeImage =
		bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount+7)/8;

    if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
//	PNGFUNC(destroy_write_struct)(&png_ptr, &info_ptr, NULL);
	(void)fclose(fp);
	pclog("[SavePNG] Unable to allocate bitmap memory!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    if (ys2 <= 250) {
	bmpInfo.bmiHeader.biSizeImage <<= 1;

	if ((pBuf2 = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
//		PNGFUNC(destroy_write_struct)(&png_ptr, &info_ptr, NULL);
		(void)fclose(fp);
		free(pBuf);
		pclog("[SavePNG] Unable to allocate secondary bitmap memory!\n");
		_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
		ui_msgbox(MBX_ERROR, temp);
		return;
	}

	bmpInfo.bmiHeader.biHeight <<= 1;
    }
#if 1
    pclog("save png w=%i h=%i\n",
	bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);
#endif

    bmpInfo.bmiHeader.biCompression = BI_RGB;

    GetDIBits(hdc, hBitmap, 0,
	      bmpInfo.bmiHeader.biHeight, pBuf, &bmpInfo, DIB_RGB_COLORS);

    PNGFUNC(set_IHDR)(png_ptr, info_ptr,
		      bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight,
		      8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    b_rgb = (png_bytep *)malloc(sizeof(png_bytep)*bmpInfo.bmiHeader.biHeight);
    if (b_rgb == NULL) {
//	PNGFUNC(destroy_write_struct)(&png_ptr, NULL, NULL);
	(void)fclose(fp);
	free(pBuf);
	free(pBuf2);
	pclog("[SavePNG] Unable to allocate RGB bitmap memory!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    for (i = 0; i < bmpInfo.bmiHeader.biHeight; i++)
	b_rgb[i] = (png_byte *)malloc(PNGFUNC(get_rowbytes)(png_ptr, info_ptr));

    if (pBuf2) {
	DoubleLines((uint8_t *)pBuf2, (uint8_t *)pBuf);
	bgra_to_rgb(b_rgb, (uint8_t *)pBuf2,
		    bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);
    } else
	bgra_to_rgb(b_rgb, (uint8_t *)pBuf,
		    bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);

    PNGFUNC(write_info)(png_ptr, info_ptr);

    PNGFUNC(write_image)(png_ptr, b_rgb);

    PNGFUNC(write_end)(png_ptr, NULL);

    /* Clean up. */
    if (fp != NULL) fclose(fp);
//  PNGFUNC(destroy_write_struct)(&png_ptr, &info_ptr, NULL);
    if (hdc) ReleaseDC(NULL, hdc); 

    if (b_rgb != NULL) {
	for (i = 0; i < bmpInfo.bmiHeader.biHeight; i++)
		free(b_rgb[i]);
	free(b_rgb);
    }

    if (pBuf != NULL) free(pBuf); 
    if (pBuf2 != NULL) free(pBuf2); 
}
#endif


static void
SaveBMP(const wchar_t *fn, HBITMAP hBitmap)
{
    wchar_t temp[512];
    BITMAPFILEHEADER bmpFileHeader; 
    BITMAPINFO bmpInfo;
    HDC hdc;
    FILE *fp = NULL;
    LPVOID pBuf = NULL;
    LPVOID pBuf2 = NULL;

    hdc = GetDC(NULL);

    ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS); 

    if (bmpInfo.bmiHeader.biSizeImage <= 0)
	bmpInfo.bmiHeader.biSizeImage =
		bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount+7)/8;

    if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL) {
	pclog("[SaveBMP] Unable to allocate bitmap memory!\n");
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    }

    if (ys2 <= 250)
	pBuf2 = malloc(bmpInfo.bmiHeader.biSizeImage * 2);

    bmpInfo.bmiHeader.biCompression = BI_RGB;

    GetDIBits(hdc, hBitmap, 0, bmpInfo.bmiHeader.biHeight,
	      pBuf, &bmpInfo, DIB_RGB_COLORS);

    if ((fp = plat_fopen(fn, L"wb")) == NULL) {
	pclog("[SaveBMP] File %ls could not be opened for writing!\n", fn);
	_swprintf(temp, get_string(IDS_ERR_SCRSHOT), fn);
	ui_msgbox(MBX_ERROR, temp);
	return;
    } 

    bmpFileHeader.bfReserved1 = 0;
    bmpFileHeader.bfReserved2 = 0;
    if (pBuf2) {
	bmpInfo.bmiHeader.biSizeImage <<= 1;
	bmpInfo.bmiHeader.biHeight <<= 1;
    }
    bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
    bmpFileHeader.bfType=0x4D42;
    bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER); 

    (void)fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fp);
    (void)fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fp);
    if (pBuf2) {
	DoubleLines((uint8_t *) pBuf2, (uint8_t *) pBuf);
	(void)fwrite(pBuf2,bmpInfo.bmiHeader.biSizeImage,1,fp); 
    } else {
	(void)fwrite(pBuf,bmpInfo.bmiHeader.biSizeImage,1,fp); 
    }

    /* Clean up. */
    if (fp != NULL) fclose(fp);
    if (hdc != NULL) ReleaseDC(NULL, hdc); 
    if (pBuf2 != NULL) free(pBuf2); 
    if (pBuf != NULL) free(pBuf); 
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
    pclog("DDRAW: close (fs=%d)\n", (lpdds_back2 != NULL)?1:0);

    video_setblit(NULL);

    if (lpdds_back2) {
	lpdds_back2->Release();
	lpdds_back2 = NULL;
    }
    if (lpdds_back) {
	lpdds_back->Release();
	lpdds_back = NULL;
    }
    if (lpdds_pri) {
	lpdds_pri->Release();
	lpdds_pri = NULL;
    }
    if (lpdd_clipper) {
	lpdd_clipper->Release();
	lpdd_clipper = NULL;
    }
    if (lpdd4) {
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


#if 0
int
ddraw_init_fs(HWND h)
{
    ddraw_w = GetSystemMetrics(SM_CXSCREEN);
    ddraw_h = GetSystemMetrics(SM_CYSCREEN);

    cgapal_rebuild();

    if (FAILED(DirectDrawCreate(NULL, &lpdd, NULL))) return 0;

    if (FAILED(lpdd->QueryInterface(IID_IDirectDraw4, (LPVOID *)&lpdd4))) return 0;

    lpdd->Release();
    lpdd = NULL;

    atexit(ddraw_close);

    if (FAILED(lpdd4->SetCooperativeLevel(h,
					  DDSCL_SETFOCUSWINDOW | \
					  DDSCL_CREATEDEVICEWINDOW | \
					  DDSCL_EXCLUSIVE | \
					  DDSCL_FULLSCREEN | \
					  DDSCL_ALLOWREBOOT))) return 0;

    if (FAILED(lpdd4->SetDisplayMode(ddraw_w, ddraw_h, 32, 0 ,0))) return 0;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.dwBackBufferCount = 1;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
    if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_pri, NULL))) return 0;

    ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
    if (FAILED(lpdds_pri->GetAttachedSurface(&ddsd.ddsCaps, &lpdds_back2))) return 0;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth  = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL))) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth  = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL))) return 0;
    }

    ddraw_hwnd = h;

    video_setblit(ddraw_blit_fs);

    return(1);
}
#endif


static int
ddraw_init(int fs)
{
    DDSURFACEDESC2 ddsd;
    LPDIRECTDRAW lpdd;
    HWND h = hwndRender;

    pclog("DDraw: initializing (fs=%d)\n", fs);

    cgapal_rebuild();

    if (FAILED(DirectDrawCreate(NULL, &lpdd, NULL))) return(0);

    if (FAILED(lpdd->QueryInterface(IID_IDirectDraw4, (LPVOID *)&lpdd4)))
					return(0);
    lpdd->Release();

    atexit(ddraw_close);

    if (FAILED(lpdd4->SetCooperativeLevel(h, DDSCL_NORMAL))) return(0);

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_pri, NULL))) return(0);

    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth  = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL))) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth  = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL)))
				fatal("CreateSurface back failed\n");
    }

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth  = 2048;
    ddsd.dwHeight = 2048;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL))) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth  = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	if (FAILED(lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL)))
				fatal("CreateSurface back failed\n");
    }

    if (FAILED(lpdd4->CreateClipper(0, &lpdd_clipper, NULL))) return(0);

    if (FAILED(lpdd_clipper->SetHWnd(0, h))) return(0);

    if (FAILED(lpdds_pri->SetClipper(lpdd_clipper))) return(0);

    ddraw_hwnd = h;

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


static void
ddraw_screenshot(const wchar_t *fn)
{
    wchar_t temp[512];
    HBITMAP hbmp;

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

    hbmp = CopySurface(lpdds_back2);

#ifdef USE_LIBPNG
    /* Save the screenshot, using PNG if available. */
    if (png_handle != NULL) {
	/* Use the PNG library. */
	SavePNG(fn, hbmp);
    } else {
#endif
	/* Use BMP, so fix the file name. */
	wcscpy(temp, fn);
	temp[wcslen(temp)-3] = L'b';
	temp[wcslen(temp)-2] = L'm';
	temp[wcslen(temp)-1] = L'p';
	SaveBMP(temp, hbmp);
#ifdef USE_LIBPNG
    }
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


#if 0
@@@@@
static int
ddraw_init(int fs)
{
    DDSURFACEDESC2 ddsd;
    LPDIRECTDRAW lpdd;
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
    hr = lpdd4->CreateSurface(&ddsd, &lpdds_pri, NULL);
    if (FAILED(hr)) {
	pclog("DDRAW: CreateSurface failed (%s)\n", GetError(hr));
	return(0);
    }

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
		pclog("DDRAW: CreateSurface back failed (%s)\n", GetError(hr));
		return(0);
	}
    }

    if (fs) {
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
    if (fs)
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
      else
	hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
    if (FAILED(hr)) {
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = 2048;
	ddsd.dwHeight = 2048;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	if (fs)
		hr = lpdd4->CreateSurface(&ddsd, &lpdds_back, NULL);
	  else
		hr = lpdd4->CreateSurface(&ddsd, &lpdds_back2, NULL);
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

    return(1);
}
#endif
