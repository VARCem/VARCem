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
 * Version:	@(#)ui.h	1.0.2	2018/03/01
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


#ifdef __cplusplus
extern "C" {
#endif


#ifdef USE_WX
# define RENDER_FPS	30			/* default render speed */
#endif

/* Message Box functions. */
#define MBX_INFO	1
#define MBX_ERROR	2
#define MBX_QUESTION	3
#define MBX_CONFIG	4
#define MBX_FATAL	0x20
#define MBX_ANSI	0x80

extern int	ui_msgbox(int type, void *arg);

extern void	ui_check_menu_item(int id, int checked);

/* Status Bar functions. */
#define SB_ICON_WIDTH	24
#define SB_FLOPPY       0x00
#define SB_CDROM        0x10
#define SB_ZIP          0x20
#define SB_RDISK        0x30
#define SB_HDD          0x50
#define SB_NETWORK      0x60
#define SB_SOUND        0x70
#define SB_TEXT         0x80

extern wchar_t  *ui_window_title(wchar_t *s);
extern void	ui_status_update(void);
extern int	ui_sb_find_part(int tag);
extern void	ui_sb_update_panes(void);
extern void	ui_sb_update_tip(int meaning);
extern void	ui_sb_check_menu_item(int tag, int id, int chk);
extern void	ui_sb_enable_menu_item(int tag, int id, int val);
extern void	ui_sb_update_icon(int tag, int val);
extern void	ui_sb_update_icon_state(int tag, int active);
extern void	ui_sb_set_text_w(wchar_t *wstr);
extern void	ui_sb_set_text(char *str);
extern void	ui_sb_bugui(char *str);
extern void	ui_sb_mount_floppy_img(uint8_t id, int part, uint8_t wp, wchar_t *file_name);
extern void	ui_sb_mount_zip_img(uint8_t id, int part, uint8_t wp, wchar_t *file_name);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_UI_H*/
