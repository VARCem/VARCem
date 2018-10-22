/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the various speeds supported by optical devices.
 *
 *		List of usable CD-ROM speeds (from Wikipedia):
 *
 *		 Speed 	KiB/s	 	Mbit/s 	 	RPM
 *		  1x	150 		1.2288  	200-500
 *		  2x	300 		2.4576	 	400-1,000
 *		  4x	600 		4.9152		800-2,000
 *		  6x	?		?		?
 *		  8x	1,200 		9.8304 		1,600-4,000
 *		  10x	1,500 		12.288 	 	2,000-5,000
 *		  12x	1,800 		14.7456 	2,400-6,000
 *		  16x	?		?		?
 *		  20x	1,200-3,000 	up to 24.576 	4,000 (CAV)
 *		  24x	?		?		?
 *		  32x	1,920-4,800 	up to 39.3216 	6,400 (CAV)
 *		  36x	2,160-5,400 	up to 44.2368 	7,200 (CAV)
 *		  40x	2,400-6,000 	up to 49.152 	8,000 (CAV)
 *		  44x	?		?		?
 *		  48x	2,880-7,200 	up to 58.9824 	9,600 (CAV)
 *		  52x	3,120-7,800 	up to 63.8976 	10,400 (CAV)
 *		  56x	3,360-8,400 	up to 68.8128 	11,200 (CAV)
 *		  72x	6,750-10,800 	up to 88.4736 	2,000 (multi-beam)
 *
 *		We discard the latter two ones.
 *
 * Version:	@(#)cdrom_speed.c	1.0.2	2018/10/05
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../../emu.h"
#include "../../version.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../scsi/scsi_device.h"
#include "cdrom.h"


const cdrom_speed_t cdrom_speeds[] = {
    { 1,	140.0,	1446.0	},
    { 2,	160.0,	1000.0	},
    { 4,	112.0,	675.0	},
    { 6,	112.0,	675.0	},
    { 8,	112.0,	675.0	},
    { 10,	112.0,	675.0	},
    { 12,	75.0,	400.0	},
    { 16,	58.0,	350.0	},
    { 20,	50.0,	300.0	},
    { 24,	45.0,	270.0	},
    { 32,	45.0,	270.0	},
    { 36,	50.0,	300.0	},
    { 40,	50.0,	300.0	},
    { 44,	50.0,	300.0	},
    { 48,	50.0,	300.0	},
    { 52,	45.0,	270.0	},
    { -1,	0.0,	0.0	},
};


/* Return a speed index for a given real speed. */
int
cdrom_speed_idx(int realspeed)
{
    int idx;

    for (idx = 0; cdrom_speeds[idx].speed > 0; idx++)
	if (cdrom_speeds[idx].speed == realspeed)
		return(idx);

    return(-1);
}
