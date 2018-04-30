/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Platform-independent resource identifiers.
 *
 * Version:	@(#)ui_resource.h	1.0.4	2018/04/30
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
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
#ifndef EMU_UI_RESOURCE_H
# define EMU_UI_RESOURCE_H


#ifdef _WIN32
# define IDM_BASE		40960
#else
# define IDM_BASE		16384
#endif

/* ACTION menu. */
#define IDM_ACTION		IDM_BASE
#define IDM_RESET_HARD		(IDM_ACTION+1)
#define IDM_RESET		(IDM_ACTION+2)
#define IDM_CAE			(IDM_ACTION+3)
#define IDM_CAB			(IDM_ACTION+4)
#define IDM_PAUSE		(IDM_ACTION+5)
#define IDM_ACTION_END		(IDM_PAUSE+1)
#define IDM_EXIT		(IDM_ACTION+99)	/* fixed on WxWidgets */

/* VIEW menu. */
#define IDM_VIEW		(IDM_BASE+100)
#define IDM_RESIZE		(IDM_VIEW+1)
#define IDM_REMEMBER		(IDM_VIEW+2)
#define IDM_RENDER		(IDM_VIEW+10)
# define IDM_RENDER_1		(IDM_RENDER+1)		/* DDraw */
# define IDM_RENDER_2		(IDM_RENDER+2)		/* D3D */
# define IDM_RENDER_3		(IDM_RENDER+3)		/* VNC */
# define IDM_RENDER_4		(IDM_RENDER+4)		/* RDP */
#define IDM_SCALE		(IDM_VIEW+20)
# define IDM_SCALE_1		(IDM_SCALE+1)
# define IDM_SCALE_2		(IDM_SCALE+2)
# define IDM_SCALE_3		(IDM_SCALE+3)
# define IDM_SCALE_4		(IDM_SCALE+4)
#define IDM_FULLSCREEN		(IDM_VIEW+30)
#define IDM_FULL_STRETCH	(IDM_FULLSCREEN+1)
# define IDM_STRETCH		(IDM_FULLSCREEN+2)
# define IDM_STRETCH_43		(IDM_FULLSCREEN+3)
# define IDM_STRETCH_SQ		(IDM_FULLSCREEN+4)
# define IDM_STRETCH_INT	(IDM_FULLSCREEN+5)
# define IDM_STRETCH_KEEP	(IDM_FULLSCREEN+6)
#define IDM_RCTRL_IS_LALT	(IDM_VIEW+5)
#define IDM_UPDATE_ICONS	(IDM_VIEW+6)
#define IDM_VIEW_END		(IDM_UPDATE_ICONS+1)

/* DISPLAY menu. */
#define IDM_DISPLAY		(IDM_BASE+200)
# define IDM_INVERT		(IDM_DISPLAY+1)
# define IDM_OVERSCAN		(IDM_DISPLAY+2)
# define IDM_SCREEN		(IDM_DISPLAY+10)
#  define IDM_SCREEN_RGB	(IDM_SCREEN+1)
#  define IDM_SCREEN_GRAYSCALE	(IDM_SCREEN+2)
#  define IDM_SCREEN_AMBER	(IDM_SCREEN+3)
#  define IDM_SCREEN_GREEN	(IDM_SCREEN+4)
#  define IDM_SCREEN_WHITE	(IDM_SCREEN+5)
#define IDM_GRAYSCALE		(IDM_DISPLAY+20)
# define IDM_GRAY_601		(IDM_GRAYSCALE+1)
# define IDM_GRAY_709		(IDM_GRAYSCALE+2)
# define IDM_GRAY_AVE		(IDM_GRAYSCALE+3)
#define IDM_FORCE_43		(IDM_DISPLAY+3)
#define IDM_CGA_CONTR		(IDM_DISPLAY+4)
#define IDM_DISPLAY_END		(IDM_CGA_CONTR+1)

/* TOOLS menu. */
#define IDM_TOOLS		(IDM_BASE+300)
#define IDM_SETTINGS		(IDM_TOOLS+1)
#define IDM_LOAD		(IDM_TOOLS+2)
#define IDM_SAVE		(IDM_TOOLS+3)
#define IDM_LOGGING		(IDM_TOOLS+10)
#  define IDM_LOG_BEGIN		(IDM_LOGGING+1)
# define IDM_LOG_BUS		(IDM_LOG_BEGIN+0)
# define IDM_LOG_KEYBOARD	(IDM_LOG_BEGIN+1)
# define IDM_LOG_MOUSE		(IDM_LOG_BEGIN+2)
# define IDM_LOG_GAME		(IDM_LOG_BEGIN+3)
# define IDM_LOG_PARALLEL	(IDM_LOG_BEGIN+4)
# define IDM_LOG_SERIAL		(IDM_LOG_BEGIN+5)
# define IDM_LOG_FDC		(IDM_LOG_BEGIN+6)
# define IDM_LOG_FDD		(IDM_LOG_BEGIN+7)
# define IDM_LOG_D86F		(IDM_LOG_BEGIN+8)
# define IDM_LOG_HDC		(IDM_LOG_BEGIN+9)
# define IDM_LOG_HDD		(IDM_LOG_BEGIN+10)
# define IDM_LOG_ZIP		(IDM_LOG_BEGIN+11)
# define IDM_LOG_CDROM		(IDM_LOG_BEGIN+12)
# define IDM_LOG_CDROM_IMAGE	(IDM_LOG_BEGIN+13)
# define IDM_LOG_CDROM_IOCTL	(IDM_LOG_BEGIN+14)
# define IDM_LOG_NETWORK	(IDM_LOG_BEGIN+15)
# define IDM_LOG_NETWORK_DEV	(IDM_LOG_BEGIN+16)
# define IDM_LOG_SOUND_EMU8K	(IDM_LOG_BEGIN+17)
# define IDM_LOG_SOUND_MPU401	(IDM_LOG_BEGIN+18)
# define IDM_LOG_SOUND_DEV	(IDM_LOG_BEGIN+19)
# define IDM_LOG_SCSI_BUS	(IDM_LOG_BEGIN+20)
# define IDM_LOG_SCSI_DISK	(IDM_LOG_BEGIN+21)
# define IDM_LOG_SCSI_DEV	(IDM_LOG_BEGIN+22)
# define IDM_LOG_VOODOO		(IDM_LOG_BEGIN+23)
#  define IDM_LOG_END		(IDM_LOG_BEGIN+24)
# define IDM_LOG_BREAKPOINT	(IDM_LOGGING+99)
#define IDM_STATUS		(IDM_TOOLS+4)
#define IDM_SCREENSHOT		(IDM_TOOLS+5)
#define IDM_TOOLS_END		(IDM_SCREENSHOT+1)

/* HELP menu. */
#define IDM_HELP		(IDM_BASE+400)
#define IDM_ABOUT		(IDM_HELP+1)	/* fixed on WxWidgets */

#define IDM_END			(IDM_HELP+99)

/*
 * Status Bar commands.
 *
 * We need 7 bits for CDROM (2 bits ID and 5 bits for host drive),
 * and 5 bits for Removable Disks (5 bits for ID), so we use an
 * 8bit (256 entries) space for these devices.
 */
#define IDM_SBAR			(IDM_BASE+1024)

#define IDM_FLOPPY_IMAGE_NEW		(IDM_SBAR + 0x0000)
#define IDM_FLOPPY_IMAGE_EXISTING	(IDM_SBAR + 0x0100)
#define IDM_FLOPPY_IMAGE_EXISTING_WP	(IDM_SBAR + 0x0200)
#define IDM_FLOPPY_EXPORT_TO_86F	(IDM_SBAR + 0x0300)
#define IDM_FLOPPY_EJECT		(IDM_SBAR + 0x0400)

#define IDM_CDROM_MUTE			(IDM_SBAR + 0x0800)
#define IDM_CDROM_EMPTY			(IDM_SBAR + 0x0900)
#define IDM_CDROM_RELOAD		(IDM_SBAR + 0x0a00)
#define IDM_CDROM_IMAGE			(IDM_SBAR + 0x0b00)
#define IDM_CDROM_HOST_DRIVE		(IDM_SBAR + 0x0c00)

#define IDM_ZIP_IMAGE_NEW		(IDM_SBAR + 0x1000)
#define IDM_ZIP_IMAGE_EXISTING		(IDM_SBAR + 0x1100)
#define IDM_ZIP_IMAGE_EXISTING_WP	(IDM_SBAR + 0x1200)
#define IDM_ZIP_EJECT			(IDM_SBAR + 0x1300)
#define IDM_ZIP_RELOAD			(IDM_SBAR + 0x1400)

#define IDM_RDISK_EJECT			(IDM_SBAR + 0x1800)
#define IDM_RDISK_RELOAD		(IDM_SBAR + 0x1900)
#define IDM_RDISK_SEND_CHANGE		(IDM_SBAR + 0x1a00)
#define IDM_RDISK_IMAGE			(IDM_SBAR + 0x1b00)
#define IDM_RDISK_IMAGE_WP		(IDM_SBAR + 0x1c00)

#define IDM_SOUND			(IDM_SBAR + 8192)


#endif	/*EMU_UI_RESOURCE_H*/
