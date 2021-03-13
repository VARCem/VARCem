/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the generic device interface to handle
 *		all devices attached to the emulator.
 *
 * **TODO**	Merge the various 'add' variants, its getting too messy.
 *
 * Version:	@(#)device.c	1.0.28	2021/01/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include "mem.h"
#include "rom.h"
#include "device.h"
#include "machines/machine.h"
#include "devices/sound/sound.h"
#include "devices/video/video.h"
#include "ui/ui.h"
#include "plat.h"


#define DEVICE_MAX	256			/* max # of devices */


typedef struct clonedev {
    const device_t	*master;
    int			count;
    struct clonedev	*next;
} clonedev_t;


static device_t		*devices[DEVICE_MAX];
static priv_t		device_priv[DEVICE_MAX];
static device_t		*device_current;
static clonedev_t	*clones = NULL;


/* Initialize the module for use. */
void
device_reset(void)
{
    clonedev_t *ptr;

    memset(devices, 0x00, sizeof(devices));

    ptr = NULL;
    while (clones != NULL) {
	ptr = clones->next;
	free(clones);
	clones = ptr;
    }

    clones = NULL;
}


#ifdef _DEBUG
static char *
device_flags(uint32_t flags)
{
    static char buff[80];
    char *sp = buff;
    char *p = NULL;

    if (flags == DEVICE_ROOT)
	return("[ ROOT ]");

    strcpy(sp, "[ "); sp += strlen(sp);

    /* System types. */
    switch(flags & 0x000f) {
	case DEVICE_ALL:		/* any/all system */
		p = "ANY";
		break;

	case DEVICE_PCJR:		/* requires an IBM PCjr */
		p = "PCjr";
		break;

	case DEVICE_AT:			/* requires an AT-compatible system */
		p = "AT";
		break;

	case DEVICE_PS2:		/* requires a PS/1 or PS/2 system */
		p = "PS/2";
		break;
    
	default:		
		p = "ANY";
		break;
	}
    strcat(sp, p); sp += strlen(sp);

    /* Buses. */
    if (flags & 0xff00) {
	strcat(sp, ", ");
	sp += strlen(sp);

	switch(flags & 0xff00) {
		case DEVICE_S100:	/* requires an S100 bus slot */
			p = "S-100";
			break;

		case DEVICE_ISA:	/* requires an ISA bus slot */
			p = "ISA";
			break;

		case DEVICE_EISA:	/* requires an EISA bus slot */
			p = "EISA";
			break;

		case DEVICE_VLB:	/* requires a VLB bus slot */
			p = "VLB";
			break;

		case DEVICE_PCI:	/* requires a PCI bus slot */
			p = "PCI";
			break;

		case DEVICE_AGP:	/* requires an AGP bus slot */
			p = "AGP";
			break;

		case DEVICE_MCA:	/* requires an MCA bus slot */
			p = "MCA";
			break;

		case DEVICE_CBUS:	/* requires a C-BUS bus slot (PC98) */
			p = "CBUS";
			break;
	}
	strcat(sp, p); sp += strlen(sp);
    }

    /* Options. */
    if (flags & 0xffff0000) switch(flags & 0xffff0000) {
	case DEVICE_UNSTABLE:		/* unstable device, be cautious */
		strcat(sp, ", <UNS>");
		sp += strlen(sp);
		break;

	default:
		flags &= DEVICE_VIDTYPE;
		flags >>= 30;
		sprintf(sp, ", VID=%i", flags);
    }

    strcat(sp, " ]");

    return(buff);
}


void
device_dump(void)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] == NULL) continue;

	INFO("DEV[%2i] = '%s' flags=%s local=%08lx\n",
		c, devices[c]->name,
		device_flags(devices[c]->flags), devices[c]->local);
    }
}
#endif


/* Clone a master device for multi-instance devices. */
const device_t *
device_clone(const device_t *master)
{
    char temp[1024], *sp;
    clonedev_t *cl, *ptr;
    device_t *dev;

    /* Look up the master. */
    for (ptr = clones; ptr != NULL; ptr = ptr->next)
	if (ptr->master == master) break;

    /* If not found, add this master to the list. */
    if (ptr == NULL) {
	ptr = (clonedev_t *)mem_alloc(sizeof(clonedev_t));
	memset(ptr, 0x00, sizeof(clonedev_t));
	if (clones != NULL) {
		for (cl = clones; cl->next != NULL; cl = cl->next)
					;
		cl->next = ptr;
	} else
		clones = ptr;
	ptr->master = master;
    }

    /* Create a new device. */
    dev = (device_t *)mem_alloc(sizeof(device_t));

    /* Copy the master info. */
    memcpy(dev, ptr->master, sizeof(device_t));

    /* Set up a clone. */
    if (++ptr->count > 1)
	sprintf(temp, "%s #%i", ptr->master->name, ptr->count);
      else
	strcpy(temp, ptr->master->name);
    sp = (char *)mem_alloc(strlen(temp) + 1);
    strcpy(sp, temp);
    dev->name = (const char *)sp;

    return((const device_t *)dev);
}


/* Add a new device to the system. */
priv_t
device_add_parent(const device_t *d, priv_t parent)
{
    wchar_t temp[1024];
    priv_t priv = NULL;
    device_t *old;
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] == (device_t *)d) {
		ERRLOG("DEVICE: device already exists!\n");
		return(NULL);
	}
	if (devices[c] == NULL) break;
    }
    if (c >= DEVICE_MAX)
	fatal("DEVICE: too many devices\n");

    /*
     * If this has the DEVICE_UNSTABLE flag set, it is 
     * considered "Under Development", and caution should
     * be taken by the user. Just to avoid screaming bug
     * reports... we warn them...
     */
    if (d->flags & DEVICE_UNSTABLE) {
        swprintf(temp, sizeof_w(temp),
		 get_string(IDS_MSG_UNSTABL), d->name);

        /* Show the messagebox, and abort if 'No' was selected. */
        if (ui_msgbox(MBX_WARNING, temp) == 1) return(0);

        /* OK, they are fine with it. Log this! */
	INFO("DEVICE: device '%s' is unstable, user agreed!\n", d->name);
    }

    /*
     * Allocate device 'c' to the device being initialized.
     *
     * We have to do this BEFORE calling the init function, or
     * else this go bad if that function in turn adds devices
     * as well (chain loading) - which would then use this same
     * slot as it was not allocated yet.
     */
    devices[c] = (device_t *)d;

    /* Keep track of our current device, which is used for configuration. */
    old = device_current;
    device_current = (device_t *)d;

    /* We do not initialize 'root' (machine) devices. */
    if (d->init != NULL) {
	/* Pass the provided 'parent' argument. */
	priv = d->init(d, parent);
	if (priv == NULL) {
		if (d->name)
			ERRLOG("DEVICE: device '%s' init failed\n", d->name);
		  else
			ERRLOG("DEVICE: device init failed\n");

		/* Free up this slot. */
		devices[c] = NULL;

		return(NULL);
	}
    }

    device_priv[c] = priv;
    device_current = old;

    return(priv);
}


priv_t
device_add(const device_t *d)
{
    return(device_add_parent(d, device_priv[0]));
}


/* For devices that do not have an init function (internal video etc.) */
void
device_add_ex(const device_t *d, priv_t priv)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] == (device_t *)d) {
		fatal("DEVICE: device already exists!\n");
		break;
	}
	if (devices[c] == NULL) break;
    }
    if (c >= DEVICE_MAX)
	fatal("DEVICE: too many devices\n");

    devices[c] = (device_t *)d;
    device_priv[c] = priv;

    /*
     * Do set the 'current' pointer, so we can do configuration
     * right after adding our device. Obviously, this will only
     * be valid until the next device is added.
     */
    device_current = (device_t *)d;
}


void
device_remove(const device_t *d, priv_t priv)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if ((devices[c] == (device_t *)d) && (device_priv[c] == priv)) {
		/* Found it. Close the device. */
		if (devices[c]->close != NULL)
			devices[c]->close(device_priv[c]);

		/* Now move up all slots. */
		for (; c < DEVICE_MAX - 1; c++) {
			devices[c] = devices[c + 1];
			device_priv[c] = device_priv[c + 1];
		}

		/* Zap last slot. */
		devices[c] = NULL;
		device_priv[c] = NULL;

		return;
	}
    }
}


void
device_close_all(void)
{
    int c;

    for (c = (DEVICE_MAX - 1); c >= 0; c--) {
	if (devices[c] != NULL) {
		if (devices[c]->close != NULL)
			devices[c]->close(device_priv[c]);
		devices[c] = NULL;
		device_priv[c] = NULL;
	}
    }
}


void
device_reset_all(int flags)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] != NULL) {
		if (devices[c]->reset == NULL) continue;

		if (flags != DEVICE_ALL) {
			if (devices[c]->flags & flags)
				devices[c]->reset(device_priv[c]);
		} else
			devices[c]->reset(device_priv[c]);
	}
    }
}


priv_t
device_get_priv(const device_t *d)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] != NULL) {
		if (devices[c] == d)
			return(device_priv[c]);
	}
    }

    return(NULL);
}


/* Return name of the bus required by this device. */
const char *
device_get_bus_name(const device_t *d)
{
    const char *s = "unknown";

    if (d != NULL) switch(d->flags & DEVICE_BUS_MASK) {
	case DEVICE_ISA:
		s = "ISA";
		break;

	case DEVICE_CBUS:
		s = "CBUS";
		break;

	case DEVICE_MCA:
		s = "MCA";
		break;

	case DEVICE_EISA:
		s = "EISA";
		break;

	case DEVICE_VLB:
		s = "VLB";
		break;

	case DEVICE_PCI:
		s = "PCI";
		break;

	case DEVICE_AGP:
		s = "AGP";
		break;

	default:
		break;
    }

    return(s);
}


int
device_available(const device_t *d)
{
    int (*func)(void);

    if (d == NULL) return(1);

#ifdef RELEASE_BUILD
    if (d->flags & DEVICE_UNSTABLE) return(0);
#endif

    /* If the 'available' function member is set, use that. */
    if (d->dev_available != NULL) {
	func = d->dev_available;
	return(func());
    }

    /* Otherwise, just do a 'present' check on the ROM path. */
    if (d->path != NULL)
	return(rom_present(d->path));

    /* Neither... always available. */
    return(1);
}


void
device_speed_changed(void)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] != NULL) {
		if (devices[c]->speed_changed != NULL)
			devices[c]->speed_changed(device_priv[c]);
	}
    }

    sound_speed_changed();

    video_update_timing();
}


void
device_force_redraw(void)
{
    int c;

    for (c = 0; c < DEVICE_MAX; c++) {
	if (devices[c] != NULL) {
		if (devices[c]->force_redraw != NULL)
                                devices[c]->force_redraw(device_priv[c]);
	}
    }
}


const char *
device_get_config_string(const char *s)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_string(device_current->name, s, c->default_string));
	c++;
    }

    return(NULL);
}


int
device_get_config_int(const char *s)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_int(device_current->name, s, c->default_int));
	c++;
    }

    return(0);
}


int
device_get_config_int_ex(const char *s, int dflt)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_int(device_current->name, s, dflt));
	c++;
    }

    return(dflt);
}


int
device_get_config_hex16(const char *s)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_hex16(device_current->name, s, c->default_int));
	c++;
    }

    return(0);
}


int
device_get_config_hex20(const char *s)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_hex20(device_current->name, s, c->default_int));
	c++;
    }

    return(0);
}


int
device_get_config_mac(const char *s, int dflt)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name))
		return(config_get_mac(device_current->name, s, dflt));
	c++;
    }

    return(dflt);
}


void
device_set_config_int(const char *s, int val)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name)) {
 		config_set_int(device_current->name, s, val);
		break;
	}

	c++;
    }
}


void
device_set_config_hex16(const char *s, int val)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name)) {
		config_set_hex16(device_current->name, s, val);
		break;
	}

	c++;
    }
}


void
device_set_config_hex20(const char *s, int val)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name)) {
		config_set_hex20(device_current->name, s, val);
		break;
	}

	c++;
    }
}


void
device_set_config_mac(const char *s, int val)
{
    const device_config_t *c = device_current->config;

    while (c && c->name) {
	if (! strcmp(s, c->name)) {
		config_set_mac(device_current->name, s, val);
		break;
	}

	c++;
    }
}


int
device_is_valid(const device_t *device, int mflags)
{
    if (device == NULL) return(1);

    if ((device->flags & DEVICE_AT) && !(mflags & MACHINE_AT)) return(0);

    if ((device->flags & DEVICE_CBUS) && !(mflags & MACHINE_CBUS)) return(0);

    if ((device->flags & DEVICE_ISA) && !(mflags & MACHINE_ISA)) return(0);

    if ((device->flags & DEVICE_MCA) && !(mflags & MACHINE_MCA)) return(0);

    if ((device->flags & DEVICE_EISA) && !(mflags & MACHINE_EISA)) return(0);

    if ((device->flags & DEVICE_VLB) && !(mflags & MACHINE_VLB)) return(0);

    if ((device->flags & DEVICE_PCI) && !(mflags & MACHINE_PCI)) return(0);

    if ((device->flags & DEVICE_PS2) && !(mflags & MACHINE_HDC_PS2)) return(0);
    if ((device->flags & DEVICE_AGP) && !(mflags & MACHINE_AGP)) return(0);

    return(1);
}
