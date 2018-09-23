/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Misc functions that do not fit anywhere else.
 *
 * Version:	@(#)misc.c	1.0.2	2018/09/22
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#include "emu.h"
#undef malloc
#include "plat.h"
#include "ui/ui.h"


const wchar_t *
get_string(int id)
{
    const wchar_t *str = NULL;
    const string_t *tbl;

    tbl = plat_strings;
    while(tbl->str != NULL) {
	if (tbl->id == id) {
		str = tbl->str;
		break;
	}

	tbl++;
    }

    return(str);
}


/* Grab the value from a string. */
uint32_t
get_val(const char *str)
{
    long unsigned int l = 0UL;

    if ((strlen(str) > 1)  &&			/* hex always is 0x... */
	(sscanf(str, "0x%lx", &l) == 0))	/* no valid field found */
	sscanf(str, "%i", (int *)&l);		/* try decimal.. */

    return(l);
}


/* Safe version of malloc(3) that catches NULL returns. */
void *
mem_alloc(size_t sz)
{
    void *ptr = malloc(sz);

    if (ptr == NULL) {
	ui_msgbox(MBX_ERROR|MBX_FATAL, (wchar_t *)IDS_ERR_NOMEM);

	/* Try to write to the logfile. This may not work anymore. */
	pclog("FATAL: system out of memory!\n");
	fflush(stdlog);
	fclose(stdlog);

	/* Now exit - this will (hopefully..) not return. */
	exit(-1);
	/*NOTREACHED*/
    }

    return(ptr);
}
