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
 * Version:	@(#)hdd_image.c	1.0.15	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include "../../misc/random.h"
#include "hdd.h"
#ifdef USE_MINIVHD
# include <minivhd.h>
#endif


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
#ifdef USE_MINIVHD
    MVHDMeta	*vhd;
#endif
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
    const wchar_t *sp;
    int len;

    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return(0);
    if ((sp = wcschr(s, L'.')) == NULL)
	return(0);

    return(! wcscasecmp(sp+1, L".HDI"));
}


int
image_is_hdx(const wchar_t *s, int check_signature)
{
    const wchar_t *sp;
    uint64_t signature;
    uint64_t filelen;
    FILE *f;
    int len;

    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return(0);
    if ((sp = wcschr(s, L'.')) == NULL)
	return(0);
    if (wcscasecmp(sp, L".HDX") != 0)
	return(0);

    if (check_signature) {
	f = plat_fopen((wchar_t *)s, L"rb");
	if (f == NULL)
		return(0);
	fseeko64(f, 0, SEEK_END);
	filelen = ftello64(f);
	fseeko64(f, 0, SEEK_SET);
	if (filelen < 44)
		return(0);
	fread(&signature, 1, 8, f);
	fclose(f);
	if (signature != 0xD778A82044445459ll)
		return(0);
    }

    return(1);
}


#ifdef USE_MINIVHD
int
image_is_vhd(const wchar_t *s, int check_signature)
{
    const wchar_t *sp;
    int len, x = 1;
    FILE *f;
    
    len = (int)wcslen(s);
    if ((len < 4) || (s[0] == L'.'))
	return(0);
    if ((sp = wcschr(s, L'.')) == NULL)
	return(0);
    if (wcscasecmp(sp, L".VHD") != 0)
	return(0);

    if (check_signature) {
	f = plat_fopen((wchar_t *)s, L"rb");
	if (f == NULL)
		return(0);

	/* Note : find a better way to do this */
	x = mvhd_file_is_vhd(f);
	fclose(f);
    }

    return(x);
}
#endif


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

    k = (1 << 20);				// 1048576 bytes, 1MB
    t = (uint32_t) (target_size / k);		// number of full 1MB blocks
    r = (uint32_t) (target_size & (k - 1));	// remainder, if any

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

#ifdef USE_MINIVHD
    /* Load and initialize the DLL here. */
#endif

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
#ifdef USE_MINIVHD
    int is_vhd[2] = { 0, 0 };
    int vhd_error = 0;
    char opath[1024];
    char fullpath[1024];
    MVHDGeom vhd_geom;
#endif
    img->base = 0;

#ifdef USE_MINIVHD
    is_vhd[0] = image_is_vhd(fn, 0);
    is_vhd[1] = image_is_vhd(fn, 1); 
#endif

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
#ifdef USE_MINIVHD
			} else if (is_vhd[0]) {
				fclose(img->file);
				img->file = NULL;

				vhd_geom.cyl = hdd[id].tracks;
				vhd_geom.heads = hdd[id].hpc;
				vhd_geom.spt = hdd[id].spt;
				full_size = ((uint64_t) hdd[id].spt) *
					    ((uint64_t) hdd[id].hpc) *
					    ((uint64_t) hdd[id].tracks) << 9LL;
				
			
				wcstombs(opath, fn, sizeof(opath));
				img->vhd = mvhd_create_fixed(opath, vhd_geom, &vhd_error, NULL);
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
	} else if (is_vhd[1]) {
		fclose(img->file);
		img->file = NULL;

		wcstombs(opath, fn, sizeof(opath));
		_fullpath(fullpath, opath, sizeof(fullpath)); /* May break linux */

		img->vhd = mvhd_open(fullpath, 0, &vhd_error);

		if (img->vhd == NULL) {
			if (vhd_error == MVHD_ERR_FILE)
				ERRLOG("hdd_image_load(): VHD: Error opening VHD file '%s': %s\n", fullpath, mvhd_strerr(vhd_error));
			else
				ERRLOG("hdd_image_load(): VHD: Error opening VHD file '%s': %s\n", fullpath, mvhd_strerr(vhd_error));
		} else if (vhd_error == MVHD_ERR_TIMESTAMP) {
			ERRLOG("hdd_image_load(): VHD: Parent/child timestamp mismatch for VHD file '%s'\n", fullpath);
		}

		vhd_geom = mvhd_get_geometry(img->vhd);
		hdd[id].spt = vhd_geom.spt;
		hdd[id].hpc = vhd_geom.heads;
		hdd[id].tracks = vhd_geom.cyl;
		full_size = mvhd_get_current_size(img->vhd);

		img->type = HDD_IMAGE_VHD;

		img->last_sector = (uint32_t) (full_size >> 9) - 1;
		img->loaded = 1;
		return 1;
#endif
    	} else { /* RAW image */
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
#ifdef USE_MINIVHD
    int remaining;
#endif
    uint32_t i;

#ifdef USE_MINIVHD
    if (img->type == HDD_IMAGE_VHD) {
	remaining = mvhd_write_sectors(img->vhd, sector, count, buffer);
	img->pos = sector + count - remaining - 1;
    } else {
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


#if 0
// FIXME : not called by anything ?
int
hdd_image_write_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
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
#endif


void
hdd_image_zero(uint8_t id, uint32_t sector, uint32_t count)
{
    hdd_image_t *img = &hdd_images[id];
    uint8_t empty[512];
#ifdef USE_MINIVHD
    int remaining;
#endif
    uint32_t i = 0;

#ifdef USE_MINIVHD
    if (img->type == HDD_IMAGE_VHD) {
	remaining = mvhd_format_sectors (img->vhd, sector, count);
	img->pos = sector + count - remaining - 1;
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

    if (img->type == HDD_IMAGE_HDX) {
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
#endif
    }

    memset(img, 0x00, sizeof(hdd_image_t));

    img->loaded = 0;
}
