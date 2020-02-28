#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cwalk.h"
#include "libxml2_encoding.h"
#include "minivhd_internal.h"
#include "minivhd_util.h"
#include "minivhd_struct_rw.h"
#include "minivhd_io.h"
#include "minivhd_create.h"
#include "minivhd.h"

static void mvhd_gen_footer(MVHDFooter* footer, MVHDGeom* geom, MVHDType type, uint64_t sparse_header_off);
static void mvhd_gen_sparse_header(MVHDSparseHeader* header, uint32_t num_blks, uint64_t bat_offset);
static int mvhd_gen_par_loc(MVHDSparseHeader* header, 
                            const char* child_path, 
                            const char* par_path, 
                            uint64_t start_offset, 
                            void* w2ku_buff,
                            void* w2ru_buff,
                            MVHDError* err);
static MVHDMeta* mvhd_create_sparse_diff(const char* path, const char* par_path, MVHDGeom* geom, int* err);

/**
 * \brief Populate a VHD footer
 * 
 * \param [in] footer to populate
 * \param [in] geom to use
 * \param [in] type of HVD that is being created
 * \param [in] sparse_header_off, an absolute file offset to the sparse header. Not used for fixed VHD images
 */
static void mvhd_gen_footer(MVHDFooter* footer, MVHDGeom* geom, MVHDType type, uint64_t sparse_header_off) {
    memcpy(footer->cookie, "conectix", sizeof footer->cookie);
    footer->features = 0x00000002;
    footer->fi_fmt_vers = 0x00010000;
    footer->data_offset = (type == MVHD_TYPE_DIFF || type == MVHD_TYPE_DYNAMIC) ? sparse_header_off : 0xffffffffffffffff;
    footer->timestamp = vhd_calc_timestamp();
    memcpy(footer->cr_app, "mvhd", sizeof footer->cr_app);
    footer->cr_vers = 0x000e0000;
    memcpy(footer->cr_host_os, "Wi2k", sizeof footer->cr_host_os);
    footer->orig_sz = footer->curr_sz = mvhd_calc_size_bytes(geom);
    footer->geom.cyl = geom->cyl;
    footer->geom.heads = geom->heads;
    footer->geom.spt = geom->spt;
    footer->disk_type = type;
    mvhd_generate_uuid(footer->uuid);
    footer->checksum = mvhd_gen_footer_checksum(footer);
}

/**
 * \brief Populate a VHD sparse header
 * 
 * \param [in] header for sparse and differencing images
 * \param [in] num_blks is the number of data blocks that the image contains
 * \param [in] bat_offset is the absolute file offset for start of the Block Allocation Table
 */
static void mvhd_gen_sparse_header(MVHDSparseHeader* header, uint32_t num_blks, uint64_t bat_offset) {
    memcpy(header->cookie, "cxsparse", sizeof header->cookie);
    header->data_offset = 0xffffffffffffffff;
    header->bat_offset = bat_offset;
    header->head_vers = 0x00010000;
    header->max_bat_ent = num_blks;
    header->block_sz = (uint32_t)MVHD_SECT_PER_BLOCK * (uint32_t)MVHD_SECTOR_SIZE;
    header->checksum = mvhd_gen_sparse_checksum(header);
}

/**
 * \brief Generate parent locators for differencing VHD images
 * 
 * \param [in] header the sparse header to populate with parent locator entries
 * \param [in] child_path is the full path to the VHD being created
 * \param [in] par_path is the full path to the parent image
 * \param [in] start_offset is the absolute file offset from where to start storing the entries themselves. Must be sector aligned.
 * \param [out] w2ku_buffer is a buffer containing the full path to the parent, encoded as UTF16-LE
 * \param [out] w2ru_buffer is a buffer containing the relative path to the parent, encoded as UTF16-LE
 * \param [out] err indicates what error occurred, if any
 * 
 * \retval 0 if success
 * \retval < 0 if an error occurrs. Check value of *err for actual error
 */
static int mvhd_gen_par_loc(MVHDSparseHeader* header, 
                            const char* child_path, 
                            const char* par_path, 
                            uint64_t start_offset, 
                            void* w2ku_buff,
                            void* w2ru_buff,
                            MVHDError* err) {
    /* Get our paths to store in the differencing VHD. We want both the absolute path to the parent,
       as well as the relative path from the child VHD */
    int rv = 0;
    char* par_filename;
    size_t par_fn_len;
    char rel_path[MVHD_MAX_PATH_BYTES] = {0};
    char child_dir[MVHD_MAX_PATH_BYTES] = {0};
    size_t child_dir_len;
    if (strlen(child_path) < sizeof child_dir) {
        strcpy(child_dir, child_path);
    } else {
        *err = MVHD_ERR_PATH_LEN;
        rv = -1;
        goto end;
    }
    cwk_path_get_basename(par_path, (const char**)&par_filename, &par_fn_len);
    cwk_path_get_dirname(child_dir, &child_dir_len);
    child_dir[child_dir_len] = '\0';
    size_t rel_len = cwk_path_get_relative(child_dir, par_path, rel_path, sizeof rel_path);
    if (rel_len > sizeof rel_path) {
        *err = MVHD_ERR_PATH_LEN;
        rv = -1;
        goto end;
    }
    /* We have our paths, now store the parent filename directly in the sparse header. */
    int outlen = sizeof header->par_utf16_name;
    int utf_ret;
    utf_ret = UTF8ToUTF16BE((unsigned char*)header->par_utf16_name, &outlen, (const unsigned char*)par_filename, (int*)&par_fn_len);
    if (utf_ret < 0) {
        mvhd_set_encoding_err(utf_ret, err);
        rv = -1;
        goto end;
    }
    /** 
     * Create output buffers to encode paths into. 
     * The paths are not stored directly in the sparse header, hence the need to 
     * store them in buffers to be written to the VHD image later
     */
    mvhd_utf16 *w2ku_path = calloc(MVHD_MAX_PATH_CHARS, sizeof *w2ku_path);
    if (w2ku_path == NULL) {
        *err = MVHD_ERR_MEM;
        rv = -1;
        goto end;
    }
    mvhd_utf16 *w2ru_path = calloc(MVHD_MAX_PATH_CHARS, sizeof *w2ru_path);
    if (w2ru_path == NULL) {
        *err = MVHD_ERR_MEM;
        rv = -1;
        goto cleanup_w2ku;
    }
    /* And encode the paths to UTF16-LE */
    size_t par_path_len = strlen(par_path);
    outlen = sizeof *w2ku_path * MVHD_MAX_PATH_CHARS;
    utf_ret = UTF8ToUTF16LE((unsigned char*)w2ku_path, &outlen, (const unsigned char*)par_path, (int*)&par_path_len);
    if (utf_ret < 0) {
        mvhd_set_encoding_err(utf_ret, err);
        rv = -1;
        goto cleanup_w2ru;
    }
    int w2ku_len = utf_ret;
    outlen = sizeof *w2ru_path * MVHD_MAX_PATH_CHARS;
    utf_ret = UTF8ToUTF16LE((unsigned char*)w2ru_path, &outlen, (const unsigned char*)rel_path, (int*)&rel_len);
    if (utf_ret < 0) {
        mvhd_set_encoding_err(utf_ret, err);
        rv = -1;
        goto cleanup_w2ru;
    }
    int w2ru_len = utf_ret;
    /** 
     * Finally populate the parent locaters in the sparse header.
     * This is the information needed to find the paths saved elsewhere
     * in the VHD image 
     */
    header->par_loc_entry[0].plat_code = MVHD_DIF_LOC_W2KU;
    header->par_loc_entry[0].plat_data_len = (uint32_t)w2ku_len;
    header->par_loc_entry[0].plat_data_offset = (uint64_t)start_offset;
    header->par_loc_entry[0].plat_data_space = (header->par_loc_entry[0].plat_data_len / MVHD_SECTOR_SIZE) + 1;
    header->par_loc_entry[1].plat_code = MVHD_DIF_LOC_W2RU;
    header->par_loc_entry[1].plat_data_len = (uint32_t)w2ru_len;
    header->par_loc_entry[1].plat_data_offset = (uint64_t)start_offset + header->par_loc_entry[0].plat_data_space;
    header->par_loc_entry[1].plat_data_space = (header->par_loc_entry[1].plat_data_len / MVHD_SECTOR_SIZE) + 1;
    goto end;

cleanup_w2ru:
    free(w2ru_path);
cleanup_w2ku:
    free(w2ku_path);
end:
    return rv;
}

MVHDMeta* mvhd_create_fixed(const char* path, MVHDGeom geom, int* pos, int* err) {
    return mvhd_create_fixed_raw(path, NULL, &geom, pos, err);
}

/**
 * \brief internal function that implements public mvhd_create_fixed() functionality
 * 
 * Contains one more parameter than the public function, to allow using an existing
 * raw disk image as the data source for the new fixed VHD.
 * 
 * \param [in] raw_image file handle to a raw disk image to populate VHD
 */
MVHDMeta* mvhd_create_fixed_raw(const char* path, FILE* raw_img, MVHDGeom* geom, int* pos, int* err) {
    uint8_t img_data[MVHD_SECTOR_SIZE] = {0};
    uint8_t footer_buff[MVHD_FOOTER_SIZE] = {0};
    MVHDMeta* vhdm = calloc(1, sizeof *vhdm);
    if (vhdm == NULL) {
        *err = MVHD_ERR_MEM;
        goto end;
    }
    if (raw_img == NULL && geom == NULL) {
        *err = MVHD_ERR_INVALID_GEOM;
        goto cleanup_vhdm;
    }
    if (geom != NULL && (geom->cyl == 0 || geom->heads == 0 || geom->spt == 0)) {
        *err = MVHD_ERR_INVALID_GEOM;
        goto cleanup_vhdm;
    }
    FILE* f = mvhd_fopen(path, "wb+", err);
    if (f == NULL) {
        goto cleanup_vhdm;
    }
    fseeko64(f, 0, SEEK_SET);
    uint32_t size_sectors, s;
    if (raw_img != NULL) {
        fseeko64(raw_img, 0, SEEK_END);
        uint64_t raw_size = (uint64_t)ftello64(raw_img);
        MVHDGeom raw_geom = mvhd_calculate_geometry(raw_size);
        if (mvhd_calc_size_bytes(&raw_geom) != raw_size) {
            *err = MVHD_ERR_CONV_SIZE;
            goto cleanup_vhdm;
        }
        mvhd_gen_footer(&vhdm->footer, geom, MVHD_TYPE_FIXED, 0);
        size_sectors = mvhd_calc_size_sectors(geom);
        fseeko64(raw_img, 0, SEEK_SET);
        for (s = 0; s < size_sectors; s++) {
            if (pos != NULL) {
                *pos = (int)s;
            }
            fread(img_data, sizeof img_data, 1, raw_img);
            fwrite(img_data, sizeof img_data, 1, f);
        }
    } else {
        mvhd_gen_footer(&vhdm->footer, geom, MVHD_TYPE_FIXED, 0);
        size_sectors = mvhd_calc_size_sectors(geom);
        for (s = 0; s < size_sectors; s++) {
            if (pos != NULL) {
                *pos = (int)s;
            }
            fwrite(img_data, sizeof img_data, 1, f);
        }
    }
    mvhd_footer_to_buffer(&vhdm->footer, footer_buff);
    fwrite(footer_buff, sizeof footer_buff, 1, f);
    fclose(f);
    f = NULL;
    free(vhdm);
    vhdm = mvhd_open(path, false, err);
    goto end;

cleanup_vhdm:
    free(vhdm);
    vhdm = NULL;
end:
    return vhdm;
}

/**
 * \brief Create sparse or differencing VHD image.
 * 
 * \param [in] path is the absolute path to the VHD file to create
 * \param [in] par_path is the absolute path to a parent image. If NULL, a sparse image is created, otherwise create a differencing image
 * \param [in] geom is the HDD geometry of the image to create. Determines final image size
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns pointer to a MVHDMeta struct
 */
static MVHDMeta* mvhd_create_sparse_diff(const char* path, const char* par_path, MVHDGeom* geom, int* err) {
    uint8_t footer_buff[MVHD_FOOTER_SIZE] = {0};
    uint8_t sparse_buff[MVHD_SPARSE_SIZE] = {0};
    uint8_t bat_sect[MVHD_SECTOR_SIZE];
    MVHDGeom par_geom = {};
    memset(bat_sect, 0xffffffff, sizeof bat_sect);
    MVHDMeta* par_vhdm = NULL;
    if (par_path != NULL) {
        par_vhdm = mvhd_open(par_path, true, err);
        if (par_vhdm == NULL) {
            goto end;
        }
    }
    MVHDMeta* vhdm = calloc(1, sizeof *vhdm);
    if (vhdm == NULL) {
        *err = MVHD_ERR_MEM;
        goto cleanup_par_vhdm;
    }
    if (par_vhdm != NULL) {
        /* We use the geometry from the parent VHD, not what was passed in */
        par_geom.cyl = par_vhdm->footer.geom.cyl;
        par_geom.heads = par_vhdm->footer.geom.heads;
        par_geom.spt = par_vhdm->footer.geom.spt;
        geom = &par_geom;
    } else if (geom != NULL && (geom->cyl == 0 || geom->heads == 0 || geom->spt == 0)) {
        *err = MVHD_ERR_INVALID_GEOM;
        goto cleanup_vhdm;
    } else if (geom == NULL) {
        *err = MVHD_ERR_INVALID_GEOM;
        goto cleanup_vhdm;
    }
    FILE* f = mvhd_fopen(path, "wb+", err);
    if (f == NULL) {
        goto cleanup_vhdm;
    }
    fseeko64(f, 0, SEEK_SET);
    /* Note, the sparse header follows the footer copy at the beginning of the file */
    if (par_path == NULL) {
        mvhd_gen_footer(&vhdm->footer, geom, MVHD_TYPE_DYNAMIC, MVHD_FOOTER_SIZE);
    } else {
        mvhd_gen_footer(&vhdm->footer, geom, MVHD_TYPE_DIFF, MVHD_FOOTER_SIZE);
    }
    mvhd_footer_to_buffer(&vhdm->footer, footer_buff);
    /* As mentioned, start with a copy of the footer */
    fwrite(footer_buff, sizeof footer_buff, 1, f);
    /**
     * Calculate the number of (2MB) data blocks required to store the entire
     * contents of the disk image, followed by the number of sectors the 
     * BAT occupies in the image. Note, the BAT is sector aligned, and is padded
     * to the next sector boundary 
     * */
    uint32_t num_blks = mvhd_calc_size_sectors(geom) / MVHD_SECT_PER_BLOCK;
    if (mvhd_calc_size_sectors(geom) % MVHD_SECT_PER_BLOCK != 0) {
        num_blks += 1;
    }
    uint32_t num_bat_sect = num_blks / MVHD_BAT_ENT_PER_SECT;
    if (num_blks % MVHD_BAT_ENT_PER_SECT != 0) {
        num_bat_sect += 1;
    }
    /* Storing the BAT directly following the footer and header */
    uint64_t bat_offset = MVHD_FOOTER_SIZE + MVHD_SPARSE_SIZE;
    uint64_t par_loc_offset = 0;
    uint8_t *w2ku_path, *w2ru_path;
    w2ku_path = w2ru_path = NULL;
    /**
     * If creating a differencing VHD, populate the sparse header with additional 
     * data about the parent image, and where to find it
     * */
    if (par_vhdm != NULL) {
        memcpy(vhdm->sparse.par_uuid, par_vhdm->footer.uuid, sizeof vhdm->sparse.par_uuid);
        par_loc_offset = bat_offset + (num_bat_sect * MVHD_SECTOR_SIZE) + (5 * MVHD_SECTOR_SIZE);
        if (mvhd_gen_par_loc(&vhdm->sparse, path, par_path, par_loc_offset, w2ku_path, w2ru_path, err) < 0) {
            goto cleanup_vhdm;
        }
    }
    mvhd_gen_sparse_header(&vhdm->sparse, num_blks, bat_offset);
    mvhd_header_to_buffer(&vhdm->sparse, sparse_buff);
    fwrite(sparse_buff, sizeof sparse_buff, 1, f);
    /* The BAT sectors need to be filled with 0xffffffff */
    for (uint32_t i = 0; i < num_bat_sect; i++) {
        fwrite(bat_sect, sizeof bat_sect, 1, f);
    }
    mvhd_write_empty_sectors(f, 5);
    /**
     * If creating a differencing VHD, the paths to the parent image need to be written
     * tp the file. Both absolute and relative paths are written 
     * */
    if (par_vhdm != NULL) {
        uint64_t curr_pos = (uint64_t)ftello64(f);
        /* Double check my sums... */
        assert(curr_pos == par_loc_offset);
        /* Fill the space required for location data with zero */
        uint8_t empty_sect[MVHD_SECTOR_SIZE] = {0};
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < vhdm->sparse.par_loc_entry[i].plat_data_space; j++) {
                fwrite(empty_sect, sizeof empty_sect, 1, f);
            }
        }
        /* Now write the location entries */
        fseeko64(f, vhdm->sparse.par_loc_entry[0].plat_data_offset, SEEK_SET);
        fwrite(w2ku_path, vhdm->sparse.par_loc_entry[0].plat_data_len, 1, f);
        fseeko64(f, vhdm->sparse.par_loc_entry[1].plat_data_offset, SEEK_SET);
        fwrite(w2ru_path, vhdm->sparse.par_loc_entry[1].plat_data_len, 1, f);
        /* and reset the file position to continue */
        fseeko64(f, vhdm->sparse.par_loc_entry[1].plat_data_offset + (vhdm->sparse.par_loc_entry[1].plat_data_space * MVHD_SECTOR_SIZE), SEEK_SET);
        mvhd_write_empty_sectors(f, 5);
    }
    /* And finish with the footer */
    fwrite(footer_buff, sizeof footer_buff, 1, f);
    fclose(f);
    f = NULL;
    free(vhdm);
    vhdm = mvhd_open(path, false, err);
    goto end;

cleanup_vhdm:
    free(vhdm);
    vhdm = NULL;
cleanup_par_vhdm:
    if (par_vhdm != NULL) {
        mvhd_close(par_vhdm);
    }
end:
    return vhdm;
}

MVHDMeta* mvhd_create_sparse(const char* path, MVHDGeom geom, int* err) {
    return mvhd_create_sparse_diff(path, NULL, &geom, err);
}

MVHDMeta* mvhd_create_diff(const char* path, const char* par_path, int* err) {
    return mvhd_create_sparse_diff(path, par_path, NULL, err);
}