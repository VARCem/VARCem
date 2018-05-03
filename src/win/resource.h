/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Windows resource defines.
 *
 * Version:	@(#)resource.h	1.0.14	2018/05/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#ifndef WIN_RESOURCE_H
# define WIN_RESOURCE_H


/* Dialog IDs. */
#define DLG_ABOUT		101	/* top-level dialog */
#define DLG_STATUS		102	/* top-level dialog */
#define DLG_SND_GAIN		103	/* top-level dialog */
#define DLG_NEW_FLOPPY		104	/* top-level dialog */
#define DLG_CONFIG		110	/* top-level dialog */
#define  DLG_CFG_MACHINE	111	/* sub-dialog of config */
#define  DLG_CFG_VIDEO		112	/* sub-dialog of config */
#define  DLG_CFG_INPUT		113	/* sub-dialog of config */
#define  DLG_CFG_SOUND		114	/* sub-dialog of config */
#define  DLG_CFG_NETWORK	115	/* sub-dialog of config */
#define  DLG_CFG_PORTS		116	/* sub-dialog of config */
#define  DLG_CFG_PERIPHERALS	117	/* sub-dialog of config */
#define  DLG_CFG_DISK		118	/* sub-dialog of config */
#define  DLG_CFG_DISK_ADD	119	/* sub-dialog of config */
#define  DLG_CFG_FLOPPY		120	/* sub-dialog of config */
#define  DLG_CFG_RMV_DEVICES	121	/* sub-dialog of config */

/* Static text label IDs. */
#define IDT_1700		1700	/* Language: */
#define IDT_1701		1701	/* Machine: */
#define IDT_1702		1702	/* CPU type: */
#define IDT_1703		1703	/* Wait states: */
#define IDT_1704		1704	/* CPU: */
#define IDT_1705		1705	/* MB	== IDC_TEXT_MB */
#define IDT_1706		1706	/* Memory: */
#define IDT_1707		1707	/* Video: */
#define IDT_1708		1708	/* Video speed: */
#define IDT_1709		1709	/* Mouse: */
#define IDT_1710		1710	/* Joystick: */
#define IDT_1711		1711	/* Sound card: */
#define IDT_1712		1712	/* MIDI Out Device: */
#define IDT_1713		1713	/* Network type: */
#define IDT_1714		1714	/* PCap device: */
#define IDT_1715		1715	/* Network adapter: */
#define IDT_1716		1716	/* SCSI Controller: */
#define IDT_1717		1717	/* HD Controller: */
#define IDT_1718		1718	/* Tertiary IDE: */
#define IDT_1719		1719	/* Quaternary IDE: */
#define IDT_1720		1720	/* Hard disks: */
#define IDT_1721		1721	/* Bus: */
#define IDT_1722		1722	/* Channel: */
#define IDT_1723		1723	/* ID: */
#define IDT_1724		1724	/* LUN: */
#define IDT_1726		1726	/* Sectors: */
#define IDT_1727		1727	/* Heads: */
#define IDT_1728		1728	/* Cylinders: */
#define IDT_1729		1729	/* Size (MB): */
#define IDT_1730		1730	/* Type: */
#define IDT_1731		1731	/* File name: */
#define IDT_1737		1737	/* Floppy drives: */
#define IDT_1738		1738	/* Type: */
#define IDT_1739		1739	/* CD-ROM drives: */
#define IDT_1740		1740	/* Bus: */
#define IDT_1741		1741	/* ID: */
#define IDT_1742		1742	/* LUN: */
#define IDT_1743		1743	/* Channel: */
#define IDT_STEXT		1744	/* text in status window */
#define IDT_SDEVICE		1745	/* text in status window */
#define IDT_1746		1746	/* Gain */
#define IDT_1749		1749	/* File name: */
#define IDT_1750		1750	/* Disk size: */
#define IDT_1751		1751	/* RPM mode: */
#define IDT_1752		1752	/* Progress: */
#define IDT_1753		1753	/* Bus: */
#define IDT_1754		1754	/* ID: */
#define IDT_1755		1755	/* LUN: */
#define IDT_1756		1756	/* Channel: */
#define IDT_1757		1757	/* Progress: */
#define IDT_1758		1758	/* ZIP drives: */
#define IDT_TITLE		1759	/* "VARCem for Plaform" */
#define IDT_VERSION		1760	/* "version.." */
#define IDT_1761		1761	/* Speed: */
#define IDT_1762		1762	/* ZIP drives: */

/*
 * To try to keep these organized, we now group the
 * constants per dialog, as this allows easy adding
 * and deleting items.
 */
#define IDC_SETTINGSCATLIST	1001	/* generic config */
#define IDC_CFILE		1002	/* Select File dialog */
#define IDC_CHECK_SYNC		1008
/* Leave this as is until we finally get into localization in. */
#if 0
# define IDC_COMBO_LANG		1009
#endif

#define IDC_COMBO_MACHINE	1010	/* machine/cpu config */
#define IDC_CONFIGURE_MACHINE	1011
#define IDC_COMBO_CPU_TYPE	1012
#define IDC_COMBO_CPU		1013
#define IDC_CHECK_FPU		1014
#define IDC_COMBO_WS		1015
#ifdef USE_DYNAREC
# define IDC_CHECK_DYNAREC	1016
#endif
#define IDC_MEMTEXT		1017
#define IDC_MEMSPIN		1018
#define IDC_TEXT_MB		IDT_1705

#define IDC_VIDEO		1030	/* video config */
#define IDC_COMBO_VIDEO		1031
#define IDC_COMBO_VIDEO_SPEED	1032
#define IDC_CHECK_VOODOO	1033
#define IDC_BUTTON_VOODOO	1034

#define IDC_INPUT		1050	/* input config */
#define IDC_COMBO_MOUSE		1051
#define IDC_COMBO_JOYSTICK	1052
#define IDC_COMBO_JOY		1053
#define IDC_CONFIGURE_MOUSE	1054

#define IDC_SOUND		1070	/* sound config */
#define IDC_COMBO_SOUND		1071
#define IDC_COMBO_MIDI		1072
#define IDC_CHECK_NUKEDOPL	1073
#define IDC_CHECK_FLOAT		1074
#define IDC_CHECK_MPU401	1075
#define IDC_CONFIGURE_MPU401	1076

#define IDC_COMBO_NET_TYPE	1090	/* network config */
#define IDC_COMBO_PCAP		1091
#define IDC_COMBO_NET		1092

#define IDC_CHECK_GAME		1110	/* ports config */
#define IDC_CHECK_PARALLEL1	1111
#define IDC_CHECK_PARALLEL2	1112
#define IDC_CHECK_PARALLEL3	1113
#define IDC_COMBO_PARALLEL1	1114
#define IDC_COMBO_PARALLEL2	1115
#define IDC_COMBO_PARALLEL3	1116
#define IDC_CHECK_SERIAL1	1117
#define IDC_CHECK_SERIAL2	1118

#define IDC_OTHER_PERIPH	1120	/* other periph config */
#define IDC_COMBO_SCSI		1121
#define IDC_CONFIGURE_SCSI	1122
#define IDC_COMBO_HDC		1123
#define IDC_CONFIGURE_HDC	1124
#define IDC_COMBO_IDE_TER	1125
#define IDC_COMBO_IDE_QUA	1126
#define IDC_CHECK_BUGGER	1127

#define IDC_HARD_DISKS		1130	/* hard disk config */
#define IDC_LIST_HARD_DISKS	1131
#define IDC_BUTTON_HDD_ADD_NEW	1132
#define IDC_BUTTON_HDD_ADD	1133
#define IDC_BUTTON_HDD_REMOVE	1134
#define IDC_COMBO_HD_BUS	1135
#define IDC_COMBO_HD_CHANNEL	1136
#define IDC_COMBO_HD_ID		1137
#define IDC_COMBO_HD_LUN	1138
#define IDC_COMBO_HD_CHANNEL_IDE 1139

#define IDC_EDIT_HD_FILE_NAME	1140	/* add hard disk dialog */
#define IDC_EDIT_HD_SPT		1141
#define IDC_EDIT_HD_HPC		1142
#define IDC_EDIT_HD_CYL		1143
#define IDC_EDIT_HD_SIZE	1144
#define IDC_COMBO_HD_TYPE	1145
#define IDC_PBAR_IMG_CREATE	1146

#define IDC_REMOV_DEVICES	1150	/* removable dev config */
#define IDC_LIST_FLOPPY_DRIVES	1151
#define IDC_COMBO_FD_TYPE	1152
#define IDC_CHECKTURBO		1153
#define IDC_CHECKBPB		1154
#define IDC_LIST_CDROM_DRIVES	1155
#define IDC_COMBO_CD_BUS	1156
#define IDC_COMBO_CD_ID		1157
#define IDC_COMBO_CD_LUN	1158
#define IDC_COMBO_CD_CHANNEL_IDE 1159
#define IDC_LIST_ZIP_DRIVES	1160
#define IDC_COMBO_ZIP_BUS	1161
#define IDC_COMBO_ZIP_ID	1162
#define IDC_COMBO_ZIP_LUN	1163
#define IDC_COMBO_ZIP_CHANNEL_IDE 1164
#define IDC_CHECK250		1165
#define IDC_COMBO_CD_SPEED	1166

#define IDC_SLIDER_GAIN		1180	/* sound gain dialog */

#define IDC_EDIT_FILE_NAME	1190	/* new floppy image dialog */
#define IDC_COMBO_DISK_SIZE	1191
#define IDC_COMBO_RPM_MODE	1192


/* For the DeviceConfig code, re-do later. */
#define IDC_CONFIG_BASE		1200
#define  IDC_CONFIGURE_VID	1200
#define  IDC_CONFIGURE_SND	1201
#define  IDC_CONFIGURE_VOODOO	1202
#define  IDC_CONFIGURE_MOD	1203
#define  IDC_CONFIGURE_NET_TYPE	1204
#define  IDC_CONFIGURE_BUSLOGIC	1205
#define  IDC_CONFIGURE_PCAP	1206
#define  IDC_CONFIGURE_NET	1207
#define  IDC_CONFIGURE_MIDI	1208
#define  IDC_JOY1		1210
#define  IDC_JOY2		1211
#define  IDC_JOY3		1212
#define  IDC_JOY4		1213

#define IDC_HDTYPE		1280

#define IDC_RENDER		1281

#define IDC_STATBAR		1282


/* HELP menu. */
#define   IDC_ABOUT_ICON	65535


#endif	/*WIN_RESOURCE_H*/
