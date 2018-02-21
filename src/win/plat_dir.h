/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the platform OpenDir module.
 *
 * Version:	@(#)plat_dir.h	1.0.1	2018/02/14
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
#ifndef PLAT_DIR_H
# define PLAT_DIR_H


#ifdef _MAX_FNAME
# define MAXNAMLEN	_MAX_FNAME
#else
# define MAXNAMLEN	15
#endif
# define MAXDIRLEN	127


struct direct {
    long		d_ino;
    unsigned short 	d_reclen;
    unsigned short	d_off;
#ifdef UNICODE
    wchar_t		d_name[MAXNAMLEN + 1];
#else
    char		d_name[MAXNAMLEN + 1];
#endif
};
#define	d_namlen	d_reclen


typedef struct {
    short	flags;			/* internal flags		*/
    short	offset;			/* offset of entry into dir	*/
    long	handle;			/* open handle to Win32 system	*/
    short	sts;			/* last known status code	*/
    char	*dta;			/* internal work data		*/
#ifdef UNICODE
    wchar_t	dir[MAXDIRLEN+1];	/* open dir			*/
#else
    char	dir[MAXDIRLEN+1];	/* open dir			*/
#endif
    struct direct dent;			/* actual directory entry	*/
} DIR;


/* Directory routine flags. */
#define DIR_F_LOWER	0x0001		/* force to lowercase		*/
#define DIR_F_SANE	0x0002		/* force this to sane path	*/
#define DIR_F_ISROOT	0x0010		/* this is the root directory	*/


/* Function prototypes. */
#ifdef UNICODE
extern DIR		*opendirw(const wchar_t *);
#else
extern DIR		*opendir(const char *);
#endif
extern struct direct	*readdir(DIR *);
extern long		telldir(DIR *);
extern void		seekdir(DIR *, long);
extern int		closedir(DIR *);

#define rewinddir(dirp)	seekdir(dirp, 0L)


#endif	/*PLAT_DIR_H*/
