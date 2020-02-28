#ifndef MINIVHD_CREATE_H
#define MINIVHD_CREATE_H
#include <stdio.h>
#include "minivhd.h"

MVHDMeta* mvhd_create_fixed_raw(const char* path, FILE* raw_img, MVHDGeom* geom, int* pos, int* err);

#endif