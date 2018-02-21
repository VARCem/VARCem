/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Simple PNG image file format handler.
 *
 * Version:	@(#)win_png.h	1.0.1	2018/02/14
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
#ifndef WIN_PNG_H
# define WIN_PNG_H


/* PNG defintions, as per the specification. */
#define PNG_COLOR_TYPE		0x02		/* 3-sample sRGB */
#define PNG_COMPRESSION_TYPE	0x00		/* deflate compression */
#define PNG_FILTER_TYPE		0x00		/* no filtering */
#define PNG_INTERLACE_MODE	0x00		/* no interlacing */

/* DEFLATE definition, as per RFC1950/1 specification. */
#define DEFL_MAX_BLKSZ		65535		/* DEFLATE max block size */


typedef struct {
    wchar_t	*name;		/* name of datafile */
    FILE	*fp;

    uint16_t	width,		/* configured with in pixels */
		height;		/* configured with in pixels */
    uint8_t	bpp,		/* configured bits per pixel */
		ctype;		/* configured color type */

    uint16_t	col,		/* current column */
		line,		/* current scanline */
		lwidth;		/* line width in bytes */
    uint8_t	cdepth,		/* color depth in bits */
		pwidth;		/* bytes per pixel */
    uint32_t	crc;		/* idat chunk crc */
    uint32_t	dcrc;		/* deflate data crc */

    uint32_t	bufcnt;		/* #bytes in block */
} png_t;


#ifdef __cplusplus
extern "C" {
#endif

extern void	png_putlong(uint8_t *ptr, uint32_t val, int *off);
extern void	png_close(png_t *png);
extern png_t	*png_create(wchar_t *fn, int width, int height, int bpp);
extern int	png_write(png_t *png, uint8_t *bitmap, uint32_t pixels);

#ifdef __cplusplus
}
#endif


#endif	/*WIN_PNG_H*/
