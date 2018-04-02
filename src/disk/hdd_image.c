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
 * Version:	@(#)hdd_image.c	1.0.3	2018/03/31
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <errno.h>
#define HAVE_STDARG_H
#include "../emu.h"
#include "../plat.h"
#include "hdd.h"


typedef struct
{
	FILE *file;
	uint32_t base;
	uint32_t last_sector;
	uint8_t type;
	uint8_t loaded;
} hdd_image_t;


hdd_image_t hdd_images[HDD_NUM];

static char empty_sector[512];
static char *empty_sector_1mb;


#ifdef ENABLE_HDD_LOG
int hdd_image_do_log = ENABLE_HDD_LOG;
#endif


static void
hdd_image_log(const char *fmt, ...)
{
#ifdef ENABLE_HDD_LOG
	va_list ap;

	if (hdd_image_do_log) {
		va_start(ap, fmt);
		pclog_ex(fmt, ap);
		va_end(ap);
	}
#endif
}


int image_is_hdi(const wchar_t *s)
{
	int len;
	wchar_t ext[5] = { 0, 0, 0, 0, 0 };
	char *ws = (char *) s;
	len = wcslen(s);
	if ((len < 4) || (s[0] == L'.')) return 0;

	memcpy(ext, ws + ((len - 4) << 1), 8);
	if (! wcscasecmp(ext, L".HDI")) return 1;

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
	len = wcslen(s);
	if ((len < 4) || (s[0] == L'.')) return 0;

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

			return 0;
		}

		return 1;
	}

	return 0;
}


int hdd_image_load(int id)
{
	uint32_t sector_size = 512;
	uint32_t zero = 0;
	uint64_t signature = 0xD778A82044445459ll;
	uint64_t s = 0, t, full_size = 0;
	int spt = 0, hpc = 0, tracks = 0;
	int c, i;
	wchar_t *fn = hdd[id].fn;
	int is_hdx[2] = { 0, 0 };

	memset(empty_sector, 0, sizeof(empty_sector));

	hdd_images[id].base = 0;

	if (hdd_images[id].loaded) {
		if (hdd_images[id].file) {
			fclose(hdd_images[id].file);
			hdd_images[id].file = NULL;
		}
		hdd_images[id].loaded = 0;
	}

	is_hdx[0] = image_is_hdx(fn, 0);
	is_hdx[1] = image_is_hdx(fn, 1);

	/* Try to open existing hard disk image */
	if (fn[0] == '.') {
		hdd_image_log("File name starts with .\n");
		memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
		return 0;
	}
	hdd_images[id].file = plat_fopen(fn, L"rb+");
	if (hdd_images[id].file == NULL) {
		/* Failed to open existing hard disk image */
		if (errno == ENOENT) {
			/* Failed because it does not exist,
			   so try to create new file */
			if (hdd[id].wp) {
				hdd_image_log("A write-protected image must exist\n");
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			}

			hdd_images[id].file = plat_fopen(fn, L"wb+");
			if (hdd_images[id].file == NULL) {
				hdd_image_log("Unable to open image\n");
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			} else {
				if (image_is_hdi(fn)) {
					full_size = hdd[id].spt * hdd[id].hpc * hdd[id].tracks * 512;
					hdd_images[id].base = 0x1000;
					fwrite(&zero, 1, 4, hdd_images[id].file);
					fwrite(&zero, 1, 4, hdd_images[id].file);
					fwrite(&(hdd_images[id].base), 1, 4, hdd_images[id].file);
					fwrite(&full_size, 1, 4, hdd_images[id].file);
					fwrite(&sector_size, 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].spt), 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].hpc), 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].tracks), 1, 4, hdd_images[id].file);
					for (c = 0; c < 0x3f8; c++)
						fwrite(&zero, 1, 4, hdd_images[id].file);
					hdd_images[id].type = 1;
				} else if (is_hdx[0]) {
					full_size = hdd[id].spt * hdd[id].hpc * hdd[id].tracks * 512;
					hdd_images[id].base = 0x28;
					fwrite(&signature, 1, 8, hdd_images[id].file);
					fwrite(&full_size, 1, 8, hdd_images[id].file);
					fwrite(&sector_size, 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].spt), 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].hpc), 1, 4, hdd_images[id].file);
					fwrite(&(hdd[id].tracks), 1, 4, hdd_images[id].file);
					fwrite(&zero, 1, 4, hdd_images[id].file);
					fwrite(&zero, 1, 4, hdd_images[id].file);
					hdd_images[id].type = 2;
				} else {
					hdd_images[id].type = 0;
				}
				hdd_images[id].last_sector = 0;
			}

			s = full_size = hdd[id].spt * hdd[id].hpc * hdd[id].tracks * 512;

			goto prepare_new_hard_disk;
		} else {
			/* Failed for another reason */
			hdd_image_log("Failed for another reason\n");
			memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
			return 0;
		}
	} else {
		if (image_is_hdi(fn)) {
			fseeko64(hdd_images[id].file, 0x8, SEEK_SET);
			fread(&(hdd_images[id].base), 1, 4, hdd_images[id].file);
			fseeko64(hdd_images[id].file, 0xC, SEEK_SET);
			full_size = 0;
			fread(&full_size, 1, 4, hdd_images[id].file);
			fseeko64(hdd_images[id].file, 0x10, SEEK_SET);
			fread(&sector_size, 1, 4, hdd_images[id].file);
			if (sector_size != 512) {
				/* Sector size is not 512 */
				hdd_image_log("HDI: Sector size is not 512\n");
				fclose(hdd_images[id].file);
				hdd_images[id].file = NULL;
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			}
			fread(&spt, 1, 4, hdd_images[id].file);
			fread(&hpc, 1, 4, hdd_images[id].file);
			fread(&tracks, 1, 4, hdd_images[id].file);
			if (hdd[id].bus == HDD_BUS_SCSI_REMOVABLE) {
				if ((spt != hdd[id].spt) || (hpc != hdd[id].hpc) || (tracks != hdd[id].tracks)) {
					hdd_image_log("HDI: Geometry mismatch\n");
					fclose(hdd_images[id].file);
					hdd_images[id].file = NULL;
					memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
					return 0;
				}
			}
			hdd[id].spt = (uint8_t)spt;
			hdd[id].hpc = (uint8_t)hpc;
			hdd[id].tracks = (uint16_t)tracks;
			hdd_images[id].type = 1;
		} else if (is_hdx[1]) {
			hdd_images[id].base = 0x28;
			fseeko64(hdd_images[id].file, 8, SEEK_SET);
			fread(&full_size, 1, 8, hdd_images[id].file);
			fseeko64(hdd_images[id].file, 0x10, SEEK_SET);
			fread(&sector_size, 1, 4, hdd_images[id].file);
			if (sector_size != 512) {
				/* Sector size is not 512 */
				hdd_image_log("HDX: Sector size is not 512\n");
				fclose(hdd_images[id].file);
				hdd_images[id].file = NULL;
				memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
				return 0;
			}
			fread(&spt, 1, 4, hdd_images[id].file);
			fread(&hpc, 1, 4, hdd_images[id].file);
			fread(&tracks, 1, 4, hdd_images[id].file);
			if (hdd[id].bus == HDD_BUS_SCSI_REMOVABLE) {
				if ((spt != hdd[id].spt) || (hpc != hdd[id].hpc) || (tracks != hdd[id].tracks)) {
					hdd_image_log("HDX: Geometry mismatch\n");
					fclose(hdd_images[id].file);
					hdd_images[id].file = NULL;
					memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
					return 0;
				}
			}
			hdd[id].spt = (uint8_t)spt;
			hdd[id].hpc = (uint8_t)hpc;
			hdd[id].tracks = (uint16_t)tracks;
			fread(&(hdd[id].at_spt), 1, 4, hdd_images[id].file);
			fread(&(hdd[id].at_hpc), 1, 4, hdd_images[id].file);
			hdd_images[id].type = 2;
		} else {
			full_size = hdd[id].spt * hdd[id].hpc * hdd[id].tracks * 512;
			hdd_images[id].type = 0;
		}
	}

	fseeko64(hdd_images[id].file, 0, SEEK_END);
	if (ftello64(hdd_images[id].file) < (int64_t)(full_size + hdd_images[id].base)) {
		s = (full_size + hdd_images[id].base) - ftello64(hdd_images[id].file);
prepare_new_hard_disk:
		s >>= 9;
		t = (s >> 11) << 11;
		s -= t;
		t >>= 11;

		empty_sector_1mb = (char *) malloc(1048576);
		memset(empty_sector_1mb, 0, 1048576);

		if (s > 0) {
			for (i = 0; i < s; i++) {
				fwrite(empty_sector, 1, 512, hdd_images[id].file);
			}
		}

		if (t > 0) {
			for (i = 0; i < t; i++) {
				fwrite(empty_sector_1mb, 1, 1045876, hdd_images[id].file);
			}
		}

		free(empty_sector_1mb);
	}

	hdd_images[id].last_sector = (uint32_t) (full_size >> 9) - 1;

	hdd_images[id].loaded = 1;

	return 1;
}

void hdd_image_seek(uint8_t id, uint32_t sector)
{
	off64_t addr = sector;
	addr = (uint64_t)sector * 512;

	fseeko64(hdd_images[id].file, addr + hdd_images[id].base, SEEK_SET);
}

void hdd_image_read(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	fread(buffer, 1, count * 512, hdd_images[id].file);
}

uint32_t hdd_sectors(uint8_t id)
{
	fseeko64(hdd_images[id].file, 0, SEEK_END);
	return (uint32_t) (ftello64(hdd_images[id].file) >> 9);
}

int hdd_image_read_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
	uint32_t transfer_sectors = count;
	uint32_t sectors = hdd_sectors(id);

	if ((sectors - sector) < transfer_sectors)
		transfer_sectors = sectors - sector;

	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	fread(buffer, 1, transfer_sectors * 512, hdd_images[id].file);

	if (count != transfer_sectors)
		return 1;

	return 0;
}

void hdd_image_write(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	fwrite(buffer, count * 512, 1, hdd_images[id].file);
}

int hdd_image_write_ex(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
	uint32_t transfer_sectors = count;
	uint32_t sectors = hdd_sectors(id);

	if ((sectors - sector) < transfer_sectors)
		transfer_sectors = sectors - sector;

	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	fwrite(buffer, transfer_sectors * 512, 1, hdd_images[id].file);

	if (count != transfer_sectors)
		return 1;

	return 0;
}

void hdd_image_zero(uint8_t id, uint32_t sector, uint32_t count)
{
	uint32_t i;

	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	for (i = 0; i < count; i++)
		fwrite(empty_sector, 512, 1, hdd_images[id].file);
}

int hdd_image_zero_ex(uint8_t id, uint32_t sector, uint32_t count)
{
	uint32_t i;

	uint32_t transfer_sectors = count;
	uint32_t sectors = hdd_sectors(id);

	if ((sectors - sector) < transfer_sectors)
		transfer_sectors = sectors - sector;

	fseeko64(hdd_images[id].file, ((uint64_t)sector * 512) + hdd_images[id].base, SEEK_SET);
	for (i = 0; i < transfer_sectors; i++)
		fwrite(empty_sector, 1, 512, hdd_images[id].file);

	if (count != transfer_sectors)
		return 1;

	return 0;
}

uint32_t hdd_image_get_last_sector(uint8_t id)
{
	return hdd_images[id].last_sector;
}

uint8_t hdd_image_get_type(uint8_t id)
{
	return hdd_images[id].type;
}

void hdd_image_specify(uint8_t id, int hpc, int spt)
{
	if (hdd_images[id].type == 2) {
		hdd[id].at_hpc = (uint8_t)hpc;
		hdd[id].at_spt = (uint8_t)spt;
		fseeko64(hdd_images[id].file, 0x20, SEEK_SET);
		fwrite(&(hdd[id].at_spt), 1, 4, hdd_images[id].file);
		fwrite(&(hdd[id].at_hpc), 1, 4, hdd_images[id].file);
	}
}

void hdd_image_unload(uint8_t id, int fn_preserve)
{
	if (wcslen(hdd[id].fn) == 0)
		return;

	if (hdd_images[id].loaded) {
		if (hdd_images[id].file != NULL) {
			fclose(hdd_images[id].file);
			hdd_images[id].file = NULL;
		}
		hdd_images[id].loaded = 0;
	}

	hdd_images[id].last_sector = -1;

	memset(hdd[id].prev_fn, 0, sizeof(hdd[id].prev_fn));
	if (fn_preserve)
		wcscpy(hdd[id].prev_fn, hdd[id].fn);
	memset(hdd[id].fn, 0, sizeof(hdd[id].fn));
}

void hdd_image_close(uint8_t id)
{
	if (hdd_images[id].file != NULL) {
		fclose(hdd_images[id].file);
		hdd_images[id].file = NULL;
	}
	hdd_images[id].loaded = 0;
}
