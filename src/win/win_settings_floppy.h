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
 * Version:	@(#)win_settings_floppy.h	1.0.13	2021/02/06
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
 *			       Floppy Dialog				*
 *									*
 ************************************************************************/

/* GLobal variables needed for the Floppy dialog. */
static int fd_ignore_change = 0;
static int fdlv_current_sel;
static int	floppy_to_list[20],
			list_to_floppy[20];

int
floppy_is_valid(int num, int nm)
{
    const device_t *dev;
    const machine_t *m;
	int floppy_flags;

    /* Get info about the selected machine. */
	dev = machine_get_device_ex(nm);
    m = (machine_t *)dev->mach_info;

	/* Filter only PS/2 flagged floppy */
    floppy_flags = fdd_get_flags_from_type(num);

    if ((floppy_flags & FLAG_PS2) && (!(m->flags & MACHINE_FDC_PS2))) return(0);
	if (!(floppy_flags & FLAG_PS2) && ((m->flags & MACHINE_FDC_PS2))) return(0);

    return(1);
} 

static void
floppy_image_list_init(HWND hwndList)
{
    HICON hIconItem;
    HIMAGELIST hSmall;
    int i;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
			      GetSystemMetrics(SM_CYSMICON),
			      ILC_MASK | ILC_COLOR32, 1, 1);

    for (i = 0; i < 14; i++) {
#if (defined(_MSC_VER) && defined(_M_X64)) || \
    (defined(__GNUC__) && defined(__amd64__))
	hIconItem = LoadIcon(hInstance, (LPCWSTR)((uint64_t)ui_fdd_icon(i)));
#else
	hIconItem = LoadIcon(hInstance, (LPCWSTR)((uint32_t)ui_fdd_icon(i)));
#endif
	ImageList_AddIcon(hSmall, hIconItem);
	DestroyIcon(hIconItem);
    }

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
}


static BOOL
floppy_recalc_list(HWND hwndList)
{
    WCHAR temp[128];
    char tempA[128];
    LVITEM lvI;
    int i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.state = 0;

    for (i = 0; i < 4; i++) {
	lvI.iSubItem = 0;
	if (temp_fdd_types[i] > 0) {
		strcpy(tempA, fdd_getname(temp_fdd_types[i]));
		mbstowcs(temp, tempA, sizeof_w(temp));
		lvI.pszText = temp;
	} else {
		lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
	}
	lvI.iItem = i;
	lvI.iImage = temp_fdd_types[i];
	if (ListView_InsertItem(hwndList, &lvI) == -1)
		return FALSE;

	lvI.iSubItem = 1;
	lvI.pszText = (LPTSTR)get_string(temp_fdd_turbo[i] ? IDS_ON : IDS_OFF);
	lvI.iItem = i;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1)
		return FALSE;

	lvI.iSubItem = 2;
	lvI.pszText = (LPTSTR)get_string(temp_fdd_check_bpb[i] ? IDS_ON : IDS_OFF);
	lvI.iItem = i;
	lvI.iImage = 0;
	if (ListView_SetItem(hwndList, &lvI) == -1)
		return FALSE;
    }

    return TRUE;
}


static BOOL
floppy_init_columns(HWND hwndList)
{
    LVCOLUMN lvc;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = (LPTSTR)get_string(IDS_TYPE);		// "Type"
    lvc.cx = 292;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 0, &lvc) == -1)
	return FALSE;

    lvc.iSubItem = 1;
    lvc.pszText = (LPTSTR)get_string(IDS_3554);		// "Turbo"
    lvc.cx = 50;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 1, &lvc) == -1)
	return FALSE;

    lvc.iSubItem = 2;
    lvc.pszText = (LPTSTR)get_string(IDS_3555);		// "bpb"
    lvc.cx = 75;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 2, &lvc) == -1)
	return FALSE;

    return TRUE;
}


static int
floppy_get_selected(HWND hdlg)
{
    int drive = -1;
    int i, j = 0;
    HWND h;

    for (i = 0; i < 6; i++) {
	h = GetDlgItem(hdlg, IDC_LIST_FLOPPY_DRIVES);
	j = ListView_GetItemState(h, i, LVIS_SELECTED);
	if (j)
		drive = i;
    }

    return drive;
}


static void
floppy_update_item(HWND hwndList, int i)
{
    WCHAR temp[128];
    char tempA[128];
    LVITEM lvI;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    lvI.iSubItem = 0;
    lvI.iItem = i;

    if (temp_fdd_types[i] > 0) {
	strcpy(tempA, fdd_getname(temp_fdd_types[i]));
	mbstowcs(temp, tempA, sizeof_w(temp));
	lvI.pszText = temp;
    } else {
	lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
    }
    lvI.iImage = temp_fdd_types[i];
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;

    lvI.iSubItem = 1;
    lvI.pszText = (LPTSTR)get_string(temp_fdd_turbo[i] ? IDS_ON : IDS_OFF);
    lvI.iItem = i;
    lvI.iImage = 0;
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;

    lvI.iSubItem = 2;
    lvI.pszText = (LPTSTR)get_string(temp_fdd_check_bpb[i] ? IDS_ON : IDS_OFF);
    lvI.iItem = i;
    lvI.iImage = 0;
    if (ListView_SetItem(hwndList, &lvI) == -1)
	return;
}


static WIN_RESULT CALLBACK
floppy_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WCHAR temp[128];
    HWND h = NULL;
    int i;
	int j = 0;
    int old_sel = 0;

    switch (message) {
	case WM_INITDIALOG:
		fd_ignore_change = 1;

		fdlv_current_sel = 0;
		h = GetDlgItem(hdlg, IDC_LIST_FLOPPY_DRIVES);
		floppy_init_columns(h);
		floppy_image_list_init(h);
		floppy_recalc_list(h);
		ListView_SetItemState(h, 0, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
		h = GetDlgItem(hdlg, IDC_COMBO_FD_TYPE);
		i = j = 0;
		for (;;) {
			
			if (fdd_getname(i) == NULL) break;

			/* Skip floppy drives that are unavailable. */
			if (! floppy_is_valid(i, temp_cfg.machine_type)) {
					i++;
					continue;
			}
			if (i == 0) {
				SendMessage(h, CB_ADDSTRING, 0,
					    (LPARAM)get_string(IDS_DISABLED));
			} else {
				mbstowcs(temp, fdd_getname(i), sizeof_w(temp));
				SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
			}
			
			floppy_to_list[i] = j;
			list_to_floppy[j] = i;
			i++; j++;
		}
		SendMessage(h, CB_SETCURSEL, floppy_to_list[temp_fdd_types[fdlv_current_sel]], 0);

		h = GetDlgItem(hdlg, IDC_CHECKTURBO);
		SendMessage(h, BM_SETCHECK, temp_fdd_turbo[fdlv_current_sel], 0);

		h = GetDlgItem(hdlg, IDC_CHECKBPB);
		SendMessage(h, BM_SETCHECK, temp_fdd_check_bpb[fdlv_current_sel], 0);

		fd_ignore_change = 0;
		return TRUE;

	case WM_NOTIFY:
		if (fd_ignore_change)
			return FALSE;

		if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_LIST_FLOPPY_DRIVES)) {
			old_sel = fdlv_current_sel;
			fdlv_current_sel = floppy_get_selected(hdlg);
			if (fdlv_current_sel == old_sel)
				return FALSE;
			if (fdlv_current_sel == -1) {
				fd_ignore_change = 1;
				fdlv_current_sel = old_sel;
				ListView_SetItemState(h, fdlv_current_sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
				fd_ignore_change = 0;
				return FALSE;
			}
			fd_ignore_change = 1;
			h = GetDlgItem(hdlg, IDC_COMBO_FD_TYPE);
			SendMessage(h, CB_SETCURSEL, temp_fdd_types[fdlv_current_sel], 0);
			h = GetDlgItem(hdlg, IDC_CHECKTURBO);
			SendMessage(h, BM_SETCHECK, temp_fdd_turbo[fdlv_current_sel], 0);
			h = GetDlgItem(hdlg, IDC_CHECKBPB);
			SendMessage(h, BM_SETCHECK, temp_fdd_check_bpb[fdlv_current_sel], 0);
			fd_ignore_change = 0;
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDC_COMBO_FD_TYPE:
				if (fd_ignore_change)
					return FALSE;

				fd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_COMBO_FD_TYPE);
				temp_fdd_types[fdlv_current_sel] = list_to_floppy[SendMessage(h, CB_GETCURSEL, 0, 0)];
				h = GetDlgItem(hdlg, IDC_LIST_FLOPPY_DRIVES);
				floppy_update_item(h, fdlv_current_sel);
				fd_ignore_change = 0;
				return FALSE;

			case IDC_CHECKTURBO:
				if (fd_ignore_change)
					return FALSE;

				fd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_CHECKTURBO);
				temp_fdd_turbo[fdlv_current_sel] = (int)SendMessage(h, BM_GETCHECK, 0, 0);
				h = GetDlgItem(hdlg, IDC_LIST_FLOPPY_DRIVES);
				floppy_update_item(h, fdlv_current_sel);
				fd_ignore_change = 0;
				return FALSE;

			case IDC_CHECKBPB:
				if (fd_ignore_change)
					return FALSE;

				fd_ignore_change = 1;
				h = GetDlgItem(hdlg, IDC_CHECKBPB);
				temp_fdd_check_bpb[fdlv_current_sel] = (int)SendMessage(h, BM_GETCHECK, 0, 0);
				h = GetDlgItem(hdlg, IDC_LIST_FLOPPY_DRIVES);
				floppy_update_item(h, fdlv_current_sel);
				fd_ignore_change = 0;
				return FALSE;
		}

	default:
		break;
    }

    return FALSE;
}
