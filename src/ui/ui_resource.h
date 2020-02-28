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
 * NOTE:	All entries in the strings table now have an IDS, even if
 *		those are not used by the platform code. This is easier to
 *		maintain.
 *
 * Version:	@(#)ui_resource.h	1.0.21	2019/05/17
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018,2019 Fred N. van Kempen.
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
# define IDM_RENDER_1		(IDM_RENDER+1)
# define IDM_RENDER_2		(IDM_RENDER+2)
# define IDM_RENDER_3		(IDM_RENDER+3)
# define IDM_RENDER_4		(IDM_RENDER+4)
# define IDM_RENDER_5		(IDM_RENDER+5)
# define IDM_RENDER_6		(IDM_RENDER+6)
# define IDM_RENDER_7		(IDM_RENDER+7)
# define IDM_RENDER_8		(IDM_RENDER+8)
#define IDM_SCALE		(IDM_VIEW+20)
# define IDM_SCALE_1		(IDM_SCALE+1)
# define IDM_SCALE_2		(IDM_SCALE+2)
# define IDM_SCALE_3		(IDM_SCALE+3)
# define IDM_SCALE_4		(IDM_SCALE+4)
#define IDM_FULLSCREEN		(IDM_VIEW+30)
#define IDM_FULL_STRETCH	(IDM_FULLSCREEN+1)
# define IDM_STRETCH		(IDM_FULLSCREEN+2)
# define IDM_STRETCH_43		(IDM_FULLSCREEN+3)
# define IDM_STRETCH_KEEP	(IDM_FULLSCREEN+4)
# define IDM_STRETCH_INT	(IDM_FULLSCREEN+5)
#define IDM_RCTRL_IS_LALT	(IDM_VIEW+5)
#define IDM_VIEW_END		(IDM_RCTRL_IS_LALT+1)

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
#define IDM_LANGUAGE		(IDM_TOOLS+10)
#define IDM_LOGGING		(IDM_TOOLS+40)
# define IDM_LOG_BREAKPOINT	(IDM_LOGGING+1)
# define IDM_LOG_BEGIN		(IDM_LOGGING+2)
# define  IDM_LOG_BUS		(IDM_LOG_BEGIN+0)
# define  IDM_LOG_KEYBOARD	(IDM_LOG_BEGIN+1)
# define  IDM_LOG_MOUSE		(IDM_LOG_BEGIN+2)
# define  IDM_LOG_GAME		(IDM_LOG_BEGIN+3)
# define  IDM_LOG_PARALLEL	(IDM_LOG_BEGIN+4)
# define  IDM_LOG_SERIAL	(IDM_LOG_BEGIN+5)
# define  IDM_LOG_FDC		(IDM_LOG_BEGIN+6)
# define  IDM_LOG_FDD		(IDM_LOG_BEGIN+7)
# define  IDM_LOG_D86F		(IDM_LOG_BEGIN+8)
# define  IDM_LOG_HDC		(IDM_LOG_BEGIN+9)
# define  IDM_LOG_HDD		(IDM_LOG_BEGIN+10)
# define  IDM_LOG_ZIP		(IDM_LOG_BEGIN+11)
# define  IDM_LOG_CDROM		(IDM_LOG_BEGIN+12)
# define  IDM_LOG_CDROM_IMAGE	(IDM_LOG_BEGIN+13)
# define  IDM_LOG_CDROM_HOST	(IDM_LOG_BEGIN+14)
# define  IDM_LOG_NETWORK	(IDM_LOG_BEGIN+15)
# define  IDM_LOG_NETWORK_DEV	(IDM_LOG_BEGIN+16)
# define  IDM_LOG_SOUND		(IDM_LOG_BEGIN+17)
# define  IDM_LOG_SOUND_MIDI	(IDM_LOG_BEGIN+18)
# define  IDM_LOG_SOUND_DEV	(IDM_LOG_BEGIN+19)
# define  IDM_LOG_SCSI		(IDM_LOG_BEGIN+20)
# define  IDM_LOG_SCSI_DEV	(IDM_LOG_BEGIN+21)
# define  IDM_LOG_SCSI_CDROM	(IDM_LOG_BEGIN+22)
# define  IDM_LOG_SCSI_DISK	(IDM_LOG_BEGIN+23)
# define  IDM_LOG_VIDEO		(IDM_LOG_BEGIN+28)
# define  IDM_LOG_VIDEO_DEV	(IDM_LOG_BEGIN+29)
# define IDM_LOG_END		(IDM_LOG_BEGIN+30)
#define IDM_LOAD		(IDM_TOOLS+90)
#define IDM_SAVE		(IDM_TOOLS+91)
#define IDM_SCREENSHOT		(IDM_TOOLS+92)
#define IDM_TOOLS_END		(IDM_SCREENSHOT+1)

/* HELP menu. */
#define IDM_HELP		(IDM_BASE+400)
#define IDM_ABOUT		(IDM_HELP+1)	/* fixed on WxWidgets */
#define IDM_LOCALIZE		(IDM_HELP+2)

#define IDM_END			(IDM_HELP+99)

/*
 * Status Bar commands.
 *
 * We need 7 bits for CDROM (2 bits ID and 5 bits for host drive),
 * and 5 bits for Hard Disks (5 bits for ID), so we use an 8bit
 * (256 entries) space for these devices.
 */
#define IDM_SBAR		(IDM_BASE+1024)

#define IDM_FLOPPY_IMAGE_NEW	(IDM_SBAR + 0x0000)
#define IDM_FLOPPY_IMAGE_EXIST	(IDM_SBAR + 0x0100)
#define IDM_FLOPPY_EXPORT	(IDM_SBAR + 0x0200)
#define IDM_FLOPPY_EJECT	(IDM_SBAR + 0x0300)

#define IDM_CDROM_MUTE		(IDM_SBAR + 0x0800)
#define IDM_CDROM_EMPTY		(IDM_SBAR + 0x0900)
#define IDM_CDROM_RELOAD	(IDM_SBAR + 0x0a00)
#define IDM_CDROM_IMAGE		(IDM_SBAR + 0x0b00)
#define IDM_CDROM_LOCK		(IDM_SBAR + 0x0c00)
#define IDM_CDROM_UNLOCK	(IDM_SBAR + 0x0d00)
#define IDM_CDROM_HOST_DRIVE	(IDM_SBAR + 0x0f00)

#define IDM_ZIP_IMAGE_NEW	(IDM_SBAR + 0x1000)
#define IDM_ZIP_IMAGE_EXIST	(IDM_SBAR + 0x1100)
#define IDM_ZIP_EJECT		(IDM_SBAR + 0x1200)
#define IDM_ZIP_RELOAD		(IDM_SBAR + 0x1300)

#define IDM_DISK_IMAGE_NEW	(IDM_SBAR + 0x1800)
#define IDM_DISK_IMAGE_EXIST	(IDM_SBAR + 0x1900)
#define IDM_DISK_EJECT		(IDM_SBAR + 0x1a00)
#define IDM_DISK_RELOAD		(IDM_SBAR + 0x1b00)
#define IDM_DISK_NOTIFY		(IDM_SBAR + 0x1c00)

#define IDM_SOUND		(IDM_SBAR + 8192)


/* Main icons. */
#define ICON_MAIN		10	/* "varcem.ico" */
#define ICON_DONATE		11	/* "paypal-donate.ico" */

/* Icons for status bar. */
#define ICON_ACTIVE		1	/* add to base icon ID */
#define ICON_EMPTY		128	/* add to base icon ID */
#define ICON_MKREG(x)		(x)
#define ICON_MKEMPTY(x)		(ICON_EMPTY + x)
#define ICON_MKACTIVE(x)	(ICON_ACTIVE + x)
#define ICON_MKIDLE(x)		(ICON_MKEMPTY(x) + 1)

#define ICON_FLOPPY_525		16	/* "floppy_525.ico" */
#define ICON_FLOPPY_525_A	17	/* "floppy_525_active.ico" */
#define ICON_FLOPPY_525_E	144	/* "floppy_525_empty.ico" */
#define ICON_FLOPPY_525_I	145	/* "floppy_525_empty_active.ico" */
#define ICON_FLOPPY_35		24	/* "floppy_35.ico" */
#define ICON_FLOPPY_35_A	25	/* "floppy_35_active.ico" */
#define ICON_FLOPPY_35_E	152	/* "floppy_35_empty.ico" */
#define ICON_FLOPPY_35_I	153	/* "floppy_35_empty_active.ico" */
#define ICON_CDROM		32	/* "cdrom.ico" */
#define ICON_CDROM_A		33	/* "cdrom_active.ico" */
#define ICON_CDROM_E		160	/* "cdrom_empty.ico" */
#define ICON_CDROM_I		161	/* "cdrom_empty_active.ico" */
#define ICON_ZIP		48	/* "zip.ico" */
#define ICON_ZIP_A		49	/* "zip_active.ico" */
#define ICON_ZIP_E		176	/* "zip_empty.ico" */
#define ICON_ZIP_I		177	/* "zip_empty_active.ico" */
#define ICON_DISK		64	/* "hard_disk.ico" */
#define ICON_DISK_A		65	/* "hard_disk_active.ico" */
#define ICON_NETWORK		80	/* "network.ico" */
#define ICON_NETWORK_A		81	/* "network_active.ico" */

/* Icons for Settings UI. */
#define ICON_MACHINE		240	/* "machine.ico" */
#define ICON_DISPLAY		241	/* "display.ico" */
#define ICON_INPDEV		242	/* "input_devices.ico" */
#define ICON_SOUND		243	/* "sound.ico" */
#define ICON_PORTS		244	/* "ports.ico" */
#define ICON_PERIPH		245	/* "other_peripherals.ico" */
#define ICON_FLOPPY		246	/* "floppy_drives.ico" */
#define ICON_REMOV		247	/* "other_removable_devices.ico" */
#define ICON_FLOPPY_D		248	/* "floppy_disabled.ico" */
#define ICON_CDROM_D		249	/* "cdrom_disabled.ico" */
#define ICON_ZIP_D		250	/* "zip_disabled.ico" */


/* String IDs. */
#define IDS_BEGIN	2000		/* start of accessible string IDs */

/* Application main strings (2000.) */
#define IDS_VERSION	2000		/* "1.0.1" */
#define IDS_AUTHOR	2001		/* "Your Name" */
#define IDS_EMAIL	2002		/* "your@email.address" */
#define IDS_NAME	2003		/* "VARCem" */
#define IDS_TITLE	2004		/* "Virtual ARchaeological.." */

/* Messagebox classes (2100.) */
#define IDS_ERROR	2100		/* "Error" */
#define IDS_ERROR_FATAL	2101		/* "Fatal Error" */
#define IDS_ERROR_CONF	2102		/* "Configuration Error" */
#define IDS_WARNING	2103		/* "Warning" */

/* System errors (2200.) */
#define IDS_ERR_ACCEL	2200		/* "Unable to load Accelerators" */
#define IDS_ERR_INPUT	2201		/* "Unable to register Raw Input" */
#define IDS_ERR_NONET	2202		/* "%s failed to set up.." */
#define IDS_ERR_PCAP_NO	2203		/* "No PCap devices found" */
#define IDS_ERR_PCAP_DEV 2204		/* "Invalid PCap device" */
#define IDS_ERR_NOLIB	2205		/* "Unable to initialize %s.." */

/* Application error messages (2300.) */
#define IDS_ERR_NOMEM	2300		/* "System is out of memory!" */
#define IDS_ERR_NOROMS	2301		/* "No usable ROM images found!" */
#define IDS_ERR_NOCONF	2302		/* "No valid configuration.." */
#define IDS_ERR_NOAVAIL	2303		/* "Configured %s not avai.." */
#define IDS_ERR_NORENDR	2304		/* "Selected renderer not avai.." */
#define IDS_ERR_SAVEIT	2305		/* "Must save settings first.." */
#define IDS_ERR_SCRSHOT	2306		/* "Unable to create bitmap file" */
#define IDS_ERR_NO_USB	2307		/* "USB is not yet supported" */
#define IDS_ERR_NOBIOS	IDS_ERR_NOCONF	/* "No BIOS found.." */

/* Application messages (2400.) */
#define IDS_MSG_SAVE	2400		/* "Are you sure you want to save.." */
#define IDS_MSG_RESTART	2401		/* "Changes saved, please restart.." */
#define IDS_MSG_UNSTABL	2402		/* "The requested device '%ls'.." */
#define IDS_MSG_CAPTURE	2403		/* "Click to capture mouse" */
#define IDS_MSG_MRLS_1	2404		/* "Press F12-F8 to release mouse" */
#define IDS_MSG_MRLS_2	2405		/* "Press F12-F8 or middle button.." */
#define IDS_MSG_WINDOW	2406		/* "Use CTRL+ALT+PAGE DOWN.." */

/* Misc application strings (2500.) */
#define IDS_2500	2500		/* "Configuration files (*.varc.." */


/* UI: common elements (3000.) */
#define IDS_OK		3000		/* "OK" */
#define IDS_CANCEL	3001		/* "Cancel" */
#define IDS_YES		3002		/* "Yes" */
#define IDS_NO		3003		/* "No" */
#define IDS_CONFIGURE	3004		/* "Configure" */
#define IDS_BROWSE	3005		/* "Browse" */


/* UI: dialog shared strings (3100.) */
#define IDS_NONE	3100		/* "None" */
#define IDS_INTERNAL	3101		/* "Internal" */
#define IDS_DISABLED	3102		/* "Disabled" */
#define IDS_ENABLED	3103		/* "Enabled" */
#define IDS_OFF		3104		/* "Off" */
#define IDS_ON		3105		/* "On" */
#define IDS_UNLOCK	3106		/* "Unlock" */
#define IDS_LOCK	3107		/* "Lock" */
#define IDS_TYPE	3108		/* "Type" */
#define IDS_FILENAME	3109		/* "File name:" */
#define IDS_PROGRESS	3110		/* "Progress:" */
#define IDS_BUS		3111		/* "Bus:" */
#define IDS_CHANNEL	3112		/* "Channel:" */
#define IDS_ID		3113		/* "ID:" */
#define IDS_LUN		3114		/* "LUN:" */
#define IDS_INV_NAME	3115		/* "Please enter a valid file name" */
#define IDS_IMG_EXIST	3116		/* "This image exists and will be.." */
#define IDS_OPEN_READ	3117		/* "Unable to open for read" */
#define IDS_OPEN_WRITE	3118		/* "Unable to open for write" */
#define IDS_DEVCONF_1	3119		/* "Configuration" */
#define IDS_DEVCONF_2	3120		/* "Device:" */


/* UI: dialog: About (3200.) */
#define IDS_ABOUT	3200		/* "About VARCem.." */
#define  IDS_3201	3201		/* "Authors:" */
#define  IDS_3202	3202		/* "Fred.., Miran.., .." */
#define  IDS_3203	3203		/* "Released under.." */
#define  IDS_3204	3204		/* "See LICENSE.txt.." */
#define IDS_LOCALIZE	3210		/* "Localization" */
#define IDS_3211	3211		/* "Translations provided by.." */

/* UI dialog: Status (3225.) */
/* not used anymore */

/* UI dialog: Sound Gain (3250.) */
#define IDS_SNDGAIN	3250		/* "Sound Gain" */
#define  IDS_3251	3251		/* "Gain" */

/* UI dialog: New Image (3275.) */
#define IDS_NEWIMG	3275		/* "New Floppy Image" */
#define  IDS_3276	3276		/* "Disk size:" */
#define  IDS_3277	3277		/* "RPM mode:" */
#define  IDS_3278	3278		/* "Perfect RPM" */
#define  IDS_3279	3279		/* "1% below perfect RPM" */
#define  IDS_3280	3280		/* "1.5% below perfect RPM" */
#define  IDS_3281	3281		/* "2% below perfect RPM" */
#define  IDS_3282	3282		/* "160 KB" */
#define  IDS_3283	3283		/* "180 KB" */
#define  IDS_3284	3284		/* "320 KB" */
#define  IDS_3285	3285		/* "360 KB" */
#define  IDS_3286	3286		/* "640 KB" */
#define  IDS_3287	3287		/* "720 KB" */
#define  IDS_3288	3288		/* "1.2 MB" */
#define  IDS_3289	3289		/* "1.25 MB" */
#define  IDS_3290	3290		/* "1.44 MB" */
#define  IDS_3291	3291		/* "DMF (cluster 1024)" */
#define  IDS_3292	3292		/* "DMF (cluster 2048)" */
#define  IDS_3293	3293		/* "2.88 MB" */
#define  IDS_3294	3294		/* "ZIP 100" */
#define  IDS_3295	3295		/* "ZIP 250" */

/* UI dialog: Settings (3300.) */
#define IDS_SETTINGS	3300		/* "Settings"*/
#define  IDS_3310	3310		/* "Machine" */
#define  IDS_3311	3311		/* "Display" */
#define  IDS_3312	3312		/* "Input devices" */
#define  IDS_3313	3313		/* "Sound" */
#define  IDS_3314	3314		/* "Network" */
#define  IDS_3315	3315		/* "Ports (COM & LPT)" */
#define  IDS_3316	3316		/* "Other peripherals" */
#define  IDS_3317	3317		/* "Hard disks" */
#define  IDS_3318	3318		/* "Floppy drives" */
#define  IDS_3319	3319		/* "Other removable devices" */

/* UI dialog: Settings (Machine, 3325.) */
#define  IDS_3325	3325		/* "Machine:" */
#define  IDS_3326	3326		/* "CPU type:" */
#define  IDS_3327	3327		/* "CPU:" */
#define  IDS_3328	3328		/* "Wait states:" */
#define  IDS_3329	3329		/* "Memory:" */
#define  IDS_3330	3330		/* "MB" */
#define  IDS_3331	3331		/* "Time sync:" */
#define  IDS_3332	3332		/* "Enable FPU" */
#define  IDS_3333	3333		/* "Dynamic Recompiler" */
#define  IDS_3334	3334		/* "KB" */
#define  IDS_3335	3335		/* "Default" */
#define  IDS_3336	3336		/* "Enabled (UTC)" */

/* UI dialog: Settings (Video, 3350.) */
#define  IDS_3350	3350		/* "Video:" */
#define  IDS_3351	3351		/* "Voodoo Graphics" */

/* UI dialog: Settings (Other Peripherals, 3375.) */
#define  IDS_3375	3375		/* "Mouse:" */
#define  IDS_3376	3376		/* "Joystick:" */
#define  IDS_3377	3377		/* "Joystick 1" */
#define  IDS_3378	3378		/* "Joystick 2" */
#define  IDS_3379	3379		/* "Joystick 3" */
#define  IDS_3380	3380		/* "Joystick 4" */

/* UI dialogs: Settings (Sound, 3400.) */
#define  IDS_3400	3400		/* "Sound card:" */
#define  IDS_3401	3401		/* "MIDI Out Device:" */
#define  IDS_3402	3402		/* "Standalone MPU-401" */
#define  IDS_3403	3403		/* "Use FLOAT32 sound" */

/* UI dialog: Settings (Network, 3425.) */
#define  IDS_3425	3425		/* "Network type:" */
#define  IDS_3426	3426		/* "PCap device:" */
#define  IDS_3427	3427		/* "Network adapter:" */

/* UI dialog: Settings (Ports, 3450.) */
#define  IDS_3450	3450		/* "Game port" */
#define  IDS_3451	3451		/* "Parallel port 1" */
#define  IDS_3452	3452		/* "Parallel port 2" */
#define  IDS_3453	3453		/* "Parallel port 3" */
#define  IDS_3454	3454		/* "Serial port 1" */
#define  IDS_3455	3455		/* "Serial port 2" */

/* UI dialog: Settings (Other Devices, 3475.) */
#define  IDS_3475	3475		/* "SCSI Controller:" */
#define  IDS_3476	3476		/* "HD Controller:" */
#define  IDS_3477	3477		/* "Tertiary IDE" */
#define  IDS_3478	3478		/* "Quaternary IDE" */
#define  IDS_3479	3479		/* "ISABugger Card" */
#define  IDS_3480	3480		/* "ISA Memory Expansion Card" */
#define  IDS_3481	3481		/* "ISA Clock/RTC Card" */
 
/* UI dialog: Settings (Hard Disks, 3500.) */
#define  IDS_3500	3500		/* "Hard disks:" */
#define  IDS_3501	3501		/* "&New.." */
#define  IDS_3502	3502		/* "&Existing.." */
#define  IDS_3503	3503		/* "&Remove" */
#define  IDS_3504	3504		/* "Bus" */
#define  IDS_3505	3505		/* "File" */
#define  IDS_3506	3506		/* "C" */
#define  IDS_3507	3507		/* "H" */
#define  IDS_3508	3508		/* "S" */
#define  IDS_3509	3509		/* "MB" */
#define  IDS_3510	3510		/* "MB (CHS: %".." */
#define  IDS_3511	3511		/* "Custom..." */
#define  IDS_3512	3512		/* "Custom (large)..." */
#define  IDS_3515	3515		/* "ST506" */
#define  IDS_3516	3516		/* "ESDI" */
#define  IDS_3517	3517		/* "IDE" */
#define  IDS_3518	3518		/* "SCSI" */
#define  IDS_3519	3519		/* "USB" */

/* VHD Type*/
#define  IDS_3521	3521		/* "Fixed" */
#define  IDS_3522	3522		/* "Sparse" */
#define  IDS_3523	3523		/* "Differential" */

/* UI dialog: Settings (Add Hard Disk, 3525.) */
#define  IDS_3525	3525		/* "Add Hard Disk" */
#define  IDS_3526	3526		/* "Add New Hard Disk" */
#define  IDS_3527	3527		/* "Add Existing Hard Disk" */
#define  IDS_3528	3528		/* "Cylinders:" */
#define  IDS_3529	3529		/* "Heads:" */
#define  IDS_3530	3530		/* "Sectors:" */
#define  IDS_3531	3531		/* "Size (MB):" */
#define  IDS_3532	3532		/* "Type:" */
#define  IDS_3533	3533		/* "Attempting to create a HDI ima.." */
#define  IDS_3534	3534		/* "Attempting to create a spurio.." */
#define  IDS_3535	3535		/* "HDI or HDX image with a sect.." */
#define  IDS_3536	3536		/* "Hard disk images (*.HDI;*.HD.." */
#define  IDS_3537	3537		/* "Remember to partition and fo.." */
#define  IDS_3538	3538		/* "Hard disk images (*.VHD)" */

/* UI dialog: Settings (Floppy Drives, 3550.) */
#define  IDS_3550	3550		/* "Floppy drives:" */
#define  IDS_3551	3551		/* "Type:" */
#define  IDS_3552	3552		/* "Turbo timings" */
#define  IDS_3553	3553		/* "Check BPB" */
#define  IDS_3554	3554		/* "Turbo" */
#define  IDS_3555	3555		/* "Check BPB" */

/* UI dialog: Settings (Removable Devices, 3575) */
#define  IDS_3575	3575		/* "CD-ROM drives:" */
#define  IDS_3576	3576		/* "Speed:" */
#define  IDS_3577	3577		/* "ZIP drives:" */
#define  IDS_3578	3578		/* "ZIP 250" */
#define  IDS_3579	3579		/* "Speed" */
#define  IDS_3580	3580		/* "ATAPI" */
#define  IDS_3581	3581		/* "SCSI" */
#define  IDS_3582	3582		/* "USB" */


/* UI dialog: Status Bar (3900.) */
#define IDS_3900	3900		/* "(empty)" */
#define IDS_3901	3901		/* "(host drive %c:)" */
#define IDS_3902	3902		/* "[WP]" */
#define IDS_3903	3903		/* "[Locked]" */
#define IDS_3904	3904		/* "&New image..." */
#define IDS_3905	3905		/* "&Load image..." */
#define IDS_3906	3906		/* "&Reload previous image" */
#define IDS_3907	3907		/* "&Unload" */
#define IDS_3908	3908		/* "Notify disk &change" */
#define IDS_3910	3910		/* "Floppy %i (%s): %ls" */
#define  IDS_3911	3911		/* "All floppy images (*.0??;*.." */
#define  IDS_3912	3912		/* "All floppy images (*.dsk..." */
#define  IDS_3913	3913		/* "Surface-based images (*.8.." */
#define  IDS_3914	3914		/* "E&xport to ..." */
#define IDS_3920	3920		/* "CD-ROM %i (%ls): %ls" */
#define  IDS_3921	3921		/* "Host CD/DVD Drive (%c:)" */
#define  IDS_3922	3922		/* "CD-ROM images (*.iso;*.cu.." */
#define  IDS_3923	3923		/* "&Mute" */
#define IDS_3930	3930		/* "Disk %i (%ls): %ls" */
#define IDS_3950	3950		/* "ZIP%03i %i (%ls): %ls" */
#define  IDS_3951	3951		/* "ZIP images (*.im?)\0*.im..." */
#define  IDS_3952	3952		/* "ZIP images (*.im?)\0*.im..." */
#define IDS_3960	3960		/* "Network (%s) */
#define IDS_3970	3970		/* "Sound (%s) */


/* UI menu: Action (4000.) */
#define IDS_ACTION	4000		/* "&Action" */
#define IDS_4001	4001		/* "&Hard Reset" */
#define IDS_4002	4002		/* "&Ctrl+Alt+Del" */
#define IDS_4003	4003		/* "Send Ctrl+Alt+&Esc" */
#define IDS_4004	4004		/* "Send Ctrl+Alt+&Break" */
#define IDS_4005	4005		/* "&Pause" */
#define IDS_4006	4006		/* "E&xit" */

/* UI menu: View (4010.) */
#define IDS_VIEW	4010		/* "&View" */
#define IDS_4011	4011		/* "&Resizeable window" */
#define IDS_4012	4012		/* "R&emember size && position" */
#define IDS_4013	4013		/* "&Fullscreen\tCtrl+Alt+PageUP" */
#define IDS_4014	4014		/* "R&ight CTRL is left ALT" */

/* UI menu: View > Renderer (4020.) */
#define IDS_4020	4020		/* "Re&nderer" */

/* UI menu: View > Window Scale Factor (4030.) */
#define IDS_4030	4030		/* "&Window scale factor" */
#define IDS_4031	4031		/* "&0.5x" */
#define IDS_4032	4032		/* "&1x" */
#define IDS_4033	4033		/* "1.&5x" */
#define IDS_4034	4034		/* "&2x" */

/* UI menu: View > Fullscreen Stretch Mode (4040.) */
#define IDS_4040	4040		/* "Fullscreen &stretch mode" */
#define IDS_4041	4041		/* "&Full screen stretch" */
#define IDS_4042	4042		/* "&4:3" */
/* 4043 available */
#define IDS_4044	4044		/* "&Integer scale" */
#define IDS_4045	4045		/* "&Keep size" */

/* UI menu: Display (4050.) */
#define IDS_DISPLAY	4050		/* "&Display" */
#define IDS_4051	4051		/* "&Inverted display" */
#define IDS_4052	4052		/* "Enable &overscan" */
#define IDS_4053	4053		/* "F&orce 4:3 display ratio" */
#define IDS_4054	4054		/* "Change &contrast.." */

/* UI menu: Display > DisplayType (4060.) */
#define IDS_DISPTYPE	4060		/* "Display &type" */
#define IDS_4061	4061		/* "RGB &Color monitor" */
#define IDS_4062	4062		/* "&Grayscale monitor" */
#define IDS_4063	4063		/* "&Amber monitor" */
#define IDS_4064	4064		/* "&Green monitor" */
#define IDS_4065	4065		/* "&White monitor" */

/* UI menu: Display > GrayscaleType (4070.) */
#define IDS_GRAYSCALE	4070		/* "&Grayscale conversion type" */
#define IDS_4071	4071		/* "BT&601 (NTSC/PAL)" */
#define IDS_4072	4072		/* "BT&709 (HDTV)" */
#define IDS_4073	4073		/* "&Average" */


/* UI menu: Tools (4080.) */
#define IDS_TOOLS	4080		/* "&Tools" */
#define IDS_4081	4081		/* "&Settings" */
#define IDS_4082	4082		/* "&Language" */
#define IDS_4083	4083		/* "L&ogging" */
#define IDS_4084	4084		/* "Log Breakpoint" */
#define IDS_4085	4085		/* "Toggle %s logging" */
#define IDS_4086	4086		/* "Load &configuration" */
#define IDS_4087	4087		/* "S&ave configuration" */
#define IDS_4088	4088		/* "&Take screenshot\tCtrl+Home" */


/* UI menu: Help (4090.) */
#define IDS_HELP	4090		/* "&Help" */
#define IDS_4091	4091		/* "&About VARCem" */


#define IDS_END		4100		/* end of string IDs */


#endif	/*EMU_UI_RESOURCE_H*/
