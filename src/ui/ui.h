/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Define the various UI functions.
 *
 * Version:	@(#)ui.h	1.0.9	2018/05/11
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
#ifndef EMU_UI_H
# define EMU_UI_H


/* Also include our common resources. */
#include "ui_resource.h"


/* Message Box definitions. */
#define MBX_INFO	1
#define MBX_WARNING	2
#define MBX_ERROR	3
#define MBX_QUESTION	4
#define MBX_CONFIG	5
#define MBX_FATAL	0x20
#define MBX_ANSI	0x80

/* FileDialog flags. */
#define DLG_FILE_LOAD	0x00
#define DLG_FILE_SAVE	0x01
#define DLG_FILE_RO	0x80

/* Status Bar definitions. */
#define SB_ICON_WIDTH	24
#define SB_FLOPPY       0x00
#define SB_CDROM        0x10
#define SB_ZIP          0x20
#define SB_RDISK        0x30
#define SB_HDD          0x50
#define SB_NETWORK      0x60
#define SB_SOUND        0x70
#define SB_TEXT         0x80

#ifdef USE_WX
# define RENDER_FPS	30			/* default render speed */
#endif

/* Define whether or not we need the Logging submenu. */
#if defined(ENABLE_BUS_LOG) || \
    defined(ENABLE_KEYBOARD_LOG) || defined(ENABLE_MOUSE_LOG) || \
    defined(ENABLE_GAME_LOG) || \
    defined(ENABLE_SERIAL_LOG) || defined(ENABLE_PARALLELL_LOG) || \
    defined(ENABLE_FDC_LOG) || defined(ENABLE_FDD_LOG) || \
    defined(ENABLE_D86F_LOG) || \
    defined(ENABLE_HDC_LOG) || defined(ENABLE_HDD_LOG) || \
    defined(ENABLE_ZIP_LOG) || defined(ENABLE_CDROM_LOG) || \
    defined(ENABLE_CDROM_IMAGE_LOG) || defined(ENABLE_CDROM_IOCTL_LOG) || \
    defined(ENABLE_SOUND_EMU8K_LOG) || defined(ENABLE_SOUND_MPU401_LOG) || \
    defined(ENABLE_SOUND_DEV_LOG) || \
    defined(ENABLE_NETWORK_LOG) || defined(ENABLE_NETWORK_DEV_LOG) || \
    defined(ENABLE_SCSI_BUS_LOG) || defined(ENABLE_SCSI_DISK_LOG) || \
    defined(ENABLE_SCSI_DEV_LOG) || \
    defined(ENABLE_VOODOO_LOG)
# define ENABLE_LOG_TOGGLES	1
#endif

#if defined(ENABLE_LOG_BREAKPOINT)
# define ENABLE_LOG_COMMANDS	1
#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_LOG_TOGGLES
extern int	pci_do_log;
extern int	keyboard_do_log;
extern int	mouse_do_log;
extern int	game_do_log;
extern int	parallel_do_log;
extern int	serial_do_log;
extern int	fdc_do_log;
extern int	fdd_do_log;
extern int	d86f_do_log;
extern int	hdc_do_log;
extern int	hdd_do_log;
extern int	zip_do_log;
extern int	cdrom_do_log;
extern int	cdrom_image_do_log;
extern int	cdrom_ioctl_do_log;
extern int	sound_emu8k_do_log;
extern int	sound_mpu401_do_log;
extern int	sound_dev_do_log;
extern int	network_do_log;
extern int	network_dev_do_log;
extern int	scsi_bus_do_log;
extern int	scsi_hd_do_log;
extern int	scsi_dev_do_log;
extern int	voodoo_do_log;
#endif

/* Main GUI functions. */
extern void	ui_show_cursor(int on);
extern void	ui_show_render(int on);
extern void	ui_resize(int x, int y);
extern int	ui_msgbox(int type, void *arg);
extern void	ui_menu_reset_all(void);
extern int	ui_menu_command(int idm);
extern void	ui_menu_set_logging_item(int idm, int val);
extern void	ui_menu_toggle_video_item(int idm, int *val);

/* Main GUI helper functions. */
extern void	menu_enable_item(int idm, int val);
extern void	menu_set_item(int idm, int val);
extern void	menu_set_radio_item(int idm, int num, int val);
extern wchar_t  *ui_window_title(const wchar_t *s);

/* Status Bar functions. */
extern void	ui_sb_update(void);
extern void	ui_sb_click(int part);
extern void	ui_sb_menu_command(int idm, int tag);
extern void	ui_sb_menu_enable_item(int tag, int id, int val);
extern void	ui_sb_menu_set_item(int tag, int id, int val);
extern void	ui_sb_text_set_w(const wchar_t *str);
extern void	ui_sb_text_set(const char *str);
extern void	ui_sb_icon_update(int tag, int val);
extern void	ui_sb_icon_state(int tag, int active);
extern void	ui_sb_tip_update(int tag);
extern void	ui_sb_mount_floppy(uint8_t drive, int part, int8_t wp,
				   const wchar_t *fn);
extern void	ui_sb_mount_zip(uint8_t drive, int part, int8_t wp,
				const wchar_t *fn);

/* Status Bar helper functions. */
extern int	sb_fdd_icon(int type);
extern void	sb_setup(int parts, const int *widths);
extern void	sb_menu_destroy(void);
extern void	sb_menu_create(int part);
extern void	sb_menu_add_item(int part, int idm, const wchar_t *str);
extern void	sb_menu_enable_item(int part, int idm, int val);;
extern void	sb_menu_set_item(int part, int idm, int val);
extern void	sb_set_icon(int part, int icon);
extern void	sb_set_text(int part, const wchar_t *str);
extern void	sb_set_tooltip(int part, const wchar_t *str);

/* Dialogs. */
extern void	dlg_about(void);
extern int	dlg_settings(int ask);
extern void	dlg_status(void);
extern void	dlg_status_update(void);
extern void     dlg_new_floppy(int idm, int tag);
extern void	dlg_sound_gain(void);
extern int      dlg_file(const wchar_t *filt, const wchar_t *ifn,
			 wchar_t *fn, int save);

/* Platform VidApi. */
extern int	vidapi_count(void);
extern int	vidapi_available(int api);
extern int	vidapi_from_internal_name(const char *name);
extern const char *vidapi_internal_name(int api);
extern int	vidapi_set(int api);
extern void	vidapi_resize(int x, int y);
extern int	vidapi_pause(void);
extern void	vidapi_reset(void);
extern void	vidapi_screenshot(void);
extern void	plat_startblit(void);
extern void	plat_endblit(void);

/* Floppy image creation. */
extern int	floppy_create_86f(const wchar_t *, int8_t sz, int8_t rpm_mode);
extern int	floppy_create_image(const wchar_t *, int8_t sz, int8_t is_zip, int8_t is_fdi);
extern int	zip_create_image(const wchar_t *, int8_t sz, int8_t is_zdi);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_UI_H*/
