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
 * Version:	@(#)resource.h	1.0.19	2018/09/09
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
#define DLG_ABOUT		1000	/* top-level dialog */
# define DLG_LOCALIZE		1001
#define DLG_SND_GAIN		1010	/* top-level dialog */
#define DLG_NEW_FLOPPY		1020	/* top-level dialog */
#define DLG_CONFIG		1030	/* top-level dialog */
#define  DLG_CFG_MACHINE	1031	/* sub-dialog of config */
#define  DLG_CFG_VIDEO		1032	/* sub-dialog of config */
#define  DLG_CFG_INPUT		1033	/* sub-dialog of config */
#define  DLG_CFG_SOUND		1034	/* sub-dialog of config */
#define  DLG_CFG_NETWORK	1035	/* sub-dialog of config */
#define  DLG_CFG_PORTS		1036	/* sub-dialog of config */
#define  DLG_CFG_PERIPHERALS	1037	/* sub-dialog of config */
#define  DLG_CFG_DISK		1038	/* sub-dialog of config */
#define  DLG_CFG_DISK_ADD	1039	/* sub-dialog of config */
#define  DLG_CFG_FLOPPY		1040	/* sub-dialog of config */
#define  DLG_CFG_RMV_DEVICES	1041	/* sub-dialog of config */


/* Static text label IDs. */
#define IDT_1700		1700	/* Language: */
#define IDT_1701		1701	/* Machine: */
#define IDT_1702		1702	/* CPU type: */
#define IDT_1703		1703	/* Wait states: */
#define IDT_1704		1704	/* CPU: */
#define IDT_1705		1705	/* MB	== IDC_TEXT_MB */
#define IDT_1706		1706	/* Memory: */
#define IDT_1707		1707	/* Video: */
#define IDT_1708		1708	/* Time sync: */
#define IDT_1709		1709	/* Mouse: */
#define IDT_1710		1710	/* Joystick: */
#define IDT_1711		1711	/* Sound card: */
#define IDT_1712		1712	/* MIDI Out Device: */
#define IDT_1713		1713	/* Network type: */
#define IDT_1714		1714	/* PCap device: */
#define IDT_1715		1715	/* Network adapter: */
#define IDT_1716		1716	/* SCSI Controller: */
#define IDT_1717		1717	/* HD Controller: */
/* 1718 unused */
/* 1719 unused */
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
/* 1744/1745 unused */
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
#define IDT_1761		1761	/* Speed: */
#define IDT_1762		1762	/* ZIP drives: */
#define IDT_1763		1763	/* ISAMEM #1: */
#define IDT_1764		1764	/* ISAMEM #2: */
#define IDT_1765		1765	/* ISAMEM #3: */
#define IDT_1766		1766	/* ISAMEM #4: */
#define IDT_1767		1767	/* ISARTC: */

#define IDT_TITLE		1790	/* "VARCem for Plaform" */
#define IDT_VERSION		1791	/* "version.." */
#define IDT_LOCALIZE		1795	/* "Localization done by.." */


/*
 * To try to keep these organized, we now group the
 * constants per dialog, as this allows easy adding
 * and deleting items.
 */
#define IDC_SETTINGSCATLIST	1001	/* generic config */
#define IDC_CFILE		1002	/* Select File dialog */
#define IDC_COMBO_SYNC		1008

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
#define IDC_CONFIGURE_VIDEO	1032
#define IDC_CHECK_VOODOO	1033
#define IDC_CONFIGURE_VOODOO	1034

#define IDC_INPUT		1050	/* input config */
#define IDC_COMBO_MOUSE		1051
#define IDC_CONFIGURE_MOUSE	1052
#define IDC_COMBO_JOYSTICK	1053
#define IDC_COMBO_JOY		1054
#define IDC_CONFIGURE_JOY1	1235
#define IDC_CONFIGURE_JOY2	1236
#define IDC_CONFIGURE_JOY3	1237
#define IDC_CONFIGURE_JOY4	1238

#define IDC_SOUND		1070	/* sound config */
#define IDC_COMBO_SOUND		1071
#define IDC_CONFIGURE_SOUND	1072
#define IDC_COMBO_MIDI		1073
#define IDC_CONFIGURE_MIDI	1074
#define IDC_CHECK_NUKEDOPL	1075
#define IDC_CHECK_FLOAT		1076
#define IDC_CHECK_MPU401	1077
#define IDC_CONFIGURE_MPU401	1078

#define IDC_COMBO_NET_TYPE	1090	/* network config */
#define IDC_CONFIGURE_NET_TYPE	1091
#define IDC_COMBO_PCAP		1092
#define IDC_CONFIGURE_PCAP	1093
#define IDC_COMBO_NET_CARD	1094
#define IDC_CONFIGURE_NET_CARD	1095

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
#define IDC_CHECK_IDE_TER	1125
#define IDC_CONFIGURE_IDE_TER	1126
#define IDC_CHECK_IDE_QUA	1127
#define IDC_CONFIGURE_IDE_QUA	1128
#define IDC_CHECK_BUGGER	1129
#define IDC_CONFIGURE_BUGGER	1130
#define IDC_COMBO_ISARTC	1131
#define IDC_CONFIGURE_ISARTC	1132

#define IDC_GROUP_ISAMEM	1140
#define IDC_COMBO_ISAMEM_1	1141
#define IDC_COMBO_ISAMEM_2	1142
#define IDC_COMBO_ISAMEM_3	1143
#define IDC_COMBO_ISAMEM_4	1144
#define IDC_CONFIGURE_ISAMEM_1	1145
#define IDC_CONFIGURE_ISAMEM_2	1146
#define IDC_CONFIGURE_ISAMEM_3	1147
#define IDC_CONFIGURE_ISAMEM_4	1148

#define IDC_HARD_DISKS		1150	/* hard disk config */
#define IDC_LIST_HARD_DISKS	1151
#define IDC_BUTTON_HDD_ADD_NEW	1152
#define IDC_BUTTON_HDD_ADD	1153
#define IDC_BUTTON_HDD_REMOVE	1154
#define IDC_COMBO_HD_BUS	1155
#define IDC_COMBO_HD_CHANNEL	1156
#define IDC_COMBO_HD_ID		1157
#define IDC_COMBO_HD_LUN	1158
#define IDC_COMBO_HD_CHANNEL_IDE 1159

#define IDC_EDIT_HD_FILE_NAME	1160	/* add hard disk dialog */
#define IDC_EDIT_HD_SPT		1161
#define IDC_EDIT_HD_HPC		1162
#define IDC_EDIT_HD_CYL		1163
#define IDC_EDIT_HD_SIZE	1164
#define IDC_COMBO_HD_TYPE	1165
#define IDC_PBAR_IMG_CREATE	1166

#define IDC_REMOV_DEVICES	1170	/* removable dev config */
#define IDC_LIST_FLOPPY_DRIVES	1171
#define IDC_COMBO_FD_TYPE	1172
#define IDC_CHECKTURBO		1173
#define IDC_CHECKBPB		1174
#define IDC_LIST_CDROM_DRIVES	1175
#define IDC_COMBO_CD_BUS	1176
#define IDC_COMBO_CD_ID		1177
#define IDC_COMBO_CD_LUN	1178
#define IDC_COMBO_CD_CHANNEL_IDE 1179
#define IDC_LIST_ZIP_DRIVES	1180
#define IDC_COMBO_ZIP_BUS	1181
#define IDC_COMBO_ZIP_ID	1182
#define IDC_COMBO_ZIP_LUN	1183
#define IDC_COMBO_ZIP_CHANNEL_IDE 1184
#define IDC_CHECK250		1185
#define IDC_COMBO_CD_SPEED	1186

#define IDC_SLIDER_GAIN		1190	/* sound gain dialog */

#define IDC_EDIT_FILE_NAME	1195	/* new floppy image dialog */
#define IDC_COMBO_DISK_SIZE	1196
#define IDC_COMBO_RPM_MODE	1197

/* Misc items. */
#define IDC_CONFIGURE_DEV	1200	/* dynamic DeviceConfig code */
#define IDC_STATBAR		1201	/* status bar events */


/* HELP menu. */
#define IDC_LOCALIZE		1290
#define IDC_DONATE		1291
#define IDC_SUPPORT		1292
#define  IDC_ABOUT_ICON		65535


#endif	/*WIN_RESOURCE_H*/
