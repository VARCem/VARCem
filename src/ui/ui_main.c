/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Generic code for the "main" user interface.
 *
 *		This code is called by the UI frontend modules, and, also,
 *		depends on those same modules for lower-level functions.
 *
 * Version:	@(#)ui_main.c	1.0.5	2018/04/30
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
#include <wchar.h>
#include "../emu.h"
#include "../config.h"
#include "../device.h"
#include "../input/keyboard.h"
#include "../video/video.h"
#include "../plat.h"
#include "ui.h"
#include "ui_resource.h"


#if defined(ENABLE_LOG_TOGGLES) || defined(ENABLE_LOG_COMMANDS)
/* Simplest way to handle all these, for now.. */
void
ui_menu_set_logging_item(int idm, int val)
{
    int *ptr = NULL;

    switch(idm) {
#ifdef ENABLE_BUS_LOG
	case IDM_LOG_BUS:
		ptr = &pci_do_log;
		break;
#endif

#ifdef ENABLE_KEYBOARD_LOG
	case IDM_LOG_KEYBOARD:
		ptr = &keyboard_do_log;
		break;
#endif

#ifdef ENABLE_MOUSE_LOG
	case IDM_LOG_MOUSE:
		ptr = &mouse_do_log;
		break;
#endif

#ifdef ENABLE_GAME_LOG
	case IDM_LOG_GAME:
		ptr = &game_do_log;
		break;
#endif

#ifdef ENABLE_PARALLEL_LOG
	case IDM_LOG_PARALLEL:
		ptr = &parallel_do_log;
		break;
#endif

#ifdef ENABLE_FDC_LOG
	case IDM_LOG_FDC:
		ptr = &fdc_do_log;
		break;
#endif

#ifdef ENABLE_FDD_LOG
	case IDM_LOG_FDD:
		ptr = &fdd_do_log;
		break;
#endif

#ifdef ENABLE_D86F_LOG
	case IDM_LOG_D86F:
		ptr = &d86f_do_log;
		break;
#endif

#ifdef ENABLE_HDC_LOG
	case IDM_LOG_HDC:
		ptr = &hdc_do_log;
		break;
#endif

#ifdef ENABLE_HDD_LOG
	case IDM_LOG_HDD:
		ptr = &hdd_do_log;
		break;
#endif

#ifdef ENABLE_ZIP_LOG
	case IDM_LOG_ZIP:
		ptr = &zip_do_log;
		break;
#endif

#ifdef ENABLE_CDROM_LOG
	case IDM_LOG_CDROM:
		ptr = &cdrom_do_log;
		break;
#endif

#ifdef ENABLE_CDROM_IMAGE_LOG
	case IDM_LOG_CDROM_IMAGE:
		ptr = &cdrom_image_do_log;
		break;
#endif

#ifdef ENABLE_CDROM_IOCTL_LOG
	case IDM_LOG_CDROM_IOCTL:
		ptr = &cdrom_ioctl_do_log;
		break;
#endif

#ifdef ENABLE_NETWORK_LOG
	case IDM_LOG_NETWORK:
		ptr = &network_do_log;
		break;
#endif

#ifdef ENABLE_NETWORK_DEV_LOG
	case IDM_LOG_NETWORK_DEV:
		ptr = &network_dev_do_log;
		break;
#endif

#ifdef ENABLE_SOUND_EMU8K_LOG
	case IDM_LOG_SOUND_EMU8K:
		ptr = &sound_emu8k_do_log;
		break;
#endif

#ifdef ENABLE_SOUND_MPU401_LOG
	case IDM_LOG_SOUND_MPU401:
		ptr = &sound_mpu401_do_log;
		break;
#endif

#ifdef ENABLE_SOUND_DEV_LOG
	case IDM_LOG_SOUND_DEV:
		ptr = &sound_dev_do_log;
		break;
#endif

#ifdef ENABLE_SCSI_BUS_LOG
	case IDM_LOG_SCSI_BUS:
		ptr = &scsi_bus_do_log;
		break;
#endif

#ifdef ENABLE_SCSI_DISK_LOG
	case IDM_LOG_SCSI_DISK:
		ptr = &scsi_hd_do_log;
		break;
#endif

#ifdef ENABLE_SCSI_DEV_LOG
	case IDM_LOG_SCSI_DEV:
		ptr = &scsi_dev_do_log;
		break;
#endif

#ifdef ENABLE_VOODOO_LOG
	case IDM_LOG_VOODOO:
		ptr = &voodoo_do_log;
		break;
#endif
    }

    if (ptr != NULL) {
	/* Set the desired value. */
	if (val != 0xff) {
		/* Initialize the logging value. */
		if (val < 0) *ptr ^= 1;
		  else *ptr = val;
	}

	/* And update its menu entry. */
	menu_set_item(idm, *ptr);
    }
}
#endif


/* Toggle one of the Video options, with a lock on the blitter. */
void
ui_menu_toggle_video_item(int idm, int *val)
{
    startblit();
    video_wait_for_blit();
    *val ^= 1;
    endblit();

    menu_set_item(idm, *val);

    device_force_redraw();

    config_save();
}


/* Reset all (main) menu items to their default state. */
void
ui_menu_reset_all(void)
{
    int i;

#ifndef DEV_BRANCH
    /* FIXME: until we fix these.. --FvK */
    menu_enable_item(IDM_LOAD, 0);
    menu_enable_item(IDM_SAVE, 0);
#endif

    menu_set_item(IDM_RESIZE, vid_resize);
    menu_set_item(IDM_REMEMBER, window_remember);

    menu_set_radio_item(IDM_RENDER_1, 4, -1);

    menu_set_radio_item(IDM_SCALE_1, 4, scale);

    menu_set_item(IDM_FULLSCREEN, vid_fullscreen);

    menu_set_radio_item(IDM_STRETCH, 5, vid_fullscreen_scale);

    menu_set_item(IDM_RCTRL_IS_LALT, rctrl_is_lalt);

    menu_set_item(IDM_UPDATE_ICONS, update_icons);

    menu_set_item(IDM_INVERT, invert_display);
    menu_set_item(IDM_OVERSCAN, enable_overscan);

    menu_set_radio_item(IDM_SCREEN_RGB, 5, vid_grayscale);

    menu_set_radio_item(IDM_GRAY_601, 3, vid_graytype);

    menu_set_item(IDM_FORCE_43, force_43);

    menu_set_item(IDM_CGA_CONTR, vid_cga_contrast);

#ifdef ENABLE_LOG_TOGGLES
    for (i = IDM_LOG_BEGIN; i < IDM_LOG_END; i++)
	ui_menu_set_logging_item(i, 0xff);
#endif
}


/* Handle a main menu command. */
int
ui_menu_command(int idm)
{
    wchar_t temp[512];
    int i;

    switch (idm) {
	case IDM_RESET_HARD:			/* ACTION menu */
		pc_reset(1);
		break;

	case IDM_RESET:				/* ACTION menu */
		pc_reset(0);
		break;

	case IDM_CAE:				/* ACTION menu */
		keyboard_send_cae();
		break;

	case IDM_CAB:				/* ACTION menu */
		keyboard_send_cab();
		break;

	case IDM_PAUSE:				/* ACTION menu */
		plat_pause(dopause ^ 1);
		menu_set_item(idm, dopause);
		break;

#ifdef IDM_Test
	case IDM_Test:				/* ACTION menu */
		pclog("TEST\n");
		break;
#endif

	case IDM_EXIT:				/* ACTION menu */
		/*NOTHANDLED*/
		return(0);

	case IDM_RESIZE:			/* VIEW menu */
		vid_resize ^= 1;
		menu_set_item(idm, vid_resize);
		if (vid_resize) {
			/* Force scaling to 1.0. */
			scale = 1;
			menu_set_radio_item(IDM_SCALE_1, 4, scale);
		}

		/* Disable scaling settings. */
		for (i = 0; i < 4; i++)
			menu_enable_item(IDM_SCALE_1+i, !vid_resize);
		doresize = 1;
		config_save();
		return(0);

	case IDM_REMEMBER:			/* VIEW menu */
		window_remember ^= 1;
		menu_set_item(idm, window_remember);
		return(0);

	case IDM_RENDER_1:			/* VIEW menu */
	case IDM_RENDER_2:
	case IDM_RENDER_3:
	case IDM_RENDER_4:
		plat_setvid(idm - IDM_RENDER_1);
		menu_set_radio_item(IDM_RENDER_1, 4, vid_api);
		config_save();
		break;

	case IDM_FULLSCREEN:			/* VIEW menu */
		plat_setfullscreen(1);
		config_save();
		break;

	case IDM_STRETCH:			/* VIEW menu */
	case IDM_STRETCH_43:
	case IDM_STRETCH_SQ:                                
	case IDM_STRETCH_INT:
	case IDM_STRETCH_KEEP:
		vid_fullscreen_scale = (idm - IDM_STRETCH);
		menu_set_radio_item(IDM_STRETCH, 5, vid_fullscreen_scale);
		device_force_redraw();
		config_save();
		break;

	case IDM_SCALE_1:			/* VIEW menu */
	case IDM_SCALE_2:
	case IDM_SCALE_3:
	case IDM_SCALE_4:
		scale = (idm - IDM_SCALE_1);
		menu_set_radio_item(IDM_SCALE_1, 4, scale);
		device_force_redraw();
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_FORCE_43:			/* VIEW menu */
		ui_menu_toggle_video_item(idm, &force_43);
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_RCTRL_IS_LALT:			/* VIEW menu */
		rctrl_is_lalt ^= 1;
		menu_set_item(idm, rctrl_is_lalt);
		config_save();
		break;

	case IDM_UPDATE_ICONS:			/* VIEW menu */
		update_icons ^= 1;
		menu_set_item(idm, update_icons);
		config_save();
		break;

	case IDM_INVERT:			/* DISPLAY menu */
		ui_menu_toggle_video_item(idm, &invert_display);
		config_save();
		break;

	case IDM_OVERSCAN:			/* DISPLAY menu */
		ui_menu_toggle_video_item(idm, &enable_overscan);
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_SCREEN_RGB:			/* DISPLAY menu */
	case IDM_SCREEN_GRAYSCALE:
	case IDM_SCREEN_AMBER:
	case IDM_SCREEN_GREEN:
	case IDM_SCREEN_WHITE:
		vid_grayscale = (idm - IDM_SCREEN_RGB);
		menu_set_radio_item(IDM_SCREEN_RGB, 5, vid_grayscale);
		device_force_redraw();
		config_save();
		break;

	case IDM_GRAY_601:			/* DISPLAY menu */
	case IDM_GRAY_709:
	case IDM_GRAY_AVE:
		vid_graytype = (idm - IDM_GRAY_601);
		menu_set_radio_item(IDM_GRAY_601, 3, vid_graytype);
		device_force_redraw();
		config_save();
		break;

	case IDM_CGA_CONTR:			/* DISPLAY menu */
		vid_cga_contrast ^= 1;
		menu_set_item(idm, vid_cga_contrast);
		cgapal_rebuild();
		config_save();
		break;

	case IDM_SETTINGS:			/* TOOLS menu */
		plat_pause(1);
		if (dlg_settings(1) == 2)
			pc_reset_hard_init();
		plat_pause(0);
		break;

#ifdef ENABLE_LOG_TOGGLES
	case IDM_LOG_BUS:			/* TOOLS menu */
	case IDM_LOG_KEYBOARD:
	case IDM_LOG_MOUSE:
	case IDM_LOG_GAME:
	case IDM_LOG_PARALLEL:
	case IDM_LOG_SERIAL:
	case IDM_LOG_FDC:
	case IDM_LOG_FDD:
	case IDM_LOG_D86F:
	case IDM_LOG_HDC:
	case IDM_LOG_HDD:
	case IDM_LOG_ZIP:
	case IDM_LOG_CDROM:
	case IDM_LOG_CDROM_IMAGE:
	case IDM_LOG_CDROM_IOCTL:
	case IDM_LOG_NETWORK:
	case IDM_LOG_NETWORK_DEV:
	case IDM_LOG_SOUND_EMU8K:
	case IDM_LOG_SOUND_MPU401:
	case IDM_LOG_SOUND_DEV:
	case IDM_LOG_SCSI_BUS:
	case IDM_LOG_SCSI_DISK:
	case IDM_LOG_SCSI_DEV:
	case IDM_LOG_VOODOO:
		ui_menu_set_logging_item(idm, -1);
		break;
#endif

#ifdef ENABLE_LOG_BREAKPOINT
	case IDM_LOG_BREAKPOINT:		/* TOOLS menu */
		pclog("---- LOG BREAKPOINT ----\n");
		break;
#endif

	/* FIXME: need to fix these.. */
	case IDM_LOAD:				/* TOOLS menu */
		plat_pause(1);
		if (! dlg_file(plat_get_string(IDS_2160), NULL, temp, 0) &&
		    (ui_msgbox(MBX_QUESTION, (wchar_t *)IDS_2051) == 0)) {
			pc_reload(temp);
			ui_menu_reset_all();
		}
		plat_pause(0);
		break;                        

	case IDM_SAVE:				/* TOOLS menu */
		plat_pause(1);
		if (! dlg_file(plat_get_string(IDS_2160), NULL, temp, 1)) {
			config_write(temp);
		}
		plat_pause(0);
		break;                                                

	case IDM_STATUS:			/* TOOLS menu */
		dlg_status();
		break;

	case IDM_SCREENSHOT:			/* TOOLS menu */
		take_screenshot();
		break;

	case IDM_ABOUT:				/* HELP menu */
		dlg_about();
		break;
    }

    return(1);
}
