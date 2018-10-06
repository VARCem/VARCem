/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle language support for the platform.
 *
 * Version:	@(#)win_lang.c	1.0.6	2018/10/05
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
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "win.h"
#include "win_opendir.h"


/* Define a translation entry in the file. */
typedef struct {
  WORD			wLanguage;
  WORD			wCodePage;
} dll_lang_t;


/* Local data. */
static HMODULE		lang_handle = NULL;


static void
lang_add(lang_t *ptr, int sort)
{
    wchar_t name[128];
    wchar_t *str;
    LCID lcid;

    /* Convert the language ID to a (localized) name. */
    lcid = MAKELCID(ptr->id, SORT_DEFAULT);
    if (! GetLocaleInfo(lcid, LOCALE_SLANGUAGE, name, sizeof_w(name))) {
	ERRLOG("UI: unable to get name for language ID 0x%04X\n", ptr->id);
	return;
    }

    /* Clean up any existing name. */
    if (ptr->name != NULL)
	free((void *)ptr->name);

    /* Set the (new) name. */
    str = (wchar_t *)mem_alloc(sizeof(wchar_t) * (wcslen(name) + 1));
    memset(str, 0x00, sizeof(wchar_t) * (wcslen(name) + 1));
    wcscpy(str, name);
    ptr->name = (const wchar_t *)str;

    /* Actually add this language. */
    ui_lang_add(ptr, sort);
}


/*
 * Scan the LANG dir for any Language Modules we
 * may have installed there. The main application
 * only has English compiled in, any others are
 * kept in separate language modules.
 */
void
plat_lang_scan(void)
{
    wchar_t path[512], temp[512];
    uint8_t buffer[8192];
    wchar_t *str, *ptr;
    struct direct *de;
    dll_lang_t *lptr;
    lang_t lang;
    DIR *dir;
    int l, lflen;

    lflen = wcslen(LANG_FILE);

    /* Open the "language modules" directory. */
    swprintf(path, sizeof_w(temp), L"%ls%ls", exe_path, LANGUAGE_PATH);
    dir = opendir(path);
    if (dir != NULL) {
	/* Scan all files, and see if we find a usable one. */
	for (;;) {
		/* Get an entry from the directory. */
		if ((de = readdir(dir)) == NULL) break;

		/* Is this a file we want? */
		if ((int)wcslen(de->d_name) < lflen) continue;
		if (wcsncmp(de->d_name, LANG_FILE, lflen)) continue;
		str = wcschr(de->d_name, L'.');
		if (str == NULL) continue;
		if (wcsncasecmp(str, L".dll", 4)) continue;

		/* Looks like we have one here.. */
		memset(temp, 0x00, sizeof(temp));
		swprintf(temp, sizeof_w(temp), L"%ls\\%ls", path, de->d_name);
		l = wcslen(temp) + 1;

		/* Create a buffer for the DLL file name. */
		memset(&lang, 0x00, sizeof(lang));
		str = (wchar_t *)mem_alloc(sizeof(wchar_t) * l);
		memset(str, 0x00, sizeof(wchar_t) * l);
		wcscpy(str, temp);
		lang.dll = (const wchar_t *)str;

		/* Grab the version info block from the DLL. */
		if (! GetFileVersionInfo(temp, 0, sizeof(buffer), buffer)) {
			ERRLOG("UI: unable to access '%ls', skipping!\n", temp);
			free((void *)lang.dll);
			continue;
		}

		/* Extract the (first) "Translation" entry. */
		VerQueryValue(buffer, L"\\VarFileInfo\\Translation",
			      (LPVOID*)&lptr, (PUINT)&l);
		lang.id = lptr->wLanguage;

		/* Get the various strings from the block. */
		swprintf(temp, sizeof_w(temp),
			 L"\\StringFileInfo\\%04x%04x\\%ls",
			 lptr->wLanguage, lptr->wCodePage, L"FileVersion");
		if (! VerQueryValue(buffer, temp, (LPVOID*)&str, (PUINT)&l)) {
			ERRLOG("UI: invalid data in DLL, skipping!\n");
			free((void *)lang.dll);
			continue;
		}
		ptr = (wchar_t *)mem_alloc(sizeof(wchar_t) * (l+1));
		lang.version = (const wchar_t *)ptr;
		*ptr++ = L'v';
		do {
			/* Copy string, replacing , with . on the fly. */
			if (*str == L',')
				*ptr++ = L'.';
			  else
				*ptr++ = *str;
		} while (*str++ != L'\0');

		swprintf(temp, sizeof_w(temp),
			 L"\\StringFileInfo\\%04x%04x\\%ls",
			 lptr->wLanguage, lptr->wCodePage, L"Comments");
		if (! VerQueryValue(buffer, temp, (LPVOID*)&str, (PUINT)&l)) {
			ERRLOG("UI: invalid data in DLL, skipping!\n");
			free((void *)lang.version);
			free((void *)lang.dll);
			continue;
		}
		ptr = temp;
		while (*str != L'\0')
			*ptr++ = *str++;
		*ptr++ = *str++;
		l -= (ptr - temp);
		ptr = (wchar_t *)mem_alloc(sizeof(wchar_t) * (ptr-temp));
		wcscpy(ptr, temp);
		lang.author = (const wchar_t *)ptr;

		ptr = (wchar_t *)mem_alloc(sizeof(wchar_t) * l);
		wcscpy(ptr, str);
		lang.email = (const wchar_t *)ptr;

		/* Add this language. */
		lang_add(&lang, 1);
	}
	(void)closedir(dir);
    }

    /* Add the application's primary language. */
    memset(&lang, 0x00, sizeof(lang));
    lang.id = 0x0409;
    lang_add(&lang, 0);
}


/* Activate the new language for the application. */
void
plat_lang_set(int id)
{
    LCID lcid;

    SetThreadUILanguage(id);

    /* This can be removed, it no longer works since Vista+ */
    lcid = MAKELCID(id, SORT_DEFAULT);
    SetThreadLocale(lcid);
}


/* Pre-load the strings from our resource file. */
const string_t *
plat_lang_load(lang_t *ptr)
{
    wchar_t temp[512], *str;
    string_t *array, *tbl;
    int c = 0, i;

    /* Do we need to unload a resource DLL? */
    if (lang_handle != NULL) {
	FreeLibrary(lang_handle);
	lang_handle = NULL;
    }

    /* Do we need to load a resource DLL? */
    if (ptr->dll != NULL) {
	lang_handle = LoadLibraryEx(ptr->dll, 0,
				    LOAD_LIBRARY_AS_IMAGE_RESOURCE | \
				    LOAD_LIBRARY_AS_DATAFILE);
	if (lang_handle == NULL) {
		ERRLOG("UI: unable to load resource DLL '%ls' !\n", ptr->dll);
		return(NULL);
	}
    }

    /*
     * First, we need to know how many strings are in the table.
     * Sadly, there is no other way to determine this but to try
     * to load all possible ID's...
     */
    for (i = IDS_BEGIN; i < IDS_END; i++)
	if (LoadString(plat_lang_dll(), i, temp, sizeof_w(temp)) > 0) c++;

    /*
     * Now that we know how many strings exist, we can allocate
     * our string_table array.
     */
    i = (c + 1) * sizeof(string_t);
    array = (string_t *)mem_alloc(i);
    memset(array, 0x00, i);

    /* Now load the actual strings into our string table. */
    tbl = array;
    for (i = IDS_BEGIN; i < IDS_END; i++) {
	c = LoadString(plat_lang_dll(), i, temp, sizeof_w(temp));
	if (c == 0) continue;

	tbl->id = i;
	str = (wchar_t *)mem_alloc((c + 1) * sizeof(wchar_t));
	memset(str, 0x00, (c + 1) * sizeof(wchar_t));
	memcpy(str, temp, c * sizeof(wchar_t));
	tbl->str = str;

	tbl++;
    }

    /* Terminate the table. */
    tbl->str = NULL;

    return((const string_t *)array);
}


HMODULE
plat_lang_dll(void)
{
    if (lang_handle == NULL)
	return(hInstance);

    return(lang_handle);
}
