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
 * Version:	@(#)ui.h	1.0.17	2019/04/26
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
#define SB_DISK         0x10
#define SB_CDROM        0x20
#define SB_ZIP          0x30
#define SB_NETWORK      0x50
#define SB_SOUND        0x60
#define SB_TEXT         0x70

/* MenuItem types. */
#define ITEM_SEPARATOR	0
#define ITEM_NORMAL	1
#define ITEM_CHECK	2
#define ITEM_RADIO	3


#ifdef USE_WX
# define RENDER_FPS	30			/* default render speed */
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Define an in-memory language entry. */
typedef struct _lang_ {
    const wchar_t	*name;
    const wchar_t	*dll;

    const wchar_t	*author;
    const wchar_t	*email;
    const wchar_t	*version;

    int			id;
    struct _lang_	*next;
} lang_t;

/* Define an entry in the strings table. */
typedef struct {
    int                 id;
    const wchar_t       *str;
} string_t;

typedef struct {
    uint16_t	width;
    uint8_t	flags;
    uint8_t	tag;
    uint8_t	icon;
    wchar_t	*tip;
    wchar_t	*text;
} sbpart_t;


/* Main GUI functions. */
extern void	ui_reset(void);
extern int	ui_menu_command(int idm);
extern int	ui_msgbox(int type, const void *arg);
extern void	ui_fullscreen(int on);
extern void	ui_mouse_capture(int on);

extern int	ui_lang_set(int id);
extern lang_t	*ui_lang_get(void);
extern void	ui_lang_add(lang_t *ptr, int sort);
extern void	ui_lang_menu(void);

extern void	ui_floppy_mount(uint8_t drive, int part, int8_t wp,
				const wchar_t *fn);
extern void	ui_cdrom_eject(uint8_t id);
extern void	ui_cdrom_reload(uint8_t id);
extern void	ui_zip_mount(uint8_t drive, int part, int8_t wp,
			     const wchar_t *fn);
extern void	ui_zip_eject(uint8_t id);
extern void	ui_zip_reload(uint8_t id);
extern void	ui_disk_mount(uint8_t drive, int part, int8_t wp,
			      const wchar_t *fn);
extern void	ui_disk_unload(uint8_t id);
extern void	ui_disk_eject(uint8_t id);
extern void	ui_disk_reload(uint8_t id);

/* Main GUI (platform) helper functions. */
extern void	ui_plat_reset(void);
extern void	ui_resize(int x, int y);
extern void	ui_show_cursor(int on);
extern void	ui_show_render(int on);
extern wchar_t  *ui_window_title(const wchar_t *s);
extern int	ui_fdd_icon(int type);
extern void	ui_set_kbd_state(int flags);
extern void	menu_add_item(int idm, int type, int id, const wchar_t *str);
extern void	menu_enable_item(int idm, int val);
extern void	menu_set_item(int idm, int val);
extern void	menu_set_radio_item(int idm, int num, int val);

/* Status Bar functions. */
extern void	ui_sb_reset(void);
extern void	ui_sb_click(int part);
extern void	ui_sb_kbstate(int flags);
extern void	ui_sb_icon_update(uint8_t tag, int val);
extern void	ui_sb_icon_state(uint8_t tag, int active);
extern void	ui_sb_tip_update(uint8_t tag);
extern void	ui_sb_text_set_w(uint8_t tag, const wchar_t *str);
extern void	ui_sb_text_set(uint8_t tag, const char *str);
extern void	ui_sb_menu_command(int idm, uint8_t tag);
extern void	ui_sb_menu_enable_item(uint8_t tag, int id, int val);
extern void	ui_sb_menu_set_item(uint8_t tag, int id, int val);

/* Status Bar (platform) helper functions. */
extern void	sb_setup(int parts, const sbpart_t *data);
extern void	sb_set_icon(int part, int icon);
extern void	sb_set_text(int part, const wchar_t *str);
extern void	sb_set_tooltip(int part, const wchar_t *str);
extern void	sb_menu_create(int part);
extern void	sb_menu_add_item(int part, int idm, const wchar_t *str);
extern void	sb_menu_enable_item(int part, int idm, int val);;
extern void	sb_menu_set_item(int part, int idm, int val);

/* Dialogs. */
extern void	dlg_about(void);
extern void	dlg_localize(void);
extern int	dlg_settings(int ask);
extern void     dlg_new_image(int drive, int part, int is_zip);
extern void	dlg_sound_gain(void);
extern int      dlg_file(const wchar_t *filt, const wchar_t *ifn,
			 wchar_t *fn, int save);

/* Platform VidApi. */
extern int	vidapi_count(void);
extern int	vidapi_available(int api);
extern int	vidapi_from_internal_name(const char *name);
extern const char *vidapi_get_internal_name(int api);
extern const char *vidapi_getname(int api);
extern int	vidapi_set(int api);
extern void	vidapi_resize(int x, int y);
extern int	vidapi_pause(void);
extern void	vidapi_reset(void);
extern void	vidapi_screenshot(void);
extern void	plat_startblit(void);
extern void	plat_endblit(void);

/* Floppy image creation. */
extern int	floppy_create_86f(const wchar_t *, int8_t sz, int8_t rpm_mode);
extern int	floppy_create_image(const wchar_t *, int8_t sz, int8_t is_fdi);
extern int	zip_create_image(const wchar_t *, int8_t sz, int8_t is_zdi);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_UI_H*/
