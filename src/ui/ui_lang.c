/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the generic part of the language support.
 *
 * Version:	@(#)ui_lang.c	1.0.1	2018/10/05
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
#include "../emu.h"
#include "ui.h"
#include "../plat.h"


/* Local data. */
static const string_t	*strings = NULL;
static lang_t		*languages = NULL;
static int		languages_num = 0;


/* Set the correct item in the Language menu. */
static void
lang_select(int id)
{
    lang_t *ptr;
    int i = 0;

    for (ptr = languages; ptr != NULL; ptr = ptr->next) {
	if (ptr->id == id) {
		menu_set_radio_item(IDM_LANGUAGE+1, languages_num, i);
		return;
	}
	i++;
    }
}


/* Get language from index number. */
static lang_t *
lang_index(int id)
{
    lang_t *ptr;
    int i = 0;

    for (ptr = languages; ptr != NULL; ptr = ptr->next)
	if (i++ == id) return(ptr);

    return(NULL);
}


void
ui_lang_menu(void)
{
    lang_t *ptr;
    int i;

    i = IDM_LANGUAGE;
    for (ptr = languages; ptr != NULL; ptr = ptr->next) {
	i++;

	/* Add this language to the Languages menu. */
	menu_add_item(IDM_LANGUAGE, ITEM_RADIO, i, ptr->name);

#ifndef USE_WX
	/* Add separator after primary language. */
	//FIXME: with WX, a radio group cannot have any
	//       separators inbetween, it breaks the
	//       group. Maybe use 0 -sep- 1..n ?  --FvK
	if (ptr == languages)
		menu_add_item(IDM_LANGUAGE, ITEM_SEPARATOR, i, NULL);
#endif
    }

    /* Set active language. */
    lang_select(emu_lang_id);
}


/* Add a language to the list of supported languages. */
void
ui_lang_add(lang_t *ptr, int sort)
{
    lang_t *p, *pp;
    lang_t *prev = NULL;

    /* Create a copy of the entry data. */
    pp = (lang_t *)mem_alloc(sizeof(lang_t));
    memcpy(pp, ptr, sizeof(lang_t));

    /* Add this entry to the tail of the list. */
    if (sort) {
	p = languages;
	if ((p == NULL) || (wcscmp(pp->name, p->name) <= 0)) {
		pp->next = p;
		languages = pp;
	} else {
        	while (p != NULL) {
			if (wcscmp(pp->name, p->name) >= 0) {
				prev = p;
				p = p->next;
				continue;
			} else {
				prev->next = pp;
				pp->next = p;
				break;
			}
		}
		prev->next = pp;
        }
    } else {
	/* Just prepend to beginning. */
	pp->next = languages;
	languages = pp;
    }

    /* We got one more! */
    languages_num++;
}


/* Set (or re-set) the language for the application. */
int
ui_lang_set(int id)
{
    const string_t *str;
    lang_t *ptr;

    /* First, set up our language list if needed. */
    if (languages == NULL) plat_lang_scan();

    /* If a small value, this is a menu index, not an ID. */
    if (id < 200) {
	ptr = lang_index(id);
	id = ptr->id;
    }

    /* Set new language ID if not already set. */
    if (emu_lang_id == id) return(1);

    /* Find language in the table. */
    for (ptr = languages; ptr != NULL; ptr = ptr->next)
	if (ptr->id == id) break;
    if (ptr == NULL) {
	ERRLOG("UI: language not supported, not setting.\n");
	return(0);
    }

    /*
     * Load the language module.
     *
     * For Windows, this will load dialogs,
     * templates, icons and string tables.
     *
     * For UNIX, this will load strings.
     */
    str = plat_lang_load(ptr);
    if (str == NULL) {
	ERRLOG("UI: unable to load language module '%ls' !\n", ptr->dll);
	return(0);
    }

    /* We are good to go! */
    strings = str;
    emu_lang_id = id;

    /* Set active language. */
    lang_select(emu_lang_id);

    /* Activate the new language for the application. */
    plat_lang_set(emu_lang_id);

    /* All is good, language set! */
    return(1);
}


lang_t *
ui_lang_get(void)
{
    return(languages);
}


const wchar_t *
get_string(int id)
{
    static wchar_t buff[32];
    const wchar_t *str = NULL;
    const string_t *tbl;

    tbl = strings;
    while(tbl->str != NULL) {
	if (tbl->id == id) {
		str = tbl->str;
		break;
	}

	tbl++;
    }

    /* Just to catch strings not in the table. */
    if (str == NULL) {
	ERRLOG("UI: string %i not found in table!\n", id);
	swprintf(buff, sizeof_w(buff), L"<%i>", id);
	str = (const wchar_t *)buff;
    }

    return(str);
}
