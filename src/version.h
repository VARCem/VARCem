/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define application version and build info.
 *
 * Version:	@(#)version.h	1.0.23	2020/06/05
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018-2020 Fred N. van Kempen.
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
#ifndef EMU_VERSION_H
# define EMU_VERSION_H


/* Application name. */
#define EMU_NAME	"VARCem"

/* Version info. */
#define EMU_VER_MAJOR	1
#define EMU_VER_MINOR	7
#define EMU_VER_REV	3
//#define EMU_VER_PATCH	0


/* Standard C preprocessor macros. */
#define STR_STRING(x)	#x
#define STR(x)		STR_STRING(x)
#define STR_RC(a,e)	a ## , ## e


/* These are used in the application. */
#define EMU_VER_NUM	EMU_VER_MAJOR.EMU_VER_MINOR.EMU_VER_REV
#if defined(EMU_VER_PATCH) && EMU_VER_PATCH > 0
# define EMU_VER_NUM_4	EMU_VER_MAJOR.EMU_VER_MINOR.EMU_VER_REV.EMU_VER_PATCH
#else
# define EMU_VER_NUM_4	EMU_VER_MAJOR.EMU_VER_MINOR.EMU_VER_REV.0
#endif
#define EMU_VERSION	STR(EMU_VER_NUM)
#define EMU_VERSION_4	STR(EMU_VER_NUM_4)


#endif	/*EMU_VERSION_H*/
