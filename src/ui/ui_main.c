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
 * Version:	@(#)ui_main.c	1.0.26	2021/02/18
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018-2021 Fred N. van Kempen.
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
#include "../config.h"
#include "../device.h"
#include "../plat.h"
#include "../devices/input/keyboard.h"
#include "../devices/input/mouse.h"
#include "../devices/video/video.h"
#include "ui.h"


#ifdef _LOGGING
/* Simplest way to handle all these, for now.. */
static void
set_logging_item(int idm, int val)
{
    wchar_t temp[128];
    wchar_t tmp2[128];
    void *ptr = NULL;
    int *iptr, i;

    switch(idm) {
# ifdef ENABLE_BUS_LOG
	case IDM_LOG_BUS:
		ptr = (val != -3) ? &pci_do_log : (void *)"Bus";
		break;
# endif

# ifdef ENABLE_KEYBOARD_LOG
	case IDM_LOG_KEYBOARD:
		ptr = (val != -3) ? &keyboard_do_log : (void *)"Keyboard";
		break;
# endif

# ifdef ENABLE_MOUSE_LOG
	case IDM_LOG_MOUSE:
		ptr = (val != -3) ? &mouse_do_log : (void *)"Mouse";
		break;
# endif

# ifdef ENABLE_GAME_LOG
	case IDM_LOG_GAME:
		ptr = (val != -3) ? &game_do_log : (void *)"Game Port";
		break;
# endif

# ifdef ENABLE_PARALLEL_LOG
	case IDM_LOG_PARALLEL:
		ptr = (val != -3) ? &parallel_do_log : (void *)"Parallel Port";
		break;
# endif

# ifdef ENABLE_SERIAL_LOG
	case IDM_LOG_SERIAL:
		ptr = (val != -3) ? &serial_do_log : (void *)"Serial Port";
		break;
# endif

# ifdef ENABLE_FDC_LOG
	case IDM_LOG_FDC:
		ptr = (val != -3) ? &fdc_do_log : (void *)"FDC";
		break;
# endif

# ifdef ENABLE_FDD_LOG
	case IDM_LOG_FDD:
		ptr = (val != -3) ? &fdd_do_log : (void *)"FDD (image)";
		break;
# endif

# ifdef ENABLE_D86F_LOG
	case IDM_LOG_D86F:
		ptr = (val != -3) ? &d86f_do_log : (void *)"D86F";
		break;
# endif

# ifdef ENABLE_HDC_LOG
	case IDM_LOG_HDC:
		ptr = (val != -3) ? &hdc_do_log : (void *)"HDC";
		break;
# endif

# ifdef ENABLE_HDD_LOG
	case IDM_LOG_HDD:
		ptr = (val != -3) ? &hdd_do_log : (void *)"HDD (image)";
		break;
# endif

# ifdef ENABLE_ZIP_LOG
	case IDM_LOG_ZIP:
		ptr = (val != -3) ? &zip_do_log : (void *)"ZIP";
		break;
# endif

# ifdef ENABLE_CDROM_LOG
	case IDM_LOG_CDROM:
		ptr = (val != -3) ? &cdrom_do_log : (void *)"CD-ROM";
		break;
# endif
# ifdef ENABLE_CDROM_IMAGE_LOG
	case IDM_LOG_CDROM_IMAGE:
		ptr = (val != -3) ? &cdrom_image_do_log : (void *)"CD-ROM (image)";
		break;
# endif
# ifdef ENABLE_CDROM_HOST_LOG
	case IDM_LOG_CDROM_HOST:
		ptr = (val != -3) ? &cdrom_host_do_log : (void *)"CD-ROM (host)";
		break;
# endif

# ifdef ENABLE_NETWORK_LOG
	case IDM_LOG_NETWORK:
		ptr = (val != -3) ? &network_do_log : (void *)"Network";
		break;
# endif
# ifdef ENABLE_NETWORK_DEV_LOG
	case IDM_LOG_NETWORK_DEV:
		ptr = (val != -3) ? &network_card_do_log : (void *)"Network Device";
		break;
# endif

# ifdef ENABLE_SOUND_LOG
	case IDM_LOG_SOUND:
		ptr = (val != -3) ? &sound_do_log : (void *)"Sound";
		break;
# endif
# ifdef ENABLE_SOUND_DEV_LOG
	case IDM_LOG_SOUND_DEV:
		ptr = (val != -3) ? &sound_card_do_log : (void *)"Sound Device";
		break;
# endif
# ifdef ENABLE_SOUND_MIDI_LOG
	case IDM_LOG_SOUND_MIDI:
		ptr = (val != -3) ? &sound_midi_do_log : (void *)"Sound (MIDI)";
		break;
# endif

# ifdef ENABLE_SCSI_LOG
	case IDM_LOG_SCSI:
		ptr = (val != -3) ? &scsi_do_log : (void *)"SCSI";
		break;
# endif
# ifdef ENABLE_SCSI_DEV_LOG
	case IDM_LOG_SCSI_DEV:
		ptr = (val != -3) ? &scsi_card_do_log : (void *)"SCSI Device";
		break;
# endif
# ifdef ENABLE_SCSI_CDROM_LOG
	case IDM_LOG_SCSI_CDROM:
		ptr = (val != -3) ? &scsi_cdrom_do_log : (void *)"SCSI (CD-ROM)";
		break;
# endif
# ifdef ENABLE_SCSI_DISK_LOG
	case IDM_LOG_SCSI_DISK:
		ptr = (val != -3) ? &scsi_disk_do_log : (void *)"SCSI (Disk)";
		break;
# endif

# ifdef ENABLE_VIDEO_LOG
	case IDM_LOG_VIDEO:
		ptr = (val != -3) ? &video_do_log : (void *)"Video";
		break;
# endif
# ifdef ENABLE_VIDEO_DEV_LOG
	case IDM_LOG_VIDEO_DEV:
		ptr = (val != -3) ? &video_card_do_log : (void *)"Video Device";
		break;
# endif
    }

    if (ptr == NULL) return;

    iptr = (int *)ptr;
    switch(val) {
	case -3:		/* create menu item */
		/* Add a menu item. */
		i = (idm - IDM_LOG_BEGIN);
		swprintf(temp, sizeof_w(temp),
			 get_string(IDS_4085), (const char *)ptr);
		swprintf(tmp2, sizeof_w(tmp2), L"\tCtrl%s+F%i",
			(i >= 12) ? "+Alt" : "", ((i >= 12) ? i - 12 : i) + 1);
		wcscat(temp, tmp2);
		menu_add_item(IDM_LOGGING, ITEM_CHECK, IDM_LOG_BEGIN+i, temp);
		break;

	case -2:		/* set current value */
		menu_set_item(idm, *iptr);
		break;

	case -1:		/* toggle */
#if 1
		if (*iptr <= LOG_INFO)
			*iptr = LOG_DEBUG;
		  else
			*iptr = LOG_INFO;
		menu_set_item(idm, (*iptr >= LOG_DEBUG) ? 1 : 0);
#else
		*iptr ^= 1;
		menu_set_item(idm, *iptr);
#endif
		break;

	default:		/* 0 ... n */
#if 1
		*iptr = LOG_INFO + val;
		menu_set_item(idm, (*iptr > LOG_INFO) ? 1 : 0);
#else
		*iptr = val;
		menu_set_item(idm, *iptr);
#endif
		break;
    }
}
#endif


/* Toggle one of the Video options, with a lock on the blitter. */
static void
toggle_video_item(int idm, int *val)
{
    plat_blitter(1);
    *val ^= 1;
    plat_blitter(0);

    menu_set_item(idm, *val);

    device_force_redraw();

    config_save();
}


/* Reset all (main) menu items to their default state. */
static void
main_reset_all(void)
{
    wchar_t temp[128];
    int i;

    menu_set_item(IDM_RESIZE, config.vid_resize);
    menu_set_item(IDM_REMEMBER, config.window_remember);

    /* Add all renderers to the Renderer menu. */
    for (i = 0; vidapi_getname(i) != NULL; i++) {
	/* Get name of the renderer and add a menu item. */
	mbstowcs(temp, vidapi_getname(i), sizeof_w(temp));
	menu_add_item(IDM_RENDER, ITEM_RADIO, IDM_RENDER_1 + i, temp);

	/* Disable entries that are not available. */
	if (! vidapi_available(i))
		menu_enable_item(IDM_RENDER_1 + i, 0);
    }
    menu_set_radio_item(IDM_RENDER_1, i, config.vid_api);

    menu_set_radio_item(IDM_SCALE_1, 4, config.scale);

    menu_set_item(IDM_FULLSCREEN, config.vid_fullscreen);

    menu_set_radio_item(IDM_STRETCH, 4, config.vid_fullscreen_scale);

    menu_set_item(IDM_RCTRL_IS_LALT, config.rctrl_is_lalt);

    menu_set_item(IDM_INVERT, config.invert_display);
    menu_set_item(IDM_OVERSCAN, config.enable_overscan);

    menu_set_radio_item(IDM_SCREEN_RGB, 5, config.vid_grayscale);

    menu_set_radio_item(IDM_GRAY_601, 3, config.vid_graytype);

    menu_set_item(IDM_FORCE_43, config.force_43);

    menu_set_item(IDM_CGA_CONTR, config.vid_cga_contrast);

#ifdef _LOGGING
    for (i = IDM_LOG_BEGIN; i < IDM_LOG_END; i++) {
	set_logging_item(i, -3);

	set_logging_item(i, -2);
    }
#endif

#ifdef DEV_BRANCH
    /* FIXME: until we fix these.. --FvK */
    menu_enable_item(IDM_LOAD, 0);
    menu_enable_item(IDM_SAVE, 0);
#endif

    /* Load the Languages into the current menu. */
    ui_lang_menu();
}


/* Re-load and reset the entire UI. */
void
ui_reset(void)
{
    /* Maybe the underlying (platform) UI has to rebuild. */
    ui_plat_reset();

    /* Reset all main menu items. */
    main_reset_all();

    /* Update the statusbar menus. */
    ui_sb_reset();

    /* Reset the mouse-capture message. */
    ui_mouse_capture(-1);
}


/* Handle a main menu command. */
int
ui_menu_command(int idm)
{
    wchar_t temp[512];
    int i;

    switch (idm) {
	case IDM_RESET_HARD:			// ACTION menu
		pc_reset(1);
		break;

	case IDM_RESET:				// ACTION menu
		pc_reset(0);
		break;

	case IDM_CAE:				// ACTION menu
		keyboard_cae();
		break;

	case IDM_CAB:				// ACTION menu
		keyboard_cab();
		break;

	case IDM_PAUSE:				// ACTION menu
		pc_pause(dopause ^ 1);
		break;

#ifdef IDM_Test
	case IDM_Test:				// ACTION menu
		pclog(LOG_ALWAYS, "TEST\n");
		break;
#endif

	case IDM_EXIT:				// ACTION menu
		/*NOTHANDLED*/
		return(0);

	case IDM_RESIZE:			// VIEW menu
		config.vid_resize ^= 1;
		menu_set_item(idm, config.vid_resize);
		if (config.vid_resize) {
			/* Force scaling to 1.0. */
			config.scale = 1;
			menu_set_radio_item(IDM_SCALE_1, 4, config.scale);
		}

		/* Disable scaling settings. */
		for (i = 0; i < 4; i++)
			menu_enable_item(IDM_SCALE_1+i, !config.vid_resize);

		device_force_redraw();
		video_force_resize_set(1);
		config_save();
		return(0);

	case IDM_REMEMBER:			// VIEW menu
		config.window_remember ^= 1;
		menu_set_item(idm, config.window_remember);
		return(0);

	case IDM_RENDER_1:			// VIEW menu
	case IDM_RENDER_2:
	case IDM_RENDER_3:
	case IDM_RENDER_4:
	case IDM_RENDER_5:
	case IDM_RENDER_6:
	case IDM_RENDER_7:
	case IDM_RENDER_8:
		vidapi_set(idm - IDM_RENDER_1);
		config_save();
		break;

	case IDM_FULLSCREEN:			// VIEW menu
		ui_fullscreen(1);
		break;

	case IDM_STRETCH:			// VIEW menu
	case IDM_STRETCH_43:
	case IDM_STRETCH_KEEP:
	case IDM_STRETCH_INT:
		config.vid_fullscreen_scale = (idm - IDM_STRETCH);
		menu_set_radio_item(IDM_STRETCH, 4,
				    config.vid_fullscreen_scale);
		device_force_redraw();
		config_save();
		break;

	case IDM_SCALE_1:			// VIEW menu
	case IDM_SCALE_2:
	case IDM_SCALE_3:
	case IDM_SCALE_4:
		config.scale = (idm - IDM_SCALE_1);
		menu_set_radio_item(IDM_SCALE_1, 4, config.scale);
		device_force_redraw();
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_FORCE_43:			// VIEW menu
		toggle_video_item(idm, &config.force_43);
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_RCTRL_IS_LALT:			// VIEW menu
		config.rctrl_is_lalt ^= 1;
		menu_set_item(idm, config.rctrl_is_lalt);
		config_save();
		break;

	case IDM_INVERT:			// DISPLAY menu
		toggle_video_item(idm, &config.invert_display);
		config_save();
		break;

	case IDM_OVERSCAN:			// DISPLAY menu
		toggle_video_item(idm, &config.enable_overscan);
		video_force_resize_set(1);
		config_save();
		break;

	case IDM_SCREEN_RGB:			// DISPLAY menu
	case IDM_SCREEN_GRAYSCALE:
	case IDM_SCREEN_AMBER:
	case IDM_SCREEN_GREEN:
	case IDM_SCREEN_WHITE:
		config.vid_grayscale = (idm - IDM_SCREEN_RGB);
		menu_set_radio_item(IDM_SCREEN_RGB, 5, config.vid_grayscale);
		device_force_redraw();
		config_save();
		break;

	case IDM_GRAY_601:			// DISPLAY menu
	case IDM_GRAY_709:
	case IDM_GRAY_AVE:
		config.vid_graytype = (idm - IDM_GRAY_601);
		menu_set_radio_item(IDM_GRAY_601, 3, config.vid_graytype);
		device_force_redraw();
		config_save();
		break;

	case IDM_CGA_CONTR:			// DISPLAY menu
		config.vid_cga_contrast ^= 1;
		menu_set_item(idm, config.vid_cga_contrast);
		video_palette_rebuild();
		config_save();
		break;

	case IDM_SETTINGS:			// TOOLS menu
		pc_pause(1);
		if (dlg_settings(1) == 2)
			pc_reset_hard_init();
		pc_pause(0);
		break;

	case IDM_LANGUAGE+1:			// select language
	case IDM_LANGUAGE+2:
	case IDM_LANGUAGE+3:
	case IDM_LANGUAGE+4:
	case IDM_LANGUAGE+5:
	case IDM_LANGUAGE+6:
	case IDM_LANGUAGE+7:
	case IDM_LANGUAGE+8:
	case IDM_LANGUAGE+9:
	case IDM_LANGUAGE+10:
	case IDM_LANGUAGE+11:
	case IDM_LANGUAGE+12:
	case IDM_LANGUAGE+13:
	case IDM_LANGUAGE+14:
	case IDM_LANGUAGE+15:
	case IDM_LANGUAGE+16:
	case IDM_LANGUAGE+17:
	case IDM_LANGUAGE+18:
	case IDM_LANGUAGE+19:
	case IDM_LANGUAGE+20:
		ui_lang_set(idm - (IDM_LANGUAGE + 1));
		ui_reset();
		config_save();
		break;

#ifdef _LOGGING
	case IDM_LOG_BREAKPOINT:		// TOOLS menu
		pclog(LOG_ALWAYS, "---- LOG BREAKPOINT ----\n");
		break;

	case IDM_LOG_BEGIN:			// TOOLS menu
	case IDM_LOG_BEGIN+1:
	case IDM_LOG_BEGIN+2:
	case IDM_LOG_BEGIN+3:
	case IDM_LOG_BEGIN+4:
	case IDM_LOG_BEGIN+5:
	case IDM_LOG_BEGIN+6:
	case IDM_LOG_BEGIN+7:
	case IDM_LOG_BEGIN+8:
	case IDM_LOG_BEGIN+9:
	case IDM_LOG_BEGIN+10:
	case IDM_LOG_BEGIN+11:
	case IDM_LOG_BEGIN+12:
	case IDM_LOG_BEGIN+13:
	case IDM_LOG_BEGIN+14:
	case IDM_LOG_BEGIN+15:
	case IDM_LOG_BEGIN+16:
	case IDM_LOG_BEGIN+17:
	case IDM_LOG_BEGIN+18:
	case IDM_LOG_BEGIN+19:
	case IDM_LOG_BEGIN+20:
	case IDM_LOG_BEGIN+21:
	case IDM_LOG_BEGIN+22:
	case IDM_LOG_BEGIN+23:
	case IDM_LOG_BEGIN+24:
	case IDM_LOG_BEGIN+25:
	case IDM_LOG_BEGIN+26:
	case IDM_LOG_BEGIN+27:
	case IDM_LOG_BEGIN+28:
	case IDM_LOG_BEGIN+29:
		set_logging_item(idm, -1);
		break;
#endif

	/* FIXME: need to fix these.. */
	case IDM_LOAD:				// TOOLS menu
		pc_pause(1);
		i = dlg_file(get_string(IDS_2500), NULL, temp, DLG_FILE_LOAD);
		if (i && (ui_msgbox(MBX_QUESTION, (wchar_t *)IDS_WARNING) == 0)) {
			pc_reload(temp);
			ui_reset();
			config_ro = !!(i & DLG_FILE_RO);
		}
		pc_pause(0);
		break;                        

	case IDM_SAVE:				// TOOLS menu
		pc_pause(1);
		if (dlg_file(get_string(IDS_2500), NULL, temp, DLG_FILE_SAVE)) {
			config_write(temp);
		}
		pc_pause(0);
		break;                                                

	case IDM_SCREENSHOT:			// TOOLS menu
		vidapi_screenshot();
		break;

	case IDM_ABOUT:				// HELP menu
		pc_pause(1);
		dlg_about();
		pc_pause(0);
		break;

	case IDM_LOCALIZE:			// HELP MENU 
		pc_pause(1);
		dlg_localize();
		pc_pause(0);
		break;
    }

    return(1);
}


/* Set the desired fullscreen/windowed mode. */
void
ui_fullscreen(int on)
{
    /* Mode change? */
    if (on == config.vid_fullscreen) return;

    if (on && config.vid_fullscreen_first) {
	config.vid_fullscreen_first = 0;
	ui_msgbox(MBX_INFO, (wchar_t *)IDS_MSG_WINDOW);
    }

    /* OK, claim the video. */
    plat_blitter(1);
//    video_blit_wait();

    /* Close the current mode, and open the new one. */
    plat_vidapis[config.vid_api]->close();
    config.vid_fullscreen = on;
    plat_vidapis[config.vid_api]->init(config.vid_fullscreen);

    plat_fullscreen(on);

    /* Release video and make it redraw the screen. */
    plat_blitter(0);
    device_force_redraw();

    /* Finally, handle the host's mouse cursor. */
    ui_show_cursor(config.vid_fullscreen ? 0 : -1);

    /* Update the menu item. */
    menu_set_item(IDM_FULLSCREEN, on);
}


/* Enable or disable mouse clipping. */
void
ui_mouse_capture(int on)
{
    const wchar_t *str = NULL;
    int state = mouse_capture;

    /* Do not try to capture the mouse if no mouse configured. */
    if (config.mouse_type == MOUSE_NONE) return;

    if ((on == 1) && !mouse_capture) {
	state = 1;
    } else if (!on && mouse_capture) {
	state = 0;
    }

    if (state != mouse_capture) {
	/* State changed. */
	plat_mouse_capture(state);
	ui_show_cursor(!state);
	mouse_capture = state;
    }

    if (state) {
	if (mouse_get_buttons() > 2)
		str = get_string(IDS_MSG_MRLS_1);
	  else
		str = get_string(IDS_MSG_MRLS_2);
    } else
	str = get_string(IDS_MSG_CAPTURE);

    /* Set the correct message on the status bar. */
    ui_sb_text_set_w(SB_TEXT, str);
}
