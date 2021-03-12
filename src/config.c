/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Configuration file handler.
 *
 * NOTE:	Forcing config files to be in Unicode encoding breaks it
 *		on Windows XP, possibly Vista and several UNIX systems.
 *		Use the -DANSI_CFG for use on these systems.
 *
 * Version:	@(#)config.c	1.0.52	2021/03/10
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		David Simunic, <simunic.david@outlook.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "emu.h"
#include "config.h"
#include "timer.h"
#include "cpu/cpu.h"
#include "nvr.h"
#include "device.h"
#include "machines/machine.h"
#include "devices/misc/isamem.h"
#include "devices/misc/isartc.h"
#include "devices/ports/game_dev.h"
#include "devices/ports/serial.h"
#include "devices/ports/parallel.h"
#include "devices/ports/parallel_dev.h"
#include "devices/input/mouse.h"
#include "devices/input/game/joystick.h"
#include "devices/floppy/fdd.h"
#include "devices/floppy/fdc.h"
#include "devices/disk/hdd.h"
#include "devices/disk/hdc.h"
#include "devices/disk/hdc_ide.h"
#include "devices/scsi/scsi.h"
#include "devices/scsi/scsi_device.h"
#include "devices/cdrom/cdrom.h"
#include "devices/disk/zip.h"
#include "devices/network/network.h"
#include "devices/sound/sound.h"
#include "devices/sound/midi.h"
#include "devices/sound/sound.h"
#include "devices/video/video.h"
#include "ui/ui.h"
#include "plat.h"


#define ANSI_CFG		1		/* for now, always */


typedef struct _list_ {
    struct _list_ *next;
} list_t;

typedef struct {
    list_t	list;
    char	name[32];
    list_t	entry_head;
} section_t;

typedef struct {
    list_t	list;
    section_t	*sec;
    int8_t	quoted;
    int8_t	comment;
    char	name[64];
    char	data[256];
    wchar_t	wdata[256];
} entry_t;

#define list_add(new, head) {		\
    list_t *next = head;		\
					\
    while (next->next != NULL)		\
	next = next->next;		\
					\
    (next)->next = new;			\
    (new)->next = NULL;			\
}

#define list_delete(old, head) {	\
    list_t *next = head;		\
					\
    while ((next)->next != old) {	\
	next = (next)->next;		\
    }					\
					\
    (next)->next = (old)->next;		\
}


static list_t	config_head;


static section_t *
find_section(const char *name)
{
    const char *blank = "";
    section_t *sec;

    sec = (section_t *)config_head.next;
    if (name == NULL)
	name = blank;

    while (sec != NULL) {
	if (! strncmp(sec->name, name, sizeof(sec->name)))
				return(sec);

	sec = (section_t *)sec->list.next;
    }

    return(NULL);
}


static entry_t *
find_entry(section_t *section, const char *name)
{
    entry_t *ent;

    ent = (entry_t *)section->entry_head.next;

    while (ent != NULL) {
	if (! strncmp(ent->name, name, sizeof(ent->name)))
				return(ent);

	ent = (entry_t *)ent->list.next;
    }

    return(NULL);
}


static int
entries_num(section_t *section)
{
    entry_t *ent;
    int i = 0;

    ent = (entry_t *)section->entry_head.next;

    while (ent != NULL) {
	if (strlen(ent->name) > 0) i++;

	ent = (entry_t *)ent->list.next;
    }

    return(i);
}


static void
delete_section_if_empty(const char *cat)
{
    section_t *section;

    section = find_section(cat);
    if (section == NULL) return;

    if (entries_num(section) == 0) {
	list_delete(&section->list, &config_head);
	free(section);
    }
}


static section_t *
create_section(const char *name)
{
    section_t *ns = (section_t *)mem_alloc(sizeof(section_t));

    memset(ns, 0x00, sizeof(section_t));
    strncpy(ns->name, name, sizeof(ns->name) - 1);
    list_add(&ns->list, &config_head);

    return(ns);
}


static entry_t *
create_entry(section_t *section, const char *name)
{
    entry_t *ne = (entry_t *)mem_alloc(sizeof(entry_t));

    memset(ne, 0x00, sizeof(entry_t));
    strncpy(ne->name, name, sizeof(ne->name) - 1);
    list_add(&ne->list, &section->entry_head);

    return(ne);
}


#if 0
static void
config_free(void)
{
    section_t *sec, *ns;
    entry_t *ent;

    sec = (section_t *)config_head.next;
    while (sec != NULL) {
	ns = (section_t *)sec->list.next;
	ent = (entry_t *)sec->entry_head.next;

	while (ent != NULL) {
		entry_t *nent = (entry_t *)ent->list.next;

		free(ent);
		ent = nent;
	}

	free(sec);		
	sec = ns;
    }
}
#endif


/* Load "General" section. */
static void
load_general(config_t *cfg, const char *cat)
{
    wchar_t *w;
    char *p;

    memset(cfg->title, 0x00, sizeof(cfg->title));
    w = config_get_wstring(cat, "title", NULL);
    if (w != NULL)
	wcsncpy(cfg->title, w, TITLE_MAX);

    cfg->language = config_get_hex16(cat, "language", emu_lang_id);

    cfg->rctrl_is_lalt = config_get_int(cat, "rctrl_is_lalt", 0);

    cfg->vid_resize = !!config_get_int(cat, "vid_resize", 0);

    p = config_get_string(cat, "vid_renderer", "default");
    cfg->vid_api = vidapi_from_internal_name(p);

    cfg->vid_fullscreen_scale = config_get_int(cat, "video_fullscreen_scale", 0);

    cfg->vid_fullscreen_first = config_get_int(cat, "video_fullscreen_first", 0);

    cfg->force_43 = !!config_get_int(cat, "force_43", 0);
    cfg->scale = config_get_int(cat, "scale", 1);
    if (cfg->scale > 3)
	cfg->scale = 3;

    cfg->invert_display = !!config_get_int(cat, "invert_display", 0);
    cfg->enable_overscan = !!config_get_int(cat, "enable_overscan", 0);
    cfg->vid_cga_contrast = !!config_get_int(cat, "vid_cga_contrast", 0);
    cfg->vid_grayscale = config_get_int(cat, "video_grayscale", 0);
    cfg->vid_graytype = config_get_int(cat, "video_graytype", 0);

    cfg->window_remember = config_get_int(cat, "window_remember", 0);
    if (cfg->window_remember) {
	p = config_get_string(cat, "window_coordinates", "0, 0, 0, 0");
	sscanf(p, "%i, %i, %i, %i",
	       &cfg->win_w, &cfg->win_h, &cfg->win_x, &cfg->win_y);
    } else {
	config_delete_var(cat, "window_remember");
	config_delete_var(cat, "window_coordinates");

	cfg->win_w = cfg->win_h = cfg->win_x = cfg->win_y = 0;
    }

    cfg->sound_gain = config_get_int(cat, "sound_gain", 0);
}


/* Save "General" section. */
static void
save_general(const config_t *cfg, const char *cat)
{
    char temp[512];
    const char *str;

    if (cfg->title[0] == L'\0')
	config_delete_var(cat, "title");
    else
	config_set_wstring(cat, "title", cfg->title);

    if (emu_lang_id == 0x0409)
	config_delete_var(cat, "language");
    else
	config_set_hex16(cat, "language", emu_lang_id);

    if (cfg->rctrl_is_lalt == 0)
	config_delete_var(cat, "rctrl_is_lalt");
    else
	config_set_int(cat, "rctrl_is_lalt", cfg->rctrl_is_lalt);

    if (cfg->vid_resize == 0)
	config_delete_var(cat, "vid_resize");
    else
	config_set_int(cat, "vid_resize", cfg->vid_resize);

    str = vidapi_get_internal_name(cfg->vid_api);
    if (! strcmp(str, "default"))
	config_delete_var(cat, "vid_renderer");
    else
	config_set_string(cat, "vid_renderer", str);

    if (cfg->vid_fullscreen_scale == 0)
	config_delete_var(cat, "video_fullscreen_scale");
    else
	config_set_int(cat, "video_fullscreen_scale", cfg->vid_fullscreen_scale);

    if (cfg->vid_fullscreen_first == 0)
	config_delete_var(cat, "video_fullscreen_first");
    else
	config_set_int(cat, "video_fullscreen_first", cfg->vid_fullscreen_first);

    if (cfg->force_43 == 0)
	config_delete_var(cat, "force_43");
    else
	config_set_int(cat, "force_43", cfg->force_43);

    if (cfg->scale == 1)
	config_delete_var(cat, "scale");
    else
	config_set_int(cat, "scale", cfg->scale);

    if (cfg->invert_display == 0)
	config_delete_var(cat, "invert_display");
    else
	config_set_int(cat, "invert_display", cfg->invert_display);

    if (cfg->enable_overscan == 0)
	config_delete_var(cat, "enable_overscan");
    else
	config_set_int(cat, "enable_overscan", cfg->enable_overscan);

    if (cfg->vid_cga_contrast == 0)
	config_delete_var(cat, "vid_cga_contrast");
    else
	config_set_int(cat, "vid_cga_contrast", cfg->vid_cga_contrast);

    if (cfg->vid_grayscale == 0)
	config_delete_var(cat, "video_grayscale");
    else
	config_set_int(cat, "video_grayscale", cfg->vid_grayscale);

    if (cfg->vid_graytype == 0)
	config_delete_var(cat, "video_graytype");
    else
	config_set_int(cat, "video_graytype", cfg->vid_graytype);

    if (cfg->window_remember) {
	config_set_int(cat, "window_remember", cfg->window_remember);

	sprintf(temp, "%i, %i, %i, %i",
		cfg->win_w, cfg->win_h, cfg->win_x, cfg->win_y);
	config_set_string(cat, "window_coordinates", temp);
    } else {
	config_delete_var(cat, "window_remember");
	config_delete_var(cat, "window_coordinates");
    }

    if (cfg->sound_gain == 0)
	config_delete_var(cat, "sound_gain");
    else
	config_set_int(cat, "sound_gain", cfg->sound_gain);

    delete_section_if_empty(cat);
}


/* Load "Machine" section. */
static void
load_machine(config_t *cfg, const char *cat)
{
    char *p;

    p = config_get_string(cat, "machine", NULL);
    if (p != NULL)
	cfg->machine_type = machine_get_from_internal_name(p);
    else 
	cfg->machine_type = -1;
    if (machine_load() == NULL)
	cfg->machine_type = -1;

    cfg->cpu_manuf = config_get_int(cat, "cpu_manufacturer", 0);
    cfg->cpu_type = config_get_int(cat, "cpu", 0);
    cfg->cpu_waitstates = config_get_int(cat, "cpu_waitstates", 0);
    cfg->cpu_use_dynarec = !!config_get_int(cat, "cpu_use_dynarec", 0);
    cfg->enable_ext_fpu = !!config_get_int(cat, "cpu_enable_fpu", 0);

    cfg->mem_size = config_get_int(cat, "mem_size", 4096);
    cfg->mem_size = machine_get_memsize(cfg->mem_size);

    cfg->time_sync = config_get_int(cat, "enable_sync", -1);
    if (cfg->time_sync != -1) {
	/* FIXME: remove this after 12/31/2019 --FvK */
	config_delete_var(cat, "enable_sync");
    } else
	cfg->time_sync = config_get_int(cat, "time_sync", TIME_SYNC_DISABLED);
}


/* Save "Machine" section. */
static void
save_machine(const config_t *cfg, const char *cat)
{
    config_set_string(cat, "machine", machine_get_internal_name());

    if (cfg->cpu_manuf == 0)
	config_delete_var(cat, "cpu_manufacturer");
    else
	config_set_int(cat, "cpu_manufacturer", cfg->cpu_manuf);

    if (cfg->cpu_type == 0)
	config_delete_var(cat, "cpu");
    else
	config_set_int(cat, "cpu", cfg->cpu_type);

    if (cfg->cpu_waitstates == 0)
	config_delete_var(cat, "cpu_waitstates");
    else
	config_set_int(cat, "cpu_waitstates", cfg->cpu_waitstates);

    config_set_int(cat, "cpu_use_dynarec", cfg->cpu_use_dynarec);

    if (cfg->enable_ext_fpu == 0)
	config_delete_var(cat, "cpu_enable_fpu");
    else
	config_set_int(cat, "cpu_enable_fpu", cfg->enable_ext_fpu);

    config_set_int(cat, "mem_size", cfg->mem_size);

    if (cfg->time_sync == TIME_SYNC_DISABLED)
	config_delete_var(cat, "time_sync");
    else
	config_set_int(cat, "time_sync", cfg->time_sync);

    delete_section_if_empty(cat);
}


/* Load "Video" section. */
static void
load_video(config_t *cfg, const char *cat)
{
    char *p;

    if (machine_get_flags_fixed() & MACHINE_VIDEO) {
	config_delete_var(cat, "video_card");
	cfg->video_card = VID_INTERNAL;
    } else {
	p = config_get_string(cat, "video_card", NULL);
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_VIDEO)
			p = "internal";
		  else
			p = "none";
	}
	cfg->video_card = video_get_video_from_internal_name(p);
    }

    cfg->voodoo_enabled = !!config_get_int(cat, "voodoo", 0);
}


/* Save "Video" section. */
static void
save_video(const config_t *cfg, const char *cat)
{
    config_set_string(cat, "video_card",
		      video_get_internal_name(cfg->video_card));

    if (cfg->voodoo_enabled == 0)
	config_delete_var(cat, "voodoo");
    else
	config_set_int(cat, "voodoo", cfg->voodoo_enabled);

    delete_section_if_empty(cat);
}


/* Load "Input Devices" section. */
static void
load_input(config_t *cfg, const char *cat)
{
    char temp[512];
    int c, d;
    char *p;

    p = config_get_string(cat, "mouse_type", "none");
    cfg->mouse_type = mouse_get_from_internal_name(p);

    //FIXME: should be an internal_name string!!
    cfg->joystick_type = config_get_int(cat, "joystick_type", 0);

    for (c = 0; c < gamedev_get_max_joysticks(cfg->joystick_type); c++) {
	sprintf(temp, "joystick_%i_nr", c);
	joystick_state[c].plat_joystick_nr = config_get_int(cat, temp, 0);
//FIXME: state should be in cfg!
	if (joystick_state[c].plat_joystick_nr) {
		for (d = 0; d < gamedev_get_axis_count(cfg->joystick_type); d++) {
			sprintf(temp, "joystick_%i_axis_%i", c, d);
			joystick_state[c].axis_mapping[d] = config_get_int(cat, temp, d);
		}
		for (d = 0; d < gamedev_get_button_count(cfg->joystick_type); d++) {			
			sprintf(temp, "joystick_%i_button_%i", c, d);
			joystick_state[c].button_mapping[d] = config_get_int(cat, temp, d);
		}
		for (d = 0; d < gamedev_get_pov_count(cfg->joystick_type); d++) {
			sprintf(temp, "joystick_%i_pov_%i", c, d);
			p = config_get_string(cat, temp, "0, 0");
			joystick_state[c].pov_mapping[d][0] = joystick_state[c].pov_mapping[d][1] = 0;
			sscanf(p, "%i, %i", &joystick_state[c].pov_mapping[d][0], &joystick_state[c].pov_mapping[d][1]);
		}
	}
    }
}


/* Save "Input Devices" section. */
static void
save_input(const config_t *cfg, const char *cat)
{
    char temp[512], tmp2[512];
    int c, d;

    config_set_string(cat, "mouse_type",
		      mouse_get_internal_name(cfg->mouse_type));

    if (cfg->joystick_type != 0) {
	config_set_int(cat, "joystick_type", cfg->joystick_type);

	for (c = 0; c < gamedev_get_max_joysticks(cfg->joystick_type); c++) {
		sprintf(tmp2, "joystick_%i_nr", c);
		config_set_int(cat, tmp2, joystick_state[c].plat_joystick_nr);

		if (joystick_state[c].plat_joystick_nr) {
			for (d = 0; d < gamedev_get_axis_count(cfg->joystick_type); d++) {			
				sprintf(tmp2, "joystick_%i_axis_%i", c, d);
				config_set_int(cat, tmp2, joystick_state[c].axis_mapping[d]);
			}
			for (d = 0; d < gamedev_get_button_count(cfg->joystick_type); d++) {			
				sprintf(tmp2, "joystick_%i_button_%i", c, d);
				config_set_int(cat, tmp2, joystick_state[c].button_mapping[d]);
			}
			for (d = 0; d < gamedev_get_pov_count(cfg->joystick_type); d++) {			
				sprintf(tmp2, "joystick_%i_pov_%i", c, d);
				sprintf(temp, "%i, %i", joystick_state[c].pov_mapping[d][0], joystick_state[c].pov_mapping[d][1]);
				config_set_string(cat, tmp2, temp);
			}
		}
	}
    } else {
	config_delete_var(cat, "joystick_type");

	for (c = 0; c < 16; c++) {
		sprintf(tmp2, "joystick_%i_nr", c);
		config_delete_var(cat, tmp2);

		for (d = 0; d < 16; d++) {			
			sprintf(tmp2, "joystick_%i_axis_%i", c, d);
			config_delete_var(cat, tmp2);
		}
		for (d = 0; d < 16; d++) {			
			sprintf(tmp2, "joystick_%i_button_%i", c, d);
			config_delete_var(cat, tmp2);
		}
		for (d = 0; d < 16; d++) {			
			sprintf(tmp2, "joystick_%i_pov_%i", c, d);
			config_delete_var(cat, tmp2);
		}
	}
    }

    delete_section_if_empty(cat);
}


/* Load "Sound" section. */
static void
load_sound(config_t *cfg, const char *cat)
{
    char *p;

    p = config_get_string(cat, "sound_type", "float");
    if (!strcmp(p, "float") || !strcmp(p, "1"))
	cfg->sound_is_float = 1;
    else
	cfg->sound_is_float = 0;

    if (machine_get_flags_fixed() & MACHINE_SOUND) {
	config_delete_var(cat, "sound_card");
	cfg->sound_card = SOUND_INTERNAL;
    } else {
	p = config_get_string(cat, "sound_card", NULL);
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_SOUND)
			p = "internal";
	  	else
			p = "none";
	}
	cfg->sound_card = sound_card_get_from_internal_name(p);
    }

    p = config_get_string(cat, "midi_device", "none");
    cfg->midi_device = midi_device_get_from_internal_name(p);

    cfg->mpu401_standalone_enable = !!config_get_int(cat, "mpu401_standalone", 0);
}


/* Save "Sound" section. */
static void
save_sound(const config_t *cfg, const char *cat)
{
    if (cfg->sound_is_float == 1)
	config_delete_var(cat, "sound_type");
    else
	config_set_string(cat, "sound_type", (cfg->sound_is_float == 1) ? "float" : "int16");

    if (cfg->sound_card == SOUND_NONE)
	config_delete_var(cat, "sound_card");
    else
	config_set_string(cat, "sound_card",
			  sound_card_get_internal_name(cfg->sound_card));

    if (!strcmp(midi_device_get_internal_name(cfg->midi_device), "none"))
	config_delete_var(cat, "midi_device");
    else
	config_set_string(cat, "midi_device",
			  midi_device_get_internal_name(cfg->midi_device));

    if (cfg->mpu401_standalone_enable == 0)
	config_delete_var(cat, "mpu401_standalone");
    else
	config_set_int(cat, "mpu401_standalone", cfg->mpu401_standalone_enable);

    delete_section_if_empty(cat);
}


/* Load "Network" section. */
static void
load_network(config_t *cfg, const char *cat)
{
    char *p;

    p = config_get_string(cat, "net_type", "none");
    cfg->network_type = network_get_from_internal_name(p);

    memset(cfg->network_host, '\0', sizeof(cfg->network_host));
    p = config_get_string(cat, "net_host_device", NULL);
    if (p == NULL) {
	p = config_get_string(cat, "net_pcap_device", NULL);
	if (p != NULL)
		config_delete_var(cat, "net_pcap_device");
    }
    if (p != NULL) {
	if ((network_card_to_id(p) == -1) || (network_host_ndev == 1)) {
		if ((network_host_ndev == 1) && strcmp(cfg->network_host, "none")) {
			ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_PCAP_NO);
		} else if (network_card_to_id(p) == -1) {
			ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_PCAP_DEV);
		}

		strcpy(cfg->network_host, "none");
	} else {
		strcpy(cfg->network_host, p);
	}
    } else
	strcpy(cfg->network_host, "none");

    if (machine_get_flags_fixed() & MACHINE_NETWORK) {
	config_delete_var(cat, "net_card");
	cfg->network_card = NET_CARD_INTERNAL;
    } else {
	p = config_get_string(cat, "net_card", NULL);
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_NETWORK)
			p = "internal";
	  	else
			p = "none";
	}
	cfg->network_card = network_card_get_from_internal_name(p);
    }
}


/* Save "Network" section. */
static void
save_network(const config_t *cfg, const char *cat)
{
    if (cfg->network_type == NET_NONE)
	config_delete_var(cat, "net_type");
    else
	config_set_string(cat, "net_type",
			  network_get_internal_name(cfg->network_type));

    if (cfg->network_host[0] != '\0') {
	if (! strcmp(cfg->network_host, "none"))
		config_delete_var(cat, "net_host_device");
	else
		config_set_string(cat, "net_host_device", cfg->network_host);
    } else {
	config_delete_var(cat, "net_host_device");
    }

    if (cfg->network_card == 0)
	config_delete_var(cat, "net_card");
    else
	config_set_string(cat, "net_card",
			  network_card_get_internal_name(cfg->network_card));

    delete_section_if_empty(cat);
}


/* Load "Ports" section. */
static void
load_ports(config_t *cfg, const char *cat)
{
    char temp[128];
    char *p;
    int i;

    sprintf(temp, "game_enabled");
    cfg->game_enabled = !!config_get_int(cat, temp, 0);

    for (i = 0; i < SERIAL_MAX; i++) {
	sprintf(temp, "serial%i_enabled", i);
	cfg->serial_enabled[i] = !!config_get_int(cat, temp, 0);
    }

    for (i = 0; i < PARALLEL_MAX; i++) {
	sprintf(temp, "parallel%i_enabled", i);
	cfg->parallel_enabled[i] = !!config_get_int(cat, temp, 0);

	sprintf(temp, "parallel%i_device", i);
	p = (char *)config_get_string(cat, temp, "none");
	cfg->parallel_device[i] = parallel_device_get_from_internal_name(p);
    }
}


/* Save "Ports" section. */
static void
save_ports(const config_t *cfg, const char *cat)
{
    char temp[128];
    int i;

    sprintf(temp, "game_enabled");
    if (cfg->game_enabled) {
	config_set_int(cat, temp, 1);
    } else
	config_delete_var(cat, temp);

    for (i = 0; i < SERIAL_MAX; i++) {
	sprintf(temp, "serial%i_enabled", i);
	if (cfg->serial_enabled[i]) {
		config_set_int(cat, temp, 1);
	} else
		config_delete_var(cat, temp);
    }

    for (i = 0; i < PARALLEL_MAX; i++) {
	sprintf(temp, "parallel%i_enabled", i);
	if (cfg->parallel_enabled[i])
		config_set_int(cat, temp, 1);
	else
		config_delete_var(cat, temp);

	sprintf(temp, "parallel%i_device", i);
	if (cfg->parallel_device[i] != 0)
		config_set_string(cat, temp,
			parallel_device_get_internal_name(cfg->parallel_device[i]));
	else
		config_delete_var(cat, temp);
    }

    delete_section_if_empty(cat);
}


/* Load "Other Peripherals" section. */
static void
load_other(config_t *cfg, const char *cat)
{
    char temp[128], *p;
    int c;

    if (machine_get_flags_fixed() & MACHINE_FDC) {
	config_delete_var(cat, "fdc");
	cfg->fdc_type = FDC_INTERNAL;
    } else {
	p = config_get_string(cat, "fdc", NULL);
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_FDC)
			p = "internal";
	  	else
			p = "none";
	}
#if 0	/* not yet */
	cfg->fdc_type = fdc_get_from_internal_name(p);
#else
	if (! strcmp(p, "none"))
		cfg->fdc_type = 0;
	if (! strcmp(p, "internal"))
		cfg->fdc_type = 1;

	if (! strcmp(p, "dtk_pii151b"))
		cfg->fdc_type = 10;
	if (! strcmp(p, "dtk_pii158b"))
		cfg->fdc_type = 11;
#endif
    }

    if (machine_get_flags_fixed() & MACHINE_HDC) {
	config_delete_var(cat, "hdc");
	cfg->hdc_type = HDC_INTERNAL;
    } else {
	p = config_get_string(cat, "hdc", NULL);
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_HDC)
			p = "internal";
	  	else
			p = "none";
	}
	cfg->hdc_type = hdc_get_from_internal_name(p);
    }

    cfg->ide_ter_enabled = !!config_get_int(cat, "ide_ter", 0);
    cfg->ide_qua_enabled = !!config_get_int(cat, "ide_qua", 0);

    if (machine_get_flags_fixed() & MACHINE_SCSI) {
	config_delete_var(cat, "scsi_card");
	cfg->scsi_card = SCSI_INTERNAL;
    } else {
	p = config_get_string(cat, "scsi_card", "none");
	if (p == NULL) {
		if (machine_get_flags() & MACHINE_SCSI)
			p = "internal";
	  	else
			p = "none";
	}
	cfg->scsi_card = scsi_card_get_from_internal_name(p);
    }

    cfg->bugger_enabled = !!config_get_int(cat, "bugger_enabled", 0);

    for (c = 0; c < ISAMEM_MAX; c++) {
	sprintf(temp, "isamem%i_type", c);

	p = config_get_string(cat, temp, "none");
	cfg->isamem_type[c] = isamem_get_from_internal_name(p);
    }

    p = config_get_string(cat, "isartc_type", "none");
    cfg->isartc_type = isartc_get_from_internal_name(p);

    cfg->ami_disk = !!config_get_int(cat, "amidisk", 0);

#ifdef WALTJE
    cfg->romdos_enabled = !!config_get_int(cat, "romdos_enabled", 0);
#endif
}


/* Save "Other Peripherals" section. */
static void
save_other(const config_t *cfg, const char *cat)
{
    char temp[512];
    int c;

    if (cfg->fdc_type == FDC_NONE)
	config_delete_var(cat, "fdc");
    else {
#if 0	/* not yet */
	config_set_string(cat, "fdc",
			  fdc_get_internal_name(cfg->fdc_type));
#else
	switch(cfg->fdc_type) {
		case FDC_NONE:
			strcpy(temp, "none");		break;

		case FDC_INTERNAL:
			strcpy(temp, "internal");	break;

		case 10:
			strcpy(temp, "dtk_pii151b");	break;

		case 11:
			strcpy(temp, "dtk_pii158b");	break;
	}
#endif
    }

    if (cfg->hdc_type == HDC_NONE)
	config_delete_var(cat, "hdc");
    else
	config_set_string(cat, "hdc",
			  hdc_get_internal_name(cfg->hdc_type));

    if (cfg->ide_ter_enabled == 0)
	config_delete_var(cat, "ide_ter");
    else
	config_set_int(cat, "ide_ter", cfg->ide_ter_enabled);

    if (cfg->ide_qua_enabled == 0)
	config_delete_var(cat, "ide_qua");
    else
	config_set_int(cat, "ide_qua", cfg->ide_qua_enabled);

    if (cfg->scsi_card == SCSI_NONE)
	config_delete_var(cat, "scsi_card");
    else
	config_set_string(cat, "scsi_card",
			  scsi_card_get_internal_name(cfg->scsi_card));

    if (cfg->bugger_enabled == 0)
	config_delete_var(cat, "bugger_enabled");
    else
	config_set_int(cat, "bugger_enabled", cfg->bugger_enabled);

    for (c = 0; c < ISAMEM_MAX; c++) {
	sprintf(temp, "isamem%i_type", c);
	if (cfg->isamem_type[c] == 0)
		config_delete_var(cat, temp);
	else
		config_set_string(cat, temp,
				  isamem_get_internal_name(cfg->isamem_type[c]));
    }

    if (cfg->isartc_type == 0)
	config_delete_var(cat, "isartc_type");
    else
	config_set_string(cat, "isartc_type",
			  isartc_get_internal_name(cfg->isartc_type));

    delete_section_if_empty(cat);
}


/* Load "Hard Disks" section. */
//FIXME: hdd[] should be loaded into config_t
static void
load_disks(config_t *cfg, const char *cat)
{
    char temp[512], tmp2[512], s[512];
    uint32_t max_spt, max_hpc, max_tracks;
    uint32_t board = 0, dev = 0;
    wchar_t *wp;
    char *p;
    int c;

    for (c = 0; c < HDD_NUM; c++) {
	sprintf(temp, "hdd_%02i_parameters", c+1);
	p = config_get_string(cat, temp, "0, 0, 0, 0, none");

	sscanf(p, "%u, %u, %u, %i, %s",
	       (unsigned *)&hdd[c].spt, (unsigned *)&hdd[c].hpc,
		(unsigned *)&hdd[c].tracks, (int *)&hdd[c].wp, s);

	hdd[c].bus = hdd_string_to_bus(s);
	switch(hdd[c].bus) {
		case HDD_BUS_DISABLED:
		default:
			max_spt = max_hpc = max_tracks = 0;
			break;

		case HDD_BUS_ST506:
			max_spt = 26;	/* 17=MFM, 26=RLL */
			max_hpc = 15;
			max_tracks = 1023;
			break;

		case HDD_BUS_ESDI:
			max_spt = 63;
			max_hpc = 16;
			max_tracks = 1023;
			break;

		case HDD_BUS_IDE:
			max_spt = 63;
			max_hpc = 16;
			max_tracks = 266305;
			break;

		case HDD_BUS_SCSI:
			max_spt = 99;
			max_hpc = 255;
			max_tracks = 266305;
			break;
	}

	if (hdd[c].spt > max_spt)
		hdd[c].spt = max_spt;
	if (hdd[c].hpc > max_hpc)
		hdd[c].hpc = max_hpc;
	if (hdd[c].tracks > max_tracks)
		hdd[c].tracks = max_tracks;

	/* ST506 */
	sprintf(temp, "hdd_%02i_st506_channel", c+1);
	if (hdd[c].bus == HDD_BUS_ST506) {
		/* Try new syntax. */
		dev = config_get_int(cat, temp, -1);
		hdd[c].bus_id.st506_channel = dev;
	} else
		config_delete_var(cat, temp);

	/* ESDI */
	sprintf(temp, "hdd_%02i_esdi_channel", c+1);
	if (hdd[c].bus == HDD_BUS_ESDI)
		hdd[c].bus_id.esdi_channel = !!config_get_int(cat, temp, c & 1);
	  else
		config_delete_var(cat, temp);

	/* IDE */
	sprintf(temp, "hdd_%02i_ide_channel", c+1);
	if (hdd[c].bus == HDD_BUS_IDE) {
		sprintf(tmp2, "%01u:%01u", c>>1, c&1);
		p = config_get_string(cat, temp, tmp2);
		sscanf(p, "%01u:%01u", &board, &dev);
		board &= 3;
		dev &= 1;
		hdd[c].bus_id.ide_channel = (board<<1) + dev;

		if (hdd[c].bus_id.ide_channel > 7)
			hdd[c].bus_id.ide_channel = 7;
	} else {
		config_delete_var(cat, temp);
	}

	/* SCSI */
	sprintf(temp, "hdd_%02i_scsi_location", c+1);
	if (hdd[c].bus == HDD_BUS_SCSI) {
		sprintf(tmp2, "%02u:%02u", c, 0);
		p = config_get_string(cat, temp, tmp2);

		sscanf(p, "%02u:%02u",
			(int *)&hdd[c].bus_id.scsi.id, (int *)&hdd[c].bus_id.scsi.lun);

		if (hdd[c].bus_id.scsi.id > 15)
			hdd[c].bus_id.scsi.id = 15;
		if (hdd[c].bus_id.scsi.lun > 7)
			hdd[c].bus_id.scsi.lun = 7;
	} else {
		config_delete_var(cat, temp);
	}

	memset(hdd[c].fn, 0x00, sizeof(hdd[c].fn));
	memset(hdd[c].prev_fn, 0x00, sizeof(hdd[c].prev_fn));
	sprintf(temp, "hdd_%02i_fn", c+1);
	wp = config_get_wstring(cat, temp, L"");

	/* Try to make relative, and copy to destination. */
	pc_path(hdd[c].fn, sizeof_w(hdd[c].fn), wp);

	/* If disk is empty or invalid, mark it for deletion. */
	if (! hdd_is_valid(c)) {
		sprintf(temp, "hdd_%02i_parameters", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "hdd_%02i_ide_channels", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "hdd_%02i_scsi_location", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "hdd_%02i_fn", c+1);
		config_delete_var(cat, temp);
	}
    }
}


/* Save "Hard Disks" section. */
static void
save_disks(const config_t *cfg, const char *cat)
{
    char temp[128], tmp2[128];
    const char *str;
    int c;

    for (c = 0; c < HDD_NUM; c++) {
	sprintf(temp, "hdd_%02i_parameters", c+1);
	if (hdd_is_valid(c)) {
		str = hdd_bus_to_string(hdd[c].bus);
		sprintf(tmp2, "%u, %u, %u, %i, %s",
			hdd[c].spt, hdd[c].hpc, hdd[c].tracks, hdd[c].wp, str);
		config_set_string(cat, temp, tmp2);
	} else {
		config_delete_var(cat, temp);
	}

	sprintf(temp, "hdd_%02i_st506_channel", c+1);
	if (hdd_is_valid(c) && (hdd[c].bus == HDD_BUS_ST506))
		config_set_int(cat, temp, hdd[c].bus_id.st506_channel);
	  else
		config_delete_var(cat, temp);

	sprintf(temp, "hdd_%02i_esdi_channel", c+1);
	if (hdd_is_valid(c) && (hdd[c].bus == HDD_BUS_ESDI))
		config_set_int(cat, temp, hdd[c].bus_id.esdi_channel);
	  else
		config_delete_var(cat, temp);

	sprintf(temp, "hdd_%02i_ide_channel", c+1);
	if (!hdd_is_valid(c) || (hdd[c].bus != HDD_BUS_IDE)) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%01u:%01u",
			hdd[c].bus_id.ide_channel >> 1,
			hdd[c].bus_id.ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "hdd_%02i_scsi_location", c+1);
	if (!hdd_is_valid(c) || (hdd[c].bus != HDD_BUS_SCSI)) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02i:%02i",
			hdd[c].bus_id.scsi.id, hdd[c].bus_id.scsi.lun);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "hdd_%02i_fn", c+1);
	if (hdd_is_valid(c) && (wcslen(hdd[c].fn) != 0))
		config_set_wstring(cat, temp, hdd[c].fn);
	  else
		config_delete_var(cat, temp);
    }

    delete_section_if_empty(cat);
}


/* Load "Floppy Drives" section. */
//FIXME: we should load stuff into config_t !
static void
load_floppy(config_t *cfg, const char *cat)
{
    char temp[512], *p;
    wchar_t *wp;
    int c;

    for (c = 0; c < FDD_NUM; c++) {
	sprintf(temp, "fdd_%02i_type", c+1);
	p = config_get_string(cat, temp, (c < 2) ? "525_2dd" : "none");
       	fdd_set_type(c, fdd_get_from_internal_name(p));
	if (fdd_get_type(c) > 13)
		fdd_set_type(c, 13);

	/* Try to make paths relative, and copy to destination. */
	sprintf(temp, "fdd_%02i_fn", c + 1);
	wp = config_get_wstring(cat, temp, L"");
	pc_path(floppyfns[c], sizeof_w(floppyfns[c]), wp);

	sprintf(temp, "fdd_%02i_writeprot", c+1);
	ui_writeprot[c] = !!config_get_int(cat, temp, 0);
	sprintf(temp, "fdd_%02i_turbo", c + 1);
	fdd_set_turbo(c, !!config_get_int(cat, temp, 0));
	sprintf(temp, "fdd_%02i_check_bpb", c+1);
	fdd_set_check_bpb(c, !!config_get_int(cat, temp, 1));

	/*
	 * Check whether or not each value is default, and if yes,
	 * delete it so that only non-default values will later be
         * saved.
	 */
	if (fdd_get_type(c) == ((c < 2) ? 2 : 0)) {
		sprintf(temp, "fdd_%02i_type", c+1);
		config_delete_var(cat, temp);
	}
	if (wcslen(floppyfns[c]) == 0) {
		sprintf(temp, "fdd_%02i_fn", c+1);
		config_delete_var(cat, temp);
	}
	if (! ui_writeprot[c]) {
		sprintf(temp, "fdd_%02i_writeprot", c+1);
		config_delete_var(cat, temp);
	}
	if (! fdd_get_turbo(c)) {
		sprintf(temp, "fdd_%02i_turbo", c+1);
		config_delete_var(cat, temp);
	}
	if (! fdd_get_check_bpb(c)) {
		sprintf(temp, "fdd_%02i_check_bpb", c+1);
		config_delete_var(cat, temp);
	}
    }
}


/* Save "Floppy Drives" section. */
static void
save_floppy(const config_t *cfg, const char *cat)
{
    char temp[512];
    int c;

    for (c = 0; c < FDD_NUM; c++) {
	sprintf(temp, "fdd_%02i_type", c+1);
	if (fdd_get_type(c) == ((c < 2) ? 2 : 0))
		config_delete_var(cat, temp);
	  else
		config_set_string(cat, temp,
				  fdd_get_internal_name(fdd_get_type(c)));

	sprintf(temp, "fdd_%02i_fn", c+1);
	if (wcslen(floppyfns[c]) == 0) {
		config_delete_var(cat, temp);

		ui_writeprot[c] = 0;

		sprintf(temp, "fdd_%02i_writeprot", c+1);
		config_delete_var(cat, temp);
	} else
		config_set_wstring(cat, temp, floppyfns[c]);

	sprintf(temp, "fdd_%02i_writeprot", c+1);
	if (ui_writeprot[c] == 0)
		config_delete_var(cat, temp);
	else
		config_set_int(cat, temp, ui_writeprot[c]);

	sprintf(temp, "fdd_%02i_turbo", c+1);
	if (fdd_get_turbo(c) == 0)
		config_delete_var(cat, temp);
	else
		config_set_int(cat, temp, fdd_get_turbo(c));

	sprintf(temp, "fdd_%02i_check_bpb", c+1);
	if (fdd_get_check_bpb(c) == 1)
		config_delete_var(cat, temp);
	else
		config_set_int(cat, temp, fdd_get_check_bpb(c));
    }

    delete_section_if_empty(cat);
}


/* Load "Removable Devices" section. */
//FIXME: stuff should be loaded into config_t !
static void
load_removable(config_t *cfg, const char *cat)
{
    char temp[512], tmp2[512], *p;
    char s[512];
    unsigned int board = 0, dev = 0;
    wchar_t *wp;
    int c;

    for (c = 0; c < CDROM_NUM; c++) {
	sprintf(temp, "cdrom_%02i_host_drive", c+1);
	cdrom[c].host_drive = config_get_int(cat, temp, 0);
	cdrom[c].prev_host_drive = cdrom[c].host_drive;

	sprintf(temp, "cdrom_%02i_parameters", c+1);
	p = config_get_string(cat, temp, "0, none");
	sscanf(p, "%01u, %s", (int *)&cdrom[c].sound_on, s);
	cdrom[c].bus_type = cdrom_string_to_bus(s);

	sprintf(temp, "cdrom_%02i_speed", c+1);
	cdrom[c].speed_idx = config_get_int(cat, temp, cdrom_speed_idx(CDROM_SPEED_DEFAULT));

	/* Default values, needed for proper operation of the Settings dialog. */
	cdrom[c].bus_id.ide_channel = cdrom[c].bus_id.scsi.id = c + 2;

	sprintf(temp, "cdrom_%02i_ide_channel", c+1);
	if (cdrom[c].bus_type == CDROM_BUS_ATAPI) {
		sprintf(tmp2, "%01u:%01u", (c+2)>>1, (c+2)&1);
		p = config_get_string(cat, temp, tmp2);
		sscanf(p, "%01u:%01u", &board, &dev);
		board &= 3;
		dev &= 1;
		cdrom[c].bus_id.ide_channel = (board<<1)+dev;

		if (cdrom[c].bus_id.ide_channel > 7)
			cdrom[c].bus_id.ide_channel = 7;
	} else {
		sprintf(temp, "cdrom_%02i_scsi_location", c+1);
		if (cdrom[c].bus_type == CDROM_BUS_SCSI) {
			sprintf(tmp2, "%02u:%02u", c+2, 0);
			p = config_get_string(cat, temp, tmp2);
			sscanf(p, "%02u:%02u",
				(int *)&cdrom[c].bus_id.scsi.id,
				(int *)&cdrom[c].bus_id.scsi.lun);
	
			if (cdrom[c].bus_id.scsi.id > 15)
				cdrom[c].bus_id.scsi.id = 15;
			if (cdrom[c].bus_id.scsi.lun > 7)
				cdrom[c].bus_id.scsi.lun = 7;
		} else {
			config_delete_var(cat, temp);
		}
	}

	sprintf(temp, "cdrom_%02i_image_path", c+1);
	wp = config_get_wstring(cat, temp, L"");

	/* Try to make relative, and copy to destination. */
	pc_path(cdrom[c].image_path, sizeof_w(cdrom[c].image_path), wp);

	if (cdrom[c].host_drive < 'A')
		cdrom[c].host_drive = 0;

	if ((cdrom[c].host_drive == 0x200) &&
	    (wcslen(cdrom[c].image_path) == 0))
		cdrom[c].host_drive = 0;

	/* If the CD-ROM is disabled, delete all its variables. */
	if (cdrom[c].bus_type == CDROM_BUS_DISABLED) {
		sprintf(temp, "cdrom_%02i_host_drive", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "cdrom_%02i_parameters", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "cdrom_%02i_ide_channel", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "cdrom_%02i_scsi_location", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "cdrom_%02i_image_path", c+1);
		config_delete_var(cat, temp);
	}

	sprintf(temp, "cdrom_%02i_iso_path", c+1);
	config_delete_var(cat, temp);
    }

    for (c = 0; c < ZIP_NUM; c++) {
	sprintf(temp, "zip_%02i_parameters", c+1);
	p = config_get_string(cat, temp, "0, none");
	sscanf(p, "%01u, %s", (unsigned *)&zip_drives[c].is_250, s);
	zip_drives[c].bus_type = zip_string_to_bus(s);

	/* Default values, needed for proper operation of the Settings dialog. */
	zip_drives[c].bus_id.ide_channel = zip_drives[c].bus_id.scsi.id = c + 2;

	sprintf(temp, "zip_%02i_ide_channel", c+1);
	if (zip_drives[c].bus_type == ZIP_BUS_ATAPI) {
		sprintf(tmp2, "%01u:%01u", (c+2)>>1, (c+2)&1);
		p = config_get_string(cat, temp, tmp2);
		sscanf(p, "%01u:%01u", &board, &dev);

		board &= 3;
		dev &= 1;
		zip_drives[c].bus_id.ide_channel = (board<<1)+dev;

		if (zip_drives[c].bus_id.ide_channel > 7)
			zip_drives[c].bus_id.ide_channel = 7;
	} else {
		sprintf(temp, "zip_%02i_scsi_location", c+1);
		if (zip_drives[c].bus_type == CDROM_BUS_SCSI) {
			sprintf(tmp2, "%02u:%02u", c+2, 0);
			p = config_get_string(cat, temp, tmp2);
			sscanf(p, "%02u:%02u",
				(unsigned *)&zip_drives[c].bus_id.scsi.id,
				(unsigned *)&zip_drives[c].bus_id.scsi.lun);
	
			if (zip_drives[c].bus_id.scsi.id > 15)
				zip_drives[c].bus_id.scsi.id = 15;
			if (zip_drives[c].bus_id.scsi.lun > 7)
				zip_drives[c].bus_id.scsi.lun = 7;
		} else {
			config_delete_var(cat, temp);
		}
	}

	sprintf(temp, "zip_%02i_image_path", c+1);
	wp = config_get_wstring(cat, temp, L"");

	/* Try to make relative, and copy to destination. */
	pc_path(zip_drives[c].image_path, sizeof_w(zip_drives[c].image_path), wp);

	/* If the CD-ROM is disabled, delete all its variables. */
	if (zip_drives[c].bus_type == ZIP_BUS_DISABLED) {
		sprintf(temp, "zip_%02i_host_drive", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "zip_%02i_parameters", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "zip_%02i_ide_channel", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "zip_%02i_scsi_location", c+1);
		config_delete_var(cat, temp);

		sprintf(temp, "zip_%02i_image_path", c+1);
		config_delete_var(cat, temp);
	}

	sprintf(temp, "zip_%02i_iso_path", c+1);
	config_delete_var(cat, temp);
    }
}


/* Save "Other Removable Devices" section. */
static void
save_removable(const config_t *cfg, const char *cat)
{
    char temp[512], tmp2[512];
    int c;

    for (c = 0; c < CDROM_NUM; c++) {
	sprintf(temp, "cdrom_%02i_host_drive", c+1);
	if ((cdrom[c].bus_type == 0) ||
	    (cdrom[c].host_drive < 'A') || ((cdrom[c].host_drive > 'Z') && (cdrom[c].host_drive != 200))) {
		config_delete_var(cat, temp);
	} else {
		config_set_int(cat, temp, cdrom[c].host_drive);
	}

	sprintf(temp, "cdrom_%02i_speed", c+1);
	if ((cdrom[c].bus_type == 0) ||
		(cdrom_speeds[cdrom[c].speed_idx].speed == cdrom_speed_idx(CDROM_SPEED_DEFAULT))) {
		config_delete_var(cat, temp);
	} else {
		config_set_int(cat, temp, cdrom[c].speed_idx);
	}

	sprintf(temp, "cdrom_%02i_parameters", c+1);
	if (cdrom[c].bus_type == 0) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%u, %s", cdrom[c].sound_on,
			cdrom_bus_to_string(cdrom[c].bus_type));
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_ide_channel", c+1);
	if (cdrom[c].bus_type != CDROM_BUS_ATAPI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%01u:%01u", cdrom[c].bus_id.ide_channel>>1,
					cdrom[c].bus_id.ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_scsi_location", c + 1);
	if (cdrom[c].bus_type != CDROM_BUS_SCSI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02u:%02u",
			cdrom[c].bus_id.scsi.id, cdrom[c].bus_id.scsi.lun);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_image_path", c + 1);
	if ((cdrom[c].bus_type == 0) ||
	    (wcslen(cdrom[c].image_path) == 0)) {
		config_delete_var(cat, temp);
	} else {
		config_set_wstring(cat, temp, cdrom[c].image_path);
	}
    }

    for (c = 0; c < ZIP_NUM; c++) {
	sprintf(temp, "zip_%02i_parameters", c+1);
	if (zip_drives[c].bus_type == 0) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%u, %s", zip_drives[c].is_250,
			zip_bus_to_string(zip_drives[c].bus_type));
		config_set_string(cat, temp, tmp2);
	}
		
	sprintf(temp, "zip_%02i_ide_channel", c+1);
	if (zip_drives[c].bus_type != ZIP_BUS_ATAPI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%01u:%01u", zip_drives[c].bus_id.ide_channel>>1,
					zip_drives[c].bus_id.ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "zip_%02i_scsi_location", c + 1);
	if (zip_drives[c].bus_type != ZIP_BUS_SCSI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02u:%02u", zip_drives[c].bus_id.scsi.id,
					zip_drives[c].bus_id.scsi.lun);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "zip_%02i_image_path", c + 1);
	if ((zip_drives[c].bus_type == 0) ||
	    (wcslen(zip_drives[c].image_path) == 0)) {
		config_delete_var(cat, temp);
	} else {
		config_set_wstring(cat, temp, zip_drives[c].image_path);
	}
    }

    delete_section_if_empty(cat);
}


static const struct {
    const char	*name;
    void	(*load)(config_t *, const char *);
    void	(*save)(const config_t *, const char *);
} categories[] = {
  { "General",		load_general,		save_general		},
  { "Machine",		load_machine,		save_machine		},
  { "Video",		load_video,		save_video		},
  { "Input devices",	load_input,		save_input		},
  { "Sound",		load_sound,		save_sound		},
  { "Network",		load_network,		save_network		},
  { "Ports (COM & LPT)",load_ports,		save_ports		},
  { "Other peripherals",load_other,		save_other		},
  { "Hard disks",	load_disks,		save_disks		},
  { "Floppy drives",	load_floppy,		save_floppy		},
  { "Other removable devices",load_removable,	save_removable		},
  { NULL,		NULL,			NULL			}
};


/*
 * Read and parse the configuration file into memory.
 *
 * This is not trivial, since we allow for a fairly standard
 * Unix-style ASCII-based text file, including the usual line
 * extenders and character escapes.
 *
 * We also allow for multi-lingual (wide) characters to be in
 * the file, so care must be taken reading from it.
 */
static int
config_read(const wchar_t *fn)
{
    char sname[32], ename[32];
    wchar_t cline[1024], *cp;
    wchar_t line[1024], *p;
    int quoted, comment;
    int doquot, dolit;
    section_t *sec, *ns;
    entry_t *ne;
    FILE *fp;
    int c, l;

    /* See if we can open the file for reading. */
#if defined(ANSI_CFG) || !defined(_WIN32)
    fp = plat_fopen(fn, L"rt");
#else
    fp = plat_fopen(fn, L"rt, ccs=UNICODE");
#endif
    if (fp == NULL) return(0);

    sec = (section_t *)mem_alloc(sizeof(section_t));
    memset(sec, 0x00, sizeof(section_t));
    memset(&config_head, 0x00, sizeof(list_t));
    list_add(&sec->list, &config_head);

    l = 1;

    for (;;) {
	/* Clear the per-line stuff. */
	dolit = doquot = comment = quoted = 0;
	p = line;
	cp = cline;

	/* Get the next logical line from the file. */
	while ((c = fgetwc(fp)) != WEOF) {
		/* Literals come first... */
		if (dolit) {
			switch (c) {
				case L'\r':	// raw CR, ignore it
					continue;

				case L'\n':	// line continuation!
					l++;
					break;

				case L'n':
					*p++ = L'\n';
					break;

				case L'r':
					*p++ = L'\r';
					break;

				case L'b':
					*p++ = L'\b';
					break;

				case L'e':
					*p++ = 27;
					break;

				case L'"':
					*p++ = L'"';
					break;

				case L'#':
					*p++ = L'#';
					break;

				case L'!':
					*p++ = L'!';
					break;

				case L'\\':
					*p++ = L'\\';
					break;

				default:
					ERRLOG("CONFIG: syntax error: escape '\\%c' on line %i", c, l);
					config_ro = 1;
					return(0);
			}

			dolit = 0;
			continue;
		}

		/* Are we starting a literal character? */
		if (c == L'\\') {
			dolit = 1;
			continue;
		}

		/* Raw CRs are common in DOS-originated files. */
		if (c == L'\r')
			continue;

		/* A raw newline terminates the logical line. */
		if (c == L'\n')
			break;

		/* Are we in a comment? */
		if (comment) {
			/* Save into comment buffer. */
			if (c != L'\n')
				*cp++ = (wchar_t)c;
			else
				break;

			continue;
		}

		/* Are we starting a comment? */
		if ((c == L';') || (c == L'#')) {
			/* Terminate value string. */
			*p = L'\0';

			/*
			 * Normally, one would just skip all the comments
			 * in a file. However, as these might be used to
			 * manually document settings, we try to preserve
			 * them.
			 *
			 * We save the current variable pointer to SP, and
			 * set P to (ascii) string buffer which will not be
			 * used otherwise for this line. It has some extra
			 * space in the 'name' field, just in case.
			 */
			comment++;
			cp = cline;

			/* Save the actual marker as well! */
			*cp++ = (wchar_t)c;

			continue;
		}

		/* Are we starting a quote? */
		if (c == L'"') {
			doquot ^= 1;
			quoted = 1;
			continue;
		}

		/*
		 * Quoted text will not be modified or (stripped),
		 * and for that reason, we also consider the [ and
	 	 * ] characters (for section names) such.
		 */
		if ((c == L'[') || (c == L']'))
			doquot ^= 1;

		/* Quoting means raw insertion. */
		if (doquot) {
			/* We are quoting, so insert as is. */
			if (c == L'\n') {
				ERRLOG("CONFIG: syntax error: unexpected newline, expected (\") on line %i\n", l);
				config_ro = 1;
				return(0);
			}
			*p++ = (wchar_t)c;
			continue;
		}

		/*
		 * Everything else, normal character insertion.
		 *
		 * This means, we process each character as if it
		 * was typed in.  We assume the usual rules for
		 * skipping whitespace, blank lines and so forth.
		 *
		 */
		if (!doquot && ((c == L' ') || (c == L'\t')))
			continue;

		*p++ = (wchar_t)c;
	}
	*p = L'\0';
	*cp++ = L'\0';

	if (feof(fp)) break;

	if (ferror(fp)) {
		ERRLOG("CONFIG: Read Error on line %i\n", l);
		config_ro = 1;
		return(0);
	}
	DEBUG("CONGIG: [%3i] '%ls'\n", l, line);

	/* Now parse the line we just read. */
	p = line;
	if ((*p == L'\0') && !comment)
		continue;

	/* We seem to have a valid line. */
	if (*p == L'[') {
		/* We are starting a new section. */
		p++;
		c = 0;
		while (*p && *p != L']')
			wctomb(&sname[c++], *p++);
		sname[c] = '\0';
		DEBUG("CONFIG: section: '%s'\n", sname);

		/* Is the section name properly terminated? */
		if (*p != L']') {
			ERRLOG("CONFIG: malformed section name on line %i\n", l);
			config_ro = 1;
			continue;
		}

		/* Create a new section and insert it. */
		ns = (section_t *)mem_alloc(sizeof(section_t));
		memset(ns, 0x00, sizeof(section_t));
		strncpy(ns->name, sname, sizeof(ns->name));
		list_add(&ns->list, &config_head);

		/* New section is now the current one. */
		sec = ns;			

		/* Get next line. */
		continue;
	}

	/* Allocate a new configuration entry.. */
	ne = (entry_t *)mem_alloc(sizeof(entry_t));
	memset(ne, 0x00, sizeof(entry_t));
	ne->sec = sec;
	ne->quoted = quoted;
	ne->comment = comment;

	if (!comment && (*p != L'\0')) {
		/* Get the variable name. */
		c = 0;
		while (*p && *p != L'=')
			wctomb(&ename[c++], *p++);
		ename[c] = '\0';
		DEBUG("CONFIG, variable: '%s'\n", ename);

		/* Skip incomplete lines. */
		if ((*p != L'=') || (*p+1 == L'\0')) {
			ERRLOG("CONFIG: syntax error on line %i\n", l);
			config_ro = 1;
			continue;
		}
		p++;
		DEBUG("CONFIG: value: '%ls'\n", p);

		strncpy(ne->name, ename, sizeof(ne->name));
		wcstombs(ne->data, p, sizeof(ne->data));
		wcsncpy(ne->wdata, p, sizeof_w(ne->wdata));
	} else
		wcsncpy(ne->wdata, cline, sizeof_w(ne->wdata));

	/* .. and insert it. */
	list_add(&ne->list, &sec->entry_head);

	/* Next line, please! */
	l++;
    }

    c = !ferror(fp);
    (void)fclose(fp);

    if (do_dump_config)
	config_dump();

    return(c);
}


/*
 * Write the in-memory configuration to disk.
 *
 * This is a public function, because the Settings UI
 * want to directly write the configuration after it
 * has changed it.
 */
void
config_write(const wchar_t *fn)
{
    wchar_t temp[512];
    section_t *sec;
    entry_t *ent;
    int fl = 0;
    FILE *fp;

#if defined(ANSI_CFG) || !defined(_WIN32)
    fp = plat_fopen(fn, L"wt");
#else
    fp = plat_fopen(fn, L"wt, ccs=UNICODE");
#endif
    if (fp == NULL) return;

    sec = (section_t *)config_head.next;
    while (sec != NULL) {
	if (sec->name[0]) {
		mbstowcs(temp, sec->name, sizeof_w(temp));
		if (fl)
			fwprintf(fp, L"\n");
		else
			fl = 1;
		fwprintf(fp, L"[%ls]\n", temp);
	}

	ent = (entry_t *)sec->entry_head.next;
	while (ent != NULL) {
		if (! ent->comment) {
			if (ent->name[0] != '\0') {
				mbstowcs(temp, ent->name, sizeof_w(temp));
				if (ent->quoted)
					fwprintf(fp, L"%ls = \"%ls\"\n", temp, ent->wdata);
			  	else
					fwprintf(fp, L"%ls = %ls\n", temp, ent->wdata);
			}
		} else
			fwprintf(fp, L"%ls\n", ent->wdata);

		ent = (entry_t *)ent->list.next;
	}

	sec = (section_t *)sec->list.next;
    }

    (void)fclose(fp);
}


/* Create a usable default configuration. */
void
config_init(config_t *cfg)
{
    int i;

    /* Start with a clean slate. */
    memset(cfg, 0x00, sizeof(config_t));

    cfg->language = 0x0000;			// language ID
    cfg->rctrl_is_lalt = 0;			// set R-CTRL as L-ALT

    cfg->window_remember = 0;			// window size and
    cfg->win_w = 0; cfg->win_h = 0;		// position info
    cfg->win_x = 0; cfg->win_y = 0;

    cfg->machine_type = -1;			// current machine ID
    cfg->cpu_manuf = 0;				// cpu manufacturer
    cfg->cpu_type = 3;				// cpu type
    cfg->cpu_use_dynarec = 0,			// cpu uses/needs Dyna
    cfg->enable_ext_fpu = 0;			// enable external FPU
    cfg->mem_size = 256;			// memory size
    cfg->time_sync = TIME_SYNC_DISABLED;	// enable time sync

    i = vidapi_from_internal_name("default");	// video renderer
    cfg->vid_api = i;				// video renderer

    cfg->video_card = VID_CGA,			// graphics/video card
    cfg->voodoo_enabled = 0;			// video option
    cfg->scale = 1;				// screen scale factor
    cfg->vid_resize = 0;			// allow resizing
    cfg->vid_cga_contrast = 0;			// video
    cfg->vid_fullscreen = 0;			// video
    cfg->vid_fullscreen_scale = 0;		// video
    cfg->vid_fullscreen_first = 0;		// video
    cfg->vid_grayscale = 0;			// video
    cfg->vid_graytype = 0;			// video
    cfg->invert_display = 0;			// invert the display
    cfg->enable_overscan = 0;			// enable overscans
    cfg->force_43 = 0;				// video

    cfg->mouse_type = MOUSE_NONE;		// selected mouse type
    cfg->joystick_type = 0;			// joystick type

    cfg->sound_is_float = 1;			// sound uses FP values
    cfg->sound_gain = 0;			// sound volume gain
    cfg->sound_card = SOUND_NONE;		// selected sound card
    cfg->mpu401_standalone_enable = 0;		// sound option
    cfg->midi_device = 0;			// selected midi device

    cfg->game_enabled = 0;			// enable game port

    for (i = 0; i < SERIAL_MAX; i++)		// enable serial ports
	cfg->serial_enabled[i] = 0;

    for (i = 0; i < PARALLEL_MAX; i++) {	// enable LPT ports
	cfg->parallel_enabled[i] = 0;
	cfg->parallel_device[i] = 0;		// set up LPT devices
    }

    fdd_set_type(0, 2);
    fdd_set_check_bpb(0, 1);
    fdd_set_type(1, 2);
    fdd_set_check_bpb(1, 1);

    cfg->hdc_type = HDC_NONE;			// HDC type

    cfg->scsi_card = SCSI_NONE;			// selected SCSI card

    cfg->network_type = NET_NONE;		// network provider type
    cfg->network_card = NET_CARD_NONE;		// network interface num
    strcpy(cfg->network_host, "");		// host network intf

    cfg->bugger_enabled = 0;			// enable ISAbugger

    for (i = 0; i < ISAMEM_MAX; i++)		// enable ISA mem cards
	cfg->isamem_type[i] = 0;
    cfg->isartc_type = 0;			// enable ISA RTC card

#ifdef WALTJE
    cfg->romdos_enabled = 0;			// enable ROM DOS
#endif

    /* Mark the configuration as clean. */
    config_changed = 0;
}


/* Load the specified or a default configuration file. */
int
config_load(config_t *cfg)
{
    int i = 0;

    /* Start with a clean, known configuration. */
    config_init(cfg);

    if (config_read(cfg_path)) {
	/* Load all the categories. */
	for (i = 0; categories[i].name != NULL; i++)
		categories[i].load(cfg, categories[i].name);

	/* Mark the configuration as changed. */
	config_changed = 1;
    } else {
	ERRLOG("CONFIG: file not present or invalid, using defaults!\n");
    }

    return(i);
}


void
config_save(void)
{
    config_t *cfg = &config;
    int i;

    /* Save all the categories. */
    for (i = 0; categories[i].name != NULL; i++) {
	if (categories[i].save != NULL)
		categories[i].save(cfg, categories[i].name);
    }

    /* Write it back to file if enabled. */
    if (! config_ro)
	config_write(cfg_path);
}


void
config_dump(void)
{
    section_t *sec;
	
    sec = (section_t *)config_head.next;
    while (sec != NULL) {
	entry_t *ent;

	if (sec->name[0])
		INFO("[%s]\n", sec->name);
	
	ent = (entry_t *)sec->entry_head.next;
	while (ent != NULL) {
		INFO("%s = %ls\n", ent->name, ent->wdata);

		ent = (entry_t *)ent->list.next;
	}

	sec = (section_t *)sec->list.next;
    }
}


/* Compare two configurations. */
int
config_compare(const config_t *one, const config_t *two)
{
    int i;
#if 0
    int j;
#endif

    i = memcmp(one, two, sizeof(config_t));
    DEBUG("COMPARE: memcmp=%i\n", i);

#if 0
    /*
     * Compare the data old-style, per variable.
     * We keep this until the migration is completed.
     */

    /* Machine category */
    i = 0;
    i = i || (one->machine_type != two->machine_type);
    i = i || (one->cpu_manuf != two->cpu_manuf);
    i = i || (one->cpu_waitstates != two->cpu_waitstates);
    i = i || (one->cpu_type != two->cpu_type);
    i = i || (one->mem_size != two->mem_size);
#ifdef USE_DYNAREC
    i = i || (one->cpu_use_dynarec != two->cpu_use_dynarec);
#endif
    i = i || (one->enable_ext_fpu != two->enable_ext_fpu);
    i = i || (one->time_sync != two->time_sync);

    /* Video category */
    i = i || (one->video_card != two->video_card);
    i = i || (one->voodoo_enabled != two->voodoo_enabled);

    /* Input devices category */
    i = i || (one->mouse_type != two->mouse_type);
    i = i || (one->joystick_type != two->joystick_type);

    /* Sound category */
    i = i || (one->sound_card != two->sound_card);
    i = i || (one->midi_device != two->midi_device);
    i = i || (one->mpu401_standalone_enable != two->mpu401_standalone_enable);
    i = i || (one->opl_type != two->opl_type);
    i = i || (one->sound_is_float != two->sound_is_float);

    /* Network category */
    i = i || (one->network_type != two->network_type);
    i = i || strcmp(one->network_host, two->network_host);
    i = i || (one->network_card != two->network_card);

    /* Ports category */
    i = i || (one->game_enabled != two->game_enabled);
    for (j = 0; j < SERIAL_MAX; j++)
	i = i || (one->serial_enabled[j] != two->serial_enabled[j]);
    for (j = 0; j < PARALLEL_MAX; j++) {
	i = i || (one->parallel_enabled[j] != two->parallel_enabled[j]);
	i = i || (one->parallel_device[j] != two->parallel_device[j]);
    }

    /* Peripherals category */
    i = i || (one->scsi_card != two->scsi_card);
    i = i || (one->hdc_type != two->hdc_type);
    i = i || (one->ide_ter_enabled != two->ide_ter_enabled);
    i = i || (one->ide_qua_enabled != two->ide_qua_enabled);
    i = i || (one->bugger_enabled != two->bugger_enabled);
    i = i || (one->isartc_type != two->isartc_type);

    /* ISA memory boards. */
    for (j = 0; j < ISAMEM_MAX; j++)
	i = i || (one->isamem_type[j] != two->isamem_type[j]);

# if 0
    /* Hard disks category */
    i = i || memcmp(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (j = 0; j < FDD_NUM; j++) {
	i = i || (temp_fdd_types[j] != fdd_get_type(j));
	i = i || (temp_fdd_turbo[j] != fdd_get_turbo(j));
	i = i || (temp_fdd_check_bpb[j] != fdd_get_check_bpb(j));
    }

    /* Other removable devices category */
    i = i || memcmp(cdrom, temp_cdrom_drives, CDROM_NUM * sizeof(cdrom_t));
    i = i || memcmp(zip_drives, temp_zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    i = i || !!temp_deviceconfig;
# endif
    DEBUG("COMPARE: vars=%i\n", i);
#endif

    return(i);
}


void
config_delete_var(const char *cat, const char *name)
{
    section_t *section;
    entry_t *entry;

    section = find_section(cat);
    if (section == NULL) return;
		
    entry = find_entry(section, name);
    if (entry != NULL) {
	list_delete(&entry->list, &section->entry_head);
	free(entry);
    }
}


int
config_get_int(const char *cat, const char *name, int def)
{
    section_t *section;
    entry_t *entry;
    int value;

    section = find_section(cat);
    if (section == NULL)
	return(def);
		
    entry = find_entry(section, name);
    if (entry == NULL)
	return(def);

    sscanf(entry->data, "%i", &value);

    return(value);
}


int
config_get_hex16(const char *cat, const char *name, int def)
{
    section_t *section;
    entry_t *entry;
    unsigned int value;

    section = find_section(cat);
    if (section == NULL)
	return(def);

    entry = find_entry(section, name);
    if (entry == NULL)
	return(def);

    sscanf(entry->data, "%04X", &value);

    return((int)value);
}


int
config_get_hex20(const char *cat, const char *name, int def)
{
    section_t *section;
    entry_t *entry;
    unsigned int value;

    section = find_section(cat);
    if (section == NULL)
	return(def);

    entry = find_entry(section, name);
    if (entry == NULL)
	return(def);

    sscanf(entry->data, "%05X", &value);

    return((int)value);
}


int
config_get_mac(const char *cat, const char *name, int def)
{
    section_t *section;
    entry_t *entry;
    unsigned int val0 = 0, val1 = 0, val2 = 0;

    section = find_section(cat);
    if (section == NULL)
	return(def);

    entry = find_entry(section, name);
    if (entry == NULL)
	return(def);

    sscanf(entry->data, "%02x:%02x:%02x", &val0, &val1, &val2);

    return((int)((val0 << 16) + (val1 << 8) + val2));
}


char *
config_get_string(const char *cat, const char *name, const char *def)
{
    section_t *section;
    entry_t *entry;

    section = find_section(cat);
    if (section == NULL)
	return((char *)def);

    entry = find_entry(section, name);
    if (entry == NULL)
	return((char *)def);
     
    return(entry->data);
}


wchar_t *
config_get_wstring(const char *cat, const char *name, const wchar_t *def)
{
    section_t *section;
    entry_t *entry;

    section = find_section(cat);
    if (section == NULL)
	return((wchar_t *)def);

    entry = find_entry(section, name);
    if (entry == NULL)
	return((wchar_t *)def);
   
    return(entry->wdata);
}


void
config_set_int(const char *cat, const char *name, int val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    sprintf(ent->data, "%i", val);
    mbstowcs(ent->wdata, ent->data, sizeof_w(ent->wdata));
}


void
config_set_hex16(const char *cat, const char *name, int val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    sprintf(ent->data, "%04X", val);
    mbstowcs(ent->wdata, ent->data, sizeof_w(ent->wdata));
}


void
config_set_hex20(const char *cat, const char *name, int val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    sprintf(ent->data, "%05X", val);
    mbstowcs(ent->wdata, ent->data, sizeof_w(ent->wdata));
}


void
config_set_mac(const char *cat, const char *name, int val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    sprintf(ent->data, "%02x:%02x:%02x",
		(val>>16)&0xff, (val>>8)&0xff, val&0xff);
    mbstowcs(ent->wdata, ent->data, sizeof_w(ent->wdata));
}


void
config_set_string(const char *cat, const char *name, const char *val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    strncpy(ent->data, val, sizeof(ent->data));
    mbstowcs(ent->wdata, ent->data, sizeof_w(ent->wdata));
}


void
config_set_wstring(const char *cat, const char *name, const wchar_t *val)
{
    section_t *section;
    entry_t *ent;

    section = find_section(cat);
    if (section == NULL)
	section = create_section(cat);

    ent = find_entry(section, name);
    if (ent == NULL)
	ent = create_entry(section, name);

    wcsncpy(ent->wdata, val, sizeof_w(ent->wdata));
    wcstombs(ent->data, ent->wdata, sizeof(ent->data));
}
