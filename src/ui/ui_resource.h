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
 * NOTE:	Many entries in the strings table do not have an IDS, as
 *		they are not referenced outside of the platform UI. This
 *		may change at some point.
 *
 * Version:	@(#)ui_resource.h	1.0.16	2018/09/21
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
#define IDM_LANGUAGE		(IDM_TOOLS+10)
#define IDM_LOAD		(IDM_TOOLS+2)
#define IDM_SAVE		(IDM_TOOLS+3)
#define IDM_LOGGING		(IDM_TOOLS+50)
# define IDM_LOG_BREAKPOINT	(IDM_LOGGING+49)
# define IDM_LOG_BEGIN		(IDM_LOGGING+1)
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
# define  IDM_LOG_CDROM_IOCTL	(IDM_LOG_BEGIN+14)
# define  IDM_LOG_NETWORK	(IDM_LOG_BEGIN+15)
# define  IDM_LOG_NETWORK_DEV	(IDM_LOG_BEGIN+16)
# define  IDM_LOG_SOUND_EMU8K	(IDM_LOG_BEGIN+17)
# define  IDM_LOG_SOUND_MPU401	(IDM_LOG_BEGIN+18)
# define  IDM_LOG_SOUND_DEV	(IDM_LOG_BEGIN+19)
# define  IDM_LOG_SCSI_BUS	(IDM_LOG_BEGIN+20)
# define  IDM_LOG_SCSI_DISK	(IDM_LOG_BEGIN+21)
# define  IDM_LOG_SCSI_DEV	(IDM_LOG_BEGIN+22)
# define  IDM_LOG_VOODOO	(IDM_LOG_BEGIN+23)
# define IDM_LOG_END		(IDM_LOG_BEGIN+24)
#define IDM_STATUS		(IDM_TOOLS+4)
#define IDM_SCREENSHOT		(IDM_TOOLS+5)
#define IDM_TOOLS_END		(IDM_LOG_BREAKPOINT+1)

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
#define IDM_FLOPPY_EXPORT_TO_86F	(IDM_SBAR + 0x0200)
#define IDM_FLOPPY_EJECT		(IDM_SBAR + 0x0300)

#define IDM_CDROM_MUTE			(IDM_SBAR + 0x0800)
#define IDM_CDROM_EMPTY			(IDM_SBAR + 0x0900)
#define IDM_CDROM_RELOAD		(IDM_SBAR + 0x0a00)
#define IDM_CDROM_IMAGE			(IDM_SBAR + 0x0b00)
#define IDM_CDROM_HOST_DRIVE		(IDM_SBAR + 0x0c00)

#define IDM_ZIP_IMAGE_NEW		(IDM_SBAR + 0x1000)
#define IDM_ZIP_IMAGE_EXISTING		(IDM_SBAR + 0x1100)
#define IDM_ZIP_EJECT			(IDM_SBAR + 0x1200)
#define IDM_ZIP_RELOAD			(IDM_SBAR + 0x1300)

#define IDM_RDISK_EJECT			(IDM_SBAR + 0x1800)
#define IDM_RDISK_RELOAD		(IDM_SBAR + 0x1900)
#define IDM_RDISK_SEND_CHANGE		(IDM_SBAR + 0x1a00)
#define IDM_RDISK_IMAGE			(IDM_SBAR + 0x1b00)

#define IDM_SOUND			(IDM_SBAR + 8192)


/* String IDs. */
#define IDS_BEGIN	2000		// start of accesible string IDs

/* Unused block (2000.) */

/* Messagebox classes (2100.) */
#define IDS_ERROR	2100		// "Error"
#define IDS_FATAL_ERROR	2101		// "Fatal Error"
#define IDS_CONFIG_ERROR 2102		// "Configuration Error"
#define IDS_WARNING	2103		// "Warning"

/* System errors (2200.) */
#define IDS_ERR_ACCEL	2200		// "Unable to load Accelerators"
#define IDS_ERR_INPUT	2201		// "Unable to register Raw Input"
#define IDS_ERR_PCAP	2202		// "PCap failed to set up.."
#define IDS_ERR_PCAP_NO	2203		// "No PCap devices found"
#define IDS_ERR_PCAP_DEV 2204		// "Invalid PCap device"
#define IDS_ERR_OPENAL	2205		// "Unable to initialize OpenAL.."
#define IDS_ERR_FSYNTH	2206		// "Unable to initialize Flui.."

/* Application error messages (2300.) */
#define IDS_ERR_SCRSHOT	2300		// "Unable to create bitmap file: %s"
#define IDS_ERR_NOROMS	2301		// "No usable ROM images found!"
#define IDS_ERR_NOCONF	2302		// "No valid configuration.."
#define IDS_ERR_NOMACH	2303		// "Configured machine not avai.."
#define IDS_ERR_NOVIDEO	2304		// "Configured video card not.."
#define IDS_ERR_NORENDR	2305		// "Selected renderer not avai.."
#define IDS_ERR_NOCDROM	2306		// "ST506/ESDI CDROM drives.."
#define IDS_ERR_NO_USB	2307		// "USB is not yet supported"
#define IDS_ERR_SAVEIT	2308		// "Must save new config first.."

/* Application messages (2400.) */
#define IDS_MSG_SAVE	2400		// "Are you sure you want to save.."
#define IDS_MSG_RESTART	2401		// "Changes saved, please restart.."
#define IDS_MSG_UNSTABL	2402		// "The requested device '%ls'.."
#define IDS_MSG_CAPTURE	2403		// "Click to capture mouse"
#define IDS_MSG_MRLS_1	2404		// "Press F12-F8 to release mouse"
#define IDS_MSG_MRLS_2	2405		// "Press F12-F8 or middle button.."
#define IDS_MSG_WINDOW	2406		// "Use CTRL+ALT+PAGE DOWN.."

/* Misc application strings (2500.) */
#define IDS_2500	2500		// "Configuration files (*.CF.."


/* UI: common elements (3000.) */
#define IDS_OK		3000		// "OK"
#define IDS_CANCEL	3001		// "Cancel"
#define IDS_CONFIGURE	3002		// "Configure"
#define IDS_BROWSE	3003		// "Browse"


/* UI: dialog shared strings (3100.) */
#define IDS_NONE	3100		// "None"
#define IDS_DISABLED	3101		// "Disabled"
#define IDS_ENABLED	3102		// "Enabled"
#define IDS_OFF		3103		// "Off"
#define IDS_ON		3104		// "On"
#define IDS_TYPE	3105		// "Type"
#define IDS_FILENAME	3106		// "File name:"
#define IDS_PROGRESS	3107		// "Progress:"
#define IDS_BUS		3108		// "Bus:"
#define IDS_CHANNEL	3109		// "Channel:"
#define IDS_ID		3110		// "ID:"
#define IDS_LUN		3111		// "LUN:"
#define IDS_INV_NAME	3112		// "Please enter a valid file name"
#define IDS_IMG_EXIST	3113		// "This image exists and will be.."
#define IDS_OPEN_READ	3114		// "Unable to open the file for read"
#define IDS_OPEN_WRITE	3115		// "Unable to open the file for write"
#define IDS_DEVCONF_1	3116		// "Configuration"
#define IDS_DEVCONF_2	3117		// "Device:"


/* UI: dialog: About (3200.) */

/* UI dialog: Status (3225.) */

/* UI dialog: Sound Gain (3250.) */

/* UI dialog: New Image (3275.) */
#define IDS_3278	3278		// "Perfect RPM"
#define IDS_3279	3279		// "1% below perfect RPM"
#define IDS_3280	3280		// "1.5% below perfect RPM"
#define IDS_3281	3281		// "2% below perfect RPM"
#define IDS_3282	3282		// "160 KB"
#define IDS_3283	3283		// "180 KB"
#define IDS_3284	3284		// "320 KB"
#define IDS_3285	3285		// "360 KB"
#define IDS_3286	3286		// "640 KB"
#define IDS_3287	3287		// "720 KB"
#define IDS_3288	3288		// "1.2 MB"
#define IDS_3289	3289		// "1.25 MB"
#define IDS_3290	3290		// "1.44 MB"
#define IDS_3291	3291		// "DMF (cluster 1024)"
#define IDS_3292	3292		// "DMF (cluster 2048)"
#define IDS_3293	3293		// "2.88 MB"
#define IDS_3294	3294		// "ZIP 100"
#define IDS_3295	3295		// "ZIP 250"

/* UI dialog: Settings (3300.) */
#define IDS_3310	3310		// "Machine"
#define IDS_3311	3311		// "Display"
#define IDS_3312	3312		// "Input devices"
#define IDS_3313	3313		// "Sound"
#define IDS_3314	3314		// "Network"
#define IDS_3315	3315		// "Ports (COM & LPT)"
#define IDS_3316	3316		// "Other peripherals"
#define IDS_3317	3317		// "Hard disks"
#define IDS_3318	3318		// "Floppy drives"
#define IDS_3319	3319		// "Other removable devices"

/* UI dialog: Settings (Machine, 3325.) */
#define IDS_3330	3330		// "MB"
#define IDS_3334	3334		// "KB"
#define IDS_3335	3335		// "Default"

/* UI dialog: Settings (Video, 3350.) */
#define IDS_3353	3353		// "Default"
#define IDS_3354	3354		// "8-bit"
#define IDS_3355	3355		// "Slow 16-bit"
#define IDS_3356	3356		// "Fast 16-bit"
#define IDS_3357	3357		// "Slow VLB/PCI"
#define IDS_3358	3358		// "Mid  VLB/PCI"
#define IDS_3359	3359		// "Fast VLB/PCI"

/* UI dialog: Settings (Other Peripherals, 3475.) */

/* UI dialog: Settings (Hard Disks, 3500.) */
#define IDS_3504	3504		// "Bus"
#define IDS_3505	3505		// "File"
#define IDS_3506	3506		// "C"
#define IDS_3507	3507		// "H"
#define IDS_3508	3508		// "S"
#define IDS_3509	3509		// "MB"
#define IDS_3510	3510		// "MB (CHS: %".."
#define IDS_3511	3511		// "Custom..."
#define IDS_3512	3512		// "Custom (large)..."
#define IDS_3515	3515		// "ST506"
#define IDS_3516	3516		// "ESDI"
#define IDS_3517	3517		// "IDE (PIO-only)"
#define IDS_3518	3518		// "IDE (PIO+DMA)"
#define IDS_3519	3519		// "SCSI"
#define IDS_3520	3520		// "SCSI (removable)"
#define IDS_3526	3526		// "Add New Hard Disk"
#define IDS_3527	3527		// "Add Existing Hard Disk"
#define IDS_3533	3533		// "Attempting to create a HDI ima.."
#define IDS_3534	3534		// "Attempting to create a spurio.."
#define IDS_3535	3535		// "HDI or HDX image with a sect.."
#define IDS_3536	3536		// "Hard disk images (*.HDI;*.HD.."
#define IDS_3537	3537		// "Remember to partition and fo.."

/* UI dialog: Settings (Floppy Drives, 3550.) */
#define IDS_3554	3554		// "Turbo"
#define IDS_3555	3555		// "Check BPB"

/* UI dialog: Settings (Removable Devices, 3575) */
#define IDS_3579	3579		// "Speed"
#define IDS_3580	3580		// "<Reserved>"
#define IDS_3581	3581		// "<Reserved>"
#define IDS_3582	3582		// "ATAPI (PIO-only)"
#define IDS_3583	3583		// "ATAPI (PIO and DMA)"
#define IDS_3584	3584		// "SCSI"

/* UI dialog: Status Bar (3900.) */
#define IDS_3900	3900		// "(empty)"
#define IDS_3901	3901		// "(host drive %c:)"
#define IDS_3902	3902		// "[WP]"
#define IDS_3903	3903		// "&New image..."
#define IDS_3904	3904		// "&Load image..."
#define IDS_3905	3905		// "&Reload previous image"
#define IDS_3906	3906		// "&Unload"
#define IDS_3910	3910		// "Floppy %i (%s): %ls"
#define  IDS_3911	3911		// "All floppy images (*.0??;*.."
#define  IDS_3912	3912		// "All floppy images (*.DSK..."
#define  IDS_3913	3913		// "Surface-based images (*.8.."
#define  IDS_3914	3914		// "E&xport to 86F..."
#define IDS_3920	3920		// "CD-ROM %i (%s): %s"
#define  IDS_3921	3921		// "Host CD/DVD Drive (%c:)"
#define  IDS_3922	3922		// "CD-ROM images (*.ISO;*.CU.."
#define  IDS_3923	3923		// "&Mute"
#define IDS_3930	3930		// "Hard disk (%s)"
#define IDS_3940	3940		// "Removable disk %i: %ls"
#define  IDS_3941	3941		// "&Notify disk change"
#define IDS_3950	3950		// "ZIP %i (%03i): %ls"
#define  IDS_3951	3951		// "ZIP images (*.IM?)\0*.IM..."
#define  IDS_3952	3952		// "ZIP images (*.IM?)\0*.IM..."
#define IDS_3960	3960		// "Network (%s)
#define IDS_3970	3970		// "Sound (%s)


#define IDS_END		4000		// end of accesible string IDs


#endif	/*EMU_UI_RESOURCE_H*/
