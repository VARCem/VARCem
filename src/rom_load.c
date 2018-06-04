/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the external ROM loader.
 *		This loader defines a 'ROM set' to be one or more images
 *		of ROM chip(s), where all properties of these defined in
 *		a single 'ROM definition' (text) file.
 *
 * NOTE:	This file uses a fairly generic script parser, which can
 *		be re-used for others parts. This would mean passing the
 *		'parser' function a pointer to either a command handler,
 *		or to use a generic handler, and then pass it a pointer
 *		to a command table. For now, we don't.
 *
 * Version:	@(#)rom_load.c	1.0.11	2018/05/27
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
#include <ctype.h>
#include "emu.h"
#include "machines/machine.h"
#include "mem.h"
#include "rom.h"
#include "device.h"
#include "plat.h"


#define MAX_ARGS	16		/* max number of arguments */


/* Process a single (logical) command line. */
static int
process(int ln, int argc, char **argv, romdef_t *r)
{
again:
    if (! strcmp(argv[0], "size")) {
	/* Total size of image. */
	r->total = get_val(argv[1]);
    } else if (! strcmp(argv[0], "offset")) {
	/* Offset into the ROM area. */
	r->offset = get_val(argv[1]);
    } else if (! strcmp(argv[0], "mode")) {
	/* Loading method to use for this image. */
	if (! strcmp(argv[1], "linear"))
		r->mode = 0;
	  else if (! strcmp(argv[1], "interleaved"))
		r->mode = 1;
	  else {
		pclog("ROM: invalid mode '%s' on line %d.\n", argv[1], ln);
		return(0);
	}
    } else if (! strcmp(argv[0], "optional")) {
	/*
	 * This is an optional file.
	 * Next word is the name of the configuration
	 * variable this depends on, for example "basic"
	 * or "romdos".
	 */
	if (! machine_get_config_int(argv[1])) return(1);

	/* Skip the keyword and variable name, and re-parse. */
	argv += 2;
	argc -= 2;
	goto again;
    } else if (! strcmp(argv[0], "file")) {
	/* Specify the image filename and/or additional parameters. */
	mbstowcs(r->files[r->nfiles].path, argv[1],
		sizeof_w(r->files[r->nfiles].path));
	r->files[r->nfiles].skip = 0;
	r->files[r->nfiles].offset = r->offset;
	r->files[r->nfiles].size = r->total;
	switch(argc) {
		case 5:
			r->files[r->nfiles].size = get_val(argv[4]);
			/*FALLTHROUGH*/

		case 4:
			r->files[r->nfiles].offset = get_val(argv[3]);
			/*FALLTHROUGH*/

		case 3:
			r->files[r->nfiles].skip = get_val(argv[2]);
			break;
	}
	r->nfiles++;
    } else if (! strcmp(argv[0], "font")) {
	/* Load a video controller font. */
	r->fontnum = atoi(argv[1]);
	mbstowcs(r->fontfn, argv[2], sizeof_w(r->fontfn));
    } else if (! strcmp(argv[0], "md5sum")) {
	/* Skip for now. */
    } else if (! strcmp(argv[0], "video")) {
	/* Load a video controller BIOS. */
	mbstowcs(r->vidfn, argv[1], sizeof_w(r->vidfn));
	sscanf(argv[2], "%i", &r->vidsz);
    } else {
	pclog("ROM: invalid command '%s' on line %d.\n", argv[0], ln);
	return(0);
    }

    return(1);
}


/* Parse a script file, and call the command handler for each command. */
static int
parser(FILE *fp, romdef_t *r)
{
    char line[1024];
    char *args[MAX_ARGS];
    int doskip, doquot;
    int skipnl, dolit;
    int a, c, l;
    char *sp;

    /* Initialize the parser and run. */
    l = 0;
    for (;;) {
	/* Clear the per-line stuff. */
	skipnl = dolit = doquot = 0;
	doskip = 1;
	for (a=0; a<MAX_ARGS; a++)
		args[a] = NULL;
	a = 0;
	sp = line;

	/* Catch the first argument. */
	args[a] = sp;

	/*
	 * Get the next logical line from the configuration file.
	 * This is not trivial, since we allow for a fairly standard
	 * Unix-style ASCII-based text file, including the usual line
	 * extenders and character escapes.
	 */
	while ((c = fgetc(fp)) != EOF) {
		/* Literals come first... */
		if (dolit) {
			switch(c) {
				case '\r':	/* raw CR, ignore it */
					continue;

				case '\n':	/* line continuation! */
					l++;
					break;

				case 'n':
					*sp++ = '\n';
					break;

				case 'r':
					*sp++ = '\r';
					break;

				case 'b':
					*sp++ = '\b';
					break;

				case 'e':
					*sp++ = 27;
					break;

				case '"':
					*sp++ = '"';
					break;

				case '#':
					*sp++ = '#';
					break;

				case '!':
					*sp++ = '!';
					break;

				case '\\':
					*sp++ = '\\';
					break;

				default:
					pclog("ROM: syntax error: escape '\\%c'", c);
					*sp++ = '\\';
					*sp++ = (char)c;
			}
			dolit = 0;
			continue;

		}

		/* Are we in a comment? */
		if (skipnl) {
			if (c == '}')
				skipnl--;	/* nested comment closed */

			if (c == '\n')
				skipnl = 0;	/* end of line! */

			if (skipnl == 0)
				break;

			continue;		/* skip more... */
		}

		/* Are we starting a comment? */
		if ((c == '{') || (c == ';') || (c == '#')) {
			skipnl++;
			continue;
		}

		/* Are we starting a literal character? */
		if (c == '\\') {
			dolit = 1;
			continue;
		}

		/* Are we starting a quote? */
		if (c == '"') {
			doquot = (1 - doquot);
			if (doskip) {
				args[a++] = sp;
				doskip = 0;
			}
			continue;
		}

		/* Quoting means raw insertion. */
		if (doquot) {
			/* We are quoting, so insert as is. */
			if (c == '\n') pclog("ROM: syntax error: unexpected newline, expected (\")\n");
			*sp++ = (char)c;
			continue;
		}

		/*
		 * Everything else, normal character insertion.
		 * This means, we process each character as if it
		 * was typed in.  We assume the usual rules for
		 * skipping whitespace, blank lines and so forth.
		 */
		/* Ignore raw CRs, they are common in DOS-originated files. */
		if (c == '\r')
			continue;

		/* A raw newline terminates the logical line. */
		if (c == '\n') break;

		/* See if this is some word breaker. */
		if (c == ',') {
			/* Terminate current word. */
			*sp++ = '\0';

			/* ... and start new word. */
			args[a] = sp;
			doskip = 0;
			continue;
		}

		/* Is it regular whitespace? */
		if ((c == ' ') || (c == '\t')) {
			/* Are we skipping whitespace? */
			if (! doskip) {
				/* Nope, Start a new argument. */
				*sp++ = '\0';
				doskip = 1;
			}

			/* Yes, skip it. */
			continue;
		}

		/* Just a regular thingie. Store it. */
		if (isupper(c))
			c = tolower(c);

		/* If we are skipping space, we now hit the end of that. */
		if (doskip)
			args[a++] = sp;
		*sp++ = (char)c;
		doskip = 0;
	}
	*sp = '\0';
	if (feof(fp)) break;
	if (ferror(fp)) {
		pclog("ROM: Read Error on line '%s'\n", l);
		return(0);
	}
	l++;

	/* Ignore comment lines and empty lines. */
	if (*args[0] == '\0') continue;

	/* Process this line. */
	if (! process(l, a, args, r)) return(0);
    }

    return(1);
}


/* Load a BIOS ROM image into memory. */
int
rom_load_bios(romdef_t *r, const wchar_t *fn, int test_only)
{
    wchar_t path[1024];
    wchar_t temp[1024];
    wchar_t script[1024];
    FILE *fp;
    int c, i;

    /* Generate the BIOS pathname. */
    wcscpy(path, fn);
    plat_append_slash(path);

    /* Generate the full script pathname. */
    wcscpy(script, path);
    wcscat(script, BIOS_FILE);
    pc_path(script, sizeof_w(script), NULL);

    if (! test_only) {
	pclog("ROM: loading script '%ls'\n", rom_path(script));

	/* If not done yet, allocate a 128KB buffer for the BIOS ROM. */
	if (rom == NULL)
		rom = (uint8_t *)malloc(131072);
	memset(rom, 0xff, 131072);

	/* Default to a 64K ROM BIOS image. */
	biosmask = 0xffff;

	/* Zap the BIOS ROM EXTENSION area. */
	memset(romext, 0xff, 0x8000);
	mem_mapping_disable(&romext_mapping);
    }

    /* Open the script file. */
    if ((fp = plat_fopen(rom_path(script), L"rb")) == NULL) {
	pclog("ROM: unable to open '%ls'\n", rom_path(script));
	return(0);
    }

    /* Clear ROM definition. */
    memset(r, 0x00, sizeof(romdef_t));
    r->fontnum = -1;

    /* Parse and process the file. */
    i = parser(fp, r);

    (void)fclose(fp);

    /* Show the resulting data. */
    if (! test_only) {
	pclog("Size     : %lu\n", r->total);
	pclog("Offset   : 0x%06lx (%lu)\n", r->offset, r->offset);
	pclog("Mode     : %s\n", (r->mode == 1)?"interleaved":"linear");
	pclog("Files    : %d\n", r->nfiles);
	for (c=0; c<r->nfiles; c++) {
		pclog("%c[%d]     : '%ls', %i, 0x%06lx, %i\n",
			(r->files[c].offset==0xffffffff)?'*':' ', c+1,
			r->files[c].path, r->files[c].skip,
			r->files[c].offset, r->files[c].size);
	}
	if (r->fontnum != -1)
		pclog("Font     : %i, '%ls'\n", r->fontnum, r->fontfn);
	if (r->vidsz != 0)
		pclog("VideoBIOS: '%ls', %i\n", r->vidfn, r->vidsz);

	/* Actually perform the work. */
	switch(r->mode) {
		case 0:			/* linear file(s) */
			/* We loop on all files. */
			for (c=0; c<r->nfiles; c++) {
				/* If this is a no-load file, skip. */
				if (r->files[c].offset == 0xffffffff)
					continue;

				wcscpy(script, path);
				wcscat(script, r->files[c].path);
				pc_path(script, sizeof_w(script), NULL);

				i = rom_load_linear(script,
						    r->files[c].offset,
						    r->files[c].size,
						    r->files[c].skip, rom);
				if (i != 1) break;
			}
			if (r->total >= 0x010000)
				biosmask = (r->total - 1);
			break;

		case 1:			/* interleaved file(s) */
			/* We loop on all files. */
			for (c=0; c<r->nfiles/2; c+=2) {
				/* If this is a no-load file, skip. */
				if (r->files[c].offset == 0xffffffff)
					continue;

				wcscpy(script, path);
				wcscat(script, r->files[c].path);
				pc_path(script, sizeof_w(script), NULL);
				wcscpy(temp, path);
				wcscat(temp, r->files[c+1].path);
				pc_path(temp, sizeof_w(temp), NULL);

				i = rom_load_interleaved(script, temp,
						 	r->files[c].offset,
						 	r->files[c].size,
						 	r->files[c].skip, rom);
				if (i != 1) break;
			}
			if (r->total >= 0x010000)
				biosmask = (r->total - 1);
			break;
	}

	/* Create a full pathname for the video font file. */
	if (r->fontnum != -1) {
		wcscpy(temp, path);
		wcscat(temp, r->fontfn);
		pc_path(r->fontfn, sizeof_w(r->fontfn), temp);
	}

	/* Create a full pathname for the video BIOS file. */
	if (r->vidsz != 0) {
		wcscpy(temp, path);
		wcscat(temp, r->vidfn);
		pc_path(r->vidfn, sizeof_w(r->vidfn), temp);
	}

	pclog("ROM: status %d, tot %u, mask 0x%06lx\n",
				i, r->total, biosmask);
    }

    return(i);
}
