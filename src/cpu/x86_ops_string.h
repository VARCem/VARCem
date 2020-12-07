/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x86 CPU Instructions.
 *
 * Version:	@(#)x86_ops_string.h	1.0.3	2020/12/05
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2020 Sarah Walker.
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

static int opMOVSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        writememb(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { DI--; SI--; }
        else                { DI++; SI++; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opMOVSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        writememb(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { EDI--; ESI--; }
        else                { EDI++; ESI++; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opMOVSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        writememw(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { DI -= 2; SI -= 2; }
        else                { DI += 2; SI += 2; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opMOVSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        writememw(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { EDI -= 2; ESI -= 2; }
        else                { EDI += 2; ESI += 2; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opMOVSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        writememl(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { DI -= 4; SI -= 4; }
        else                { DI += 4; SI += 4; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 0,1,0,1, 0);
        return 0;
}
static int opMOVSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        writememl(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) { EDI -= 4; ESI -= 4; }
        else                { EDI += 4; ESI += 4; }
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 0,1,0,1, 1);
        return 0;
}


static int opCMPSB_a16(uint32_t fetchdat)
{
        uint8_t src = readmemb(cpu_state.ea_seg->base, SI);
        uint8_t dst = readmemb(es, DI);         if (cpu_state.abrt) return 1;
        setsub8(src, dst);
        if (cpu_state.flags & D_FLAG) { DI--; SI--; }
        else                { DI++; SI++; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 2,0,0,0, 0);
        return 0;
}
static int opCMPSB_a32(uint32_t fetchdat)
{
        uint8_t src = readmemb(cpu_state.ea_seg->base, ESI);
        uint8_t dst = readmemb(es, EDI);        if (cpu_state.abrt) return 1;
        setsub8(src, dst);
        if (cpu_state.flags & D_FLAG) { EDI--; ESI--; }
        else                { EDI++; ESI++; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 2,0,0,0, 1);
        return 0;
}

static int opCMPSW_a16(uint32_t fetchdat)
{
        uint16_t src = readmemw(cpu_state.ea_seg->base, SI);
        uint16_t dst = readmemw(es, DI);        if (cpu_state.abrt) return 1;
        setsub16(src, dst);
        if (cpu_state.flags & D_FLAG) { DI -= 2; SI -= 2; }
        else                { DI += 2; SI += 2; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 2,0,0,0, 0);
        return 0;
}
static int opCMPSW_a32(uint32_t fetchdat)
{
        uint16_t src = readmemw(cpu_state.ea_seg->base, ESI);
        uint16_t dst = readmemw(es, EDI);        if (cpu_state.abrt) return 1;
        setsub16(src, dst);
        if (cpu_state.flags & D_FLAG) { EDI -= 2; ESI -= 2; }
        else                { EDI += 2; ESI += 2; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 2,0,0,0, 1);
        return 0;
}

static int opCMPSL_a16(uint32_t fetchdat)
{
        uint32_t src = readmeml(cpu_state.ea_seg->base, SI);
        uint32_t dst = readmeml(es, DI);        if (cpu_state.abrt) return 1;
        setsub32(src, dst);
        if (cpu_state.flags & D_FLAG) { DI -= 4; SI -= 4; }
        else                { DI += 4; SI += 4; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 0,2,0,0, 0);
        return 0;
}
static int opCMPSL_a32(uint32_t fetchdat)
{
        uint32_t src = readmeml(cpu_state.ea_seg->base, ESI);
        uint32_t dst = readmeml(es, EDI);        if (cpu_state.abrt) return 1;
        setsub32(src, dst);
        if (cpu_state.flags & D_FLAG) { EDI -= 4; ESI -= 4; }
        else                { EDI += 4; ESI += 4; }
        CLOCK_CYCLES((is486) ? 8 : 10);
        PREFETCH_RUN((is486) ? 8 : 10, 1, -1, 0,2,0,0, 1);
        return 0;
}

static int opSTOSB_a16(uint32_t fetchdat)
{
        writememb(es, DI, AL);                  if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI--;
        else                DI++;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,1,0, 0);
        return 0;
}
static int opSTOSB_a32(uint32_t fetchdat)
{
        writememb(es, EDI, AL);                 if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI--;
        else                EDI++;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,1,0, 1);
        return 0;
}

static int opSTOSW_a16(uint32_t fetchdat)
{
        writememw(es, DI, AX);                  if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI -= 2;
        else                DI += 2;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,1,0, 0);
        return 0;
}
static int opSTOSW_a32(uint32_t fetchdat)
{
        writememw(es, EDI, AX);                 if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,1,0, 1);
        return 0;
}

static int opSTOSL_a16(uint32_t fetchdat)
{
        writememl(es, DI, EAX);                 if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI -= 4;
        else                DI += 4;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,0,1, 0);
        return 0;
}
static int opSTOSL_a32(uint32_t fetchdat)
{
        writememl(es, EDI, EAX);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        CLOCK_CYCLES(4);
        PREFETCH_RUN(4, 1, -1, 0,0,0,1, 1);
        return 0;
}


static int opLODSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        AL = temp;
        if (cpu_state.flags & D_FLAG) SI--;
        else                SI++;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 0);
        return 0;
}
static int opLODSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        AL = temp;
        if (cpu_state.flags & D_FLAG) ESI--;
        else                ESI++;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 1);
        return 0;
}

static int opLODSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        AX = temp;
        if (cpu_state.flags & D_FLAG) SI -= 2;
        else                SI += 2;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 0);
        return 0;
}
static int opLODSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        AX = temp;
        if (cpu_state.flags & D_FLAG) ESI -= 2;
        else                ESI += 2;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 1,0,0,0, 1);
        return 0;
}

static int opLODSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        EAX = temp;
        if (cpu_state.flags & D_FLAG) SI -= 4;
        else                SI += 4;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 0,1,0,0, 0);
        return 0;
}
static int opLODSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        EAX = temp;
        if (cpu_state.flags & D_FLAG) ESI -= 4;
        else                ESI += 4;
        CLOCK_CYCLES(5);
        PREFETCH_RUN(5, 1, -1, 0,1,0,0, 1);
        return 0;
}


static int opSCASB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(es, DI);        if (cpu_state.abrt) return 1;
        setsub8(AL, temp);
        if (cpu_state.flags & D_FLAG) DI--;
        else                DI++;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,0,0, 0);
        return 0;
}
static int opSCASB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(es, EDI);       if (cpu_state.abrt) return 1;
        setsub8(AL, temp);
        if (cpu_state.flags & D_FLAG) EDI--;
        else                EDI++;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,0,0, 1);
        return 0;
}

static int opSCASW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(es, DI);       if (cpu_state.abrt) return 1;
        setsub16(AX, temp);
        if (cpu_state.flags & D_FLAG) DI -= 2;
        else                DI += 2;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,0,0, 0);
        return 0;
}
static int opSCASW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(es, EDI);      if (cpu_state.abrt) return 1;
        setsub16(AX, temp);
        if (cpu_state.flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 1,0,0,0, 1);
        return 0;
}

static int opSCASL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(es, DI);       if (cpu_state.abrt) return 1;
        setsub32(EAX, temp);
        if (cpu_state.flags & D_FLAG) DI -= 4;
        else                DI += 4;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 0,1,0,0, 0);
        return 0;
}
static int opSCASL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(es, EDI);      if (cpu_state.abrt) return 1;
        setsub32(EAX, temp);
        if (cpu_state.flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        CLOCK_CYCLES(7);
        PREFETCH_RUN(7, 1, -1, 0,1,0,0, 1);
        return 0;
}

static int opINSB_a16(uint32_t fetchdat)
{
        uint8_t temp;
        check_io_perm(DX);
        temp = inb(DX);
        writememb(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI--;
        else                DI++;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opINSB_a32(uint32_t fetchdat)
{
        uint8_t temp;
        check_io_perm(DX);
        temp = inb(DX);
        writememb(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI--;
        else                EDI++;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opINSW_a16(uint32_t fetchdat)
{
        uint16_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        temp = inw(DX);
        writememw(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI -= 2;
        else                DI += 2;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opINSW_a32(uint32_t fetchdat)
{
        uint16_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        temp = inw(DX);
        writememw(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opINSL_a16(uint32_t fetchdat)
{
        uint32_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        temp = inl(DX);
        writememl(es, DI, temp);                if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) DI -= 4;
        else                DI += 4;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 0,1,0,1, 0);
        return 0;
}
static int opINSL_a32(uint32_t fetchdat)
{
        uint32_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        temp = inl(DX);
        writememl(es, EDI, temp);               if (cpu_state.abrt) return 1;
        if (cpu_state.flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        CLOCK_CYCLES(15);
        PREFETCH_RUN(15, 1, -1, 0,1,0,1, 1);
        return 0;
}

static int opOUTSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        if (cpu_state.flags & D_FLAG) SI--;
        else                SI++;
        outb(DX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opOUTSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        if (cpu_state.flags & D_FLAG) ESI--;
        else                ESI++;
        outb(DX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opOUTSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        if (cpu_state.flags & D_FLAG) SI -= 2;
        else                SI += 2;
        outw(DX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 1,0,1,0, 0);
        return 0;
}
static int opOUTSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        if (cpu_state.flags & D_FLAG) ESI -= 2;
        else                ESI += 2;
        outw(DX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 1,0,1,0, 1);
        return 0;
}

static int opOUTSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, SI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        if (cpu_state.flags & D_FLAG) SI -= 4;
        else                SI += 4;
        outl(EDX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 0,1,0,1, 0);
        return 0;
}
static int opOUTSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(cpu_state.ea_seg->base, ESI); if (cpu_state.abrt) return 1;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        if (cpu_state.flags & D_FLAG) ESI -= 4;
        else                ESI += 4;
        outl(EDX, temp);
        CLOCK_CYCLES(14);
        PREFETCH_RUN(14, 1, -1, 0,1,0,1, 1);
        return 0;
}
