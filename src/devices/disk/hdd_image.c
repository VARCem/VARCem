/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handling of hard disk image files.
 *
 * FIXME:	The hdd_image_t structure should be merged with the main
 *		hard_disk_t structure, with an extra field for private
 *		data for an image handler. This entire module should be
 *		merged with hdd.c, since that is the scope of hdd.c. The
 *		actual format handlers can then be in hdd_format.c etc.
 *
 * Version:	@(#)hdd_image.c	1.0.13	2020/11/25
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include <errno.h>
#include <time.h>
#define HAVE_STDARG_H
#define dbglog hdd_image_log
#include "../../emu.h"
#include "../../plat.h"
#include "../../random.h"
#include "hdd.h"
#include "../../minivhd/minivhd.h"
#include "../../minivhd/minivhd_internal.h"
#include <stdbool.h>


#define VHD_OFFSET_COOKIE 0
#define VHD_OFFSET_FEATURES 8 
#define VHD_OFFSET_VERSION 12
#define VHD_OFFSET_DATA_OFFSET 16
#define VHD_OFFSET_TIMESTAMP 24
#define VHD_OFFSET_CREATOR 28
#define VHD_OFFSET_CREATOR_VERS 32
#define VHD_OFFSET_CREATOR_HOST 36
#define VHD_OFFSET_ORIG_SIZE 40
#define VHD_OFFSET_CURR_SIZE 48
#define VHD_OFFSET_GEOM_CYL 56
#define VHD_OFFSET_GEOM_HEAD 58
#define VHD_OFFSET_GEOM_SPT 59
#define VHD_OFFSET_TYPE 60
#define VHD_OFFSET_CHECKSUM 64
#define VHD_OFFSET_UUID 68
#define VHD_OFFSET_SAVED_STATE 84
#define VHD_OFFSET_RESERVED 85

#define HDD_IMAGE_RAW 0
#define HDD_IMAGE_HDI 1
#define HDD_IMAGE_HDX 2
#define HDD_IMAGE_VHD 3


typedef struct {
    FILE	*file;
    uint32_t	base;
    uint32_t	last_sector,
				pos;
    uint8_t	type;
    uint8_t	loaded;
	MVHDMeta*    vhd; /* Pointer to vhd needed for minivhd functions */
} hdd_image_t;


#ifdef ENABLE_HDD_LOG
int		hdd_image_do_log = ENABLE_HDD_LOG;
#endif
hdd_image_t	hdd_images[HDD_NUM];

void
hdd_image_log(int level, const char *fmt, ...)
{
#ifdef ENABLE_HDD_LOG
    va_list ap;

    if (hdd_image_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
#endif
}


int
image_is_hdi(const wchar_t *s)
{
    int len;
    wchar_t ext[5] = { 0, 0, 0, 0, 0 };
    char *ws = (char *) s;
    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return 0;
    memcpy(ext, ws + ((len - 4) << 1), 8);
    if (! wcscasecmp(ext, L".HDI"))
	return 1;
    else
	return 0;
}


int
image_is_hdx(const wchar_t *s, int check_signature)
{
    int len;
    FILE *f;
    uint64_t filelen;
    uint64_t signature;
    char *ws = (char *) s;
    wchar_t ext[5] = { 0, 0, 0, 0, 0 };
    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return 0;
    memcpy(ext, ws + ((len - 4) << 1), 8);
    if (wcscasecmp(ext, L".HDX") == 0) {
	if (check_signature) {
		f = plat_fopen((wchar_t *)s, L"rb");
		if (!f)
			return 0;
		fseeko64(f, 0, SEEK_END);
		filelen = ftello64(f);
		fseeko64(f, 0, SEEK_SET);
		if (filelen < 44)
			return 0;
		fread(&signature, 1, 8, f);
		fclose(f);
		if (signature == 0xD778A82044445459ll)
			return 1;
		else
			return 0;
	} else
		return 1;
    } else
    	return 0;
}


int
image_is_vhd(const wchar_t *s, int check_signature)
{
    int len;
    FILE *f;
#ifndef USE_MINIVHD
    uint64_t filelen;
    uint64_t signature;
#endif
    char *ws = (char *) s;
    wchar_t ext[5] = { 0, 0, 0, 0, 0 };
    
    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return 0;

    memcpy(ext, ws + ((len - 4) << 1), 8);
    if (wcscasecmp(ext, L".VHD") == 0) {	
	if (check_signature) {
		f = plat_fopen((wchar_t *)s, L"rb");
		if (!f)
			return 0;
#ifdef USE_MINIVHD
		bool x = mvhd_file_is_vhd (f); /* Note : find a better way to do this */
		fclose(f);
		if (x)
#else
		fseeko64(f, 0, SEEK_END);
		filelen = ftello64(f);
		fseeko64(f, -512, SEEK_END);
		if (filelen < 512)
			return 0;
		fread(&signature, 1, 8, f);
		fclose(f);
		if (signature == 0x78697463656E6F63ll)
#endif
			return 1;
		else
			return 0;
	} else
		return 1;
    } else
    	return 0;
}


static uint64_t
be_to_u64(uint8_t *bytes, int start)
{
    uint64_t n = ((uint64_t)bytes[start+7] << 0) | 
		 ((uint64_t)bytes[start+6] << 8) |
		 ((uint64_t)bytes[start+5] << 16) | 
		 ((uint64_t)bytes[start+4] << 24) | 
		 ((uint64_t)bytes[start+3] << 32) | 
		 ((uint64_t)bytes[start+2] << 40) | 
		 ((uint64_t)bytes[start+1] << 48) | 
		 ((uint64_t)bytes[start] << 56);
    return n;
}


static uint32_t
be_to_u32(uint8_t *bytes, int start)
{
    uint32_t n = ((uint32_t)bytes[start+3] << 0) | 
		 ((uint32_t)bytes[start+2] << 8) | 
		 ((uint32_t)bytes[start+1] << 16) | 
		 ((uint32_t)bytes[start] << 24);
    return n;
}


static uint16_t
be_to_u16(uint8_t *bytes, int start)
{
    uint16_t n = ((uint16_t)bytes[start+1] << 0) | 
		 ((uint16_t)bytes[start] <<8);
    return n;
}


static uint64_t
u64_to_be(uint64_t value, int is_be)
{
    uint64_t res = 0;

    if (is_be) 
	res = value;
    else {
	uint64_t mask = 0xff00000000000000;

	res = ((value & (mask >> 0)) >> 56) |
	      ((value & (mask >> 8)) >> 40) |
	      ((value & (mask >> 16)) >> 24) |
	      ((value & (mask >> 24)) >> 8) |
	      ((value & (mask >> 32)) << 8) |
	      ((value & (mask >> 40)) << 24) |
	      ((value & (mask >> 48)) << 40) |
	      ((value & (mask >> 56)) << 56);
    }

    return res;
}


static uint32_t
u32_to_be(uint32_t value, int is_be)
{
    uint32_t res = 0;

    if (is_be) 
	res = value;
    else {
	uint32_t mask = 0xff000000;

	res = ((value & (mask >> 0)) >> 24) |
	      ((value & (mask >> 8)) >> 8) |
	      ((value & (mask >> 16)) << 8) |
	      ((value & (mask >> 24)) << 24);
    }

    return res;
}


static uint16_t
u16_to_be(uint16_t value, int is_be)
{
    uint16_t res = 0;

    if (is_be) 
	res = value;
    else 
	res = (value >> 8) | (value << 8);

    return res;
}


static void
mk_guid(uint8_t *guid)
{
    int n;

    for (n = 0; n < 16; n++)
	guid[n] = random_generate();

    guid[6] &= 0x0F;
    guid[6] |= 0x40;	/* Type 4 */
    guid[8] &= 0x3F;
    guid[8] |= 0x80;	/* Variant 1 */
}


static uint32_t
calc_vhd_timestamp(void)
{
    time_t start_time;
    time_t curr_time;
    double vhd_time;

    start_time = 946684800; /* 1 Jan 2000 00:00 */
    curr_time = time(NULL);
    vhd_time = difftime(curr_time, start_time);

    return (uint32_t)vhd_time;
}


void
vhd_footer_from_bytes(vhd_footer_t *vhd, uint8_t *bytes)
{
    memcpy(vhd->cookie, bytes + VHD_OFFSET_COOKIE, sizeof(vhd->cookie));
    vhd->features = be_to_u32(bytes, VHD_OFFSET_FEATURES);
    vhd->version = be_to_u32(bytes, VHD_OFFSET_VERSION);
    vhd->offset = be_to_u64(bytes, VHD_OFFSET_DATA_OFFSET);
    vhd->timestamp = be_to_u32(bytes, VHD_OFFSET_TIMESTAMP);
    memcpy(vhd->creator, bytes + VHD_OFFSET_CREATOR, sizeof(vhd->creator));
    vhd->creator_vers = be_to_u32(bytes, VHD_OFFSET_CREATOR_VERS);
    memcpy(vhd->creator_host_os, bytes + VHD_OFFSET_CREATOR_HOST, sizeof(vhd->creator_host_os));
    vhd->orig_size = be_to_u64(bytes, VHD_OFFSET_ORIG_SIZE);
    vhd->curr_size = be_to_u64(bytes, VHD_OFFSET_CURR_SIZE);
    vhd->geom.cyl = be_to_u16(bytes, VHD_OFFSET_GEOM_CYL);
    vhd->geom.heads = bytes[VHD_OFFSET_GEOM_HEAD];
    vhd->geom.spt = bytes[VHD_OFFSET_GEOM_SPT];
    vhd->type = be_to_u32(bytes, VHD_OFFSET_TYPE);
    vhd->checksum = be_to_u32(bytes, VHD_OFFSET_CHECKSUM);
    memcpy(vhd->uuid, bytes + VHD_OFFSET_UUID, sizeof(vhd->uuid)); /* TODO: handle UUID's properly */
    vhd->saved_state = bytes[VHD_OFFSET_SAVED_STATE];
    memcpy(vhd->reserved, bytes + VHD_OFFSET_RESERVED, sizeof(vhd->reserved));
}


void
vhd_footer_to_bytes(uint8_t *bytes, vhd_footer_t *vhd)
{
    /* Quick endian check */
    int is_be = 0;
    uint8_t e = 1;
    uint8_t *ep = &e;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;

    if (ep[0] == 0) 
	is_be = 1;

    memcpy(bytes + VHD_OFFSET_COOKIE, vhd->cookie, sizeof(vhd->cookie));
    u32 = u32_to_be(vhd->features, is_be);
    memcpy(bytes + VHD_OFFSET_FEATURES, &u32, sizeof(vhd->features));
    u32 = u32_to_be(vhd->version, is_be);
    memcpy(bytes + VHD_OFFSET_VERSION, &u32, sizeof(vhd->version));
    u64 = u64_to_be(vhd->offset, is_be);
    memcpy(bytes + VHD_OFFSET_DATA_OFFSET, &u64, sizeof(vhd->offset));
    u32 = u32_to_be(vhd->timestamp, is_be);
    memcpy(bytes + VHD_OFFSET_TIMESTAMP, &u32, sizeof(vhd->timestamp));
    memcpy(bytes + VHD_OFFSET_CREATOR, vhd->creator, sizeof(vhd->creator));
    u32 = u32_to_be(vhd->creator_vers, is_be);
    memcpy(bytes + VHD_OFFSET_CREATOR_VERS, &u32, sizeof(vhd->creator_vers));
    memcpy(bytes + VHD_OFFSET_CREATOR_HOST, vhd->creator_host_os, sizeof(vhd->creator_host_os));
    u64 = u64_to_be(vhd->orig_size, is_be);
    memcpy(bytes + VHD_OFFSET_ORIG_SIZE, &u64, sizeof(vhd->orig_size));
    u64 = u64_to_be(vhd->curr_size, is_be);
    memcpy(bytes + VHD_OFFSET_CURR_SIZE, &u64, sizeof(vhd->curr_size));
    u16 = u16_to_be(vhd->geom.cyl, is_be);
    memcpy(bytes + VHD_OFFSET_GEOM_CYL, &u16, sizeof(vhd->geom.cyl));
    memcpy(bytes + VHD_OFFSET_GEOM_HEAD, &(vhd->geom.heads), sizeof(vhd->geom.heads));
    memcpy(bytes + VHD_OFFSET_GEOM_SPT, &(vhd->geom.spt), sizeof(vhd->geom.spt));
    u32 = u32_to_be(vhd->type, is_be);
    memcpy(bytes + VHD_OFFSET_TYPE, &u32, sizeof(vhd->type));
    u32 = u32_to_be(vhd->checksum, is_be);
    memcpy(bytes + VHD_OFFSET_CHECKSUM, &u32, sizeof(vhd->checksum));
    memcpy(bytes + VHD_OFFSET_UUID, vhd->uuid, sizeof(vhd->uuid));
    memcpy(bytes + VHD_OFFSET_SAVED_STATE, &(vhd->saved_state), sizeof(vhd->saved_state));
    memcpy(bytes + VHD_OFFSET_RESERVED, vhd->reserved, sizeof(vhd->reserved));
}


void
new_vhd_footer(vhd_footer_t **vhd)
{
    uint8_t cookie[8] = {'c', 'o', 'n', 'e', 'c', 't', 'i', 'x'};
    uint8_t creator[4] = {'V', 'A', 'R', 'C'};
    uint8_t cr_host_os[4] = {'W', 'i', '2', 'k'};

    if (*vhd == NULL)
	*vhd = (vhd_footer_t *)mem_alloc(sizeof(vhd_footer_t));

    memcpy((*vhd)->cookie, cookie, 8);
    (*vhd)->features = 0x00000002;
    (*vhd)->version = 0x00010000;
    (*vhd)->offset = 0xffffffffffffffff; /* fixed disk */
    (*vhd)->timestamp = calc_vhd_timestamp();
    memcpy((*vhd)->creator, creator, 4);
    (*vhd)->creator_vers = 0x00010000;
    memcpy((*vhd)->creator_host_os, cr_host_os, 4);
    (*vhd)->type = 2; /* fixed disk */
    mk_guid((*vhd)->uuid);
    (*vhd)->saved_state = 0;
    memset((*vhd)->reserved, 0, 427);
}


void
generate_vhd_checksum(vhd_footer_t *vhd)
{
    uint32_t chk = 0;
    int i;

    for (i = 0; i < sizeof(vhd_footer_t); i++) {
	/* We don't include the checksum field in the checksum */
	if ((i < VHD_OFFSET_CHECKSUM) || (i >= VHD_OFFSET_UUID))
		chk += ((uint8_t*)vhd)[i];
    }

    vhd->checksum = ~chk;
}


/*
 * Calculate the geometry from size (in MB), using the algorithm
 * provided in "Virtual Hard Disk Image Format Specification,
 * Appendix: CHS Calculation"i
 */
void
hdd_image_calc_chs(uint32_t *c, uint32_t *h, uint32_t *s, uint32_t size)
{
    uint64_t ts = ((uint64_t) size) << 11LL;
    uint32_t spt, heads, cyl, cth;

    if (ts > 65535 * 16 * 255)
	ts = 65535 * 16 * 255;
    if (ts >= 65535 * 16 * 63) {
	spt = 255;
	heads = 16;
	cth = (uint32_t) (ts / spt);
    } else {
	spt = 17;
	cth = (uint32_t) (ts / spt);
	heads = (cth +1023) / 1024;
	if (heads < 4)
		heads = 4;
	if ((cth >= (heads * 1024)) || (heads > 16)) {
		spt = 31;
		heads = 16;
		cth = (uint32_t) (ts / spt);
	}
	if (cth >= (heads * 1024)) {
		spt = 63;
		heads = 16;
		cth = (uint32_t) (ts / spt);
	}
    }
    cyl = cth / heads;
    *c = cyl;
    *h = heads;
    *s = spt;
}


static int
prepare_new_hard_disk(hdd_image_t *img, uint64_t full_size)
{
    uint64_t target_size;
    uint8_t *bufp;
    uint32_t r, t;
    uint32_t i, k;

    /* Make sure we start writing at the beginning. */
    target_size = (full_size + img->base) - ftello64(img->file);
#if 0
    fseeko64(img->file, 0ULL, SEEK_SET);
#endif

    k = (1 << 20);				/* 1048576 bytes, 1MB */
    t = (uint32_t) (target_size / k);		/* number of full 1MB blocks */
    r = (uint32_t) (target_size & (k - 1));	/* remainder, if any */

    bufp = (uint8_t *)mem_alloc(k);
    memset(bufp, 0x00, k);

    /* First, write all the 1MB blocks (if any.) */
    for (i = 0; i < t; i++)
	fwrite((uint8_t *)bufp, 1, k, img->file);

    /* Then, write the remainder. */
    fwrite((uint8_t *)bufp, 1, r, img->file);

    free(bufp);

    img->last_sector = (uint32_t) (full_size >> 9) - 1;
    img->loaded = 1;

    return 1;
}


void
hdd_image_init(void)
{
    int i;

    for (i = 0; i < HDD_NUM; i++)
	memset(&hdd_images[i], 0, sizeof(hdd_image_t));
}


int
hdd_image_load(int id)
{
    hdd_image_t *img = &hdd_images[id];
    uint32_t sector_size = 512;
    uint32_t zero = 0;
    uint64_t signature = 0xD778A82044445459ll;
    uint64_t full_size = 0;
    int spt = 0, hpc = 0, tracks = 0;
    int c, ret;
    uint64_t s = 0;
    wchar_t *fn = hdd[id].fn;
    int is_hdx[2] = { 0, 0 };
    int is_vhd[2] = { 0, 0 };
#ifndef USE_MINIVHD
    vhd_footer_t *vft = NULL;
    uint8_t *empty;
#else
    int vhd_error = 0;
	char opath[1024];
	char fullpath[1024];
#endif
    img->base = 0;

    is_vhd[0] = image_is_vhd(fn, 0);
    is_vhd[1] = image_is_vhd(fn, 1); 

    if (img->loaded) {
		if (img->file) {
			(void)fclose(img->file);
			img->file = NULL;
		} 
#ifdef USE_MINIVHD
		else if (img->vhd) {
			mvhd_close(img->vhd);
			img->vhd = NULL;
		}
#endif
		img->loaded = 0;
    }

    is_hdx[0] = image_is_hdx(fn, 0);
    is_hdx[1] = image_is_hdx(fn, 1);

    img->pos = 0;

    /* Try to open existing hard disk image */
    img->file = plat_fopen(fn, L"rb+");
    if (img->file == NULL) {
	/* Failed to open existing hard disk image */
	if (errno == ENOENT) {
		/* Failed because it does not exist,
		   so try to create new file */
		if (hdd[id].wp) {
			DEBUG("A write-protected image must exist\n");
			memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
			return 0;
		}

		img->file = plat_fopen(fn, L"wb+");
		if (img->file == NULL) {
			DEBUG("Unable to open image\n");
			memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
			return 0;
		} else {
			if (image_is_hdi(fn)) {
				full_size = ((uint64_t) hdd[id].spt) *
					    ((uint64_t) hdd[id].hpc) *
					    ((uint64_t) hdd[id].tracks) << 9LL;
				img->base = 0x1000;
				fwrite(&zero, 1, 4, img->file);
				fwrite(&zero, 1, 4, img->file);
				fwrite(&(img->base), 1, 4, img->file);
				fwrite(&full_size, 1, 4, img->file);
				fwrite(&sector_size, 1, 4, img->file);
				fwrite(&(hdd[id].spt), 1, 4, img->file);
				fwrite(&(hdd[id].hpc), 1, 4, img->file);
				fwrite(&(hdd[id].tracks), 1, 4, img->file);
				for (c = 0; c < 0x3f8; c++)
					fwrite(&zero, 1, 4, img->file);
				img->type = 1;
			} else if (is_hdx[0]) {
				full_size = ((uint64_t) hdd[id].spt) *
					    ((uint64_t) hdd[id].hpc) *
					    ((uint64_t) hdd[id].tracks) << 9LL;
				img->base = 0x28;
				fwrite(&signature, 1, 8, img->file);
				fwrite(&full_size, 1, 8, img->file);
				fwrite(&sector_size, 1, 4, img->file);
				fwrite(&(hdd[id].spt), 1, 4, img->file);
				fwrite(&(hdd[id].hpc), 1, 4, img->file);
				fwrite(&(hdd[id].tracks), 1, 4, img->file);
				fwrite(&zero, 1, 4, img->file);
				fwrite(&zero, 1, 4, img->file);
				img->type = 2;
			} 
#ifndef USE_MINIVHD
			else if (is_vhd[1]) {
				empty = (uint8_t *)mem_alloc(512);
				memset(empty, 0x00, 512);
				fseeko64(img->file, -512, SEEK_END);
				fread(empty, 1, 512, img->file);
				new_vhd_footer(&vft);
				vhd_footer_from_bytes(vft, empty);
				if (vft->type != 2) {
					/* VHD is not fixed size */
					DEBUG("VHD: Image is not fixed size\n");
					free(vft);
					vft = NULL;
					free(empty);
					fclose(img->file);
					img->file = NULL;
					memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
					return 0;
				}
				full_size = vft->orig_size;
				hdd[id].tracks = vft->geom.cyl;
				hdd[id].hpc = vft->geom.heads;
				hdd[id].spt = vft->geom.spt;
				free(vft);
				vft = NULL;
				free(empty);
				img->type = 3;

				/* If we're here, this means there is a valid VHD footer in the
		   		image, which means that by definition, all valid sectors
		   		are there. */
				img->last_sector = (uint32_t) (full_size >> 9) - 1;
				img->loaded = 1;
				return 1;
#else
			else if (is_vhd[0]) {
				MVHDGeom geometry;
				
				fclose(img->file);
				img->file = NULL;

				geometry.cyl = hdd[id].tracks;
				geometry.heads = hdd[id].hpc;
				geometry.spt = hdd[id].spt;
				full_size = ((uint64_t) hdd[id].spt) *
					    ((uint64_t) hdd[id].hpc) *
					    ((uint64_t) hdd[id].tracks) << 9LL;
				
			
				wcstombs(opath, fn, sizeof(opath));
				img->vhd = mvhd_create_fixed(opath, geometry, &vhd_error, NULL);
				if (img->vhd == NULL)
					DEBUG("hdd_image_load: could not create VHD file '%s': %s\n", opath, mvhd_strerr(vhd_error));
				img->type = HDD_IMAGE_VHD;

				img->last_sector = (uint32_t) (full_size >> 9) - 1;
				img->loaded = 1;
				return 1;
#endif
			} else 
				img->type = HDD_IMAGE_RAW;
			img->last_sector = 0;
		}

		s = full_size = ((uint64_t) hdd[id].spt) *
				((uint64_t) hdd[id].hpc) *
				((uint64_t) hdd[id].tracks) << 9LL;

		ret = prepare_new_hard_disk(img, full_size);

#ifndef USE_MINIVHD
		if (is_vhd[0]) {
			/* VHD image. */
			/* Generate new footer. */
			empty = (uint8_t *)mem_alloc(512);
			memset(empty, 0x00, 512);
			new_vhd_footer(&vft);
			vft->orig_size = vft->curr_size = full_size;
			vft->geom.cyl = hdd[id].tracks;
			vft->geom.heads = hdd[id].hpc;
			vft->geom.spt = hdd[id].spt;
			generate_vhd_checksum(vft);
			vhd_footer_to_bytes(empty, vft);
			fseeko64(img->file, 0, SEEK_END);
			fwrite(empty, 1, 512, img->file);
			free(vft);
			free(empty);
			img->type = HDD_IMAGE_VHD;
		}
#endif
		return ret;
	} else {
		/* Failed for another reason */
		DEBUG("Failed for another reason\n");
		memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
		return 0;
	}
    } else {
		if (image_is_hdi(fn)) {
			fseeko64(img->file, 0x8, SEEK_SET);
			fread(&(img->base), 1, 4, img->file);
			fseeko64(img->file, 0xC, SEEK_SET);
			full_size = 0LL;
			fread(&full_size, 1, 4, img->file);
			fseeko64(img->file, 0x10, SEEK_SET);
			fread(&sector_size, 1, 4, img->file);
			if (sector_size != 512) {
				/* Sector size is not 512 */
				DEBUG("HDI: Sector size is not 512\n");
				fclose(img->file);
				img->file = NULL;
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			}
			fread(&spt, 1, 4, img->file);
			fread(&hpc, 1, 4, img->file);
			fread(&tracks, 1, 4, img->file);
			hdd[id].spt = spt;
			hdd[id].hpc = hpc;
			hdd[id].tracks = tracks;
			img->type = HDD_IMAGE_HDI;
		} else if (is_hdx[1]) {
			img->base = 0x28;
			fseeko64(img->file, 8, SEEK_SET);
			fread(&full_size, 1, 8, img->file);
			fseeko64(img->file, 0x10, SEEK_SET);
			fread(&sector_size, 1, 4, hdd_images[id].file);
			if (sector_size != 512) {
				/* Sector size is not 512 */
				DEBUG("HDX: Sector size is not 512\n");
				fclose(img->file);
				img->file = NULL;
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			}
			fread(&spt, 1, 4, img->file);
			fread(&hpc, 1, 4, img->file);
			fread(&tracks, 1, 4, img->file);
			hdd[id].spt = spt;
			hdd[id].hpc = hpc;
			hdd[id].tracks = tracks;
			fread(&(hdd[id].at_spt), 1, 4, img->file);
			fread(&(hdd[id].at_hpc), 1, 4, img->file);
			img->type = HDD_IMAGE_HDX;
#ifdef USE_MINIVHD
		} else if (is_vhd[1]) /* Real VHD . Perhaps use is_vhd(0) to convert file to vhd ? */ {
			/* let's close file already opened and use the minivhd struct*/	
			fclose(img->file);
			img->file = NULL;
		
			wcstombs(opath, fn, sizeof(opath));
			_fullpath(fullpath, opath, sizeof(fullpath)); /* May break linux */
	
			img->vhd = mvhd_open(fullpath, (bool)0, &vhd_error);
			//free (path);
			//free (fullpath);

			if (img->vhd == NULL) {
				if (vhd_error == MVHD_ERR_FILE)
					DEBUG("hdd_image_load(): VHD: Error opening VHD file '%s': %s\n", fullpath, mvhd_strerr(vhd_error));
				else
					DEBUG("hdd_image_load(): VHD: Error opening VHD file '%s': %s\n", fullpath, mvhd_strerr(vhd_error));
			} else if (vhd_error == MVHD_ERR_TIMESTAMP) {
				DEBUG("hdd_image_load(): VHD: Parent/child timestamp mismatch for VHD file '%s'\n", fullpath);
			}

			full_size = img->vhd->footer.curr_sz;
			hdd[id].spt = img->vhd->footer.geom.spt;
			hdd[id].hpc = img->vhd->footer.geom.heads;
			hdd[id].tracks = img->vhd->footer.geom.cyl;
		
			img->type = HDD_IMAGE_VHD;

			img->last_sector = (uint32_t) (full_size >> 9) - 1;
			img->loaded = 1;
			return 1;
#endif
    	}	else { /* RAW image */
			full_size = ((uint64_t) hdd[id].spt) *
				    ((uint64_t) hdd[id].hpc) *
			    	((uint64_t) hdd[id].tracks) << 9LL;
			img->type = HDD_IMAGE_RAW;
		}
    }

    fseeko64(img->file, 0, SEEK_END);
    s = ftello64(img->file);
    if (s < (full_size + img->base)) {
		ret = prepare_new_hard_disk(img, full_size);
    } else {
		img->last_sector = (uint32_t) (full_size >> 9) - 1;
		img->loaded = 1;
		ret = 1;
    }

#ifndef USE_MINIVHD //should be useless
	if (is_vhd[0]) {
		fseeko64(img->file, 0, SEEK_END);
		s = ftello64(img->file);	
		if (s == (full_size + img->base)) {
			/* VHD image: generate new footer. */
			empty = (uint8_t *)mem_alloc(512);
			memset(empty, 0x00, 512);
			new_vhd_footer(&vft);
			vft->orig_size = vft->curr_size = full_size;
			vft->geom.cyl = hdd[id].tracks;
			vft->geom.heads = hdd[id].hpc;
			vft->geom.spt = hdd[id].spt;
			generate_vhd_checksum(vft);
			vhd_footer_to_bytes(empty, vft);
			fwrite((uint8_t *)empty, 1, 512, img->file);
			free(vft);
			free(empty);
			img->type = HDD_IMAGE_VHD;
			}
		}
#endif

    return ret;
}


void
hdd_image_seek(uint8_t id, uint32_t sector)
{
    hdd_image_t *img = &hdd_images[id];
    off64_t addr = (off64_t)sector << 9LL;

    img->pos = sector;

    if (img->type != HDD_IMAGE_VHD)
		fseeko64(img->file, addr + img->base, SEEK_SET);
}


void
hdd_image_read(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    hdd_image_t *img = &hdd_images[id];
    uint32_t i;
 
 #ifdef USE_MINIVHD
    if (img->type == HDD_IMAGE_VHD) {
		int non_transferred_sectors = mvhd_read_sectors(img->vhd, sector, count, buffer);
		img->pos = sector + count - non_transferred_sectors - 1;
    }
	if (img->type != HDD_IMAGE_VHD) {
#endif
		/* Move to the desired position in the image. */
		fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);

		/* Now read all (consecutive) blocks from the image. */
		for (i = 0; i < count; i++) {
			/* If past end of image, give up. */
			if (ferror(img->file) || feof(img->file))
				break;

			/* Read a block. */
			fread(buffer + (i << 9), 1, 512, img->file);

			/* If error during read, give up. */
			if (ferror(img->file))
				break;

			/* Update position. */
			img->pos = sector + i;
		}
#ifdef USE_MINIVHD
    }
#endif
}


uint32_t
hdd_sectors(uint8_t id)
{
    hdd_image_t *img = &hdd_images[id];

#ifdef USE_MINIVHD
	if (img->type == HDD_IMAGE_VHD) {
		return (uint32_t) (img->last_sector - 1);
	} else {
#endif
    fseeko64(img->file, 0, SEEK_END);

    return (uint32_t) ((ftello64(img->file) - img->base) >> 9);
#ifdef USE_MINIVHD
	}
#endif
}


int
hdd_image_read_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    hdd_image_t *img = &hdd_images[id];
    uint32_t transfer_sectors = count;
    uint32_t sectors = hdd_sectors(id);
    
	if ((sectors - sector) < transfer_sectors)
		transfer_sectors = sectors - sector;

	img->pos = sector;

	fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);
	fread(buffer, 1, transfer_sectors << 9, img->file);

	if (ferror(img->file) || (count != transfer_sectors))
		return 1;

	return 0;
}


void
hdd_image_write(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    hdd_image_t *img = &hdd_images[id];
    uint32_t i;

#ifdef USE_MINIVHD
    if (img->type == HDD_IMAGE_VHD) {
	int non_transferred_sectors = mvhd_write_sectors(img->vhd, sector, count, buffer);
	img->pos = sector + count - non_transferred_sectors - 1;
    }
	else {
#endif
		/* Move to the desired position in the image. */
		fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);

		/* Now write all (consecutive) blocks to the image. */
		for (i = 0; i < count; i++) {
			/* Write a block. */
			fwrite(buffer + (i << 9), 512, 1, img->file);

			/* If error during write, give up. */
			if (ferror(img->file))
				break;

			/* Update position. */
			img->pos = sector + i;
		}
#ifdef USE_MINIVHD		
    }
#endif
}


int
hdd_image_write_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer) // FIXME : not called by anything ?
{
    hdd_image_t *img = &hdd_images[id];
    uint32_t transfer_sectors = count;
    uint32_t sectors = hdd_sectors(id);

    if ((sectors - sector) < transfer_sectors)
		transfer_sectors = sectors - sector;

	img->pos = sector;

	fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);
	fwrite(buffer, transfer_sectors << 9, 1, img->file);

	if (ferror(img->file) || (count != transfer_sectors))
		return 1;
    	
	return 0;
}


void
hdd_image_zero(uint8_t id, uint32_t sector, uint32_t count)
{
    hdd_image_t *img = &hdd_images[id];
    uint8_t empty[512];
    uint32_t i = 0;

#ifdef USE_MINIVHD
    if (img->type == HDD_IMAGE_VHD) {
		int non_transferred_sectors = mvhd_format_sectors (img->vhd, sector, count);
		img->pos = sector + count - non_transferred_sectors - 1;
    } else {
#endif
		memset(empty, 0x00, sizeof(empty));

		/* Move to the desired position in the image. */
		fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);

		/* Now write all (consecutive) blocks to the image. */
		for (i = 0; i < count; i++) {
			/* Write a block. */
			fwrite(empty, 512, 1, img->file);

		/* If error during write, give up. */
		if (ferror(img->file))
			break;

		/* Update position. */
		img->pos = sector + i;
		}
#ifdef USE_MINIVHD
    }
#endif
}


int
hdd_image_zero_ex(uint8_t id, uint32_t sector, uint32_t count)
{
    hdd_image_t *img = &hdd_images[id];
    uint8_t empty[512];
    uint32_t transfer_sectors = count;
    uint32_t sectors = hdd_sectors(id);
    uint32_t i = 0;

    if ((sectors - sector) < transfer_sectors)
	transfer_sectors = sectors - sector;

    memset(empty, 0x00, sizeof(empty));

    img->pos = sector;

    fseeko64(img->file, ((uint64_t)sector << 9LL) + img->base, SEEK_SET);

    for (i = 0; i < transfer_sectors; i++) {
	fwrite(empty, 1, 512, img->file);

	/* If error during write, give up. */
	if (ferror(img->file))
		break;

    }

    if (ferror(img->file) || (count != transfer_sectors))
	return 1;

    return 0;
}


uint32_t
hdd_image_get_last_sector(uint8_t id)
{
    return hdd_images[id].last_sector;
}


uint32_t
hdd_image_get_pos(uint8_t id)
{
    return hdd_images[id].pos;
}


uint8_t
hdd_image_get_type(uint8_t id)
{
    return hdd_images[id].type;
}


void
hdd_image_specify(uint8_t id, int hpc, int spt)
{
    hdd_image_t *img = &hdd_images[id];

    if (img->type == 2) {
	hdd[id].at_hpc = hpc;
	hdd[id].at_spt = spt;

	fseeko64(img->file, 0x20, SEEK_SET);

	fwrite(&(hdd[id].at_spt), 1, 4, img->file);
	fwrite(&(hdd[id].at_hpc), 1, 4, img->file);
    }
}


void
hdd_image_unload(uint8_t id, int fn_preserve)
{
    hdd_image_t *img = &hdd_images[id];

    if (wcslen(hdd[id].fn) == 0)
	return;

    if (img->loaded) {
		if (img->file != NULL) {
			(void)fclose(img->file);
			img->file = NULL;
		}
#ifdef USE_MINIVHD
		else if (img->vhd != NULL) {
				mvhd_close(img->vhd);
				img->vhd = NULL;
			}
#endif
		img->loaded = 0;
    }

    img->last_sector = -1;

    memset(hdd[id].prev_fn, 0, sizeof(hdd[id].prev_fn));
    if (fn_preserve)
	wcscpy(hdd[id].prev_fn, hdd[id].fn);
    memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
}


void
hdd_image_close(uint8_t id)
{
    hdd_image_t *img = &hdd_images[id];

    DEBUG("hdd_image_close(%i)\n", id);
  
	if (! img->loaded) return;

	if (img->file != NULL) {
		(void)fclose(img->file);
		img->file = NULL;
#ifdef USE_MINIVHD
	 } else if (img->vhd != NULL) {
			mvhd_close(img->vhd);
			img->vhd = NULL;
	}
#endif
	}
    memset(img, 0x00, sizeof(hdd_image_t));
	img->loaded = 0;
}
