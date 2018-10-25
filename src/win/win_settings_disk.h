/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Settings dialog.
 *
 * Version:	@(#)win_settings_disk.h	1.0.18	2018/10/24
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
 *			     (Hard) Disk Dialog				*
 *									*
 ************************************************************************/

/* Global variables needed for the Hard Disk dialogs. */
static uint64_t st506_tracking, esdi_tracking,
                ide_tracking, scsi_tracking[16];
static hard_disk_t *hdd_ptr;
static hard_disk_t new_hdd;
static wchar_t	hd_file_name[512];
static int	hard_disk_added = 0;
static int	max_spt = 63;
static int	max_hpc = 255;
static int	max_tracks = 266305;
static int	no_update = 0;
static int	existing = 0;
static uint64_t	selection = 127;
static uint64_t	spt, hpc, tracks, size;
static int	chs_enabled = 0;
static int	ignore_change = 0;
static int	hdc_id_to_listview_index[HDD_NUM];
static int	hd_listview_items;
static int	hdlv_current_sel;
static int	next_free_id = 0;


static void
disk_track_init(void)
{
    int i;

    st506_tracking = esdi_tracking = ide_tracking = 0;
    for (i = 0; i < 16; i++)
	scsi_tracking[i] = 0;

    for (i = 0; i < HDD_NUM; i++) {
	if (hdd[i].bus == HDD_BUS_ST506)
		st506_tracking |= (1ULL << (hdd[i].bus_id.st506_channel << 3));
	else if (hdd[i].bus == HDD_BUS_ESDI)
		esdi_tracking |= (1ULL << (hdd[i].bus_id.esdi_channel << 3));
	else if (hdd[i].bus == HDD_BUS_IDE)
		ide_tracking |= (1ULL << (hdd[i].bus_id.ide_channel << 3));
	else if (hdd[i].bus == HDD_BUS_SCSI)
		scsi_tracking[hdd[i].bus_id.scsi.id] |= (1ULL << (hdd[i].bus_id.scsi.lun << 3));
    }
}


static BOOL
disk_image_list_init(HWND hwndList)
{
    HICON hiconItem;
    HIMAGELIST hSmall;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);

    hiconItem = LoadIcon(hInstance, (LPCWSTR)ICON_DISK);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);

    return TRUE;
}


static void
disk_normalize_list(void)
{
    hard_disk_t ihdd[HDD_NUM];
    int i, j;

    memset(ihdd, 0x00, HDD_NUM * sizeof(hard_disk_t));

    j = 0;
    for (i = 0; i < HDD_NUM; i++) {
	if (temp_hdd[i].bus != HDD_BUS_DISABLED) {
		memcpy(&ihdd[j], &temp_hdd[i], sizeof(hard_disk_t));
		j++;
	}
    }

    memcpy(temp_hdd, ihdd, HDD_NUM * sizeof(hard_disk_t));
}


static int
disk_get_selected(HWND hdlg)
{
    int hard_disk = -1;
    int i, j;
    HWND h;

    if (hd_listview_items == 0)
	return 0;

    j = 0;
    for (i = 0; i < hd_listview_items; i++) {
	h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
	j = ListView_GetItemState(h, i, LVIS_SELECTED);
	if (j)
		hard_disk = i;
    }

    return hard_disk;
}


static void
disk_add_locations(HWND hdlg)
{
    WCHAR temp[128];
    HWND h;
    int i;

    h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
    for (i = 0; i < HDD_BUS_MAX; i++)
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)hdd_bus_to_ids(i));

    h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), L"%01i:%01i", i >> 1, i & 1);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
    for (i = 0; i < 16; i++) {
	swprintf(temp, sizeof_w(temp), L"%i", i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), L"%i", i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), L"%01i:%01i", i >> 1, i & 1);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}


// FIXME: this stuff has to be moved to the proper HDC/SCSI modules!  --FVK
static int8_t
next_free_binary_channel(uint64_t *tracking)
{
    int8_t i;

    for (i = 0; i < 2; i++) {
	if (!(*tracking & (0xffLL << (i << 3LL))))
		return i;
    }

    return 2;
}


static int8_t
next_free_ide_channel(void)
{
    int8_t i;

    for (i = 0; i < 8; i++) {
	if (!(ide_tracking & (0xffLL << (i << 3LL))))
		return i;
    }

    return 7;
}


static void
next_free_scsi_id_and_lun(uint8_t *id, uint8_t *lun)
{
    int8_t i, j;

    for (i = 0; i < 16; i++) {
	for (j = 0; j < 8; j++) {
		if (! (scsi_tracking[i] & (0xffULL << (j << 3LL)))) {
			*id = (uint8_t)i;
			*lun = (uint8_t)j;
			return;
		}
	}
    }

    *id = 6;
    *lun = 7;
}


static int
bus_full(uint64_t *tracking, int count)
{
    uint64_t full = 0;

    switch(count) {
	case 2:
	default:
		full = (*tracking & 0xFF00ULL);
		full = full && (*tracking & 0x00FFULL);
		return full != 0;

	case 8:
		full = (*tracking & 0xFF00000000000000ULL);
		full = full && (*tracking & 0x00FF000000000000ULL);
		full = full && (*tracking & 0x0000FF0000000000ULL);
		full = full && (*tracking & 0x000000FF00000000ULL);
		full = full && (*tracking & 0x00000000FF000000ULL);
		full = full && (*tracking & 0x0000000000FF0000ULL);
		full = full && (*tracking & 0x000000000000FF00ULL);
		full = full && (*tracking & 0x00000000000000FFULL);
		return full != 0;
    }
}


static void
disk_recalc_location_controls(HWND hdlg, int is_add_dlg, int assign_id)
{
    HWND h;
    int bus, i;

    for (i = IDT_1722; i <= IDT_1724; i++) {
	h = GetDlgItem(hdlg, i);
	EnableWindow(h, FALSE);
	ShowWindow(h, SW_HIDE);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    if ((hd_listview_items > 0) || is_add_dlg) {
	h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
	bus = (int)SendMessage(h, CB_GETCURSEL, 0, 0);

	switch(bus) {
		case HDD_BUS_ST506:		/* ST506 MFM/RLL */
			h = GetDlgItem(hdlg, IDT_1722);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			if (assign_id)
				temp_hdd[hdlv_current_sel].bus_id.st506_channel = next_free_binary_channel(&st506_tracking);
			SendMessage(h, CB_SETCURSEL, is_add_dlg ? new_hdd.bus_id.st506_channel : temp_hdd[hdlv_current_sel].bus_id.st506_channel, 0);
			break;

		case HDD_BUS_ESDI:		/* ESDI */
			h = GetDlgItem(hdlg, IDT_1722);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			if (assign_id)
				temp_hdd[hdlv_current_sel].bus_id.esdi_channel = next_free_binary_channel(&esdi_tracking);
			SendMessage(h, CB_SETCURSEL, is_add_dlg ? new_hdd.bus_id.esdi_channel : temp_hdd[hdlv_current_sel].bus_id.esdi_channel, 0);
			break;

		case HDD_BUS_IDE:		/* IDE */
			h = GetDlgItem(hdlg, IDT_1722);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			if (assign_id)
				temp_hdd[hdlv_current_sel].bus_id.ide_channel = next_free_ide_channel();
			SendMessage(h, CB_SETCURSEL, is_add_dlg ? new_hdd.bus_id.ide_channel : temp_hdd[hdlv_current_sel].bus_id.ide_channel, 0);
			break;

		case HDD_BUS_SCSI:		/* SCSI */
			h = GetDlgItem(hdlg, IDT_1723);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			h = GetDlgItem(hdlg, IDT_1724);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);

			if (assign_id)
				next_free_scsi_id_and_lun((uint8_t *) &temp_hdd[hdlv_current_sel].bus_id.scsi.id, (uint8_t *) &temp_hdd[hdlv_current_sel].bus_id.scsi.lun);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			SendMessage(h, CB_SETCURSEL, is_add_dlg ? new_hdd.bus_id.scsi.id : temp_hdd[hdlv_current_sel].bus_id.scsi.id, 0);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
			ShowWindow(h, SW_SHOW);
			EnableWindow(h, TRUE);
			SendMessage(h, CB_SETCURSEL, is_add_dlg ? new_hdd.bus_id.scsi.lun : temp_hdd[hdlv_current_sel].bus_id.scsi.lun, 0);
			break;

		case HDD_BUS_USB:		/* USB */
			/* Just kidding. Not yet supported. */
			break;
	}
    }

    if ((hd_listview_items == 0) && !is_add_dlg) {
	h = GetDlgItem(hdlg, IDT_1721);
	EnableWindow(h, FALSE);
	ShowWindow(h, SW_HIDE);

	h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
	EnableWindow(h, FALSE);
	ShowWindow(h, SW_HIDE);
    } else {
	h = GetDlgItem(hdlg, IDT_1721);
	ShowWindow(h, SW_SHOW);
	EnableWindow(h, TRUE);

	h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
	ShowWindow(h, SW_SHOW);
	EnableWindow(h, TRUE);
    }
}


static void
recalc_next_free_id(HWND hdlg)
{
    HWND h;
    int c_st506 = 0;
    int c_esdi = 0;
    int c_ide = 0;
    int c_scsi = 0;
    int enable_add = 0;
    int i;

    next_free_id = -1;

    for (i = 0; i < HDD_NUM; i++) {
	if (temp_hdd[i].bus == HDD_BUS_ST506)
		c_st506++;
	else if (temp_hdd[i].bus == HDD_BUS_ESDI)
		c_esdi++;
	else if (temp_hdd[i].bus == HDD_BUS_IDE)
		c_ide++;
	else if (temp_hdd[i].bus == HDD_BUS_SCSI)
		c_scsi++;
    }

    for (i = 0; i < HDD_NUM; i++) {
	if (temp_hdd[i].bus == HDD_BUS_DISABLED) {
		next_free_id = i;
		break;
	}
    }

    enable_add = enable_add || (next_free_id >= 0);
    enable_add = enable_add && ((c_st506 < ST506_NUM) ||
			        (c_esdi < ESDI_NUM) ||
				(c_ide < IDE_NUM) ||
				(c_scsi < SCSI_NUM));
    enable_add = enable_add && !bus_full(&st506_tracking, 2);
    enable_add = enable_add && !bus_full(&esdi_tracking, 2);
    enable_add = enable_add && !bus_full(&ide_tracking, 8);
    for (i = 0; i < 16; i++)
	enable_add = enable_add && !bus_full(&(scsi_tracking[i]), 8);

    h = GetDlgItem(hdlg, IDC_BUTTON_HDD_ADD_NEW);
    if (enable_add)
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_BUTTON_HDD_ADD);
    if (enable_add)
	EnableWindow(h, TRUE);
      else
	EnableWindow(h, FALSE);

    h = GetDlgItem(hdlg, IDC_BUTTON_HDD_REMOVE);
    if ((c_st506 == 0) && (c_esdi == 0) && (c_ide == 0) && (c_scsi == 0))
	EnableWindow(h, FALSE);
      else
	EnableWindow(h, TRUE);
}


static void
disk_update_item(HWND hwndList, int i, int column)
{
    WCHAR temp[128];
    LVITEM lvI;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;
    lvI.iSubItem = column;
    lvI.iItem = i;

    if (column == 0) {
	switch(temp_hdd[i].bus) {
		case HDD_BUS_ST506:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.st506_channel >> 1,
				 temp_hdd[i].bus_id.st506_channel & 1);
			break;

		case HDD_BUS_ESDI:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.esdi_channel >> 1,
				 temp_hdd[i].bus_id.esdi_channel & 1);
			break;

		case HDD_BUS_IDE:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.ide_channel >> 1,
				 temp_hdd[i].bus_id.ide_channel & 1);
			break;

		case HDD_BUS_SCSI:
			swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.scsi.id,
				 temp_hdd[i].bus_id.scsi.lun);
			break;

		case HDD_BUS_USB:
			/* Nope. Not yet. */
			break;
	}

	lvI.pszText = temp;
	lvI.iImage = 0;
    } else if (column == 1) {
	lvI.pszText = temp_hdd[i].fn;
	lvI.iImage = 0;
    } else if (column == 2) {
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].tracks);
	lvI.pszText = temp;
	lvI.iImage = 0;
    } else if (column == 3) {
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].hpc);
	lvI.pszText = temp;
	lvI.iImage = 0;
    } else if (column == 4) {
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].spt);
	lvI.pszText = temp;
	lvI.iImage = 0;
    } else if (column == 5) {
	swprintf(temp, sizeof_w(temp), L"%i", (temp_hdd[i].tracks * temp_hdd[i].hpc * temp_hdd[i].spt) >> 11);
	lvI.pszText = temp;
	lvI.iImage = 0;
    }

    if (ListView_SetItem(hwndList, &lvI) == -1) {
	/* Error, not used. */
	return;
    }
}


static BOOL
disk_recalc_list(HWND hwndList)
{
    WCHAR temp[128];
    LVITEM lvI;
    int i, j;

    hd_listview_items = 0;
    hdlv_current_sel = -1;

    ListView_DeleteAllItems(hwndList);

    j = 0;
    for (i = 0; i < HDD_NUM; i++) {
	if (temp_hdd[i].bus <= 0) {
		hdc_id_to_listview_index[i] = -1;
		continue;
	}

	/* Clear the item. */
	memset(&lvI, 0x00, sizeof(lvI));
	lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
	lvI.stateMask = lvI.iSubItem = lvI.state = 0;
	lvI.iSubItem = 0;

	hdc_id_to_listview_index[i] = j;

	switch(temp_hdd[i].bus) {
		case HDD_BUS_ST506:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.st506_channel >> 1,
				 temp_hdd[i].bus_id.st506_channel & 1);
			break;

		case HDD_BUS_ESDI:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.esdi_channel >> 1,
				 temp_hdd[i].bus_id.esdi_channel & 1);
			break;

		case HDD_BUS_IDE:
			swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.ide_channel >> 1,
				 temp_hdd[i].bus_id.ide_channel & 1);
			break;

		case HDD_BUS_SCSI:
			swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
				 hdd_bus_to_ids(temp_hdd[i].bus),
				 temp_hdd[i].bus_id.scsi.id,
				 temp_hdd[i].bus_id.scsi.lun);
			break;

		case HDD_BUS_USB:
			/* Still not implemened. Submit yours! */
			break;
	}

	lvI.pszText = temp;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_InsertItem(hwndList, &lvI) == -1) return FALSE;

	lvI.iSubItem = 1;
	lvI.pszText = temp_hdd[i].fn;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1) return FALSE;

	lvI.iSubItem = 2;
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].tracks);
	lvI.pszText = temp;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1) return FALSE;

	lvI.iSubItem = 3;
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].hpc);
	lvI.pszText = temp;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1) return FALSE;

	lvI.iSubItem = 4;
	swprintf(temp, sizeof_w(temp), L"%i", temp_hdd[i].spt);
	lvI.pszText = temp;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1) return FALSE;

	lvI.iSubItem = 5;
	swprintf(temp, sizeof_w(temp), L"%i", (temp_hdd[i].tracks * temp_hdd[i].hpc * temp_hdd[i].spt) >> 11);
	lvI.pszText = temp;
	lvI.iItem = j;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1) return FALSE;

	j++;
    }

    hd_listview_items = j;

    return TRUE;
}


static BOOL
disk_init_columns(HWND hwndList)
{
    LVCOLUMN lvc;
    int iCol;

    /* Icon, Bus, File, C, H, S, Size */
    for (iCol = 0; iCol < 6; iCol++) {
	memset(&lvc, 0x00, sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	lvc.iSubItem = iCol;

	switch(iCol) {
		case 0: /* Bus */
			lvc.pszText = (LPTSTR)get_string(IDS_3504);
			lvc.cx = 135;
			lvc.fmt = LVCFMT_LEFT;
			break;

		case 1: /* File */
			lvc.pszText = (LPTSTR)get_string(IDS_3505);
			lvc.cx = 150;
			lvc.fmt = LVCFMT_LEFT;
			break;

		case 2: /* Cylinders */
			lvc.pszText = (LPTSTR)get_string(IDS_3506);
			lvc.cx = 41;
			lvc.fmt = LVCFMT_RIGHT;
			break;

		case 3: /* Heads */
			lvc.pszText = (LPTSTR)get_string(IDS_3507);
			lvc.cx = 25;
			lvc.fmt = LVCFMT_RIGHT;
			break;

		case 4: /* Sectors */
			lvc.pszText = (LPTSTR)get_string(IDS_3508);
			lvc.cx = 25;
			lvc.fmt = LVCFMT_RIGHT;
			break;

		case 5: /* Size (MB) 8 */
			lvc.pszText = (LPTSTR)get_string(IDS_3509);
			lvc.cx = 41;
			lvc.fmt = LVCFMT_RIGHT;
			break;
	}

	if (ListView_InsertColumn(hwndList, iCol, &lvc) == -1) return FALSE;
    }

    return TRUE;
}


static void
get_edit_box_contents(HWND hdlg, int id, uint64_t *val)
{
    WCHAR temp[128];
    char tempA[128];
    HWND h;

    h = GetDlgItem(hdlg, id);
    SendMessage(h, WM_GETTEXT, sizeof_w(temp), (LPARAM)temp);
    wcstombs(tempA, temp, sizeof(tempA));
    sscanf(tempA, "%" PRIu64, val);
}


static void
get_combo_box_selection(HWND hdlg, int id, uint64_t *val)
{
    HWND h;

    h = GetDlgItem(hdlg, id);
    *val = (uint64_t)SendMessage(h, CB_GETCURSEL, 0, 0);
}


static void
set_edit_box_contents(HWND hdlg, int id, uint64_t val)
{
    WCHAR temp[128];
    HWND h;

    h = GetDlgItem(hdlg, id);
    swprintf(temp, sizeof_w(temp), L"%" PRIu64, val);
    SendMessage(h, WM_SETTEXT, (WPARAM)wcslen(temp), (LPARAM)temp);
}


static uint64_t
disk_initialize_hdt(HWND hdlg)
{
    WCHAR temp[128], temp2[128];
    uint64_t temp_size = 0;
    uint64_t size_mb = 0;
    HWND h;
    int i;

    selection = 127;

    h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
    for (i = 0; i < 127; i++) {	
	temp_size = hdd_table[i].cyls * hdd_table[i].head * hdd_table[i].sect;
	size_mb = temp_size >> 11;
	swprintf(temp, sizeof_w(temp), L"%" PRIu64, size_mb);
	swprintf(temp2, sizeof_w(temp2), get_string(IDS_3510),
		 hdd_table[i].cyls, hdd_table[i].head, hdd_table[i].sect);
	wcscat(temp, L" ");
	wcscat(temp, temp2);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
	if ((tracks == hdd_table[i].cyls) && (hpc == hdd_table[i].head) && (spt == hdd_table[i].sect))
		selection = i;
    }

    /*
     * Add the "Custom" and "Custom (large)" strings.
     * Maybe we should have these first in the list?
     */
    SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3511));
    SendMessage(h, CB_ADDSTRING, 0, win_string(IDS_3512));

    SendMessage(h, CB_SETCURSEL, selection & 0xffff, 0);

    return selection;
}


static void
disk_recalc_selection(HWND hdlg)
{
    HWND h;
    int i;

    selection = 127;
    h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
    for (i = 0; i < 127; i++) {	
	if ((tracks == hdd_table[i].cyls) &&
	    (hpc == hdd_table[i].head) && (spt == hdd_table[i].sect))
		selection = i;
    }

    if ((selection == 127) && (hpc == 16) && (spt == 63))
	selection = 128;

    SendMessage(h, CB_SETCURSEL, selection & 0xffff, 0);
}


static WIN_RESULT CALLBACK
disk_add_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    wchar_t temp_path[512];
    char buf[512], *big_buf;
    HWND h = INVALID_HANDLE_VALUE;
    uint64_t i = 0;
    uint64_t temp;
    FILE *f;
    uint32_t sector_size = 512;
    uint32_t zero = 0;
    uint32_t base = 0x1000;
    uint64_t signature = 0xD778A82044445459ll;
    int b = 0;
    uint64_t r = 0;
    uint8_t channel = 0;
    uint8_t id = 0, lun = 0;
    wchar_t *twcs;

    switch (message) {
	case WM_INITDIALOG:
		dialog_center(hdlg);
		memset(hd_file_name, 0x00, sizeof(hd_file_name));

		if (existing & 2) {
			next_free_id = (existing >> 3) & 0x1f;
			hdd_ptr = &(hdd[next_free_id]);
		} else {
			hdd_ptr = &(temp_hdd[next_free_id]);
		}

		/* Override the window title. */
		SetWindowText(hdlg, get_string((existing & 1) ? IDS_3527
							      : IDS_3526));
		no_update = 1;
		spt = (existing & 1) ? 0 : 17;
		set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
		hpc = (existing & 1) ? 0 : 15;
		set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
		tracks = (existing & 1) ? 0 : 1023;
		set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
		size = (tracks * hpc * spt) << 9;
		set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);

		disk_initialize_hdt(hdlg);

		if (existing & 1) {
			h = GetDlgItem(hdlg, IDC_EDIT_HD_SPT);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_EDIT_HD_HPC);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_EDIT_HD_CYL);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_EDIT_HD_SIZE);
			EnableWindow(h, FALSE);
			h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
			EnableWindow(h, FALSE);
			chs_enabled = 0;
		} else {
			chs_enabled = 1;
		}

		disk_add_locations(hdlg);

		h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
#ifdef USE_REMOVABLE_DISK
		/* Will leave this for now, we may have removables again! */
		if (existing & 2) {
			hdd_ptr->bus = HDD_BUS_SCSI_REMOVABLE;
			max_spt = 99;
			max_hpc = 255;
		} else {
			hdd_ptr->bus = HDD_BUS_IDE;
			max_spt = 63;
			max_hpc = 255;
		}
#else
		hdd_ptr->bus = HDD_BUS_IDE;
		max_spt = 63;
		max_hpc = 255;
#endif
		SendMessage(h, CB_SETCURSEL, hdd_ptr->bus, 0);
		max_tracks = 266305;
		disk_recalc_location_controls(hdlg, 1, 0);

#ifdef USE_REMOVABLE_DISK
		if (existing & 2) {
			/*
			 * If we are being called as a "load image" dialog
			 * for a removable disk, disable the bus selection.
			 */
			EnableWindow(h, FALSE);
			ShowWindow(h, SW_HIDE);

			/* Disable and hide the IDE CHANNEL combo box. */
			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
			EnableWindow(h, FALSE);
			ShowWindow(h, SW_HIDE);

			/* Disable and hide the SCSI ID and LUN combo boxes. */
			h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
			EnableWindow(h, FALSE);
			ShowWindow(h, SW_HIDE);

			h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
			EnableWindow(h, FALSE);
			ShowWindow(h, SW_HIDE);

			/* Set the file name edit box to current name. */
			h = GetDlgItem(hdlg, IDC_EDIT_HD_FILE_NAME);
			SendMessage(h, WM_SETTEXT, 0,
				    (LPARAM)hdd[next_free_id].fn);
		} else {
#endif
			channel = next_free_ide_channel();
			next_free_scsi_id_and_lun(&id, &lun);
			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
			SendMessage(h, CB_SETCURSEL, 0, 0);
			h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
			SendMessage(h, CB_SETCURSEL, id, 0);
			h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
			SendMessage(h, CB_SETCURSEL, lun, 0);
			h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
			SendMessage(h, CB_SETCURSEL, channel, 0);

			new_hdd.bus_id.st506_channel = next_free_binary_channel(&st506_tracking);
			new_hdd.bus_id.esdi_channel = next_free_binary_channel(&esdi_tracking);
			new_hdd.bus_id.ide_channel = channel;
			new_hdd.bus_id.scsi.id = id;
			new_hdd.bus_id.scsi.lun = lun;
#ifdef USE_REMOVABLE_DISK
		}
#endif
		h = GetDlgItem(hdlg, IDC_EDIT_HD_FILE_NAME);
		EnableWindow(h, FALSE);

		h = GetDlgItem(hdlg, IDT_1752);
		EnableWindow(h, FALSE);
		ShowWindow(h, SW_HIDE);

		h = GetDlgItem(hdlg, IDC_PBAR_IMG_CREATE);
		EnableWindow(h, FALSE);
		ShowWindow(h, SW_HIDE);

		no_update = 0;
		return TRUE;

	case WM_COMMAND:
	       	switch (LOWORD(wParam)) {
			case IDOK:
				if (!(existing & 2)) {
					h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
					hdd_ptr->bus = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				}

				/* Do we have a file name? */
				if (wcslen(hd_file_name) == 0) {
#ifdef USE_REMOVABLE_DISK
					/* No, this is OK with removables. */
					if (hdd_ptr->bus == HDD_BUS_SCSI_REMOVABLE)) {
						/* Mark disk added but return empty. */
						hdd_ptr->tracks = 0;
						hdd_ptr->spt = hdd_ptr->hpc = 0;
						memset(hdd_ptr->fn, 0, sizeof(hdd_ptr->fn));
						goto hd_add_ok_common;
					}
#endif
					/* Not allowed, disable disk. */
					hdd_ptr->bus = HDD_BUS_DISABLED;
					settings_msgbox(MBX_ERROR,
						    (wchar_t *)IDS_INV_NAME);
					return TRUE;
				}

				get_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, &i);
				hdd_ptr->spt = (uint8_t)i;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, &i);
				hdd_ptr->hpc = (uint8_t)i;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, &i);
				hdd_ptr->tracks = (uint16_t)i;
				spt = hdd_ptr->spt;
				hpc = hdd_ptr->hpc;
				tracks = hdd_ptr->tracks;

				if (existing & 2) {
#ifdef USE_REMOVABLE_DISK
					if (hdd_ptr->bus == HDD_BUS_SCSI_REMOVABLE) {
						memset(hdd_ptr->prev_fn, 0, sizeof(hdd_ptr->prev_fn));
						wcscpy(hdd_ptr->prev_fn, hdd_ptr->fn);
					}
#endif
				} else {
					switch(hdd_ptr->bus) {
						case HDD_BUS_ST506:
							h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
							hdd_ptr->bus_id.st506_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
							break;

						case HDD_BUS_ESDI:
							h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
							hdd_ptr->bus_id.esdi_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
							break;

						case HDD_BUS_IDE:
							h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
							hdd_ptr->bus_id.ide_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
							break;

						case HDD_BUS_SCSI:
							h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
							hdd_ptr->bus_id.scsi.id = (uint8_t)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
							h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
							hdd_ptr->bus_id.scsi.lun = (uint8_t)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
							break;

						case HDD_BUS_USB:
							break;
					}
				}

				memset(hdd_ptr->fn, 0, sizeof(hdd_ptr->fn));
				wcscpy(hdd_ptr->fn, hd_file_name);

				sector_size = 512;

				if (!(existing & 1) && (wcslen(hd_file_name) > 0)) {
					f = _wfopen(hd_file_name, L"wb");

					if (image_is_hdi(hd_file_name)) {
						if (size >= 0x100000000ll) {
							fclose(f);
							settings_msgbox(MBX_ERROR, (wchar_t *)IDS_3536);
							return TRUE;
						}

						fwrite(&zero, 1, 4, f);			/* 00000000: Zero/unknown */
						fwrite(&zero, 1, 4, f);			/* 00000004: Zero/unknown */
						fwrite(&base, 1, 4, f);			/* 00000008: Offset at which data starts */
						fwrite(&size, 1, 4, f);			/* 0000000C: Full size of the data (32-bit) */
						fwrite(&sector_size, 1, 4, f);		/* 00000010: Sector size in bytes */
						fwrite(&spt, 1, 4, f);			/* 00000014: Sectors per cylinder */
						fwrite(&hpc, 1, 4, f);			/* 00000018: Heads per cylinder */
						fwrite(&tracks, 1, 4, f);		/* 0000001C: Cylinders */

						for (i = 0; i < 0x3f8; i++) {
							fwrite(&zero, 1, 4, f);
						}
					} else if (image_is_hdx(hd_file_name, 0)) {
						if (size > 0xffffffffffffffffll) {
							fclose(f);
							settings_msgbox(MBX_ERROR, (wchar_t *)IDS_3537);
							return TRUE;							
						}

						fwrite(&signature, 1, 8, f);		/* 00000000: Signature */
						fwrite(&size, 1, 8, f);			/* 00000008: Full size of the data (64-bit) */
						fwrite(&sector_size, 1, 4, f);		/* 00000010: Sector size in bytes */
						fwrite(&spt, 1, 4, f);			/* 00000014: Sectors per cylinder */
						fwrite(&hpc, 1, 4, f);			/* 00000018: Heads per cylinder */
						fwrite(&tracks, 1, 4, f);		/* 0000001C: Cylinders */
						fwrite(&zero, 1, 4, f);			/* 00000020: [Translation] Sectors per cylinder */
						fwrite(&zero, 1, 4, f);			/* 00000004: [Translation] Heads per cylinder */
					}

					memset(buf, 0, 512);
					size >>= 9;
					r = (size >> 11) << 11;
					size -= r;
					r >>= 11;

					if (size || r) {
						h = GetDlgItem(hdlg, IDT_1731);
						EnableWindow(h, FALSE);
						ShowWindow(h, SW_HIDE);

						h = GetDlgItem(hdlg, IDC_EDIT_HD_FILE_NAME);
						EnableWindow(h, FALSE);
						ShowWindow(h, SW_HIDE);

						h = GetDlgItem(hdlg, IDC_CFILE);
						EnableWindow(h, FALSE);
						ShowWindow(h, SW_HIDE);

						h = GetDlgItem(hdlg, IDC_PBAR_IMG_CREATE);
						SendMessage(h, PBM_SETRANGE32, (WPARAM) 0, (LPARAM) (size + r - 1));
						SendMessage(h, PBM_SETPOS, (WPARAM) 0, (LPARAM) 0);
						EnableWindow(h, TRUE);
						ShowWindow(h, SW_SHOW);

						h = GetDlgItem(hdlg, IDT_1752);
						EnableWindow(h, TRUE);
						ShowWindow(h, SW_SHOW);
					}

					if (size) {
						for (i = 0; i < size; i++) {
							fwrite(buf, 1, 512, f);
							SendMessage(h, PBM_SETPOS, (WPARAM) i, (LPARAM) 0);
						}
					}

					if (r) {
						big_buf = (char *)mem_alloc(1048576);
						memset(big_buf, 0, 1048576);
						for (i = 0; i < r; i++) {
							fwrite(big_buf, 1, 1048576, f);
							SendMessage(h, PBM_SETPOS, (WPARAM) (size + i), (LPARAM) 0);
						}
						free(big_buf);
					}

					fclose(f);
					settings_msgbox(MBX_INFO, (wchar_t *)IDS_3537);
				}

#ifdef USE_REMOVABLE_DISK
hd_add_ok_common:
#endif
				hard_disk_added = 1;
				EndDialog(hdlg, 0);
				return TRUE;

			case IDCANCEL:
				hard_disk_added = 0;
				if (!(existing & 2)) {
					hdd_ptr->bus = HDD_BUS_DISABLED;
				}
				EndDialog(hdlg, 0);
				return TRUE;

			case IDC_CFILE:
				memset(hd_file_name, 0x00, sizeof(hd_file_name));
				b = (existing&1)?DLG_FILE_LOAD:DLG_FILE_SAVE;
				if (! dlg_file_ex(hdlg, get_string(IDS_3536), NULL, temp_path, b)) {
					return TRUE;
				}

				if (! wcschr(temp_path, L'.')) {
					if (wcslen(temp_path) && (wcslen(temp_path) <= 256)) {
						twcs = &temp_path[wcslen(temp_path)];
						twcs[0] = L'.';
						twcs[1] = L'i';
						twcs[2] = L'm';
						twcs[3] = L'g';
					}
				}

				if (! (existing & 1)) {
					f = _wfopen(temp_path, L"rb");
					if (f != NULL) {
						fclose(f);
						if (settings_msgbox(MBX_QUESTION, (wchar_t *)IDS_IMG_EXIST) != 0) {	/* yes */
							return FALSE;
						}
					}
				}

				f = _wfopen(temp_path, (existing & 1) ? L"rb" : L"wb");
				if (f == NULL) {
hdd_add_file_open_error:
					settings_msgbox(MBX_ERROR, (existing & 1) ? (wchar_t *)IDS_OPEN_READ : (wchar_t *)IDS_OPEN_WRITE);
					return TRUE;
				}
				if (existing & 1) {
					if (image_is_hdi(temp_path) || image_is_hdx(temp_path, 1)) {
						fseeko64(f, 0x10, SEEK_SET);
						fread(&sector_size, 1, 4, f);
						if (sector_size != 512) {
							settings_msgbox(MBX_ERROR, (wchar_t *)IDS_3535);
							fclose(f);
							return TRUE;
						}
						spt = hpc = tracks = 0;
						fread(&spt, 1, 4, f);
						fread(&hpc, 1, 4, f);
						fread(&tracks, 1, 4, f);
					} else {
						fseeko64(f, 0, SEEK_END);
						size = ftello64(f);
						fclose(f);
						if (((size % 17) == 0) && (size <= 142606336)) {
							spt = 17;
							if (size <= 26738688) {
								hpc = 4;
							} else if (((size % 3072) == 0) && (size <= 53477376)) {
								hpc = 6;
							} else {
								for (i = 5; i < 16; i++) {
									if (((size % (i << 9)) == 0) && (size <= ((i * 17) << 19)))
										break;
									if (i == 5)
										i++;
								}
								hpc = i;
							}
						} else {
							spt = 63;
							hpc = 16;
						}

						tracks = ((size >> 9) / hpc) / spt;
					}

					if ((spt > max_spt) || (hpc > max_hpc) || (tracks > max_tracks)) {
						goto hdd_add_file_open_error;
					}
					no_update = 1;

					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
					disk_recalc_selection(hdlg);

					h = GetDlgItem(hdlg, IDC_EDIT_HD_SPT);
					EnableWindow(h, TRUE);
					h = GetDlgItem(hdlg, IDC_EDIT_HD_HPC);
					EnableWindow(h, TRUE);
					h = GetDlgItem(hdlg, IDC_EDIT_HD_CYL);
					EnableWindow(h, TRUE);
					h = GetDlgItem(hdlg, IDC_EDIT_HD_SIZE);
					EnableWindow(h, TRUE);
					h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
					EnableWindow(h, TRUE);

					chs_enabled = 1;

					no_update = 0;
				} else {
					fclose(f);
				}

				h = GetDlgItem(hdlg, IDC_EDIT_HD_FILE_NAME);
				SendMessage(h, WM_SETTEXT, 0, (LPARAM)temp_path);
				wcscpy(hd_file_name, temp_path);

				return TRUE;

			case IDC_EDIT_HD_CYL:
				if (no_update)
					return FALSE;

				no_update = 1;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, &temp);
				if (temp != tracks) {
					tracks = temp;
					size = ((uint64_t) tracks * (uint64_t) hpc * (uint64_t) spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
					disk_recalc_selection(hdlg);
				}

				if (tracks > max_tracks) {
					tracks = max_tracks;
					size = ((uint64_t) tracks * (uint64_t) hpc * (uint64_t) spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, (uint32_t) tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				no_update = 0;
				break;

			case IDC_EDIT_HD_HPC:
				if (no_update)
					return FALSE;

				no_update = 1;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, &temp);
				if (hpc != temp) {
					hpc = temp;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
					disk_recalc_selection(hdlg);
				}

				if (hpc > max_hpc) {
					hpc = max_hpc;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, (uint32_t) hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				no_update = 0;
				break;

			case IDC_EDIT_HD_SPT:
				if (no_update)
					return FALSE;

				no_update = 1;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, &temp);
				if (spt != temp) {
					spt = temp;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
					disk_recalc_selection(hdlg);
				}

				if (spt > max_spt) {
					spt = max_spt;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				no_update = 0;
				break;

			case IDC_EDIT_HD_SIZE:
				if (no_update)
					return FALSE;

				no_update = 1;
				get_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, &temp);
				if (temp != (size >> 20)) {
					size = temp << 20;
					tracks = ((size >> 9) / hpc) / spt;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					disk_recalc_selection(hdlg);
				}

				if (tracks > max_tracks) {
					tracks = max_tracks;
					size = (tracks * hpc * spt) << 9;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				no_update = 0;
				break;

			case IDC_COMBO_HD_TYPE:
				if (no_update)
					return FALSE;

				no_update = 1;
				get_combo_box_selection(hdlg, IDC_COMBO_HD_TYPE, &temp);
				if ((temp != selection) && (temp != 127) && (temp != 128)) {
					selection = temp;
					tracks = hdd_table[selection].cyls;
					hpc = hdd_table[selection].head;
					spt = hdd_table[selection].sect;
					size = (tracks * hpc * spt) << 9;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
				} else if ((temp != selection) && (temp == 127)) {
					selection = temp;
				} else if ((temp != selection) && (temp == 128)) {
					selection = temp;
					hpc = 16;
					spt = 63;
					size = (tracks * hpc * spt) << 9;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, size >> 20);
				}

				if (spt > max_spt) {
					spt = max_spt;
					size = (tracks * hpc * spt) << 9;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				if (hpc > max_hpc) {
					hpc = max_hpc;
					size = (tracks * hpc * spt) << 9;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				if (tracks > max_tracks) {
					tracks = max_tracks;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				no_update = 0;
					break;

			case IDC_COMBO_HD_BUS:
				if (no_update)
					return FALSE;

				no_update = 1;
				disk_recalc_location_controls(hdlg, 1, 0);
				h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
				b = (int)SendMessage(h,CB_GETCURSEL,0,0) + 1;
				if (b == hdd_ptr->bus)
					goto hd_add_bus_skip;

				hdd_ptr->bus = b;
				switch(hdd_ptr->bus) {
					case HDD_BUS_DISABLED:
					default:
						max_spt = max_hpc = max_tracks = 0;
						break;

					case HDD_BUS_ST506:
						max_spt = 26;
						max_hpc = 15;
						max_tracks = 1023;
						break;

					case HDD_BUS_ESDI:
						max_spt = 63;
						max_hpc = 16;
						max_tracks = 1023;
						break;

					case HDD_BUS_IDE:
						max_spt = 63;
						max_hpc = 255;
						max_tracks = 266305;
						break;

#ifdef USE_REMOVABLE_DISK
					case HDD_BUS_SCSI_REMOVABLE:
						if (spt == 0) {
							spt = 63;
							size = (tracks * hpc * spt) << 9;
							set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
							set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
						}
						if (hpc == 0) {
							hpc = 16;
							size = (tracks * hpc * spt) << 9;
							set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
							set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
						}
						if (tracks == 0) {
							tracks = 1023;
							size = (tracks * hpc * spt) << 9;
							set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
							set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
						}
						/*FALLTHROUGH*/
#endif

					case HDD_BUS_SCSI:
						max_spt = 99;
						max_hpc = 255;
						max_tracks = 266305;
						break;

					case HDD_BUS_USB:
						break;
				}

				if (! chs_enabled) {
#ifdef USE_REMOVABLE_DISK
					if (hdd_ptr->bus == HDD_BUS_SCSI_REMOVABLE) {
						h = GetDlgItem(hdlg, IDC_EDIT_HD_SPT);
						EnableWindow(h, TRUE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_HPC);
						EnableWindow(h, TRUE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_CYL);
						EnableWindow(h, TRUE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_SIZE);
						EnableWindow(h, TRUE);
						h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
						EnableWindow(h, TRUE);
					} else {
#endif
						h = GetDlgItem(hdlg, IDC_EDIT_HD_SPT);
						EnableWindow(h, FALSE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_HPC);
						EnableWindow(h, FALSE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_CYL);
						EnableWindow(h, FALSE);
						h = GetDlgItem(hdlg, IDC_EDIT_HD_SIZE);
						EnableWindow(h, FALSE);
						h = GetDlgItem(hdlg, IDC_COMBO_HD_TYPE);
						EnableWindow(h, FALSE);
#ifdef USE_REMOVABLE_DISK
					}
#endif
				}

				if (spt > max_spt) {
					spt = max_spt;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SPT, spt);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				if (hpc > max_hpc) {
					hpc = max_hpc;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_HPC, hpc);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

				if (tracks > max_tracks) {
					tracks = max_tracks;
					size = ((uint64_t)tracks * (uint64_t)hpc * (uint64_t)spt) << 9LL;
					set_edit_box_contents(hdlg, IDC_EDIT_HD_CYL, tracks);
					set_edit_box_contents(hdlg, IDC_EDIT_HD_SIZE, (size >> 20));
					disk_recalc_selection(hdlg);
				}

hd_add_bus_skip:
				no_update = 0;
				break;
		}

		return FALSE;
    }

    return FALSE;
}


void
disk_add_open(HWND hwnd, int is_existing)
{
    existing = is_existing;

    hard_disk_added = 0;

    DialogBox(plat_lang_dll(), (LPCWSTR)DLG_CFG_DISK_ADD, hwnd, disk_add_proc);
}


static void
disk_track(uint8_t id)
{
    switch(temp_hdd[id].bus) {
	case HDD_BUS_ST506:
		st506_tracking |= (1ULL << (temp_hdd[id].bus_id.st506_channel << 3));
		break;

	case HDD_BUS_ESDI:
		esdi_tracking |= (1ULL << (temp_hdd[id].bus_id.esdi_channel << 3));
		break;

	case HDD_BUS_IDE:
		ide_tracking |= (1ULL << (temp_hdd[id].bus_id.ide_channel << 3));
		break;

	case HDD_BUS_SCSI:
		scsi_tracking[temp_hdd[id].bus_id.scsi.id] |= (1ULL << (temp_hdd[id].bus_id.scsi.lun << 3));
		break;
    }
}


static void
disk_untrack(uint8_t id)
{
    switch(temp_hdd[id].bus) {
	case HDD_BUS_ST506:
		st506_tracking &= ~(1 << (temp_hdd[id].bus_id.st506_channel << 3));
		break;

	case HDD_BUS_ESDI:
		esdi_tracking &= ~(1 << (temp_hdd[id].bus_id.esdi_channel << 3));
		break;

	case HDD_BUS_IDE:
		ide_tracking &= ~(1 << (temp_hdd[id].bus_id.ide_channel << 3));
		break;

	case HDD_BUS_SCSI:
		scsi_tracking[temp_hdd[id].bus_id.scsi.id] &= ~(1 << (temp_hdd[id].bus_id.scsi.lun << 3));
		break;
    }
}


static void
disk_track_all(void)
{
    int i;

    for (i = 0; i < HDD_NUM; i++)
	disk_track(i);
}


static WIN_RESULT CALLBACK
disk_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h;
    int old_sel = 0;
    int b = 0;
    int assign = 0;

    switch (message) {
	case WM_INITDIALOG:
		ignore_change = 1;

		/*
		 * Normalize the hard disks so that non-disabled hard
		 * disks start from index 0, and so they are contiguous.
		 * This will cause an emulator reset prompt on the first
		 * opening of this category with a messy hard disk list
		 * (which can only happen by manually editing the
		 * configuration file).
		 */
		disk_normalize_list();

		h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
		disk_init_columns(h);
		disk_image_list_init(h);
		disk_recalc_list(h);
		recalc_next_free_id(hdlg);
		disk_add_locations(hdlg);

		if (hd_listview_items > 0) {
			ListView_SetItemState(h, 0, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
			hdlv_current_sel = 0;
			h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
			SendMessage(h, CB_SETCURSEL, temp_hdd[0].bus, 0);
		} else {
			hdlv_current_sel = -1;
		}
		disk_recalc_location_controls(hdlg, 0, 0);
			
		ignore_change = 0;
		return TRUE;

	case WM_NOTIFY:
		if ((hd_listview_items == 0) || ignore_change) return FALSE;

		if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_LIST_HARD_DISKS)) {
			old_sel = hdlv_current_sel;
			h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
			hdlv_current_sel = disk_get_selected(hdlg);
			if (hdlv_current_sel == old_sel)
				return FALSE;

			if (hdlv_current_sel == -1) {
				ignore_change = 1;
				hdlv_current_sel = old_sel;
				ListView_SetItemState(h, hdlv_current_sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
				ignore_change = 0;
				return FALSE;
			}

			ignore_change = 1;
			h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
			SendMessage(h, CB_SETCURSEL, temp_hdd[hdlv_current_sel].bus, 0);
			disk_recalc_location_controls(hdlg, 0, 0);
		}
		ignore_change = 0;
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_HD_BUS:
				if (ignore_change)
					return FALSE;

				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
				b = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
				if (b == temp_hdd[hdlv_current_sel].bus)
					goto hd_bus_skip;
				disk_untrack(hdlv_current_sel);
				assign = (temp_hdd[hdlv_current_sel].bus == b) ? 0 : 1;
				if ((b == HDD_BUS_IDE) && (temp_hdd[hdlv_current_sel].bus == HDD_BUS_IDE))
					assign = 0;
				temp_hdd[hdlv_current_sel].bus = b;
				disk_recalc_location_controls(hdlg, 0, assign);
				disk_track(hdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_update_item(h, hdlv_current_sel, 0);
hd_bus_skip:
				ignore_change = 0;
				return FALSE;

			case IDC_COMBO_HD_CHANNEL:
				if (ignore_change)
					return FALSE;

				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL);
				disk_untrack(hdlv_current_sel);
				if (temp_hdd[hdlv_current_sel].bus == HDD_BUS_ST506)
					temp_hdd[hdlv_current_sel].bus_id.st506_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				else if (temp_hdd[hdlv_current_sel].bus == HDD_BUS_ESDI)
					temp_hdd[hdlv_current_sel].bus_id.esdi_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				disk_track(hdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_update_item(h, hdlv_current_sel, 0);
				ignore_change = 0;
				return FALSE;

			case IDC_COMBO_HD_CHANNEL_IDE:
				if (ignore_change)
					return FALSE;

				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_HD_CHANNEL_IDE);
				disk_untrack(hdlv_current_sel);
				temp_hdd[hdlv_current_sel].bus_id.ide_channel = (int)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				disk_track(hdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_update_item(h, hdlv_current_sel, 0);
				ignore_change = 0;
				return FALSE;

			case IDC_COMBO_HD_ID:
				if (ignore_change)
					return FALSE;

				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_HD_ID);
				disk_untrack(hdlv_current_sel);
				temp_hdd[hdlv_current_sel].bus_id.scsi.id = (uint8_t)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				disk_track(hdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_update_item(h, hdlv_current_sel, 0);
				ignore_change = 0;
				return FALSE;

			case IDC_COMBO_HD_LUN:
				if (ignore_change)
					return FALSE;

				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_HD_LUN);
				disk_untrack(hdlv_current_sel);
				temp_hdd[hdlv_current_sel].bus_id.scsi.lun = (uint8_t)SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				disk_track(hdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_update_item(h, hdlv_current_sel, 0);
				ignore_change = 0;
				return FALSE;

			case IDC_BUTTON_HDD_ADD:
				disk_add_open(hdlg, 1);
				if (hard_disk_added) {
					ignore_change = 1;
					h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
					disk_recalc_list(h);
					recalc_next_free_id(hdlg);
					disk_track_all();
					ignore_change = 0;
				}
				return FALSE;

			case IDC_BUTTON_HDD_ADD_NEW:
				disk_add_open(hdlg, 0);
				if (hard_disk_added) {
					ignore_change = 1;
					h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
					disk_recalc_list(h);
					recalc_next_free_id(hdlg);
					disk_track_all();
					ignore_change = 0;
				}
				return FALSE;

			case IDC_BUTTON_HDD_REMOVE:
				wcscpy(temp_hdd[hdlv_current_sel].fn, L"");
				disk_untrack(hdlv_current_sel);

				/*
				 * Only set the bus to zero, the list normalize
				 * code below will take care of turning this
				 * entire entry to a complete zero.
				 */
				temp_hdd[hdlv_current_sel].bus = HDD_BUS_DISABLED;
				/*
				 * Normalize the hard disks so that all
				 * non-disabled hard disks start from index 0,
				 * and so they are contiguous.
				 */
				disk_normalize_list();
				ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_LIST_HARD_DISKS);
				disk_recalc_list(h);
				recalc_next_free_id(hdlg);
				if (hd_listview_items > 0) {
					ListView_SetItemState(h, 0, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
					hdlv_current_sel = 0;
					h = GetDlgItem(hdlg, IDC_COMBO_HD_BUS);
					SendMessage(h, CB_SETCURSEL, temp_hdd[0].bus, 0);
				} else {
					hdlv_current_sel = -1;
				}
				disk_recalc_location_controls(hdlg, 0, 0);
				ignore_change = 0;
				return FALSE;
		}
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
