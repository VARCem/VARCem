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
 * NOTE:	Forcing config files to be in Unicode encoding breaks
 *		it on Windows XP, and possibly also Vista. Use the
 *		-DANSI_CFG for use on these systems.
 *
 * Version:	@(#)config.c	1.0.35	2018/10/07
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		David Simunic, <simunic.david@outlook.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include "cpu/cpu.h"
#include "machines/machine.h"
#include "nvr.h"
#include "device.h"
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

    char	name[128];

    list_t	entry_head;
} section_t;

typedef struct {
    list_t	list;

    char	name[128];
    char	data[256];
    wchar_t	wdata[512];
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
    strncpy(ns->name, name, sizeof(ns->name));
    list_add(&ns->list, &config_head);

    return(ns);
}


static entry_t *
create_entry(section_t *section, const char *name)
{
    entry_t *ne = (entry_t *)mem_alloc(sizeof(entry_t));

    memset(ne, 0x00, sizeof(entry_t));
    strncpy(ne->name, name, sizeof(ne->name));
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
load_general(const char *cat)
{
    char *p;

    vid_resize = !!config_get_int(cat, "vid_resize", 0);

    p = config_get_string(cat, "vid_renderer", "default");
    vid_api = vidapi_from_internal_name(p);

    vid_fullscreen_scale = config_get_int(cat, "video_fullscreen_scale", 0);

    vid_fullscreen_first = config_get_int(cat, "video_fullscreen_first", 0);

    force_43 = !!config_get_int(cat, "force_43", 0);
    scale = config_get_int(cat, "scale", 1);
    if (scale > 3)
	scale = 3;

    invert_display = !!config_get_int(cat, "invert_display", 0);
    enable_overscan = !!config_get_int(cat, "enable_overscan", 0);
    vid_cga_contrast = !!config_get_int(cat, "vid_cga_contrast", 0);
    vid_grayscale = config_get_int(cat, "video_grayscale", 0);
    vid_graytype = config_get_int(cat, "video_graytype", 0);

    rctrl_is_lalt = config_get_int(cat, "rctrl_is_lalt", 0);

    window_remember = config_get_int(cat, "window_remember", 0);
    if (window_remember) {
	p = config_get_string(cat, "window_coordinates", "0, 0, 0, 0");
	sscanf(p, "%i, %i, %i, %i", &window_w, &window_h, &window_x, &window_y);
    } else {
	config_delete_var(cat, "window_remember");
	config_delete_var(cat, "window_coordinates");

	window_w = window_h = window_x = window_y = 0;
    }

    sound_gain = config_get_int(cat, "sound_gain", 0);

    language = config_get_hex16(cat, "language", emu_lang_id);
}


/* Save "General" section. */
static void
save_general(const char *cat)
{
    char temp[512];
    const char *str;

    config_set_int(cat, "vid_resize", vid_resize);
    if (vid_resize == 0)
	config_delete_var(cat, "vid_resize");

    str = vidapi_internal_name(vid_api);
    if (! strcmp(str, "default")) {
	config_delete_var(cat, "vid_renderer");
    } else {
	config_set_string(cat, "vid_renderer", str);
    }

    if (vid_fullscreen_scale == 0)
	config_delete_var(cat, "video_fullscreen_scale");
      else
	config_set_int(cat, "video_fullscreen_scale", vid_fullscreen_scale);

    if (vid_fullscreen_first == 0)
	config_delete_var(cat, "video_fullscreen_first");
      else
	config_set_int(cat, "video_fullscreen_first", vid_fullscreen_first);

    if (force_43 == 0)
	config_delete_var(cat, "force_43");
      else
	config_set_int(cat, "force_43", force_43);

    if (scale == 1)
	config_delete_var(cat, "scale");
      else
	config_set_int(cat, "scale", scale);

    if (invert_display == 0)
	config_delete_var(cat, "invert_display");
      else
	config_set_int(cat, "invert_display", invert_display);

    if (enable_overscan == 0)
	config_delete_var(cat, "enable_overscan");
      else
	config_set_int(cat, "enable_overscan", enable_overscan);

    if (vid_cga_contrast == 0)
	config_delete_var(cat, "vid_cga_contrast");
      else
	config_set_int(cat, "vid_cga_contrast", vid_cga_contrast);

    if (vid_grayscale == 0)
	config_delete_var(cat, "video_grayscale");
      else
	config_set_int(cat, "video_grayscale", vid_grayscale);

    if (vid_graytype == 0)
	config_delete_var(cat, "video_graytype");
      else
	config_set_int(cat, "video_graytype", vid_graytype);

    if (rctrl_is_lalt == 0)
	config_delete_var(cat, "rctrl_is_lalt");
      else
	config_set_int(cat, "rctrl_is_lalt", rctrl_is_lalt);

    if (window_remember) {
	config_set_int(cat, "window_remember", window_remember);

	sprintf(temp, "%i, %i, %i, %i", window_w, window_h, window_x, window_y);
	config_set_string(cat, "window_coordinates", temp);
    } else {
	config_delete_var(cat, "window_remember");
	config_delete_var(cat, "window_coordinates");
    }

    if (sound_gain != 0)
	config_set_int(cat, "sound_gain", sound_gain);
      else
	config_delete_var(cat, "sound_gain");

    if (emu_lang_id == 0x0409)
	config_delete_var(cat, "language");
      else
	config_set_hex16(cat, "language", emu_lang_id);

    delete_section_if_empty(cat);
}


/* Load "Machine" section. */
static void
load_machine(const char *cat)
{
    char *p;

    p = config_get_string(cat, "machine", NULL);
    if (p != NULL)
	machine = machine_get_from_internal_name(p);
      else 
	machine = -1;

    cpu_manufacturer = config_get_int(cat, "cpu_manufacturer", 0);
    cpu = config_get_int(cat, "cpu", 0);
    cpu_waitstates = config_get_int(cat, "cpu_waitstates", 0);

    mem_size = config_get_int(cat, "mem_size", 4096);
    if ((uint32_t)mem_size < (((machines[machine].flags & MACHINE_AT) &&
        (machines[machine].ram_granularity < 128)) ? machines[machine].min_ram*1024 : machines[machine].min_ram))
	mem_size = (((machines[machine].flags & MACHINE_AT) && (machines[machine].ram_granularity < 128)) ? machines[machine].min_ram*1024 : machines[machine].min_ram);
    if (mem_size > 1048576)
	mem_size = 1048576;

    cpu_use_dynarec = !!config_get_int(cat, "cpu_use_dynarec", 0);

    enable_external_fpu = !!config_get_int(cat, "cpu_enable_fpu", 0);

    time_sync = !!config_get_int(cat, "enable_sync", -1);
    if (time_sync != -1) {
	/* FIXME: remove this after 12/01/2018 --FvK */
	config_delete_var(cat, "enable_sync");
    } else
	time_sync = !!config_get_int(cat, "time_sync", TIME_SYNC_DISABLED);
}


/* Save "Machine" section. */
static void
save_machine(const char *cat)
{
    config_set_string(cat, "machine", machine_get_internal_name());

    if (cpu_manufacturer == 0)
	config_delete_var(cat, "cpu_manufacturer");
      else
	config_set_int(cat, "cpu_manufacturer", cpu_manufacturer);

    if (cpu == 0)
	config_delete_var(cat, "cpu");
      else
	config_set_int(cat, "cpu", cpu);

    if (cpu_waitstates == 0)
	config_delete_var(cat, "cpu_waitstates");
      else
	config_set_int(cat, "cpu_waitstates", cpu_waitstates);

    if (mem_size == 4096)
	config_delete_var(cat, "mem_size");
      else
	config_set_int(cat, "mem_size", mem_size);

    config_set_int(cat, "cpu_use_dynarec", cpu_use_dynarec);

    if (enable_external_fpu == 0)
	config_delete_var(cat, "cpu_enable_fpu");
      else
	config_set_int(cat, "cpu_enable_fpu", enable_external_fpu);

    if (time_sync == TIME_SYNC_DISABLED)
	config_delete_var(cat, "time_sync");
      else
	config_set_int(cat, "time_sync", time_sync);

    delete_section_if_empty(cat);
}


/* Load "Video" section. */
static void
load_video(const char *cat)
{
    char *p;

    if (machines[machine].fixed_vidcard) {
	config_delete_var(cat, "video_card");
	video_card = VID_INTERNAL;
    } else {
	p = config_get_string(cat, "video_card", NULL);
	if (p == NULL) {
		if (machines[machine].flags & MACHINE_VIDEO)
			p = "internal";
		  else
			p = "none";
	}
	video_card = video_get_video_from_internal_name(p);
    }

    /*FXME: remove by 12/01/2018 --FvK */
    config_delete_var(cat, "video_speed");

    voodoo_enabled = !!config_get_int(cat, "voodoo", 0);
}


/* Save "Video" section. */
static void
save_video(const char *cat)
{
    config_set_string(cat, "video_card", video_get_internal_name(video_card));

    if (voodoo_enabled == 0)
	config_delete_var(cat, "voodoo");
      else
	config_set_int(cat, "voodoo", voodoo_enabled);

    delete_section_if_empty(cat);
}


/* Load "Input Devices" section. */
static void
load_input(const char *cat)
{
    char temp[512];
    int c, d;
    char *p;

    p = config_get_string(cat, "mouse_type", "none");
	mouse_type = mouse_get_from_internal_name(p);

    //FIXME: should be an internal_name string!!
    joystick_type = config_get_int(cat, "joystick_type", 0);

    for (c=0; c<gamedev_get_max_joysticks(joystick_type); c++) {
	sprintf(temp, "joystick_%i_nr", c);
	joystick_state[c].plat_joystick_nr = config_get_int(cat, temp, 0);

	if (joystick_state[c].plat_joystick_nr) {
		for (d=0; d<gamedev_get_axis_count(joystick_type); d++) {
			sprintf(temp, "joystick_%i_axis_%i", c, d);
			joystick_state[c].axis_mapping[d] = config_get_int(cat, temp, d);
		}
		for (d=0; d<gamedev_get_button_count(joystick_type); d++) {			
			sprintf(temp, "joystick_%i_button_%i", c, d);
			joystick_state[c].button_mapping[d] = config_get_int(cat, temp, d);
		}
		for (d=0; d<gamedev_get_pov_count(joystick_type); d++) {
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
save_input(const char *cat)
{
    char temp[512], tmp2[512];
    int c, d;

    config_set_string(cat, "mouse_type", mouse_get_internal_name(mouse_type));

    if (joystick_type != 0) {
	config_set_int(cat, "joystick_type", joystick_type);

	for (c=0; c<gamedev_get_max_joysticks(joystick_type); c++) {
		sprintf(tmp2, "joystick_%i_nr", c);
		config_set_int(cat, tmp2, joystick_state[c].plat_joystick_nr);

		if (joystick_state[c].plat_joystick_nr) {
			for (d=0; d<gamedev_get_axis_count(joystick_type); d++) {			
				sprintf(tmp2, "joystick_%i_axis_%i", c, d);
				config_set_int(cat, tmp2, joystick_state[c].axis_mapping[d]);
			}
			for (d=0; d<gamedev_get_button_count(joystick_type); d++) {			
				sprintf(tmp2, "joystick_%i_button_%i", c, d);
				config_set_int(cat, tmp2, joystick_state[c].button_mapping[d]);
			}
			for (d=0; d<gamedev_get_pov_count(joystick_type); d++) {			
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
load_sound(const char *cat)
{
    char *p;

    p = config_get_string(cat, "sound_card", "none");
    sound_card = sound_card_get_from_internal_name(p);

    p = config_get_string(cat, "midi_device", "none");
    midi_device = midi_device_get_from_internal_name(p);

    mpu401_standalone_enable = !!config_get_int(cat, "mpu401_standalone", 0);

    p = config_get_string(cat, "opl_type", "dbopl");
    if (!strcmp(p, "nukedopl") || !strcmp(p, "1"))
	opl_type = 1;
      else
	opl_type = 0;

    p = config_get_string(cat, "sound_type", "float");
    if (!strcmp(p, "float") || !strcmp(p, "1"))
	sound_is_float = 1;
      else
	sound_is_float = 0;
}


/* Save "Sound" section. */
static void
save_sound(const char *cat)
{
    if (sound_card == 0)
	config_delete_var(cat, "sound_card");
      else
	config_set_string(cat, "sound_card",
			  sound_card_get_internal_name(sound_card));

    if (!strcmp(midi_device_get_internal_name(midi_device), "none"))
	config_delete_var(cat, "midi_device");
      else
	config_set_string(cat, "midi_device",
			  midi_device_get_internal_name(midi_device));

    if (mpu401_standalone_enable == 0)
	config_delete_var(cat, "mpu401_standalone");
      else
	config_set_int(cat, "mpu401_standalone", mpu401_standalone_enable);

    if (opl_type == 0)
	config_delete_var(cat, "opl_type");
      else
	config_set_string(cat, "opl_type", (opl_type == 1) ? "nukedopl" : "dbopl");

    if (sound_is_float == 1)
	config_delete_var(cat, "sound_type");
      else
	config_set_string(cat, "sound_type", (sound_is_float == 1) ? "float" : "int16");

    delete_section_if_empty(cat);
}


/* Load "Network" section. */
static void
load_network(const char *cat)
{
    char *p;

    p = config_get_string(cat, "net_type", NULL);
    if (p != NULL) {
	if (!strcmp(p, "pcap") || !strcmp(p, "1"))
		network_type = NET_TYPE_PCAP;
	else
	if (!strcmp(p, "slirp") || !strcmp(p, "2"))
		network_type = NET_TYPE_SLIRP;
	else
		network_type = NET_TYPE_NONE;
    } else
	network_type = NET_TYPE_NONE;

    memset(network_host, '\0', sizeof(network_host));
    p = config_get_string(cat, "net_host_device", NULL);
    if (p == NULL) {
	p = config_get_string(cat, "net_pcap_device", NULL);
	if (p != NULL)
		config_delete_var(cat, "net_pcap_device");
    }
    if (p != NULL) {
	if ((network_dev_to_id(p) == -1) || (network_ndev == 1)) {
		if ((network_ndev == 1) && strcmp(network_host, "none")) {
			ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_PCAP_NO);
		} else if (network_dev_to_id(p) == -1) {
			ui_msgbox(MBX_ERROR, (wchar_t *)IDS_ERR_PCAP_DEV);
		}

		strcpy(network_host, "none");
	} else {
		strcpy(network_host, p);
	}
    } else
	strcpy(network_host, "none");

    p = config_get_string(cat, "net_card", "none");
	network_card = network_card_get_from_internal_name(p);
}


/* Save "Network" section. */
static void
save_network(const char *cat)
{
    if (network_type == NET_TYPE_NONE)
	config_delete_var(cat, "net_type");
      else
	config_set_string(cat, "net_type",
		(network_type == NET_TYPE_SLIRP) ? "slirp" : "pcap");

    if (network_host[0] != '\0') {
	if (! strcmp(network_host, "none"))
		config_delete_var(cat, "net_host_device");
	  else
		config_set_string(cat, "net_host_device", network_host);
    } else {
	config_delete_var(cat, "net_host_device");
    }

    if (network_card == 0)
	config_delete_var(cat, "net_card");
      else
	config_set_string(cat, "net_card",
			  network_card_get_internal_name(network_card));

    delete_section_if_empty(cat);
}


/* Load "Ports" section. */
static void
load_ports(const char *cat)
{
    char temp[128];
    char *p;
    int i;

    sprintf(temp, "game_enabled");
    game_enabled = !!config_get_int(cat, temp, 0);

    for (i = 0; i < SERIAL_MAX; i++) {
	sprintf(temp, "serial%i_enabled", i);
	serial_enabled[i] = !!config_get_int(cat, temp, 0);
    }

    for (i = 0; i < PARALLEL_MAX; i++) {
	sprintf(temp, "parallel%i_enabled", i);
	parallel_enabled[i] = !!config_get_int(cat, temp, 0);

	sprintf(temp, "parallel%i_device", i);
	p = (char *)config_get_string(cat, temp, "none");
	parallel_device[i] = parallel_device_get_from_internal_name(p);
    }
}


/* Save "Ports" section. */
static void
save_ports(const char *cat)
{
    char temp[128];
    int i;

    sprintf(temp, "game_enabled");
    if (game_enabled) {
	config_set_int(cat, temp, 1);
    } else
	config_delete_var(cat, temp);

    for (i = 0; i < SERIAL_MAX; i++) {
	sprintf(temp, "serial%i_enabled", i);
	if (serial_enabled[i]) {
		config_set_int(cat, temp, 1);
	} else
		config_delete_var(cat, temp);
    }

    for (i = 0; i < PARALLEL_MAX; i++) {
	sprintf(temp, "parallel%i_enabled", i);
	if (parallel_enabled[i])
		config_set_int(cat, temp, 1);
	  else
		config_delete_var(cat, temp);

	sprintf(temp, "parallel%i_device", i);
	if (parallel_device[i] != 0)
		config_set_string(cat, temp,
			parallel_device_get_internal_name(parallel_device[i]));
	  else
		config_delete_var(cat, temp);
    }

    delete_section_if_empty(cat);
}


/* Load "Other Peripherals" section. */
static void
load_other(const char *cat)
{
    char temp[128], *p;
    int c;

    p = config_get_string(cat, "scsi_card", "none");
    scsi_card = scsi_card_get_from_internal_name(p);

    p = config_get_string(cat, "hdc", NULL);
    if (p == NULL) {
	if (machines[machine].flags & MACHINE_HDC)
		p = "internal";
	  else
		p = "none";
    }
    hdc_type = hdc_get_from_internal_name(p);

    ide_ter_enabled = !!config_get_int(cat, "ide_ter", 0);
    ide_qua_enabled = !!config_get_int(cat, "ide_qua", 0);

    bugger_enabled = !!config_get_int(cat, "bugger_enabled", 0);

    for (c = 0; c < ISAMEM_MAX; c++) {
	sprintf(temp, "isamem%i_type", c);

	p = config_get_string(cat, temp, "none");
	isamem_type[c] = isamem_get_from_internal_name(p);
    }

    p = config_get_string(cat, "isartc_type", "none");
    isartc_type = isartc_get_from_internal_name(p);

#ifdef WALTJE
    romdos_enabled = !!config_get_int(cat, "romdos_enabled", 0);
#endif
}


/* Save "Other Peripherals" section. */
static void
save_other(const char *cat)
{
    char temp[512];
    int c;

    if (scsi_card == 0)
	config_delete_var(cat, "scsi_card");
      else
	config_set_string(cat, "scsi_card",
			  scsi_card_get_internal_name(scsi_card));

    config_set_string(cat, "hdc", hdc_get_internal_name(hdc_type));

    if (ide_ter_enabled == 0)
	config_delete_var(cat, "ide_ter");
      else
	config_set_int(cat, "ide_ter", ide_ter_enabled);

    if (ide_qua_enabled == 0)
	config_delete_var(cat, "ide_qua");
      else
	config_set_int(cat, "ide_qua", ide_qua_enabled);

    if (bugger_enabled == 0)
	config_delete_var(cat, "bugger_enabled");
      else
	config_set_int(cat, "bugger_enabled", bugger_enabled);

    for (c = 0; c < ISAMEM_MAX; c++) {
	sprintf(temp, "isamem%i_type", c);
	if (isamem_type[c] == 0)
		config_delete_var(cat, temp);
	  else
		config_set_string(cat, temp,
				  isamem_get_internal_name(isamem_type[c]));
    }

    if (isartc_type == 0)
	config_delete_var(cat, "isartc_type");
      else
	config_set_string(cat, "isartc_type",
			  isartc_get_internal_name(isartc_type));

    delete_section_if_empty(cat);
}


/* Load "Hard Disks" section. */
static void
load_disks(const char *cat)
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
		hdd[c].id.st506_channel = dev;
	} else
		config_delete_var(cat, temp);

	/* ESDI */
	sprintf(temp, "hdd_%02i_esdi_channel", c+1);
	if (hdd[c].bus == HDD_BUS_ESDI)
		hdd[c].id.esdi_channel = !!config_get_int(cat, temp, c & 1);
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
		hdd[c].id.ide_channel = (board<<1) + dev;

		if (hdd[c].id.ide_channel > 7)
			hdd[c].id.ide_channel = 7;
	} else {
		config_delete_var(cat, temp);
	}

	/* SCSI */
	sprintf(temp, "hdd_%02i_scsi_location", c+1);
	if (hdd[c].bus == HDD_BUS_SCSI) {
		sprintf(tmp2, "%02u:%02u", c, 0);
		p = config_get_string(cat, temp, tmp2);

		sscanf(p, "%02u:%02u",
			(int *)&hdd[c].id.scsi.id, (int *)&hdd[c].id.scsi.lun);

		if (hdd[c].id.scsi.id > 15)
			hdd[c].id.scsi.id = 15;
		if (hdd[c].id.scsi.lun > 7)
			hdd[c].id.scsi.lun = 7;
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
save_disks(const char *cat)
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
		config_set_int(cat, temp, hdd[c].id.st506_channel);
	  else
		config_delete_var(cat, temp);

	sprintf(temp, "hdd_%02i_esdi_channel", c+1);
	if (hdd_is_valid(c) && (hdd[c].bus == HDD_BUS_ESDI))
		config_set_int(cat, temp, hdd[c].id.esdi_channel);
	  else
		config_delete_var(cat, temp);

	sprintf(temp, "hdd_%02i_ide_channel", c+1);
	if (!hdd_is_valid(c) || (hdd[c].bus != HDD_BUS_IDE)) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%01u:%01u",
			hdd[c].id.ide_channel >> 1,
			hdd[c].id.ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "hdd_%02i_scsi_location", c+1);
	if (!hdd_is_valid(c) || (hdd[c].bus != HDD_BUS_SCSI)) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02i:%02i",
			hdd[c].id.scsi.id, hdd[c].id.scsi.lun);
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
static void
load_floppy(const char *cat)
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

	sprintf(temp, "fdd_%02i_fn", c + 1);
	wp = config_get_wstring(cat, temp, L"");

	/* Try to make relative, and copy to destination. */
	pc_path(floppyfns[c], sizeof_w(floppyfns[c]), wp);

	sprintf(temp, "fdd_%02i_writeprot", c+1);
	ui_writeprot[c] = !!config_get_int(cat, temp, 0);
	sprintf(temp, "fdd_%02i_turbo", c + 1);
	fdd_set_turbo(c, !!config_get_int(cat, temp, 0));
	sprintf(temp, "fdd_%02i_check_bpb", c+1);
	fdd_set_check_bpb(c, !!config_get_int(cat, temp, 1));

	/* Check whether each value is default, if yes, delete it so that only non-default values will later be saved. */
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
save_floppy(const char *cat)
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
	} else {
		config_set_wstring(cat, temp, floppyfns[c]);
	}

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
static void
load_removable(const char *cat)
{
    char temp[512], tmp2[512], *p;
    char s[512];
    unsigned int board = 0, dev = 0;
    wchar_t *wp;
    int c;

    for (c = 0; c < CDROM_NUM; c++) {
	sprintf(temp, "cdrom_%02i_host_drive", c+1);
	cdrom_drives[c].host_drive = config_get_int(cat, temp, 0);
	cdrom_drives[c].prev_host_drive = cdrom_drives[c].host_drive;

	sprintf(temp, "cdrom_%02i_parameters", c+1);
	p = config_get_string(cat, temp, "0, none");
	sscanf(p, "%01u, %s", (int *)&cdrom_drives[c].sound_on, s);
	cdrom_drives[c].bus_type = cdrom_string_to_bus(s);

	sprintf(temp, "cdrom_%02i_speed", c+1);
	cdrom_drives[c].speed_idx = config_get_int(cat, temp, cdrom_speed_idx(CDROM_SPEED_DEFAULT));

	/* Default values, needed for proper operation of the Settings dialog. */
	cdrom_drives[c].ide_channel = cdrom_drives[c].scsi_device_id = c + 2;

	sprintf(temp, "cdrom_%02i_ide_channel", c+1);
	if (cdrom_drives[c].bus_type == CDROM_BUS_ATAPI) {
		sprintf(tmp2, "%01u:%01u", (c+2)>>1, (c+2)&1);
		p = config_get_string(cat, temp, tmp2);
		sscanf(p, "%01u:%01u", &board, &dev);
		board &= 3;
		dev &= 1;
		cdrom_drives[c].ide_channel = (board<<1)+dev;

		if (cdrom_drives[c].ide_channel > 7)
			cdrom_drives[c].ide_channel = 7;
	} else {
		sprintf(temp, "cdrom_%02i_scsi_location", c+1);
		if (cdrom_drives[c].bus_type == CDROM_BUS_SCSI) {
			sprintf(tmp2, "%02u:%02u", c+2, 0);
			p = config_get_string(cat, temp, tmp2);
			sscanf(p, "%02u:%02u",
				(int *)&cdrom_drives[c].scsi_device_id,
				(int *)&cdrom_drives[c].scsi_device_lun);
	
			if (cdrom_drives[c].scsi_device_id > 15)
				cdrom_drives[c].scsi_device_id = 15;
			if (cdrom_drives[c].scsi_device_lun > 7)
				cdrom_drives[c].scsi_device_lun = 7;
		} else {
			config_delete_var(cat, temp);
		}
	}

	sprintf(temp, "cdrom_%02i_image_path", c+1);
	wp = config_get_wstring(cat, temp, L"");

	/* Try to make relative, and copy to destination. */
	pc_path(cdrom_image[c].image_path, sizeof_w(cdrom_image[c].image_path), wp);

	if (cdrom_drives[c].host_drive < 'A')
		cdrom_drives[c].host_drive = 0;

	if ((cdrom_drives[c].host_drive == 0x200) &&
	    (wcslen(cdrom_image[c].image_path) == 0))
		cdrom_drives[c].host_drive = 0;

	/* If the CD-ROM is disabled, delete all its variables. */
	if (cdrom_drives[c].bus_type == CDROM_BUS_DISABLED) {
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
	zip_drives[c].ide_channel = zip_drives[c].scsi_device_id = c + 2;

	sprintf(temp, "zip_%02i_ide_channel", c+1);
	if (zip_drives[c].bus_type == ZIP_BUS_ATAPI) {
		sprintf(tmp2, "%01u:%01u", (c+2)>>1, (c+2)&1);
		p = config_get_string(cat, temp, tmp2);
		sscanf(p, "%01u:%01u", &board, &dev);

		board &= 3;
		dev &= 1;
		zip_drives[c].ide_channel = (board<<1)+dev;

		if (zip_drives[c].ide_channel > 7)
			zip_drives[c].ide_channel = 7;
	} else {
		sprintf(temp, "zip_%02i_scsi_location", c+1);
		if (zip_drives[c].bus_type == CDROM_BUS_SCSI) {
			sprintf(tmp2, "%02u:%02u", c+2, 0);
			p = config_get_string(cat, temp, tmp2);
			sscanf(p, "%02u:%02u",
				(unsigned *)&zip_drives[c].scsi_device_id,
				(unsigned *)&zip_drives[c].scsi_device_lun);
	
			if (zip_drives[c].scsi_device_id > 15)
				zip_drives[c].scsi_device_id = 15;
			if (zip_drives[c].scsi_device_lun > 7)
				zip_drives[c].scsi_device_lun = 7;
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
save_removable(const char *cat)
{
    char temp[512], tmp2[512];
    int c;

    for (c = 0; c < CDROM_NUM; c++) {
	sprintf(temp, "cdrom_%02i_host_drive", c+1);
	if ((cdrom_drives[c].bus_type == 0) ||
	    (cdrom_drives[c].host_drive < 'A') || ((cdrom_drives[c].host_drive > 'Z') && (cdrom_drives[c].host_drive != 200))) {
		config_delete_var(cat, temp);
	} else {
		config_set_int(cat, temp, cdrom_drives[c].host_drive);
	}

	sprintf(temp, "cdrom_%02i_speed", c+1);
	if ((cdrom_drives[c].bus_type == 0) ||
		(cdrom_speeds[cdrom_drives[c].speed_idx].speed == cdrom_speed_idx(CDROM_SPEED_DEFAULT))) {
		config_delete_var(cat, temp);
	} else {
		config_set_int(cat, temp, cdrom_drives[c].speed_idx);
	}

	sprintf(temp, "cdrom_%02i_parameters", c+1);
	if (cdrom_drives[c].bus_type == 0) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%u, %s", cdrom_drives[c].sound_on,
			cdrom_bus_to_string(cdrom_drives[c].bus_type));
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_ide_channel", c+1);
	if (cdrom_drives[c].bus_type != CDROM_BUS_ATAPI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%01u:%01u", cdrom_drives[c].ide_channel>>1,
					cdrom_drives[c].ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_scsi_location", c + 1);
	if (cdrom_drives[c].bus_type != CDROM_BUS_SCSI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02u:%02u", cdrom_drives[c].scsi_device_id,
					cdrom_drives[c].scsi_device_lun);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "cdrom_%02i_image_path", c + 1);
	if ((cdrom_drives[c].bus_type == 0) ||
	    (wcslen(cdrom_image[c].image_path) == 0)) {
		config_delete_var(cat, temp);
	} else {
		config_set_wstring(cat, temp, cdrom_image[c].image_path);
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
		sprintf(tmp2, "%01u:%01u", zip_drives[c].ide_channel>>1,
					zip_drives[c].ide_channel & 1);
		config_set_string(cat, temp, tmp2);
	}

	sprintf(temp, "zip_%02i_scsi_location", c + 1);
	if (zip_drives[c].bus_type != ZIP_BUS_SCSI) {
		config_delete_var(cat, temp);
	} else {
		sprintf(tmp2, "%02u:%02u", zip_drives[c].scsi_device_id,
					zip_drives[c].scsi_device_lun);
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
    void	(*load)(const char *);
    void	(*save)(const char *);
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


/* Create a usable default configuration. */
static void
config_default(void)
{
    int i;

    cpu = 0;
    scale = 1;
    video_card = VID_CGA;
    vid_api = vidapi_from_internal_name("default");;
    time_sync = TIME_SYNC_ENABLED;
    joystick_type = 0;
    hdc_type = 0;

    for (i = 0; i < SERIAL_MAX; i++)
	serial_enabled[i] = 0;

    for (i = 0; i < PARALLEL_MAX; i++)
	parallel_enabled[i] = 0;

    fdd_set_type(0, 2);
    fdd_set_check_bpb(0, 1);
    fdd_set_type(1, 2);
    fdd_set_check_bpb(1, 1);

    mem_size = 640;
}


/* Read and parse the configuration file into memory. */
static int
config_read(const wchar_t *fn)
{
    char sname[128], ename[128];
    wchar_t buff[1024];
    section_t *sec, *ns;
    entry_t *ne;
    int c, d;
    FILE *f;

#if defined(ANSI_CFG) || !defined(_WIN32)
    f = plat_fopen(fn, L"rt");
#else
    f = plat_fopen(fn, L"rt, ccs=UNICODE");
#endif
    if (f == NULL) return(0);
	
    sec = (section_t *)mem_alloc(sizeof(section_t));
    memset(sec, 0x00, sizeof(section_t));
    memset(&config_head, 0x00, sizeof(list_t));
    list_add(&sec->list, &config_head);

    while (1) {
	memset(buff, 0x00, sizeof(buff));
	fgetws(buff, sizeof_w(buff), f);
	if (ferror(f) || feof(f)) break;

	/* Make sure there are no stray newlines or hard-returns in there. */
	if (buff[wcslen(buff)-1] == L'\n') buff[wcslen(buff)-1] = L'\0';
	if (buff[wcslen(buff)-1] == L'\r') buff[wcslen(buff)-1] = L'\0';

	/* Skip any leading whitespace. */
	c = 0;
	while ((buff[c] == L' ') || (buff[c] == L'\t'))
		  c++;

	/* Skip empty lines. */
	if (buff[c] == L'\0') continue;

	/* Skip lines that (only) have a comment. */
	if ((buff[c] == L'#') || (buff[c] == L';')) continue;

	if (buff[c] == L'[') {	/*Section*/
		c++;
		d = 0;
		while (buff[c] && (buff[c] != L']'))
			wctomb(&(sname[d++]), buff[c++]);
		sname[d] = L'\0';

		/* Is the section name properly terminated? */
		if (buff[c] != L']') continue;

		/* Create a new section and insert it. */
		ns = (section_t *)mem_alloc(sizeof(section_t));
		memset(ns, 0x00, sizeof(section_t));
		strncpy(ns->name, sname, sizeof(ns->name));
		list_add(&ns->list, &config_head);

		/* New section is now the current one. */
		sec = ns;			
		continue;
	}

	/* Get the variable name. */
	d = 0;
	while ((buff[c] != L'=') && (buff[c] != L' ') && buff[c])
		wctomb(&(ename[d++]), buff[c++]);
	ename[d] = L'\0';

	/* Skip incomplete lines. */
	if (buff[c] == L'\0') continue;

	/* Look for =, skip whitespace. */
	while ((buff[c] == L'=' || buff[c] == L' ') && buff[c])
		c++;

	/* Skip incomplete lines. */
	if (buff[c] == L'\0') continue;

	/* This is where the value part starts. */
	d = c;

	/* Allocate a new variable entry.. */
	ne = (entry_t *)mem_alloc(sizeof(entry_t));
	memset(ne, 0x00, sizeof(entry_t));
	strncpy(ne->name, ename, sizeof(ne->name));
	wcsncpy(ne->wdata, &buff[d], sizeof_w(ne->wdata));
	ne->wdata[sizeof_w(ne->wdata)-1] = L'\0';
	wcstombs(ne->data, ne->wdata, sizeof(ne->data));
	ne->data[sizeof(ne->data)-1] = '\0';

	/* .. and insert it. */
	list_add(&ne->list, &sec->entry_head);
    }

    c = !ferror(f);
    (void)fclose(f);

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
    wchar_t wtemp[512];
    section_t *sec;
    FILE *f;
    int fl = 0;

#if defined(ANSI_CFG) || !defined(_WIN32)
    f = plat_fopen(fn, L"wt");
#else
    f = plat_fopen(fn, L"wt, ccs=UNICODE");
#endif
    if (f == NULL) return;

    sec = (section_t *)config_head.next;
    while (sec != NULL) {
	entry_t *ent;

	if (sec->name[0]) {
		mbstowcs(wtemp, sec->name, strlen(sec->name)+1);
		if (fl)
			fwprintf(f, L"\n[%ls]\n", wtemp);
		  else
			fwprintf(f, L"[%ls]\n", wtemp);
		fl++;
	}

	ent = (entry_t *)sec->entry_head.next;
	while (ent != NULL) {
		if (ent->name[0] != '\0') {
			mbstowcs(wtemp, ent->name, sizeof_w(wtemp));
			if (ent->wdata[0] == L'\0')
				fwprintf(f, L"%ls = \n", wtemp);
			  else
				fwprintf(f, L"%ls = %ls\n", wtemp, ent->wdata);
			fl++;
		}

		ent = (entry_t *)ent->list.next;
	}

	sec = (section_t *)sec->list.next;
    }
	
    (void)fclose(f);
}


/* Load the specified or a default configuration file. */
int
config_load(void)
{
    int i = 0;

    if (config_read(cfg_path)) {
	/* Load all the categories. */
	for (i = 0; categories[i].name != NULL; i++)
		categories[i].load(categories[i].name);

	/* Mark the configuration as changed. */
	config_changed = 1;
    } else {
	ERRLOG("CONFIG: file not present or invalid, using defaults!\n");

	config_default();

	/* Flag this as an invalid configuration. */
	machine = -1;
    }

    return(i);
}


void
config_save(void)
{
    int i;

    /* Save all the categories. */
    for (i = 0; categories[i].name != NULL; i++)
	categories[i].save(categories[i].name);

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

	if (sec->name && sec->name[0])
		INFO("[%s]\n", sec->name);
	
	ent = (entry_t *)sec->entry_head.next;
	while (ent != NULL) {
		INFO("%s = %ls\n", ent->name, ent->wdata);

		ent = (entry_t *)ent->list.next;
	}

	sec = (section_t *)sec->list.next;
    }
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

    memcpy(ent->wdata, val, sizeof_w(ent->wdata));
    wcstombs(ent->data, ent->wdata, sizeof(ent->data));
}
