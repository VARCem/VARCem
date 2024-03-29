/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define all known processor types.
 *
 * Version:	@(#)cpu_table.c	1.0.14	2020/12/11
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "cpu.h"


const CPU cpus_8088[] = {
    /* 8088 */
    { "8088/4.77",			CPU_8088,	   4772728, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8088/7.16",			CPU_8088,	   7159092, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8088/8",				CPU_8088,	   8000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 }, 
    { "8088/9.54",			CPU_8088,	   9545456, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 }, 
    { "8088/10",			CPU_8088,	  10000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8088/12",			CPU_8088,	  12000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8088/16",			CPU_8088,	  16000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
#if defined(WALTJE) && defined(_DEBUG)
    { "286/6",				CPU_286,	   6000000, 1, 0,	 0,	 0,	 0,	 0, 2,2,2,2, 1 },
#endif
    { NULL }
};

const CPU cpus_8086[] = {
    /* 8086 */
    { "8086/7.16",			CPU_8086,	   7159092, 1, 0,	 0,	 0,	 0,	 CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8086/8",				CPU_8086,	   8000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8086/9.54",			CPU_8086,	   9545456, 1, 0,	 0,	 0,	 0,	 CPU_ALTERNATE_XTAL, 0,0,0,0, 1 },
    { "8086/10",			CPU_8086,	  10000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8086/12",			CPU_8086,	  12000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "8086/16",			CPU_8086,	  16000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 2 },
    { NULL }
};

const CPU cpus_nec[] = {
    /* NEC V20/30 */
    { "V20/8",				CPU_NEC,	   8000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "V20/10",				CPU_NEC,	  10000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "V20/12",				CPU_NEC,	  12000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "V20/16",				CPU_NEC,	  16000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 2 },
    { NULL }
};

const CPU cpus_186[] = {
    /* 80186 */
    { "80186/7.16",			CPU_186,	   7159092, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "80186/8",			CPU_186,	   8000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "80186/9.54",			CPU_186,	   9545456, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "80186/10",			CPU_186,	  10000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "80186/12",			CPU_186,	  12000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 1 },
    { "80186/16",			CPU_186,	  16000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 2 },
    { "80186/20",			CPU_186,	  20000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 3 },
    { "80186/25",			CPU_186,	  25000000, 1, 0,	 0,	 0,	 0,	 0, 0,0,0,0, 3 },
    { NULL }
};

const CPU cpus_286[] = {
    /* 80286 */
    { "286/6",				CPU_286,	   6000000, 1, 0,	 0,	 0,	 0,	 0, 2,2,2,2, 1 },
    { "286/8",				CPU_286,	   8000000, 1, 0,	 0,	 0,	 0,	 0, 2,2,2,2, 1 },
    { "286/10",				CPU_286,	  10000000, 1, 0,	 0,	 0,	 0,	 0, 2,2,2,2, 1 },
    { "286/12",				CPU_286,	  12000000, 1, 0,	 0,	 0,	 0,	 0, 3,3,3,3, 2 },
    { "286/16",				CPU_286,	  16000000, 1, 0,	 0,	 0,	 0,	 0, 3,3,3,3, 2 },
    { "286/20",				CPU_286,	  20000000, 1, 0,	 0,	 0,	 0,	 0, 4,4,4,4, 3 },
    { "286/25",				CPU_286,	  25000000, 1, 0,	 0,	 0,	 0,	 0, 4,4,4,4, 3 },
#if defined(WALTJE) && defined(_DEBUG)
    { "286/100",			CPU_286,	 100000000, 1, 0,	 0,	 0,	 0,	 0, 4,4,4,4, 3 },
#endif
    { NULL }
};

const CPU cpus_i386SX[] = {
    /* i386SX */
    { "i386SX/16",			CPU_386SX,	  16000000, 1, 0,	 0x2308, 0,	 0,	 0, 3,3,3,3, 2 },
    { "i386SX/20",			CPU_386SX,	  20000000, 1, 0,	 0x2308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "i386SX/25",			CPU_386SX,	  25000000, 1, 0,	 0x2308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "i386SX/33",			CPU_386SX,	  33333333, 1, 0,	 0x2308, 0,	 0,	 0, 6,6,3,3, 4 },
    { "i386SX/40",			CPU_386SX,	  40000000, 1, 0,	 0x2308, 0,	 0,	 0, 7,7,3,3, 5 },
    { NULL }
};

const CPU cpus_i386DX[] = {
    /* i386DX */
    { "i386DX/16",			CPU_386DX,	  16000000, 1, 0,	 0x0308, 0,	 0,	 0, 3,3,3,3, 2 },
    { "i386DX/20",			CPU_386DX,	  20000000, 1, 0,	 0x0308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "i386DX/25",			CPU_386DX,	  25000000, 1, 0,	 0x0308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "i386DX/33",			CPU_386DX,	  33333333, 1, 0,	 0x0308, 0,	 0,	 0, 6,6,3,3, 4 },
    { "i386DX/40",			CPU_386DX,	  40000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
#if defined(WALTJE) && defined(_DEBUG)
    { "i386DX/100",			CPU_386DX,	 100000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
    { "i386DX/200",			CPU_386DX,	 200000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
#endif
    { "RapidCAD/25",			CPU_RAPIDCAD,	  25000000, 1, 0,	 0x0430, 0,	 0,	 0, 4,4,3,3, 3 },
    { "RapidCAD/33",			CPU_RAPIDCAD,	  33333333, 1, 0,	 0x0430, 0,	 0,	 0, 6,6,3,3, 4 },
    { "RapidCAD/40",			CPU_RAPIDCAD,	  40000000, 1, 0,	 0x0430, 0,	 0,	 0, 7,7,3,3, 5 },
    { NULL }
};

const CPU cpus_Am386SX[] = {
    /* Am386 */
    { "Am386SX/16",			CPU_386SX,	  16000000, 1, 0,	 0x2308, 0,	 0,	 0, 3,3,3,3, 2 },
    { "Am386SX/20",			CPU_386SX,	  20000000, 1, 0,	 0x2308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "Am386SX/25",			CPU_386SX,	  25000000, 1, 0,	 0x2308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "Am386SX/33",			CPU_386SX,	  33333333, 1, 0,	 0x2308, 0,	 0,	 0, 6,6,3,3, 4 },
    { "Am386SX/40",			CPU_386SX,	  40000000, 1, 0,	 0x2308, 0,	 0,	 0, 7,7,3,3, 5 },
    { NULL }
};

const CPU cpus_Am386DX[] = {
    /* Am386 */
    { "Am386DX/25",			CPU_386DX,	  25000000, 1, 0,	 0x0308, 0,	 0,	 0, 4,4,3,3, 3 },
    { "Am386DX/33",			CPU_386DX,	  33333333, 1, 0,	 0x0308, 0,	 0,	 0, 6,6,3,3, 4 },
    { "Am386DX/40",			CPU_386DX,	  40000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
#if defined(WALTJE) && defined(_DEBUG)
    { "Am386DX/100",			CPU_386DX,	 100000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
    { "Am386DX/200",			CPU_386DX,	 200000000, 1, 0,	 0x0308, 0,	 0,	 0, 7,7,3,3, 5 },
#endif
    { NULL }
};

const CPU cpus_486SLC[] = {
    /* Cx486SLC */
    { "Cx486SLC/20",			CPU_486SLC,	  20000000, 1, 0,	 0x0400, 0,	 0x0000, 0, 4,4,3,3, 3 },
    { "Cx486SLC/25",			CPU_486SLC,	  25000000, 1, 0,	 0x0400, 0,	 0x0000, 0, 4,4,3,3, 3 },
    { "Cx486SLC/33",			CPU_486SLC,	  33333333, 1, 0,	 0x0400, 0,	 0x0000, 0, 6,6,3,3, 4 },
    { "Cx486SRx2/32",			CPU_486SLC,	  32000000, 2, 0,	 0x0406, 0,	 0x0006, 0, 6,6,6,6, 4 },
    { "Cx486SRx2/40",			CPU_486SLC,	  40000000, 2, 0,	 0x0406, 0,	 0x0006, 0, 8,8,6,6, 6 },
    { "Cx486SRx2/50",			CPU_486SLC,	  50000000, 2, 0,	 0x0406, 0,	 0x0006, 0, 8,8,6,6, 6 },
    { NULL }
};

const CPU cpus_486DLC[] = {
    /* Cx486DLC */
    { "Cx486DLC/25",			CPU_486DLC,	  25000000, 1, 0,	 0x0401, 0,	 0x0001, 0, 4,4,3,3, 3 },
    { "Cx486DLC/33",			CPU_486DLC,	  33333333, 1, 0,	 0x0401, 0,	 0x0001, 0, 6,6,3,3, 4 },
    { "Cx486DLC/40",			CPU_486DLC,	  40000000, 1, 0,	 0x0401, 0,	 0x0001, 0, 7,7,3,3, 5 },
    { "Cx486DRx2/32",			CPU_486DLC,	  32000000, 2, 0,	 0x0407, 0,	 0x0007, 0, 6,6,6,6, 4 },
    { "Cx486DRx2/40",			CPU_486DLC,	  40000000, 2, 0,	 0x0407, 0,	 0x0007, 0, 8,8,6,6, 6 },
    { "Cx486DRx2/50",			CPU_486DLC,	  50000000, 2, 0,	 0x0407, 0,	 0x0007, 0, 8,8,6,6, 6 },
    { "Cx486DRx2/66",			CPU_486DLC,	  66666666, 2, 0,	 0x0407, 0,	 0x0007, 0, 12,12,6,6, 8 },
    { NULL }
};

const CPU cpus_i486[] = {
    /* i486 */
    { "i486SX/16",			CPU_i486SX,	  16000000, 1, 16000000, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 3,3,3,3, 2 },
    { "i486SX/20",			CPU_i486SX,	  20000000, 1, 20000000, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3 },
    { "i486SX/25",			CPU_i486SX,	  25000000, 1, 25000000, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3 },
    { "i486SX/33",			CPU_i486SX,	  33333333, 1, 33333333, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "i486SX2/50",			CPU_i486SX,	  50000000, 2, 25000000, 0x045b, 0x045b, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 },
    { "i486SX2/66 (Q0569)",		CPU_i486SX,	  66666666, 2, 33333333, 0x045b, 0x045b, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 8 },
    { "i486DX/25",			CPU_i486DX,	  25000000, 1, 25000000, 0x0404, 0,      0,      CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3 },
    { "i486DX/33",			CPU_i486DX,	  33333333, 1, 33333333, 0x0404, 0,      0,      CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "i486DX/50",			CPU_i486DX,	  50000000, 1, 25000000, 0x0404, 0,      0,      CPU_SUPPORTS_DYNAREC, 8,8,4,4, 6 },
    { "i486DX2/40",			CPU_i486DX,	  40000000, 2, 20000000, 0x0430, 0x0430, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 },
    { "i486DX2/50",			CPU_i486DX,	  50000000, 2, 25000000, 0x0430, 0x0430, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 },
    { "i486DX2/66",			CPU_i486DX,	  66666666, 2, 33333333, 0x0430, 0x0430, 0,      CPU_SUPPORTS_DYNAREC, 12,12,6,6, 8 },
    { "i486DX4/75",			CPU_iDX4,	  75000000, 3, 25000000, 0x0481, 0x0481, 0,      CPU_SUPPORTS_DYNAREC, 12,12,9,9, 9 }, /*CPUID available on DX4, >= 75 MHz*/
    { "i486DX4/100",			CPU_iDX4,        100000000, 3, 33333333, 0x0481, 0x0481, 0,      CPU_SUPPORTS_DYNAREC, 18,18,9,9, 12 }, /*Is on some real Intel DX2s, limit here is pretty arbitary*/
    { "Pentium OverDrive/63",		CPU_PENTIUM,      62500000, 3, 25000000, 0x1531, 0x1531, 0,      CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,7,7, 15/2 },
    { "Pentium OverDrive/83",		CPU_PENTIUM,      83333333, 3, 33333333, 0x1532, 0x1532, 0,      CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,8,8, 10 },
    { NULL }
};

const CPU cpus_Am486[] = {
    /* Am486/5x86 */
    { "Am486SX/33",			CPU_Am486SX,	  33333333, 1, 33333333, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "Am486SX/40",			CPU_Am486SX,	  40000000, 1, 20000000, 0x042a, 0,      0,      CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5 },
    { "Am486SX2/50",			CPU_Am486SX,	  50000000, 2, 25000000, 0x045b, 0x045b, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 }, /*CPUID available on SX2, DX2, DX4, 5x86, >= 50 MHz*/
    { "Am486SX2/66",			CPU_Am486SX,	  66666666, 2, 33333333, 0x045b, 0x045b, 0,      CPU_SUPPORTS_DYNAREC, 12,12,6,6, 8 }, /*Isn't on all real AMD SX2s and DX2s, availability here is pretty arbitary (and distinguishes them from the Intel chips)*/
    { "Am486DX/33",			CPU_Am486DX,	  33333333, 1, 33333333, 0x0430, 0,      0,      CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "Am486DX/40",			CPU_Am486DX,	  40000000, 1, 20000000, 0x0430, 0,      0,      CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5 },
    { "Am486DX2/50",			CPU_Am486DX,	  50000000, 2, 25000000, 0x0470, 0x0470, 0,      CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 },
    { "Am486DX2/66",			CPU_Am486DX,	  66666666, 2, 33333333, 0x0470, 0x0470, 0,      CPU_SUPPORTS_DYNAREC, 12,12,6,6, 8 },
    { "Am486DX2/80",			CPU_Am486DX,	  80000000, 2, 20000000, 0x0470, 0x0470, 0,      CPU_SUPPORTS_DYNAREC, 14,14,6,6, 10 },
    { "Am486DX4/75",			CPU_Am486DX,	  75000000, 3, 25000000, 0x0482, 0x0482, 0,      CPU_SUPPORTS_DYNAREC, 12,12,9,9, 9 },
    { "Am486DX4/90",			CPU_Am486DX,	  90000000, 3, 30000000, 0x0482, 0x0482, 0,      CPU_SUPPORTS_DYNAREC, 15,15,9,9, 12 },
    { "Am486DX4/100",			CPU_Am486DX,	 100000000, 3, 33333333, 0x0482, 0x0482, 0,      CPU_SUPPORTS_DYNAREC, 15,15,9,9, 12 },
    { "Am486DX4/120",			CPU_Am486DX,	 120000000, 3, 20000000, 0x0482, 0x0482, 0,      CPU_SUPPORTS_DYNAREC, 21,21,9,9, 15 },
    { "Am5x86/P75",			CPU_Am486DX,	 133333333, 4, 33333333, 0x04e0, 0x04e0, 0,      CPU_SUPPORTS_DYNAREC, 24,24,12,12, 16 },
    { "Am5x86/P75+",			CPU_Am486DX,	 160000000, 4, 20000000, 0x04e0, 0x04e0, 0,      CPU_SUPPORTS_DYNAREC, 28,28,12,12, 20 },
    { NULL }
};

const CPU cpus_Cx486[] = {
    /* Cx486/5x86 */
    { "Cx486S/25",			CPU_Cx486S,	  25000000, 1, 25000000, 0x0420, 0,      0x0010, CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3 },
    { "Cx486S/33",			CPU_Cx486S,	  33333333, 1, 33333333, 0x0420, 0,      0x0010, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "Cx486S/40",			CPU_Cx486S,	  40000000, 1, 20000000, 0x0420, 0,      0x0010, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5 },
    { "Cx486DX/33",			CPU_Cx486DX,	  33333333, 1, 33333333, 0x0430, 0,      0x051a, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4 },
    { "Cx486DX/40",			CPU_Cx486DX,	  40000000, 1, 20000000, 0x0430, 0,      0x051a, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5 },
    { "Cx486DX2/50",			CPU_Cx486DX,	  50000000, 2, 25000000, 0x0430, 0,      0x081b, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 6 },
    { "Cx486DX2/66",			CPU_Cx486DX,	  66666666, 2, 33333333, 0x0430, 0,      0x0b1b, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 8 },
    { "Cx486DX2/80",			CPU_Cx486DX,	  80000000, 2, 20000000, 0x0430, 0,      0x311b, CPU_SUPPORTS_DYNAREC, 14,14,16,16, 10 },
    { "Cx486DX4/75",			CPU_Cx486DX,	  75000000, 3, 25000000, 0x0480, 0,      0x361f, CPU_SUPPORTS_DYNAREC, 12,12,9,9, 9 },
    { "Cx486DX4/100",			CPU_Cx486DX,	 100000000, 3, 33333333, 0x0480, 0,      0x361f, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 12 },
    { "Cx5x86/100",			CPU_Cx5x86,	 100000000, 3, 33333333, 0x0480, 0,      0x002f, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 12 },
    { "Cx5x86/120",			CPU_Cx5x86,	 120000000, 3, 20000000, 0x0480, 0,      0x002f, CPU_SUPPORTS_DYNAREC, 21,21,9,9, 15 },
    { "Cx5x86/133",			CPU_Cx5x86,	 133333333, 4, 33333333, 0x0480, 0,      0x002f, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 16 },
    { NULL }
};

const CPU cpus_6x86[] = {
    /* Cyrix 6x86 */
    { "6x86-P90",			CPU_Cx6x86,	  80000000, 3, 40000000, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 8,8,6,6, 10 },
    { "6x86-PR120+",			CPU_Cx6x86,	 100000000, 3, 25000000, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 12 },
    { "6x86-PR133+",			CPU_Cx6x86,	 110000000, 3, 27500000, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 14 },
    { "6x86-PR150+",			CPU_Cx6x86,	 120000000, 3, 30000000, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "6x86-PR166+",			CPU_Cx6x86,	 133333333, 3, 33333333, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "6x86-PR200+",			CPU_Cx6x86,	 150000000, 3, 37500000, 0x0520, 0x0520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 18 },

    /* Cyrix 6x86L */
    { "6x86L-PR133+",			CPU_Cx6x86L,	 110000000, 3, 27500000, 0x0540, 0x0540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 14 },
    { "6x86L-PR150+",			CPU_Cx6x86L,	 120000000, 3, 30000000, 0x0540, 0x0540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "6x86L-PR166+",			CPU_Cx6x86L,	 133333333, 3, 33333333, 0x0540, 0x0540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "6x86L-PR200+",			CPU_Cx6x86L,	 150000000, 3, 37500000, 0x0540, 0x0540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 18 },

    /* Cyrix 6x86MX */
    { "6x86MX-PR166",			CPU_Cx6x86MX,	 133333333, 3, 33333333, 0x0600, 0x0600, 0x0451, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "6x86MX-PR200",			CPU_Cx6x86MX,	 166666666, 3, 33333333, 0x0600, 0x0600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "6x86MX-PR233",			CPU_Cx6x86MX,	 188888888, 3, 37500000, 0x0600, 0x0600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 45/2 },
    { "6x86MX-PR266",			CPU_Cx6x86MX,	 207500000, 3, 41666667, 0x0600, 0x0600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7, 25 },
    { "6x86MX-PR300",			CPU_Cx6x86MX,	 233333333, 3, 33333333, 0x0600, 0x0600, 0x0454, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,7,7, 28 },
    { "6x86MX-PR333",			CPU_Cx6x86MX,	 250000000, 3, 41666667, 0x0600, 0x0600, 0x0453, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 20,20,9,9, 30 },
    { "6x86MX-PR366",			CPU_Cx6x86MX,	 250000000, 3, 33333333, 0x0600, 0x0600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 30 },
    { "6x86MX-PR400",			CPU_Cx6x86MX,	 285000000, 3, 41666667, 0x0600, 0x0600, 0x0453, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 33 },
    { NULL }
};

const CPU cpus_WinChip[] = {
    /* IDT WinChip */
    { "WinChip 75",			CPU_WINCHIP,	  75000000, 2, 25000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 8,8,4,4, 9 },
    { "WinChip 90",			CPU_WINCHIP,	  90000000, 2, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4, 21/2 },
    { "WinChip 100",			CPU_WINCHIP,	 100000000, 2, 33333333, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4, 12 },
    { "WinChip 120",			CPU_WINCHIP,	 120000000, 2, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 14 },
    { "WinChip 133",			CPU_WINCHIP,	 133333333, 2, 33333333, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 16 },
    { "WinChip 150",			CPU_WINCHIP,	 150000000, 3, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7, 35/2 },
    { "WinChip 166",			CPU_WINCHIP,	 166666666, 3, 33333333, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7, 40 },
    { "WinChip 180",			CPU_WINCHIP,	 180000000, 3, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 21 },
    { "WinChip 200",			CPU_WINCHIP,	 200000000, 3, 33333333, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 225",			CPU_WINCHIP,	 225000000, 3, 37500000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 27 },
    { "WinChip 240",			CPU_WINCHIP,	 240000000, 6, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 28 },
    { "WinChip 2/200",		   CPU_WINCHIP2,	 200000000, 3, 33333333, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 2/240",		   CPU_WINCHIP2,	 240000000, 6, 30000000, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 28 },
    { "WinChip 2A/200",        CPU_WINCHIP2,     200000000, 3, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 2A/233",        CPU_WINCHIP2,     233333333, 3, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, (7*8)/2 },
    { NULL }
};

const CPU cpus_WinChip_SS7[] = {
    /* IDT WinChip (Super Socket 7) */
    { "WinChip 200",			CPU_WINCHIP,	 200000000, 3, 33333333, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 225",			CPU_WINCHIP,	 225000000, 3, 37500000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 27 },
    { "WinChip 240",			CPU_WINCHIP,	 240000000, 6, 30000000, 0x0540, 0x0540, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 28 },
    { "WinChip 2/200",		   CPU_WINCHIP2,	 200000000, 3, 33333333, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 2/225",		   CPU_WINCHIP2,	 225000000, 3, 37500000, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 27 },
    { "WinChip 2/240",		   CPU_WINCHIP2,	 240000000, 6, 30000000, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30 },
    { "WinChip 2/250",		   CPU_WINCHIP2,	 250000000, 6, 41666667, 0x0580, 0x0580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30 },
    { "WinChip 2A/200",		   CPU_WINCHIP2,	 200000000, 3, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24 },
    { "WinChip 2A/233",		   CPU_WINCHIP2,	 233333333, 3, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 28 },
    { "WinChip 2A/266",		   CPU_WINCHIP2,	 233333333, 6, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 28 },
    { "WinChip 2A/300",		   CPU_WINCHIP2,	 250000000, 6, 33333333, 0x0587, 0x0587, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30 },
    { NULL }
};

const CPU cpus_Pentium5V[] = {
    /* Intel Pentium (5V, socket 4) */
    { "Pentium 60",			CPU_PENTIUM,	  60000000, 1, 30000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 7 },
    { "Pentium 66",			CPU_PENTIUM,	  66666666, 1, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 8 },
    { "Pentium OverDrive 120",		CPU_PENTIUM,     120000000, 2, 30000000, 0x051a, 0x051a, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "Pentium OverDrive 133",		CPU_PENTIUM,     133333333, 2, 33333333, 0x051a, 0x051a, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { NULL }
};

const CPU cpus_Pentium5V50[] = {
    /* Intel Pentium (5V, socket 4, including 50 MHz FSB) */
    { "Pentium 50 (Q0399)",		CPU_PENTIUM,      50000000, 1, 25000000, 0x0513, 0x0513, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 4,4,3,3, 6 },
    { "Pentium 60",			CPU_PENTIUM,	  60000000, 1, 30000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 7 },
    { "Pentium 66",			CPU_PENTIUM,	  66666666, 1, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 8 },
    { "Pentium OverDrive 100",		CPU_PENTIUM,     100000000, 2, 25000000, 0x051a, 0x051a, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 8,8,6,6, 12 },
    { "Pentium OverDrive 120",		CPU_PENTIUM,     120000000, 2, 30000000, 0x051a, 0x051a, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "Pentium OverDrive 133",		CPU_PENTIUM,     133333333, 2, 33333333, 0x051a, 0x051a, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { NULL }
};

const CPU cpus_PentiumS5[] = {
    /* Intel Pentium (Socket 5) */
    { "Pentium 75",			CPU_PENTIUM,	  75000000, 2, 25000000, 0x0522, 0x0522, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "Pentium 90",			CPU_PENTIUM,	  90000000, 2, 30000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "Pentium 100/50",			CPU_PENTIUM,	 100000000, 2, 25000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 12 },
    { "Pentium 100/66",			CPU_PENTIUM,	 100000000, 2, 33333333, 0x0526, 0x0526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "Pentium 120",			CPU_PENTIUM,	 120000000, 2, 30000000, 0x0526, 0x0526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "Pentium OverDrive 125",		CPU_PENTIUM,     125000000, 3, 25000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, 16 },
    { "Pentium OverDrive 150",		CPU_PENTIUM,     150000000, 3, 30000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Pentium OverDrive 166",		CPU_PENTIUM,     166666666, 3, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 40 },
    { "Pentium OverDrive MMX 125",	CPU_PENTIUM_MMX, 125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, 15 },
    { "Pentium OverDrive MMX 150/60",	CPU_PENTIUM_MMX, 150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Pentium OverDrive MMX 166",	CPU_PENTIUM_MMX, 166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Pentium OverDrive MMX 180",	CPU_PENTIUM_MMX, 180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 21 },
    { "Pentium OverDrive MMX 200",	CPU_PENTIUM_MMX, 200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { NULL }
};

const CPU cpus_Pentium[] = {
    /* Intel Pentium */
    { "Pentium 75",			CPU_PENTIUM,	  75000000, 2, 25000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "Pentium 90",			CPU_PENTIUM,	  90000000, 2, 30000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "Pentium 100/50",			CPU_PENTIUM,	 100000000, 2, 25000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 12 },
    { "Pentium 100/66",			CPU_PENTIUM,	 100000000, 2, 33333333, 0x0526, 0x0526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "Pentium 120",			CPU_PENTIUM,	 120000000, 2, 30000000, 0x0526, 0x0526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "Pentium 133",			CPU_PENTIUM,	 133333333, 2, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "Pentium 150",			CPU_PENTIUM,	 150000000, 3, 30000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Pentium 166",			CPU_PENTIUM,	 166666666, 3, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Pentium 200",			CPU_PENTIUM,	 200000000, 3, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "Pentium MMX 166",		CPU_PENTIUM_MMX, 166666666, 3, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Pentium MMX 200",		CPU_PENTIUM_MMX, 200000000, 3, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "Pentium MMX 233",		CPU_PENTIUM_MMX, 233333333, 4, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28 },
    { "Pentium MMX 266",		CPU_PENTIUM_MMX, 266666666, 4, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32 },
    { "Pentium MMX 300",		CPU_PENTIUM_MMX, 300000000, 5, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36 },
    { "Mobile Pentium MMX 120",		CPU_PENTIUM_MMX, 120000000, 2, 30000000, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "Mobile Pentium MMX 133",		CPU_PENTIUM_MMX, 133333333, 2, 33333333, 0x0543, 0x0543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "Mobile Pentium MMX 150",		CPU_PENTIUM_MMX, 150000000, 3, 30000000, 0x0544, 0x0544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Mobile Pentium MMX 166",		CPU_PENTIUM_MMX, 166666666, 3, 33333333, 0x0544, 0x0544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Mobile Pentium MMX 200",		CPU_PENTIUM_MMX, 200000000, 3, 33333333, 0x0581, 0x0581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "Mobile Pentium MMX 233",		CPU_PENTIUM_MMX, 233333333, 4, 33333333, 0x0581, 0x0581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28 },
    { "Mobile Pentium MMX 266",		CPU_PENTIUM_MMX, 266666666, 4, 33333333, 0x0582, 0x0582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32 },
    { "Mobile Pentium MMX 300",		CPU_PENTIUM_MMX, 300000000, 5, 33333333, 0x0582, 0x0582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36 },
    { "Pentium OverDrive 125",		CPU_PENTIUM,     125000000, 3, 25000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, 15 },
    { "Pentium OverDrive 150",		CPU_PENTIUM,     150000000, 3, 30000000, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Pentium OverDrive 166",		CPU_PENTIUM,     166666666, 3, 33333333, 0x052c, 0x052c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Pentium OverDrive MMX 125",	CPU_PENTIUM_MMX, 125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, 15 },
    { "Pentium OverDrive MMX 150/60",	CPU_PENTIUM_MMX, 150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "Pentium OverDrive MMX 166",	CPU_PENTIUM_MMX, 166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "Pentium OverDrive MMX 180",	CPU_PENTIUM_MMX, 180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 21 },
    { "Pentium OverDrive MMX 200",	CPU_PENTIUM_MMX, 200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { NULL }
};

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
const CPU cpus_K5[] = {
    /* AMD K5 (Socket 5) */
    { "K5 (5k86) 75 (P75)",		CPU_K5,		  75000000, 2, 25000000, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "K5 (SSA/5) 75 (PR75)",		CPU_K5,		  75000000, 2, 25000000, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "K5 (5k86) 90 (P90)",		CPU_K5,		  90000000, 2, 30000000, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "K5 (SSA/5) 90 (PR90)",		CPU_K5,		  90000000, 2, 30000000, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "K5 (5k86) 100 (P100)",		CPU_K5,		 100000000, 2, 33333333, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "K5 (SSA/5) 100 (PR100)",		CPU_K5,		 100000000, 2, 33333333, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "K5 (5k86) 90 (PR120)",		CPU_5K86,	 120000000, 2, 30000000, 0x0511, 0x0511, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "K5 (5k86) 100 (PR133)",		CPU_5K86,	 133333333, 2, 33333333, 0x0514, 0x0514, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "K5 (5k86) 105 (PR150)",		CPU_5K86,	 150000000, 3, 30000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "K5 (5k86) 116.5 (PR166)",	CPU_5K86,	 166666666, 3, 33333333, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "K5 (5k86) 133 (PR200)",		CPU_5K86,	 200000000, 3, 33333333, 0x0534, 0x0534, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { NULL }
};

const CPU cpus_K56[] = {
    /* AMD K5 and K6 (Socket 7) */
    { "K5 (5k86) 75 (P75)",		CPU_K5,		  75000000, 2, 25000000, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "K5 (SSA/5) 75 (PR75)",		CPU_K5,		  75000000, 2, 25000000, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    { "K5 (5k86) 90 (P90)",		CPU_K5,		  90000000, 2, 30000000, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "K5 (SSA/5) 90 (PR90)",		CPU_K5,		  90000000, 2, 30000000, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 21/2 },
    { "K5 (5k86) 100 (P100)",		CPU_K5,		 100000000, 2, 33333333, 0x0500, 0x0500, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "K5 (SSA/5) 100 (PR100)",		CPU_K5,		 100000000, 2, 33333333, 0x0501, 0x0501, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, 12 },
    { "K5 (5k86) 90 (PR120)",		CPU_5K86,	 120000000, 2, 30000000, 0x0511, 0x0511, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 14 },
    { "K5 (5k86) 100 (PR133)",		CPU_5K86,	 133333333, 2, 33333333, 0x0514, 0x0514, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 16 },
    { "K5 (5k86) 105 (PR150)",		CPU_5K86,	 150000000, 3, 30000000, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    { "K5 (5k86) 116.5 (PR166)",	CPU_5K86,	 166666666, 3, 33333333, 0x0524, 0x0524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "K5 (5k86) 133 (PR200)",		CPU_5K86,	 200000000, 3, 33333333, 0x0534, 0x0534, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "K6 (Model 6) 166",		CPU_K6,		 166666666, 3, 33333333, 0x0562, 0x0562, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    { "K6 (Model 6) 200",		CPU_K6,		 200000000, 3, 33333333, 0x0562, 0x0562, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "K6 (Model 6) 233",		CPU_K6,		 233333333, 4, 33333333, 0x0562, 0x0562, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28 },
    { "K6 (Model 7) 200",		CPU_K6,		 200000000, 3, 33333333, 0x0570, 0x0570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
    { "K6 (Model 7) 233",		CPU_K6,		 233333333, 4, 33333333, 0x0570, 0x0570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28 },
    { "K6 (Model 7) 266",		CPU_K6,		 266666666, 4, 33333333, 0x0570, 0x0570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32 },
    { "K6 (Model 7) 300",		CPU_K6,		 300000000, 5, 33333333, 0x0570, 0x0570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36 },
    { NULL }
};
#endif

const CPU cpus_PentiumPro[] = {
    /* Intel Pentium Pro and II Overdrive */
#if defined(DEV_BRANCH) && defined(_DEBUG)
    {"Pentium Pro 50",			CPU_PENTIUM_PRO,  50000000, 1, 25000000, 0x0612, 0x0612, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 4,4,3,3, 6 },
    {"Pentium Pro 60",			CPU_PENTIUM_PRO,  60000000, 1, 30000000, 0x0612, 0x0612, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 7 },
    {"Pentium Pro 66",			CPU_PENTIUM_PRO,  66666666, 1, 33333333, 0x0612, 0x0612, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 8 },
    {"Pentium Pro 75",			CPU_PENTIUM_PRO,  75000000, 2, 25000000, 0x0612, 0x0612, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
#endif
    {"Pentium Pro 150",			CPU_PENTIUM_PRO, 150000000, 3, 30000000, 0x0612, 0x0612, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 35/2 },
    {"Pentium Pro 166",			CPU_PENTIUM_PRO, 166666666, 3, 33333333, 0x0617, 0x0617, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20 },
    {"Pentium Pro 180",			CPU_PENTIUM_PRO, 180000000, 3, 30000000, 0x0617, 0x0617, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 21 },
    {"Pentium Pro 200",			CPU_PENTIUM_PRO, 200000000, 3, 33333333, 0x0617, 0x0617, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24 },
#if defined(DEV_BRANCH) && defined(_DEBUG)
    {"Pentium II Overdrive 50",		CPU_PENTIUM_2D,	  50000000, 1, 25000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 4,4,3,3, 6 },
    {"Pentium II Overdrive 60",		CPU_PENTIUM_2D,   60000000, 1, 30000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 7 },
    {"Pentium II Overdrive 66",		CPU_PENTIUM_2D,   66666666, 1, 33333333, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 8 },
    {"Pentium II Overdrive 75",		CPU_PENTIUM_2D,   75000000, 2, 25000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, 9 },
    {"Pentium II Overdrive 210",	CPU_PENTIUM_2D,  210000000, 4, 30000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7, 25 },
    {"Pentium II Overdrive 233",	CPU_PENTIUM_2D,  233333333, 4, 33333333, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,1, 28 },
    {"Pentium II Overdrive 240",	CPU_PENTIUM_2D,  240000000, 4, 30000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 29 },
    {"Pentium II Overdrive 266",	CPU_PENTIUM_2D,  266666666, 4, 33333333, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32 },
    {"Pentium II Overdrive 270",	CPU_PENTIUM_2D,  270000000, 5, 30000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 25,25,12,12, 33 },
#endif
    {"Pentium II Overdrive 300/66",	CPU_PENTIUM_2D,  300000000, 5, 33333333, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 16,16,8,8, 36 },
    {"Pentium II Overdrive 300/60",	CPU_PENTIUM_2D,  300000000, 5, 30000000, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 36 },
    {"Pentium II Overdrive 333",	CPU_PENTIUM_2D,  333333333, 5, 33333333, 0x1632, 0x1632, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 40 },
    { NULL }
};
