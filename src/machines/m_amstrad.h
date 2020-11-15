/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Amstrad machines.
 *
 * Version:	@(#)m_amstrad.h	1.0.4	2020/11/10
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2020 Fred N. van Kempen.
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
#ifndef MACHINE_AMSTRAD_H
# define MACHINE_AMSTRAD_H


/* Display type. */
#define IDA_CGA		0		/* CGA monitor */
#define IDA_MDA		1		/* MDA monitor */
#define IDA_TV		2		/* Television */
#define IDA_LCD_C	3		/* PPC LCD as CGA*/
#define IDA_LCD_M	4		/* PPC LCD as MDA*/


#ifdef EMU_DEVICE_H
extern const device_t	m_amstrad_1512;
extern const device_t	m_amstrad_1640;
extern const device_t	m_amstrad_200;
extern const device_t	m_amstrad_ppc;
extern const device_t	m_amstrad_2086;
extern const device_t	m_amstrad_3086;
extern const device_t	m_amstrad_5086;
extern const device_t	m_amstrad_mega;
#endif


extern priv_t	m_amstrad_1512_vid_init(const wchar_t *fn, int fnt, int cp);

extern priv_t	m_amstrad_1640_vid_init(const wchar_t *fn, int sz);

extern priv_t	m_amstrad_ida_init(int type, const wchar_t *fn,
				    int cp, int em, int dt);
extern uint8_t	m_amstrad_ida_ddm(priv_t);


#endif	/*MACHINE_AMSTRAD_H*/
