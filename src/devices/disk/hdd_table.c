/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the available hard disk types.
 *
 * Version:	@(#)hdd_table.c	1.0.4	2021/03/13
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include "../../emu.h"
#include "hdd.h"


const hddtab_t hdd_table[] = {
    {  306,  4, 17, "Seagate ST-412"	 	},	// 10M, 5, FH
    {  615,  2, 17, "Tandon TM-261"	 	},	// 10M, 5, FH
    {  697,  3, 17, "CDC Wren-1 (94155-21)"	},	// 21M, 5, FH
    {  306,  8, 17, "Seagate ST-425"	 	},	// 21M, 5, FH
    {  614,  4, 17, "Seagate ST-4026"	 	},	// 21M, 5, FH
    {  615,  4, 17, "Seagate ST-225"	 	},	// 21M, 5, HH
    {  615,  4, 17, "Seagate ST-125"	 	},	// 21M, 3, HH
    {  667,  2, 31, "Seagate ST-225R"	 	},	// 21M, 5, HH
    {  670,  4, 17, "CDC 77731614"	 	},	// 23M, 5, FH
    {  697,  4, 17, "CDC Wren-1 (94155-25)" 	},	// 24M, 5, FH
    {  670,  5, 17, "CDC 77731608"	 	},	// 29M, 5, FH
    {  697,  5, 17, "CDC Wren-1 (94155-36)" 	},	// 36M, 5, FH
    {  733,  5, 17, "Seagate ST-4038"	 	},	// 31M, 5, FH
    {  615,  4, 26, "Seagate ST-238R"	 	},	// 32M, 5, HH
    {  615,  6, 17, "Seagate ST-138"	 	},	// 32M, 3, HH
    {  615,  4, 26, "Seagate ST-138R"	 	},	// 32M, 3, HH
    { 1024,  4, 17, "Micropolis 1323"	 	},	// 35M, 5, FH
    {  987,  3, 17, "Priam V-130"	 	},	// 39M, 5, FH
    {  925,  5, 17, "CDC Wren-2 (94155-48)" 	},	// 40M, 5, FH
    {  699,  7, 17, "Hitachi DK511-5"	 	},	// 40M, 5, FH
    {  667,  4, 31, "Seagate ST-250R"	 	},	// 42M, 5, HH
    {  820,  6, 17, "Seagate ST-251"	 	},	// 42M, 5, HH
    {  809,  6, 17, "Miniscribe 3650"	 	},	// 42M, 5, HH
    {  977,  5, 17, "Seagate ST-4051"	 	},	// 42M, 5, FH
    {  977,  5, 17, "Seagate ST-151"	 	},	// 42M, 3, HH
    {  615,  8, 17, "Fujitsu M2227D2"	 	},	// 42M, 3, HH
    {  733,  7, 17, "Panasonic JU-128"	 	},	// 42M, 3, HH
    {  820,  4, 26, "Seagate ST-251R"	 	},	// 43M, 5, HH
    {  754,  7, 17, "Fujitsu M2242AS2"	 	},	// 43M, 5, FH
    {  981,  5, 17, "Tandon TM-755"	 	},	// 43M, 5, FH
    {  733,  7, 26, "Toshiba MK134FA"	 	},	// 44M, 3, HH
    { 1024,  5, 17, "Seagate ST-4053"	 	},	// 44M, 5, FH
    { 1024,  5, 17, "Miniscribe 3053"	 	},	// 44M, 5, HH
    { 1024,  5, 17, "Micropolis 1323A"	 	},	// 44M, 5, HH
    { 1024,  5, 17, "Microscience 1060"		},	// 44M, 5, HH
    { 1024,  5, 17, "Microscience 4050"		},	// 44M, 3, HH
    {  615,  6, 26, "Seagate ST-157R"	 	},	// 49M, 3, HH
    {  918,  7, 17, "Maxtor XT-1065"	 	},	// 56M, 5, FH
    {  925,  7, 17, "CDC Wren-2 (94155-67)" 	},	// 56M, 5, FH
    {  830,  7, 17, "Toshiba MK54FA"	 	},	// 60M, 5, FH
    {  987,  7, 17, "Priam V-170"	 	},	// 60M, 5, FH
    { 1024,  7, 17, "Micropolis 1324A"	 	},	// 62M, 5, FH
    {  809,  6, 26, "Miniscribe 3675"	 	},	// 63M, 5, HH
    {  925,  8, 17, "CDC Wren-2 (94155-77)" 	},	// 64M, 5, FH
    {  615,  8, 26, "Fujitsu M2227DR"	 	},	// 65M, 3, HH
    {  820,  6, 26, "Seagate ST-277R"	 	},	// 65M, 5, HH
    { 1024,  5, 26, "Seagate ST-4077R"	 	},	// 65M, 5, FH
    {  754, 11, 17, "Fujitsu M2243AS3"	 	},	// 67M, 5, FH
    {  823, 10, 17, "Hitachi DK511-8"	 	},	// 67M, 5, FH
    { 1024,  8, 17, "Tandon TM-3085"	 	},	// 71M, 5, FH
    { 1024,  8, 17, "Maxtor XT-1085"	 	},	// 71M, 5, FH
    { 1024,  8, 17, "Micropolis 1325"	 	},	// 71M, 5, FH
    {  925,  9, 17, "CDC Wren-2 (94155-86)" 	},	// 72M, 5, FH
    {  925,  9, 26, "CDC Wren-2 (94155-86)" 	},	// 72M, 5, HH
    { 1224,  7, 17, "Maxtor XT-2085"	 	},	// 74M, 5, FH
    { 1024,  9, 17, "Seagate ST-4096"	 	},	// 80M, 5, FH
    {  969,  5, 34, "CDC Wren-3 (94166-101)" 	},	// 86M, 5, FH
    {  918, 11, 17, "Maxtor XT-1105"	 	},	// 87M, 5, FH
    { 1024,  8, 25, "Maxtor XT-1120R"		},	// 104M, 5, FH
    {  960,  9, 26, "CDC Wren-2 (94155-135)" 	},	// 115M, 5, HH
    { 1224, 11, 17, "Maxtor XT-2140"	 	},	// 117M, 5, FH
    {  918, 15, 17, "Maxtor XT-1140"	 	},	// 119M, 5, FH
    {  969,  7, 34, "CDC Wren-3 (94166-141)" 	},	// 121M, 5, FH
    { 1024,  9, 26, "Seagate ST-4114R"	 	},	// 122M, 5, FH
    {  830, 10, 17, "Toshiba MK55FB"	 	},	// 130M, 5, FH
    {  830, 10, 26, "Seagate MKR56FB"	 	},	// 130M, 5, FH
    {  823, 10, 26, "Hitachi DK512-17"	 	},	// 134M, 5, FH
    {  969,  9, 34, "CDC Wren-3 (94166-182)" 	},	// 155M, 5, FH
    { 1224, 15, 17, "Maxtor XT-2190"	 	},	// 159M, 5, FH
    { 1147,  8, 36, "Seagate ST-4129E"	 	},	// 169M, 5, FH
    { 1024, 15, 25, "Maxtor XT-1240R"		},	// 196M, 5, FH
    {  306,  8, 26, NULL		 	},
    {  306,  4, 26, NULL		 	},
    {  462,  8, 17, NULL		 	},
    {  462,  8, 26, NULL		 	},
    {  684, 16, 38, NULL		 	},
    {  698,  7, 17, NULL		 	},
    {  698,  7, 26, NULL		 	},
    {  699,  7, 26, NULL		 	},
    {  733,  5, 26, NULL		 	},
    {  751,  8, 17, NULL		 	},
    {  751,  8, 26, NULL		 	},
    {  755, 16, 17, NULL		 	},
    {  755, 16, 26, NULL		 	},
    {  776,  8, 33, NULL		 	},
    {  820,  4, 17, NULL		 	},
    {  823,  4, 38, NULL		 	},
    {  855,  5, 17, NULL		 	},
    {  855,  5, 26, NULL		 	},
    {  855,  7, 17, NULL		 	},
    {  855,  7, 26, NULL		 	},
    {  900, 15, 17, NULL		 	},
    {  900, 15, 26, NULL		 	},
    {  903,  8, 46, NULL		 	},
    {  917, 15, 17, NULL		 	},
    {  917, 15, 26, NULL		 	},
    {  932,  5, 17, NULL		 	},
    {  940,  6, 17, NULL		 	},
    {  940,  6, 26, NULL		 	},
    {  940,  8, 17, NULL		 	},
    {  940,  8, 26, NULL		 	},
    {  960,  5, 35, NULL		 	},
    {  965, 10, 17, NULL		 	},
    {  965, 10, 26, NULL		 	},
    {  967, 16, 31, NULL		 	},
    {  969,  7, 34, NULL		 	},
    {  976,  5, 17, NULL		 	},
    {  976,  5, 26, NULL		 	},
    {  977,  5, 26, NULL		 	},
    {  977,  7, 17, NULL		 	},
    {  977,  7, 26, NULL		 	},
//  {  987, 16, 63, NULL		 	},
    {  980, 10, 17, NULL		 	},
    {  980, 10, 26, NULL		 	},
    {  984, 10, 34, NULL		 	},
    {  989,  5, 17, "Seagate ST-253"	 	},
//  {  995, 16, 63, NULL		 	},
//  { 1002, 13, 63, NULL		 	},
//  { 1013, 10, 63, NULL		 	},
    { 1020, 15, 17, NULL		 	},
    { 1023, 15, 17, NULL		 	},
    { 1023, 15, 26, NULL		 	},
    { 1024,  2, 17, NULL		 	},
    { 1024,  2, 40, NULL		 	},
    { 1024,  4, 26, NULL	 		},
    { 1024,  7, 26, NULL		 	},
    { 1024, 10, 17, NULL		 	},
    { 1024, 11, 17, NULL		 	},
    { 1024, 12, 17, NULL		 	},
    { 1024, 13, 17, NULL		 	},
    { 1024, 14, 17, NULL		 	},
    { 1024, 15, 17, NULL		 	},
    { 1024, 16, 17, NULL		 	},
//  { 1024, 16, 63, NULL		 	},
//  { 1036, 16, 63, NULL		 	},
//  { 1054, 16, 63, NULL		 	},
//  { 1120, 16, 59, NULL		 	},
    { 1218, 15, 36, NULL		 	},
    { 1524,  4, 39, NULL		 	},
//  { 1930,  4, 62, NULL		 	},

    {    0,  0,  0, NULL		 	}
};
