/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement threads and mutexes for the Win32 platform.
 *
 * Version:	@(#)win_thread.c	1.0.2	2018/02/21
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../plat.h"


typedef struct {
    HANDLE handle;
} win_event_t;


thread_t *
thread_create(void (*func)(void *param), void *param)
{
    return((thread_t *)_beginthread(func, 0, param));
}


void
thread_kill(void *arg)
{
    if (arg == NULL) return;

    TerminateThread(arg, 0);
}


int
thread_wait(thread_t *arg, int timeout)
{
    if (arg == NULL) return(0);

    if (timeout == -1)
	timeout = INFINITE;

    if (WaitForSingleObject(arg, timeout)) return(1);

    return(0);
}


event_t *
thread_create_event(void)
{
    win_event_t *ev = malloc(sizeof(win_event_t));

    ev->handle = CreateEvent(NULL, FALSE, FALSE, NULL);

    return((event_t *)ev);
}


void
thread_set_event(event_t *arg)
{
    win_event_t *ev = (win_event_t *)arg;

    if (arg == NULL) return;

    SetEvent(ev->handle);
}


void
thread_reset_event(event_t *arg)
{
    win_event_t *ev = (win_event_t *)arg;

    if (arg == NULL) return;

    ResetEvent(ev->handle);
}


int
thread_wait_event(event_t *arg, int timeout)
{
    win_event_t *ev = (win_event_t *)arg;

    if (arg == NULL) return(0);

    if (ev->handle == NULL) return(0);

    if (timeout == -1)
	timeout = INFINITE;

    if (WaitForSingleObject(ev->handle, timeout)) return(1);

    return(0);
}


void
thread_destroy_event(event_t *arg)
{
    win_event_t *ev = (win_event_t *)arg;

    if (arg == NULL) return;

    CloseHandle(ev->handle);

    free(ev);
}


mutex_t *
thread_create_mutex(wchar_t *name)
{
    return((mutex_t*)CreateMutex(NULL, FALSE, name));
}


void
thread_close_mutex(mutex_t *mutex)
{
    if (mutex == NULL) return;

    CloseHandle((HANDLE)mutex);
}


int
thread_wait_mutex(mutex_t *mutex)
{
    if (mutex == NULL) return(0);

    DWORD dwres = WaitForSingleObject((HANDLE)mutex, INFINITE);

    if (dwres == WAIT_OBJECT_0) return(1);

    return(0);
}


int
thread_release_mutex(mutex_t *mutex)
{
    if (mutex == NULL) return(0);

    return(!!ReleaseMutex((HANDLE)mutex));
}
