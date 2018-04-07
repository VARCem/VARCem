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
 * Version:	@(#)win_settings_remov.h	1.0.1	2018/04/05
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
 *			  Removable Devices Dialog			*
 *									*
 ************************************************************************/

/* GLobal variables needed for the Removable Devices dialog. */
static int rd_ignore_change = 0;
static int cdlv_current_sel;
static int zdlv_current_sel;

static int
combo_to_string(int combo_id)
{
    return IDS_5376 + combo_id;
}


static int
combo_to_format(int combo_id)
{
    return IDS_5632 + combo_id;
}


static void
cdrom_image_list_init(HWND hwndList)
{
    HICON hiconItem;
    HIMAGELIST hSmall;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);

    hiconItem = LoadIcon(hinstance, (LPCWSTR)514);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    hiconItem = LoadIcon(hinstance, (LPCWSTR)160);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
}


static BOOL
cdrom_recalc_list(HWND hwndList)
{
    WCHAR temp[128];
    LVITEM lvI;
    int fsid, i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i = 0; i < 4; i++) {
	fsid = combo_to_format(temp_cdrom_drives[i].bus_type);

	lvI.iSubItem = 0;
	switch (temp_cdrom_drives[i].bus_type) {
		case CDROM_BUS_DISABLED:
		default:
			lvI.pszText = plat_get_string(fsid);
			lvI.iImage = 0;
			break;

		case CDROM_BUS_ATAPI_PIO_ONLY:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].ide_channel >> 1, temp_cdrom_drives[i].ide_channel & 1);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;

		case CDROM_BUS_ATAPI_PIO_AND_DMA:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].ide_channel >> 1, temp_cdrom_drives[i].ide_channel & 1);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;

		case CDROM_BUS_SCSI:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].scsi_device_id, temp_cdrom_drives[i].scsi_device_lun);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;
	}
	lvI.iItem = i;
	if (ListView_InsertItem(hwndList, &lvI) == -1)
		return FALSE;

	lvI.iSubItem = 1;
	if (temp_cdrom_drives[i].bus_type == CDROM_BUS_DISABLED) {
			lvI.pszText = plat_get_string(IDS_2152);
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
cdrom_init_columns(HWND hwndList)
{
    LVCOLUMN lvc;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = plat_get_string(IDS_2082);
    lvc.cx = 342;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 0, &lvc) == -1)
	return FALSE;

    lvc.iSubItem = 1;
    lvc.pszText = plat_get_string(IDS_2179);
    lvc.cx = 50;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 1, &lvc) == -1)
	return FALSE;

    return TRUE;
}


static int
cdrom_get_selected(HWND hdlg)
{
    int drive = -1;
    int i, j = 0;
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
cdrom_update_item(HWND hwndList, int i)
{
    WCHAR temp[128];
    LVITEM lvI;
    int fsid;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    lvI.iSubItem = 0;
    lvI.iItem = i;

    fsid = combo_to_format(temp_cdrom_drives[i].bus_type);

    switch (temp_cdrom_drives[i].bus_type) {
	case CDROM_BUS_DISABLED:
	default:
		lvI.pszText = plat_get_string(fsid);
		lvI.iImage = 0;
		break;

	case CDROM_BUS_ATAPI_PIO_ONLY:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].ide_channel >> 1, temp_cdrom_drives[i].ide_channel & 1);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;

	case CDROM_BUS_ATAPI_PIO_AND_DMA:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].ide_channel >> 1, temp_cdrom_drives[i].ide_channel & 1);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;

	case CDROM_BUS_SCSI:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_cdrom_drives[i].scsi_device_id, temp_cdrom_drives[i].scsi_device_lun);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;
    }
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;

    lvI.iSubItem = 1;
    if (temp_cdrom_drives[i].bus_type == CDROM_BUS_DISABLED) {
	lvI.pszText = plat_get_string(IDS_2152);
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
cdrom_add_locations(HWND hdlg)
{
    WCHAR temp[128];
    HWND h;
    int i = 0;

    h = GetDlgItem(hdlg, IDC_COMBO_CD_BUS);
    for (i = CDROM_BUS_DISABLED; i <= CDROM_BUS_SCSI; i++) {
	if ((i == CDROM_BUS_DISABLED) || (i >= CDROM_BUS_ATAPI_PIO_ONLY)) {
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(combo_to_string(i)));
	}
    }

    /* Create a list of usable CD-ROM speeds. */
    h = GetDlgItem(hdlg, IDC_COMBO_CD_SPEED);
    for (i = 0; cdrom_speeds[i].speed > 0; i++) {
	wsprintf(temp, L"%ix", cdrom_speeds[i]);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
    for (i = 0; i < 16; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4098), i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4098), i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4097), i >> 1, i & 1);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}


static void
cdrom_recalc_location_controls(HWND hdlg, int assign_id)
{
    HWND h;
    int i;
    int bus = temp_cdrom_drives[cdlv_current_sel].bus_type;

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
	SendMessage(h, CB_SETCURSEL, temp_cdrom_drives[cdlv_current_sel].speed_idx, 0);
    }

    switch(bus) {
	case CDROM_BUS_ATAPI_PIO_ONLY:		/* ATAPI (PIO-only) */
	case CDROM_BUS_ATAPI_PIO_AND_DMA:	/* ATAPI (PIO and DMA) */
		h = GetDlgItem(hdlg, IDT_1743);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		if (assign_id)
			temp_cdrom_drives[cdlv_current_sel].ide_channel = next_free_ide_channel();

		h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_cdrom_drives[cdlv_current_sel].ide_channel, 0);
		break;

	case CDROM_BUS_SCSI:			/* SCSI */
		h = GetDlgItem(hdlg, IDT_1741);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		h = GetDlgItem(hdlg, IDT_1742);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		if (assign_id)
			next_free_scsi_id_and_lun((uint8_t *) &temp_cdrom_drives[cdlv_current_sel].scsi_device_id, (uint8_t *) &temp_cdrom_drives[cdlv_current_sel].scsi_device_lun);

		h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_cdrom_drives[cdlv_current_sel].scsi_device_id, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_cdrom_drives[cdlv_current_sel].scsi_device_lun, 0);
		break;
    }
}


static void
cdrom_track(uint8_t id)
{
    if ((temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI_PIO_ONLY) || (temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI_PIO_ONLY))
	ide_tracking |= (2 << (temp_cdrom_drives[id].ide_channel << 3));
    else if (temp_cdrom_drives[id].bus_type == CDROM_BUS_SCSI)
	scsi_tracking[temp_cdrom_drives[id].scsi_device_id] |= (1ULL << temp_cdrom_drives[id].scsi_device_lun);
}


static void
cdrom_untrack(uint8_t id)
{
    if ((temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI_PIO_ONLY) || (temp_cdrom_drives[id].bus_type == CDROM_BUS_ATAPI_PIO_ONLY))
	ide_tracking &= ~(2 << (temp_cdrom_drives[id].ide_channel << 3));
    else if (temp_cdrom_drives[id].bus_type == CDROM_BUS_SCSI)
	scsi_tracking[temp_cdrom_drives[id].scsi_device_id] &= ~(1ULL << temp_cdrom_drives[id].scsi_device_lun);
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


static void
zip_image_list_init(HWND hwndList)
{
    HICON hiconItem;
    HIMAGELIST hSmall;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);

    hiconItem = LoadIcon(hinstance, (LPCWSTR)515);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    hiconItem = LoadIcon(hinstance, (LPCWSTR)176);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
}


static BOOL
zip_recalc_list(HWND hwndList)
{
    WCHAR temp[256];
    LVITEM lvI;
    int fsid, i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i = 0; i < 4; i++) {
	fsid = combo_to_format(temp_zip_drives[i].bus_type);

	lvI.iSubItem = 0;
	switch (temp_zip_drives[i].bus_type) {
		case ZIP_BUS_DISABLED:
		default:
			lvI.pszText = plat_get_string(fsid);
			lvI.iImage = 0;
			break;

		case ZIP_BUS_ATAPI_PIO_ONLY:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].ide_channel >> 1, temp_zip_drives[i].ide_channel & 1);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;

		case ZIP_BUS_ATAPI_PIO_AND_DMA:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].ide_channel >> 1, temp_zip_drives[i].ide_channel & 1);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;

		case ZIP_BUS_SCSI:
			swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].scsi_device_id, temp_zip_drives[i].scsi_device_lun);
			lvI.pszText = temp;
			lvI.iImage = 1;
			break;
	}
	lvI.iItem = i;
	if (ListView_InsertItem(hwndList, &lvI) == -1)
		return FALSE;

	lvI.iSubItem = 1;
	lvI.pszText = plat_get_string(temp_zip_drives[i].is_250 ? IDS_5901 : IDS_5900);
	lvI.iItem = i;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1)
		return FALSE;
    }

    return TRUE;
}


static BOOL
zip_init_columns(HWND hwndList)
{
    LVCOLUMN lvc;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = plat_get_string(IDS_2082);
    lvc.cx = 342;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 0, &lvc) == -1)
	return FALSE;

    lvc.iSubItem = 1;
    lvc.pszText = plat_get_string(IDS_2143);
    lvc.cx = 50;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 1, &lvc) == -1)
	return FALSE;

    return TRUE;
}


static int
zip_get_selected(HWND hdlg)
{
    int drive = -1;
    int i, j = 0;
    HWND h;

    for (i = 0; i < 6; i++) {
	h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
	j = ListView_GetItemState(h, i, LVIS_SELECTED);
	if (j)
		drive = i;
    }

    return drive;
}


static void
zip_update_item(HWND hwndList, int i)
{
    WCHAR temp[128];
    LVITEM lvI;
    int fsid;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    lvI.iSubItem = 0;
    lvI.iItem = i;

    fsid = combo_to_format(temp_zip_drives[i].bus_type);

    switch (temp_zip_drives[i].bus_type) {
	case ZIP_BUS_DISABLED:
	default:
		lvI.pszText = plat_get_string(fsid);
		lvI.iImage = 0;
		break;

	case ZIP_BUS_ATAPI_PIO_ONLY:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].ide_channel >> 1, temp_zip_drives[i].ide_channel & 1);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;

	case ZIP_BUS_ATAPI_PIO_AND_DMA:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].ide_channel >> 1, temp_zip_drives[i].ide_channel & 1);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;

	case ZIP_BUS_SCSI:
		swprintf(temp, sizeof_w(temp), plat_get_string(fsid), temp_zip_drives[i].scsi_device_id, temp_zip_drives[i].scsi_device_lun);
		lvI.pszText = temp;
		lvI.iImage = 1;
		break;
    }
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;

    lvI.iSubItem = 1;
    lvI.pszText = plat_get_string(temp_zip_drives[i].is_250 ? IDS_5901 : IDS_5900);
    lvI.iItem = i;
    lvI.iImage = 0;
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;
}


static void
zip_add_locations(HWND hdlg)
{
    WCHAR temp[128];
    HWND h;
    int i;

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_BUS);
    for (i = ZIP_BUS_DISABLED; i <= ZIP_BUS_SCSI; i++) {
	if ((i == ZIP_BUS_DISABLED) || (i >= ZIP_BUS_ATAPI_PIO_ONLY)) {
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)plat_get_string(combo_to_string(i)));
	}
    }

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_ID);
    for (i = 0; i < 16; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4098), i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_LUN);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4098), i);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_CHANNEL_IDE);
    for (i = 0; i < 8; i++) {
	swprintf(temp, sizeof_w(temp), plat_get_string(IDS_4097), i >> 1, i & 1);
	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}


static void
zip_recalc_location_controls(HWND hdlg, int assign_id)
{
    HWND h;
    int i;
    int bus = temp_zip_drives[zdlv_current_sel].bus_type;

    for (i = IDT_1754; i < (IDT_1756 + 1); i++) {
	h = GetDlgItem(hdlg, i);
	EnableWindow(h, FALSE);
	ShowWindow(h, SW_HIDE);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_ID);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_LUN);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_ZIP_CHANNEL_IDE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    switch(bus) {
	case ZIP_BUS_ATAPI_PIO_ONLY:		/* ATAPI (PIO-only) */
	case ZIP_BUS_ATAPI_PIO_AND_DMA:		/* ATAPI (PIO and DMA) */
		h = GetDlgItem(hdlg, IDT_1756);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);

		if (assign_id)
			temp_zip_drives[zdlv_current_sel].ide_channel = next_free_ide_channel();

		h = GetDlgItem(hdlg, IDC_COMBO_ZIP_CHANNEL_IDE);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_zip_drives[zdlv_current_sel].ide_channel, 0);
		break;

	case ZIP_BUS_SCSI:			/* SCSI */
		h = GetDlgItem(hdlg, IDT_1754);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		h = GetDlgItem(hdlg, IDT_1755);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		if (assign_id)
			next_free_scsi_id_and_lun((uint8_t *) &temp_zip_drives[zdlv_current_sel].scsi_device_id, (uint8_t *) &temp_zip_drives[zdlv_current_sel].scsi_device_lun);

		h = GetDlgItem(hdlg, IDC_COMBO_ZIP_ID);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_zip_drives[zdlv_current_sel].scsi_device_id, 0);

		h = GetDlgItem(hdlg, IDC_COMBO_ZIP_LUN);
		ShowWindow(h, SW_SHOW);
		EnableWindow(h, TRUE);
		SendMessage(h, CB_SETCURSEL, temp_zip_drives[zdlv_current_sel].scsi_device_lun, 0);
		break;
    }
}


static void
zip_track(uint8_t id)
{
    if ((temp_zip_drives[id].bus_type == ZIP_BUS_ATAPI_PIO_ONLY) || (temp_zip_drives[id].bus_type == ZIP_BUS_ATAPI_PIO_ONLY))
	ide_tracking |= (1ULL << temp_zip_drives[id].ide_channel);
    else if (temp_zip_drives[id].bus_type == ZIP_BUS_SCSI)
	scsi_tracking[temp_zip_drives[id].scsi_device_id] |= (1ULL << temp_zip_drives[id].scsi_device_lun);
}


static void
zip_untrack(uint8_t id)
{
    if ((temp_zip_drives[id].bus_type == ZIP_BUS_ATAPI_PIO_ONLY) || (temp_zip_drives[id].bus_type == ZIP_BUS_ATAPI_PIO_ONLY))
	ide_tracking &= ~(1ULL << temp_zip_drives[id].ide_channel);
    else if (temp_zip_drives[id].bus_type == ZIP_BUS_SCSI)
	scsi_tracking[temp_zip_drives[id].scsi_device_id] &= ~(1ULL << temp_zip_drives[id].scsi_device_lun);
}


#if 0
static void
zip_track_all(void)
{
    int i;

    for (i = 0; i < ZIP_NUM; i++)
	zip_track(i);
}
#endif


#ifdef __amd64__
static LRESULT CALLBACK
#else
static BOOL CALLBACK
#endif
rmv_devices_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h = INVALID_HANDLE_VALUE;
    int old_sel = 0;
    int b = 0;
    int b2 = 0;
    int assign = 0;

    switch (message) {
	case WM_INITDIALOG:
		rd_ignore_change = 1;

		cdlv_current_sel = 0;
		h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
		cdrom_init_columns(h);
		cdrom_image_list_init(h);
		cdrom_recalc_list(h);
		ListView_SetItemState(h, 0, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
		cdrom_add_locations(hdlg);
		h = GetDlgItem(hdlg, IDC_COMBO_CD_BUS);
		switch (temp_cdrom_drives[cdlv_current_sel].bus_type) {
			case CDROM_BUS_DISABLED:
			default:
				b = 0;
				break;

			case CDROM_BUS_ATAPI_PIO_ONLY:
				b = 1;
				break;

			case CDROM_BUS_ATAPI_PIO_AND_DMA:
				b = 2;
				break;

			case CDROM_BUS_SCSI:
				b = 3;
				break;
		}
		SendMessage(h, CB_SETCURSEL, b, 0);
		cdrom_recalc_location_controls(hdlg, 0);

		zdlv_current_sel = 0;
		h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
		zip_init_columns(h);
		zip_image_list_init(h);
		zip_recalc_list(h);
		zip_add_locations(hdlg);
		h = GetDlgItem(hdlg, IDC_COMBO_ZIP_BUS);
		switch (temp_zip_drives[zdlv_current_sel].bus_type) {
			case ZIP_BUS_DISABLED:
			default:
				b = 0;
				break;

			case ZIP_BUS_ATAPI_PIO_ONLY:
				b = 1;
				break;

			case ZIP_BUS_ATAPI_PIO_AND_DMA:
				b = 2;
				break;

			case ZIP_BUS_SCSI:
				b = 3;
				break;
		}
		SendMessage(h, CB_SETCURSEL, b, 0);
		zip_recalc_location_controls(hdlg, 0);
		h = GetDlgItem(hdlg, IDC_CHECK250);
		SendMessage(h, BM_SETCHECK, temp_zip_drives[zdlv_current_sel].is_250, 0);
		rd_ignore_change = 0;
		return TRUE;

	case WM_NOTIFY:
		if (rd_ignore_change)
			return FALSE;

		if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_LIST_CDROM_DRIVES)) {
			old_sel = cdlv_current_sel;
			cdlv_current_sel = cdrom_get_selected(hdlg);
			if (cdlv_current_sel == old_sel)
				return FALSE;
			if (cdlv_current_sel == -1) {
				rd_ignore_change = 1;
				cdlv_current_sel = old_sel;
				ListView_SetItemState(h, cdlv_current_sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
				rd_ignore_change = 0;
				return FALSE;
			}
			rd_ignore_change = 1;

			h = GetDlgItem(hdlg, IDC_COMBO_CD_BUS);
			switch (temp_cdrom_drives[cdlv_current_sel].bus_type) {
				case CDROM_BUS_DISABLED:
				default:
					b = 0;
					break;
				case CDROM_BUS_ATAPI_PIO_ONLY:
					b = 1;
					break;
				case CDROM_BUS_ATAPI_PIO_AND_DMA:
					b = 2;
					break;
				case CDROM_BUS_SCSI:
					b = 3;
					break;
			}
			SendMessage(h, CB_SETCURSEL, b, 0);
			cdrom_recalc_location_controls(hdlg, 0);
		} else if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_LIST_ZIP_DRIVES)) {
			old_sel = zdlv_current_sel;
			zdlv_current_sel = zip_get_selected(hdlg);
			if (zdlv_current_sel == old_sel)
				return FALSE;

			if (zdlv_current_sel == -1) {
				rd_ignore_change = 1;
				zdlv_current_sel = old_sel;
				ListView_SetItemState(h, zdlv_current_sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
				rd_ignore_change = 0;
				return FALSE;
			}
			rd_ignore_change = 1;

			h = GetDlgItem(hdlg, IDC_COMBO_ZIP_BUS);
			switch (temp_zip_drives[zdlv_current_sel].bus_type) {
				case ZIP_BUS_DISABLED:
				default:
					b = 0;
					break;

				case ZIP_BUS_ATAPI_PIO_ONLY:
					b = 1;
					break;

				case ZIP_BUS_ATAPI_PIO_AND_DMA:
					b = 2;
					break;

				case ZIP_BUS_SCSI:
					b = 3;
					break;
			}
			SendMessage(h, CB_SETCURSEL, b, 0);
			zip_recalc_location_controls(hdlg, 0);
			h = GetDlgItem(hdlg, IDC_CHECK250);
			SendMessage(h, BM_SETCHECK, temp_zip_drives[zdlv_current_sel].is_250, 0);
		}
		rd_ignore_change = 0;
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_CD_BUS:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_CD_BUS);
				b = SendMessage(h, CB_GETCURSEL, 0, 0);
				switch (b) {
					case 0:
						b2 = CDROM_BUS_DISABLED;
						break;

					case 1:
						b2 = CDROM_BUS_ATAPI_PIO_ONLY;
						break;

					case 2:
						b2 = CDROM_BUS_ATAPI_PIO_AND_DMA;
						break;

					case 3:
						b2 = CDROM_BUS_SCSI;
						break;
				}
				if (b2 == temp_cdrom_drives[cdlv_current_sel].bus_type)
					goto cdrom_bus_skip;
				cdrom_untrack(cdlv_current_sel);
				assign = (temp_cdrom_drives[cdlv_current_sel].bus_type == b2) ? 0 : 1;
				if (temp_cdrom_drives[cdlv_current_sel].bus_type == CDROM_BUS_DISABLED)
					temp_cdrom_drives[cdlv_current_sel].speed_idx = cdrom_speed_idx(cdrom_speed_idx(CDROM_SPEED_DEFAULT));
				if ((b2 == CDROM_BUS_ATAPI_PIO_ONLY) && (temp_cdrom_drives[cdlv_current_sel].bus_type == CDROM_BUS_ATAPI_PIO_AND_DMA))
					assign = 0;
				else if ((b2 == CDROM_BUS_ATAPI_PIO_AND_DMA) && (temp_cdrom_drives[cdlv_current_sel].bus_type == CDROM_BUS_ATAPI_PIO_ONLY))
					assign = 0;
				temp_cdrom_drives[cdlv_current_sel].bus_type = b2;
				cdrom_recalc_location_controls(hdlg, assign);
				cdrom_track(cdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
				cdrom_update_item(h, cdlv_current_sel);
cdrom_bus_skip:
					rd_ignore_change = 0;
					return FALSE;

			case IDC_COMBO_CD_ID:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_CD_ID);
				cdrom_untrack(cdlv_current_sel);
				temp_cdrom_drives[cdlv_current_sel].scsi_device_id = SendMessage(h, CB_GETCURSEL, 0, 0);
				cdrom_track(cdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
				cdrom_update_item(h, cdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_CD_LUN:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_CD_LUN);
				cdrom_untrack(cdlv_current_sel);
				temp_cdrom_drives[cdlv_current_sel].scsi_device_lun = SendMessage(h, CB_GETCURSEL, 0, 0);
				cdrom_track(cdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
				cdrom_update_item(h, cdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_CD_CHANNEL_IDE:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_CD_CHANNEL_IDE);
				cdrom_untrack(cdlv_current_sel);
				temp_cdrom_drives[cdlv_current_sel].ide_channel = SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				cdrom_track(cdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
				cdrom_update_item(h, cdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_CD_SPEED:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_CD_SPEED);
				temp_cdrom_drives[cdlv_current_sel].speed_idx = (uint8_t)SendMessage(h, CB_GETCURSEL, 0, 0);
				h = GetDlgItem(hdlg, IDC_LIST_CDROM_DRIVES);
				cdrom_update_item(h, cdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_ZIP_BUS:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_ZIP_BUS);
				b = SendMessage(h, CB_GETCURSEL, 0, 0);
				switch (b) {
					case 0:
						b2 = ZIP_BUS_DISABLED;
						break;
					case 1:
						b2 = ZIP_BUS_ATAPI_PIO_ONLY;
						break;
					case 2:
						b2 = ZIP_BUS_ATAPI_PIO_AND_DMA;
						break;
					case 3:
						b2 = ZIP_BUS_SCSI;
						break;
				}
				if (b2 == temp_zip_drives[zdlv_current_sel].bus_type)
					goto zip_bus_skip;
				zip_untrack(zdlv_current_sel);
				assign = (temp_zip_drives[zdlv_current_sel].bus_type == b2) ? 0 : 1;
				if ((b2 == ZIP_BUS_ATAPI_PIO_ONLY) && (temp_zip_drives[zdlv_current_sel].bus_type == ZIP_BUS_ATAPI_PIO_AND_DMA))
					assign = 0;
				else if ((b2 == ZIP_BUS_ATAPI_PIO_AND_DMA) && (temp_zip_drives[cdlv_current_sel].bus_type == ZIP_BUS_ATAPI_PIO_ONLY))
					assign = 0;
				temp_zip_drives[zdlv_current_sel].bus_type = b2;
				zip_recalc_location_controls(hdlg, assign);
				zip_track(zdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
				zip_update_item(h, zdlv_current_sel);
zip_bus_skip:
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_ZIP_ID:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_ZIP_ID);
				zip_untrack(zdlv_current_sel);
				temp_zip_drives[zdlv_current_sel].scsi_device_id = SendMessage(h, CB_GETCURSEL, 0, 0);
				zip_track(zdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
				zip_update_item(h, zdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_ZIP_LUN:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_ZIP_LUN);
				zip_untrack(zdlv_current_sel);
				temp_zip_drives[zdlv_current_sel].scsi_device_lun = SendMessage(h, CB_GETCURSEL, 0, 0);
				zip_track(zdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
				zip_update_item(h, zdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_COMBO_ZIP_CHANNEL_IDE:
				if (rd_ignore_change)
					return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_ZIP_CHANNEL_IDE);
				zip_untrack(zdlv_current_sel);
				temp_zip_drives[zdlv_current_sel].ide_channel = SendMessage(h, CB_GETCURSEL, 0, 0) & 0xff;
				zip_track(zdlv_current_sel);
				h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
				zip_update_item(h, zdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;

			case IDC_CHECK250:
				if (rd_ignore_change)
						return FALSE;

				rd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_CHECK250);
				temp_zip_drives[zdlv_current_sel].is_250 = SendMessage(h, BM_GETCHECK, 0, 0);
				h = GetDlgItem(hdlg, IDC_LIST_ZIP_DRIVES);
				zip_update_item(h, zdlv_current_sel);
				rd_ignore_change = 0;
				return FALSE;
		}
		return FALSE;

	default:
		break;
    }

    return FALSE;
}
