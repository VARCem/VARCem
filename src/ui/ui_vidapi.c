/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Handle the various video renderer modules.
 *
 * Version:	@(#)ui_vidapi.c	1.0.6	2019/04/29
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
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
#include <time.h>
#include "../emu.h"
#include "../device.h"
#include "../plat.h"
#include "../devices/video/video.h"
#include "ui.h"


/* Get availability of a VidApi entry. */
int
vidapi_available(int api)
{
    int ret = 1;

    if (plat_vidapis[api]->is_available != NULL)
	ret = plat_vidapis[api]->is_available();

    return(ret);
}


/* Return the VIDAPI number for the given name. */
int
vidapi_from_internal_name(const char *name)
{
    int i;

    if (!strcasecmp(name, "default") ||
	!strcasecmp(name, "system")) return(0);

    for (i = 0; plat_vidapis[i] != NULL; i++)
	if (! strcasecmp(plat_vidapis[i]->internal_name, name)) return(i);

    /* Default value. */
    return(0);
}


/* Return the VIDAPI name for the given number. */
const char *
vidapi_get_internal_name(int api)
{
    const char *name = "default";

    if (plat_vidapis[api] != NULL)
	return(plat_vidapis[api]->internal_name);

    return(name);
}


/* Return the VIDAPI dpslay name for the given number. */
const char *
vidapi_getname(int api)
{
    const char *name = NULL;

    if (plat_vidapis[api] != NULL)
	return(plat_vidapis[api]->name);

    return(name);
}


/* Set (initialize) a (new) API to use. */
int
vidapi_set(int api)
{
    int i;

    INFO("Initializing VIDAPI: api=%d\n", api);

    /* Lock the blitter. */
    plat_startblit();

    /* Wait for it to be ours. */
    video_blit_wait();

    /* Close the (old) API. */
    plat_vidapis[vid_api]->close();

    /* Initialize the (new) API. */
    vid_api = api;
    i = plat_vidapis[vid_api]->init(vid_fullscreen);

    /* Enable or disable the Render window. */
    ui_show_render(plat_vidapis[vid_api]->local);

    /* Update the menu item. */
    menu_set_radio_item(IDM_RENDER_1, vidapi_count(), vid_api);

    /* OK, release the blitter. */
    plat_endblit();

    /* If all OK, redraw the rendering area. */
    if (i)
	device_force_redraw();

    return(i);
}


/* Tell the renderers about a new screen resolution. */
void
vidapi_resize(int x, int y)
{
    /* If not defined, not supported or needed. */
    if (plat_vidapis[vid_api]->resize == NULL) return;

    /* Lock the blitter. */
    plat_startblit();

    /* Wait for it to be ours. */
    video_blit_wait();

    plat_vidapis[vid_api]->resize(x, y);

    /* Release the blitter. */
    plat_endblit();
}


int
vidapi_pause(void)
{
    /* If not defined, assume always OK. */
    if (plat_vidapis[vid_api]->pause == NULL) return(0);

    return(plat_vidapis[vid_api]->pause());
}


void
vidapi_enable(int yes)
{
    /* If not defined, assume not needed. */
    if (plat_vidapis[vid_api]->enable == NULL) return;

    /* Lock the blitter. */
    plat_startblit();

    /* Wait for it to be ours. */
    video_blit_wait();

    plat_vidapis[vid_api]->enable(yes);

    /* Release the blitter. */
    plat_endblit();
}


void
vidapi_reset(void)
{
    /* If not defined, assume always OK. */
    if (plat_vidapis[vid_api]->reset == NULL) return;

    plat_vidapis[vid_api]->reset(vid_fullscreen);
}


/* Try to get a screen-shot of the active rendering surface. */
void
vidapi_screenshot(void)
{
    wchar_t path[1024], fn[128];
    struct tm *info;
    time_t now;

    DEBUG("Screenshot: video API is: %i\n", vid_api);

    if (vid_api < 0) return;

    (void)time(&now);
    info = localtime(&now);

    memset(path, 0x00, sizeof(path));
    plat_append_filename(path, usr_path, SCREENSHOT_PATH);

    if (! plat_dir_check(path))
	plat_dir_create(path);

    plat_append_slash(path);

    wcsftime(fn, sizeof_w(fn), L"%Y%m%d_%H%M%S.png", info);
    wcscat(path, fn);

    if (plat_vidapis[vid_api]->screenshot != NULL)
	plat_vidapis[vid_api]->screenshot(path);
}
