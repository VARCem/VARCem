/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the "CD/DVD Devices" dialog.
 *
 * Version:	@(#)win_settings_mmc.h	1.0.1	2019/05/03
 *
 * Authors:     Natalia Portillo, <claunia@claunia.com>
 *              Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020      Natalia Portillo.
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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


 /************************************************************************
  *									*
  *			  CD/DVD Devices Dialog			*
  *									*
  ************************************************************************/

  /* GLobal variables needed for the CD/DVD Devices dialog. */

static int mmc_ignore_change = 0;
static int cdlv_current_sel;

static void
cdrom_track_init(void) {
    int i;

    for (i = 0; i < CDROM_NUM; i++) {
        if (cdrom[i].bus_type == CDROM_BUS_ATAPI)
            ide_tracking |= (2 << (cdrom[i].bus_id.ide_channel << 3));
        else if (cdrom[i].bus_type == CDROM_BUS_SCSI)
            scsi_tracking[cdrom[i].bus_id.scsi.id] |= (2 << (cdrom[i].bus_id.scsi.lun << 3));
    }
}

static void
cdrom_image_list_init(HWND hwndList) {
    HICON      hiconItem;
    HIMAGELIST hSmall;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON),
                              ILC_MASK | ILC_COLOR32, 1, 1);

    hiconItem = LoadIcon(hInstance, (LPCWSTR)ICON_CDROM_D);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    hiconItem = LoadIcon(hInstance, (LPCWSTR)ICON_CDROM);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
}

static BOOL
cdrom_recalc_list(HWND hwndList) {
    WCHAR  temp[128];
    LVITEM lvI;
    int    fsid, i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i = 0; i < CDROM_NUM; i++) {
        fsid = temp_cdrom_drives[i].bus_type;

        lvI.iSubItem = 0;
        switch (fsid) {
            case CDROM_BUS_DISABLED:
            default:
                lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
                lvI.iImage = 0;
                break;

            case CDROM_BUS_ATAPI:
                swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
                         get_string(combo_to_string(fsid)),
                         temp_cdrom_drives[i].bus_id.ide_channel >> 1,
                         temp_cdrom_drives[i].bus_id.ide_channel & 1);
                lvI.pszText = temp;
                lvI.iImage = 1;
                break;

            case CDROM_BUS_SCSI:
                swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
                         get_string(combo_to_string(fsid)),
                         temp_cdrom_drives[i].bus_id.scsi.id,
                         temp_cdrom_drives[i].bus_id.scsi.lun);
                lvI.pszText = temp;
                lvI.iImage = 1;
                break;
        }
        lvI.iItem = i;
        if (ListView_InsertItem(hwndList, &lvI) == -1)
            return FALSE;

        lvI.iSubItem = 1;
        if (fsid == CDROM_BUS_DISABLED) {
            lvI.pszText = (LPTSTR)get_string(IDS_NONE);
        } else {
            wsprintf(temp, L"%ix",
                     cdrom_speeds[temp_cdrom_drives[i].speed_idx].speed);
            lvI.pszText = temp;
        }
        lvI.iItem = i;
        lvI.iImage = 0;
        if (ListView_SetItem(hwndList, &lvI) == -1)
            return FALSE;
    }

    return TRUE;
}

static BOOL
cdrom_init_columns(HWND hwndList) {
    LVCOLUMN lvc;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = (LPTSTR)get_string(IDS_BUS);
    lvc.cx = 342;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 0, &lvc) == -1)
        return FALSE;

    lvc.iSubItem = 1;
    lvc.pszText = (LPTSTR)get_string(IDS_3579);
    lvc.cx = 80;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 1, &lvc) == -1)
        return FALSE;

    return TRUE;
}

static int
cdrom_get_selected(HWND hdlg) {
    int  drive = -1;
    int  i, j = 0;
    HWND h;

    for (i = 0; i < 6; i++) {
        h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
        j = ListView_GetItemState(h, i, LVIS_SELECTED);
        if (j)
            drive = i;
    }

    return drive;
}

static void
cdrom_update_item(HWND hwndList, int i) {
    WCHAR  temp[128];
    LVITEM lvI;
    int    fsid;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    lvI.iSubItem = 0;
    lvI.iItem = i;

    fsid = temp_cdrom_drives[i].bus_type;

    switch (fsid) {
        case CDROM_BUS_DISABLED:
        default:
            lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
            lvI.iImage = 0;
            break;

        case CDROM_BUS_ATAPI:
            swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
                     get_string(combo_to_string(fsid)),
                     temp_cdrom_drives[i].bus_id.ide_channel >> 1,
                     temp_cdrom_drives[i].bus_id.ide_channel & 1);
            lvI.pszText = temp;
            lvI.iImage = 1;
            break;

        case CDROM_BUS_SCSI:
            swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
                     get_string(combo_to_string(fsid)),
                     temp_cdrom_drives[i].bus_id.scsi.id,
                     temp_cdrom_drives[i].bus_id.scsi.lun);
            lvI.pszText = temp;
            lvI.iImage = 1;
            break;
    }
    if (ListView_SetItem(hwndList, &lvI) == -1)
        return;

    lvI.iSubItem = 1;
    if (fsid == CDROM_BUS_DISABLED) {
        lvI.pszText = (LPTSTR)get_string(IDS_NONE);
    } else {
        wsprintf(temp, L"%ix",
                 cdrom_speeds[temp_cdrom_drives[i].speed_idx].speed);
        lvI.pszText = temp;
    }
    lvI.iItem = i;
    lvI.iImage = 0;
    if (ListView_SetItem(hwndList, &lvI) == -1)
        return;
}

static void
cdrom_add_locations(HWND hdlg) {
    WCHAR temp[128];
    HWND  h;
    int   i = 0;

    h = GetDlgItem(hdlg, IDC_COMBO_CD_BUS);
    for (i = CDROM_BUS_DISABLED; i < CDROM_BUS_MAX; i++)
        SendMessage(h, CB_ADDSTRING, 0, win_string(combo_to_string(i)));

    /* Create a list of usable CD-ROM speeds. */
    h = GetDlgItem(hdlg, IDC_COMBO_CD_SPEED);
    for (i = 0; cdrom_speeds[i].speed > 0; i++) {
        swprintf(temp, sizeof_w(temp), L"%ix", cdrom_speeds[i].speed);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
    id_to_combo(h, 16);

    h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
    id_to_combo(h, 8);

    h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
    for (i = 0; i < 8; i++) {
        swprintf(temp, sizeof_w(temp), L"%01i:%01i", i >> 1, i & 1);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}

static void
cdrom_recalc_location_controls(HWND hdlg, int assign_id) {
    HWND h;
    int  i;
    int  bus = temp_cdrom_drives[cdlv_current_sel].bus_type;

    for (i = IDT_1741; i < (IDT_1743 + 1); i++) {
        h = GetDlgItem(hdlg, i);
        EnableWindow(h, FALSE);
        ShowWindow(h, SW_HIDE);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_CD_SPEED);
    if (bus == CDROM_BUS_DISABLED) {
        EnableWindow(h, FALSE);
        ShowWindow(h, SW_HIDE);
    } else {
        ShowWindow(h, SW_SHOW);
        EnableWindow(h, TRUE);
        SendMessage(h, CB_SETCURSEL,
                    temp_cdrom_drives[cdlv_current_sel].speed_idx, 0);
    }

    switch (bus) {
        case CDROM_BUS_ATAPI:
            h = GetDlgItem(hdlg, IDT_1743);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            if (assign_id)
                temp_cdrom_drives[cdlv_current_sel].bus_id.ide_channel = next_free_ide_channel();

            h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_cdrom_drives[cdlv_current_sel].bus_id.ide_channel, 0);
            break;

        case CDROM_BUS_SCSI:
            h = GetDlgItem(hdlg, IDT_1741);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            h = GetDlgItem(hdlg, IDT_1742);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            if (assign_id)
                next_free_scsi_id_and_lun((uint8_t *)&temp_cdrom_drives[cdlv_current_sel].bus_id.scsi.id, (uint8_t *)&temp_cdrom_drives[cdlv_current_sel].bus_id.scsi.lun);

            h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_cdrom_drives[cdlv_current_sel].bus_id.scsi.id, 0);

            h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_cdrom_drives[cdlv_current_sel].bus_id.scsi.lun, 0);
            break;
    }
}

static void
cdrom_track(uint8_t id) {
    if (temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI)
        ide_tracking |= (2 << (temp_cdrom_drives[id].bus_id.ide_channel << 3));
    else if (temp_cdrom_drives[id].bus_type == CDROM_BUS_SCSI)
        scsi_tracking[temp_cdrom_drives[id].bus_id.scsi.id] |= (1ULL << temp_cdrom_drives[id].bus_id.scsi.lun);
}

static void
cdrom_untrack(uint8_t id) {
    if (temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI)
        ide_tracking &= ~(2 << (temp_cdrom_drives[id].bus_id.ide_channel << 3));
    else if (temp_cdrom_drives[id].bus_type == CDROM_BUS_SCSI)
        scsi_tracking[temp_cdrom_drives[id].bus_id.scsi.id] &= ~(1ULL << temp_cdrom_drives[id].bus_id.scsi.lun);
}

#if 0
static void
cdrom_track_all(void)
{
    int i;

    for (i = 0; i < CDROM_NUM; i++)
	cdrom_track(i);
}
#endif