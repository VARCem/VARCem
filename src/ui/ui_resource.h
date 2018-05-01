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
 * Version:	@(#)ui_resource.h	1.0.5	2018/04/30
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


/* String IDs. */
#define IDS_2048	2048		// "Error"
#define IDS_2049	2049		// "Fatal Error"
#define IDS_2050	2050		// "Configuration Error"
#define IDS_2051	2051		// "Warning"
#define IDS_2052	2052		// "This will reset the emulator.."
#define IDS_2053	2053		// "Invalid number of sectors.."
#define IDS_2054	2054		// "Invalid number of heads.."
#define IDS_2055	2055		// "Invalid number of cylinders.."
#define IDS_2056	2056		// "No usable ROM images found!"
#define IDS_2057	2057		// "(empty)"
#define IDS_2058	2058		// "(host drive %c:)"
#define IDS_2059	2059		// "(Turbo)"
#define IDS_2060	2060		// "On"
#define IDS_2061	2061		// "Off"
#define IDS_2062	2062		// "Changes saved, please restart.."
#define IDS_2063	2063		// "Configured ROM set not avai.."
#define IDS_2064	2064		// "Configured video BIOS not.."
#define IDS_2065	2065		// "Machine"
#define IDS_2066	2066		// "Display"
#define IDS_2067	2067		// "Input devices"
#define IDS_2068	2068		// "Sound"
#define IDS_2069	2069		// "Network"
#define IDS_2070	2070		// "Ports (COM & LPT)"
#define IDS_2071	2071		// "Other peripherals"
#define IDS_2072	2072		// "Hard disks"
#define IDS_2073	2073		// "Floppy drives"
#define IDS_2074	2074		// "Other removable devices"
#define IDS_2075	2075		// "CD-ROM images (*.ISO;*.CU.."
#define IDS_2076	2076		// "Host CD/DVD Drive (%c:)"
#define IDS_2077	2077		// "Click to capture mouse"
#define IDS_2078	2078		// "Press F12-F8 to release mouse"
#define IDS_2079	2079		// "Press F12-F8 or middle button.."
#define IDS_2080	2080		// "Drive"
#define IDS_2081	2081		// "Location"
#define IDS_2082	2082		// "Bus"
#define IDS_2083	2083		// "File"
#define IDS_2084	2084		// "C"
#define IDS_2085	2085		// "H"
#define IDS_2086	2086		// "S"
#define IDS_2087	2087		// "MB"
#define IDS_2088	2088		// "Unable to create bitmap file: %s"
#define IDS_2089	2089		// "Enabled"
#define IDS_2090	2090		// "Mute"
#define IDS_2091	2091		// "Type"
#define IDS_2092	2092		// "Bus"
#define IDS_2093	2093		// "DMA"
#define IDS_2094	2094		// "KB"
#define IDS_2095	2095		// "Neither DirectDraw nor Dire.."
#define IDS_2096	2096		// "Slave"
#define IDS_2097	2097		// "SCSI (ID %s, LUN %s)"
#define IDS_2098	2098		// "Adapter Type"
#define IDS_2099	2099		// "Base Address"
#define IDS_2100	2100		// "IRQ"
#define IDS_2101	2101		// "8-bit DMA"
#define IDS_2102	2102		// "16-bit DMA"
#define IDS_2103	2103		// "BIOS"
#define IDS_2104	2104		// "Network Type"
#define IDS_2105	2105		// "Surround Module"
#define IDS_2106	2106		// "MPU-401 Base Address"
#define IDS_2107	2107		// "Use CTRL+ALT+PAGE DOWN.."
#define IDS_2108	2108		// "On-board RAM"
#define IDS_2109	2109		// "Memory Size"
#define IDS_2110	2110		// "Display Type"
#define IDS_2111	2111		// "RGB"
#define IDS_2112	2112		// "Composite"
#define IDS_2113	2113		// "Composite Type"
#define IDS_2114	2114		// "Old"
#define IDS_2115	2115		// "New"
#define IDS_2116	2116		// "RGB Type"
#define IDS_2117	2117		// "Color"
#define IDS_2118	2118		// "Monochrome (Green)"
#define IDS_2119	2119		// "Monochrome (Amber)"
#define IDS_2120	2120		// "Monochrome (Gray)"
#define IDS_2121	2121		// "Color (no brown)"
#define IDS_2122	2122		// "Monochrome (Default)"
#define IDS_2123	2123		// "Snow Emulation"
#define IDS_2124	2124		// "Bilinear Filtering"
#define IDS_2125	2125		// "Dithering"
#define IDS_2126	2126		// "Framebuffer Memory Size"
#define IDS_2127	2127		// "Texture Memory Size"
#define IDS_2128	2128		// "Screen Filter"
#define IDS_2129	2129		// "Render Threads"
#define IDS_2130	2130		// "Recompiler"
#define IDS_2131	2131		// "System Default"
#define IDS_2132	2132		// "%i Wait state(s)"
#define IDS_2133	2133		// "8-bit"
#define IDS_2134	2134		// "Slow 16-bit"
#define IDS_2135	2135		// "Fast 16-bit"
#define IDS_2136	2136		// "Slow VLB/PCI"
#define IDS_2137	2137		// "Mid  VLB/PCI"
#define IDS_2138	2138		// "Fast VLB/PCI"
#define IDS_2139	2139		// "PCap failed to set up.."
#define IDS_2140	2140		// "No PCap devices found"
#define IDS_2141	2141		// "Invalid PCap device"
#define IDS_2142	2142		// "&Notify disk change"
#define IDS_2143	2143		// "Type"
#define IDS_2144	2144		// "The requested device '%ls'.."
/* IDS_2145-51 available */
#define IDS_2152	2152		// "None"
#define IDS_2153	2153		// "Unable to load Accelerators"
#define IDS_2154	2154		// "Unable to register Raw Input"
#define IDS_2155	2155		// "IRQ %i"
#define IDS_2156	2156		// "%" PRIu64
#define IDS_2157	2157		// "%" PRIu64 " MB (CHS: %".."
#define IDS_2158	2158		// "Floppy %i (%s): %ls"
#define IDS_2159	2159		// "All floppy images (*.0??;*.."
#define IDS_2160	2160		// "Configuration files (*.CF.."
#define IDS_2161	2161		// "&New image..."
#define IDS_2162	2162		// "&Existing image..."
#define IDS_2163	2163		// "Existing image (&Write-pr..."
#define IDS_2164	2164		// "E&ject"
#define IDS_2165	2165		// "&Mute"
#define IDS_2166	2166		// "E&mpty"
#define IDS_2167	2167		// "&Reload previous image"
#define IDS_2168	2168		// "&Image..."
#define IDS_2169	2169		// "Image (&Write-protected)..."
#define IDS_2170	2170		// "Check BPB"
#define IDS_2171	2171		// "Unable to initialize Flui.."
#define IDS_2172	2172		// "E&xport to 86F..."
#define IDS_2173	2173		// "Surface-based images (*.8.."
#define IDS_2174	2174		// "All floppy images (*.DSK..."
#define IDS_2175	2175		// "ZIP images (*.IM?)\0*.IM..."
#define IDS_2176	2176		// "ZIP images (*.IM?)\0*.IM..."
#define IDS_2177	2177		// "ZIP %i (%03i): %ls"
#define IDS_2178	2178		// "Unable to initialize OpenAL.."
#define IDS_2179	2179		// "Speed"

#define IDS_4096	4096		// "Hard disk (%s)"
#define IDS_4097	4097		// "%01i:%01i"
#define IDS_4098	4098		// "%i"
#define IDS_4099	4099		// "Disabled"
#define IDS_4100	4100		// "Custom..."
#define IDS_4101	4101		// "Custom (large)..."
#define IDS_4102	4102		// "Add New Hard Disk"
#define IDS_4103	4103		// "Add Existing Hard Disk"
#define IDS_4104	4104		// "Attempting to create a HDI ima.."
#define IDS_4105	4105		// "Attempting to create a spurio.."
#define IDS_4106	4106		// "Hard disk images (*.HDI;*.HD.."
#define IDS_4107	4107		// "Unable to open the file for read"
#define IDS_4108	4108		// "Unable to open the file for write"
#define IDS_4109	4109		// "HDI or HDX image with a sect.."
#define IDS_4110	4110		// "USB is not yet supported"
#define IDS_4111	4111		// "This image exists and will be.."
#define IDS_4112	4112		// "Please enter a valid file name"
#define IDS_4113	4113		// "Remember to partition and fo.."
#define IDS_4114	4114		// "ST506 or ESDI CD-ROM driv.."
#define IDS_4115	4115		// "Removable disk %i (SCSI): %ls"

#define IDS_4352	4352		// "ST506"
#define IDS_4353	4353		// "ESDI"
#define IDS_4354	4354		// "IDE (PIO-only)"
#define IDS_4355	4355		// "IDE (PIO+DMA)"
#define IDS_4356	4356		// "SCSI"
#define IDS_4357	4357		// "SCSI (removable)"

#define IDS_4608	4608		// "ST506 (%01i:%01i)"
#define IDS_4609	4609		// "ESDI (%01i:%01i)"
#define IDS_4610	4610		// "IDE (PIO-only) (%01i:%01i)"
#define IDS_4611	4611		// "IDE (PIO+DMA) (%01i:%01i)"
#define IDS_4612	4612		// "SCSI (%02i:%02i)"
#define IDS_4613	4613		// "SCSI (removable) (%02i:%02i)"

#define IDS_5120	5120		// "CD-ROM %i (%s): %s"

#define IDS_5376	5376		// "Disabled"
#define IDS_5377	5377		// "<Reserved>"
#define IDS_5378	5378		// "<Reserved>"
#define IDS_5379	5379		// "ATAPI (PIO-only)"
#define IDS_5380	5380		// "ATAPI (PIO and DMA)"
#define IDS_5381	5381		// "SCSI"

#define IDS_5632	5632		// "Disabled"
#define IDS_5633	5633		// "<Reserved>"
#define IDS_5634	5634		// "<Reserved>"
#define IDS_5635	5635		// "ATAPI (PIO-only) (%01i:%01i)"
#define IDS_5636	5636		// "ATAPI (PIO and DMA) (%01i:%01i)"
#define IDS_5637	5637		// "SCSI (%02i:%02i)"

#define IDS_5888	5888		// "160 kB"
#define IDS_5889	5889		// "180 kB"
#define IDS_5890	5890		// "320 kB"
#define IDS_5891	5891		// "360 kB"
#define IDS_5892	5892		// "640 kB"
#define IDS_5893	5893		// "720 kB"
#define IDS_5894	5894		// "1.2 MB"
#define IDS_5895	5895		// "1.25 MB"
#define IDS_5896	5896		// "1.44 MB"
#define IDS_5897	5897		// "DMF (cluster 1024)"
#define IDS_5898	5898		// "DMF (cluster 2048)"
#define IDS_5899	5899		// "2.88 MB"
#define IDS_5900	5900		// "ZIP 100"
#define IDS_5901	5901		// "ZIP 250"

#define IDS_6144	6144		// "Perfect RPM"
#define IDS_6145	6145		// "1%% below perfect RPM"
#define IDS_6146	6146		// "1.5%% below perfect RPM"
#define IDS_6147	6147		// "2%% below perfect RPM"

#define IDS_7168	7168		// "English (United States)"

#define IDS_LANG_ENUS	IDS_7168

#define STR_NUM_2048	132
#define STR_NUM_3072	11
#define STR_NUM_4096	20
#define STR_NUM_4352	6
#define STR_NUM_4608	6
#define STR_NUM_5120	1
#define STR_NUM_5376	6
#define STR_NUM_5632	6
#define STR_NUM_5888	14
#define STR_NUM_6144	4
#define STR_NUM_7168	1


#endif	/*EMU_UI_RESOURCE_H*/
