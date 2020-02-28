/**
 * \file
 * \brief Utility functions
 */

#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "libxml2_encoding.h"
#include "minivhd_internal.h"
#include "minivhd_util.h"

const char MVHD_CONECTIX_COOKIE[] = "conectix";
const char MVHD_CREATOR[] = "VARC";
const char MVHD_CREATOR_HOST_OS[] = "Wi2k";
const char MVHD_CXSPARSE_COOKIE[] = "cxsparse";

uint16_t mvhd_from_be16(uint16_t val) {
    uint8_t *tmp = (uint8_t*)&val;
    uint16_t ret = 0;
    ret |= (uint16_t)tmp[0] << 8;
    ret |= (uint16_t)tmp[1] << 0;
    return ret;
}
uint32_t mvhd_from_be32(uint32_t val) {
    uint8_t *tmp = (uint8_t*)&val;
    uint32_t ret = 0;
    ret =  (uint32_t)tmp[0] << 24;
    ret |= (uint32_t)tmp[1] << 16;
    ret |= (uint32_t)tmp[2] << 8;
    ret |= (uint32_t)tmp[3] << 0;
    return ret;
}
uint64_t mvhd_from_be64(uint64_t val) {
    uint8_t *tmp = (uint8_t*)&val;
    uint64_t ret = 0;
    ret =  (uint64_t)tmp[0] << 56;
    ret |= (uint64_t)tmp[1] << 48;
    ret |= (uint64_t)tmp[2] << 40;
    ret |= (uint64_t)tmp[3] << 32;
    ret |= (uint64_t)tmp[4] << 24;
    ret |= (uint64_t)tmp[5] << 16;
    ret |= (uint64_t)tmp[6] << 8;
    ret |= (uint64_t)tmp[7] << 0;
    return ret;
}
uint16_t mvhd_to_be16(uint16_t val) {
    uint16_t ret = 0;
    uint8_t *tmp = (uint8_t*)&ret;
    tmp[0] = (val & 0xff00) >> 8;
    tmp[1] = (val & 0x00ff) >> 0;
    return ret;
}
uint32_t mvhd_to_be32(uint32_t val) {
    uint32_t ret = 0;
    uint8_t *tmp = (uint8_t*)&ret;
    tmp[0] = (val & 0xff000000) >> 24;
    tmp[1] = (val & 0x00ff0000) >> 16;
    tmp[2] = (val & 0x0000ff00) >> 8;
    tmp[3] = (val & 0x000000ff) >> 0;
    return ret;
}
uint64_t mvhd_to_be64(uint64_t val) {
    uint64_t ret = 0;
    uint8_t *tmp = (uint8_t*)&ret;
    tmp[0] = (val & 0xff00000000000000) >> 56;
    tmp[1] = (val & 0x00ff000000000000) >> 48;
    tmp[2] = (val & 0x0000ff0000000000) >> 40;
    tmp[3] = (val & 0x000000ff00000000) >> 32;
    tmp[4] = (val & 0x00000000ff000000) >> 24;
    tmp[5] = (val & 0x0000000000ff0000) >> 16;
    tmp[6] = (val & 0x000000000000ff00) >> 8;
    tmp[7] = (val & 0x00000000000000ff) >> 0;
    return ret;
}

bool mvhd_is_conectix_str(const void* buffer) {
    if (strncmp(buffer, MVHD_CONECTIX_COOKIE, strlen(MVHD_CONECTIX_COOKIE)) == 0) {
        return true;
    } else {
        return false;
    }
}

void mvhd_generate_uuid(uint8_t* uuid)
{
    /* We aren't doing crypto here, so using system time as seed should be good enough */
    srand(time(0));
    for (int n = 0; n < 16; n++) {
        uuid[n] = rand();
    }
    uuid[6] &= 0x0F;
    uuid[6] |= 0x40; /* Type 4 */
    uuid[8] &= 0x3F;
    uuid[8] |= 0x80; /* Variant 1 */
}

uint32_t vhd_calc_timestamp(void)
{
        time_t start_time;
        time_t curr_time;
        double vhd_time;
        start_time = MVHD_START_TS; /* 1 Jan 2000 00:00 */
        curr_time = time(NULL);
        vhd_time = difftime(curr_time, start_time);
        return (uint32_t)vhd_time;
}

time_t vhd_get_created_time(MVHDMeta *vhdm)
{
        time_t vhd_time = (time_t)vhdm->footer.timestamp;
        time_t vhd_time_unix = MVHD_START_TS + vhd_time;
        return vhd_time_unix;
}

FILE* mvhd_fopen(const char* path, const char* mode, int* err) {
    FILE* f = NULL;
#ifdef _WIN32
    size_t path_len = strlen(path);
    size_t mode_len = strlen(mode);
    mvhd_utf16 new_path[260] = {0};
    int new_path_len = (sizeof new_path) - 2;
    mvhd_utf16 mode_str[5] = {0};
    int new_mode_len = (sizeof mode_str) - 2;
    int path_res = UTF8ToUTF16LE((unsigned char*)new_path, &new_path_len, (const unsigned char*)path, (int*)&path_len);
    int mode_res = UTF8ToUTF16LE((unsigned char*)mode_str, &new_mode_len, (const unsigned char*)mode, (int*)&mode_len);
    if (path_res > 0 && mode_res > 0) {
        f = _wfopen(new_path, mode_str);
        if (f == NULL) {
            mvhd_errno = errno;
            *err = MVHD_ERR_FILE;
        }
    } else {
        if (path_res == -1 || mode_res == -1) {
            *err = MVHD_ERR_UTF_SIZE;
        } else if (path_res == -2 || mode_res == -2) {
            *err = MVHD_ERR_UTF_TRANSCODING_FAILED;
        }
    }
#else
    f = fopen64(path, mode);
    if (f == NULL) {
        mvhd_errno = errno;
        *err = MVHD_ERR_FILE;
    }
#endif
    return f;
}

void mvhd_set_encoding_err(int encoding_retval, int* err) {
    if (encoding_retval == -1) {
        *err = MVHD_ERR_UTF_SIZE;
    } else if (encoding_retval == -2) {
        *err = MVHD_ERR_UTF_TRANSCODING_FAILED;
    }
}

uint64_t mvhd_calc_size_bytes(MVHDGeom *geom) {
    uint64_t img_size = (uint64_t)geom->cyl * (uint64_t)geom->heads * (uint64_t)geom->spt * (uint64_t)MVHD_SECTOR_SIZE;
    return img_size;
}

uint32_t mvhd_calc_size_sectors(MVHDGeom *geom) {
    uint32_t sector_size = (uint32_t)geom->cyl * (uint32_t)geom->heads * (uint32_t)geom->spt;
    return sector_size;
}

uint32_t mvhd_gen_footer_checksum(MVHDFooter* footer) {
    uint32_t new_chk = 0;
    uint32_t orig_chk = footer->checksum;
    footer->checksum = 0;
    uint8_t* footer_bytes = (uint8_t*)footer;
    for (size_t i = 0; i < sizeof *footer; i++) {
        new_chk += footer_bytes[i];
    }
    footer->checksum = orig_chk;
    return ~new_chk;
}

uint32_t mvhd_gen_sparse_checksum(MVHDSparseHeader* header) {
    uint32_t new_chk = 0;
    uint32_t orig_chk = header->checksum;
    header->checksum = 0;
    uint8_t* sparse_bytes = (uint8_t*)header;
    for (size_t i = 0; i < sizeof *header; i++) {
        new_chk += sparse_bytes[i];
    }
    header->checksum = orig_chk;
    return ~new_chk;
}

const char* mvhd_strerr(MVHDError err) {
    switch (err) {
    case MVHD_ERR_MEM:
        return "memory allocation error";
    case MVHD_ERR_FILE:
        return "file error";
    case MVHD_ERR_NOT_VHD:
        return "file is not a VHD image";
    case MVHD_ERR_TYPE:
        return "unsupported VHD image type";
    case MVHD_ERR_FOOTER_CHECKSUM:
        return "invalid VHD footer checksum";
    case MVHD_ERR_SPARSE_CHECKSUM:
        return "invalid VHD sparse header checksum";
    case MVHD_ERR_UTF_TRANSCODING_FAILED:
        return "error converting path encoding";
    case MVHD_ERR_UTF_SIZE:
        return "buffer size mismatch when converting path encoding";
    case MVHD_ERR_PATH_REL:
        return "relative path detected where absolute path expected";
    case MVHD_ERR_PATH_LEN:
        return "path length exceeds MVHD_MAX_PATH";
    case MVHD_ERR_PAR_NOT_FOUND:
        return "parent VHD image not found";
    case MVHD_ERR_INVALID_PAR_UUID:
        return "UUID mismatch between child and parent VHD";
    case MVHD_ERR_INVALID_GEOM:
        return "invalid geometry detected";
    case MVHD_ERR_INVALID_PARAMS:
        return "invalid parameters passed to function";
    case MVHD_ERR_CONV_SIZE:
        return "error converting image. Size mismatch detechted";
    default:
        return "unknown error";
    }
}