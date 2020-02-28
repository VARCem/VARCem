#ifndef MINIVHD_H
#define MINIVHD_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

extern int mvhd_errno;

typedef enum MVHDError {
    MVHD_ERR_MEM = -128,
    MVHD_ERR_FILE,
    MVHD_ERR_NOT_VHD,
    MVHD_ERR_TYPE,
    MVHD_ERR_FOOTER_CHECKSUM,
    MVHD_ERR_SPARSE_CHECKSUM,
    MVHD_ERR_UTF_TRANSCODING_FAILED,
    MVHD_ERR_UTF_SIZE,
    MVHD_ERR_PATH_REL,
    MVHD_ERR_PATH_LEN,
    MVHD_ERR_PAR_NOT_FOUND,
    MVHD_ERR_INVALID_PAR_UUID,
    MVHD_ERR_INVALID_GEOM,
    MVHD_ERR_INVALID_PARAMS,
    MVHD_ERR_CONV_SIZE
} MVHDError;

typedef struct MVHDGeom {
    uint16_t cyl;
    uint8_t heads;
    uint8_t spt;
} MVHDGeom;

typedef struct MVHDMeta MVHDMeta;

/**
 * \brief Output a string from a MiniVHD error number
 * 
 * \param [in] err is the error number to return string from
 * 
 * \return Error string
 */
const char* mvhd_strerr(MVHDError err);

/**
 * \brief A simple test to see if a given file is a VHD
 * 
 * \param [in] f file to test
 * 
 * \retval true if f is a VHD
 * \retval false if f is not a VHD
 */
bool mvhd_file_is_vhd(FILE* f);

/**
 * \brief Open a VHD image for reading and/or writing
 * 
 * The returned pointer contains all required values and structures (and files) to 
 * read and write to a VHD file.
 * 
 * Remember to call mvhd_close() when you are finished.
 * 
 * \param [in] Absolute path to VHD file. Relative path will cause issues when opening
 * a differencing VHD file
 * \param [in] readonly set this to true to open the VHD in a read only manner
 * \param [out] err will be set if the VHD fails to open. Value could be one of 
 * MVHD_ERR_MEM, MVHD_ERR_FILE, MVHD_ERR_NOT_VHD, MVHD_ERR_FOOTER_CHECKSUM, MVHD_ERR_SPARSE_CHECKSUM, 
 * MVHD_ERR_TYPE
 * If MVHD_ERR_FILE is set, mvhd_errno will be set to the appropriate system errno value
 * 
 * \return MVHDMeta pointer. If NULL, check err.
 */
MVHDMeta* mvhd_open(const char* path, bool readonly, int* err);

/**
 * \brief Create a fixed VHD image
 * 
 * \param [in] path is the absolute path to the image to create
 * \param [in] geom is the HDD geometry of the image to create. Determines final image size
 * \param [out] pos stores the current sector being written to disk
 * \param [out] err indicates what error occurred, if any
 * 
 * \retval 0 if success
 * \retval < 0 if an error occurrs. Check value of *err for actual error
 */
MVHDMeta* mvhd_create_fixed(const char* path, MVHDGeom geom, int* pos, int* err);

/**
 * \brief Create sparse (dynamic) VHD image.
 * 
 * \param [in] path is the absolute path to the VHD file to create
 * \param [in] geom is the HDD geometry of the image to create. Determines final image size
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns pointer to a MVHDMeta struct
 */
MVHDMeta* mvhd_create_sparse(const char* path, MVHDGeom geom, int* err);

/**
 * \brief Create differencing VHD imagee.
 * 
 * \param [in] path is the absolute path to the VHD file to create
 * \param [in] par_path is the absolute path to a parent image. If NULL, a sparse image is created, otherwise create a differencing image
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns pointer to a MVHDMeta struct
 */
MVHDMeta* mvhd_create_diff(const char* path, const char* par_path, int* err);

/**
 * \brief Safely close a VHD image
 * 
 * \param [in] vhdm MiniVHD data structure to close
 */
void mvhd_close(MVHDMeta* vhdm);

/**
 * \brief Calculate hard disk geometry from a provided size
 * 
 * The VHD format uses Cylinder, Heads, Sectors per Track (CHS) when accessing the disk.
 * The size of the disk can be determined from C * H * S * sector_size.
 * 
 * Note, maximum VHD size (in bytes) is 65535 * 16 * 255 * 512, which is 127GB
 * 
 * This function determines the appropriate CHS geometry from a provided size in bytes.
 * The calculations used are those provided in "Appendix: CHS Calculation" from the document 
 * "Virtual Hard Disk Image Format Specification" provided by Microsoft.
 * 
 * \param [in] size the desired VHD image size, in bytes
 * 
 * \return MVHDGeom the calculated geometry. This can be used in the appropriate create functions.
 */
MVHDGeom mvhd_calculate_geometry(uint64_t size);

/**
 * \brief Convert a raw disk image to a fixed VHD image
 * 
 * \param [in] utf8_raw_path is the path of the raw image to convert
 * \param [in] utf8_vhd_path is the path of the VHD to create
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns pointer to a MVHDMeta struct
 */
MVHDMeta* mvhd_convert_to_vhd_fixed(const char* utf8_raw_path, const char* utf8_vhd_path, int* err);

/**
 * \brief Convert a raw disk image to a sparse VHD image
 * 
 * \param [in] utf8_raw_path is the path of the raw image to convert
 * \param [in] utf8_vhd_path is the path of the VHD to create
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns pointer to a MVHDMeta struct
 */
MVHDMeta* mvhd_convert_to_vhd_sparse(const char* utf8_raw_path, const char* utf8_vhd_path, int* err);

/**
 * \brief Convert a VHD image to a raw disk image
 * 
 * \param [in] utf8_vhd_path is the path of the VHD to convert
 * \param [in] utf8_raw_path is the path of the raw image to create
 * \param [out] err indicates what error occurred, if any
 * 
 * \return NULL if an error occurrs. Check value of *err for actual error. Otherwise returns the raw disk image FILE pointer
 */
FILE* mvhd_convert_to_raw(const char* utf8_vhd_path, const char* utf8_raw_path, int *err);

/**
 * \brief Read sectors from VHD file
 * 
 * Read num_sectors, beginning at offset from the VHD file into a buffer
 * 
 * \param [in] vhdm MiniVHD data structure
 * \param [in] offset the sector offset from which to start reading from
 * \param [in] num_sectors the number of sectors to read
 * \param [out] out_buff the buffer to write sector data to
 * 
 * \return the number of sectors that were not read, or zero
 */
int mvhd_read_sectors(MVHDMeta* vhdm, int offset, int num_sectors, void* out_buff);

/**
 * \brief Write sectors to VHD file
 * 
 * Write num_sectors, beginning at offset from a buffer VHD file into the VHD file
 * 
 * \param [in] vhdm MiniVHD data structure
 * \param [in] offset the sector offset from which to start writing to
 * \param [in] num_sectors the number of sectors to write
 * \param [in] in_buffer the buffer to write sector data to
 * 
 * \return the number of sectors that were not written, or zero
 */
int mvhd_write_sectors(MVHDMeta* vhdm, int offset, int num_sectors, void* in_buff);

/**
 * \brief Write zeroed sectors to VHD file
 * 
 * Write num_sectors, beginning at offset, of zero data into the VHD file. 
 * We reuse the existing write functions, with a preallocated zero buffer as 
 * our source buffer.
 * 
 * \param [in] vhdm MiniVHD data structure
 * \param [in] offset the sector offset from which to start writing to
 * \param [in] num_sectors the number of sectors to write
 * 
 * \return the number of sectors that were not written, or zero
 */
int mvhd_format_sectors(MVHDMeta* vhdm, int offset, int num_sectors);
#endif