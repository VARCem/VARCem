/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Try to load a support DLL.
 *
 * Version:	@(#)win_dynld.c	1.0.6	2018/05/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../plat.h"


void *
dynld_module(const char *name, const dllimp_t *table)
{
    HMODULE h;
    const dllimp_t *imp;
    void *func;

    /* See if we can load the desired module. */
    if ((h = LoadLibrary(name)) == NULL) {
#ifdef _DEBUG
	pclog("DynLd(\"%s\"): library not found!\n", name);
#endif
	return(NULL);
    }

    /* If no table was given, we just detect library presence. */
    if (table == NULL) {
	FreeLibrary(h);
	return((void *)h);
    }

    /* Now load the desired function pointers. */
    for (imp = table; imp->name != NULL; imp++) {
	func = GetProcAddress(h, imp->name);
	if (func == NULL) {
		pclog("DynLd(\"%s\"): function '%s' not found!\n",
						name, imp->name);
		FreeLibrary(h);
		return(NULL);
	}

	/* To overcome typing issues.. */
	*(char **)imp->func = (char *)func;
    }

    /* All good. */
    return((void *)h);
}


void
dynld_close(void *handle)
{
    if (handle != NULL)
	FreeLibrary((HMODULE)handle);
}
